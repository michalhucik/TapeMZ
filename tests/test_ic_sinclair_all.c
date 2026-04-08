/**
 * @file   test_ic_sinclair_all.c
 * @brief  Integracni test: dekodovani Intercopy SINCLAIR nahravky
 *
 * WAV soubor ic-sinclair-all.wav obsahuje 4 kopie programu Turbo Copy V1.21
 * nahrane s Intercopy v SINCLAIR rezimu ve 4 rychlostech.
 * Pred kazdym datovym blokem je audio zaznam s popisem nazvu dalsiho souboru.
 *
 * SINCLAIR format pouziva ZX Spectrum protokol (pilot -> sync -> data)
 * pri ruznych rychlostech. Kazda kopie se sklada ze dvou ZX bloku:
 * - Header blok (flag=0x00, 19 bajtu)
 * - Data blok (flag=0xFF, body + checksum)
 *
 * Test overuje:
 * 1. Alespon 4 datove bloky dekodovany (flag=0xFF)
 * 2. Body data z datovych bloku se shodnou s referencnim MZF
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
static const char *g_wav_path = "../tstdata/ic-sinclair-all.wav";

/** @brief Cesta k referencnimu MZF souboru (relativne z build/). */
static const char *g_ref_mzf_path = "../tstdata/mzf/Turbo_Copy_V1.21.mzf";

/** @brief Ocekavana velikost tela programu Turbo Copy V1.21. */
#define EXPECTED_BODY_SIZE  7152    /* 0x1BF0 */

/** @brief Minimalni pocet dekodovanych datovych bloku (flag=0xFF).
 *  4 kopie: SINCLAIR pri 4 rychlostech, kazda s 1 datovym blokem.
 */
#define EXPECTED_MIN_DATA_BLOCKS    4

/** @brief ZX Spectrum flag bajt pro datovy blok. */
#define ZX_FLAG_DATA    0xFF


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

    printf ( "=== Intercopy SINCLAIR Test ===\n" );
    printf ( "WAV: %s\n", wav_path );
    printf ( "REF: %s\n", ref_path );
    printf ( "Expected: >= %d data blocks, body_size=%u\n\n",
             EXPECTED_MIN_DATA_BLOCKS, EXPECTED_BODY_SIZE );

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

    /* === 3. Hledani datovych bloku a overeni === */

    uint32_t data_block_count = 0;
    uint32_t matched_count = 0;

    for ( uint32_t i = 0; i < result.file_count; i++ ) {
        const st_WAV_ANALYZER_FILE_RESULT *f = &result.files[i];

        printf ( "\n--- Block #%u ---\n", i + 1 );
        printf ( "Format: %s\n", wav_tape_format_name ( f->format ) );

        /*
         * SINCLAIR bloky jsou ulozeny jako tap_data (ZX protokol).
         * Datovy blok: flag(0xFF) + body(N) + checksum(1)
         * tap_data_size = 1 + N + 1 = N + 2
         */
        if ( !f->tap_data || f->tap_data_size < 3 ) {
            printf ( "INFO: no TAP data or too short\n" );
            continue;
        }

        uint8_t flag = f->tap_data[0];
        printf ( "Flag: 0x%02X, TAP size: %u, CRC: %s\n",
                 flag, f->tap_data_size,
                 f->header_crc == WAV_CRC_OK ? "OK" :
                 f->header_crc == WAV_CRC_ERROR ? "ERROR" : "N/A" );

        if ( flag != ZX_FLAG_DATA ) {
            printf ( "INFO: header block (skip body comparison)\n" );
            continue;
        }

        data_block_count++;

        /* extrakce body: tap_data[1 .. tap_data_size-2] */
        uint32_t body_size = f->tap_data_size - 2;
        const uint8_t *body = &f->tap_data[1];

        if ( body_size != ref_body_size ) {
            printf ( "FAIL: body_size = %u, expected %u\n", body_size, ref_body_size );
            failures++;
            continue;
        }

        printf ( "OK: body_size = %u\n", body_size );

        /* porovnani body dat s referenci */
        if ( memcmp ( body, ref_body, ref_body_size ) != 0 ) {
            for ( uint32_t j = 0; j < ref_body_size; j++ ) {
                if ( body[j] != ref_body[j] ) {
                    printf ( "FAIL: body differs from reference at offset %u "
                             "(ref=0x%02X, got=0x%02X)\n",
                             j, ref_body[j], body[j] );
                    break;
                }
            }
            failures++;
        } else {
            printf ( "OK: body data matches reference (%u bytes)\n", ref_body_size );
            matched_count++;
        }

        /* CRC */
        if ( f->header_crc != WAV_CRC_OK ) {
            printf ( "FAIL: CRC error\n" );
            failures++;
        } else {
            printf ( "OK: CRC\n" );
        }
    }

    /* === 4. Overeni poctu datovych bloku === */

    printf ( "\n--- Summary ---\n" );

    if ( ( int ) data_block_count < EXPECTED_MIN_DATA_BLOCKS ) {
        printf ( "FAIL: expected >= %d data blocks, got %u\n",
                 EXPECTED_MIN_DATA_BLOCKS, data_block_count );
        failures++;
    } else {
        printf ( "OK: %u data blocks decoded (expected >= %d)\n",
                 data_block_count, EXPECTED_MIN_DATA_BLOCKS );
    }

    if ( matched_count == 0 ) {
        printf ( "FAIL: no data block matches reference MZF\n" );
        failures++;
    } else {
        printf ( "OK: %u data block(s) match reference MZF\n", matched_count );
    }

    /* === Vysledek === */

    printf ( "\n=== Intercopy SINCLAIR Test: %s ===\n",
             failures == 0 ? "PASS" : "FAIL" );

    wav_analyzer_result_destroy ( &result );
    free ( ref_body );
    return failures == 0 ? 0 : 1;
}
