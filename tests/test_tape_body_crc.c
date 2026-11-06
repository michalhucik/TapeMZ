/**
 * @file   test_tape_body_crc.c
 * @brief  Kontrolní součet těla FSK, SLOW a DIRECT ve wav_analyzer.
 *
 * Zakóduje MZF kodérem mzcmt_* (předzavaděč + zavaděč + data), uloží WAV
 * do temp/ a dekóduje ho wav_analyzer. Kontroluje, že body_crc odpovídá
 * shodě dekódovaného těla s originálem (OK jen při shodě) a že useknutý
 * záznam dá WAV_CRC_ERROR. Data nepotřebuje.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/generic_driver/generic_driver.h"
#include "libs/generic_driver/memory_driver.h"
#include "libs/wav_analyzer/wav_analyzer.h"
#include "libs/mzcmt_fsk/mzcmt_fsk.h"
#include "libs/mzcmt_slow/mzcmt_slow.h"
#include "libs/mzcmt_direct/mzcmt_direct.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše OK/FAIL. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )

/** @brief Formáty testu. */
typedef enum { FMT_FSK, FMT_SLOW, FMT_DIRECT } en_FMT;


/**
 * @brief Zakóduje MZF daným kodérem a uloží WAV (44100 Hz).
 * @param fmt Formát. @param h Hlavička. @param body Tělo. @param path Cesta WAV.
 * @return 1 = uloženo, 0 = chyba.
 */
static int encode ( en_FMT fmt, const st_MZF_HEADER *h, const uint8_t *body, const char *path ) {
    st_CMT_STREAM *s = NULL;
    if ( fmt == FMT_FSK ) {
        st_MZCMT_FSK_TAPE_CONFIG c = { .pulseset = MZCMT_FSK_PULSESET_800, .speed = MZCMT_FSK_SPEED_0, .loader_speed = 0 };
        s = mzcmt_fsk_create_tape_stream ( h, body, h->fsize, &c, CMT_STREAM_TYPE_VSTREAM, 44100 );
    } else if ( fmt == FMT_SLOW ) {
        st_MZCMT_SLOW_TAPE_CONFIG c = { .pulseset = MZCMT_SLOW_PULSESET_800, .speed = MZCMT_SLOW_SPEED_0, .loader_speed = 0 };
        s = mzcmt_slow_create_tape_stream ( h, body, h->fsize, &c, CMT_STREAM_TYPE_VSTREAM, 44100 );
    } else {
        st_MZCMT_DIRECT_TAPE_CONFIG c = { .pulseset = MZCMT_DIRECT_PULSESET_800, .loader_speed = 0 };
        s = mzcmt_direct_create_tape_stream ( h, body, h->fsize, &c, CMT_STREAM_TYPE_VSTREAM, 44100 );
    }
    if ( !s ) return 0;
    int r = cmt_stream_save_wav ( s, 44100, ( char* ) path );
    cmt_stream_destroy ( s );
    return r == EXIT_SUCCESS;
}


/**
 * @brief Zkrátí datový chunk WAV na daný podíl (useknutý záznam).
 * @param path Cesta WAV. @param keep_percent Kolik procent dat ponechat.
 * @return 1 = hotovo, 0 = chyba.
 */
static int truncate_wav ( const char *path, unsigned keep_percent ) {
    FILE *f = fopen ( path, "rb" );
    if ( !f ) return 0;
    fseek ( f, 0, SEEK_END );
    long n = ftell ( f );
    fseek ( f, 0, SEEK_SET );
    uint8_t *d = malloc ( ( size_t ) n );
    if ( !d || fread ( d, 1, ( size_t ) n, f ) != ( size_t ) n ) {
        fclose ( f );
        free ( d );
        return 0;
    }
    fclose ( f );
    long p = 12;
    while ( p + 8 <= n && memcmp ( d + p, "data", 4 ) != 0 ) {
        uint32_t cs = d[p + 4] | ( d[p + 5] << 8 ) | ( d[p + 6] << 16 ) | ( ( uint32_t ) d[p + 7] << 24 );
        p += 8 + cs + ( cs & 1 );
    }
    if ( p + 8 > n ) {
        free ( d );
        return 0;
    }
    uint32_t ds = d[p + 4] | ( d[p + 5] << 8 ) | ( d[p + 6] << 16 ) | ( ( uint32_t ) d[p + 7] << 24 );
    uint32_t nds = ( uint32_t ) ( ( uint64_t ) ds * keep_percent / 100 ) & ~3u;
    for ( int i = 0; i < 4; i++ ) d[p + 4 + i] = ( uint8_t ) ( nds >> ( 8 * i ) );
    uint32_t riff = ( uint32_t ) ( p + 8 + nds - 8 );
    for ( int i = 0; i < 4; i++ ) d[4 + i] = ( uint8_t ) ( riff >> ( 8 * i ) );
    f = fopen ( path, "wb" );
    int ok = f && fwrite ( d, 1, ( size_t ) ( p + 8 + nds ), f ) == ( size_t ) ( p + 8 + nds );
    if ( f ) fclose ( f );
    free ( d );
    return ok;
}


