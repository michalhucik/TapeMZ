/**
 * @file   test_turbo_readpoint.c
 * @brief  Invariant TURBO: generované pulzy musí odpovídat čtecímu bodu N.
 *
 * Pro profil TurboCopy, rychlosti 2:1, 7:3, 8:3, 3:1, 3:2, 9:7 a frekvence
 * 44100/48000/96000 Hz ověří rezervy >= MZCMT_TURBO_MIN_MARGIN_US.
 * Při 44100 Hz porovná N a šířky s nahrávkou TurboCopy V1.21 z emulátoru
 * (tstdata/tc-loader-all.wav, disassembly zavaděče TurboCopy),
 * tolerance 1 vzorek. Při 22050 Hz nesmí get_timing + check projít
 * s rezervou pod minimem. Ověří také odmítnutí 25:14 a výpočet N
 * z vlastních pulzů.
 * Bez tstdata/ vypíše SKIP a skončí 0 (tests/tstdata_skip.h).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzcmt_turbo/mzcmt_turbo.h"
#include "libs/cmtspeed/cmtspeed.h"
#include "libs/mzf/mzf.h"
#include "tstdata_skip.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Referenční hodnoty z tc-loader-all.wav při 44100 Hz (vzorky). */
typedef struct st_REF {
    en_CMTSPEED speed;  /**< rychlost */
    uint8_t n;          /**< čtecí bod z předzavaděče */
    uint32_t sh;        /**< SHORT HIGH */
    uint32_t lh;        /**< LONG HIGH */
} st_REF;

/** @brief Nahrávka TurboCopy V1.21 (emulátor): N ověřeno, šířky měřeny. */
static const st_REF g_ref[] = {
    { CMTSPEED_2_1, 41, 6, 12 },
    { CMTSPEED_7_3, 35, 5, 11 },
    { CMTSPEED_8_3, 30, 5, 10 },
    { CMTSPEED_3_1, 27, 5, 9 },
};


/** @brief |a - b| <= tol pro uint32_t. */
static int near ( uint32_t a, uint32_t b, uint32_t tol ) {
    return ( a > b ? a - b : b - a ) <= tol;
}


/**
 * @brief Předzavaděč TurboCopy proti bajtům zachyceným v emulátoru (3/1, N=27).
 */
static void test_preloader ( void ) {
    uint8_t mzf[1152], ref_hdr[128], ref_body[90];
    FILE *f = fopen ( "../tstdata/ref/emutest.mzf", "rb" );
    size_t n1 = f ? fread ( mzf, 1, sizeof ( mzf ), f ) : 0;
    if ( f ) fclose ( f );
    f = fopen ( "../tstdata/ref/tc121-preloader-header-emutest-3to1.bin", "rb" );
    size_t n2 = f ? fread ( ref_hdr, 1, 128, f ) : 0;
    if ( f ) fclose ( f );
    f = fopen ( "../tstdata/ref/tc121-preloader-body-emutest-3to1.bin", "rb" );
    size_t n3 = f ? fread ( ref_body, 1, 90, f ) : 0;
    if ( f ) fclose ( f );
    if ( n1 != 1152 || n2 != 128 || n3 != 90 ) {
        printf ( "preloader reference files -> FAIL\n" );
        g_failures++;
        return;
    }
    st_MZF_HEADER hdr;
    memcpy ( &hdr, mzf, 128 );
    mzf_header_items_correction ( &hdr );   /* LE -> hostitel (nastaví i fname.terminator 11h na 0Dh) */
    uint8_t out_hdr[128], out_body[MZCMT_TURBO_PRELOADER_SIZE];
    int ok = mzcmt_turbo_build_preloader ( &hdr, 1024, 27, out_hdr, out_body ) == EXIT_SUCCESS
             && memcmp ( out_hdr, ref_hdr, 128 ) == 0 && memcmp ( out_body, ref_body, 90 ) == 0;
    printf ( "preloader bytes match emulator capture (3/1, N=27) -> %s\n", ok ? "OK" : "FAIL" );
    if ( !ok ) g_failures++;
    unsigned a = 0, b = 0;
    ok = mzcmt_turbo_speed_to_ratio ( CMTSPEED_9_7, &a, &b ) == EXIT_SUCCESS && a == 9 && b == 7
         && mzcmt_turbo_speed_to_ratio ( CMTSPEED_25_14, &a, &b ) == EXIT_FAILURE;
    printf ( "speed_to_ratio public -> %s\n", ok ? "OK" : "FAIL" );
    if ( !ok ) g_failures++;
}


