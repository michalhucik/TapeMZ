/**
 * @file   test_tmz_v1legacy.c
 * @brief  Profil TAPEMZ-V1-LEGACY proti generátorům v1.0 (spec 9.2): půlvlny
 *         stejné v počtu a úrovních, délka ±1 vzorek. Spouštět z build/.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/mztape/mztape.h"
#include "libs/mzcmt_fastipl/mzcmt_fastipl.h"
#include "libs/mzcmt_bsd/mzcmt_bsd.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_mzfile.h"
#include "libs/tmz/tmz_part_fm.h"
#include "libs/tmz/tmz_profile.h"
#include "libs/tmz/tmz_player.h"
#include "tmz_halfwave_compare.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;
/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )

/** @brief Vzorkovací frekvence testu. */
#define RATE 44100
/** @brief Velikost testovacího těla. */
#define BODY 300

/** @brief Hlavička v pořadí hostitele. */
static st_MZF_HEADER g_hdr;
/** @brief Hlavička v pořadí souboru (LE), jak ji dostane mztape. */
static uint8_t g_hdr_le[128];
/** @brief Tělo. */
static uint8_t g_body[BODY];

/** @brief Naplní hlavičku a tělo (11h = 0Dh jako po mzf_header_items_correction). */
static void make_data ( void ) {
    memset ( &g_hdr, 0, sizeof ( g_hdr ) );
    uint8_t *h = ( uint8_t* ) &g_hdr;
    h[0] = 0x01;
    memcpy ( h + 1, "LEGACY TEST", 11 );
    memset ( h + 12, 0x0D, 6 );
    g_hdr.fsize = BODY;
    g_hdr.fstrt = 0x1200;
    g_hdr.fexec = 0x1200;
    memcpy ( g_hdr_le, &g_hdr, 128 );
    mzf_header_items_correction ( ( st_MZF_HEADER* ) g_hdr_le );
    for ( int i = 0; i < BODY; i++ ) g_body[i] = ( uint8_t ) ( i * 37 + 11 );
}

/** @brief Počet jedničkových bitů (checksum mztape). */
static uint32_t popcount ( const uint8_t *d, uint32_t n ) {
    uint32_t c = 0;
    for ( uint32_t i = 0; i < n; i++ ) for ( uint8_t b = d[i]; b; b >>= 1 ) c += b & 1;
    return c;
}

/**
 * @brief Vstream generátoru v1.0 mztape.
 * @param fset Formát. @param speed Rychlost. @param pul Vlastní pulzy (LH, LL, SH, SL; vše 0 = ze sady).
 * @return Stream (uvolnit cmt_stream_destroy), nebo NULL.
 */
static st_CMT_STREAM* v1_normal ( en_MZTAPE_FORMATSET fset, en_CMTSPEED speed, const uint16_t pul[4] ) {
    static st_MZTAPE_MZF m;
    memcpy ( m.header, g_hdr_le, 128 );
    m.body = g_body;
    m.size = BODY;
    m.chkh = popcount ( g_hdr_le, 128 );
    m.chkb = popcount ( g_body, BODY );
    return mztape_create_stream_from_mztapemzf_ex ( &m, speed, CMT_STREAM_TYPE_VSTREAM, fset, RATE,
                                                     pul[0], pul[1], pul[2], pul[3] );
}

/**
 * @brief Přehraje blok LEGACY sestavený z args.
 * @return Stream (uvolnit cmt_stream_destroy), nebo NULL.
 */
static st_CMT_STREAM* legacy_play ( const st_TMZ_PROFILE_ARGS *a, const uint8_t *body, uint32_t size ) {
    en_TMZ_PROFILE_ERROR pe;
    st_TZX_BLOCK *b = tmz_profile_build_block ( "TAPEMZ-V1-LEGACY", a, &g_hdr, body, size, &pe );
    if ( !b ) return NULL;
    st_TMZ_PLAYER_CONFIG cfg;
    tmz_player_config_init ( &cfg );
    cfg.sample_rate = RATE;
    cfg.stream_type = CMT_STREAM_TYPE_VSTREAM;
    en_TMZ_PLAYER_ERROR e;
    st_CMT_STREAM *s = tmz_player_play_mz_file ( b, &cfg, &e );
    tmz_block_free ( b );
    return s;
}

/** @brief Porovná LEGACY normal s mztape pro jeden případ. */
static void case_normal ( const char *label, en_MZTAPE_FORMATSET fset, int pulseset, uint32_t lgap,
                          en_CMTSPEED speed, const uint16_t pul[4] ) {
    st_TMZ_PROFILE_ARGS a;
    tmz_profile_args_init ( &a );
    a.legacy_format = TMZ_LEGACY_NORMAL;
    a.pulseset = pulseset;
    a.speed = speed;
    a.lgap = lgap;
    a.sgap = 11000;
    a.pause_ms = 0;
    memcpy ( a.pulses, pul, sizeof ( a.pulses ) );
    st_CMT_STREAM *r = v1_normal ( fset, speed, pul );
    st_CMT_STREAM *g = legacy_play ( &a, g_body, BODY );
    hw_compare ( r ? r->str.vstream : NULL, g ? g->str.vstream : NULL, label, &g_failures );
    cmt_stream_destroy ( r );
    cmt_stream_destroy ( g );
}

