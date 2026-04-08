/**
 * @file   test_ic_loader_all.c
 * @brief  Integracni test: dekodovani Intercopy FAST IPL loader nahravky
 *
 * WAV soubor ic-loader-all.wav obsahuje 4 kopie programu Turbo Copy V1.21
 * nahrane s FAST IPL loaderem v rychlostech 1200, 2400, 2800 a 3200 Bd.
 * Pred kazdym datovym blokem je audio zaznam s popisem nazvu dalsiho souboru.
 *
 * Kazda FASTIPL nahravka se sklada z:
 * - NORMAL FM preloader (hlavicka + FASTIPL loader binarni kod)
 * - FASTIPL datova cast (uzivatelska data)
 *
 * Test overuje:
 * 1. Alespon 4 soubory dekodovany
 * 2. Dekodovane soubory se shodnou s referencnim MZF (body data)
 * 3. Header pole (ftype, fsize, fstrt, fexec) odpovidaji ocekavanym hodnotam
 *
 * Referencni MZF: tstdata/mzf/Turbo_Copy_V1.21.mzf
 *   - SIZE: 0x1BF0 (7152 bajtu)
 *   - FROM: 0x1200
 *   - STRT: 0x2D88
 *   - ftype: 0x01 (OBJ)
 *
 * @par Licence:
 * GNU General Public License v3 (GPLv3)
 *
 * Copyright (C) 2026 Michal Hucik <hucik@ordoz.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/generic_driver/generic_driver.h"
#include "libs/generic_driver/memory_driver.h"
#include "libs/mzf/mzf.h"
#include "libs/wav_analyzer/wav_analyzer.h"


/** @brief Cesta k testovaci WAV nahravce (relativne z build/). */
static const char *g_wav_path = "../tstdata/ic-loader-all.wav";

/** @brief Cesta k referencnimu MZF souboru (relativne z build/). */
static const char *g_ref_mzf_path = "../tstdata/mzf/Turbo_Copy_V1.21.mzf";

/** @brief Ocekavana velikost tela programu Turbo Copy V1.21. */
#define EXPECTED_BODY_SIZE  7152    /* 0x1BF0 */

/** @brief Ocekavany ftype (OBJ). */
#define EXPECTED_FTYPE      0x01

/** @brief Ocekavana startovni adresa. */
#define EXPECTED_FSTRT      0x1200

/** @brief Ocekavana adresa spusteni. */
#define EXPECTED_FEXEC      0x2D88

/** @brief Minimalni pocet dekodovanych souboru.
 *  4 kopie: FASTIPL pri 1200, 2400, 2800, 3200 Bd.
 */
#define EXPECTED_MIN_FILES  4


/**
 * @brief Nacte body data z referencniho MZF souboru.
 *
 * Otevre MZF soubor pres generic_driver, parsuje hlavicku a telo,
 * vrati body data v dynamicky alokovanem bufferu.
 *
 * @param path Cesta k MZF souboru.
 * @param[out] out_body Ukazatel na alokovany buffer s body daty. Volajici uvolni.
 * @param[out] out_body_size Velikost body dat v bajtech.
 * @return 0 pri uspechu, 1 pri chybe.
 *
 * @pre path != NULL, out_body != NULL, out_body_size != NULL
 * @post Pri uspechu *out_body ukazuje na novy buffer (malloc), *out_body_size > 0.
 */
static int load_reference_mzf ( const char *path, uint8_t **out_body,
                                uint32_t *out_body_size ) {
    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *hin = generic_driver_open_file ( &h, &d, ( char* ) path, FILE_DRIVER_OPMODE_RO );
    if ( !hin ) {
        fprintf ( stderr, "ERROR: cannot open reference MZF '%s'\n", path );
        return 1;
    }

    en_MZF_ERROR merr;
    st_MZF *mzf = mzf_load ( hin, &merr );
    generic_driver_close ( hin );

    if ( !mzf ) {
        fprintf ( stderr, "ERROR: cannot parse MZF '%s'\n", path );
        return 1;
    }

    *out_body_size = mzf->body_size;
    *out_body = ( uint8_t* ) malloc ( mzf->body_size );
    if ( *out_body && mzf->body ) {
        memcpy ( *out_body, mzf->body, mzf->body_size );
    }

    mzf_free ( mzf );
    return 0;
}


