/**
 * @file   test_fsk_tape_roundtrip.c
 * @brief  Round-trip test: MZF -> FSK tape WAV -> wav_analyzer -> MZF
 *
 * Testovaci program pro dvourychlostni FSK tape signal:
 * 1. Vytvori testovaci MZF v pameti (telo 256B: 0x00-0xFF)
 * 2. Zakoduje jako kompletni tape signal (preloader + loader + FSK data)
 *    pres mzcmt_fsk_create_tape_stream
 * 3. Ulozi do WAV souboru
 * 4. Nacte WAV zpet a dekoduje pres wav_analyzer
 * 5. Porovna originalni a dekodovana data
 *
 * Struktura tape signalu:
 * - Cast 1: Preloader hlavicka (NORMAL FM 1:1)
 * - Cast 2: FSK loader body (NORMAL FM)
 * - Cast 3: Uzivatelska data (FSK kodovani)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/mzcmt_fsk/mzcmt_fsk.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/generic_driver/generic_driver.h"
#include "libs/generic_driver/memory_driver.h"
#include "libs/wav_analyzer/wav_analyzer.h"


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
    for ( uint32_t i = 0; i < size; i++ ) {
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

    const char *wav_path = "temp/fsk_tape_test.wav";
    const uint32_t rate = 44100;
    int failures = 0;

    if ( argc > 1 ) wav_path = argv[1];

    /* === 1. Priprava testovacich dat === */

    st_MZF_HEADER header;
    memset ( &header, 0, sizeof ( header ) );
    header.ftype = 0x01;
    memcpy ( header.fname.name, "TEST FSK TAPE\r", 14 );
    header.fsize = 256;
    header.fstrt = 0x1200;
    header.fexec = 0x1200;

    uint8_t body[256];
    for ( int i = 0; i < 256; i++ ) body[i] = ( uint8_t ) i;

    printf ( "Original MZF: ftype=0x%02X fsize=%u fstrt=0x%04X fexec=0x%04X\n",
             header.ftype, header.fsize, header.fstrt, header.fexec );

    /* === 2. FSK tape kodovani (preloader + loader + FSK data) === */

    st_MZCMT_FSK_TAPE_CONFIG config = {
        .pulseset = MZCMT_FSK_PULSESET_800,
        .speed = MZCMT_FSK_SPEED_0,
        .loader_speed = 0,
    };

    st_CMT_STREAM *stream = mzcmt_fsk_create_tape_stream (
                                &header, body, header.fsize, &config,
                                CMT_STREAM_TYPE_VSTREAM, rate
                            );
    if ( !stream ) {
        fprintf ( stderr, "ERROR: mzcmt_fsk_create_tape_stream failed\n" );
        return 1;
    }

    printf ( "FSK tape stream created: %.3f sec (speed 0)\n",
             cmt_stream_get_length ( stream ) );

    /* === 3. Ulozeni do WAV === */

    int result = cmt_stream_save_wav ( stream, rate, ( char* ) wav_path );
    cmt_stream_destroy ( stream );

    if ( result != EXIT_SUCCESS ) {
        fprintf ( stderr, "ERROR: cmt_stream_save_wav failed\n" );
        return 1;
    }

    printf ( "WAV written: %s\n", wav_path );

    /* === 4. Dekodovani WAV zpet pres wav_analyzer === */

    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *h_in = generic_driver_open_file ( &h, &d, ( char* ) wav_path, FILE_DRIVER_OPMODE_RO );
    if ( !h_in ) {
        fprintf ( stderr, "ERROR: Can't open WAV file for reading\n" );
        return 1;
    }

    st_WAV_ANALYZER_CONFIG analyzer_config;
    wav_analyzer_config_default ( &analyzer_config );
    analyzer_config.verbose = 1;

    st_WAV_ANALYZER_RESULT analyzer_result;
    en_WAV_ANALYZER_ERROR aerr = wav_analyzer_analyze ( h_in, &analyzer_config, &analyzer_result );
    generic_driver_close ( h_in );

    if ( aerr != WAV_ANALYZER_OK ) {
        fprintf ( stderr, "ERROR: wav_analyzer_analyze failed: %s\n",
                  wav_analyzer_error_string ( aerr ) );
        return 1;
    }

    wav_analyzer_print_summary ( &analyzer_result, stdout, 0 );

    /* === 5. Verifikace === */

    if ( analyzer_result.file_count < 1 ) {
        printf ( "FAIL: No files decoded\n" );
        wav_analyzer_result_destroy ( &analyzer_result );
        return 1;
    }

    const st_WAV_ANALYZER_FILE_RESULT *f = &analyzer_result.files[0];

    /* kontrola formatu */
    if ( f->format != WAV_TAPE_FORMAT_FSK ) {
        printf ( "FAIL: Expected format FSK, got %s\n", wav_tape_format_name ( f->format ) );
        failures++;
    } else {
        printf ( "OK: Format = FSK\n" );
    }

    if ( !f->mzf ) {
        printf ( "FAIL: No MZF decoded\n" );
        wav_analyzer_result_destroy ( &analyzer_result );
        return 1;
    }

    /* kontrola parametru (extrahovanych z loader body) */
    if ( f->mzf->header.fsize != header.fsize ) {
        printf ( "FAIL: fsize mismatch: expected %u, got %u\n",
                 header.fsize, f->mzf->header.fsize );
        failures++;
    } else {
        printf ( "OK: fsize = %u\n", f->mzf->header.fsize );
    }

    if ( f->mzf->header.fstrt != header.fstrt ) {
        printf ( "FAIL: fstrt mismatch: expected 0x%04X, got 0x%04X\n",
                 header.fstrt, f->mzf->header.fstrt );
        failures++;
    } else {
        printf ( "OK: fstrt = 0x%04X\n", f->mzf->header.fstrt );
    }

    if ( f->mzf->header.fexec != header.fexec ) {
        printf ( "FAIL: fexec mismatch: expected 0x%04X, got 0x%04X\n",
                 header.fexec, f->mzf->header.fexec );
        failures++;
    } else {
        printf ( "OK: fexec = 0x%04X\n", f->mzf->header.fexec );
    }

    /* kontrola dat tela */
    if ( f->mzf->body_size != header.fsize ) {
        printf ( "FAIL: body_size mismatch: expected %u, got %u\n",
                 header.fsize, f->mzf->body_size );
        failures++;
    } else if ( f->mzf->body ) {
        if ( compare_data ( "Body", body, f->mzf->body, header.fsize ) ) {
            failures++;
        } else {
            printf ( "OK: Body data match (256 bytes, all 0x00-0xFF)\n" );
        }
    }

    /* celkovy vysledek */
    printf ( "\n=== FSK Tape Round-Trip Test: %s ===\n", failures == 0 ? "PASS" : "FAIL" );

    wav_analyzer_result_destroy ( &analyzer_result );
    return failures == 0 ? 0 : 1;
}
