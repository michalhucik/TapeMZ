/**
 * @file   extract_preloader.c
 * @brief  Extraktor TurboCopy TURBO preloader binarky z WAV nahravky.
 *
 * Pouziva nizkourovenove API wav_analyzeru (preprocessing, pulse extraction,
 * leader detection, FM dekoder) k dekodovani NORMAL FM casti nahravky.
 * Hleda preloader s fstrt=$D400, fsize=90 a ulozi jeho 90B telo
 * do binarniho souboru.
 *
 * Pouziti:
 *   extract_preloader <input.wav> <output.bin>
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
#include "libs/wav/wav.h"
#include "libs/mzf/mzf.h"
#include "libs/wav_analyzer/wav_analyzer.h"
#include "libs/wav_analyzer/wav_preprocess.h"
#include "libs/wav_analyzer/wav_pulse.h"
#include "libs/wav_analyzer/wav_leader.h"
#include "libs/wav_analyzer/wav_decode_fm.h"


/** @brief Hledana startovni adresa preloaderu. */
#define PRELOADER_FSTRT     0xD400

/** @brief Ocekavana velikost preloader body (90 bajtu). */
#define PRELOADER_FSIZE     90


/**
 * @brief Vypise hex dump dat na stdout.
 *
 * @param data Ukazatel na data.
 * @param size Pocet bajtu k vypisu.
 * @param base_addr Bazova adresa pro levy sloupec.
 */
static void hex_dump ( const uint8_t *data, uint32_t size, uint16_t base_addr ) {
    for ( uint32_t i = 0; i < size; i += 16 ) {
        printf ( "%04X: ", base_addr + i );
        for ( uint32_t j = 0; j < 16; j++ ) {
            if ( i + j < size ) {
                printf ( "%02X ", data[i + j] );
            } else {
                printf ( "   " );
            }
            if ( j == 7 ) printf ( " " );
        }
        printf ( " |" );
        for ( uint32_t j = 0; j < 16 && ( i + j ) < size; j++ ) {
            uint8_t c = data[i + j];
            printf ( "%c", ( c >= 0x20 && c <= 0x7E ) ? c : '.' );
        }
        printf ( "|\n" );
    }
}


