/**
 * @file   test_tzx_csw.c
 * @brief  Blok TZX 0x18 CSW Recording: zápis (tzx_block_create_csw_recording),
 *         parse, generátor vstreamu (tzx_generate_csw_recording, tzx_generate_block)
 *         a přehrání přes tmz_player.
 *
 * Pulzy {10, 20, 300, 70000, 5} při 44 100 Hz: délky 300 a 70000 se v RLE
 * zapíší únikem (bajt 00h + DWORD). Při výstupu 44 100 Hz musí běhy vyjít
 * přesně, při 17 721 600 Hz celková délka nejvýš o 1 vzorek. Chybové stavy
 * se ověřují s vlastním error callbackem, aby výstup testu zůstal čistý.
 * Soubor se zapisuje do temp/ (spouštět z build/).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "libs/tzx/tzx.h"
#include "libs/tmz/tmz_player.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/cmt_stream/cmt_vstream.h"
#include "libs/generic_driver/generic_driver.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše OK/FAIL. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )

/** @brief Testovací pulzy (vzorky při 44 100 Hz); 300 a 70000 > 255 = únik. */
static const uint32_t g_pulses[] = { 10, 20, 300, 70000, 5 };
/** @brief Počet testovacích pulzů. */
#define PULSE_COUNT 5u
/** @brief Vzorkovací frekvence testovacího záznamu (Hz). */
#define CSW_RATE 44100u
/** @brief Očekávaná data RLE: 10, 20, 00h + DWORD 300, 00h + DWORD 70000, 5. */
static const uint8_t g_exp_rle[] = { 10, 20, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x00, 0x70, 0x11, 0x01, 0x00, 5 };

/** @brief Počet hlášení, která zachytil testovací error callback. */
static int g_cb_count = 0;


/**
 * @brief Testovací error callback knihovny tzx - hlášení jen počítá.
 * @param func Funkce. @param line Řádek. @param fmt Formát.
 */
static void count_cb ( const char *func, int line, const char *fmt, ... ) {
    ( void ) func;
    ( void ) line;
    ( void ) fmt;
    g_cb_count++;
}


/**
 * @brief Porovná běhy vstreamu s očekávanými délkami a střídavou úrovní od LOW.
 * @param vs Vstream. @param exp Očekávané délky. @param n Počet.
 * @return 1 = shoda (počet, délky i úrovně), 0 = neshoda.
 */
static int runs_match ( st_CMT_VSTREAM *vs, const uint32_t *exp, uint32_t n ) {
    uint64_t len;
    int v;
    uint32_t i = 0;
    if ( !vs ) return 0;
    cmt_vstream_read_reset ( vs );
    while ( cmt_vstream_read_pulse ( vs, &len, &v ) == EXIT_SUCCESS ) {
        if ( i >= n || len != exp[i] || v != ( int ) ( i & 1 ) ) return 0;
        i++;
    }
    return i == n;
}


/**
 * @brief Sestaví surová data bloku 0x18 (s DWORD délkou na začátku).
 * @param buf Výstup (min. 14 + n B). @param rate Frekvence. @param comp Typ komprese.
 * @param count Počet pulzů v hlavičce. @param csw CSW data. @param n Délka CSW dat.
 * @return Délka surových dat (14 + n).
 */
static uint32_t raw_csw ( uint8_t *buf, uint32_t rate, uint8_t comp, uint32_t count, const uint8_t *csw, uint32_t n ) {
    uint32_t dw = 10 + n;
    buf[0] = ( uint8_t ) dw; buf[1] = ( uint8_t ) ( dw >> 8 ); buf[2] = ( uint8_t ) ( dw >> 16 ); buf[3] = ( uint8_t ) ( dw >> 24 );
    buf[4] = 0; buf[5] = 0;
    buf[6] = ( uint8_t ) rate; buf[7] = ( uint8_t ) ( rate >> 8 ); buf[8] = ( uint8_t ) ( rate >> 16 );
    buf[9] = comp;
    buf[10] = ( uint8_t ) count; buf[11] = ( uint8_t ) ( count >> 8 ); buf[12] = ( uint8_t ) ( count >> 16 );
    buf[13] = ( uint8_t ) ( count >> 24 );
    if ( n ) memcpy ( buf + 14, csw, n );
    return 14 + n;
}


