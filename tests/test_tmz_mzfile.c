/**
 * @file   test_tmz_mzfile.c
 * @brief  Testy bloku 0x46 MZ File (spec TMZ v1.1 kap. 4): vektory
 *         tstdata/tmz-v2, zápis tam a zpět, poškozené vstupy.
 *
 * Spouštět z build/ (vstupy ../tstdata/tmz-v2/).
 * Bez tstdata/ vypíše SKIP a skončí 0 (tests/tstdata_skip.h).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/tzx/tzx.h"
#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_tlv.h"
#include "libs/tmz/tmz_mzfile.h"
#include "libs/generic_driver/generic_driver.h"
#include "tstdata_skip.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )


/**
 * @brief Načte soubor .tmz a vrátí kopii prvního bloku (volající uvolní tmz_block_free).
 * @param path Cesta k souboru.
 * @return Blok, nebo NULL.
 */
static st_TZX_BLOCK* load_first_block ( const char *path ) {
    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *hp = generic_driver_open_file ( &h, &d, ( char* ) path, FILE_DRIVER_OPMODE_RO );
    if ( !hp ) return NULL;
    en_TZX_ERROR err = TZX_OK;
    st_TZX_FILE *f = tzx_load ( hp, &err );
    generic_driver_close ( hp );
    if ( !f || f->block_count < 1 ) { if ( f ) tzx_free ( f ); return NULL; }
    st_TZX_BLOCK *b = malloc ( sizeof ( st_TZX_BLOCK ) );
    b->id = f->blocks[0].id;
    b->length = f->blocks[0].length;
    b->data = malloc ( b->length );
    memcpy ( b->data, f->blocks[0].data, b->length );
    tzx_free ( f );
    return b;
}


/**
 * @brief Blok z bajtů v paměti (data včetně 4B délky), pro poškozené vstupy.
 * @param bytes Data za ID.
 * @param len Délka.
 * @return Blok (uvolnit tmz_block_free).
 */
static st_TZX_BLOCK* block_from_bytes ( const uint8_t *bytes, uint32_t len ) {
    st_TZX_BLOCK *b = malloc ( sizeof ( st_TZX_BLOCK ) );
    b->id = TMZ_BLOCK_ID_MZ_FILE;
    b->length = len;
    b->data = malloc ( len ? len : 1 );
    memcpy ( b->data, bytes, len );
    return b;
}


/** @brief Vektor V1: pole bloku podle spec 11.1. */
static void test_v1 ( void ) {
    st_TZX_BLOCK *b = load_first_block ( "../tstdata/tmz-v2/v1-rom-800-nominal.tmz" );
    CHECK ( b && b->id == TMZ_BLOCK_ID_MZ_FILE, "V1 loaded, id 0x46" );
    if ( !b ) return;
    st_TMZ_MZFILE mf;
    en_TZX_ERROR e = tmz_mzfile_parse ( b, &mf );
    CHECK ( e == TZX_OK, "V1 parse ok" );
    if ( e != TZX_OK ) { tmz_block_free ( b ); return; }
    CHECK ( strcmp ( mf.profile_id, "ROM-800-NOMINAL" ) == 0, "V1 profile id" );
    CHECK ( mf.body_size == 4 && mf.body[0] == 0x3E && mf.header[0] == 0x01, "V1 header and body" );
    CHECK ( mf.block_tlv_length == 0 && mf.part_count == 2, "V1 no block TLV, 2 parts" );
    CHECK ( mf.parts[0].role == TMZ_PART_ROLE_HEADER && mf.parts[0].family == TMZ_FAMILY_FM
            && mf.parts[0].source == TMZ_SOURCE_MZF_HEADER, "V1 part 0 header/FM/MZF_HEADER" );
    CHECK ( mf.parts[0].tlv_length == 75 - 3, "V1 part 0 TLV length 72" );
    uint32_t sz = 0;
    en_TMZ_PART_ERROR pe;
    const uint8_t *pd = tmz_mzfile_part_data ( &mf, &mf.parts[1], &sz, &pe );
    CHECK ( pd == mf.body && sz == 4 && pe == TMZ_PART_OK, "V1 part 1 data = body" );

    /* zápis tam a zpět: bajtově shodný blok */
    st_TZX_BLOCK *c = tmz_mzfile_create_block ( &mf, &e );
    CHECK ( c && e == TZX_OK && c->length == b->length && memcmp ( c->data, b->data, b->length ) == 0,
            "V1 re-create byte-identical" );
    tmz_block_free ( c );
    tmz_mzfile_free ( &mf );
    CHECK ( mf.parts == NULL && mf.part_count == 0, "free clears parts" );
    tmz_block_free ( b );
}


