/**
 * @file   test_tmz_tlv.c
 * @brief  Testy kódování TLV (spec TMZ v1.1 kap. 5.1).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/tmz/tmz_tlv.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )


int main ( void ) {
    st_TMZ_TLVBUF b;
    tmz_tlvbuf_init ( &b );
    uint8_t v4[4] = { 0x40, 0x42, 0x0F, 0x00 };   /* 1000000 LE */
    CHECK ( tmz_tlvbuf_add ( &b, TMZ_TAG_TIMEBASE, 1, v4, 4 ) == EXIT_SUCCESS, "add TIMEBASE" );
    CHECK ( tmz_tlvbuf_add ( &b, TMZ_TAG_STOP, 0, "\x01", 1 ) == EXIT_SUCCESS, "add STOP" );
    static const uint8_t expect[] = { 0x01, 0x80, 0x04, 0x00, 0x40, 0x42, 0x0F, 0x00,
                                      0x04, 0x00, 0x01, 0x00, 0x01 };
    CHECK ( b.length == sizeof ( expect ) && memcmp ( b.data, expect, sizeof ( expect ) ) == 0,
            "encoded bytes match spec layout" );
    CHECK ( tmz_tlvbuf_add ( &b, ( uint16_t ) ( TMZ_TAG_STOP | TMZ_TLV_CRITICAL ), 0, "\x01", 1 ) == EXIT_FAILURE
            && b.length == sizeof ( expect ), "tag with bit 15 rejected, buffer unchanged" );
    /* délka blízko UINT32_MAX nepřeteče (stav nastavený ručně, nic se nealokuje) */
    {
        st_TMZ_TLVBUF big = { b.data, UINT32_MAX - 6u, UINT32_MAX - 6u };
        CHECK ( tmz_tlvbuf_add ( &big, TMZ_TAG_STOP, 0, "\x01\x02\x03\x04", 4 ) == EXIT_FAILURE
                && big.length == UINT32_MAX - 6u && big.data == b.data, "length overflow rejected" );
    }

    st_TMZ_TLV_ITER it;
    st_TMZ_TLV t;
    tmz_tlv_iter_init ( &it, b.data, b.length );
    int r1 = tmz_tlv_iter_next ( &it, &t );
    CHECK ( r1 == 1 && t.tag == TMZ_TAG_TIMEBASE && t.critical && t.length == 4 && tmz_rd32 ( t.value ) == 1000000,
            "iter item 1 = TIMEBASE critical 1000000" );
    int r2 = tmz_tlv_iter_next ( &it, &t );
    CHECK ( r2 == 1 && t.tag == TMZ_TAG_STOP && !t.critical && t.length == 1 && t.value[0] == 1,
            "iter item 2 = STOP non-critical" );
    CHECK ( tmz_tlv_iter_next ( &it, &t ) == 0, "iter end" );
    CHECK ( tmz_tlv_area_check ( b.data, b.length ) == EXIT_SUCCESS, "area well-formed" );

    /* useknutá hodnota: délka 4, k dispozici 3 B */
    CHECK ( tmz_tlv_area_check ( b.data, 7 ) == EXIT_FAILURE, "truncated value detected" );
    tmz_tlv_iter_init ( &it, b.data, 7 );
    CHECK ( tmz_tlv_iter_next ( &it, &t ) == -1, "iter reports overrun" );
    /* useknutá hlavička položky: 2 B */
    CHECK ( tmz_tlv_area_check ( b.data, 2 ) == EXIT_FAILURE, "truncated item header detected" );
    CHECK ( tmz_tlv_area_check ( NULL, 0 ) == EXIT_SUCCESS, "empty area ok" );

    uint8_t w[4];
    tmz_wr32 ( w, 0x11223344u );
    CHECK ( w[0] == 0x44 && w[3] == 0x11 && tmz_rd32 ( w ) == 0x11223344u, "wr32/rd32 LE" );
    tmz_wr16 ( w, 0xA1B2u );
    CHECK ( w[0] == 0xB2 && tmz_rd16 ( w ) == 0xA1B2u, "wr16/rd16 LE" );

    tmz_tlvbuf_free ( &b );
    CHECK ( b.data == NULL && b.length == 0, "buffer freed" );

    printf ( "\n=== TMZ TLV Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