/**
 * @brief Dekóduje WAV a vrátí první soubor (nebo NULL).
 * @param path Cesta. @param[out] res Výsledek (uvolnit wav_analyzer_result_destroy).
 * @return První soubor, nebo NULL.
 */
static const st_WAV_ANALYZER_FILE_RESULT* decode ( const char *path, st_WAV_ANALYZER_RESULT *res ) {
    st_HANDLER h;
    st_DRIVER d;
    memset ( res, 0, sizeof ( *res ) );
    generic_driver_file_init ( &d );
    st_HANDLER *hp = generic_driver_open_file ( &h, &d, ( char* ) path, FILE_DRIVER_OPMODE_RO );
    if ( !hp ) return NULL;
    st_WAV_ANALYZER_CONFIG cfg;
    wav_analyzer_config_default ( &cfg );
    en_WAV_ANALYZER_ERROR e = wav_analyzer_analyze ( hp, &cfg, res );
    generic_driver_close ( hp );
    if ( e != WAV_ANALYZER_OK || res->file_count < 1 ) return NULL;
    return &res->files[0];
}


int main ( void ) {
    memory_driver_init ();
    static const char *const names[3] = { "FSK", "SLOW", "DIRECT" };
    for ( int fmt = FMT_FSK; fmt <= FMT_DIRECT; fmt++ ) {
        st_MZF_HEADER h;
        memset ( &h, 0, sizeof ( h ) );
        h.ftype = 0x01;
        memcpy ( h.fname.name, "BODY CRC\r", 9 );
        h.fsize = 256;
        h.fstrt = 0x1200;
        h.fexec = 0x1200;
        uint8_t body[256];
        char path[64], msg[160];
        snprintf ( path, sizeof ( path ), "temp/body_crc_%s.wav", names[fmt] );

        /* a) tělo 00h-FFh: dekóduje se správně -> OK */
        for ( int i = 0; i < 256; i++ ) body[i] = ( uint8_t ) i;
        st_WAV_ANALYZER_RESULT r;
        memset ( &r, 0, sizeof ( r ) );
        const st_WAV_ANALYZER_FILE_RESULT *f = encode ( fmt, &h, body, path ) ? decode ( path, &r ) : NULL;
        int same = f && f->mzf && f->mzf->body_size == 256 && memcmp ( f->mzf->body, body, 256 ) == 0;
        snprintf ( msg, sizeof ( msg ), "%s: correct body -> Body CRC OK", names[fmt] );
        CHECK ( same && f->body_crc == WAV_CRC_OK, msg );
        wav_analyzer_result_destroy ( &r );

        /*
         * b) tělo začínající 63x 00h: CRC musí odpovídat shodě s originálem.
         * Lichý počet: XOR nepozná sudý počet stejně poškozených bajtů (64x 00h jako FFh dá
         * stejný XOR, stejně jako by ho přijal zavaděč na Z80).
         */
        memset ( body, 0x00, 63 );
        for ( int i = 63; i < 256; i++ ) body[i] = ( uint8_t ) i;
        f = encode ( fmt, &h, body, path ) ? decode ( path, &r ) : NULL;
        same = f && f->mzf && f->mzf->body_size == 256 && memcmp ( f->mzf->body, body, 256 ) == 0;
        printf ( "  %s: 63x 00h body decoded %s\n", names[fmt], same ? "correctly" : "wrong" );
        snprintf ( msg, sizeof ( msg ), "%s: Body CRC matches the decoded data (no silent corruption)", names[fmt] );
        CHECK ( f && ( same ? f->body_crc == WAV_CRC_OK : f->body_crc == WAV_CRC_ERROR ), msg );
        wav_analyzer_result_destroy ( &r );

        /* c) useknutý záznam (90 % dat): neúplné tělo -> ERROR */
        for ( int i = 0; i < 256; i++ ) body[i] = ( uint8_t ) i;
        f = ( encode ( fmt, &h, body, path ) && truncate_wav ( path, 90 ) ) ? decode ( path, &r ) : NULL;
        snprintf ( msg, sizeof ( msg ), "%s: truncated recording -> Body CRC ERROR (or not decoded)", names[fmt] );
        CHECK ( !f || f->body_crc == WAV_CRC_ERROR, msg );
        wav_analyzer_result_destroy ( &r );
    }
    printf ( "\n=== Tape Body CRC Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
