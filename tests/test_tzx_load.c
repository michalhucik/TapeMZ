/**
 * @file   test_tzx_load.c
 * @brief  Regresní testy načítání TZX/TMZ: useknutý soubor,
 *         blok 0x35 Custom Info, hranice bloku 0x18 CSW.
 *
 * Soubory se zapisují do temp/ (spouštět z build/).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/tzx/tzx.h"
#include "libs/generic_driver/generic_driver.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )

/** @brief Hlavička TZX v1.20 ("ZXTape!" 1Ah 01h 14h). */
static const uint8_t g_tzx_hdr[10] = { 'Z', 'X', 'T', 'a', 'p', 'e', '!', 0x1A, 0x01, 0x14 };

/** @brief Největší alokace, kterou zaznamenal hlídací alokátor. */
static size_t g_max_alloc = 0;

/** @brief Hlídací malloc - zaznamená největší požadavek. */
static void* guard_alloc ( size_t size ) { if ( size > g_max_alloc ) g_max_alloc = size; return malloc ( size ); }
/** @brief Hlídací calloc - zaznamená největší požadavek. */
static void* guard_alloc0 ( size_t size ) { if ( size > g_max_alloc ) g_max_alloc = size; return calloc ( 1, size ); }
/** @brief Hlídací free. */
static void  guard_free ( void *ptr ) { free ( ptr ); }


/**
 * @brief Zapíše buffer do souboru.
 * @param path Cesta.
 * @param data Data.
 * @param size Velikost v bajtech.
 * @return 0 při úspěchu, -1 při chybě.
 */
static int write_file ( const char *path, const uint8_t *data, size_t size ) {
    FILE *f = fopen ( path, "wb" );
    if ( !f ) return -1;
    size_t w = fwrite ( data, 1, size, f );
    fclose ( f );
    return ( w == size ) ? 0 : -1;
}


/**
 * @brief Načte TZX soubor přes tzx_load.
 * @param path Cesta.
 * @param[out] err Chybový kód tzx_load.
 * @return Načtený soubor (uvolnit tzx_free) nebo NULL.
 */
static st_TZX_FILE* load_file ( const char *path, en_TZX_ERROR *err ) {
    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *hp = generic_driver_open_file ( &h, &d, ( char* ) path, FILE_DRIVER_OPMODE_RO );
    if ( !hp ) { *err = TZX_ERROR_IO; return NULL; }
    st_TZX_FILE *f = tzx_load ( hp, err );
    generic_driver_close ( hp );
    return f;
}


