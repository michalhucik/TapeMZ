/**
 * @file   test_mzcmt_fm.c
 * @brief  Testy generátoru FM (spec TMZ v1.1 kap. 6): posloupnost pulzů,
 *         kontexty a rodiče, LSB, start bity, checksum, kopie, LEVEL a pauza.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzcmt_fm/mzcmt_fm.h"
#include "libs/cmt_stream/cmt_vstream.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )

/** @brief Nasbírané půlvlny. */
typedef struct st_HALVES {
    uint32_t n;            /**< počet */
    int level[4096];       /**< úrovně */
    uint32_t dur[4096];    /**< délky */
} st_HALVES;

/**
 * @brief Callback generátoru: uloží půlvlnu.
 * @param user st_HALVES. @param level Úroveň. @param duration Délka.
 * @return EXIT_SUCCESS, nebo EXIT_FAILURE při plném poli.
 */
static int collect ( void *user, int level, uint32_t duration ) {
    st_HALVES *h = user;
    if ( h->n >= 4096 ) return EXIT_FAILURE;
    h->level[h->n] = level;
    h->dur[h->n] = duration;
    h->n++;
    return EXIT_SUCCESS;
}

/**
 * @brief Základní parametry: S = 10/11, L = 20/21, žádná struktura.
 * @param p Parametry k naplnění.
 */
static void base ( st_MZCMT_FM_PARAMS *p ) {
    mzcmt_fm_params_init ( p );
    p->short_first = 10;
    p->short_second = 11;
    p->long_first = 20;
    p->long_second = 21;
}


/** @brief Akumulace času: součet vzorků = round(součet jednotek * rate / timebase). */
static void test_vstream ( void ) {
    st_MZCMT_FM_PARAMS p;
    base ( &p );
    p.timebase = 3546895;
    p.short_first = 844;
    p.short_second = 920;
    p.long_first = 1664;
    p.long_second = 1750;
    p.tone = 1000;
    uint8_t z = 0x55;
    /* součet jednotek: 1000 * 1764 + start L 3414 + 4 x L 3414 + 4 x S 1764 = 1788126 */
    st_CMT_VSTREAM *vs = mzcmt_fm_create_vstream ( &p, &z, 1, 44100 );
    CHECK ( vs != NULL, "create_vstream" );
    if ( vs ) {
        uint64_t expect = ( 1788126ull * 44100 + 3546895 / 2 ) / 3546895;
        CHECK ( cmt_vstream_get_count_scans ( vs ) == expect, "total samples = rounded total time (no drift)" );
        cmt_vstream_destroy ( vs );
    }
    /* 192 kHz a ~8 min signálu (tón 1 000 000 pulzů = 1 764 024 126 T při 3 546 895 Hz): bez přetečení */
    p.tone = 1000000;
    vs = mzcmt_fm_create_vstream ( &p, &z, 1, 192000 );
    CHECK ( vs != NULL, "long part at 192 kHz" );
    if ( vs ) {
        uint64_t units = 1000000ull * 1764 + 3414ull * 5 + 7056;
        uint64_t expect = ( units * 192000 + 3546895 / 2 ) / 3546895;
        CHECK ( cmt_vstream_get_count_scans ( vs ) == expect, "192 kHz total samples exact" );
        cmt_vstream_destroy ( vs );
    }
    /* půlvlna kratší než vzorek -> chyba */
    base ( &p );
    p.timebase = 1000000;
    p.short_first = 5;
    p.short_second = 5;
    p.long_first = 10;
    p.long_second = 10;
    p.tone = 10;
    vs = mzcmt_fm_create_vstream ( &p, &z, 1, 22050 );
    CHECK ( vs == NULL, "half shorter than a sample rejected" );
}


/**
 * @brief Úroveň pauzy (Intercopy): pauza HIGH je poslední půlvlna s úrovní 1
 *        a ve vstreamu splyne s první půlvlnou HIGH další části.
 */
