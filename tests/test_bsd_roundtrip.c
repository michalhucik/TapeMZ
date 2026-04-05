/**
 * @file   test_bsd_roundtrip.c
 * @brief  Round-trip test: data -> BSD WAV -> wav2tmz -> MZF
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/mzcmt_bsd/mzcmt_bsd.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/endianity/endianity.h"
#include "libs/generic_driver/memory_driver.h"


int main ( int argc, char *argv[] ) {
    memory_driver_init ();

    const char *wav_path = "temp/bsd_test.wav";
    const uint32_t rate = 44100;

    if ( argc > 1 ) wav_path = argv[1];

    /* === 1. Priprava testovacich dat === */

    /* BSD hlavicka: ftype=0x04 (BRD), fsize=0, fstrt=0, fexec=0 */
    uint8_t header_raw[128];
    memset ( header_raw, 0, 128 );
    header_raw[0] = 0x04;  /* BRD */
    memcpy ( &header_raw[1], "TEST BSD\r", 9 );

    /* 2 chunky: chunk0 (ID=0x0000) + chunk1 (ID=0xFFFF, posledni) */
    uint8_t chunks[2 * 258];
    memset ( chunks, 0, sizeof ( chunks ) );

    /* chunk 0: ID=0x0000, data = 0x00..0xFF */
    chunks[0] = 0x00; chunks[1] = 0x00;  /* ID LE */
    for ( int i = 0; i < 256; i++ ) chunks[2 + i] = ( uint8_t ) i;

    /* chunk 1: ID=0xFFFF, data = 0xFF..0x00 */
    chunks[258] = 0xFF; chunks[259] = 0xFF;  /* ID LE */
    for ( int i = 0; i < 256; i++ ) chunks[260 + i] = ( uint8_t ) ( 255 - i );

    printf ( "BSD test: 2 chunks, 512 bytes total data\n" );

    /* === 2. BSD kodovani === */

    st_MZCMT_BSD_CONFIG config = {
        .pulseset = MZCMT_BSD_PULSESET_800,
        .speed = CMTSPEED_1_1,
        .lgap_length = 0,
        .sgap_length = 0,
        .flags = 0,
    };

    st_CMT_STREAM *stream = mzcmt_bsd_create_stream (
                                header_raw, chunks, 2, &config,
                                CMT_STREAM_TYPE_VSTREAM, rate
                            );
    if ( !stream ) {
        fprintf ( stderr, "ERROR: mzcmt_bsd_create_stream failed\n" );
        return 1;
    }

    printf ( "BSD stream created: %.3f sec\n", cmt_stream_get_length ( stream ) );

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
