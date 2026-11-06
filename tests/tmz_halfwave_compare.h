/**
 * @file   tmz_halfwave_compare.h
 * @brief  Porovnání půlvln dvou vstreamů pro testy převodu v1.0 (spec 9.2):
 *         stejný počet a úrovně, délka každé půlvlny ±1 vzorek.
 *
 * Hlavička obsahuje statické funkce; includuje ji jen jeden testovací soubor programu.
 */
#ifndef TMZ_HALFWAVE_COMPARE_H
#define TMZ_HALFWAVE_COMPARE_H

#include <stdio.h>
#include <stdlib.h>
#include "libs/cmt_stream/cmt_vstream.h"

/**
 * @brief Porovná půlvlny vstreamu got s referencí ref.
 * @param ref Výstup generátoru v1.0.
 * @param got Výstup převodu (blok 0x46 přes player).
 * @param label Popis do výpisu.
 * @param[in,out] failures Počítadlo selhání (zvýší se o 1 při neshodě).
 * @return 1 = shoda v toleranci, 0 = neshoda (vypíše první rozdíl).
 * @post Čtecí pozice obou vstreamů je na konci.
 */
static int hw_compare ( st_CMT_VSTREAM *ref, st_CMT_VSTREAM *got, const char *label, int *failures ) {
    if ( !ref || !got ) {
        printf ( "FAIL: %s (missing stream)\n", label );
        ( *failures )++;
        return 0;
    }
    cmt_vstream_read_reset ( ref );
    cmt_vstream_read_reset ( got );
    uint64_t n = 0, sa, sb;
    int va, vb, worst = 0;
    for ( ;; ) {
        int ea = cmt_vstream_read_pulse ( ref, &sa, &va );
        int eb = cmt_vstream_read_pulse ( got, &sb, &vb );
        if ( ea != EXIT_SUCCESS || eb != EXIT_SUCCESS ) {
            if ( ea != eb ) {
                printf ( "FAIL: %s: half-wave count differs (at %llu, %s ended first)\n", label,
                         ( unsigned long long ) n, ea != EXIT_SUCCESS ? "reference" : "converted" );
                ( *failures )++;
                return 0;
            }
            break;
        }
        long long d = ( long long ) sa - ( long long ) sb;
        if ( va != vb || d > 1 || d < -1 ) {
            printf ( "FAIL: %s: half-wave %llu: ref %llu/%d, got %llu/%d\n", label, ( unsigned long long ) n,
                     ( unsigned long long ) sa, va, ( unsigned long long ) sb, vb );
            ( *failures )++;
            return 0;
        }
        if ( d != 0 ) worst = 1;
        n++;
    }
    printf ( "OK: %s (%llu half-waves, %s)\n", label, ( unsigned long long ) n, worst ? "+-1 sample" : "identical" );
    return 1;
}

#endif /* TMZ_HALFWAVE_COMPARE_H */
