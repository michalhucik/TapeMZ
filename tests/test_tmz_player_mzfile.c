/**
 * @file   test_tmz_player_mzfile.c
 * @brief  Player a blok 0x46 (spec 4.4, 5.3, 5.4): délka streamu, neznámá
 *         rodina části, výchozí časová základna z bloku 0x43, kontrola
 *         čtecího bodu po převodu na vzorky, bloky v1.0 přes převod
 *         na 0x46 (spec 9.1, 9.3).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/tzx/tzx.h"
#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_mzfile.h"
#include "libs/tmz/tmz_tlv.h"
#include "libs/tmz/tmz_player.h"
#include "libs/tmz/tmz_profile.h"
#include "libs/tmz/tmz_v1conv.h"
#include "libs/mzf/mzf.h"
#include "libs/cmt_stream/cmt_stream.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )

/** @brief TLV jedné FM části: PULSES 1000/1000/2000/2000, bez TIMEBASE, bez struktury. */
static const uint8_t g_fm_tlv[20] = { 0x01, 0x81, 0x10, 0x00, 0xE8, 0x03, 0, 0, 0xE8, 0x03, 0, 0,
                                      0xD0, 0x07, 0, 0, 0xD0, 0x07, 0, 0 };


/**
 * @brief Vytvoří blok 0x46 s tělem 1 B (0x00) a danými částmi.
 * @param parts Části.
 * @param n Počet.
 * @return Blok (uvolnit tmz_block_free).
 */
static st_TZX_BLOCK* make_block ( st_TMZ_PART *parts, uint8_t n ) {
    static uint8_t hdr[128];
    static uint8_t body[1] = { 0x00 };
    st_TMZ_MZFILE w = { "X-T", hdr, body, 1, NULL, 0, n, parts };
    return tmz_mzfile_create_block ( &w, NULL );
}


/**
 * @brief Blok TC-1.21 s předzavaděčem (Loader Y, část těla má READER) a tělem 16 B.
 * @param speed Rychlost.
 * @return Blok (uvolnit tmz_block_free), nebo NULL.
 */
static st_TZX_BLOCK* make_tc_block ( en_CMTSPEED speed ) {
    static uint8_t body[16];
    st_MZF_HEADER h;
    memset ( &h, 0, sizeof ( h ) );
    h.ftype = 0x01;
    h.fsize = sizeof ( body );
    h.fstrt = 0x1200;
    h.fexec = 0x1200;
    st_TMZ_PROFILE_ARGS a;
    tmz_profile_args_init ( &a );
    a.speed = speed;
    a.loader = 1;
    en_TMZ_PROFILE_ERROR pe;
    return tmz_profile_build_block ( "TC-1.21", &a, &h, body, sizeof ( body ), &pe );
}


/**
 * @brief Kontrola čtecího bodu při převodu na vzorky: rezerva pod 0
 *        = chyba, pod 10 µs jen varování.
 * @param cfg Konfigurace playeru (sample_rate se mění a vrací na 44100).
 */
