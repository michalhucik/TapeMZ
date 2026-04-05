/**
 * @file   test_tc_normal.c
 * @brief  Integracni test: dekodovani realnych nahravek TurboCopy NORMAL
 *
 * Testuje dekodovani vsech zaznamu z TurboCopy nahravek pri ruznych
 * rychlostech (pomery 1:1 az 9:8). Kazda nahravka obsahuje program
 * Y2K ulozeny pri ruznych rychlostech. Test overuje:
 *
 * 1. Pocet dekodovanych souboru odpovida ocekavani
 * 2. Format vsech souboru = NORMAL
 * 3. Velikost body dat = 32747 (Y2K program)
 * 4. Header CRC = OK u vsech souboru
 * 5. Body CRC = OK u vsech souboru
 * 6. Body data vsech kopii jsou shodna
 *
 * Testovaci WAV soubory:
 * - tc-normal1.wav: 19 zaznamu (rychlosti 1:1 az 5:4)
 * - tc-normal2.wav: 18 zaznamu (rychlosti 5:6 az 8:3)
 * - tc-normal3.wav: 11 zaznamu (rychlosti 8:4 az 9:8)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/generic_driver/generic_driver.h"
#include "libs/wav_analyzer/wav_analyzer.h"


/** @brief Ocekavana velikost tela Y2K programu. */
#define EXPECTED_BODY_SIZE  32747

/** @brief Ocekavany ftype (OBJ). */
#define EXPECTED_FTYPE      0x01


/**
 * @brief Definice jedne testovaci nahravky.
 *
 * @param path Cesta k WAV souboru.
 * @param expected_files Ocekavany pocet dekodovanych souboru.
 * @param name Identifikator nahravky pro vypis.
 */
typedef struct {
    const char *path;           /**< Cesta k WAV souboru. */
    uint32_t expected_files;    /**< Presny pocet ocekavanych souboru. */
    const char *name;           /**< Popis nahravky pro vypis. */
} st_TC_TEST_CASE;


/** @brief Testovaci nahravky TurboCopy NORMAL. */
static const st_TC_TEST_CASE g_test_cases[] = {
    { "../tstdata/tc-normal1.wav", 19, "tc-normal1 (1:1 to 5:4)" },
    { "../tstdata/tc-normal2.wav", 18, "tc-normal2 (5:6 to 8:3)" },
    { "../tstdata/tc-normal3.wav", 11, "tc-normal3 (8:4 to 9:8)" },
};

/** @brief Pocet testovacich nahravek. */
#define TEST_CASE_COUNT  ( sizeof ( g_test_cases ) / sizeof ( g_test_cases[0] ) )


/**
 * @brief Otestuje jednu TurboCopy nahravku.
 *
 * Nacte WAV, spusti wav_analyzer, overi format, CRC a datovou
 * konzistenci vsech dekodovanych souboru.
 *
 * @param tc Definice testovaci nahravky.
 * @return Pocet selhani (0 = PASS).
 */
static int test_one_recording ( const st_TC_TEST_CASE *tc ) {
    int failures = 0;

    printf ( "\n========================================\n" );
    printf ( "Testing: %s\n", tc->name );
    printf ( "WAV: %s\n", tc->path );
    printf ( "Expected: %u files\n", tc->expected_files );
    printf ( "========================================\n" );

    /* otevreni WAV */
    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );

    st_HANDLER *h_in = generic_driver_open_file ( &h, &d, ( char* ) tc->path, FILE_DRIVER_OPMODE_RO );
    if ( !h_in ) {
        printf ( "FAIL: cannot open '%s'\n", tc->path );
        return 1;
    }

    /* analyza */
    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );

    st_WAV_ANALYZER_RESULT result;
    en_WAV_ANALYZER_ERROR err = wav_analyzer_analyze ( h_in, &config, &result );
    generic_driver_close ( h_in );

    if ( err != WAV_ANALYZER_OK ) {
        printf ( "FAIL: analysis failed: %s\n", wav_analyzer_error_string ( err ) );
        return 1;
    }

    printf ( "WAV: %u Hz, %.1f sec, %u pulses, %u leaders, %u files\n",
             result.sample_rate, result.wav_duration_sec,
             result.total_pulses, result.leaders.count, result.file_count );

    /* presny pocet souboru */
    if ( result.file_count != tc->expected_files ) {
        printf ( "FAIL: expected %u files, got %u\n", tc->expected_files, result.file_count );
        failures++;
    } else {
        printf ( "OK: %u files decoded\n", result.file_count );
    }

    /* overeni kazdeho souboru */
    uint32_t check_count = result.file_count < tc->expected_files
                           ? result.file_count : tc->expected_files;

    for ( uint32_t i = 0; i < check_count; i++ ) {
        const st_WAV_ANALYZER_FILE_RESULT *f = &result.files[i];

        /* format */
        if ( f->format != WAV_TAPE_FORMAT_NORMAL ) {
            printf ( "FAIL: file #%u: expected NORMAL, got %s\n",
                     i + 1, wav_tape_format_name ( f->format ) );
            failures++;
        }

        if ( !f->mzf ) {
            printf ( "FAIL: file #%u: no MZF decoded\n", i + 1 );
            failures++;
            continue;
        }

        /* body size */
        if ( f->mzf->body_size != EXPECTED_BODY_SIZE ) {
            printf ( "FAIL: file #%u: body_size=%u, expected %u\n",
                     i + 1, f->mzf->body_size, EXPECTED_BODY_SIZE );
            failures++;
        }

        /* header CRC */
        if ( f->header_crc != WAV_CRC_OK ) {
            printf ( "FAIL: file #%u: header CRC error\n", i + 1 );
            failures++;
        }

        /* body CRC */
        if ( f->body_crc != WAV_CRC_OK ) {
            printf ( "FAIL: file #%u: body CRC error\n", i + 1 );
            failures++;
        }
    }

    /* datova konzistence - vsechny kopie musi byt shodne */
    if ( result.file_count >= 2 && result.files[0].mzf &&
         result.files[0].mzf->body &&
         result.files[0].mzf->body_size == EXPECTED_BODY_SIZE ) {

        const uint8_t *ref_body = result.files[0].mzf->body;

        for ( uint32_t i = 1; i < result.file_count; i++ ) {
            const st_WAV_ANALYZER_FILE_RESULT *f = &result.files[i];
            if ( !f->mzf || !f->mzf->body ||
                 f->mzf->body_size != EXPECTED_BODY_SIZE ) {
                continue;
            }

            if ( memcmp ( ref_body, f->mzf->body, EXPECTED_BODY_SIZE ) != 0 ) {
                for ( uint32_t j = 0; j < EXPECTED_BODY_SIZE; j++ ) {
                    if ( ref_body[j] != f->mzf->body[j] ) {
                        printf ( "FAIL: file #%u differs at offset %u "
                                 "(0x%02X vs 0x%02X)\n",
                                 i + 1, j, ref_body[j], f->mzf->body[j] );
                        break;
                    }
                }
                failures++;
            }
        }

        if ( failures == 0 ) {
            printf ( "OK: all %u copies have identical body data\n",
                     result.file_count );
        }
    }

    wav_analyzer_result_destroy ( &result );
    return failures;
}


int main ( void ) {
    int total_failures = 0;

    for ( uint32_t i = 0; i < TEST_CASE_COUNT; i++ ) {
        int f = test_one_recording ( &g_test_cases[i] );
        total_failures += f;
    }

    printf ( "\n=== TurboCopy NORMAL Test: %s ===\n",
             total_failures == 0 ? "PASS" : "FAIL" );

    return total_failures == 0 ? 0 : 1;
}
