/**
 * @file   test_ic_wav.c
 * @brief  Genericky integracni test: WAV -> wav_analyzer -> MZF verifikace
 *
 * Universalni testovaci program pro overeni dekodovani realnych nahravek.
 * Parametry z prikazove radky urcuji:
 * - vstupni WAV soubor
 * - referencni MZF soubor (pro porovnani body dat)
 * - ocekavany pocet dekodovanych souboru
 * - ocekavany format (NORMAL, TURBO, FASTIPL, FSK, SLOW, DIRECT, CPM-CMT, ...)
 *
 * Test overuje:
 * - pocet dekodovanych souboru >= ocekavany
 * - format kazdeho souboru
 * - body_size = referencni velikost
 * - bajtova shoda body dat s referencnim MZF
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/generic_driver/generic_driver.h"
#include "libs/generic_driver/memory_driver.h"
#include "libs/mzf/mzf.h"
#include "libs/wav_analyzer/wav_analyzer.h"


/**
 * @brief Nacte body data z referencniho MZF souboru.
 *
 * @param path Cesta k MZF souboru.
 * @param[out] out_body Vystupni ukazatel na body data (malloc).
 * @param[out] out_body_size Velikost body dat.
 * @param[out] out_header Vystupni hlavicka (volitelne, muze byt NULL).
 * @return 0 pri uspechu, 1 pri chybe.
 */
static int load_reference_mzf ( const char *path, uint8_t **out_body,
                                uint32_t *out_body_size, st_MZF_HEADER *out_header ) {
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

    if ( out_header ) {
        memcpy ( out_header, &mzf->header, sizeof ( st_MZF_HEADER ) );
    }

    *out_body_size = mzf->body_size;
    *out_body = ( uint8_t* ) malloc ( mzf->body_size );
    if ( *out_body && mzf->body ) {
        memcpy ( *out_body, mzf->body, mzf->body_size );
    }

    mzf_free ( mzf );
    return 0;
}


/**
 * @brief Prevede textovy nazev formatu na enum.
 */
static en_WAV_TAPE_FORMAT parse_format_name ( const char *name ) {
    if ( strcmp ( name, "NORMAL" ) == 0 )   return WAV_TAPE_FORMAT_NORMAL;
    if ( strcmp ( name, "TURBO" ) == 0 )    return WAV_TAPE_FORMAT_TURBO;
    if ( strcmp ( name, "FASTIPL" ) == 0 )  return WAV_TAPE_FORMAT_FASTIPL;
    if ( strcmp ( name, "FSK" ) == 0 )      return WAV_TAPE_FORMAT_FSK;
    if ( strcmp ( name, "SLOW" ) == 0 )     return WAV_TAPE_FORMAT_SLOW;
    if ( strcmp ( name, "DIRECT" ) == 0 )   return WAV_TAPE_FORMAT_DIRECT;
    if ( strcmp ( name, "CPM-CMT" ) == 0 )  return WAV_TAPE_FORMAT_CPM_CMT;
    if ( strcmp ( name, "CPM-TAPE" ) == 0 ) return WAV_TAPE_FORMAT_CPM_TAPE;
    if ( strcmp ( name, "MZ-80B" ) == 0 )   return WAV_TAPE_FORMAT_MZ80B;
    if ( strcmp ( name, "ZX" ) == 0 )       return WAV_TAPE_FORMAT_ZX_SPECTRUM;
    return WAV_TAPE_FORMAT_UNKNOWN;
}


static void print_usage ( const char *prog ) {
    fprintf ( stderr,
              "Usage: %s <wav> <ref.mzf> <min_files> <format>\n"
              "  wav       - input WAV file\n"
              "  ref.mzf   - reference MZF for body comparison\n"
              "  min_files - minimum expected decoded files\n"
              "  format    - expected format (NORMAL, TURBO, FASTIPL, FSK, SLOW, ...)\n",
              prog );
}


