/**
 * @file   test_ic_normal.c
 * @brief  Integracni test: dekodovani realne nahravky Intercopy NORMAL
 *
 * Testovaci program:
 * 1. Nacte tstdata/ic-normal.wav (realna nahravka z Intercopy)
 * 2. Spusti wav_analyzer s preprocessingem (DC offset, HP filtr, normalizace)
 * 3. Overi, ze byly dekodovany 4 kopie programu Y2K
 * 4. Kontroluje: format NORMAL, body_size 32747, ftype 0x01, CRC OK
 * 5. Porovna data vsech kopii (musi byt shodna)
 *
 * WAV soubor obsahuje 4 kopie programu Y2K v NORMAL FM pri rychlostech
 * 1200, 2400, 2800 a 3200 Bd. Pred kazdou kopii je kratky audio prefix
 * (synteticky hlas), ktery dekoder ignoruje.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/generic_driver/generic_driver.h"
#include "libs/wav_analyzer/wav_analyzer.h"


/** @brief Ocekavany pocet dekodovanych souboru. */
#define EXPECTED_FILES      4

/** @brief Ocekavana velikost tela Y2K programu. */
#define EXPECTED_BODY_SIZE  32747

/** @brief Ocekavany ftype (OBJ). */
#define EXPECTED_FTYPE      0x01

/** @brief Cesta k testovaci nahravce. */
static const char *g_wav_path = "../tstdata/ic-normal.wav";


int main ( int argc, char *argv[] ) {
    const char *wav_path = g_wav_path;
    int failures = 0;

    if ( argc > 1 ) wav_path = argv[1];

    /* === 1. Otevreni WAV souboru === */

    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );

    st_HANDLER *h_in = generic_driver_open_file ( &h, &d, ( char* ) wav_path, FILE_DRIVER_OPMODE_RO );
    if ( !h_in ) {
        fprintf ( stderr, "ERROR: cannot open '%s'\n", wav_path );
        fprintf ( stderr, "Run from build/ directory or pass path as argument.\n" );
        return 1;
    }

    /* === 2. Analyza s preprocessingem === */

    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );

    st_WAV_ANALYZER_RESULT result;
    en_WAV_ANALYZER_ERROR err = wav_analyzer_analyze ( h_in, &config, &result );
    generic_driver_close ( h_in );

    if ( err != WAV_ANALYZER_OK ) {
        fprintf ( stderr, "ERROR: analysis failed: %s\n", wav_analyzer_error_string ( err ) );
        return 1;
    }

    printf ( "WAV: %u Hz, %.1f sec, %u pulses, %u leaders, %u files\n",
             result.sample_rate, result.wav_duration_sec,
             result.total_pulses, result.leaders.count, result.file_count );

    /* === 3. Overeni poctu souboru === */

    if ( result.file_count < EXPECTED_FILES ) {
        printf ( "FAIL: expected >= %u files, got %u\n", EXPECTED_FILES, result.file_count );
        wav_analyzer_result_destroy ( &result );
        return 1;
    }
    printf ( "OK: %u files decoded (expected >= %u)\n", result.file_count, EXPECTED_FILES );

    /* === 4. Overeni kazdeho souboru === */

    for ( uint32_t i = 0; i < EXPECTED_FILES; i++ ) {
        const st_WAV_ANALYZER_FILE_RESULT *f = &result.files[i];

        printf ( "\n--- File #%u ---\n", i + 1 );

        /* format */
        if ( f->format != WAV_TAPE_FORMAT_NORMAL ) {
            printf ( "FAIL: expected NORMAL, got %s\n", wav_tape_format_name ( f->format ) );
            failures++;
        } else {
            printf ( "OK: format = NORMAL\n" );
        }

        /* MZF */
        if ( !f->mzf ) {
            printf ( "FAIL: no MZF decoded\n" );
            failures++;
            continue;
        }

        /* ftype */
        if ( f->mzf->header.ftype != EXPECTED_FTYPE ) {
            printf ( "FAIL: ftype = 0x%02X, expected 0x%02X\n",
                     f->mzf->header.ftype, EXPECTED_FTYPE );
            failures++;
        } else {
            printf ( "OK: ftype = 0x%02X\n", EXPECTED_FTYPE );
        }

        /* body size */
        if ( f->mzf->body_size != EXPECTED_BODY_SIZE ) {
            printf ( "FAIL: body_size = %u, expected %u\n",
                     f->mzf->body_size, EXPECTED_BODY_SIZE );
            failures++;
        } else {
            printf ( "OK: body_size = %u\n", EXPECTED_BODY_SIZE );
        }

        /* header CRC */
        if ( f->header_crc != WAV_CRC_OK ) {
            printf ( "FAIL: header CRC error\n" );
            failures++;
        } else {
            printf ( "OK: header CRC\n" );
        }

        /* body CRC */
        if ( f->body_crc != WAV_CRC_OK ) {
            printf ( "FAIL: body CRC error\n" );
            failures++;
        } else {
            printf ( "OK: body CRC\n" );
        }
    }

    /* === 5. Porovnani dat vsech kopii === */

    printf ( "\n--- Data consistency ---\n" );

    const st_WAV_ANALYZER_FILE_RESULT *ref = &result.files[0];
    if ( ref->mzf && ref->mzf->body && ref->mzf->body_size == EXPECTED_BODY_SIZE ) {
        for ( uint32_t i = 1; i < EXPECTED_FILES; i++ ) {
            const st_WAV_ANALYZER_FILE_RESULT *f = &result.files[i];
            if ( !f->mzf || !f->mzf->body || f->mzf->body_size != EXPECTED_BODY_SIZE ) {
                printf ( "FAIL: file #%u has no body data for comparison\n", i + 1 );
                failures++;
                continue;
            }

            if ( memcmp ( ref->mzf->body, f->mzf->body, EXPECTED_BODY_SIZE ) != 0 ) {
                /* najdeme prvni rozdilny bajt */
                for ( uint32_t j = 0; j < EXPECTED_BODY_SIZE; j++ ) {
                    if ( ref->mzf->body[j] != f->mzf->body[j] ) {
                        printf ( "FAIL: file #%u differs at offset %u (0x%02X vs 0x%02X)\n",
                                 i + 1, j, ref->mzf->body[j], f->mzf->body[j] );
                        break;
                    }
                }
                failures++;
            } else {
                printf ( "OK: file #%u data matches file #1 (%u bytes)\n", i + 1, EXPECTED_BODY_SIZE );
            }
        }
    } else {
        printf ( "FAIL: reference file #1 has no body data\n" );
        failures++;
    }

    /* === Celkovy vysledek === */

    printf ( "\n=== Intercopy NORMAL Test: %s ===\n", failures == 0 ? "PASS" : "FAIL" );

    wav_analyzer_result_destroy ( &result );
    return failures == 0 ? 0 : 1;
}
