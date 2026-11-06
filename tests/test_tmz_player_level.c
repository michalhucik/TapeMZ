/**
 * @file   test_tmz_player_level.c
 * @brief  Úroveň signálu, zastavení pásky a rezervované bloky v tmz_player
 *        . Data nepotřebuje.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/tzx/tzx.h"
#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_player.h"
#include "libs/tmz/tmz_profile.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/cmt_stream/cmt_vstream.h"
#include "libs/mzf/mzf.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše OK/FAIL. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )

/** @brief 0x12: 1 pulz 100 T. */
static uint8_t g_tone1[] = { 100, 0, 1, 0 };
/** @brief 0x12: 5 pulzů 100 T. */
static uint8_t g_tone5[] = { 100, 0, 5, 0 };
/** @brief 0x13: 2 pulzy. */
static uint8_t g_seq2[] = { 2, 100, 0, 100, 0 };
/** @brief 0x20: pauza 10 ms. */
static uint8_t g_pause10[] = { 10, 0 };
/** @brief 0x2B: úroveň 0 (DWORD délka 1 + BYTE). */
static uint8_t g_level0[] = { 1, 0, 0, 0, 0 };
/** @brief 0x2B: úroveň 1. */
static uint8_t g_level1[] = { 1, 0, 0, 0, 1 };
/** @brief 0x2B: hodnota 2 (bere se jako vysoká). */
static uint8_t g_level2[] = { 1, 0, 0, 0, 2 };
/** @brief 0x24 Loop Start: 2 opakování. */
static uint8_t g_loop2[] = { 2, 0 };


/**
 * @brief Sestaví blok (data nevlastní).
 * @param id ID. @param d Data. @param n Délka.
 * @return Blok.
 */
static st_TZX_BLOCK blk ( uint8_t id, uint8_t *d, uint32_t n ) {
    st_TZX_BLOCK b = { id, n, d };
    return b;
}


/**
 * @brief Hodnota prvního eventu vstreamu ve streamu.
 * @param s Stream (vstream). @return 0/1, nebo -1.
 */
static int first_level ( st_CMT_STREAM *s ) {
    uint64_t n;
    int v;
    if ( !s || s->stream_type != CMT_STREAM_TYPE_VSTREAM ) return -1;
    cmt_vstream_read_reset ( s->str.vstream );
    return cmt_vstream_read_pulse ( s->str.vstream, &n, &v ) == EXIT_SUCCESS ? v : -1;
}


/**
 * @brief Počet eventů vstreamu ve streamu.
 * @param s Stream. @return Počet, 0 u jiného typu.
 */
static uint32_t count_events ( st_CMT_STREAM *s ) {
    uint64_t n;
    int v;
    uint32_t c = 0;
    if ( !s || s->stream_type != CMT_STREAM_TYPE_VSTREAM ) return 0;
    cmt_vstream_read_reset ( s->str.vstream );
    while ( cmt_vstream_read_pulse ( s->str.vstream, &n, &v ) == EXIT_SUCCESS ) c++;
    return c;
}


/**
 * @brief Přehraje soubor přes tmz_player_play_next a zapíše první úroveň
 *        a počet eventů každého streamu; zastavení zapíše jako level -2.
 * @param blocks Bloky. @param nb Počet. @param[out] lv První úrovně (max 16).
 * @param[out] ev Počty eventů (max 16).
 * @return Počet záznamů.
 */