/**
 * @brief Zapíše soubor TZX s jedním blokem a načte ho zpět přes tzx_load.
 * @param path Cesta. @param b Blok (data se jen čtou).
 * @param[out] err Chybový kód tzx_save / tzx_load.
 * @return Načtený soubor (uvolnit tzx_free), nebo NULL.
 */
static st_TZX_FILE* save_load ( const char *path, const st_TZX_BLOCK *b, en_TZX_ERROR *err ) {
    st_TZX_FILE f;
    memset ( &f, 0, sizeof ( f ) );
    tzx_header_init ( &f.header );
    f.blocks = ( st_TZX_BLOCK* ) b;
    f.block_count = 1;

    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *hp = generic_driver_open_file ( &h, &d, ( char* ) path, FILE_DRIVER_OPMODE_W );
    if ( !hp ) { *err = TZX_ERROR_IO; return NULL; }
    *err = tzx_save ( hp, &f );
    generic_driver_close ( hp );
    if ( *err != TZX_OK ) return NULL;

    hp = generic_driver_open_file ( &h, &d, ( char* ) path, FILE_DRIVER_OPMODE_RO );
    if ( !hp ) { *err = TZX_ERROR_IO; return NULL; }
    st_TZX_FILE *lf = tzx_load ( hp, err );
    generic_driver_close ( hp );
    return lf;
}


/** @brief Krok 1: create -> parse dá tytéž pulzy a frekvenci, dlouhé délky únikem. */
static void test_roundtrip ( void ) {
    st_TZX_BLOCK b;
    memset ( &b, 0, sizeof ( b ) );
    en_TZX_ERROR e = tzx_block_create_csw_recording ( CSW_RATE, 0, g_pulses, PULSE_COUNT, &b );
    CHECK ( e == TZX_OK && b.id == TZX_BLOCK_ID_CSW_RECORDING && b.data, "create 0x18 (RLE)" );
    if ( e != TZX_OK ) return;

    CHECK ( b.length == 4 + 10 + sizeof ( g_exp_rle ), "block length = DWORD + 10 B header + 13 B RLE" );

    st_TZX_CSW_RECORDING p;
    e = tzx_parse_csw_recording ( b.data, b.length, &p );
    CHECK ( e == TZX_OK && p.sample_rate == CSW_RATE && p.compression_type == 1 && p.pulse_count == PULSE_COUNT
            && p.pause_ms == 0, "parse: sample rate 44100, compression 1 (RLE), 5 pulses, pause 0" );
    CHECK ( e == TZX_OK && p.data_length == sizeof ( g_exp_rle ) && memcmp ( p.data, g_exp_rle, sizeof ( g_exp_rle ) ) == 0,
            "RLE data: 300 and 70000 written as 00h + 32-bit little-endian length" );

    /* dekódované pulzy = vstup (generátor při stejné frekvenci) */
    st_TZX_CONFIG cfg;
    tzx_config_init ( &cfg );
    cfg.sample_rate = CSW_RATE;
    st_CMT_VSTREAM *vs = ( e == TZX_OK ) ? tzx_generate_csw_recording ( &p, &cfg, &e ) : NULL;
    CHECK ( vs && runs_match ( vs, g_pulses, PULSE_COUNT ), "create -> parse -> decode gives the same pulses" );
    cmt_vstream_destroy ( vs );

    free ( b.data );
}