static void test_pause_level ( void ) {
    static st_MZCMT_FM_PARAMS p;
    static st_HALVES h;
    base ( &p );
    p.tone = 2;
    p.pause = 500;
    p.pause_level = 1;
    h.n = 0;
    CHECK ( mzcmt_fm_generate ( &p, NULL, 0, collect, &h ) == EXIT_SUCCESS && h.n > 1 && h.level[h.n - 1] == 1
            && h.dur[h.n - 1] == 500 && h.level[h.n - 2] == 0, "pause HIGH after the last LOW half" );
    p.pause_level = 0;
    h.n = 0;
    CHECK ( mzcmt_fm_generate ( &p, NULL, 0, collect, &h ) == EXIT_SUCCESS && h.level[h.n - 1] == 0, "pause LOW by default" );
    p.pause_level = 2;
    CHECK ( mzcmt_fm_params_validate ( &p ) == EXIT_FAILURE, "pause_level 2 rejected" );

    /* pauza kratší než vzorek se vynechá, nic dalšího se nezmění */
    {
        static st_MZCMT_FM_PARAMS q;
        base ( &q );
        q.timebase = 1000000;
        q.short_first = q.short_second = 240;
        q.long_first = q.long_second = 480;
        q.tone = 10;
        st_CMT_VSTREAM *a = mzcmt_fm_create_vstream ( &q, NULL, 0, 22050 );
        q.pause = 10;   /* 10 µs = 0,22 vzorku */
        st_CMT_VSTREAM *c = mzcmt_fm_create_vstream ( &q, NULL, 0, 22050 );
        CHECK ( a && c && cmt_vstream_get_count_scans ( a ) == cmt_vstream_get_count_scans ( c ),
                "pause shorter than a sample dropped" );
        cmt_vstream_destroy ( a );
        cmt_vstream_destroy ( c );
    }

    /* dvě části: první s pauzou HIGH, druhá LEVEL 1 -> jedna půlvlna HIGH (pauza + první SHORT) */
    p.timebase = 1000;
    p.short_first = 10;
    p.short_second = 10;
    p.long_first = 20;
    p.long_second = 20;
    p.tone = 1;
    p.tm_long = p.tm_short = p.tm_end_long = 0;
    p.start_bits = 0;
    p.pause = 100;
    p.pause_level = 1;
    st_CMT_VSTREAM *vs = cmt_vstream_new ( 1000, CMT_VSTREAM_BYTELENGTH8, 1, CMT_STREAM_POLARITY_NORMAL );
    int ok = vs && mzcmt_fm_append_vstream ( vs, &p, NULL, 0 ) == EXIT_SUCCESS;
    p.pause = 0;
    ok = ok && mzcmt_fm_append_vstream ( vs, &p, NULL, 0 ) == EXIT_SUCCESS;
    uint64_t s[8];
    int v[8], n = 0;
    if ( ok ) {
        cmt_vstream_read_reset ( vs );
        while ( n < 8 && cmt_vstream_read_pulse ( vs, &s[n], &v[n] ) == EXIT_SUCCESS ) n++;
    }
    /* H10 L10 (tón) | H100 pauza + H10 tón druhé části = H110 | L10 */
    CHECK ( ok && n == 4 && v[0] == 1 && s[0] == 10 && v[1] == 0 && s[1] == 10 && v[2] == 1 && s[2] == 110 && v[3] == 0
            && s[3] == 10, "HIGH pause merges with the next part's first HIGH half" );
    cmt_vstream_destroy ( vs );
}


