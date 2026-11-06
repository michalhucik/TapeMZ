/**
 * @file   test_turbo_roundtrip.c
 * @brief  Round-trip TURBO: MZF -> mzcmt_turbo (profil TurboCopy) -> WAV -> wav_analyzer -> MZF.
 *
 * Pro rychlosti 2:1, 7:3, 8:3 a 3:1 při 44100 Hz zakóduje testovací MZF
 * (512 B: 00h-FFh a 256x 00h), uloží WAV, dekóduje ho a ověří, že mezi
 * dekódovanými soubory je MZF se shodným tělem a hlavičkou.
 *
 * Test ověřuje jen shodu kodéru a dekodéru TapeMZ. Čitelnost na cílovém
 * stroji ověřuje zavedení v emulátoru a test_turbo_readpoint
 * (model čtecího bodu ROM). TURBO 1:1 se netestuje: dekodér ho nedekóduje.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/mzcmt_turbo/mzcmt_turbo.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/cmtspeed/cmtspeed.h"
#include "libs/generic_driver/generic_driver.h"
#include "libs/generic_driver/memory_driver.h"
#include "libs/wav_analyzer/wav_analyzer.h"

/** @brief Vzorkovací frekvence testu (Hz). */
#define TEST_RATE   44100
/** @brief Velikost testovacího těla (B). */
#define BODY_SIZE   512


/**
 * @brief Zakóduje, uloží, dekóduje a porovná jednu rychlost.
 * @param speed Rychlost TURBO.
 * @param hdr Hlavička MZF.
 * @param body Tělo (BODY_SIZE B).
 * @return 0 při shodě, 1 při chybě.
 */
static int roundtrip ( en_CMTSPEED speed, const st_MZF_HEADER *hdr, const uint8_t *body ) {
    char wav_path[64];
    snprintf ( wav_path, sizeof ( wav_path ), "temp/turbo_rt_%u.wav", ( unsigned ) speed );

    st_MZCMT_TURBO_CONFIG cfg;
    memset ( &cfg, 0, sizeof ( cfg ) );
    cfg.pulseset = MZCMT_TURBO_PULSESET_800;
    cfg.speed = speed;

    st_CMT_STREAM *stream = mzcmt_turbo_create_tape_stream ( hdr, body, BODY_SIZE, &cfg,
                                                             CMT_STREAM_TYPE_VSTREAM, TEST_RATE );
    if ( !stream ) {
        printf ( "FAIL %s: encoder returned NULL\n", cmtspeed_get_ratiotxt ( speed ) );
        return 1;
    }
    int rc = cmt_stream_save_wav ( stream, TEST_RATE, wav_path );
    cmt_stream_destroy ( stream );
    if ( rc != EXIT_SUCCESS ) {
        printf ( "FAIL %s: cannot save %s\n", cmtspeed_get_ratiotxt ( speed ), wav_path );
        return 1;
    }

    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *hin = generic_driver_open_file ( &h, &d, wav_path, FILE_DRIVER_OPMODE_RO );
    if ( !hin ) {
        printf ( "FAIL %s: cannot open %s\n", cmtspeed_get_ratiotxt ( speed ), wav_path );
        return 1;
    }
    st_WAV_ANALYZER_CONFIG acfg;
    wav_analyzer_config_default ( &acfg );
    st_WAV_ANALYZER_RESULT res;
    memset ( &res, 0, sizeof ( res ) );
    en_WAV_ANALYZER_ERROR aerr = wav_analyzer_analyze ( hin, &acfg, &res );
    generic_driver_close ( hin );
    if ( aerr != WAV_ANALYZER_OK ) {
        printf ( "FAIL %s: analyzer error %s\n", cmtspeed_get_ratiotxt ( speed ), wav_analyzer_error_string ( aerr ) );
        return 1;
    }

    int found = 0;
    for ( uint32_t i = 0; i < res.file_count; i++ ) {
        const st_WAV_ANALYZER_FILE_RESULT *f = &res.files[i];
        if ( !f->mzf || f->mzf->body_size != BODY_SIZE || !f->mzf->body ) continue;
        if ( memcmp ( f->mzf->body, body, BODY_SIZE ) != 0 ) continue;
        if ( f->mzf->header.fstrt != hdr->fstrt || f->mzf->header.fexec != hdr->fexec ) continue;
        printf ( "OK %s: file %u (%s) body and addresses match\n", cmtspeed_get_ratiotxt ( speed ), i,
                 wav_tape_format_name ( f->format ) );
        found = 1;
        break;
    }
    if ( !found ) {
        printf ( "FAIL %s: no decoded file matches (%u files)\n", cmtspeed_get_ratiotxt ( speed ), res.file_count );
    }
    wav_analyzer_result_destroy ( &res );
    return found ? 0 : 1;
}


int main ( void ) {
    memory_driver_init ();

    st_MZF_HEADER hdr;
    memset ( &hdr, 0, sizeof ( hdr ) );
    hdr.ftype = 0x01;
    memcpy ( hdr.fname.name, "TURBO RT", 8 );
    hdr.fname.name[8] = 0x0D;
    hdr.fsize = BODY_SIZE;
    hdr.fstrt = 0x1200;
    hdr.fexec = 0x1200;

    uint8_t body[BODY_SIZE];
    for ( int i = 0; i < BODY_SIZE; i++ ) body[i] = ( i < 256 ) ? ( uint8_t ) i : 0x00;

    static const en_CMTSPEED speeds[] = { CMTSPEED_2_1, CMTSPEED_7_3, CMTSPEED_8_3, CMTSPEED_3_1 };
    int failures = 0;
    for ( size_t s = 0; s < sizeof ( speeds ) / sizeof ( speeds[0] ); s++ ) {
        failures += roundtrip ( speeds[s], &hdr, body );
    }

    printf ( "\n=== TURBO Round-Trip Test: %s ===\n", failures ? "FAIL" : "PASS" );
    return failures ? 1 : 0;
}
