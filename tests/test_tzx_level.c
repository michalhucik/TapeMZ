/**
 * @file   test_tzx_level.c
 * @brief  Generátory TZX: pilot 0x10/0x11 má
 *         pilot_count pulzů, tzx_generate_block() drží úroveň signálu podle
 *         TZX 1.20 (pulz na aktuální úrovni, pak otočení), po pauze ale
 *         TZX_LEVEL_AFTER_PAUSE (signál nízko, další pulz s hranou, jako ZX).
 *
 * Takt i vzorkovací frekvence jsou 3 500 000 Hz, 1 T = 1 vzorek. Data
 * nepotřebuje.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/tzx/tzx.h"
#include "libs/cmt_stream/cmt_vstream.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše OK/FAIL. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )


/**
 * @brief Spočítá eventy (pulzy) vstreamu.
 * @param vs Vstream. @param[out] first Hodnota prvního eventu (-1 = prázdný).
 * @param[out] first_len Délka prvního eventu ve vzorcích.
 * @return Počet eventů.
 */
static uint32_t events ( st_CMT_VSTREAM *vs, int *first, uint64_t *first_len ) {
    uint64_t n;
    int v;
    uint32_t c = 0;
    *first = -1;
    *first_len = 0;
    cmt_vstream_read_reset ( vs );
    while ( cmt_vstream_read_pulse ( vs, &n, &v ) == EXIT_SUCCESS ) {
        if ( c == 0 ) {
            *first = v;
            *first_len = n;
        }
        c++;
    }
    return c;
}


/**
 * @brief Vygeneruje blok TZX z dat bloku při 1 T = 1 vzorek.
 * @param id ID bloku. @param raw Data bloku. @param len Délka dat.
 * @param[in,out] level Úroveň signálu.
 * @return Vstream (uvolnit cmt_vstream_destroy), nebo NULL.
 */
static st_CMT_VSTREAM* gen ( uint8_t id, const uint8_t *raw, uint32_t len, uint8_t *level ) {
    st_TZX_CONFIG cfg;
    tzx_config_init ( &cfg );
    cfg.sample_rate = 3500000;
    cfg.cpu_clock = 3500000;
    st_TZX_BLOCK b = { id, len, ( uint8_t* ) raw };
    en_TZX_ERROR err;
    return tzx_generate_block ( &b, &cfg, level, &err );
}


