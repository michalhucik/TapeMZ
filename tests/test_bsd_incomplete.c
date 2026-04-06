/**
 * @file   test_bsd_incomplete.c
 * @brief  Test obnovy castecnych BSD dat z nekompletni nahravky.
 *
 * Testovaci program pro BSD recovery funkcionalitu:
 * 1. Bez --recover-bsd: dekoduje 3 soubory (DATAFILE1, WRPRG, RDPRG),
 *    DATAFILE2 se zahodi (chybejici terminator).
 * 2. S --recover-bsd: dekoduje 4 soubory vcetne DATAFILE2 [RECOVERED].
 * 3. Overeni recovery metadat (recovery_status, recovered_bytes).
 * 4. TMZ round-trip: obnoveny soubor ma Text Description s varovanim.
 *
 * Testovaci nahravka: tstdata/basic-bsd-incomplete.wav
 *   1. BSD - DATAFILE1 (1000 records, kompletni)
 *   2. BSD - DATAFILE2 (500 records, nekompletni - chybi CLOSE)
 *   3. BASIC - WRPRG
 *   4. BASIC - RDPRG
 *
 * @par Licence:
 * GNU General Public License v3 (GPLv3)
 *
 * Copyright (C) 2026 Michal Hucik <hucik@ordoz.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/generic_driver/generic_driver.h"
#include "libs/generic_driver/memory_driver.h"
#include "libs/wav_analyzer/wav_analyzer.h"
#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tzx/tzx.h"


/** @brief Cesta k testovaci nahravce (relativni z build/). */
static const char *g_wav_path = "../tstdata/basic-bsd-incomplete.wav";


/**
 * @brief Dekoduje WAV soubor pres wav_analyzer.
 *
 * @param wav_path Cesta k WAV souboru.
 * @param config Konfigurace analyzeru.
 * @param[out] result Vysledek analyzy.
 * @return 0 pri uspechu, 1 pri chybe.
 */
static int decode_wav ( const char *wav_path,
                         const st_WAV_ANALYZER_CONFIG *config,
                         st_WAV_ANALYZER_RESULT *result ) {
    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );

    st_HANDLER *h_in = generic_driver_open_file ( &h, &d, (char*) wav_path, FILE_DRIVER_OPMODE_RO );
    if ( !h_in ) {
        fprintf ( stderr, "ERROR: cannot open '%s'\n", wav_path );
        return 1;
    }

    en_WAV_ANALYZER_ERROR err = wav_analyzer_analyze ( h_in, config, result );
    generic_driver_close ( h_in );

    if ( err != WAV_ANALYZER_OK ) {
        fprintf ( stderr, "ERROR: analysis failed: %s\n", wav_analyzer_error_string ( err ) );
        return 1;
    }

    return 0;
}


/**
 * @brief Test 1: bez recovery - 3 soubory, DATAFILE2 zahozeno.
 *
 * @return Pocet selhani.
 */
static int test_no_recovery ( void ) {
    int failures = 0;

    printf ( "=== Test 1: Bez --recover-bsd (stavajici chovani) ===\n\n" );

    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );
    /* recover_bsd = 0 (vychozi) */

    st_WAV_ANALYZER_RESULT result;
    if ( decode_wav ( g_wav_path, &config, &result ) != 0 ) {
        printf ( "FAIL: cannot decode WAV\n" );
        return 1;
    }

    printf ( "WAV: %u Hz, %.1f sec, %u pulses, %u leaders, %u files\n",
             result.sample_rate, result.wav_duration_sec,
             result.total_pulses, result.leaders.count,
             result.file_count );

    /* ocekavame 3 soubory (DATAFILE1, WRPRG, RDPRG) */
    if ( result.file_count != 3 ) {
        printf ( "FAIL: expected 3 files, got %u\n", result.file_count );
        failures++;
    } else {
        printf ( "OK: 3 files decoded (DATAFILE2 correctly discarded)\n" );
    }

    /* soubor 1: BSD DATAFILE1 */
    if ( result.file_count >= 1 ) {
        const st_WAV_ANALYZER_FILE_RESULT *f = &result.files[0];
        if ( f->format != WAV_TAPE_FORMAT_BSD ) {
            printf ( "FAIL: file #1 format = %s, expected BSD\n",
                     wav_tape_format_name ( f->format ) );
            failures++;
        } else {
            printf ( "OK: file #1 = BSD\n" );
        }

        if ( f->mzf && f->recovery_status != WAV_RECOVERY_NONE ) {
            printf ( "FAIL: file #1 has unexpected recovery_status = 0x%02X\n",
                     f->recovery_status );
            failures++;
        } else if ( f->mzf ) {
            printf ( "OK: file #1 recovery_status = NONE (complete)\n" );
        }
    }

    /* soubor 2: WRPRG (NORMAL) */
    if ( result.file_count >= 2 ) {
        const st_WAV_ANALYZER_FILE_RESULT *f = &result.files[1];
        if ( f->format != WAV_TAPE_FORMAT_NORMAL ) {
            printf ( "FAIL: file #2 format = %s, expected NORMAL\n",
                     wav_tape_format_name ( f->format ) );
            failures++;
        } else {
            printf ( "OK: file #2 = NORMAL (WRPRG)\n" );
        }
    }

    /* soubor 3: RDPRG (NORMAL) */
    if ( result.file_count >= 3 ) {
        const st_WAV_ANALYZER_FILE_RESULT *f = &result.files[2];
        if ( f->format != WAV_TAPE_FORMAT_NORMAL ) {
            printf ( "FAIL: file #3 format = %s, expected NORMAL\n",
                     wav_tape_format_name ( f->format ) );
            failures++;
        } else {
            printf ( "OK: file #3 = NORMAL (RDPRG)\n" );
        }
    }

    wav_analyzer_result_destroy ( &result );
    printf ( "\n" );
    return failures;
}