/** @brief Formát normal proti mztape (SANE, MZ80B, rychlosti, vlastní pulzy). */
static void test_normal ( void ) {
    static const uint16_t none[4] = { 0, 0, 0, 0 };
    static const uint16_t own[4] = { 4980, 4980, 2490, 2490 };
    case_normal ( "normal SANE 1:1", MZTAPE_FORMATSET_MZ800_SANE, 1, 6400, CMTSPEED_1_1, none );
    case_normal ( "normal SANE 2:1", MZTAPE_FORMATSET_MZ800_SANE, 1, 6400, CMTSPEED_2_1, none );
    case_normal ( "normal SANE 7:3", MZTAPE_FORMATSET_MZ800_SANE, 1, 6400, CMTSPEED_7_3, none );
    case_normal ( "normal SANE 3:1", MZTAPE_FORMATSET_MZ800_SANE, 1, 6400, CMTSPEED_3_1, none );
    case_normal ( "normal MZ80B 1:1", MZTAPE_FORMATSET_MZ80B, 2, 10000, CMTSPEED_1_1, none );
    case_normal ( "normal SANE custom pulses", MZTAPE_FORMATSET_MZ800_SANE, 1, 6400, CMTSPEED_2_1, own );

    /* pulzní sada 700: mztape ji žádným formátem nehraje, ověří se hodnoty PULSES */
    st_TMZ_PROFILE_ARGS a;
    tmz_profile_args_init ( &a );
    a.legacy_format = TMZ_LEGACY_NORMAL;
    a.pulseset = 0;
    a.pause_ms = 250;
    en_TMZ_PROFILE_ERROR pe;
    st_TZX_BLOCK *b = tmz_profile_build_block ( "TAPEMZ-V1-LEGACY", &a, &g_hdr, g_body, BODY, &pe );
    st_TMZ_MZFILE mf;
    int ok = b && tmz_mzfile_parse ( b, &mf ) == TZX_OK;
    if ( ok ) {
        static st_MZCMT_FM_PARAMS p0, p1;
        st_TMZ_PART_COMMON c0, c1;
        ok = mf.part_count == 2
             && tmz_part_fm_decode ( mf.parts[0].tlv, mf.parts[0].tlv_length, 3546895, &p0, &c0 ) == TMZ_PART_OK
             && tmz_part_fm_decode ( mf.parts[1].tlv, mf.parts[1].tlv_length, 3546895, &p1, &c1 ) == TMZ_PART_OK
             && p0.timebase == 10000000 && p0.short_first == 2520 && p0.short_second == 2520
             && p0.long_first == 5040 && p0.long_second == 5040 && p0.tone == 6400 && p1.tone == 11000
             && p0.tm_long == 40 && p1.tm_long == 20 && p0.end_long == 3 && p0.pause == 0
             && p1.pause == 2500000 && !c0.has_reader && !c0.stop;
        tmz_mzfile_free ( &mf );
    }
    CHECK ( ok, "normal 700: PULSES 252/504 us, tones 6400/11000, pause 250 ms on body" );
    if ( b ) tmz_block_free ( b );

    /* nevyjádřitelné: 2:1 CPM a částečné vlastní pulzy */
    tmz_profile_args_init ( &a );
    a.legacy_format = TMZ_LEGACY_NORMAL;
    a.speed = CMTSPEED_2_1_CPM;
    CHECK ( tmz_profile_build_block ( "TAPEMZ-V1-LEGACY", &a, &g_hdr, g_body, BODY, &pe ) == NULL
            && pe == TMZ_PROFILE_ERROR_ARGS, "normal 2:1 CPM rejected" );
    a.speed = CMTSPEED_1_1;
    a.pulses[0] = 4980;
    CHECK ( tmz_profile_build_block ( "TAPEMZ-V1-LEGACY", &a, &g_hdr, g_body, BODY, &pe ) == NULL
            && pe == TMZ_PROFILE_ERROR_ARGS, "normal partial custom pulses rejected" );
    a.legacy_format = TMZ_LEGACY_NONE;
    a.pulses[0] = 0;
    CHECK ( tmz_profile_build_block ( "TAPEMZ-V1-LEGACY", &a, &g_hdr, g_body, BODY, &pe ) == NULL
            && pe == TMZ_PROFILE_ERROR_ARGS, "LEGACY without format rejected" );
}

