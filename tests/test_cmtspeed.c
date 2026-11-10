/**
 * @file   test_cmtspeed.c
 * @brief  Testy knihovny cmtspeed (enum rychlostí, CMTSPEED_CUSTOM, převod z Bd).
 */

#include <stdio.h>
#include <string.h>

#include "libs/cmtspeed/cmtspeed.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )


int main ( void ) {
    char buf[64];

    CHECK ( CMTSPEED_COUNT == 11, "CMTSPEED_COUNT == 11" );
    CHECK ( CMTSPEED_CUSTOM == CMTSPEED_COUNT - 1, "CMTSPEED_CUSTOM is the last value before COUNT" );
    CHECK ( CMTSPEED_25_14 == 9, "existing values keep their numbers (25:14 == 9)" );

    CHECK ( cmtspeed_is_custom ( CMTSPEED_CUSTOM ) == 1, "is_custom ( CUSTOM ) == 1" );
    CHECK ( cmtspeed_is_custom ( CMTSPEED_2_1 ) == 0, "is_custom ( 2:1 ) == 0" );
    CHECK ( cmtspeed_is_custom ( CMTSPEED_NONE ) == 0, "is_custom ( NONE ) == 0" );
    CHECK ( cmtspeed_is_valid ( CMTSPEED_CUSTOM ) == 0, "is_valid ( CUSTOM ) == 0" );
    CHECK ( cmtspeed_is_valid ( CMTSPEED_25_14 ) == 1, "is_valid ( 25:14 ) == 1" );
    CHECK ( cmtspeed_is_valid ( CMTSPEED_COUNT ) == 0, "is_valid ( COUNT ) == 0" );

    CHECK ( cmtspeed_get_divisor ( CMTSPEED_CUSTOM ) == 0.0, "divisor ( CUSTOM ) == 0" );
    CHECK ( cmtspeed_get_bdspeed ( CMTSPEED_CUSTOM, 1200 ) == 0, "bdspeed ( CUSTOM ) == 0" );
    CHECK ( strcmp ( cmtspeed_get_ratiotxt ( CMTSPEED_CUSTOM ), "custom" ) == 0, "ratiotxt ( CUSTOM ) == custom" );
    cmtspeed_get_speedtxt ( buf, sizeof ( buf ), CMTSPEED_CUSTOM, 1200 );
    CHECK ( strcmp ( buf, "? Bd" ) == 0, "speedtxt ( CUSTOM ) == \"? Bd\"" );
    cmtspeed_get_ratiospeedtxt ( buf, sizeof ( buf ), CMTSPEED_CUSTOM, 1200 );
    CHECK ( strcmp ( buf, "custom - ? Bd" ) == 0, "ratiospeedtxt ( CUSTOM ) == \"custom - ? Bd\"" );

    int names_ok = 1;
    for ( int i = 0; i < CMTSPEED_COUNT; i++ ) {
        const char *t = cmtspeed_get_ratiotxt ( ( en_CMTSPEED ) i );
        if ( t == NULL || t[0] == '\0' ) names_ok = 0;
    }
    CHECK ( names_ok, "every enum value has a non-empty ratio text" );

    CHECK ( cmtspeed_from_bdspeed ( 1200, 1200 ) == CMTSPEED_1_1, "from_bdspeed ( 1200 ) == 1:1" );
    CHECK ( cmtspeed_from_bdspeed ( 1, 1200 ) != CMTSPEED_CUSTOM, "from_bdspeed never returns CUSTOM" );
    CHECK ( cmtspeed_from_bdspeed ( 2800, 1200 ) == CMTSPEED_7_3, "from_bdspeed ( 2800 ) == 7:3" );

    CHECK ( strcmp ( cmtspeed_version ( ), "2.1.0" ) == 0, "version 2.1.0" );

    return g_failures ? 1 : 0;
}
