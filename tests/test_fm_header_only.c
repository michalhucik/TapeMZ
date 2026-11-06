/**
 * @file   test_fm_header_only.c
 * @brief  Regresní test: FM dekodér na nahrávce, kde je jen hlavička s fsize=0.
 *
 * Sestaví v paměti sekvenci půlperiod (leader, dlouhý tapemark, 2L,
 * 128 B hlavičky, checksum, 2L) bez těla a bez krátkého tapemarku.
 * Dekodér musí vrátit MZF s body == NULL a body_size == 0.
 *
 * Kódování bajtu odpovídá tomu, co čte wav_decode_fm_read_byte():
 * 8 bitů MSB first + 1 LONG. Délky půlperiod jsou nominál MZ-800
 * z dokumentace HW MZ-800 (240/278, 470/494 us). Test neověřuje shodu s ROM,
 * jen cestu dekodéru "hlavička bez těla".
 *
 * Pozn.: před opravou je hodnota body_data neurčená, takže test může
 * náhodně projít i na chybném kódu. Spolehlivou pojistkou je
 * -Wjump-misses-init v CMakeLists.txt.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/wav_analyzer/wav_analyzer_types.h"
#include "libs/wav_analyzer/wav_leader.h"
#include "libs/wav_analyzer/wav_decode_fm.h"

/** @brief Vzorkovací frekvence uvedená v sekvenci (jen pro duration_samples). */
#define TEST_RATE        44100
/** @brief HIGH půlperioda krátkého pulzu (us). */
#define SHORT_HIGH_US    240.0
/** @brief LOW půlperioda krátkého pulzu (us). */
#define SHORT_LOW_US     278.0
/** @brief HIGH půlperioda dlouhého pulzu (us). */
#define LONG_HIGH_US     470.0
/** @brief LOW půlperioda dlouhého pulzu (us). */
#define LONG_LOW_US      494.0


/**
 * @brief Přidá jednu půlperiodu do sekvence.
 * @param seq Cílová sekvence (kapacita musí stačit).
 * @param us Délka půlperiody v mikrosekundách.
 * @param level Úroveň (1 = HIGH, 0 = LOW).
 * @return 0 při úspěchu, -1 při vyčerpání kapacity.
 */
static int add_half ( st_WAV_PULSE_SEQUENCE *seq, double us, int level ) {
    if ( seq->count >= seq->capacity ) return -1;
    st_WAV_PULSE *p = &seq->pulses[seq->count++];
    p->duration_us = us;
    p->duration_samples = ( uint32_t ) ( us * seq->sample_rate / 1000000.0 + 0.5 );
    p->level = level;
    return 0;
}


/**
 * @brief Přidá jeden celý pulz (HIGH + LOW).
 * @param seq Cílová sekvence.
 * @param is_long 1 = LONG (bit 1), 0 = SHORT (bit 0).
 * @return 0 při úspěchu, -1 při vyčerpání kapacity.
 */
static int add_pulse ( st_WAV_PULSE_SEQUENCE *seq, int is_long ) {
    if ( add_half ( seq, is_long ? LONG_HIGH_US : SHORT_HIGH_US, 1 ) ) return -1;
    return add_half ( seq, is_long ? LONG_LOW_US : SHORT_LOW_US, 0 );
}


/**
 * @brief Přidá bajt: 8 bitů MSB first + 1 LONG.
 * @param seq Cílová sekvence.
 * @param b Hodnota bajtu.
 * @return 0 při úspěchu, -1 při vyčerpání kapacity.
 */
static int add_byte ( st_WAV_PULSE_SEQUENCE *seq, uint8_t b ) {
    for ( int i = 7; i >= 0; i-- ) {
        if ( add_pulse ( seq, ( b >> i ) & 1 ) ) return -1;
    }
    return add_pulse ( seq, 1 );
}


int main ( void ) {
    st_WAV_PULSE_SEQUENCE seq;
    memset ( &seq, 0, sizeof ( seq ) );
    seq.capacity = 20000;
    seq.sample_rate = TEST_RATE;
    seq.pulses = ( st_WAV_PULSE* ) calloc ( seq.capacity, sizeof ( st_WAV_PULSE ) );
    if ( !seq.pulses ) { printf ( "FAIL: alloc\n" ); return 1; }

    /* hlavička MZF: ftype 01, jméno, fsize 0, fstrt/fexec 1200h */
    uint8_t hdr[128];
    memset ( hdr, 0, sizeof ( hdr ) );
    hdr[0] = 0x01;
    memcpy ( &hdr[1], "HDRONLY\r", 8 );
    hdr[0x14] = 0x00; hdr[0x15] = 0x12;
    hdr[0x16] = 0x00; hdr[0x17] = 0x12;

    uint16_t chk = 0;
    for ( int i = 0; i < 128; i++ ) {
        for ( int bit = 0; bit < 8; bit++ ) chk += ( hdr[i] >> bit ) & 1;
    }

    int rc = 0;
    for ( int i = 0; i < 4000; i++ ) rc |= add_pulse ( &seq, 0 );   /* leader */
    for ( int i = 0; i < 40; i++ ) rc |= add_pulse ( &seq, 1 );     /* LTM */
    for ( int i = 0; i < 40; i++ ) rc |= add_pulse ( &seq, 0 );
    rc |= add_pulse ( &seq, 1 );                                     /* 2L */
    rc |= add_pulse ( &seq, 1 );
    for ( int i = 0; i < 128; i++ ) rc |= add_byte ( &seq, hdr[i] );
    rc |= add_byte ( &seq, ( uint8_t ) ( chk >> 8 ) );               /* checksum BE */
    rc |= add_byte ( &seq, ( uint8_t ) ( chk & 0xFF ) );
    rc |= add_pulse ( &seq, 1 );                                     /* 2L */
    rc |= add_pulse ( &seq, 1 );
    if ( rc ) { printf ( "FAIL: sequence capacity\n" ); free ( seq.pulses ); return 1; }

    st_WAV_LEADER_INFO leader;
    if ( wav_leader_detect ( &seq, 0, 500, WAV_ANALYZER_DEFAULT_TOLERANCE, &leader ) != WAV_ANALYZER_OK ) {
        printf ( "FAIL: leader not detected\n" );
        free ( seq.pulses );
        return 1;
    }

    st_MZF *mzf = NULL;
    en_WAV_ANALYZER_ERROR err = wav_decode_fm_decode_mzf ( &seq, &leader, &mzf, NULL, NULL );

    int failures = 0;
    if ( err != WAV_ANALYZER_OK || !mzf ) {
        printf ( "FAIL: decode returned %d\n", ( int ) err );
        failures++;
    } else {
        if ( mzf->header.fsize != 0 ) { printf ( "FAIL: fsize=%u\n", mzf->header.fsize ); failures++; }
        if ( mzf->body != NULL ) { printf ( "FAIL: body is not NULL\n" ); failures++; }
        if ( mzf->body_size != 0 ) { printf ( "FAIL: body_size=%u\n", mzf->body_size ); failures++; }
        if ( failures == 0 ) printf ( "OK: header-only MZF decoded, body NULL\n" );
        mzf_free ( mzf );
    }

    free ( seq.pulses );
    printf ( "\n=== FM header-only Test: %s ===\n", failures ? "FAIL" : "PASS" );
    return failures ? 1 : 0;
}