int main ( int argc, char *argv[] ) {
    if ( argc < 5 ) {
        print_usage ( argv[0] );
        return 1;
    }

    memory_driver_init ();

    const char *wav_path = argv[1];
    const char *ref_path = argv[2];
    int min_files = atoi ( argv[3] );
    en_WAV_TAPE_FORMAT expected_format = parse_format_name ( argv[4] );
    int failures = 0;

    printf ( "WAV: %s\n", wav_path );
    printf ( "REF: %s\n", ref_path );
    printf ( "Expected: >= %d files, format %s\n\n", min_files, argv[4] );

    /* === 1. Nacteni referencniho MZF === */

    uint8_t *ref_body = NULL;
    uint32_t ref_body_size = 0;
    st_MZF_HEADER ref_header;

    if ( load_reference_mzf ( ref_path, &ref_body, &ref_body_size, &ref_header ) != 0 ) {
        return 1;
    }
    printf ( "Reference MZF: body_size=%u, ftype=0x%02X\n", ref_body_size, ref_header.ftype );

    /* === 2. Analyza WAV === */

    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *hin = generic_driver_open_file ( &h, &d, ( char* ) wav_path, FILE_DRIVER_OPMODE_RO );
    if ( !hin ) {
        fprintf ( stderr, "ERROR: cannot open WAV '%s'\n", wav_path );
        free ( ref_body );
        return 1;
    }

    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );

    st_WAV_ANALYZER_RESULT result;
    en_WAV_ANALYZER_ERROR err = wav_analyzer_analyze ( hin, &config, &result );
    generic_driver_close ( hin );

    if ( err != WAV_ANALYZER_OK ) {
        fprintf ( stderr, "ERROR: analysis failed: %s\n", wav_analyzer_error_string ( err ) );
        free ( ref_body );
        return 1;
    }

    printf ( "Decoded: %u files (leaders: %u)\n\n", result.file_count, result.leaders.count );

    /* === 3. Overeni poctu === */

    if ( ( int ) result.file_count < min_files ) {
        printf ( "FAIL: expected >= %d files, got %u\n", min_files, result.file_count );
        failures++;
    } else {
        printf ( "OK: %u files decoded\n", result.file_count );
    }

    /* === 4. Overeni kazdeho souboru === */

    uint32_t check_count = ( ( int ) result.file_count < min_files )
                           ? result.file_count
                           : ( uint32_t ) min_files;

    for ( uint32_t i = 0; i < check_count; i++ ) {
        const st_WAV_ANALYZER_FILE_RESULT *f = &result.files[i];
        printf ( "\n--- File #%u ---\n", i + 1 );

        /* format */
        if ( f->format != expected_format ) {
            printf ( "FAIL: format = %s, expected %s\n",
                     wav_tape_format_name ( f->format ), argv[4] );
            failures++;
        } else {
            printf ( "OK: format = %s\n", wav_tape_format_name ( f->format ) );
        }

        if ( !f->mzf ) {
            printf ( "FAIL: no MZF decoded\n" );
            failures++;
            continue;
        }

        /* body size */
        if ( f->mzf->body_size != ref_body_size ) {
            printf ( "FAIL: body_size = %u, expected %u\n", f->mzf->body_size, ref_body_size );
            failures++;
        } else {
            printf ( "OK: body_size = %u\n", ref_body_size );
        }

        /* key header fields */
        if ( f->mzf->header.ftype != ref_header.ftype ) {
            printf ( "FAIL: ftype = 0x%02X, expected 0x%02X\n",
                     f->mzf->header.ftype, ref_header.ftype );
            failures++;
        }

        if ( f->mzf->header.fsize != ref_header.fsize ) {
            printf ( "FAIL: fsize = %u, expected %u\n", f->mzf->header.fsize, ref_header.fsize );
            failures++;
        }

        /* body data comparison */
        if ( f->mzf->body && f->mzf->body_size == ref_body_size && ref_body ) {
            if ( memcmp ( f->mzf->body, ref_body, ref_body_size ) != 0 ) {
                /* najdi prvni rozdilny bajt */
                for ( uint32_t j = 0; j < ref_body_size; j++ ) {
                    if ( f->mzf->body[j] != ref_body[j] ) {
                        printf ( "FAIL: body differs at offset %u (0x%02X vs 0x%02X)\n",
                                 j, ref_body[j], f->mzf->body[j] );
                        break;
                    }
                }
                failures++;
            } else {
                printf ( "OK: body data matches reference (%u bytes)\n", ref_body_size );
            }
        }
    }

    /* === Vysledek === */

    printf ( "\n=== Test: %s ===\n", failures == 0 ? "PASS" : "FAIL" );

    wav_analyzer_result_destroy ( &result );
    free ( ref_body );
    return failures == 0 ? 0 : 1;
}