int main ( int argc, char *argv[] ) {
    if ( argc < 3 ) {
        fprintf ( stderr,
                  "Usage: %s <input.wav> <output.bin>\n"
                  "\n"
                  "Extracts the TurboCopy TURBO preloader binary (90 bytes)\n"
                  "from a WAV recording and saves it to a file.\n"
                  "\n"
                  "The preloader is identified by: fstrt=$D400, fsize=90.\n",
                  argv[0] );
        return 1;
    }

    const char *wav_path = argv[1];
    const char *out_path = argv[2];

    memory_driver_init ();

    /* === 1. Otevreni WAV souboru === */

    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *hin = generic_driver_open_file ( &h, &d, ( char* ) wav_path, FILE_DRIVER_OPMODE_RO );
    if ( !hin ) {
        fprintf ( stderr, "ERROR: cannot open WAV '%s'\n", wav_path );
        return 1;
    }

    /* === 2. Parsovani WAV hlavicky === */

    en_WAV_ERROR wav_err;
    st_WAV_SIMPLE_HEADER *sh = wav_simple_header_new_from_handler ( hin, &wav_err );
    if ( !sh ) {
        fprintf ( stderr, "ERROR: WAV parse failed: %s\n", wav_error_string ( wav_err ) );
        generic_driver_close ( hin );
        return 1;
    }

    uint32_t sample_rate = sh->sample_rate;

    printf ( "WAV: %u Hz, %u-bit, %u ch, %.1f sec\n",
             sh->sample_rate, sh->bits_per_sample, sh->channels, sh->count_sec );

    /* === 3. Preprocessing === */

    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );

    double *samples = NULL;
    uint32_t sample_count = 0;
    en_WAV_ANALYZER_ERROR err;

    err = wav_preprocess_run ( hin, sh, &config, &samples, &sample_count );
    generic_driver_close ( hin );
    wav_simple_header_destroy ( sh );

    if ( err != WAV_ANALYZER_OK ) {
        fprintf ( stderr, "ERROR: preprocessing failed: %s\n", wav_analyzer_error_string ( err ) );
        return 1;
    }

    printf ( "Preprocessed: %u samples\n", sample_count );

    /* === 4. Extrakce pulzu === */

    st_WAV_PULSE_SEQUENCE seq;
    memset ( &seq, 0, sizeof ( seq ) );

    err = wav_pulse_extract ( samples, sample_count, sample_rate, &config, &seq );

    free ( samples );

    if ( err != WAV_ANALYZER_OK ) {
        fprintf ( stderr, "ERROR: pulse extraction failed: %s\n", wav_analyzer_error_string ( err ) );
        return 1;
    }

    printf ( "Extracted: %u pulses\n", seq.count );

    /* === 5. Detekce leaderu === */

    st_WAV_LEADER_LIST leaders;
    memset ( &leaders, 0, sizeof ( leaders ) );

    err = wav_leader_find_all ( &seq, config.min_leader_pulses, config.tolerance, &leaders );
    if ( err != WAV_ANALYZER_OK ) {
        fprintf ( stderr, "ERROR: leader detection failed: %s\n", wav_analyzer_error_string ( err ) );
        wav_pulse_sequence_destroy ( &seq );
        return 1;
    }

    printf ( "Found: %u leaders\n\n", leaders.count );

    /* === 6. FM dekodovani kazdeho leaderu === */

    int found = 0;

    for ( uint32_t i = 0; i < leaders.count; i++ ) {
        st_WAV_LEADER_INFO *leader = &leaders.leaders[i];

        printf ( "Leader #%u: start=%u, pulses=%u, avg=%.1f us\n",
                 i + 1, leader->start_index, leader->pulse_count, leader->avg_period_us );

        /* zkusime FM dekodovani */
        st_MZF *mzf = NULL;
        st_WAV_DECODE_RESULT hdr_res, body_res;
        memset ( &hdr_res, 0, sizeof ( hdr_res ) );
        memset ( &body_res, 0, sizeof ( body_res ) );

        err = wav_decode_fm_decode_mzf ( &seq, leader, &mzf, &hdr_res, &body_res );

        if ( err != WAV_ANALYZER_OK || !mzf ) {
            printf ( "  FM decode failed (err=%d)\n", err );
            free ( hdr_res.data );
            free ( body_res.data );
            continue;
        }

        printf ( "  Decoded: ftype=0x%02X, fsize=%u, fstrt=0x%04X, fexec=0x%04X\n",
                 mzf->header.ftype, mzf->header.fsize,
                 mzf->header.fstrt, mzf->header.fexec );
        printf ( "  Header CRC: %s, Body CRC: %s\n",
                 hdr_res.crc_status == WAV_CRC_OK ? "OK" :
                 hdr_res.crc_status == WAV_CRC_ERROR ? "ERROR" : "N/A",
                 body_res.crc_status == WAV_CRC_OK ? "OK" :
                 body_res.crc_status == WAV_CRC_ERROR ? "ERROR" : "N/A" );

        /* kontrola, jestli je to preloader */
        if ( mzf->header.fstrt == PRELOADER_FSTRT &&
             mzf->header.fsize == PRELOADER_FSIZE &&
             mzf->body && mzf->body_size == PRELOADER_FSIZE ) {

            printf ( "\n*** FOUND TurboCopy TURBO preloader! ***\n" );
            printf ( "Body: %u bytes at $%04X\n\n", mzf->body_size, mzf->header.fstrt );

            /* hex dump */
            printf ( "=== Hex dump of preloader body ===\n" );
            hex_dump ( mzf->body, mzf->body_size, ( uint16_t ) mzf->header.fstrt );

            /* ulozeni do souboru */
            FILE *fout = fopen ( out_path, "wb" );
            if ( !fout ) {
                fprintf ( stderr, "ERROR: cannot create '%s'\n", out_path );
                mzf_free ( mzf );
                free ( hdr_res.data );
                free ( body_res.data );
                wav_leader_list_destroy ( &leaders );
                wav_pulse_sequence_destroy ( &seq );
                return 1;
            }

            size_t written = fwrite ( mzf->body, 1, mzf->body_size, fout );
            fclose ( fout );

            if ( written != mzf->body_size ) {
                fprintf ( stderr, "ERROR: write failed (%zu of %u bytes)\n",
                          written, mzf->body_size );
                mzf_free ( mzf );
                free ( hdr_res.data );
                free ( body_res.data );
                wav_leader_list_destroy ( &leaders );
                wav_pulse_sequence_destroy ( &seq );
                return 1;
            }

            printf ( "\nSaved %u bytes to '%s'\n", mzf->body_size, out_path );

            /* ulozime tez celou hlavicku do .hdr souboru */
            char hdr_path[512];
            snprintf ( hdr_path, sizeof ( hdr_path ), "%s.hdr", out_path );
            FILE *fhdr = fopen ( hdr_path, "wb" );
            if ( fhdr ) {
                /* zapiseme v surovem tvaru (LE, jak je na pasce) */
                st_MZF_HEADER raw_hdr;
                memcpy ( &raw_hdr, &mzf->header, sizeof ( st_MZF_HEADER ) );
                mzf_header_items_correction ( &raw_hdr ); /* host->LE */
                fwrite ( &raw_hdr, 1, sizeof ( st_MZF_HEADER ), fhdr );
                fclose ( fhdr );
                printf ( "Saved header (128 bytes) to '%s'\n", hdr_path );
            }

            found = 1;
            mzf_free ( mzf );
            free ( hdr_res.data );
            free ( body_res.data );
            break;
        }

        mzf_free ( mzf );
        free ( hdr_res.data );
        free ( body_res.data );
    }

    if ( !found ) {
        fprintf ( stderr, "\nERROR: TurboCopy preloader (fstrt=$D400, fsize=90) not found\n" );
    }

    wav_leader_list_destroy ( &leaders );
    wav_pulse_sequence_destroy ( &seq );

    return found ? 0 : 1;
}