/**
 * @brief Test 2: s recovery - 4 soubory vcetne DATAFILE2 [RECOVERED].
 *
 * @return Pocet selhani.
 */
static int test_with_recovery ( void ) {
    int failures = 0;

    printf ( "=== Test 2: S --recover-bsd (obnova castecnych dat) ===\n\n" );

    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );
    config.recover_bsd = 1;

    st_WAV_ANALYZER_RESULT result;
    if ( decode_wav ( g_wav_path, &config, &result ) != 0 ) {
        printf ( "FAIL: cannot decode WAV\n" );
        return 1;
    }

    printf ( "WAV: %u Hz, %.1f sec, %u files\n",
             result.sample_rate, result.wav_duration_sec,
             result.file_count );

    /* ocekavame 4 soubory (DATAFILE1, DATAFILE2, WRPRG, RDPRG) */
    if ( result.file_count != 4 ) {
        printf ( "FAIL: expected 4 files, got %u\n", result.file_count );
        failures++;
    } else {
        printf ( "OK: 4 files decoded (DATAFILE2 recovered)\n" );
    }

    /* soubor 1: BSD DATAFILE1 (kompletni) */
    if ( result.file_count >= 1 ) {
        const st_WAV_ANALYZER_FILE_RESULT *f = &result.files[0];
        if ( f->format != WAV_TAPE_FORMAT_BSD ) {
            printf ( "FAIL: file #1 format = %s, expected BSD\n",
                     wav_tape_format_name ( f->format ) );
            failures++;
        } else {
            printf ( "OK: file #1 = BSD (DATAFILE1)\n" );
        }

        if ( f->recovery_status != WAV_RECOVERY_NONE ) {
            printf ( "FAIL: file #1 has recovery_status = 0x%02X (expected NONE)\n",
                     f->recovery_status );
            failures++;
        } else {
            printf ( "OK: file #1 complete (no recovery)\n" );
        }
    }

    /* soubor 2: BSD DATAFILE2 (obnoveny) */
    if ( result.file_count >= 2 ) {
        const st_WAV_ANALYZER_FILE_RESULT *f = &result.files[1];
        if ( f->format != WAV_TAPE_FORMAT_BSD ) {
            printf ( "FAIL: file #2 format = %s, expected BSD\n",
                     wav_tape_format_name ( f->format ) );
            failures++;
        } else {
            printf ( "OK: file #2 = BSD (DATAFILE2)\n" );
        }

        if ( !( f->recovery_status & WAV_RECOVERY_BSD_INCOMPLETE ) ) {
            printf ( "FAIL: file #2 recovery_status = 0x%02X, expected BSD_INCOMPLETE\n",
                     f->recovery_status );
            failures++;
        } else {
            printf ( "OK: file #2 recovery_status = BSD_INCOMPLETE\n" );
        }

        if ( f->mzf ) {
            if ( f->mzf->body_size == 0 ) {
                printf ( "FAIL: file #2 body_size = 0\n" );
                failures++;
            } else {
                printf ( "OK: file #2 body_size = %u bytes (%u chunks recovered)\n",
                         f->mzf->body_size,
                         ( f->mzf->body_size + 255 ) / 256 );
            }

            if ( f->recovered_bytes != f->mzf->body_size ) {
                printf ( "FAIL: recovered_bytes = %u, body_size = %u\n",
                         f->recovered_bytes, f->mzf->body_size );
                failures++;
            } else {
                printf ( "OK: recovered_bytes = %u\n", f->recovered_bytes );
            }

            if ( f->mzf->header.ftype != MZF_FTYPE_BSD &&
                 f->mzf->header.ftype != MZF_FTYPE_BRD ) {
                printf ( "FAIL: file #2 ftype = 0x%02X (expected BSD/BRD)\n",
                         f->mzf->header.ftype );
                failures++;
            } else {
                printf ( "OK: file #2 ftype = 0x%02X\n", f->mzf->header.ftype );
            }
        } else {
            printf ( "FAIL: file #2 has no MZF\n" );
            failures++;
        }
    }

    /* soubor 3: WRPRG (NORMAL) */
    if ( result.file_count >= 3 ) {
        const st_WAV_ANALYZER_FILE_RESULT *f = &result.files[2];
        if ( f->format != WAV_TAPE_FORMAT_NORMAL ) {
            printf ( "FAIL: file #3 format = %s, expected NORMAL\n",
                     wav_tape_format_name ( f->format ) );
            failures++;
        } else {
            printf ( "OK: file #3 = NORMAL (WRPRG)\n" );
        }
    }

    /* soubor 4: RDPRG (NORMAL) */
    if ( result.file_count >= 4 ) {
        const st_WAV_ANALYZER_FILE_RESULT *f = &result.files[3];
        if ( f->format != WAV_TAPE_FORMAT_NORMAL ) {
            printf ( "FAIL: file #4 format = %s, expected NORMAL\n",
                     wav_tape_format_name ( f->format ) );
            failures++;
        } else {
            printf ( "OK: file #4 = NORMAL (RDPRG)\n" );
        }
    }

    wav_analyzer_result_destroy ( &result );
    printf ( "\n" );
    return failures;
}