static void test_reader_rate ( st_TMZ_PLAYER_CONFIG *cfg ) {
    en_TMZ_PLAYER_ERROR e;
    st_TZX_BLOCK *b31 = make_tc_block ( CMTSPEED_3_1 );
    st_TZX_BLOCK *b83 = make_tc_block ( CMTSPEED_8_3 );
    st_TZX_BLOCK *b21 = make_tc_block ( CMTSPEED_2_1 );
    CHECK ( b31 && b83 && b21, "TC-1.21 blocks built" );
    if ( !b31 || !b83 || !b21 ) return;

    cfg->sample_rate = 22050;
    st_CMT_STREAM *s = tmz_player_play_block ( b31, cfg, &e );
    CHECK ( s == NULL && e == TMZ_PLAYER_ERROR_UNREADABLE, "TC 3:1 at 22050 Hz refused" );
    cmt_stream_destroy ( s );
    s = tmz_player_play_block ( b83, cfg, &e );
    CHECK ( s == NULL && e == TMZ_PLAYER_ERROR_UNREADABLE, "TC 8:3 at 22050 Hz refused" );
    cmt_stream_destroy ( s );
    s = tmz_player_play_block ( b21, cfg, &e );
    CHECK ( s != NULL && e == TMZ_PLAYER_OK, "TC 2:1 at 22050 Hz played (low margin = warning)" );
    cmt_stream_destroy ( s );

    cfg->sample_rate = 44100;
    s = tmz_player_play_block ( b31, cfg, &e );
    CHECK ( s != NULL && e == TMZ_PLAYER_OK, "TC 3:1 at 44100 Hz played" );
    cmt_stream_destroy ( s );
    s = tmz_player_play_block ( b83, cfg, &e );
    CHECK ( s != NULL && e == TMZ_PLAYER_OK, "TC 8:3 at 44100 Hz played (low margin = warning)" );
    cmt_stream_destroy ( s );
    CHECK ( strcmp ( tmz_player_error_string ( TMZ_PLAYER_ERROR_UNREADABLE ), "Unknown player error" ) != 0,
            "UNREADABLE has error string" );
    tmz_block_free ( b31 );
    tmz_block_free ( b83 );
    tmz_block_free ( b21 );
}


/**
 * @brief Bloky v1.0 v playeru (spec 9.1): převod na 0x46, cesta v1.0 pro
 *        nepřevoditelné a při v1_convert = 0, poškozený blok bez pádu.
 * @param cfg Konfigurace (v1_convert se mění a vrací na 1).
 */
static void test_v1_blocks ( st_TMZ_PLAYER_CONFIG *cfg ) {
    static uint8_t body[16];
    st_MZF_HEADER h;
    memset ( &h, 0, sizeof ( h ) );
    h.ftype = 0x01;
    h.fsize = sizeof ( body );
    h.fstrt = h.fexec = 0x1200;
    en_TMZ_PLAYER_ERROR e;
    CHECK ( cfg->v1_convert == 1, "config default v1_convert 1" );

    /* 0x40 sada 800: převod = stejný stream jako přehrání převedeného bloku */
    st_TZX_BLOCK *b = tmz_block_create_mz_standard ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 0, &h, body, sizeof ( body ) );
    st_TZX_BLOCK *conv = NULL;
    st_TMZ_V1CONV_CONFIG vc;
    tmz_v1conv_config_init ( &vc );
    int ok = tmz_v1conv_block ( b, &vc, &conv ) == TMZ_V1CONV_OK;
    st_CMT_STREAM *s1 = tmz_player_play_block ( b, cfg, &e );
    st_CMT_STREAM *s2 = ok ? tmz_player_play_mz_file ( conv, cfg, &e ) : NULL;
    CHECK ( ok && s1 && s2 && cmt_stream_get_count_scans ( s1 ) == cmt_stream_get_count_scans ( s2 ),
            "0x40 played through conversion" );
    cmt_stream_destroy ( s2 );
    cfg->v1_convert = 0;
    s2 = tmz_player_play_block ( b, cfg, &e );
    CHECK ( s1 && s2 && cmt_stream_get_count_scans ( s1 ) != cmt_stream_get_count_scans ( s2 ),
            "v1_convert = 0 keeps the v1.0 path (different output)" );
    cfg->v1_convert = 1;
    cmt_stream_destroy ( s1 );
    cmt_stream_destroy ( s2 );
    tmz_block_free ( conv );
    tmz_block_free ( b );

    /* nepřevoditelný 0x41 FSK hraje cestou v1.0 */
    st_TMZ_MZ_TURBO_DATA td;
    memset ( &td, 0, sizeof ( td ) );
    td.format = TMZ_FORMAT_FSK;
    td.pulseset = TMZ_PULSESET_800;
    td.mzf_header = h;
    b = tmz_block_create_mz_turbo ( &td, body, sizeof ( body ) );
    s1 = tmz_player_play_block ( b, cfg, &e );
    CHECK ( s1 && e == TMZ_PLAYER_OK, "0x41 FSK played on the v1.0 path" );
    cmt_stream_destroy ( s1 );
    tmz_block_free ( b );

    /*
     * Převedený blok, který generátor FM nevyrobí (NORMAL 1:1
     * při 3000 Hz: SHORT 0,75 vzorku, mztape zaokrouhlí na 1), se přehraje cestou v1.0
     */
    td.format = TMZ_FORMAT_NORMAL;
    td.speed = CMTSPEED_1_1;
    b = tmz_block_create_mz_turbo ( &td, body, sizeof ( body ) );
    uint32_t rate = cfg->sample_rate;
    cfg->sample_rate = 3000;
    s1 = tmz_player_play_block ( b, cfg, &e );
    CHECK ( s1 && e == TMZ_PLAYER_OK, "converted block unplayable by FM -> v1.0 path" );
    cmt_stream_destroy ( s1 );
    cfg->sample_rate = rate;
    tmz_block_free ( b );

    /* poškozený 0x40: chyba, žádný pád */
    st_TZX_BLOCK bad = { TMZ_BLOCK_ID_MZ_STANDARD_DATA, 10, body };
    s1 = tmz_player_play_block ( &bad, cfg, &e );
    CHECK ( s1 == NULL && e != TMZ_PLAYER_OK, "truncated 0x40 -> error" );

    /* 0x42 se přeskočí s varováním (spec 9.3) */
    b = tmz_block_create_mz_extra_body ( TMZ_FORMAT_NORMAL, CMTSPEED_1_1, 0, body, sizeof ( body ) );
    s1 = tmz_player_play_block ( b, cfg, &e );
    CHECK ( b && s1 == NULL && e == TMZ_PLAYER_OK, "0x42 skipped with a warning" );
    tmz_block_free ( b );
}