int main ( int argc, char *argv[] ) {
    const char *wav_path = g_wav_path;
    const char *ref_path = g_ref_mzf_path;
    int failures = 0;

    if ( argc > 1 ) wav_path = argv[1];
    if ( argc > 2 ) ref_path = argv[2];

    memory_driver_init ();

    printf ( "=== Intercopy FAST IPL Loader Test ===\n" );
    printf ( "WAV: %s\n", wav_path );
    printf ( "REF: %s\n", ref_path );
    printf ( "Expected: >= %d files, body_size=%u\n\n", EXPECTED_MIN_FILES, EXPECTED_BODY_SIZE );

    /* === 1. Nacteni referencniho MZF === */

    uint8_t *ref_body = NULL;
    uint32_t ref_body_size = 0;

    if ( load_reference_mzf ( ref_path, &ref_body, &ref_body_size ) != 0 ) {
        return 1;
    }

    if ( ref_body_size != EXPECTED_BODY_SIZE ) {
        fprintf ( stderr, "ERROR: reference body_size=%u, expected %u\n",
                  ref_body_size, EXPECTED_BODY_SIZE );
        free ( ref_body );
        return 1;
    }

    printf ( "Reference MZF: body_size=%u\n", ref_body_size );

    /* === 2. Analyza WAV === */

    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *h_in = generic_driver_open_file ( &h, &d, ( char* ) wav_path, FILE_DRIVER_OPMODE_RO );
    if ( !h_in ) {
        fprintf ( stderr, "ERROR: cannot open WAV '%s'\n", wav_path );
        free ( ref_body );
        return 1;
    }

    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );
    st_WAV_ANALYZER_RESULT result;
    en_WAV_ANALYZER_ERROR err = wav_analyzer_analyze ( h_in, &config, &result );
    generic_driver_close ( h_in );

    if ( err != WAV_ANALYZER_OK ) {
        fprintf ( stderr, "ERROR: analysis failed: %s\n", wav_analyzer_error_string ( err ) );
        free ( ref_body );
        return 1;
    }

    printf ( "WAV: %u Hz, %.1f sec, %u pulses, %u leaders, %u files\n\n",
             result.sample_rate, result.wav_duration_sec,
             result.total_pulses, result.leaders.count, result.file_count );

    /* === 3. Overeni minimalniho poctu === */

    if ( ( int ) result.file_count < EXPECTED_MIN_FILES ) {
        printf ( "FAIL: expected >= %d files, got %u\n", EXPECTED_MIN_FILES, result.file_count );
        failures++;
    } else {
        printf ( "OK: %u files decoded (expected >= %d)\n", result.file_count, EXPECTED_MIN_FILES );
    }

    /* === 4. Hledani a overeni kopii shodnych s referenci === */

    uint32_t matched_count = 0;

    for ( uint32_t i = 0; i < result.file_count; i++ ) {
        const st_WAV_ANALYZER_FILE_RESULT *f = &result.files[i];

        printf ( "\n--- File #%u ---\n", i + 1 );
        printf ( "Format: %s\n", wav_tape_format_name ( f->format ) );

        if ( !f->mzf ) {
            printf ( "INFO: no MZF decoded\n" );
            continue;
        }

        printf ( "ftype=0x%02X, fsize=%u, body_size=%u, fstrt=0x%04X, fexec=0x%04X\n",
                 f->mzf->header.ftype, f->mzf->header.fsize,
                 f->mzf->body_size,
                 f->mzf->header.fstrt, f->mzf->header.fexec );
        printf ( "Header CRC: %s, Body CRC: %s\n",
                 f->header_crc == WAV_CRC_OK ? "OK" :
                 f->header_crc == WAV_CRC_ERROR ? "ERROR" : "N/A",
                 f->body_crc == WAV_CRC_OK ? "OK" :
                 f->body_crc == WAV_CRC_ERROR ? "ERROR" : "N/A" );

        /* kontrola shody s referenci */
        if ( f->mzf->body_size != ref_body_size ) {
            printf ( "INFO: body_size mismatch (skip reference comparison)\n" );
            continue;
        }

        if ( !f->mzf->body ) {
            printf ( "INFO: no body data\n" );
            continue;
        }

        /* porovnani body dat */
        if ( memcmp ( f->mzf->body, ref_body, ref_body_size ) != 0 ) {
            for ( uint32_t j = 0; j < ref_body_size; j++ ) {
                if ( f->mzf->body[j] != ref_body[j] ) {
                    printf ( "FAIL: body differs from reference at offset %u "
                             "(ref=0x%02X, got=0x%02X)\n",
                             j, ref_body[j], f->mzf->body[j] );
                    break;
                }
            }
            failures++;
            continue;
        }

        printf ( "OK: body data matches reference (%u bytes)\n", ref_body_size );
        matched_count++;

        /* overeni header poli */
        if ( f->mzf->header.ftype != EXPECTED_FTYPE ) {
            printf ( "FAIL: ftype=0x%02X, expected 0x%02X\n",
                     f->mzf->header.ftype, EXPECTED_FTYPE );
            failures++;
        }

        if ( f->mzf->header.fsize != EXPECTED_BODY_SIZE ) {
            printf ( "FAIL: fsize=%u, expected %u\n",
                     f->mzf->header.fsize, EXPECTED_BODY_SIZE );
            failures++;
        }

        if ( f->mzf->header.fstrt != EXPECTED_FSTRT ) {
            printf ( "FAIL: fstrt=0x%04X, expected 0x%04X\n",
                     f->mzf->header.fstrt, EXPECTED_FSTRT );
            failures++;
        }

        if ( f->mzf->header.fexec != EXPECTED_FEXEC ) {
            printf ( "FAIL: fexec=0x%04X, expected 0x%04X\n",
                     f->mzf->header.fexec, EXPECTED_FEXEC );
            failures++;
        }
    }

    /* === 5. Alespon 1 kopie musi odpovidat referenci === */

    if ( matched_count == 0 ) {
        printf ( "\nFAIL: no file matches reference MZF\n" );
        failures++;
    } else {
        printf ( "\nOK: %u file(s) match reference MZF\n", matched_count );
    }

    /* === Vysledek === */

    printf ( "\n=== Intercopy FAST IPL Loader Test: %s ===\n",
             failures == 0 ? "PASS" : "FAIL" );

    wav_analyzer_result_destroy ( &result );
    free ( ref_body );
    return failures == 0 ? 0 : 1;
}