int main ( void ) {
    int f;
    uint64_t fl;
    uint8_t lv;

    /* 0x10: pauza 0, 2 B dat, flag 0x00 = hlavička -> 8063 pilot + 2 sync + 32 datových pulzů */
    static const uint8_t std[] = { 0, 0, 2, 0, 0x00, 0xFF };
    lv = 0;
    st_CMT_VSTREAM *vs = gen ( 0x10, std, sizeof ( std ), &lv );
    uint32_t n = vs ? events ( vs, &f, &fl ) : 0;
    printf ( "  0x10: %u pulses, first level %d, level after %u\n", n, f, lv );
    CHECK ( n == 8063 + 2 + 32, "0x10 header: 8063 pilot pulses (not 2x), 2 sync, 16 bits x 2" );
    CHECK ( f == 0 && fl == 2168 && lv == 1, "0x10 starts at the input level, level after = opposite of the last pulse" );
    cmt_vstream_destroy ( vs );

    /* 0x11: pilot 10 pulzů, 1 bit dat (used bits 1), pauza 0 */
    static const uint8_t turbo[] = { 100, 0, 50, 0, 60, 0, 70, 0, 80, 0, 10, 0, 1, 0, 0, 1, 0, 0, 0x80 };
    lv = 1;
    vs = gen ( 0x11, turbo, sizeof ( turbo ), &lv );
    n = vs ? events ( vs, &f, &fl ) : 0;
    CHECK ( n == 10 + 2 + 2 && f == 1 && lv == 1, "0x11: pilot_count pulses, starts high when level in is high" );
    cmt_vstream_destroy ( vs );

    /* 0x12: 5 pulzů po 100 T */
    static const uint8_t tone[] = { 100, 0, 5, 0 };
    lv = 0;
    vs = gen ( 0x12, tone, sizeof ( tone ), &lv );
    n = vs ? events ( vs, &f, &fl ) : 0;
    CHECK ( n == 5 && f == 0 && lv == 1, "0x12: 5 pulses from low, level after high" );
    cmt_vstream_destroy ( vs );

    /* 0x13: 2 pulzy */
    static const uint8_t seq[] = { 2, 100, 0, 200, 0 };
    lv = 1;
    vs = gen ( 0x13, seq, sizeof ( seq ), &lv );
    n = vs ? events ( vs, &f, &fl ) : 0;
    CHECK ( n == 2 && f == 1 && fl == 100 && lv == 1, "0x13: 2 pulses from high, level after high" );
    cmt_vstream_destroy ( vs );

    /* 0x15: 79 T/vzorek, pauza 0, 3 bity 1,1,0 -> úroveň po bloku = opak posledního bitu (1) */
    static const uint8_t direct[] = { 79, 0, 0, 0, 3, 1, 0, 0, 0xC0 };
    lv = 0;
    vs = gen ( 0x15, direct, sizeof ( direct ), &lv );
    n = vs ? events ( vs, &f, &fl ) : 0;
    CHECK ( n == 2 && f == 1 && lv == 1, "0x15: levels from bits, level after = opposite of the last bit" );
    cmt_vstream_destroy ( vs );

    /* 0x20: pauza 10 ms při vysoké úrovni = 1 ms vysoko + 9 ms nízko */
    static const uint8_t pause10[] = { 10, 0 };
    lv = 1;
    vs = gen ( 0x20, pause10, sizeof ( pause10 ), &lv );
    n = vs ? events ( vs, &f, &fl ) : 0;
    CHECK ( n == 2 && f == 1 && fl == 3500 && lv == TZX_LEVEL_AFTER_PAUSE,
            "0x20 at high level: 1 ms high, rest low, level after = TZX_LEVEL_AFTER_PAUSE" );
    cmt_vstream_destroy ( vs );

    /* 0x20 při nízké úrovni = celá pauza nízko (žádná hrana navíc) */
    lv = 0;
    vs = gen ( 0x20, pause10, sizeof ( pause10 ), &lv );
    n = vs ? events ( vs, &f, &fl ) : 0;
    CHECK ( n == 1 && f == 0 && fl == 35000 && lv == TZX_LEVEL_AFTER_PAUSE, "0x20 at low level: whole pause low" );
    cmt_vstream_destroy ( vs );

    /* pauza po pauze: celá nízko, bez 1 ms vysoko (žádná hrana navíc) */
    lv = TZX_LEVEL_AFTER_PAUSE;
    vs = gen ( 0x20, pause10, sizeof ( pause10 ), &lv );
    n = vs ? events ( vs, &f, &fl ) : 0;
    CHECK ( n == 1 && f == 0 && fl == 35000 && lv == TZX_LEVEL_AFTER_PAUSE, "0x20 after a pause: whole pause low, no 1 ms high" );
    cmt_vstream_destroy ( vs );

    /*
     * Jako ZX: první pulz po pauze vytvoří hranu (signál je nízko, pulz je vysoko),
     * takže je vidět celý pilot. TZX 1.20 by první pulz zapsal nízko (splynul by s pauzou).
     */
    lv = TZX_LEVEL_AFTER_PAUSE;
    vs = gen ( 0x12, tone, sizeof ( tone ), &lv );
    n = vs ? events ( vs, &f, &fl ) : 0;
    CHECK ( n == 5 && f == 1 && fl == 100 && lv == 0, "0x12 after a pause: first pulse high (an edge), all 5 pulses" );
    cmt_vstream_destroy ( vs );
    /* bloky bez pulzů úroveň TZX_LEVEL_AFTER_PAUSE nemění */
    static const uint8_t tone0[] = { 100, 0, 0, 0 };
    static const uint8_t seq0[] = { 0 };
    lv = TZX_LEVEL_AFTER_PAUSE;
    vs = gen ( 0x12, tone0, sizeof ( tone0 ), &lv );
    CHECK ( lv == TZX_LEVEL_AFTER_PAUSE, "0x12 with 0 pulses keeps TZX_LEVEL_AFTER_PAUSE" );
    cmt_vstream_destroy ( vs );
    lv = TZX_LEVEL_AFTER_PAUSE;
    vs = gen ( 0x13, seq0, sizeof ( seq0 ), &lv );
    CHECK ( lv == TZX_LEVEL_AFTER_PAUSE, "0x13 with 0 pulses keeps TZX_LEVEL_AFTER_PAUSE" );
    cmt_vstream_destroy ( vs );
    /* 0x14 bez dat: s pauzou po pauze celá nízko, bez pauzy úroveň beze změny */
    static const uint8_t pdata0p[] = { 100, 0, 200, 0, 8, 10, 0, 0, 0, 0 };
    static const uint8_t pdata00[] = { 100, 0, 200, 0, 8, 0, 0, 0, 0, 0 };
    lv = TZX_LEVEL_AFTER_PAUSE;
    vs = gen ( 0x14, pdata0p, sizeof ( pdata0p ), &lv );
    n = vs ? events ( vs, &f, &fl ) : 0;
    CHECK ( n == 1 && f == 0 && fl == 35000 && lv == TZX_LEVEL_AFTER_PAUSE, "0x14 without data after a pause: whole pause low" );
    cmt_vstream_destroy ( vs );
    lv = 1;
    vs = gen ( 0x14, pdata0p, sizeof ( pdata0p ), &lv );
    n = vs ? events ( vs, &f, &fl ) : 0;
    CHECK ( n == 2 && f == 1 && fl == 3500, "0x14 without data at high level: 1 ms high, then low" );
    cmt_vstream_destroy ( vs );
    lv = TZX_LEVEL_AFTER_PAUSE;
    vs = gen ( 0x14, pdata00, sizeof ( pdata00 ), &lv );
    CHECK ( lv == TZX_LEVEL_AFTER_PAUSE, "0x14 without data and pause keeps the level" );
    cmt_vstream_destroy ( vs );
    /* 0x15 bez dat s pauzou po pauze: celá nízko */
    static const uint8_t direct0p[] = { 79, 0, 10, 0, 8, 0, 0, 0 };
    lv = TZX_LEVEL_AFTER_PAUSE;
    vs = gen ( 0x15, direct0p, sizeof ( direct0p ), &lv );
    n = vs ? events ( vs, &f, &fl ) : 0;
    CHECK ( n == 1 && f == 0 && fl == 35000 && lv == TZX_LEVEL_AFTER_PAUSE, "0x15 without data after a pause: whole pause low" );
    cmt_vstream_destroy ( vs );

    /* blok s pauzou a za ním pilot: pilot 10 pulzů po pauze bloku, první vysoko */
    static const uint8_t turbo_p[] = { 100, 0, 50, 0, 60, 0, 70, 0, 80, 0, 10, 0, 1, 1, 0, 1, 0, 0, 0x80 };
    lv = 0;
    vs = gen ( 0x11, turbo_p, sizeof ( turbo_p ), &lv );
    CHECK ( vs && lv == TZX_LEVEL_AFTER_PAUSE, "0x11 with a pause: level after = TZX_LEVEL_AFTER_PAUSE" );
    cmt_vstream_destroy ( vs );
    vs = gen ( 0x11, turbo, sizeof ( turbo ), &lv );
    n = vs ? events ( vs, &f, &fl ) : 0;
    CHECK ( n == 10 + 2 + 2 && f == 1 && fl == 100, "0x11 after a pause: all 10 pilot pulses, the first high" );
    cmt_vstream_destroy ( vs );

    /* Pause 0: NULL, OK, úroveň beze změny */
    static const uint8_t pause0[] = { 0, 0 };
    lv = 1;
    vs = gen ( 0x20, pause0, sizeof ( pause0 ), &lv );
    CHECK ( !vs && lv == 1, "Pause 0: no stream, level unchanged" );

    /* nepodporovaný blok: NULL, úroveň beze změny */
    lv = 1;
    vs = gen ( 0x2B, pause0, sizeof ( pause0 ), &lv );
    CHECK ( !vs && lv == 1, "unsupported block: no stream, level unchanged" );

    /* stará API: začíná nízko */
    st_TZX_CONFIG cfg;
    tzx_config_init ( &cfg );
    cfg.sample_rate = 3500000;
    cfg.cpu_clock = 3500000;
    st_TZX_PURE_TONE pt;
    memset ( &pt, 0, sizeof ( pt ) );
    pt.pulse_length = 100;
    pt.pulse_count = 3;
    vs = tzx_generate_pure_tone ( &pt, &cfg, NULL );
    n = vs ? events ( vs, &f, &fl ) : 0;
    CHECK ( n == 3 && f == 0, "tzx_generate_pure_tone starts low" );
    cmt_vstream_destroy ( vs );

    printf ( "\n=== TZX Level Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
