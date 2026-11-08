/**
 * @file   test_fm_copy2.c
 * @brief  FM dekodér použije druhou kopii záznamu ROM.
 *
 * Sestaví sekvenci půlvln podle zápisu ROM MZ-800 (kód ROM,
 * shodně ROM-800-NOMINAL v tmz_profile.c): tón, tapemark, 1 L, data + checksum
 * (každý bajt start bit L + 8 bitů MSB), 1 L, 256 S, kopie dat + checksum, 1 L.
 * Kopie nemá tón ani tapemark. Pak poškodí první kopii a čeká, že dekodér
 * vrátí data z druhé kopie s CRC OK. Data nepotřebuje.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/wav_analyzer/wav_analyzer_types.h"
#include "libs/wav_analyzer/wav_leader.h"
#include "libs/wav_analyzer/wav_decode_fm.h"

#include "fm_test_seq.h"

/**
 * @brief Dekóduje sekvenci a ověří CRC OK, použití kopií, data těla a konec těla.
 * @param name Název případu. @param s Sekvence. @param body Očekávané tělo.
 * @param exp_hdr_c2 Čeká se použití 2. kopie hlavičky. @param exp_body_c2 Totéž pro tělo.
 * @param exp_body_end Očekávaný pulse_end těla (0 = nekontrolovat).
 * @return Počet selhání.
 */
static int run_case ( const char *name, st_WAV_PULSE_SEQUENCE *s, const uint8_t *body,
                      int exp_hdr_c2, int exp_body_c2, uint32_t exp_body_end ) {
    st_WAV_LEADER_INFO leader;
    if ( wav_leader_detect ( s, 0, 500, WAV_ANALYZER_DEFAULT_TOLERANCE, &leader ) != WAV_ANALYZER_OK ) {
        printf ( "FAIL [%s]: leader\n", name ); return 1;
    }
    st_MZF *mzf = NULL;
    st_WAV_DECODE_RESULT hr, br;
    memset ( &hr, 0, sizeof ( hr ) ); memset ( &br, 0, sizeof ( br ) );
    en_WAV_ANALYZER_ERROR err = wav_decode_fm_decode_mzf ( s, &leader, &mzf, &hr, &br );
    int f = 0;
    if ( err != WAV_ANALYZER_OK || !mzf ) { printf ( "FAIL [%s]: decode err=%d\n", name, err ); f++; goto out; }
    if ( hr.crc_status != WAV_CRC_OK ) { printf ( "FAIL [%s]: header CRC %d\n", name, hr.crc_status ); f++; }
    if ( br.crc_status != WAV_CRC_OK ) { printf ( "FAIL [%s]: body CRC %d\n", name, br.crc_status ); f++; }
    if ( hr.copy2_used != exp_hdr_c2 ) { printf ( "FAIL [%s]: header copy2_used=%d\n", name, hr.copy2_used ); f++; }
    if ( br.copy2_used != exp_body_c2 ) { printf ( "FAIL [%s]: body copy2_used=%d\n", name, br.copy2_used ); f++; }
    if ( mzf->body_size != BODY_SIZE || !mzf->body || memcmp ( mzf->body, body, BODY_SIZE ) ) {
        printf ( "FAIL [%s]: body data differ\n", name ); f++;
    }
    if ( exp_body_end && br.pulse_end != exp_body_end ) {
        printf ( "FAIL [%s]: body pulse_end=%u, expected %u (end of copy 2)\n", name, br.pulse_end, exp_body_end ); f++;
    }
    if ( !f ) printf ( "OK [%s]\n", name );
out:
    mzf_free ( mzf ); free ( hr.data ); free ( br.data );
    return f;
}

int main ( void ) {
    st_WAV_PULSE_SEQUENCE s;
    memset ( &s, 0, sizeof ( s ) );
    s.capacity = 120000;
    s.sample_rate = TEST_RATE;
    s.pulses = calloc ( s.capacity, sizeof ( st_WAV_PULSE ) );
    if ( !s.pulses ) return 1;

    uint8_t hdr[128], body[BODY_SIZE];
    memset ( hdr, 0, sizeof ( hdr ) );
    hdr[0] = 0x01; memcpy ( &hdr[1], "COPY2\r", 6 );
    hdr[0x12] = BODY_SIZE & 0xFF; hdr[0x13] = BODY_SIZE >> 8;
    hdr[0x15] = 0x12; hdr[0x17] = 0x12;
    for ( int i = 0; i < BODY_SIZE; i++ ) body[i] = ( uint8_t ) ( i * 37 + 11 );

    st_POS p;
    int fails = 0;

    /* 1. čistý záznam s kopiemi: tělo z 1. kopie, pulse_end za 2. kopií (orchestrátor ji přeskočí) */
    if ( build ( &s, hdr, body, 1, &p ) ) return 1;
    fails += run_case ( "clean, consumed past copy 2", &s, body, 0, 0, p.body2_end );

    /* 2. bitová chyba v 1. kopii těla */
    build ( &s, hdr, body, 1, &p );
    flip_bit ( &s, p.body1 + 18 * 500 );
    fails += run_case ( "body copy 1 bit error", &s, body, 0, 1, p.body2_end );

    /* 3. bitová chyba v 1. kopii hlavičky */
    build ( &s, hdr, body, 1, &p );
    flip_bit ( &s, p.hdr1 + 18 * 10 );
    fails += run_case ( "header copy 1 bit error", &s, body, 1, 0, p.body2_end );

    /* 4. výpadek 1000 půlvln v 1. kopii těla (dekodér přejede oddělovač) */
    build ( &s, hdr, body, 1, &p );
    dropout ( &s, p.body1 + 18 * 900, 1000 );
    fails += run_case ( "body copy 1 dropout", &s, body, 0, 1, p.body2_end - 999 );

    /* 5. záznam bez kopií (SANE): beze změny chování */
    build ( &s, hdr, body, 0, &p );
    fails += run_case ( "no copies", &s, body, 0, 0, 0 );

    /* 6. obě kopie těla poškozené: CRC ERROR, 2. kopie se nepoužije */
    build ( &s, hdr, body, 1, &p );
    flip_bit ( &s, p.body1 + 18 * 500 );
    flip_bit ( &s, p.body2 + 18 * 700 );
    {
        st_WAV_LEADER_INFO leader;
        st_MZF *mzf = NULL;
        st_WAV_DECODE_RESULT hr, br;
        memset ( &hr, 0, sizeof ( hr ) );
        memset ( &br, 0, sizeof ( br ) );
        int ok = wav_leader_detect ( &s, 0, 500, WAV_ANALYZER_DEFAULT_TOLERANCE, &leader ) == WAV_ANALYZER_OK
                 && wav_decode_fm_decode_mzf ( &s, &leader, &mzf, &hr, &br ) == WAV_ANALYZER_OK && mzf
                 && br.crc_status == WAV_CRC_ERROR && !br.copy2_used;
        printf ( "%s [both body copies damaged -> CRC ERROR, copy 2 not used]\n", ok ? "OK" : "FAIL" );
        if ( !ok ) fails++;
        mzf_free ( mzf );
        free ( hr.data );
        free ( br.data );
    }

    free ( s.pulses );
    printf ( "\n=== FM Copy2 Test: %s ===\n", fails ? "FAIL" : "PASS" );
    return fails ? 1 : 0;
}
