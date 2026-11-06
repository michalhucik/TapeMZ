/**
 * @file   test_tmz_v2_ref.c
 * @brief  Vektory TMZ v1.1 přehrané generátorem FM proti záznamům z emulátoru
 *         z emulátoru (spec kap. 6.4, 6.6, 12). ROM-800-EXACT: shoda na T;
 *         TC-1.21: struktura přesně, průměr do 0,05 tiku, půlvlna do 30 T
 *         (kromě první v části). Spouštět z build/.
 *
 * Záznamy tstdata/ref/<jméno>.hwh jsou hrany signálu z emulátoru (formát .hwh)
 * (popis tstdata/popis.txt). Jde o emulaci (mz800emu 2.0.6 devel), ne o HW.
 * Porovnání je ve sdílené hlavičce tmz_ref_compare.h.
 * Bez tstdata/ vypíše SKIP a skončí 0 (tests/tstdata_skip.h).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/tzx/tzx.h"
#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_profile.h"
#include "libs/endianity/endianity.h"
#include "libs/generic_driver/generic_driver.h"
#include "tmz_ref_compare.h"
#include "tstdata_skip.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Testovací zásah do parametrů (ověření, že test umí selhat). */
static int g_mutate = 0;


/**
 * @brief Načte první blok .tmz (kopie; uvolnit tmz_block_free).
 * @param path Cesta.
 * @return Blok, nebo NULL.
 */
static st_TZX_BLOCK* load_first_block ( const char *path ) {
    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *hp = generic_driver_open_file ( &h, &d, ( char* ) path, FILE_DRIVER_OPMODE_RO );
    if ( !hp ) return NULL;
    en_TZX_ERROR err = TZX_OK;
    st_TZX_FILE *f = tzx_load ( hp, &err );
    generic_driver_close ( hp );
    if ( !f || f->block_count < 1 ) { if ( f ) tzx_free ( f ); return NULL; }
    st_TZX_BLOCK *b = malloc ( sizeof ( st_TZX_BLOCK ) );
    b->id = f->blocks[0].id;
    b->length = f->blocks[0].length;
    b->data = malloc ( b->length );
    memcpy ( b->data, f->blocks[0].data, b->length );
    tzx_free ( f );
    return b;
}


/**
 * @brief Načte vektor a porovná ho se záznamem.
 * @param vec Vektor .tmz.
 * @param ref Záznam .hwh.
 * @param exact 1 = shoda na T, 0 = meze TurboCopy.
 */
static void check_vector ( const char *vec, const char *ref, int exact ) {
    st_TZX_BLOCK *b = load_first_block ( vec );
    if ( !b ) {
        printf ( "FAIL: %s: load vector\n", vec );
        g_failures++;
        return;
    }
    ref_compare_block ( b, ref, exact, g_mutate, &g_failures, vec );
    tmz_block_free ( b );
}


/**
 * @brief Sestaví blok BASIC-1Z016 z hlavičky a kusů v souboru .bin.
 * @param path Soubor: 128 B hlavičky (pořadí souboru) a kusy po 258 B.
 * @param pause_ms Pauza za posledním kusem.
 * @return Blok (uvolní volající), nebo NULL.
 */
static st_TZX_BLOCK* basic_block ( const char *path, uint32_t pause_ms ) {
    static uint8_t buf[128 + 254 * 258];
    FILE *f = fopen ( path, "rb" );
    if ( !f ) return NULL;
    size_t n = fread ( buf, 1, sizeof ( buf ), f );
    fclose ( f );
    if ( n < 128 + 258 ) return NULL;
    st_MZF_HEADER h;
    memcpy ( &h, buf, sizeof ( h ) );
    h.fsize = endianity_bswap16_LE ( h.fsize );   /* tmz_profile_build_block čeká pořadí hostitele */
    h.fstrt = endianity_bswap16_LE ( h.fstrt );
    h.fexec = endianity_bswap16_LE ( h.fexec );
    st_TMZ_PROFILE_ARGS a;
    tmz_profile_args_init ( &a );
    a.pause_ms = pause_ms;
    en_TMZ_PROFILE_ERROR pe;
    return tmz_profile_build_block ( "BASIC-1Z016", &a, &h, buf + 128, ( uint32_t ) ( n - 128 ), &pe );
}


int main ( int argc, char **argv ) {
    if ( tstdata_skip ( "test_tmz_v2_ref" ) ) return 0;
    /* "--mutate": změní SHORT druhou půlvlnu o 1 jednotku; test pak musí selhat */
    g_mutate = ( argc > 1 && strcmp ( argv[1], "--mutate" ) == 0 );
    check_vector ( "../tstdata/tmz-v2/v6-rom-800-exact.tmz", "../tstdata/ref/rom800-save-emutest.hwh", 1 );
    check_vector ( "../tstdata/tmz-v2/v2-tc-2to1-loader-n.tmz", "../tstdata/ref/tc121-2to1-loaderN-emutest.hwh", 0 );
    check_vector ( "../tstdata/tmz-v2/v3-tc-3to1-loader-y.tmz", "../tstdata/ref/tc121-3to1-loaderY-emutest.hwh", 0 );

    /* BASIC-1Z016 z hlavičky a kusů záznamu proti záznamu z emulátoru, shoda na T */
    {
        st_TZX_BLOCK *b = basic_block ( "../tstdata/ref/basic1z016-data300.bin", 1000 );
        if ( !b ) {
            printf ( "FAIL: BASIC-1Z016 block not built\n" );
            g_failures++;
        } else {
            ref_compare_block ( b, "../tstdata/ref/basic1z016-data300-emutest.hwh", 1, g_mutate, &g_failures,
                                "BASIC-1Z016 data300" );
            int neg = 0;
            printf ( "(expected failures below)\n" );
            ref_compare_block ( b, "../tstdata/ref/basic1z016-data300-emutest.hwh", 1, 1, &neg, "negative BASIC" );
            printf ( "%s: BASIC mutation detected\n", neg > 0 ? "OK" : "FAIL" );
            if ( neg == 0 ) g_failures++;
            tmz_block_free ( b );
        }
    }

    /* srovnání hlídá i úrovně; převrácené úrovně musí selhat */
    {
        int neg = 0;
        st_TZX_BLOCK *b = load_first_block ( "../tstdata/tmz-v2/v6-rom-800-exact.tmz" );
        printf ( "(expected failures below)\n" );
        if ( b ) ref_compare_block ( b, "../tstdata/ref/rom800-save-emutest.hwh", 1, 2, &neg, "negative levels" );
        if ( b ) tmz_block_free ( b );
        printf ( "%s: inverted levels detected\n", ( b && neg > 0 ) ? "OK" : "FAIL" );
        if ( !b || neg == 0 ) g_failures++;
    }
    printf ( "\n=== TMZ v1.1 Reference Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