/** @brief Celý soubor, useknutá data, useknutá délka, obří délka. */
static void test_truncated ( void ) {
    /* hlavička + 0x20 Pause (2 B) + 0x30 Text (1 + 3 B) */
    uint8_t buf[64];
    size_t n = 0;
    memcpy ( buf, g_tzx_hdr, 10 ); n = 10;
    buf[n++] = 0x20; buf[n++] = 0xE8; buf[n++] = 0x03;
    buf[n++] = 0x30; buf[n++] = 3; buf[n++] = 'a'; buf[n++] = 'b'; buf[n++] = 'c';

    en_TZX_ERROR err;
    st_TZX_FILE *f;

    write_file ( "temp/tzx_ok.tzx", buf, n );
    f = load_file ( "temp/tzx_ok.tzx", &err );
    CHECK ( f && err == TZX_OK && f->block_count == 2, "complete file loads with 2 blocks" );
    tzx_free ( f );

    write_file ( "temp/tzx_trunc_data.tzx", buf, n - 1 );
    f = load_file ( "temp/tzx_trunc_data.tzx", &err );
    CHECK ( f == NULL && err == TZX_ERROR_UNEXPECTED_EOF, "truncated block data -> UNEXPECTED_EOF" );
    tzx_free ( f );

    /* useknutí uprostřed délky: hlavička + 0x30 bez délkového bajtu */
    uint8_t cut[11];
    memcpy ( cut, g_tzx_hdr, 10 ); cut[10] = 0x30;
    write_file ( "temp/tzx_trunc_len.tzx", cut, sizeof ( cut ) );
    f = load_file ( "temp/tzx_trunc_len.tzx", &err );
    CHECK ( f == NULL && err == TZX_ERROR_UNEXPECTED_EOF, "truncated block length -> UNEXPECTED_EOF" );
    tzx_free ( f );

    /* useknutí uprostřed DWORD délky: 0x19 + 2 ze 4 bajtů délky */
    uint8_t cut_dword[13];
    memcpy ( cut_dword, g_tzx_hdr, 10 ); cut_dword[10] = 0x19; cut_dword[11] = 0x10; cut_dword[12] = 0x00;
    write_file ( "temp/tzx_trunc_dword.tzx", cut_dword, sizeof ( cut_dword ) );
    f = load_file ( "temp/tzx_trunc_dword.tzx", &err );
    CHECK ( f == NULL && err == TZX_ERROR_UNEXPECTED_EOF, "truncated DWORD length -> UNEXPECTED_EOF" );
    tzx_free ( f );

    /* DWORD délka 0xFFFFFFFF (0x19 i 0x35) přeteče 32bit -> INVALID_BLOCK */
    uint8_t ovf19[20];
    memcpy ( ovf19, g_tzx_hdr, 10 ); ovf19[10] = 0x19;
    memset ( &ovf19[11], 0xFF, 4 ); memset ( &ovf19[15], 0, 5 );
    write_file ( "temp/tzx_ovf19.tzx", ovf19, sizeof ( ovf19 ) );
    f = load_file ( "temp/tzx_ovf19.tzx", &err );
    CHECK ( f == NULL && err == TZX_ERROR_INVALID_BLOCK, "0x19 length 0xFFFFFFFF -> INVALID_BLOCK" );
    tzx_free ( f );

    uint8_t ovf35[31];
    memcpy ( ovf35, g_tzx_hdr, 10 ); ovf35[10] = 0x35;
    memcpy ( &ovf35[11], "POKEs           ", 16 ); memset ( &ovf35[27], 0xFF, 4 );
    write_file ( "temp/tzx_ovf35.tzx", ovf35, sizeof ( ovf35 ) );
    f = load_file ( "temp/tzx_ovf35.tzx", &err );
    CHECK ( f == NULL && err == TZX_ERROR_INVALID_BLOCK, "0x35 length 0xFFFFFFFF -> INVALID_BLOCK" );
    tzx_free ( f );

    /* blok 0x19 (DWORD délka) s délkou 0x7FFFFFF0, souboru je jen pár bajtů */
    uint8_t huge[20];
    memcpy ( huge, g_tzx_hdr, 10 );
    huge[10] = 0x19; huge[11] = 0xF0; huge[12] = 0xFF; huge[13] = 0xFF; huge[14] = 0x7F;
    memset ( &huge[15], 0, 5 );
    write_file ( "temp/tzx_huge.tzx", huge, sizeof ( huge ) );
    st_TZX_ALLOCATOR guard = { guard_alloc, guard_alloc0, guard_free };
    tzx_set_allocator ( &guard );
    g_max_alloc = 0;
    f = load_file ( "temp/tzx_huge.tzx", &err );
    tzx_free ( f );
    tzx_set_allocator ( NULL );
    CHECK ( f == NULL && err == TZX_ERROR_UNEXPECTED_EOF, "huge block length -> UNEXPECTED_EOF" );
    CHECK ( g_max_alloc < 1024 * 1024, "huge block length does not allocate block data" );
}


/** @brief 0x35 se 16B ID; za ním musí být správně načten blok 0x20. */
static void test_custom_info ( void ) {
    uint8_t buf[80];
    size_t n = 0;
    memcpy ( buf, g_tzx_hdr, 10 ); n = 10;
    buf[n++] = 0x35;
    memcpy ( &buf[n], "POKEs           ", 16 ); n += 16;
    buf[n++] = 3; buf[n++] = 0; buf[n++] = 0; buf[n++] = 0;
    buf[n++] = 'x'; buf[n++] = 'y'; buf[n++] = 'z';
    buf[n++] = 0x20; buf[n++] = 0x64; buf[n++] = 0x00;
    /* 0x35 s nulovou délkou dat */
    buf[n++] = 0x35;
    memcpy ( &buf[n], "Instructions    ", 16 ); n += 16;
    buf[n++] = 0; buf[n++] = 0; buf[n++] = 0; buf[n++] = 0;

    en_TZX_ERROR err;
    write_file ( "temp/tzx_custom.tzx", buf, n );
    st_TZX_FILE *f = load_file ( "temp/tzx_custom.tzx", &err );
    CHECK ( f && err == TZX_OK && f->block_count == 3, "0x35 file loads with 3 blocks" );
    CHECK ( f && f->block_count >= 2 && f->blocks[1].id == 0x20, "block after 0x35 is 0x20" );
    tzx_free ( f );
}