/** @brief Všechny vektory: parse a zápis tam a zpět. */
static void test_all_vectors ( void ) {
    static const char *names[] = { "v1-rom-800-nominal", "v2-tc-2to1-loader-n", "v3-tc-3to1-loader-y",
                                   "v4-custom-x", "v6-rom-800-exact" };
    for ( size_t i = 0; i < sizeof ( names ) / sizeof ( names[0] ); i++ ) {
        char path[128], msg[160];
        snprintf ( path, sizeof ( path ), "../tstdata/tmz-v2/%s.tmz", names[i] );
        st_TZX_BLOCK *b = load_first_block ( path );
        st_TMZ_MZFILE mf;
        en_TZX_ERROR e = b ? tmz_mzfile_parse ( b, &mf ) : TZX_ERROR_IO;
        st_TZX_BLOCK *c = ( e == TZX_OK ) ? tmz_mzfile_create_block ( &mf, &e ) : NULL;
        snprintf ( msg, sizeof ( msg ), "%s parse + re-create identical", names[i] );
        CHECK ( c && c->length == b->length && memcmp ( c->data, b->data, b->length ) == 0, msg );
        if ( c ) tmz_block_free ( c );
        if ( b && e == TZX_OK ) tmz_mzfile_free ( &mf );
        if ( b ) tmz_block_free ( b );
    }
}


/** @brief V3 (inline) a V4 (výřez): data částí. */
static void test_sources ( void ) {
    st_TZX_BLOCK *b = load_first_block ( "../tstdata/tmz-v2/v3-tc-3to1-loader-y.tmz" );
    st_TMZ_MZFILE mf;
    if ( !b || tmz_mzfile_parse ( b, &mf ) != TZX_OK ) { CHECK ( 0, "V3 parse" ); return; }
    uint32_t sz;
    en_TMZ_PART_ERROR pe;
    const uint8_t *d0 = tmz_mzfile_part_data ( &mf, &mf.parts[0], &sz, &pe );
    CHECK ( d0 && sz == 128 && d0[0x12] == 0x5A && d0[0x15] == 0xD4, "V3 part 0 inline preloader header (fsize 5Ah, D400h)" );
    const uint8_t *d1 = tmz_mzfile_part_data ( &mf, &mf.parts[1], &sz, &pe );
    CHECK ( d1 && sz == 90 && d1[0] == 0x3E && d1[0x4B] == 27, "V3 part 1 inline preloader, N=27" );
    CHECK ( strcmp ( mf.profile_id, "TC-1.21" ) == 0 && mf.block_tlv_length == 22, "V3 profile id + PROFILE_ARGS" );
    tmz_mzfile_free ( &mf );
    tmz_block_free ( b );

    b = load_first_block ( "../tstdata/tmz-v2/v4-custom-x.tmz" );
    if ( !b || tmz_mzfile_parse ( b, &mf ) != TZX_OK ) { CHECK ( 0, "V4 parse" ); return; }
    const uint8_t *s = tmz_mzfile_part_data ( &mf, &mf.parts[0], &sz, &pe );
    CHECK ( s == mf.body && sz == 4, "V4 slice 0..4" );
    st_TMZ_PART bad = mf.parts[0];
    bad.slice_offset = 2;
    bad.slice_length = 3;
    CHECK ( tmz_mzfile_part_data ( &mf, &bad, &sz, &pe ) == NULL && pe == TMZ_PART_ERROR_BAD_SOURCE,
            "slice beyond body rejected" );
    bad.slice_offset = 0xFFFFFFFFu;
    bad.slice_length = 2;
    CHECK ( tmz_mzfile_part_data ( &mf, &bad, &sz, &pe ) == NULL, "slice offset overflow rejected" );
    /* zápis bloku s useknutým TLV části nebo bloku odmítne (parse by ho odmítl taky) */
    {
        static const uint8_t cut_tlv[6] = { 0x01, 0x81, 0x10, 0x00, 0x00, 0x00 };   /* délka 16, data 2 B */
        st_TMZ_PART pp = mf.parts[0];
        pp.tlv = cut_tlv;
        pp.tlv_length = sizeof ( cut_tlv );
        st_TMZ_MZFILE w = mf;
        w.parts = &pp;
        w.part_count = 1;
        en_TZX_ERROR e;
        CHECK ( tmz_mzfile_create_block ( &w, &e ) == NULL && e == TZX_ERROR_INVALID_BLOCK, "create rejects truncated part TLV" );
        w.parts = mf.parts;
        w.part_count = mf.part_count;
        w.block_tlv = cut_tlv;
        w.block_tlv_length = sizeof ( cut_tlv );
        CHECK ( tmz_mzfile_create_block ( &w, &e ) == NULL && e == TZX_ERROR_INVALID_BLOCK, "create rejects truncated block TLV" );
    }
    tmz_mzfile_free ( &mf );
    tmz_block_free ( b );
}