int main ( void ) {
    if ( tstdata_skip ( "test_turbo_readpoint" ) ) return 0;
    static const en_CMTSPEED speeds[] = { CMTSPEED_2_1, CMTSPEED_7_3, CMTSPEED_8_3, CMTSPEED_3_1, CMTSPEED_3_2, CMTSPEED_9_7 };
    static const uint32_t rates[] = { 44100, 48000, 96000 };

    for ( size_t s = 0; s < sizeof ( speeds ) / sizeof ( speeds[0] ); s++ ) {
        for ( size_t r = 0; r < sizeof ( rates ) / sizeof ( rates[0] ); r++ ) {
            st_MZCMT_TURBO_CONFIG cfg;
            memset ( &cfg, 0, sizeof ( cfg ) );
            cfg.pulseset = MZCMT_TURBO_PULSESET_800; /* jako tmz_player pro MZ-800 */
            cfg.speed = speeds[s];
            st_MZCMT_TURBO_TIMING t;
            st_MZCMT_TURBO_MARGINS m;
            memset ( &t, 0, sizeof ( t ) );
            memset ( &m, 0, sizeof ( m ) );
            int ok = ( mzcmt_turbo_get_timing ( &cfg, rates[r], &t ) == EXIT_SUCCESS );
            int chk = ok ? mzcmt_turbo_check_readpoint ( &t, rates[r], &m ) : EXIT_FAILURE;
            printf ( "%-5s %6u Hz: N=%u S=%u/%u L=%u/%u margins %.1f/%.1f/%.1f us -> %s\n",
                     cmtspeed_get_ratiotxt ( speeds[s] ), rates[r], t.readpoint,
                     t.short_high, t.short_low, t.long_high, t.long_low,
                     m.short_high_us, m.long_high_us, m.short_period_us,
                     ( ok && chk == EXIT_SUCCESS ) ? "OK" : "FAIL" );
            if ( !ok || chk != EXIT_SUCCESS ) g_failures++;
        }
    }

    for ( size_t i = 0; i < sizeof ( g_ref ) / sizeof ( g_ref[0] ); i++ ) {
        st_MZCMT_TURBO_CONFIG cfg;
        memset ( &cfg, 0, sizeof ( cfg ) );
        cfg.pulseset = MZCMT_TURBO_PULSESET_800; /* jako tmz_player pro MZ-800 */
        cfg.speed = g_ref[i].speed;
        st_MZCMT_TURBO_TIMING t;
        memset ( &t, 0, sizeof ( t ) );
        mzcmt_turbo_get_timing ( &cfg, 44100, &t );
        int ok = ( t.readpoint == g_ref[i].n ) && near ( t.short_high, g_ref[i].sh, 1 ) && near ( t.long_high, g_ref[i].lh, 1 );
        printf ( "REF %-5s: N=%u (exp %u) SH=%u (exp %u) LH=%u (exp %u) -> %s\n",
                 cmtspeed_get_ratiotxt ( g_ref[i].speed ), t.readpoint, g_ref[i].n,
                 t.short_high, g_ref[i].sh, t.long_high, g_ref[i].lh, ok ? "OK" : "FAIL" );
        if ( !ok ) g_failures++;
    }

    /* 22050 Hz: buď rezerva >= minimum, nebo odmítnutí - nikdy "OK" s menší rezervou */
    {
        st_MZCMT_TURBO_CONFIG cfg;
        memset ( &cfg, 0, sizeof ( cfg ) );
        cfg.pulseset = MZCMT_TURBO_PULSESET_800; /* jako tmz_player pro MZ-800 */
        cfg.speed = CMTSPEED_3_1;
        st_MZCMT_TURBO_TIMING t;
        st_MZCMT_TURBO_MARGINS m;
        memset ( &m, 0, sizeof ( m ) );
        if ( mzcmt_turbo_get_timing ( &cfg, 22050, &t ) == EXIT_SUCCESS &&
             mzcmt_turbo_check_readpoint ( &t, 22050, &m ) == EXIT_SUCCESS ) {
            int ok = m.short_high_us >= MZCMT_TURBO_MIN_MARGIN_US && m.long_high_us >= MZCMT_TURBO_MIN_MARGIN_US
                     && m.short_period_us >= MZCMT_TURBO_MIN_MARGIN_US;
            printf ( "22050 Hz 3:1: check OK, margins consistent -> %s\n", ok ? "OK" : "FAIL" );
            if ( !ok ) g_failures++;
        } else {
            printf ( "22050 Hz 3:1: rejected (allowed) -> OK\n" );
        }
    }

    /* 25:14 TurboCopy zadat neumí (číslice 1-9) - musí být odmítnuto */
    {
        st_MZCMT_TURBO_CONFIG cfg;
        memset ( &cfg, 0, sizeof ( cfg ) );
        cfg.pulseset = MZCMT_TURBO_PULSESET_800; /* jako tmz_player pro MZ-800 */
        cfg.speed = CMTSPEED_25_14;
        st_MZCMT_TURBO_TIMING t;
        int rejected = ( mzcmt_turbo_get_timing ( &cfg, 44100, &t ) != EXIT_SUCCESS );
        printf ( "25:14 rejected -> %s\n", rejected ? "OK" : "FAIL" );
        if ( !rejected ) g_failures++;
    }

    /* vlastní pulzy odpovídající TurboCopy 2:1 (0,1 us), speed 0 -> N z pulzů, kontrola projde */
    {
        st_MZCMT_TURBO_CONFIG cfg;
        memset ( &cfg, 0, sizeof ( cfg ) );
        cfg.pulseset = MZCMT_TURBO_PULSESET_800; /* jako tmz_player pro MZ-800 */
        cfg.speed = CMTSPEED_NONE;
        cfg.short_high_us100 = 1403;
        cfg.short_low_us100 = 1403;
        cfg.long_high_us100 = 2806;
        cfg.long_low_us100 = 2806;
        st_MZCMT_TURBO_TIMING t;
        st_MZCMT_TURBO_MARGINS m;
        memset ( &t, 0, sizeof ( t ) );
        memset ( &m, 0, sizeof ( m ) );
        int ok = mzcmt_turbo_get_timing ( &cfg, 44100, &t ) == EXIT_SUCCESS
                 && mzcmt_turbo_check_readpoint ( &t, 44100, &m ) == EXIT_SUCCESS
                 && t.readpoint != 82;
        printf ( "custom pulses 2:1-like: N=%u margins %.1f/%.1f/%.1f -> %s\n", t.readpoint,
                 m.short_high_us, m.long_high_us, m.short_period_us, ok ? "OK" : "FAIL" );
        if ( !ok ) g_failures++;
    }

    /* podporované poměry profilu TurboCopy (pro validaci v nástrojích) */
    {
        int ok = mzcmt_turbo_speed_is_supported ( MZCMT_TURBO_PROFILE_TURBOCOPY_V12X, CMTSPEED_2_1 )
                 && mzcmt_turbo_speed_is_supported ( MZCMT_TURBO_PROFILE_TURBOCOPY_V12X, CMTSPEED_9_7 )
                 && !mzcmt_turbo_speed_is_supported ( MZCMT_TURBO_PROFILE_TURBOCOPY_V12X, CMTSPEED_25_14 )
                 && !mzcmt_turbo_speed_is_supported ( MZCMT_TURBO_PROFILE_TURBOCOPY_V12X, CMTSPEED_2_1_CPM )
                 && !mzcmt_turbo_speed_is_supported ( MZCMT_TURBO_PROFILE_TURBOCOPY_V12X, CMTSPEED_NONE );
        printf ( "speed_is_supported -> %s\n", ok ? "OK" : "FAIL" );
        if ( !ok ) g_failures++;
    }

    /* generátor: 80B a nečitelné časování (8:3 při 22050 Hz) -> NULL */
    {
        st_MZF_HEADER hdr;
        memset ( &hdr, 0, sizeof ( hdr ) );
        hdr.ftype = 0x01;
        hdr.fsize = 16;
        hdr.fstrt = 0x1200;
        hdr.fexec = 0x1200;
        uint8_t body[16] = { 0 };
        st_MZCMT_TURBO_CONFIG cfg;
        memset ( &cfg, 0, sizeof ( cfg ) );
        cfg.pulseset = MZCMT_TURBO_PULSESET_80B;
        cfg.speed = CMTSPEED_2_1;
        st_CMT_VSTREAM *vs = mzcmt_turbo_create_tape_vstream ( &hdr, body, 16, &cfg, 44100 );
        printf ( "generator rejects 80B -> %s\n", vs == NULL ? "OK" : "FAIL" );
        if ( vs ) { g_failures++; cmt_vstream_destroy ( vs ); }

        cfg.pulseset = MZCMT_TURBO_PULSESET_800;
        cfg.speed = CMTSPEED_8_3;
        vs = mzcmt_turbo_create_tape_vstream ( &hdr, body, 16, &cfg, 22050 );
        printf ( "generator rejects 8:3 at 22050 Hz -> %s\n", vs == NULL ? "OK" : "FAIL" );
        if ( vs ) { g_failures++; cmt_vstream_destroy ( vs ); }

        vs = mzcmt_turbo_create_tape_vstream ( &hdr, body, 16, &cfg, 44100 );
        printf ( "generator accepts 8:3 at 44100 Hz -> %s\n", vs != NULL ? "OK" : "FAIL" );
        if ( !vs ) g_failures++;
        cmt_vstream_destroy ( vs );
    }

    test_preloader ();

    printf ( "\n=== TURBO readpoint Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
