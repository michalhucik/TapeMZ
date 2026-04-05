/**
 * @file   test_turbo_roundtrip.c
 * @brief  Round-trip test: MZF -> TURBO WAV -> wav2tmz -> MZF
 *
 * Testovaci program:
 * 1. Vytvori testovaci MZF v pameti
 * 2. Zakoduje ho jako TURBO signal (mzcmt_turbo_create_tape_stream)
 * 3. Ulozi do WAV souboru pres cmt_stream_save_wav
 * 4. Vypise pokyny ke spusteni wav2tmz
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/mzcmt_turbo/mzcmt_turbo.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/generic_driver/memory_driver.h"


int main ( int argc, char *argv[] ) {
    memory_driver_init ();
    const char *wav_path = "temp/turbo_test.wav";
    const uint32_t rate = 44100;

    if ( argc > 1 ) wav_path = argv[1];

    /* === 1. Priprava testovacich dat === */

    st_MZF_HEADER header;
    memset ( &header, 0, sizeof ( header ) );
    header.ftype = 0x01;
    memcpy ( header.fname.name, "TEST TURBO\r", 11 );
    header.fsize = 16;
    header.fstrt = 0x1200;
    header.fexec = 0x1200;

    uint8_t body[16];
    for ( int i = 0; i < 16; i++ ) body[i] = ( uint8_t ) i;

    printf ( "Original MZF: ftype=0x%02X fsize=%u fstrt=0x%04X fexec=0x%04X\n",
             header.ftype, header.fsize, header.fstrt, header.fexec );
    printf ( "Body: " );
    for ( int i = 0; i < 16; i++ ) printf ( "%02X ", body[i] );
    printf ( "\n" );

    /* === 2. TURBO kodovani (kompletni tape signal: loader + data) === */

    st_MZCMT_TURBO_CONFIG config = {
        .pulseset = MZCMT_TURBO_PULSESET_800,
        .speed = CMTSPEED_1_1,
        .lgap_length = 0,
        .sgap_length = 0,
        .long_high_us100 = 0,
        .long_low_us100 = 0,
        .short_high_us100 = 0,
        .short_low_us100 = 0,
        .flags = 0,
    };

    st_CMT_STREAM *stream = mzcmt_turbo_create_tape_stream (
                                &header, body, header.fsize, &config,
                                CMT_STREAM_TYPE_VSTREAM, rate
                            );
    if ( !stream ) {
        fprintf ( stderr, "ERROR: mzcmt_turbo_create_tape_stream failed\n" );
        return 1;
    }

    printf ( "TURBO stream created: %.3f sec\n", cmt_stream_get_length ( stream ) );

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
