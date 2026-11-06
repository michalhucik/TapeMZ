/**
 * @file   test_tmz_part_manchester.c
 * @brief  TLV části rodiny MANCHESTER (spec TMZ v1.1 kap. 5.1, 7.2):
 *         roundtrip encode/decode, povinné HALVES, délky a opakování tagů,
 *         neznámé tagy, sčítání položek CONTEXT.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/tmz/tmz_tlv.h"
#include "libs/tmz/tmz_part_manchester.h"
#include "manch_test_params.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )

/** @brief HALVES 1000/1000/1000/1000 (tag 0501h, povinný). */
static const uint8_t g_halves[20] = { 0x01, 0x85, 0x10, 0x00, 0xE8, 0x03, 0, 0, 0xE8, 0x03, 0, 0,
                                      0xE8, 0x03, 0, 0, 0xE8, 0x03, 0, 0 };


/**
 * @brief Porovná dvě sady parametrů (všechna pole kromě nepoužitých položek kontextu).
 * @param a První. @param b Druhá. @return 1 = shodné.
 */
static int same ( const st_MZCMT_MANCH_PARAMS *a, const st_MZCMT_MANCH_PARAMS *b ) {
    if ( a->context_count != b->context_count ) return 0;
    st_MZCMT_MANCH_PARAMS x = *a, y = *b;
    memset ( x.context + x.context_count, 0, sizeof ( x.context ) - x.context_count * sizeof ( x.context[0] ) );
    memset ( y.context + y.context_count, 0, sizeof ( y.context ) - y.context_count * sizeof ( y.context[0] ) );
    return memcmp ( &x, &y, sizeof ( x ) ) == 0;
}


/**
 * @brief Encode -> decode a porovnání.
 * @param p Parametry. @param label Popis.
 */
static void roundtrip ( const st_MZCMT_MANCH_PARAMS *p, const char *label ) {
    st_TMZ_TLVBUF b;
    tmz_tlvbuf_init ( &b );
    st_TMZ_PART_COMMON c = { 1, 0, 0, 0 };   /* STOP */
    int enc = tmz_part_manchester_encode ( p, &c, 0, &b );
    st_MZCMT_MANCH_PARAMS q;
    st_TMZ_PART_COMMON qc;
    en_TMZ_PART_ERROR e = enc == EXIT_SUCCESS ? tmz_part_manchester_decode ( b.data, b.length, 1, &q, &qc ) : TMZ_PART_ERROR_BAD_VALUE;
    char msg[96];
    snprintf ( msg, sizeof ( msg ), "%s: encode -> decode identical", label );
    CHECK ( e == TMZ_PART_OK && same ( p, &q ) && qc.stop == 1, msg );
    tmz_tlvbuf_free ( &b );
}


/**
 * @brief Dekóduje TLV složené z HALVES a dalších bajtů.
 * @param extra Další položky. @param n Délka. @param p Výstup.
 * @return Výsledek dekódování.
 */
static en_TMZ_PART_ERROR decode_with ( const uint8_t *extra, uint32_t n, st_MZCMT_MANCH_PARAMS *p ) {
    uint8_t buf[256];
    memcpy ( buf, g_halves, sizeof ( g_halves ) );
    memcpy ( buf + sizeof ( g_halves ), extra, n );
    st_TMZ_PART_COMMON c;
    return tmz_part_manchester_decode ( buf, ( uint32_t ) sizeof ( g_halves ) + n, 3546895, p, &c );
}


int main ( void ) {
    st_MZCMT_MANCH_PARAMS p;
    pezik ( &p, 43, 1 );
    roundtrip ( &p, "Pezik 2400 header" );
    pezik ( &p, 28, 0 );
    roundtrip ( &p, "Pezik 3200 body" );
    intercopy ( &p, 1 );
    p.pause_level = 0;
    roundtrip ( &p, "Intercopy header" );
    intercopy ( &p, 0 );
    p.checksum_explicit = 1;
    p.checksum_bytes[0] = 0x12;
    p.checksum_bytes[1] = 0x34;
    p.level = 0;
    p.timebase = 1000000;
    roundtrip ( &p, "Intercopy body, explicit checksum, LEVEL 0, TIMEBASE" );

    st_TMZ_PART_COMMON c;
    CHECK ( tmz_part_manchester_decode ( NULL, 0, 3546895, &p, &c ) == TMZ_PART_ERROR_MISSING_PULSES, "missing HALVES" );

    static const uint8_t h15[19] = { 0x01, 0x85, 0x0F, 0x00, 0xE8, 0x03, 0, 0, 0xE8, 0x03, 0, 0, 0xE8, 0x03, 0, 0, 0xE8, 0x03, 0 };
    CHECK ( tmz_part_manchester_decode ( h15, sizeof ( h15 ), 3546895, &p, &c ) == TMZ_PART_ERROR_BAD_VALUE, "HALVES length 15" );

    static const uint8_t pilot2[16] = { 0x02, 0x85, 0x06, 0x00, 0x10, 0, 0, 0, 3, 0x06,
                                        0x02, 0x85, 0x06, 0x00, 0x10, 0 };
    uint8_t rep[20];
    memcpy ( rep, pilot2, 10 );
    memcpy ( rep + 10, pilot2, 10 );
    CHECK ( decode_with ( rep, 20, &p ) == TMZ_PART_ERROR_BAD_VALUE, "repeated PILOT" );
    CHECK ( decode_with ( pilot2, 10, &p ) == TMZ_PART_OK && p.pilot_count == 16 && p.pilot_bits == 3 && p.pilot_pattern == 6,
            "PILOT decoded" );

    static const uint8_t unk_crit[5] = { 0xFF, 0x85, 0x01, 0x00, 0x00 };
    static const uint8_t unk_opt[5] = { 0xFF, 0x05, 0x01, 0x00, 0x00 };
    CHECK ( decode_with ( unk_crit, 5, &p ) == TMZ_PART_ERROR_UNKNOWN_CRITICAL, "unknown critical tag 05FFh" );
    CHECK ( decode_with ( unk_opt, 5, &p ) == TMZ_PART_OK, "unknown optional tag 05FFh skipped" );

    /* CONTEXT dvakrát: položky se sčítají */
    static const uint8_t ctx2[20] = { 0x08, 0x85, 0x06, 0x00, MZCMT_MANCH_CTX_CK_MID, 0xFF, 0xE8, 0x02, 0, 0,
                                      0x08, 0x85, 0x06, 0x00, MZCMT_MANCH_CTX_CK_END, 0xFF, 0xE9, 0x02, 0, 0 };
    CHECK ( decode_with ( ctx2, 20, &p ) == TMZ_PART_OK && p.context_count == 2 && p.context[1].ctx == MZCMT_MANCH_CTX_CK_END
            && p.context[1].half2 == 745, "CONTEXT twice accumulates" );

    /* neplatné parametry (vzor 9 bitů) = BAD_VALUE */
    static const uint8_t pilot9[10] = { 0x02, 0x85, 0x06, 0x00, 0x10, 0, 0, 0, 9, 0x06 };
    CHECK ( decode_with ( pilot9, 10, &p ) == TMZ_PART_ERROR_BAD_VALUE, "pilot of 9 bits invalid" );

    printf ( "\n=== TMZ Part MANCHESTER Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
