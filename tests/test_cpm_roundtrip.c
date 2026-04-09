/**
 * @file   test_cpm_roundtrip.c
 * @brief  Round-trip test: MZF -> CPM WAV -> wav_analyzer -> MZF
 *
 * Testovaci program:
 * 1. Vytvori testovaci MZF v pameti (telo obsahuje vsechny hodnoty 0x00-0xFF)
 * 2. Zakoduje ho jako CPM signal pres mztape (standardni FM, 2:1 rychlost,
 *    cmt.com pulzni sada) - identicky postup jako Intercopy V10.2
 * 3. Ulozi do WAV souboru
 * 4. Nacte WAV zpet a dekoduje pres wav_analyzer (FM dekoder)
 * 5. Porovna originalni a dekodovana data
 *
 * CPM format dle Intercopy V10.2:
 * - Standardni FM kodovani (8 bitu MSB first + 1 stop bit = 9 pulzu/bajt)
 * - 2x rychlost (CMTSPEED_2_1_CPM, divisor 2.0)
 * - CPM-specificke pulzni konstanty z programu cmt.com
 * - Stejna ramcova struktura jako NORMAL (GAP, tapemark, data, CRC)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/mztape/mztape.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/cmtspeed/cmtspeed.h"
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

    const char *wav_path = "temp/cpm_test.wav";
    const uint32_t rate = 44100;
    int failures = 0;

    if ( argc > 1 ) wav_path = argv[1];

    /* === 1. Priprava testovacich dat === */

    st_MZF_HEADER header;
    memset ( &header, 0, sizeof ( header ) );
    /* CPM-CMT format: ftype=$22 (identifikace z CMT.COM v.7, $0773: LD (HL),$22) */
    header.ftype = 0x22;
    memcpy ( header.fname.name, "TEST CPM\r", 9 );
    header.fsize = 256;
    header.fstrt = 0x1200;
    header.fexec = 0x1200;

    /* telo: vsechny hodnoty 0x00-0xFF */
    uint8_t body[256];
    for ( int i = 0; i < 256; i++ ) body[i] = ( uint8_t ) i;

    printf ( "Original MZF: ftype=0x%02X fsize=%u fstrt=0x%04X fexec=0x%04X\n",
             header.ftype, header.fsize, header.fstrt, header.fexec );

    /* === 2. CPM kodovani pres mztape (standardni FM, 2:1 rychlost) === */

    st_MZTAPE_MZF mztmzf;
    memset ( &mztmzf, 0, sizeof ( mztmzf ) );
    memcpy ( mztmzf.header, &header, sizeof ( st_MZF_HEADER ) );
    mztmzf.body = body;
    mztmzf.size = header.fsize;

    /* spocitame checksums */
    uint16_t chkh = 0, chkb = 0;
    for ( int i = 0; i < ( int ) sizeof ( st_MZF_HEADER ); i++ ) {
        uint8_t b = ( ( uint8_t* ) &header )[i];
        for ( int bit = 0; bit < 8; bit++ ) {
            if ( b & 1 ) chkh++;
            b >>= 1;
        }
    }
    for ( int i = 0; i < 256; i++ ) {
        uint8_t b = body[i];
        for ( int bit = 0; bit < 8; bit++ ) {
            if ( b & 1 ) chkb++;
            b >>= 1;
        }
    }
    mztmzf.chkh = chkh;
    mztmzf.chkb = chkb;

    st_CMT_STREAM *stream = mztape_create_stream_from_mztapemzf (
                                &mztmzf, CMTSPEED_2_1_CPM,
                                CMT_STREAM_TYPE_VSTREAM,
                                MZTAPE_FORMATSET_MZ800_SANE, rate
                            );
    if ( !stream ) {
        fprintf ( stderr, "ERROR: mztape_create_stream failed\n" );
        return 1;
    }

    printf ( "CPM stream created: %.3f sec\n", cmt_stream_get_length ( stream ) );

    /* === 3. Ulozeni do WAV === */

    int result = cmt_stream_save_wav ( stream, rate, ( char* ) wav_path );
    cmt_stream_destroy ( stream );

    if ( result != EXIT_SUCCESS ) {
        fprintf ( stderr, "ERROR: cmt_stream_save_wav failed\n" );
        return 1;
    }

    printf ( "WAV written: %s\n", wav_path );

    /* === 4. Dekodovani WAV zpet === */

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
    /*
     * CPM pulzy (cmt.com sada) maji asymetricky HIGH/LOW:
     * SHORT HIGH = 152.4 us, SHORT LOW = 131.5 us (po deleni 2:1)
     * Relativni variace ~15% -> vychozi tolerance 10% nestaci.
     */
    /* vychozi tolerance 0.20 staci pro CPM asymetricke pulzy (~15% variace) */

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
    if ( f->format != WAV_TAPE_FORMAT_CPM_CMT ) {
        printf ( "FAIL: Expected format CPM-CMT, got %s\n", wav_tape_format_name ( f->format ) );
        failures++;
    } else {
        printf ( "OK: Format = CPM-CMT\n" );
    }

    /* kontrola CRC */
    if ( f->header_crc != WAV_CRC_OK ) {
        printf ( "FAIL: Header CRC error\n" );
        failures++;
    } else {
        printf ( "OK: Header CRC\n" );
    }

    if ( f->body_crc != WAV_CRC_OK ) {
        printf ( "FAIL: Body CRC error\n" );
        failures++;
    } else {
        printf ( "OK: Body CRC\n" );
    }

    if ( !f->mzf ) {
        printf ( "FAIL: No MZF decoded\n" );
        wav_analyzer_result_destroy ( &analyzer_result );
        return 1;
    }

    /* kontrola hlavicky */
    if ( f->mzf->header.ftype != header.ftype ) {
        printf ( "FAIL: ftype mismatch: expected 0x%02X, got 0x%02X\n",
                 header.ftype, f->mzf->header.ftype );
        failures++;
    }

    if ( f->mzf->header.fsize != header.fsize ) {
        printf ( "FAIL: fsize mismatch: expected %u, got %u\n",
                 header.fsize, f->mzf->header.fsize );
        failures++;
    }

    if ( f->mzf->header.fstrt != header.fstrt ) {
        printf ( "FAIL: fstrt mismatch: expected 0x%04X, got 0x%04X\n",
                 header.fstrt, f->mzf->header.fstrt );
        failures++;
    }

    if ( f->mzf->header.fexec != header.fexec ) {
        printf ( "FAIL: fexec mismatch: expected 0x%04X, got 0x%04X\n",
                 header.fexec, f->mzf->header.fexec );
        failures++;
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
    printf ( "\n=== CPM Round-Trip Test: %s ===\n", failures == 0 ? "PASS" : "FAIL" );

    wav_analyzer_result_destroy ( &analyzer_result );
    return failures == 0 ? 0 : 1;
}
