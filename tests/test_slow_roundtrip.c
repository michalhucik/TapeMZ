/**
 * @file   test_slow_roundtrip.c
 * @brief  Round-trip test: raw data -> SLOW WAV -> wav_decode_slow -> data
 *
 * Testovaci program:
 * 1. Vytvori testovaci data (256 bajtu: 0x00-0xFF)
 * 2. Zakoduje je jako SLOW signal pres mzcmt_slow (speed 0, raw - bez tape wrapperu)
 * 3. Ulozi do WAV souboru
 * 4. Nacte WAV zpet, preprocessing, extrakce pulzu
 * 5. Zavola wav_decode_slow_decode_bytes() primo
 * 6. Porovna dekodovane bajty s originalem
 *
 * SLOW format:
 * - 4 cykly na bajt (2 bity na cyklus), MSB first
 * - 4 symboly (00/01/10/11) s ruznymi delkami cyklu
 * - 5 rychlostnich urovni (speed 0-4)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzcmt_slow/mzcmt_slow.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/generic_driver/generic_driver.h"
#include "libs/generic_driver/memory_driver.h"
#include "libs/wav/wav.h"
#include "libs/wav_analyzer/wav_analyzer_types.h"
#include "libs/wav_analyzer/wav_preprocess.h"
#include "libs/wav_analyzer/wav_pulse.h"
#include "libs/wav_analyzer/wav_decode_slow.h"


/**
 * @brief Porovna dva bloky dat bajt po bajtu.
 *
 * @param name Popis bloku (pro chybove hlaseni).
 * @param expected Ocekavana data.
 * @param actual Skutecna data.
 * @param size Velikost bloku v bajtech.
 * @return 0 pokud shodne, 1 pokud rozdilne.
 */
static int compare_data ( const char *name, const uint8_t *expected, const uint8_t *actual, uint32_t size ) {
    uint32_t i;
    for ( i = 0; i < size; i++ ) {
        if ( expected[i] != actual[i] ) {
            printf ( "FAIL: %s mismatch at offset %u: expected 0x%02X, got 0x%02X\n",
                     name, i, expected[i], actual[i] );
            return 1;
        }
    }
    return 0;
}


int main ( int argc, char *argv[] ) {
    memory_driver_init ();

    const char *wav_path = "temp/slow_test.wav";
    const uint32_t rate = 44100;
    int failures = 0;

    if ( argc > 1 ) wav_path = argv[1];

    /* === 1. Priprava testovacich dat (256B: 0x00-0xFF) === */

    uint8_t body[256];
    int i;
    for ( i = 0; i < 256; i++ ) body[i] = ( uint8_t ) i;

    printf ( "Test data: 256 bytes (0x00-0xFF)\n" );

    /* === 2. SLOW kodovani (raw, speed 0) === */

    st_CMT_STREAM *stream = mzcmt_slow_create_stream (
                                body, 256, MZCMT_SLOW_SPEED_0,
                                CMT_STREAM_TYPE_VSTREAM, rate
                            );
    if ( !stream ) {
        fprintf ( stderr, "ERROR: mzcmt_slow_create_stream failed\n" );
        return 1;
    }

    printf ( "SLOW stream created: %.3f sec (speed 0)\n",
             cmt_stream_get_length ( stream ) );

    /* === 3. Ulozeni do WAV === */

    int result = cmt_stream_save_wav ( stream, rate, ( char* ) wav_path );
    cmt_stream_destroy ( stream );

    if ( result != EXIT_SUCCESS ) {
        fprintf ( stderr, "ERROR: cmt_stream_save_wav failed\n" );
        return 1;
    }

    printf ( "WAV written: %s\n", wav_path );

    /* === 4. Nacteni WAV a extrakce pulzu === */

    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *h_in = generic_driver_open_file ( &h, &d, ( char* ) wav_path, FILE_DRIVER_OPMODE_RO );
    if ( !h_in ) {
        fprintf ( stderr, "ERROR: Can't open WAV file for reading\n" );
        return 1;
    }

    en_WAV_ERROR wav_err;
    st_WAV_SIMPLE_HEADER *sh = wav_simple_header_new_from_handler ( h_in, &wav_err );
    if ( !sh ) {
        fprintf ( stderr, "ERROR: Can't parse WAV header\n" );
        generic_driver_close ( h_in );
        return 1;
    }

    /* preprocessing */
    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );

    double *samples = NULL;
    uint32_t sample_count = 0;
    en_WAV_ANALYZER_ERROR aerr = wav_preprocess_run ( h_in, sh, &config, &samples, &sample_count );
    wav_simple_header_destroy ( sh );
    generic_driver_close ( h_in );

    if ( aerr != WAV_ANALYZER_OK ) {
        fprintf ( stderr, "ERROR: wav_preprocess_run failed: %s\n",
                  wav_analyzer_error_string ( aerr ) );
        return 1;
    }

    /* extrakce pulzu */
    st_WAV_PULSE_SEQUENCE seq;
    memset ( &seq, 0, sizeof ( seq ) );
    aerr = wav_pulse_extract ( samples, sample_count, rate, &config, &seq );
    free ( samples );

    if ( aerr != WAV_ANALYZER_OK ) {
        fprintf ( stderr, "ERROR: wav_pulse_extract failed: %s\n",
                  wav_analyzer_error_string ( aerr ) );
        return 1;
    }

    printf ( "Pulses extracted: %u\n", seq.count );

    /* === 5. SLOW dekodovani === */

    uint8_t *decoded_data = NULL;
    uint32_t decoded_size = 0;
    uint32_t consumed = 0;

    aerr = wav_decode_slow_decode_bytes ( &seq, 0, &decoded_data, &decoded_size, &consumed );
    wav_pulse_sequence_destroy ( &seq );

    if ( aerr != WAV_ANALYZER_OK ) {
        fprintf ( stderr, "ERROR: wav_decode_slow_decode_bytes failed: %s\n",
                  wav_analyzer_error_string ( aerr ) );
        return 1;
    }

    printf ( "SLOW decoded: %u bytes\n", decoded_size );

    /* === 6. Verifikace === */

    if ( decoded_size != 256 ) {
        printf ( "FAIL: Expected 256 bytes, got %u\n", decoded_size );
        failures++;
    } else {
        printf ( "OK: Size = 256\n" );
    }

    uint32_t cmp_size = decoded_size < 256 ? decoded_size : 256;
    if ( compare_data ( "SLOW data", body, decoded_data, cmp_size ) ) {
        failures++;
    } else if ( decoded_size == 256 ) {
        printf ( "OK: Data match (256 bytes, all 0x00-0xFF)\n" );
    }

    free ( decoded_data );

    /* celkovy vysledek */
    printf ( "\n=== SLOW Round-Trip Test: %s ===\n", failures == 0 ? "PASS" : "FAIL" );

    return failures == 0 ? 0 : 1;
}
