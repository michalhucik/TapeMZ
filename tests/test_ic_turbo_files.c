/**
 * @file   test_ic_turbo_files.c
 * @brief  Nahrávka tstdata/ic-turbo.wav (4 záznamy TURBO z Intercopy): každý soubor
 *         má datový leader mezi svou hlavičkou a hlavičkou dalšího souboru
 *         (1. soubor nesmí vzít data 2. nahrávky).
 *
 * Bez tstdata/ vypíše SKIP a skončí 0 (tests/tstdata_skip.h).
 * Spouštět z build/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/generic_driver/generic_driver.h"
#include "libs/wav_analyzer/wav_analyzer.h"
#include "tstdata_skip.h"

/** @brief Očekávaný počet záznamů v ic-turbo.wav. */
#define EXPECTED_FILES 4


int main ( void ) {
    if ( tstdata_skip ( "test_ic_turbo_files" ) ) return 0;
    int failures = 0;

    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *hp = generic_driver_open_file ( &h, &d, ( char* ) "../tstdata/ic-turbo.wav", FILE_DRIVER_OPMODE_RO );
    if ( !hp ) {
        printf ( "FAIL: cannot open ../tstdata/ic-turbo.wav\n" );
        return 1;
    }
    st_WAV_ANALYZER_CONFIG cfg;
    wav_analyzer_config_default ( &cfg );
    st_WAV_ANALYZER_RESULT r;
    memset ( &r, 0, sizeof ( r ) );
    en_WAV_ANALYZER_ERROR e = wav_analyzer_analyze ( hp, &cfg, &r );
    generic_driver_close ( hp );
    if ( e != WAV_ANALYZER_OK ) {
        printf ( "FAIL: analysis failed: %s\n", wav_analyzer_error_string ( e ) );
        return 1;
    }

    printf ( "%s: %u files decoded (expected %u)\n", r.file_count == EXPECTED_FILES ? "OK" : "FAIL",
             r.file_count, EXPECTED_FILES );
    if ( r.file_count != EXPECTED_FILES ) failures++;

    for ( uint32_t i = 0; i < r.file_count; i++ ) {
        const st_WAV_ANALYZER_FILE_RESULT *f = &r.files[i];
        int ok = f->header_leader_pulse < f->leader.start_index;
        if ( i + 1 < r.file_count ) ok = ok && f->leader.start_index < r.files[i + 1].header_leader_pulse;
        printf ( "%s: file %u header at pulse %u, data leader at %u%s\n", ok ? "OK" : "FAIL", i,
                 f->header_leader_pulse, f->leader.start_index,
                 ok ? "" : " (data leader outside its own recording)" );
        if ( !ok ) failures++;
        if ( f->body_crc != WAV_CRC_OK ) {
            printf ( "FAIL: file %u body CRC not OK\n", i );
            failures++;
        }
    }

    wav_analyzer_result_destroy ( &r );
    printf ( "\n=== IC TURBO files Test: %s ===\n", failures ? "FAIL" : "PASS" );
    return failures ? 1 : 0;
}
