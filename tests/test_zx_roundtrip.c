/**
 * @file   test_zx_roundtrip.c
 * @brief  Round-trip test: ZX Spectrum TAP -> WAV -> wav_analyzer -> TAP data
 *
 * Testovaci program:
 * 1. Sestavi TAP header blok (flag=0x00, type=3 "CODE", name="TEST      ",
 *    datablock_length=256, param1=$8000, param2=$8000, checksum XOR)
 * 2. Sestavi TAP data blok (flag=0xFF, 256B testovacich dat 0x00-0xFF, checksum)
 * 3. Pro kazdy blok: zxtape_create_stream_from_tapblock() -> cmt_stream_save_wav()
 * 4. Pro kazdy WAV: wav_analyzer_analyze() -> overeni formatu, flag, size, CRC, dat
 * 5. Cleanup
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/zxtape/zxtape.h"
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
static int compare_data ( const char *name, const uint8_t *expected,
                          const uint8_t *actual, uint32_t size ) {
    for ( uint32_t i = 0; i < size; i++ ) {
        if ( expected[i] != actual[i] ) {
            printf ( "FAIL: %s mismatch at offset %u: expected 0x%02X, got 0x%02X\n",
                     name, i, expected[i], actual[i] );
            return 1;
        }
    }
    return 0;
}


/**
 * @brief Vypocita XOR checksum TAP bloku.
 *
 * @param data Data bloku (flag + payload).
 * @param size Velikost dat v bajtech.
 * @return XOR checksum.
 */
static uint8_t compute_xor_checksum ( const uint8_t *data, uint32_t size ) {
    uint8_t xor = 0;
    for ( uint32_t i = 0; i < size; i++ ) {
        xor ^= data[i];
    }
    return xor;
}


/**
 * @brief Testuje round-trip jednoho ZX bloku.
 *
 * Zakoduje TAP blok do WAV pres zxtape, dekoduje zpet pres wav_analyzer
 * a porovna vysledek s originalem.
 *
 * @param block_name Nazev bloku pro diagnostiku.
 * @param flag Flag bajt (0x00 header, 0xFF data).
 * @param tap_data Kompletni TAP data (flag + payload + checksum).
 * @param tap_size Velikost TAP dat.
 * @param wav_path Cesta k docasnemu WAV souboru.
 * @param rate Vzorkovaci frekvence.
 * @return Pocet chyb (0 = ok).
 */
static int test_zx_block ( const char *block_name,
                           en_ZXTAPE_BLOCK_FLAG flag,
                           uint8_t *tap_data, uint16_t tap_size,
                           const char *wav_path, uint32_t rate ) {
    int failures = 0;

    printf ( "\n--- %s ---\n", block_name );
    printf ( "TAP data: flag=0x%02X, size=%u bytes\n", tap_data[0], tap_size );

    /* === 1. Zakodovani do WAV === */
    st_CMT_STREAM *stream = zxtape_create_stream_from_tapblock (
                                flag, tap_data, tap_size,
                                CMTSPEED_1_1, rate, CMT_STREAM_TYPE_VSTREAM );
    if ( !stream ) {
        printf ( "FAIL: zxtape_create_stream_from_tapblock failed\n" );
        return 1;
    }

    printf ( "ZX stream created: %.3f sec\n", cmt_stream_get_length ( stream ) );

    int result = cmt_stream_save_wav ( stream, rate, ( char* ) wav_path );
    cmt_stream_destroy ( stream );

    if ( result != EXIT_SUCCESS ) {
        printf ( "FAIL: cmt_stream_save_wav failed\n" );
        return 1;
    }

    printf ( "WAV written: %s\n", wav_path );

    /* === 2. Dekodovani z WAV === */
    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *h_in = generic_driver_open_file ( &h, &d, ( char* ) wav_path, FILE_DRIVER_OPMODE_RO );
    if ( !h_in ) {
        printf ( "FAIL: Can't open WAV file for reading\n" );
        return 1;
    }

    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );
    config.verbose = 1;

    st_WAV_ANALYZER_RESULT analyzer_result;
    en_WAV_ANALYZER_ERROR aerr = wav_analyzer_analyze ( h_in, &config, &analyzer_result );
    generic_driver_close ( h_in );

    if ( aerr != WAV_ANALYZER_OK ) {
        printf ( "FAIL: wav_analyzer_analyze failed: %s\n", wav_analyzer_error_string ( aerr ) );
        return 1;
    }

    wav_analyzer_print_summary ( &analyzer_result, stdout, 0 );

    /* === 3. Verifikace === */

    if ( analyzer_result.file_count < 1 ) {
        printf ( "FAIL: No files decoded\n" );
        wav_analyzer_result_destroy ( &analyzer_result );
        return 1;
    }

    const st_WAV_ANALYZER_FILE_RESULT *f = &analyzer_result.files[0];

    /* kontrola formatu */
    if ( f->format != WAV_TAPE_FORMAT_ZX_SPECTRUM ) {
        printf ( "FAIL: Expected format ZX SPECTRUM, got %s\n", wav_tape_format_name ( f->format ) );
        failures++;
    } else {
        printf ( "OK: Format = ZX SPECTRUM\n" );
    }

    /* kontrola flag bajtu */
    if ( !f->tap_data || f->tap_data_size == 0 ) {
        printf ( "FAIL: No TAP data decoded\n" );
        wav_analyzer_result_destroy ( &analyzer_result );
        return failures + 1;
    }

    if ( f->tap_data[0] != tap_data[0] ) {
        printf ( "FAIL: Flag mismatch: expected 0x%02X, got 0x%02X\n",
                 tap_data[0], f->tap_data[0] );
        failures++;
    } else {
        printf ( "OK: Flag = 0x%02X\n", f->tap_data[0] );
    }

    /* kontrola velikosti */
    if ( f->tap_data_size != tap_size ) {
        printf ( "FAIL: Size mismatch: expected %u, got %u\n",
                 tap_size, f->tap_data_size );
        failures++;
    } else {
        printf ( "OK: Size = %u bytes\n", f->tap_data_size );
    }

    /* kontrola CRC */
    if ( f->header_crc != WAV_CRC_OK ) {
        printf ( "FAIL: Checksum error\n" );
        failures++;
    } else {
        printf ( "OK: Checksum\n" );
    }

    /* bajtova shoda payloadu */
    if ( f->tap_data_size == tap_size ) {
        if ( compare_data ( "TAP payload", tap_data, f->tap_data, tap_size ) ) {
            failures++;
        } else {
            printf ( "OK: TAP data match (%u bytes)\n", tap_size );
        }
    }

    wav_analyzer_result_destroy ( &analyzer_result );
    return failures;
}