/** @brief CSW blok s DWORD délkou větší než data musí být odmítnut. */
static void test_csw_bounds ( void ) {
    uint8_t raw[0x0E + 4];
    memset ( raw, 0, sizeof ( raw ) );
    raw[0x0E] = 1; raw[0x0F] = 2; raw[0x10] = 3; raw[0x11] = 4;
    st_TZX_CSW_RECORDING csw;

    /* korektní: DWORD = 10 + 4 */
    raw[0] = 14; raw[1] = 0; raw[2] = 0; raw[3] = 0;
    CHECK ( tzx_parse_csw_recording ( raw, sizeof ( raw ), &csw ) == TZX_OK && csw.data_length == 4,
            "CSW consistent length parses (4 data bytes)" );

    /* poškozený: DWORD = 0x1000 */
    raw[0] = 0x00; raw[1] = 0x10;
    CHECK ( tzx_parse_csw_recording ( raw, sizeof ( raw ), &csw ) == TZX_ERROR_INVALID_BLOCK,
            "CSW oversized length rejected" );

    /* poškozený: DWORD < 10 */
    raw[0] = 5; raw[1] = 0;
    CHECK ( tzx_parse_csw_recording ( raw, sizeof ( raw ), &csw ) == TZX_ERROR_INVALID_BLOCK,
            "CSW length below header size rejected" );
}


/**
 * @brief Major verze jiná než 1 musí skončit TZX_ERROR_INVALID_VERSION (spec v1.1 kap. 2).
 */
static void test_major_version ( void ) {
    uint8_t buf[10] = { 'T', 'a', 'p', 'e', 'M', 'Z', '!', 0x1A, 0x02, 0x00 };
    write_file ( "temp/ver2.tmz", buf, sizeof ( buf ) );
    en_TZX_ERROR err = TZX_OK;
    st_TZX_FILE *f = load_file ( "temp/ver2.tmz", &err );
    CHECK ( f == NULL && err == TZX_ERROR_INVALID_VERSION, "major version 2 rejected" );
    if ( f ) tzx_free ( f );

    uint8_t ok[10] = { 'T', 'a', 'p', 'e', 'M', 'Z', '!', 0x1A, 0x01, 0x01 };
    write_file ( "temp/ver11.tmz", ok, sizeof ( ok ) );
    f = load_file ( "temp/ver11.tmz", &err );
    CHECK ( f != NULL && err == TZX_OK, "version 1.1 accepted" );
    if ( f ) tzx_free ( f );
}


/**
 * @brief Blok 0x32 s tělem nad 65535 B se odmítne (dřív přetečení WORD a zápis za buffer).
 */
static void test_archive_info_too_long ( void ) {
    static char text[256];
    memset ( text, 'A', 255 );
    text[255] = '\0';
    static st_TZX_ARCHIVE_ENTRY e[255];
    for ( int i = 0; i < 255; i++ ) {
        e[i].type_id = 0xFF;
        e[i].text = text;
    }
    st_TZX_BLOCK b;
    memset ( &b, 0, sizeof ( b ) );
    /* 1 + 255 * (2 + 255) = 65536 B > 65535 */
    CHECK ( tzx_block_create_archive_info ( e, 255, &b ) == TZX_ERROR_INVALID_BLOCK && !b.data,
            "archive info longer than 65535 B rejected" );
    /* 254 záznamů = 65279 B: projde */
    en_TZX_ERROR r = tzx_block_create_archive_info ( e, 254, &b );
    CHECK ( r == TZX_OK && b.length == 2 + 1 + 254u * 257u && b.data[0] == ( uint8_t ) ( ( 1 + 254 * 257 ) & 0xFF ),
            "archive info of 65279 B created" );
    free ( b.data );
}


int main ( void ) {
    test_truncated ();
    test_custom_info ();
    test_csw_bounds ();
    test_major_version ();
    test_archive_info_too_long ();
    printf ( "\n=== TZX load Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