/** @brief Poškozené bloky: parse vrátí chybu, nikdy nečte mimo data. */
static void test_malformed ( void ) {
    st_TZX_BLOCK *b = load_first_block ( "../tstdata/tmz-v2/v1-rom-800-nominal.tmz" );
    if ( !b ) { CHECK ( 0, "V1 for malformed" ); return; }
    st_TMZ_MZFILE mf;
    uint8_t *m = malloc ( b->length );

    /* každé useknutí bloku (s opravenou vnitřní délkou) musí selhat */
    int all_fail = 1;
    for ( uint32_t len = 4; len < b->length; len++ ) {
        memcpy ( m, b->data, len );
        tmz_wr32 ( m, len - 4 );
        st_TZX_BLOCK *x = block_from_bytes ( m, len );
        if ( tmz_mzfile_parse ( x, &mf ) == TZX_OK ) { all_fail = 0; tmz_mzfile_free ( &mf ); }
        tmz_block_free ( x );
    }
    CHECK ( all_fail, "every truncation rejected" );

    /* vnitřní DWORD nesouhlasí s délkou bloku */
    memcpy ( m, b->data, b->length );
    tmz_wr32 ( m, tmz_rd32 ( m ) + 1 );
    st_TZX_BLOCK *x = block_from_bytes ( m, b->length );
    CHECK ( tmz_mzfile_parse ( x, &mf ) == TZX_ERROR_INVALID_BLOCK, "length field mismatch rejected" );
    tmz_block_free ( x );

    /* verze bloku 2 */
    memcpy ( m, b->data, b->length );
    m[4] = 2;
    x = block_from_bytes ( m, b->length );
    CHECK ( tmz_mzfile_parse ( x, &mf ) == TZX_ERROR_UNSUPPORTED, "block version 2 unsupported" );
    tmz_block_free ( x );

    /* ID profilu s malým písmenem */
    memcpy ( m, b->data, b->length );
    m[6] = 'r';
    x = block_from_bytes ( m, b->length );
    CHECK ( tmz_mzfile_parse ( x, &mf ) == TZX_ERROR_INVALID_BLOCK, "lowercase profile id rejected" );
    tmz_block_free ( x );

    /* ID profilu s mezerou */
    memcpy ( m, b->data, b->length );
    m[9] = ' ';
    x = block_from_bytes ( m, b->length );
    CHECK ( tmz_mzfile_parse ( x, &mf ) == TZX_ERROR_INVALID_BLOCK, "profile id with space rejected" );
    tmz_block_free ( x );

    /* počet částí 0 (offset počtu částí: 4+1+1+15+128+4+4+2 = 159) */
    memcpy ( m, b->data, b->length );
    CHECK ( m[159] == 2, "part count offset sanity" );
    m[159] = 0;
    x = block_from_bytes ( m, b->length );
    CHECK ( tmz_mzfile_parse ( x, &mf ) == TZX_ERROR_INVALID_BLOCK, "zero parts rejected" );
    tmz_block_free ( x );

    /* délka části 0 přes konec bloku */
    memcpy ( m, b->data, b->length );
    tmz_wr32 ( m + 160, 0x7FFFFFFFu );
    x = block_from_bytes ( m, b->length );
    CHECK ( tmz_mzfile_parse ( x, &mf ) == TZX_ERROR_INVALID_BLOCK, "part length overrun rejected" );
    tmz_block_free ( x );

    /* ID profilu delší než 32 */
    memcpy ( m, b->data, b->length );
    m[5] = 33;
    x = block_from_bytes ( m, b->length );
    CHECK ( tmz_mzfile_parse ( x, &mf ) == TZX_ERROR_INVALID_BLOCK, "profile id length 33 rejected" );
    tmz_block_free ( x );

    free ( m );
    tmz_block_free ( b );

    /* INLINE s délkou přes konec části */
    st_TMZ_PART p = { TMZ_PART_ROLE_OTHER, TMZ_FAMILY_FM, TMZ_SOURCE_INLINE, 0, 0,
                      ( const uint8_t* ) "\x01\x02", 2, NULL, 0 };
    uint8_t hdr[128] = { 0 };
    st_TMZ_MZFILE w = { "X-T", hdr, NULL, 0, NULL, 0, 1, &p };
    en_TZX_ERROR e;
    st_TZX_BLOCK *c = tmz_mzfile_create_block ( &w, &e );
    CHECK ( c && e == TZX_OK, "create block with inline part" );
    if ( c ) {
        CHECK ( tmz_mzfile_parse ( c, &mf ) == TZX_OK && mf.parts[0].inline_length == 2, "inline part parses" );
        tmz_mzfile_free ( &mf );
        /* inline délka je na offsetu: 4 + 1 + 1 + 3 + 128 + 4 + 2 + 1 + 4 + 3 = 151 */
        CHECK ( tmz_rd32 ( c->data + 151 ) == 2, "inline length offset sanity" );
        tmz_wr32 ( c->data + 151, 100 );
        CHECK ( tmz_mzfile_parse ( c, &mf ) == TZX_ERROR_INVALID_BLOCK, "inline length beyond part rejected" );
        tmz_block_free ( c );
    }

    /* inline délka blízko UINT32_MAX nesmí přetéct výpočet velikosti (jinak přepis haldy) */
    st_TMZ_PART huge = p;
    huge.inline_length = 0xFFFFFFFCu;
    st_TMZ_MZFILE wh = { "X-T", hdr, NULL, 0, NULL, 0, 1, &huge };
    c = tmz_mzfile_create_block ( &wh, &e );
    CHECK ( c == NULL && e == TZX_ERROR_INVALID_BLOCK, "create rejects inline length overflow" );
    if ( c ) tmz_block_free ( c );

    /* zápis s neplatným ID profilu */
    st_TMZ_MZFILE w2 = { "bad id", hdr, NULL, 0, NULL, 0, 1, &p };
    c = tmz_mzfile_create_block ( &w2, &e );
    CHECK ( c == NULL && e == TZX_ERROR_INVALID_BLOCK, "create rejects invalid profile id" );
}