int main ( void ) {
    st_TMZ_PLAYER_CONFIG cfg;
    tmz_player_config_init ( &cfg );
    cfg.sample_rate = 44100;
    cfg.stream_type = CMT_STREAM_TYPE_VSTREAM;
    CHECK ( cfg.mz_cpu_clock == 0, "config default mz_cpu_clock 0" );

    /* jedna FM část: 1 start L + 8 S = 1 x 4000 + 8 x 2000 = 20000 jednotek T */
    st_TMZ_PART fm = { TMZ_PART_ROLE_BODY, TMZ_FAMILY_FM, TMZ_SOURCE_MZF_BODY, 0, 0, NULL, 0, g_fm_tlv, 20 };
    st_TZX_BLOCK *b = make_block ( &fm, 1 );
    en_TMZ_PLAYER_ERROR e;
    st_CMT_STREAM *s = tmz_player_play_block ( b, &cfg, &e );
    uint64_t expect = ( 20000ull * 44100 + 3546895 / 2 ) / 3546895;
    CHECK ( s && e == TMZ_PLAYER_OK && cmt_stream_get_count_scans ( s ) == expect, "0x46 FM part length (T, default clock)" );
    cmt_stream_destroy ( s );

    /* bitstream výstup má stejnou délku */
    cfg.stream_type = CMT_STREAM_TYPE_BITSTREAM;
    s = tmz_player_play_block ( b, &cfg, &e );
    CHECK ( s && cmt_stream_get_stream_type ( s ) == CMT_STREAM_TYPE_BITSTREAM && cmt_stream_get_count_scans ( s ) == expect,
            "0x46 as bitstream" );
    cmt_stream_destroy ( s );
    cfg.stream_type = CMT_STREAM_TYPE_VSTREAM;

    /* výchozí takt z konfigurace (= z bloku 0x43): 1 MHz -> 20000 µs */
    cfg.mz_cpu_clock = 1000000;
    s = tmz_player_play_block ( b, &cfg, &e );
    expect = ( 20000ull * 44100 + 500000 ) / 1000000;
    CHECK ( s && cmt_stream_get_count_scans ( s ) == expect, "default timebase from config" );
    cmt_stream_destroy ( s );
    cfg.mz_cpu_clock = 0;
    tmz_block_free ( b );

    /*
     * Část MANCHESTER: HALVES 1000, bez pilotu a flagu, tělo 1 B
     * (00h) = 8 bitů x 2000 = 16000 T. Za FM částí: 20000 + 16000 T.
     */
    {
        static const uint8_t mt[20] = { 0x01, 0x85, 0x10, 0x00, 0xE8, 0x03, 0, 0, 0xE8, 0x03, 0, 0,
                                        0xE8, 0x03, 0, 0, 0xE8, 0x03, 0, 0 };
        st_TMZ_PART mp = { TMZ_PART_ROLE_BODY, TMZ_FAMILY_MANCHESTER, TMZ_SOURCE_MZF_BODY, 0, 0, NULL, 0, mt, 20 };
        b = make_block ( &mp, 1 );
        s = tmz_player_play_block ( b, &cfg, &e );
        expect = ( 16000ull * 44100 + 3546895 / 2 ) / 3546895;
        CHECK ( s && e == TMZ_PLAYER_OK && cmt_stream_get_count_scans ( s ) == expect, "0x46 MANCHESTER part length" );
        cmt_stream_destroy ( s );
        tmz_block_free ( b );
        st_TMZ_PART mix[2] = { fm, mp };
        b = make_block ( mix, 2 );
        s = tmz_player_play_block ( b, &cfg, &e );
        uint64_t got = s ? cmt_stream_get_count_scans ( s ) : 0;
        uint64_t sum = ( 20000ull * 44100 + 3546895 / 2 ) / 3546895 + expect;
        CHECK ( s && ( got == sum || got + 1 == sum || got == sum + 1 ), "FM + MANCHESTER parts played together" );
        cmt_stream_destroy ( s );
        tmz_block_free ( b );
    }

    /* část neznámé rodiny + FM část: stream jen z FM, bez pádu */
    st_TMZ_PART two[2] = { { TMZ_PART_ROLE_OTHER, 99, TMZ_SOURCE_MZF_BODY, 0, 0, NULL, 0, NULL, 0 }, fm };
    b = make_block ( two, 2 );
    s = tmz_player_play_block ( b, &cfg, &e );
    expect = ( 20000ull * 44100 + 3546895 / 2 ) / 3546895;
    CHECK ( s && cmt_stream_get_count_scans ( s ) == expect, "unknown family part skipped, FM part played" );
    cmt_stream_destroy ( s );
    tmz_block_free ( b );

    /* žádná přehratelná část -> chyba */
    b = make_block ( two, 1 );
    s = tmz_player_play_block ( b, &cfg, &e );
    CHECK ( s == NULL && e == TMZ_PLAYER_ERROR_UNSUPPORTED, "no playable part -> error" );
    tmz_block_free ( b );

    /* stavový automat: 0x43 (1 MHz) před 0x46 */
    st_TZX_FILE file;
    memset ( &file, 0, sizeof ( file ) );
    st_TZX_BLOCK *mi = tmz_block_create_mz_machine_info ( TMZ_MACHINE_MZ800, 1000000, 0 );
    st_TZX_BLOCK *b2 = make_block ( &fm, 1 );
    st_TZX_BLOCK blocks[2] = { *mi, *b2 };
    file.blocks = blocks;
    file.block_count = 2;
    st_TMZ_PLAYER_STATE st;
    tmz_player_state_init ( &st, &file, &cfg );
    s = tmz_player_play_next ( &st, &e );
    expect = ( 20000ull * 44100 + 500000 ) / 1000000;
    CHECK ( s && cmt_stream_get_count_scans ( s ) == expect, "state machine applies 0x43 clock to 0x46" );
    cmt_stream_destroy ( s );
    tmz_block_free ( mi );
    tmz_block_free ( b2 );

    /* dva bloky 0x43 s různým taktem, každý platí pro následující 0x46 */
    {
        st_TZX_BLOCK *m1 = tmz_block_create_mz_machine_info ( TMZ_MACHINE_MZ800, 1000000, 0 );
        st_TZX_BLOCK *m2 = tmz_block_create_mz_machine_info ( TMZ_MACHINE_MZ800, 2000000, 0 );
        st_TZX_BLOCK *f1 = make_block ( &fm, 1 );
        st_TZX_BLOCK *f2 = make_block ( &fm, 1 );
        uint64_t n1 = 0, n2 = 0;
        if ( m1 && m2 && f1 && f2 ) {
            st_TZX_BLOCK bl4[4] = { *m1, *f1, *m2, *f2 };
            file.blocks = bl4;
            file.block_count = 4;
            tmz_player_state_init ( &st, &file, &cfg );
            s = tmz_player_play_next ( &st, &e );
            if ( s ) n1 = cmt_stream_get_count_scans ( s );
            cmt_stream_destroy ( s );
            s = tmz_player_play_next ( &st, &e );
            if ( s ) n2 = cmt_stream_get_count_scans ( s );
            cmt_stream_destroy ( s );
        }
        CHECK ( n1 == ( 20000ull * 44100 + 500000 ) / 1000000 && n2 == ( 20000ull * 44100 + 1000000 ) / 2000000,
                "second 0x43 changes the clock for the following 0x46" );
        tmz_block_free ( m1 );
        tmz_block_free ( m2 );
        tmz_block_free ( f1 );
        tmz_block_free ( f2 );
    }

    /* TIMEBASE v části přebije takt z 0x43 (mz_cpu_clock) */
    {
        static uint8_t tb_tlv[8 + sizeof ( g_fm_tlv )] = { 0x01, 0x80, 0x04, 0x00, 0x40, 0x42, 0x0F, 0x00 };   /* TIMEBASE 1 MHz */
        memcpy ( tb_tlv + 8, g_fm_tlv, sizeof ( g_fm_tlv ) );
        st_TMZ_PART tp = { TMZ_PART_ROLE_BODY, TMZ_FAMILY_FM, TMZ_SOURCE_MZF_BODY, 0, 0, NULL, 0, tb_tlv, sizeof ( tb_tlv ) };
        st_TZX_BLOCK *bt = make_block ( &tp, 1 );
        cfg.mz_cpu_clock = 2000000;
        s = bt ? tmz_player_play_block ( bt, &cfg, &e ) : NULL;
        CHECK ( s && cmt_stream_get_count_scans ( s ) == ( 20000ull * 44100 + 500000 ) / 1000000,
                "part TIMEBASE overrides the 0x43 clock" );
        cmt_stream_destroy ( s );
        cfg.mz_cpu_clock = 0;
        if ( bt ) tmz_block_free ( bt );
    }

    /* blok z profilu s TIMEBASE nepřeškáluje 0x43 jiného taktu */
    {
        static uint8_t body[64];
        st_MZF_HEADER h;
        memset ( &h, 0, sizeof ( h ) );
        h.ftype = 1;
        h.fsize = sizeof ( body );
        h.fstrt = 0x1200;
        h.fexec = 0x1200;
        st_TZX_BLOCK *ic = tmz_profile_build_block ( "IC-10.2-TURBO", NULL, &h, body, sizeof ( body ), NULL );
        st_TZX_BLOCK *mi7 = tmz_block_create_mz_machine_info ( TMZ_MACHINE_MZ700, 1000000, 0 );
        uint64_t plain = 0, after = 0;
        s = ic ? tmz_player_play_block ( ic, &cfg, &e ) : NULL;
        if ( s ) plain = cmt_stream_get_count_scans ( s );
        cmt_stream_destroy ( s );
        if ( ic && mi7 ) {
            st_TZX_BLOCK bl[2] = { *mi7, *ic };
            file.blocks = bl;
            file.block_count = 2;
            tmz_player_state_init ( &st, &file, &cfg );
            s = tmz_player_play_next ( &st, &e );
            if ( s ) after = cmt_stream_get_count_scans ( s );
            cmt_stream_destroy ( s );
        }
        CHECK ( plain > 0 && plain == after, "IC-10.2-TURBO block keeps its timing after 0x43 with 1 MHz" );
        if ( ic ) tmz_block_free ( ic );
        if ( mi7 ) tmz_block_free ( mi7 );
    }

    test_reader_rate ( &cfg );
    test_v1_blocks ( &cfg );

    printf ( "\n=== TMZ Player MZ File Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