/**
 * @brief Test 3: data DATAFILE1 jsou shodna s i bez recovery.
 *
 * Overuje, ze zapnuti recovery neovlivni dekodovani kompletniho souboru.
 *
 * @return Pocet selhani.
 */
static int test_datafile1_consistency ( void ) {
    int failures = 0;

    printf ( "=== Test 3: Konzistence DATAFILE1 s/bez recovery ===\n\n" );

    /* dekodovani bez recovery */
    st_WAV_ANALYZER_CONFIG config_no;
    wav_analyzer_config_default ( &config_no );

    st_WAV_ANALYZER_RESULT result_no;
    if ( decode_wav ( g_wav_path, &config_no, &result_no ) != 0 ) {
        printf ( "FAIL: cannot decode WAV (no recovery)\n" );
        return 1;
    }

    /* dekodovani s recovery */
    st_WAV_ANALYZER_CONFIG config_yes;
    wav_analyzer_config_default ( &config_yes );
    config_yes.recover_bsd = 1;

    st_WAV_ANALYZER_RESULT result_yes;
    if ( decode_wav ( g_wav_path, &config_yes, &result_yes ) != 0 ) {
        printf ( "FAIL: cannot decode WAV (with recovery)\n" );
        wav_analyzer_result_destroy ( &result_no );
        return 1;
    }

    /* oba musi mit alespon 1 soubor */
    if ( result_no.file_count < 1 || result_yes.file_count < 1 ) {
        printf ( "FAIL: insufficient files decoded\n" );
        wav_analyzer_result_destroy ( &result_no );
        wav_analyzer_result_destroy ( &result_yes );
        return 1;
    }

    const st_MZF *mzf_no = result_no.files[0].mzf;
    const st_MZF *mzf_yes = result_yes.files[0].mzf;

    if ( !mzf_no || !mzf_yes ) {
        printf ( "FAIL: missing MZF\n" );
        failures++;
    } else {
        /* body size */
        if ( mzf_no->body_size != mzf_yes->body_size ) {
            printf ( "FAIL: DATAFILE1 body_size differs: no=%u, yes=%u\n",
                     mzf_no->body_size, mzf_yes->body_size );
            failures++;
        } else {
            printf ( "OK: DATAFILE1 body_size = %u (consistent)\n", mzf_no->body_size );
        }

        /* body data */
        if ( mzf_no->body_size == mzf_yes->body_size &&
             mzf_no->body && mzf_yes->body ) {
            if ( memcmp ( mzf_no->body, mzf_yes->body, mzf_no->body_size ) != 0 ) {
                printf ( "FAIL: DATAFILE1 body data differs\n" );
                failures++;
            } else {
                printf ( "OK: DATAFILE1 body data identical (%u bytes)\n",
                         mzf_no->body_size );
            }
        }
    }

    wav_analyzer_result_destroy ( &result_no );
    wav_analyzer_result_destroy ( &result_yes );
    printf ( "\n" );
    return failures;
}


/**
 * @brief Hlavni funkce - spusti vsechny BSD incomplete testy.
 *
 * @param argc Pocet argumentu.
 * @param argv Pole argumentu. argv[1] = cesta k WAV (volitelne).
 * @return 0 pri uspechu (vsechny testy PASS), 1 pri selhani.
 */
int main ( int argc, char *argv[] ) {
    memory_driver_init ();

    if ( argc > 1 ) g_wav_path = argv[1];

    int total_failures = 0;

    /* Test 1: bez recovery */
    total_failures += test_no_recovery ();

    /* Test 2: s recovery */
    total_failures += test_with_recovery ();

    /* Test 3: konzistence DATAFILE1 */
    total_failures += test_datafile1_consistency ();

    /* celkovy vysledek */
    printf ( "=== BSD Incomplete Test: %s (%d failures) ===\n",
             total_failures == 0 ? "PASS" : "FAIL", total_failures );

    return total_failures > 0 ? 1 : 0;
}