int main ( void ) {
    st_MZCMT_FM_PARAMS p;
    static st_HALVES h;

    /* výchozí hodnoty podle spec 5.3 a 6.1 */
    mzcmt_fm_params_init ( &p );
    CHECK ( p.timebase == 3546895u && p.level == 1 && p.start_bits == 1 && p.lsb_first == 0
            && p.checksum == 0 && p.context_count == 0, "params_init defaults" );
    CHECK ( mzcmt_fm_params_validate ( &p ) == EXIT_FAILURE, "zero pulses invalid" );

    /* 1 bajt 0x80, MSB, 1 start bit, bez checksumu: L(start) L(1) S x7 */
    base ( &p );
    uint8_t b80 = 0x80;
    memset ( &h, 0, sizeof ( h ) );
    CHECK ( mzcmt_fm_generate ( &p, &b80, 1, collect, &h ) == EXIT_SUCCESS, "generate 0x80" );
    CHECK ( h.n == 18 && h.dur[0] == 20 && h.dur[2] == 20 && h.dur[4] == 10 && h.dur[17] == 11,
            "0x80 MSB -> L L S S S S S S S" );
    CHECK ( h.level[0] == 1 && h.level[1] == 0, "LEVEL 1: first half HIGH" );

    /* LSB první: 0x80 -> L(start) S x7 L */
    p.lsb_first = 1;
    memset ( &h, 0, sizeof ( h ) );
    mzcmt_fm_generate ( &p, &b80, 1, collect, &h );
    CHECK ( h.n == 18 && h.dur[2] == 10 && h.dur[16] == 20, "0x80 LSB -> L S x7 L" );

    /* struktura: tón 3, tapemark 2/1/1, data 0x00, checksum popcount (0x0000), END 1/2, kopie 1 sep 2 */
    base ( &p );
    p.tone = 3;
    p.tm_long = 2;
    p.tm_short = 1;
    p.tm_end_long = 1;
    p.checksum = MZCMT_FM_CHECKSUM_MZ_POPCOUNT16_BE;
    p.end_long = 1;
    p.tail_short = 2;
    p.copies = 1;
    p.copy_sep = 2;
    uint8_t z = 0x00;
    memset ( &h, 0, sizeof ( h ) );
    mzcmt_fm_generate ( &p, &z, 1, collect, &h );
    /* pulzy: 3 S + 2 L + 1 S + 1 L + sekce (9 + 18 + 1 + 2 = 30) + kopie (2 + 30) = 69 -> 138 půlvln */
    CHECK ( h.n == 138, "structure pulse count 69" );

    /* kontexty: TONE_LAST zdědí TONE, START podle dalšího druhu, BIT_LAST_DATA_END zdědí BIT_LAST */
    base ( &p );
    p.tone = 2;
    p.context_count = 4;
    p.context[0] = ( st_MZCMT_FM_CONTEXT ) { MZCMT_FM_CTX_TONE, 0, MZCMT_FM_NEXT_ANY, 30, 31 };
    p.context[1] = ( st_MZCMT_FM_CONTEXT ) { MZCMT_FM_CTX_START, 1, 0, 40, 41 };
    p.context[2] = ( st_MZCMT_FM_CONTEXT ) { MZCMT_FM_CTX_START, 1, 1, 50, 51 };
    p.context[3] = ( st_MZCMT_FM_CONTEXT ) { MZCMT_FM_CTX_BIT_LAST, 0, MZCMT_FM_NEXT_ANY, 60, 61 };
    uint8_t two[2] = { 0x00, 0x80 };
    memset ( &h, 0, sizeof ( h ) );
    mzcmt_fm_generate ( &p, two, 2, collect, &h );
    /* pulzy (půlvlny): tón 0-1 (0-3) | START 2 (4-5), další S | bity 3-10 (6-21), bit 7 = pulz 10 (20-21)
       | START 11 (22-23), další L | bity 12-19 (24-39), bit 7 posledního bajtu = pulz 19 (38-39) */
    CHECK ( h.dur[0] == 30 && h.dur[2] == 30, "TONE and TONE_LAST (parent) = 30" );
    CHECK ( h.dur[4] == 40 && h.dur[5] == 41, "START next S = 40/41" );
    CHECK ( h.dur[20] == 60, "BIT_LAST of byte 0 = 60" );
    CHECK ( h.dur[22] == 50, "START next L = 50" );
    CHECK ( h.dur[38] == 60 && h.dur[39] == 61, "BIT_LAST_DATA_END inherits BIT_LAST" );

    /* explicitní checksum: bajty 0xFF 0x00 místo vypočtených */
    base ( &p );
    p.start_bits = 0;
    p.checksum = MZCMT_FM_CHECKSUM_MZ_POPCOUNT16_BE;
    p.checksum_explicit = 1;
    p.checksum_bytes[0] = 0xFF;
    p.checksum_bytes[1] = 0x00;
    memset ( &h, 0, sizeof ( h ) );
    mzcmt_fm_generate ( &p, &z, 1, collect, &h );
    CHECK ( h.n == 48 && h.dur[16] == 20 && h.dur[30] == 20 && h.dur[32] == 10, "explicit checksum FF 00" );

    /* vypočtený popcount: 3x 0xFF = 24 jedniček = 0x0018, vyšší bajt první */
    base ( &p );
    p.start_bits = 0;
    p.checksum = MZCMT_FM_CHECKSUM_MZ_POPCOUNT16_BE;
    uint8_t ff[3] = { 0xFF, 0xFF, 0xFF };
    memset ( &h, 0, sizeof ( h ) );
    mzcmt_fm_generate ( &p, ff, 3, collect, &h );
    /* checksum 00 18 = 00000000 00011000 -> pulzy 24-31 S, 32-34 S, 35-36 L, 37-39 S */
    CHECK ( h.n == 80 && h.dur[48] == 10 && h.dur[70] == 20 && h.dur[72] == 20 && h.dur[74] == 10,
            "computed popcount 0x0018 big-endian" );

    /* LEVEL 0 a pauza: první LOW, druhá HIGH, pauza LOW */
    base ( &p );
    p.level = 0;
    p.start_bits = 0;
    p.pause = 500;
    memset ( &h, 0, sizeof ( h ) );
    mzcmt_fm_generate ( &p, &z, 1, collect, &h );
    CHECK ( h.n == 17 && h.level[0] == 0 && h.level[1] == 1 && h.level[16] == 0 && h.dur[16] == 500,
            "LEVEL 0 + pause" );

    /* neplatné kontexty */
    base ( &p );
    p.context_count = 2;
    p.context[0] = ( st_MZCMT_FM_CONTEXT ) { MZCMT_FM_CTX_TONE, 0, MZCMT_FM_NEXT_ANY, 1, 1 };
    p.context[1] = ( st_MZCMT_FM_CONTEXT ) { MZCMT_FM_CTX_TONE, 0, MZCMT_FM_NEXT_ANY, 2, 2 };
    CHECK ( mzcmt_fm_params_validate ( &p ) == EXIT_FAILURE, "duplicate context triple invalid" );
    p.context_count = 1;
    p.context[0].ctx = MZCMT_FM_CTX_COUNT;
    CHECK ( mzcmt_fm_params_validate ( &p ) == EXIT_FAILURE, "context id out of range invalid" );
    p.context[0] = ( st_MZCMT_FM_CONTEXT ) { MZCMT_FM_CTX_TONE, 2, MZCMT_FM_NEXT_ANY, 1, 1 };
    CHECK ( mzcmt_fm_params_validate ( &p ) == EXIT_FAILURE, "kind 2 invalid" );
    p.context[0] = ( st_MZCMT_FM_CONTEXT ) { MZCMT_FM_CTX_TONE, 0, 3, 1, 1 };
    CHECK ( mzcmt_fm_params_validate ( &p ) == EXIT_FAILURE, "next 3 invalid" );
    p.context[0] = ( st_MZCMT_FM_CONTEXT ) { MZCMT_FM_CTX_TONE, 0, MZCMT_FM_NEXT_ANY, 0, 1 };
    CHECK ( mzcmt_fm_params_validate ( &p ) == EXIT_FAILURE, "zero half in context invalid" );
    base ( &p );
    p.checksum = 7;
    CHECK ( mzcmt_fm_params_validate ( &p ) == EXIT_FAILURE, "unknown checksum invalid" );
    base ( &p );
    p.timebase = 0;
    CHECK ( mzcmt_fm_params_validate ( &p ) == EXIT_FAILURE, "timebase 0 invalid" );

    /* callback chyba se propaguje */
    base ( &p );
    p.tone = 5000;   /* 10000 půlvln > kapacita 4096 */
    memset ( &h, 0, sizeof ( h ) );
    CHECK ( mzcmt_fm_generate ( &p, &z, 1, collect, &h ) == EXIT_FAILURE, "callback failure propagates" );

    test_vstream ();
    test_pause_level ();

    printf ( "\n=== MZCMT FM Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