/** @brief Krok 2: generátor při 44 100 Hz přesně, při 17 721 600 Hz celková délka na 1 vzorek. */
static void test_generator ( void ) {
    st_TZX_BLOCK b;
    memset ( &b, 0, sizeof ( b ) );
    if ( tzx_block_create_csw_recording ( CSW_RATE, 0, g_pulses, PULSE_COUNT, &b ) != TZX_OK ) {
        CHECK ( 0, "create block for the generator test" );
        return;
    }
    st_TZX_CSW_RECORDING p;
    en_TZX_ERROR e = tzx_parse_csw_recording ( b.data, b.length, &p );

    st_TZX_CONFIG cfg;
    tzx_config_init ( &cfg );
    cfg.sample_rate = 44100;
    st_CMT_VSTREAM *vs = ( e == TZX_OK ) ? tzx_generate_csw_recording ( &p, &cfg, &e ) : NULL;
    CHECK ( vs && e == TZX_OK && runs_match ( vs, g_pulses, PULSE_COUNT ),
            "44100 Hz: runs 10, 20, 300, 70000, 5 alternating from LOW" );
    cmt_vstream_destroy ( vs );

    cfg.sample_rate = 17721600;
    vs = tzx_generate_csw_recording ( &p, &cfg, &e );
    uint64_t src = 0;
    for ( uint32_t i = 0; i < PULSE_COUNT; i++ ) src += g_pulses[i];
    double t_src = ( double ) src / CSW_RATE;
    double t_out = vs ? ( double ) cmt_vstream_get_count_scans ( vs ) / 17721600.0 : -1.0;
    printf ( "  17721600 Hz: %llu samples, source %.9f s, output %.9f s\n",
             vs ? ( unsigned long long ) cmt_vstream_get_count_scans ( vs ) : 0ULL, t_src, t_out );
    CHECK ( vs && fabs ( t_out - t_src ) <= 1.0 / 17721600.0, "17721600 Hz: total length equal within 1 sample" );
    /* úrovně se střídají od LOW, 5 běhů */
    int ok = 0;
    if ( vs ) {
        uint64_t len;
        int v;
        uint32_t i = 0;
        ok = 1;
        cmt_vstream_read_reset ( vs );
        while ( cmt_vstream_read_pulse ( vs, &len, &v ) == EXIT_SUCCESS ) {
            if ( v != ( int ) ( i & 1 ) ) ok = 0;
            i++;
        }
        if ( i != PULSE_COUNT ) ok = 0;
    }
    CHECK ( ok, "17721600 Hz: 5 runs alternating from LOW" );
    cmt_vstream_destroy ( vs );

    /* podvzorkování: pulzy kratší než výstupní vzorek mají 1 vzorek, delší pulz posun vyrovná */
    {
        static const uint32_t shorts[] = { 1, 1, 1, 1000 };
        st_TZX_BLOCK sb;
        memset ( &sb, 0, sizeof ( sb ) );
        st_TZX_CSW_RECORDING sp;
        st_CMT_VSTREAM *svs = NULL;
        cfg.sample_rate = 8000;
        if ( tzx_block_create_csw_recording ( CSW_RATE, 0, shorts, 4, &sb ) == TZX_OK
             && tzx_parse_csw_recording ( sb.data, sb.length, &sp ) == TZX_OK ) {
            svs = tzx_generate_csw_recording ( &sp, &cfg, &e );
        }
        /* konce 1, 2, 3, 1003 vzorků při 44 100 Hz -> 0, 0, 1, 182 při 8000 Hz; zapíše se 1, 1, 1, 179 */
        static const uint32_t exp_s[] = { 1, 1, 1, 179 };
        CHECK ( svs && runs_match ( svs, exp_s, 4 ), "8000 Hz: short pulses get 1 sample, levels alternate, total 182 samples" );
        cmt_vstream_destroy ( svs );
        free ( sb.data );
    }

    /* tzx_generate_block: první pulz LOW i při vstupní úrovni HIGH, po bloku opak posledního pulzu */
    uint8_t lv = 1;
    cfg.sample_rate = 44100;
    vs = tzx_generate_block ( &b, &cfg, &lv, &e );
    CHECK ( vs && e == TZX_OK && runs_match ( vs, g_pulses, PULSE_COUNT ) && lv == 1,
            "tzx_generate_block 0x18: first pulse LOW regardless of input level, level after = opposite of last pulse" );
    cmt_vstream_destroy ( vs );

    free ( b.data );

    /* 4 pulzy (poslední HIGH) -> úroveň po bloku LOW; pauza 10 ms -> TZX_LEVEL_AFTER_PAUSE */
    memset ( &b, 0, sizeof ( b ) );
    if ( tzx_block_create_csw_recording ( CSW_RATE, 10, g_pulses, 4, &b ) == TZX_OK ) {
        lv = 0;
        vs = tzx_generate_block ( &b, &cfg, &lv, &e );
        /* po 4 pulzech je úroveň nízká: pauza (441 vzorků) je celá LOW, bez 1 ms HIGH */
        static const uint32_t exp4[] = { 10, 20, 300, 70000, 441 };
        CHECK ( vs && runs_match ( vs, exp4, 5 ) && lv == TZX_LEVEL_AFTER_PAUSE,
                "4 pulses + pause 10 ms: pause whole LOW, level after = TZX_LEVEL_AFTER_PAUSE" );
        cmt_vstream_destroy ( vs );
        free ( b.data );
    } else {
        CHECK ( 0, "create block with 4 pulses" );
    }
}


