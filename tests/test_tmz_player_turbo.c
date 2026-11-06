/**
 * @file   test_tmz_player_turbo.c
 * @brief  Regresní test: blok 0x41 TURBO s vlastními pulzy přes tmz_player.
 *
 * - vlastní pulzy odpovídající TurboCopy 2:1 a speed = 0 (tak je zapisuje
 *   tmzedit set --pulse): player musí vrátit stream; čtecí bod N se počítá
 *   z pulzů (dříve se dosadilo N = 82 a signál byl nečitelný),
 * - vlastní pulzy s rezervou čtecího bodu pod minimem a pulzy, pro které
 *   žádné N neexistuje: player musí vrátit chybu místo nečitelného signálu.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_player.h"
#include "libs/cmt_stream/cmt_stream.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;


/**
 * @brief Vytvoří blok 0x41 TURBO se speed 0 a danými vlastními pulzy a přehraje ho.
 * @param sh,sl,lh,ll Půlvlny SHORT/LONG v 0,1 us.
 * @return 1 pokud player vrátil stream, 0 pokud vrátil chybu.
 */
static int play_custom ( uint16_t sh, uint16_t sl, uint16_t lh, uint16_t ll ) {
    st_TMZ_MZ_TURBO_DATA tp;
    memset ( &tp, 0, sizeof ( tp ) );
    tp.machine = TMZ_MACHINE_MZ800;
    tp.pulseset = TMZ_PULSESET_800;
    tp.format = TMZ_FORMAT_TURBO;
    tp.speed = 0;
    tp.short_high = sh;
    tp.short_low = sl;
    tp.long_high = lh;
    tp.long_low = ll;
    tp.mzf_header.ftype = 0x01;
    memcpy ( tp.mzf_header.fname.name, "PLAYER", 6 );
    tp.mzf_header.fname.name[6] = 0x0D;
    tp.mzf_header.fsize = 64;
    tp.mzf_header.fstrt = 0x1200;
    tp.mzf_header.fexec = 0x1200;
    uint8_t body[64];
    for ( int i = 0; i < 64; i++ ) body[i] = ( uint8_t ) i;

    st_TZX_BLOCK *b = tmz_block_create_mz_turbo ( &tp, body, 64 );
    if ( !b ) return -1;
    st_TMZ_PLAYER_CONFIG cfg;
    tmz_player_config_init ( &cfg );
    en_TMZ_PLAYER_ERROR perr = TMZ_PLAYER_OK;
    st_CMT_STREAM *s = tmz_player_play_block ( b, &cfg, &perr );
    tmz_block_free ( b );
    if ( s ) {
        cmt_stream_destroy ( s );
        return 1;
    }
    return 0;
}


/**
 * @brief Přehraje blok 0x41 daného formátu a úrovně při dané vzorkovací frekvenci.
 * @param format en_TMZ_FORMAT (FSK/SLOW/DIRECT/TURBO). @param level Úroveň (pole speed).
 * @param rate Vzorkovací frekvence. @param[out] err Chyba playeru.
 * @return 1 = stream, 0 = NULL.
 */
static int play_format_rate ( uint8_t format, uint8_t level, uint32_t rate, en_TMZ_PLAYER_ERROR *err ) {
    st_TMZ_MZ_TURBO_DATA tp;
    memset ( &tp, 0, sizeof ( tp ) );
    tp.machine = TMZ_MACHINE_MZ800;
    tp.pulseset = TMZ_PULSESET_800;
    tp.format = format;
    tp.speed = level;
    tp.mzf_header.ftype = 0x01;
    memcpy ( tp.mzf_header.fname.name, "RATE", 4 );
    tp.mzf_header.fname.name[4] = 0x0D;
    tp.mzf_header.fsize = 64;
    tp.mzf_header.fstrt = 0x1200;
    tp.mzf_header.fexec = 0x1200;
    uint8_t body[64];
    for ( int i = 0; i < 64; i++ ) body[i] = ( uint8_t ) i;
    st_TZX_BLOCK *b = tmz_block_create_mz_turbo ( &tp, body, 64 );
    if ( !b ) return 0;
    st_TMZ_PLAYER_CONFIG cfg;
    tmz_player_config_init ( &cfg );
    cfg.sample_rate = rate;
    st_CMT_STREAM *s = tmz_player_play_block ( b, &cfg, err );
    int ok = ( s != NULL );
    cmt_stream_destroy ( s );
    tmz_block_free ( b );
    return ok;
}


/**
 * @brief Ověří výsledek přehrání kombinace z měření v emulátoru.
 * @param name Popis. @param format Formát. @param level Úroveň. @param rate Frekvence.
 * @param want_stream 1 = čeká se stream, 0 = UNREADABLE.
 */
static void check_rate ( const char *name, uint8_t format, uint8_t level, uint32_t rate, int want_stream ) {
    en_TMZ_PLAYER_ERROR e = TMZ_PLAYER_OK;
    int r = play_format_rate ( format, level, rate, &e );
    int ok = want_stream ? ( r == 1 ) : ( r == 0 && e == TMZ_PLAYER_ERROR_UNREADABLE );
    printf ( "%s: %s at %u Hz -> %s\n", ok ? "OK" : "FAIL", name, rate, want_stream ? "stream" : "UNREADABLE" );
    if ( !ok ) g_failures++;
}


int main ( void ) {
    /* kombinace podle měření v emulátoru */
    check_rate ( "FSK 6 (measured TIMEOUT)", TMZ_FORMAT_FSK, 6, 48000, 0 );
    check_rate ( "SLOW 1 (measured TIMEOUT)", TMZ_FORMAT_SLOW, 1, 88200, 0 );
    check_rate ( "DIRECT (measured FAIL)", TMZ_FORMAT_DIRECT, 1, 48000, 0 );
    check_rate ( "FSK 0 (measured PASS)", TMZ_FORMAT_FSK, 0, 48000, 1 );
    check_rate ( "SLOW 0 (measured PASS)", TMZ_FORMAT_SLOW, 0, 48000, 1 );
    check_rate ( "FSK 6 (measured PASS)", TMZ_FORMAT_FSK, 6, 44100, 1 );
    check_rate ( "FSK 6 (not measured, warning only)", TMZ_FORMAT_FSK, 6, 40000, 1 );
    check_rate ( "TURBO (not affected)", TMZ_FORMAT_TURBO, 0, 48000, 1 );

    int r = play_custom ( 1403, 1403, 2806, 2806 );
    printf ( "%s: custom 2:1-like pulses, speed 0 -> stream\n", r == 1 ? "OK" : "FAIL" );
    if ( r != 1 ) g_failures++;

    /* SHORT 100 us, LONG 120 us: N existuje (17), ale rezervy 0,6/7,4 us < 10 us -> chyba */
    r = play_custom ( 1000, 1000, 1200, 1200 );
    printf ( "%s: custom pulses below readpoint margin -> error\n", r == 0 ? "OK" : "FAIL" );
    if ( r != 0 ) g_failures++;

    /* všechny půlvlny 0,1 us (1 vzorek): žádné N nesplní podmínky -> chyba "no ROM read point" */
    r = play_custom ( 1, 1, 1, 1 );
    printf ( "%s: custom pulses without any read point -> error\n", r == 0 ? "OK" : "FAIL" );
    if ( r != 0 ) g_failures++;

    printf ( "\n=== TMZ player TURBO Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