/** @brief Porovná LEGACY fastipl s mzcmt_fastipl (V07, readpoint a blcount automaticky). */
static void case_fastipl ( const char *label, int pulseset, en_CMTSPEED speed, uint32_t lgap, const uint16_t pul[4] ) {
    st_MZCMT_FASTIPL_CONFIG f;
    memset ( &f, 0, sizeof ( f ) );
    f.version = MZCMT_FASTIPL_VERSION_V07;
    f.pulseset = ( en_MZCMT_FASTIPL_PULSESET ) pulseset;
    f.speed = speed;
    f.lgap_length = lgap;
    f.long_high_us100 = pul[0];
    f.long_low_us100 = pul[1];
    f.short_high_us100 = pul[2];
    f.short_low_us100 = pul[3];
    st_CMT_VSTREAM *r = mzcmt_fastipl_create_vstream ( &g_hdr, g_body, BODY, &f, RATE );
    st_TMZ_PROFILE_ARGS a;
    tmz_profile_args_init ( &a );
    a.legacy_format = TMZ_LEGACY_FASTIPL;
    a.pulseset = pulseset;
    a.speed = speed;
    a.lgap = lgap;
    a.pause_ms = 0;
    memcpy ( a.pulses, pul, sizeof ( a.pulses ) );
    st_CMT_STREAM *g = legacy_play ( &a, g_body, BODY );
    hw_compare ( r, g ? g->str.vstream : NULL, label, &g_failures );
    cmt_vstream_destroy ( r );
    cmt_stream_destroy ( g );
}

/** @brief Formát fastipl (sady 700, 800 asymetrická, 80B; rychlosti; LGAP těla; vlastní pulzy po polích). */
static void test_fastipl ( void ) {
    static const uint16_t none[4] = { 0, 0, 0, 0 };
    static const uint16_t sh_only[4] = { 0, 0, 1200, 0 };
    case_fastipl ( "fastipl 800 1:1", 1, CMTSPEED_1_1, 0, none );
    case_fastipl ( "fastipl 800 2:1", 1, CMTSPEED_2_1, 0, none );
    case_fastipl ( "fastipl 800 3:1", 1, CMTSPEED_3_1, 0, none );
    case_fastipl ( "fastipl 700 7:3", 0, CMTSPEED_7_3, 0, none );
    case_fastipl ( "fastipl 80B 1:1", 2, CMTSPEED_1_1, 0, none );
    case_fastipl ( "fastipl 800 2:1 lgap 3000", 1, CMTSPEED_2_1, 3000, none );
    case_fastipl ( "fastipl 800 2:1 custom SH only", 1, CMTSPEED_2_1, 0, sh_only );
}

/** @brief Porovná LEGACY bsd s mzcmt_bsd (n kusů). */
static void case_bsd ( const char *label, int pulseset, uint16_t n ) {
    uint32_t size = ( uint32_t ) n * 258;
    uint8_t *chunks = malloc ( size );
    if ( !chunks ) {
        printf ( "FAIL: %s (alloc)\n", label );
        g_failures++;
        return;
    }
    for ( uint32_t i = 0; i < size; i++ ) chunks[i] = ( uint8_t ) ( i * 13 + 5 );
    chunks[size - 258] = 0xFF;   /* poslední kus má ID FFFFh */
    chunks[size - 257] = 0xFF;
    st_MZCMT_BSD_CONFIG c;
    memset ( &c, 0, sizeof ( c ) );
    c.pulseset = ( en_MZCMT_BSD_PULSESET ) pulseset;
    c.speed = CMTSPEED_1_1;
    st_CMT_VSTREAM *r = mzcmt_bsd_create_vstream ( g_hdr_le, chunks, n, &c, RATE );
    st_TMZ_PROFILE_ARGS a;
    tmz_profile_args_init ( &a );
    a.legacy_format = TMZ_LEGACY_BSD;
    a.pulseset = pulseset;
    a.pause_ms = 0;
    st_CMT_STREAM *g = legacy_play ( &a, chunks, size );
    hw_compare ( r, g ? g->str.vstream : NULL, label, &g_failures );
    cmt_vstream_destroy ( r );
    cmt_stream_destroy ( g );
    free ( chunks );
}

/** @brief Formát bsd (sady, 1 a 3 kusy, hranice 254 kusů, tělo nedělitelné 258). */
static void test_bsd ( void ) {
    case_bsd ( "bsd 800 3 chunks", 1, 3 );
    case_bsd ( "bsd 700 1 chunk", 0, 1 );
    case_bsd ( "bsd 80B 3 chunks", 2, 3 );
    case_bsd ( "bsd 800 254 chunks", 1, 254 );

    st_TMZ_PROFILE_ARGS a;
    tmz_profile_args_init ( &a );
    a.legacy_format = TMZ_LEGACY_BSD;
    en_TMZ_PROFILE_ERROR pe;
    CHECK ( tmz_profile_build_block ( "TAPEMZ-V1-LEGACY", &a, &g_hdr, g_body, 257, &pe ) == NULL
            && pe == TMZ_PROFILE_ERROR_ARGS, "bsd body not a multiple of 258 rejected" );
    a.pulses[2] = 1200;
    CHECK ( tmz_profile_build_block ( "TAPEMZ-V1-LEGACY", &a, &g_hdr, g_body, 258, &pe ) == NULL
            && pe == TMZ_PROFILE_ERROR_ARGS, "bsd custom pulses rejected" );
}

int main ( void ) {
    make_data ();
    test_normal ();
    test_fastipl ();
    test_bsd ();
    printf ( "\n=== TMZ v1 Legacy Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