/** @brief tmz_block_to_mzf nad 0x46, názvy rolí a rodin, verze formátu. */
static void test_to_mzf_and_names ( void ) {
    st_TZX_BLOCK *b = load_first_block ( "../tstdata/tmz-v2/v2-tc-2to1-loader-n.tmz" );
    en_TZX_ERROR e;
    st_MZF *m = b ? tmz_block_to_mzf ( b, &e ) : NULL;
    CHECK ( m && e == TZX_OK && m->body_size == 1024 && m->header.fsize == 1024 && m->header.fstrt == 0x1200
            && m->body[0] == 0x18, "to_mzf from 0x46 (V2)" );
    if ( m ) mzf_free ( m );
    CHECK ( strcmp ( tmz_block_id_name ( TMZ_BLOCK_ID_MZ_FILE ), "MZ File" ) == 0, "block name 0x46" );
    CHECK ( strcmp ( tmz_part_role_name ( TMZ_PART_ROLE_LOADER_HEADER ), "loader header" ) == 0
            && strcmp ( tmz_part_role_name ( 200 ), "other" ) == 0, "role names" );
    CHECK ( strcmp ( tmz_family_name ( TMZ_FAMILY_FM ), "FM" ) == 0 && strcmp ( tmz_family_name ( 99 ), "unknown" ) == 0,
            "family names" );
    CHECK ( strcmp ( tmz_format_version (), "1.1" ) == 0 && TMZ_VERSION_MINOR == 1, "format version 1.1" );
    if ( b ) tmz_block_free ( b );

    /* tělo > 65535 B nejde do MZF */
    static uint8_t big[70000];
    uint8_t hdr[128] = { 1 };
    st_TMZ_PART p = { TMZ_PART_ROLE_BODY, TMZ_FAMILY_FM, TMZ_SOURCE_MZF_BODY, 0, 0, NULL, 0, NULL, 0 };
    st_TMZ_MZFILE w = { "X-BIG", hdr, big, sizeof ( big ), NULL, 0, 1, &p };
    st_TZX_BLOCK *c = tmz_mzfile_create_block ( &w, &e );
    m = c ? tmz_block_to_mzf ( c, &e ) : NULL;
    CHECK ( c && m == NULL && e == TZX_ERROR_INVALID_BLOCK, "to_mzf rejects body > 65535" );
    if ( m ) mzf_free ( m );
    if ( c ) tmz_block_free ( c );

    /* fsize v hlavičce se liší od délky těla N (spec 4.1 to dovoluje): MZF to neumí,
       extrakce musí skončit chybou, ne čtením mimo tělo ani useknutím */
    uint8_t hdr2[128] = { 1 };
    hdr2[0x12] = 0x00;
    hdr2[0x13] = 0x05;   /* fsize = 0500h, tělo jen 16 B */
    static uint8_t small[16];
    st_TMZ_MZFILE w2 = { "X-FS", hdr2, small, sizeof ( small ), NULL, 0, 1, &p };
    c = tmz_mzfile_create_block ( &w2, &e );
    m = c ? tmz_block_to_mzf ( c, &e ) : NULL;
    CHECK ( c && m == NULL && e == TZX_ERROR_INVALID_BLOCK, "to_mzf rejects fsize > body size" );
    if ( m ) mzf_free ( m );
    if ( c ) tmz_block_free ( c );
    hdr2[0x12] = 0x08;
    hdr2[0x13] = 0x00;   /* fsize = 8 < 16 */
    c = tmz_mzfile_create_block ( &w2, &e );
    m = c ? tmz_block_to_mzf ( c, &e ) : NULL;
    CHECK ( c && m == NULL && e == TZX_ERROR_INVALID_BLOCK, "to_mzf rejects fsize < body size" );
    if ( m ) mzf_free ( m );
    if ( c ) tmz_block_free ( c );
}


int main ( void ) {
    if ( tstdata_skip ( "test_tmz_mzfile" ) ) return 0;
    test_v1 ();
    test_all_vectors ();
    test_sources ();
    test_malformed ();
    test_to_mzf_and_names ();
    printf ( "\n=== TMZ MZ File Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