/** @brief Krok 3: soubor s jedním blokem 0x18 a pauzou 500 ms přes tmz_player_play_next(). */
static void test_player ( void ) {
    st_TZX_BLOCK b;
    memset ( &b, 0, sizeof ( b ) );
    if ( tzx_block_create_csw_recording ( CSW_RATE, 500, g_pulses, PULSE_COUNT, &b ) != TZX_OK ) {
        CHECK ( 0, "create block for the player test" );
        return;
    }
    en_TZX_ERROR te;
    st_TZX_FILE *f = save_load ( "temp/csw_one_block.tzx", &b, &te );
    CHECK ( f && te == TZX_OK && f->block_count == 1 && f->blocks[0].id == 0x18 && f->blocks[0].length == b.length
            && memcmp ( f->blocks[0].data, b.data, b.length ) == 0, "tzx_save + tzx_load keep the block 0x18 byte for byte" );
    free ( b.data );
    if ( !f ) return;

    st_TMZ_PLAYER_CONFIG cfg;
    tmz_player_config_init ( &cfg );
    st_TMZ_PLAYER_STATE st;
    tmz_player_state_init ( &st, f, &cfg );
    en_TMZ_PLAYER_ERROR e;
    st_CMT_STREAM *s = tmz_player_play_next ( &st, &e );
    /* poslední pulz (5) LOW -> pauza při vysoké úrovni: 1 ms (44 vzorků) HIGH, zbytek z 22050 LOW */
    static const uint32_t exp[] = { 10, 20, 300, 70000, 5, 44, 22006 };
    CHECK ( s && e == TMZ_PLAYER_OK && s->stream_type == CMT_STREAM_TYPE_VSTREAM
            && runs_match ( s->str.vstream, exp, 7 ), "play_next: stream with the pulses and the 500 ms pause" );
    cmt_stream_destroy ( s );
    s = tmz_player_play_next ( &st, &e );
    CHECK ( !s && e == TMZ_PLAYER_OK && tmz_player_state_finished ( &st ), "play_next: then end of tape" );
    cmt_stream_destroy ( s );

    /* bezstavové API přijme 0x18 */
    s = tmz_player_play_tzx_block ( &f->blocks[0], &cfg, &e );
    CHECK ( s && e == TMZ_PLAYER_OK, "tmz_player_play_tzx_block accepts 0x18" );
    cmt_stream_destroy ( s );
    s = tmz_player_play_block ( &f->blocks[0], &cfg, &e );
    CHECK ( s && e == TMZ_PLAYER_OK, "tmz_player_play_block accepts 0x18" );
    cmt_stream_destroy ( s );

    tzx_free ( f );
}


