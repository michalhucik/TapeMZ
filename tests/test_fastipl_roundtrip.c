/**
 * @file   test_fastipl_roundtrip.c
 * @brief  Round-trip test: MZF -> FASTIPL WAV -> wav2tmz -> MZF
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/mzcmt_fastipl/mzcmt_fastipl.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/generic_driver/memory_driver.h"


int main ( int argc, char *argv[] ) {
    memory_driver_init ();

    const char *wav_path = "temp/fastipl_test.wav";
    const uint32_t rate = 44100;

    if ( argc > 1 ) wav_path = argv[1];

    /* === 1. Priprava testovacich dat === */

    st_MZF_HEADER header;
    memset ( &header, 0, sizeof ( header ) );
    header.ftype = 0x01;
    memcpy ( header.fname.name, "TEST FASTIPL\r", 13 );
    header.fsize = 32;
    header.fstrt = 0x1200;
    header.fexec = 0x1200;

    uint8_t body[32];
    for ( int i = 0; i < 32; i++ ) body[i] = ( uint8_t ) ( i * 2 );

    printf ( "Original MZF: ftype=0x%02X fsize=%u fstrt=0x%04X fexec=0x%04X\n",
             header.ftype, header.fsize, header.fstrt, header.fexec );

    /* === 2. FASTIPL kodovani === */

    st_MZCMT_FASTIPL_CONFIG config = {
        .version = MZCMT_FASTIPL_VERSION_V07,
        .pulseset = MZCMT_FASTIPL_PULSESET_800,
        .speed = CMTSPEED_1_1,
        .lgap_length = 0,
        .sgap_length = 0,
        .long_high_us100 = 0,
        .long_low_us100 = 0,
        .short_high_us100 = 0,
        .short_low_us100 = 0,
        .blcount = 0,
        .readpoint = 0,
        .pause_ms = 500,
    };

    st_CMT_STREAM *stream = mzcmt_fastipl_create_stream (
                                &header, body, header.fsize, &config,
                                CMT_STREAM_TYPE_VSTREAM, rate
                            );
    if ( !stream ) {
        fprintf ( stderr, "ERROR: mzcmt_fastipl_create_stream failed\n" );
        return 1;
    }

    printf ( "FASTIPL stream created: %.3f sec\n", cmt_stream_get_length ( stream ) );

    /* === 3. Ulozeni do WAV === */

    int result = cmt_stream_save_wav ( stream, rate, (char*) wav_path );
    cmt_stream_destroy ( stream );

    if ( result != EXIT_SUCCESS ) {
        fprintf ( stderr, "ERROR: cmt_stream_save_wav failed\n" );
        return 1;
    }

    printf ( "WAV written: %s\n", wav_path );
    return 0;
}