int main ( int argc, char *argv[] ) {
    memory_driver_init ();

    const uint32_t rate = 44100;
    int total_failures = 0;

    ( void ) argc;
    ( void ) argv;

    /* === 1. Sestaveni TAP header bloku === */
    /*
     * ZX Spectrum TAP header (19 bajtu):
     * [0]    flag = 0x00 (header)
     * [1]    type = 3 (CODE)
     * [2-11] name = "TEST      " (10 znaku, pad mezerami)
     * [12-13] datablock_length = 256 (LE)
     * [14-15] param1 = $8000 (LE) - start adresa
     * [16-17] param2 = $8000 (LE) - pro CODE = start adresa
     * [18]    checksum XOR
     */
    uint8_t hdr_tap[19];
    memset ( hdr_tap, 0, sizeof ( hdr_tap ) );
    hdr_tap[0] = 0x00;        /* flag: header */
    hdr_tap[1] = 3;           /* type: CODE */
    memcpy ( &hdr_tap[2], "TEST      ", 10 ); /* name */
    hdr_tap[12] = 0x00; hdr_tap[13] = 0x01; /* datablock_length = 256 LE */
    hdr_tap[14] = 0x00; hdr_tap[15] = 0x80; /* param1 = $8000 LE */
    hdr_tap[16] = 0x00; hdr_tap[17] = 0x80; /* param2 = $8000 LE */
    hdr_tap[18] = compute_xor_checksum ( hdr_tap, 18 ); /* checksum */

    /* === 2. Sestaveni TAP data bloku === */
    /*
     * ZX Spectrum TAP data blok:
     * [0]       flag = 0xFF (data)
     * [1-256]   256B testovacich dat (0x00-0xFF)
     * [257]     checksum XOR
     */
    uint8_t data_tap[258]; /* flag + 256B data + checksum */
    data_tap[0] = 0xFF; /* flag: data */
    for ( int i = 0; i < 256; i++ ) {
        data_tap[1 + i] = ( uint8_t ) i;
    }
    data_tap[257] = compute_xor_checksum ( data_tap, 257 );

    /* === 3. Testy === */

    total_failures += test_zx_block (
        "ZX Header Block", ZXTAPE_BLOCK_FLAG_HEADER,
        hdr_tap, sizeof ( hdr_tap ),
        "temp/zx_header_test.wav", rate
    );

    total_failures += test_zx_block (
        "ZX Data Block", ZXTAPE_BLOCK_FLAG_DATA,
        data_tap, sizeof ( data_tap ),
        "temp/zx_data_test.wav", rate
    );

    /* === 4. Celkovy vysledek === */
    printf ( "\n=== ZX Spectrum Round-Trip Test: %s ===\n",
             total_failures == 0 ? "PASS" : "FAIL" );

    /* uklidime docasne WAV soubory */
    remove ( "temp/zx_header_test.wav" );
    remove ( "temp/zx_data_test.wav" );

    return total_failures == 0 ? 0 : 1;
}