/** @brief Odmítnutí neplatných vstupů zápisu a neplatných dat při generování. */
static void test_errors ( void ) {
    st_TZX_BLOCK b;
    memset ( &b, 0, sizeof ( b ) );
    static const uint32_t zero[] = { 10, 0, 5 };

    tzx_set_error_callback ( count_cb );

    CHECK ( tzx_block_create_csw_recording ( CSW_RATE, 0, NULL, 3, &b ) == TZX_ERROR_NULL_INPUT, "create: NULL pulses" );
    CHECK ( tzx_block_create_csw_recording ( CSW_RATE, 0, g_pulses, PULSE_COUNT, NULL ) == TZX_ERROR_NULL_INPUT,
            "create: NULL block" );
    CHECK ( tzx_block_create_csw_recording ( CSW_RATE, 0, g_pulses, 0, &b ) == TZX_ERROR_NULL_INPUT,
            "create: zero pulse count" );
    CHECK ( tzx_block_create_csw_recording ( CSW_RATE, 0, zero, 3, &b ) == TZX_ERROR_INVALID_BLOCK,
            "create: zero-length pulse" );
    CHECK ( tzx_block_create_csw_recording ( 0, 0, g_pulses, PULSE_COUNT, &b ) == TZX_ERROR_INVALID_BLOCK,
            "create: sample rate 0" );
    CHECK ( tzx_block_create_csw_recording ( 0x1000000u, 0, g_pulses, PULSE_COUNT, &b ) == TZX_ERROR_INVALID_BLOCK,
            "create: sample rate above 24 bits" );
    CHECK ( b.data == NULL && b.length == 0 && b.id == 0, "create: block unchanged on error" );
    en_TZX_ERROR e = tzx_block_create_csw_recording ( 0xFFFFFFu, 0, g_pulses, PULSE_COUNT, &b );
    CHECK ( e == TZX_OK && b.data && b.data[6] == 0xFF && b.data[7] == 0xFF && b.data[8] == 0xFF,
            "create: sample rate 16777215 (24-bit maximum) accepted" );
    free ( b.data );

    st_TZX_CONFIG cfg;
    tzx_config_init ( &cfg );
    uint8_t raw[64];
    st_TZX_CSW_RECORDING p;
    st_CMT_VSTREAM *vs;
    static const uint8_t two[] = { 10, 20 };

    /* Z-RLE: nepodporováno, s hlášením přes error callback */
    g_cb_count = 0;
    uint32_t n = raw_csw ( raw, CSW_RATE, 2, 2, two, 2 );
    tzx_parse_csw_recording ( raw, n, &p );
    vs = tzx_generate_csw_recording ( &p, &cfg, &e );
    CHECK ( !vs && e == TZX_ERROR_UNSUPPORTED && g_cb_count == 1, "generate: Z-RLE -> TZX_ERROR_UNSUPPORTED with a message" );

    n = raw_csw ( raw, CSW_RATE, 3, 2, two, 2 );
    tzx_parse_csw_recording ( raw, n, &p );
    vs = tzx_generate_csw_recording ( &p, &cfg, &e );
    CHECK ( !vs && e == TZX_ERROR_INVALID_BLOCK, "generate: unknown compression type -> TZX_ERROR_INVALID_BLOCK" );

    n = raw_csw ( raw, 0, 1, 2, two, 2 );
    tzx_parse_csw_recording ( raw, n, &p );
    vs = tzx_generate_csw_recording ( &p, &cfg, &e );
    CHECK ( !vs && e == TZX_ERROR_INVALID_BLOCK, "generate: sample rate 0 -> TZX_ERROR_INVALID_BLOCK" );

    n = raw_csw ( raw, CSW_RATE, 1, 3, two, 2 );
    tzx_parse_csw_recording ( raw, n, &p );
    vs = tzx_generate_csw_recording ( &p, &cfg, &e );
    CHECK ( !vs && e == TZX_ERROR_INVALID_BLOCK, "generate: fewer pulses in data than the header says" );

    n = raw_csw ( raw, CSW_RATE, 1, 1, two, 2 );
    tzx_parse_csw_recording ( raw, n, &p );
    vs = tzx_generate_csw_recording ( &p, &cfg, &e );
    CHECK ( !vs && e == TZX_ERROR_INVALID_BLOCK, "generate: more data than the header pulse count" );

    static const uint8_t cut[] = { 10, 0x00, 0x2C, 0x01 };
    n = raw_csw ( raw, CSW_RATE, 1, 2, cut, sizeof ( cut ) );
    tzx_parse_csw_recording ( raw, n, &p );
    vs = tzx_generate_csw_recording ( &p, &cfg, &e );
    CHECK ( !vs && e == TZX_ERROR_INVALID_BLOCK, "generate: truncated 32-bit length after 00h" );

    static const uint8_t z32[] = { 10, 0x00, 0x00, 0x00, 0x00, 0x00 };
    n = raw_csw ( raw, CSW_RATE, 1, 2, z32, sizeof ( z32 ) );
    tzx_parse_csw_recording ( raw, n, &p );
    vs = tzx_generate_csw_recording ( &p, &cfg, &e );
    CHECK ( !vs && e == TZX_ERROR_INVALID_BLOCK, "generate: zero 32-bit length -> TZX_ERROR_INVALID_BLOCK" );

    CHECK ( !tzx_generate_csw_recording ( NULL, &cfg, &e ) && e == TZX_ERROR_NULL_INPUT, "generate: NULL block" );

    tzx_set_error_callback ( NULL );
}


int main ( void ) {
    test_roundtrip ();
    test_generator ();
    test_player ();
    test_errors ();
    printf ( "\n=== TZX CSW Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