static int play_all ( st_TZX_BLOCK *blocks, uint32_t nb, int *lv, uint32_t *ev ) {
    st_TZX_FILE f;
    memset ( &f, 0, sizeof ( f ) );
    f.blocks = blocks;
    f.block_count = nb;
    st_TMZ_PLAYER_CONFIG cfg;
    tmz_player_config_init ( &cfg );
    st_TMZ_PLAYER_STATE st;
    tmz_player_state_init ( &st, &f, &cfg );
    int k = 0;
    for ( int guard = 0; guard < 64 && !tmz_player_state_finished ( &st ) && k < 16; guard++ ) {
        en_TMZ_PLAYER_ERROR e;
        st_CMT_STREAM *s = tmz_player_play_next ( &st, &e );
        if ( s ) {
            lv[k] = first_level ( s );
            ev[k] = count_events ( s );
            k++;
            cmt_stream_destroy ( s );
        } else if ( e != TMZ_PLAYER_OK ) {
            lv[k] = -3;
            ev[k] = 0;
            k++;
            break;
        }
    }
    return k;
}


int main ( void ) {
    int lv[16];
    uint32_t ev[16];
    int k;

    /* start: signál nízko, první pulz vysoko (hrana jako na ZX); 0x12 (5 pulzů) končí nízko, 0x13 začne nízko */
    st_TZX_BLOCK a[] = { blk ( 0x12, g_tone5, 4 ), blk ( 0x13, g_seq2, 5 ) };
    k = play_all ( a, 2, lv, ev );
    CHECK ( k == 2 && lv[0] == 1 && lv[1] == 0, "level carries over: 0x12 starts high (an edge), 0x13 starts low" );

    /* 0x2B nastaví úroveň dalšího pulzu */
    st_TZX_BLOCK b0[] = { blk ( 0x2B, g_level0, 5 ), blk ( 0x12, g_tone1, 4 ) };
    k = play_all ( b0, 2, lv, ev );
    CHECK ( k == 1 && lv[0] == 0, "0x2B level 0 -> next pulse low" );
    st_TZX_BLOCK b1[] = { blk ( 0x2B, g_level1, 5 ), blk ( 0x12, g_tone1, 4 ) };
    k = play_all ( b1, 2, lv, ev );
    CHECK ( k == 1 && lv[0] == 1, "0x2B level 1 -> next pulse high" );
    st_TZX_BLOCK b2[] = { blk ( 0x2B, g_level2, 5 ), blk ( 0x12, g_tone5, 4 ), blk ( 0x12, g_tone1, 4 ) };
    k = play_all ( b2, 3, lv, ev );
    CHECK ( k == 2 && lv[0] == 1 && lv[1] == 0, "0x2B value 2 = high, alternation stays intact" );
    /* krátký 0x2B (úroveň 0) se ignoruje: platí start, první pulz vysoko */
    st_TZX_BLOCK bs[] = { blk ( 0x2B, g_level0, 4 ), blk ( 0x12, g_tone1, 4 ) };
    k = play_all ( bs, 2, lv, ev );
    CHECK ( k == 1 && lv[0] == 1, "short 0x2B ignored" );

    /* pauza: po 2 pulzech od startu (poslední nízko) 1 ms vysoko + zbytek nízko; po 1 pulzu celá nízko */
    st_TZX_BLOCK p1[] = { blk ( 0x13, g_seq2, 5 ), blk ( 0x20, g_pause10, 2 ) };
    k = play_all ( p1, 2, lv, ev );
    CHECK ( k == 2 && lv[1] == 1 && ev[1] == 2, "pause after a low pulse: 1 ms high, then low" );
    st_TZX_BLOCK p2[] = { blk ( 0x12, g_tone1, 4 ), blk ( 0x20, g_pause10, 2 ) };
    k = play_all ( p2, 2, lv, ev );
    CHECK ( k == 2 && lv[1] == 0 && ev[1] == 1, "pause after a high pulse: whole pause low" );
    /* po pauze první pulz vysoko (hrana), pauza po pauze bez 1 ms */
    st_TZX_BLOCK p3[] = { blk ( 0x13, g_seq2, 5 ), blk ( 0x20, g_pause10, 2 ), blk ( 0x20, g_pause10, 2 ),
                          blk ( 0x12, g_tone1, 4 ) };
    k = play_all ( p3, 4, lv, ev );
    CHECK ( k == 4 && ev[2] == 1 && lv[2] == 0 && lv[3] == 1, "pause after a pause: whole low; next pulse high (an edge)" );

    /* Review Focus 1: smyčka, 0x12 s 1 pulzem 2x -> úrovně 1, 0 */
    st_TZX_BLOCK lp[] = { blk ( 0x24, g_loop2, 2 ), blk ( 0x12, g_tone1, 4 ), blk ( 0x25, NULL, 0 ) };
    k = play_all ( lp, 3, lv, ev );
    CHECK ( k == 2 && lv[0] == 1 && lv[1] == 0, "level carries over loop iterations" );

    /* MZ blok (IC-10.2-NORMAL, pauza HIGH) a po něm 0x12: začne opakem poslední úrovně MZ streamu */
    {
        static uint8_t body[4] = { 1, 2, 3, 4 };
        st_MZF_HEADER h;
        memset ( &h, 0, sizeof ( h ) );
        h.ftype = 1;
        h.fsize = 4;
        h.fstrt = 0x1200;
        h.fexec = 0x1200;
        st_TMZ_PROFILE_ARGS pa;
        tmz_profile_args_init ( &pa );
        st_TZX_BLOCK *ic = tmz_profile_build_block ( "IC-10.2-NORMAL", &pa, &h, body, 4, NULL );
        st_TMZ_PLAYER_CONFIG cfg;
        tmz_player_config_init ( &cfg );
        en_TMZ_PLAYER_ERROR e;
        st_CMT_STREAM *s = ic ? tmz_player_play_block ( ic, &cfg, &e ) : NULL;
        int mz_last = ( s && s->stream_type == CMT_STREAM_TYPE_VSTREAM ) ? s->str.vstream->last_set_value : -1;
        cmt_stream_destroy ( s );
        printf ( "  IC-10.2-NORMAL stream ends at level %d\n", mz_last );
        st_TZX_BLOCK m[2];
        k = 0;
        if ( ic ) {
            m[0] = *ic;
            m[1] = blk ( 0x12, g_tone1, 4 );
            k = play_all ( m, 2, lv, ev );
        }
        CHECK ( k == 2 && mz_last >= 0 && lv[1] == !mz_last, "TZX block after an MZ block starts with an edge" );
        tmz_block_free ( ic );
    }

    /* bezstavové API: signál nízko, první pulz vysoko (jako po pauze) */
    {
        st_TMZ_PLAYER_CONFIG cfg;
        tmz_player_config_init ( &cfg );
        en_TMZ_PLAYER_ERROR e;
        st_TZX_BLOCK t = blk ( 0x12, g_tone5, 4 );
        st_CMT_STREAM *s = tmz_player_play_tzx_block ( &t, &cfg, &e );
        CHECK ( first_level ( s ) == 1, "tmz_player_play_tzx_block: first pulse high (an edge)" );
        cmt_stream_destroy ( s );
    }

    /* rezervovaný blok 0x47 se přeskočí s varováním */
    {
        static uint8_t r[4] = { 0, 0, 0, 0 };
        st_TMZ_PLAYER_CONFIG cfg;
        tmz_player_config_init ( &cfg );
        en_TMZ_PLAYER_ERROR e = TMZ_PLAYER_ERROR_ALLOC;
        st_TZX_BLOCK rb = blk ( 0x47, r, 4 );
        st_CMT_STREAM *s = tmz_player_play_block ( &rb, &cfg, &e );
        CHECK ( !s && e == TMZ_PLAYER_OK, "reserved block 0x47 skipped (play_block)" );
        st_TZX_BLOCK rr[] = { blk ( 0x4F, r, 4 ), blk ( 0x12, g_tone1, 4 ) };
        k = play_all ( rr, 2, lv, ev );
        CHECK ( k == 1 && lv[0] == 1, "reserved block 0x4F skipped (play_next)" );
    }

    /* Pause 0 a 0x2A = zastavení pásky v play_next */
    {
        static uint8_t p0[2] = { 0, 0 };
        static uint8_t s48[4] = { 0, 0, 0, 0 };
        st_TZX_BLOCK sb[] = { blk ( 0x12, g_tone1, 4 ), blk ( 0x20, p0, 2 ), blk ( 0x12, g_tone1, 4 ),
                              blk ( 0x2A, s48, 4 ), blk ( 0x12, g_tone1, 4 ) };
        st_TZX_FILE f;
        memset ( &f, 0, sizeof ( f ) );
        f.blocks = sb;
        f.block_count = 5;
        st_TMZ_PLAYER_CONFIG cfg;
        tmz_player_config_init ( &cfg );
        st_TMZ_PLAYER_STATE st;
        tmz_player_state_init ( &st, &f, &cfg );
        en_TMZ_PLAYER_ERROR e;
        int seq[8];
        int n = 0;
        for ( int guard = 0; guard < 8 && !tmz_player_state_finished ( &st ); guard++ ) {
            st_CMT_STREAM *s = tmz_player_play_next ( &st, &e );
            if ( s ) seq[n++] = 1;
            else if ( tmz_player_state_stopped ( &st ) ) seq[n++] = 2 + ( int ) st.last_played_block * 10;
            cmt_stream_destroy ( s );
        }
        CHECK ( n == 5 && seq[0] == 1 && seq[1] == 12 && seq[2] == 1 && seq[3] == 32 && seq[4] == 1,
                "Pause 0 and 0x2A stop the tape, playback continues after them" );
        CHECK ( !tmz_player_state_stopped ( &st ), "stopped cleared by the next call" );
    }

    /* Review Focus 2: zastavení ve smyčce */
    {
        static uint8_t p0[2] = { 0, 0 };
        st_TZX_BLOCK lb[] = { blk ( 0x24, g_loop2, 2 ), blk ( 0x12, g_tone1, 4 ), blk ( 0x20, p0, 2 ),
                              blk ( 0x25, NULL, 0 ) };
        st_TZX_FILE f;
        memset ( &f, 0, sizeof ( f ) );
        f.blocks = lb;
        f.block_count = 4;
        st_TMZ_PLAYER_CONFIG cfg;
        tmz_player_config_init ( &cfg );
        st_TMZ_PLAYER_STATE st;
        tmz_player_state_init ( &st, &f, &cfg );
        en_TMZ_PLAYER_ERROR e;
        int streams = 0, stops = 0;
        for ( int guard = 0; guard < 16 && !tmz_player_state_finished ( &st ); guard++ ) {
            st_CMT_STREAM *s = tmz_player_play_next ( &st, &e );
            if ( s ) streams++;
            else if ( tmz_player_state_stopped ( &st ) ) stops++;
            cmt_stream_destroy ( s );
        }
        CHECK ( streams == 2 && stops == 2, "stop inside a loop: every iteration stops, the loop completes" );
    }

    /* Review Focus 3: bezstavové API - Pause 0 dál NULL a OK */
    {
        static uint8_t p0[2] = { 0, 0 };
        st_TMZ_PLAYER_CONFIG cfg;
        tmz_player_config_init ( &cfg );
        en_TMZ_PLAYER_ERROR e = TMZ_PLAYER_ERROR_ALLOC;
        st_TZX_BLOCK pb = blk ( 0x20, p0, 2 );
        st_CMT_STREAM *s = tmz_player_play_block ( &pb, &cfg, &e );
        CHECK ( !s && e == TMZ_PLAYER_OK, "play_block with Pause 0: NULL and OK" );
    }

    /* blok MZ končící tichem (pause_ms > 0) -> nízká úroveň, další pauza bez 1 ms vysoko */
    {
        static uint8_t body[16];
        st_MZF_HEADER h;
        memset ( &h, 0, sizeof ( h ) );
        h.ftype = 1;
        h.fsize = 16;
        h.fstrt = 0x1200;
        h.fexec = 0x1200;
        st_TZX_BLOCK *bp = tmz_block_create_mz_standard ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 500, &h, body, 16 );
        st_TZX_BLOCK *b0 = tmz_block_create_mz_standard ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 0, &h, body, 16 );
        k = 0;
        if ( bp ) {
            st_TZX_BLOCK m[2] = { *bp, blk ( 0x20, g_pause10, 2 ) };
            k = play_all ( m, 2, lv, ev );
        }
        CHECK ( k == 2 && lv[1] == 0 && ev[1] == 1, "MZ block ending with a pause -> low level, next pause has no 1 ms high" );
        k = 0;
        if ( bp ) {
            st_TZX_BLOCK m[2] = { *bp, blk ( 0x12, g_tone1, 4 ) };
            k = play_all ( m, 2, lv, ev );
        }
        CHECK ( k == 2 && lv[1] == 1, "MZ block ending with a low pause -> next TZX pulse high (an edge)" );
        /* bez pauzy: opak poslední úrovně MZ streamu (další blok začne hranou) */
        int mz_last = -1;
        if ( b0 ) {
            st_TMZ_PLAYER_CONFIG cfg;
            tmz_player_config_init ( &cfg );
            en_TMZ_PLAYER_ERROR e;
            st_CMT_STREAM *s = tmz_player_play_block ( b0, &cfg, &e );
            mz_last = ( s && s->stream_type == CMT_STREAM_TYPE_VSTREAM ) ? s->str.vstream->last_set_value : -1;
            cmt_stream_destroy ( s );
            st_TZX_BLOCK m[2] = { *b0, blk ( 0x12, g_tone1, 4 ) };
            k = play_all ( m, 2, lv, ev );
        }
        printf ( "  0x40 without pause ends at level %d\n", mz_last );
        CHECK ( k == 2 && mz_last >= 0 && lv[1] == !mz_last, "MZ block without a pause -> next TZX block starts with an edge" );
        tmz_block_free ( bp );
        tmz_block_free ( b0 );
    }

    /* pause_ms bloku 0x40 v cestě v1.0 je ticho ve streamu */
    {
        static uint8_t body[16];
        st_MZF_HEADER h;
        memset ( &h, 0, sizeof ( h ) );
        h.ftype = 1;
        h.fsize = 16;
        h.fstrt = 0x1200;
        h.fexec = 0x1200;
        uint64_t sc[2][2] = { { 0, 0 }, { 0, 0 } };
        for ( int conv = 0; conv < 2; conv++ ) {
            for ( int pi = 0; pi < 2; pi++ ) {
                st_TZX_BLOCK *b = tmz_block_create_mz_standard ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800,
                                                                 pi ? 500 : 0, &h, body, 16 );
                st_TMZ_PLAYER_CONFIG cfg;
                tmz_player_config_init ( &cfg );
                cfg.v1_convert = conv;
                en_TMZ_PLAYER_ERROR e;
                st_CMT_STREAM *s = b ? tmz_player_play_block ( b, &cfg, &e ) : NULL;
                sc[conv][pi] = s ? cmt_stream_get_count_scans ( s ) : 0;
                cmt_stream_destroy ( s );
                tmz_block_free ( b );
            }
        }
        long long d0 = ( long long ) sc[0][1] - ( long long ) sc[0][0];
        long long d1 = ( long long ) sc[1][1] - ( long long ) sc[1][0];
        printf ( "  pause 500 ms adds %lld samples (v1.0 path), %lld samples (converted)\n", d0, d1 );
        CHECK ( d0 >= 22049 && d0 <= 22051, "0x40 v1.0 path: pause_ms adds silence" );
        CHECK ( d1 >= 22000 && d1 <= 22100, "0x40 converted path: pause_ms adds silence (unchanged)" );
    }

    printf ( "\n=== TMZ Player Level Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
