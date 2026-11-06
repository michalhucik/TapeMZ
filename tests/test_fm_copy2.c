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

/** @brief Vzorkovací frekvence sekvence. */
#define TEST_RATE      44100
/** @brief Půlvlny ROM-800-NOMINAL v us (tmz_profile.c rom_params). */
#define SHORT_HIGH_US  240.0
/** @brief Viz SHORT_HIGH_US. */
#define SHORT_LOW_US   278.0
/** @brief Viz SHORT_HIGH_US. */
#define LONG_HIGH_US   470.0
/** @brief Viz SHORT_HIGH_US. */
#define LONG_LOW_US    494.0
/** @brief Velikost těla testovacího souboru. */
#define BODY_SIZE      1024

/** @brief Pozice v sekvenci, které test potřebuje (indexy půlvln). */
typedef struct {
    uint32_t hdr1;      /**< první datová půlvlna 1. kopie hlavičky (za start bitem) */
    uint32_t hdr2_end;  /**< konec 2. kopie hlavičky (za koncovým L) */
    uint32_t body1;     /**< první datová půlvlna 1. kopie těla */
    uint32_t body2;     /**< první datová půlvlna 2. kopie těla */
    uint32_t body2_end; /**< konec 2. kopie těla (za koncovým L) */
} st_POS;

/**
 * @brief Přidá půlvlnu do sekvence.
 * @param s Sekvence. @param us Délka (us). @param level Úroveň.
 * @return 0, nebo -1 při plné sekvenci.
 */
static int add_half ( st_WAV_PULSE_SEQUENCE *s, double us, int level ) {
    if ( s->count >= s->capacity ) return -1;
    st_WAV_PULSE *p = &s->pulses[s->count++];
    p->duration_us = us;
    p->duration_samples = ( uint32_t ) ( us * s->sample_rate / 1000000.0 + 0.5 );
    p->level = level;
    return 0;
}

/**
 * @brief Přidá celý pulz (HIGH + LOW) krátký nebo dlouhý.
 * @param s Sekvence. @param is_long 1 = dlouhý (bit 1), 0 = krátký.
 * @return 0, nebo -1 při plné sekvenci.
 */
static int add_pulse ( st_WAV_PULSE_SEQUENCE *s, int is_long ) {
    if ( add_half ( s, is_long ? LONG_HIGH_US : SHORT_HIGH_US, 1 ) ) return -1;
    return add_half ( s, is_long ? LONG_LOW_US : SHORT_LOW_US, 0 );
}

/**
 * @brief Přidá n stejných pulzů.
 * @param s Sekvence. @param is_long Druh pulzu. @param n Počet.
 * @return 0, nebo nenulové při plné sekvenci.
 */
static int add_run ( st_WAV_PULSE_SEQUENCE *s, int is_long, int n ) {
    int rc = 0;
    for ( int i = 0; i < n; i++ ) rc |= add_pulse ( s, is_long );
    return rc;
}

/** @brief Datová sekce ROM: [L + 8 bitů] x n, checksum BE stejně, 1 L. Vrací index 1. datové půlvlny. */
static int add_section ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *d, uint32_t n, uint32_t *first ) {
    uint16_t ck = 0;
    for ( uint32_t i = 0; i < n; i++ ) for ( int b = 0; b < 8; b++ ) ck += ( d[i] >> b ) & 1;
    uint8_t ckb[2] = { ( uint8_t ) ( ck >> 8 ), ( uint8_t ) ck };
    int rc = 0;
    *first = s->count + 2;
    for ( uint32_t i = 0; i < n + 2; i++ ) {
        uint8_t v = ( i < n ) ? d[i] : ckb[i - n];
        rc |= add_pulse ( s, 1 );
        for ( int b = 7; b >= 0; b-- ) rc |= add_pulse ( s, ( v >> b ) & 1 );
    }
    return rc | add_pulse ( s, 1 );
}

/**
 * @brief Sestaví záznam hlavičky a těla jako ROM (s kopiemi, nebo bez).
 * @param s Sekvence (přepíše se). @param hdr Hlavička 128 B. @param body Tělo BODY_SIZE B.
 * @param copies 1 = s druhými kopiemi. @param[out] p Pozice úseků.
 * @return 0, nebo nenulové při plné sekvenci.
 */
static int build ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *hdr, const uint8_t *body, int copies, st_POS *p ) {
    uint32_t dummy;
    int rc = 0;
    s->count = 0;
    rc |= add_run ( s, 0, 4000 );                     /* tón hlavičky */
    rc |= add_run ( s, 1, 40 ); rc |= add_run ( s, 0, 40 ); rc |= add_pulse ( s, 1 );
    rc |= add_section ( s, hdr, 128, &p->hdr1 );
    if ( copies ) { rc |= add_run ( s, 0, 256 ); rc |= add_section ( s, hdr, 128, &dummy ); }
    p->hdr2_end = s->count;
    rc |= add_run ( s, 0, 2000 );                     /* tón těla */
    rc |= add_run ( s, 1, 20 ); rc |= add_run ( s, 0, 20 ); rc |= add_pulse ( s, 1 );
    rc |= add_section ( s, body, BODY_SIZE, &p->body1 );
    p->body2 = 0;
    if ( copies ) { rc |= add_run ( s, 0, 256 ); rc |= add_section ( s, body, BODY_SIZE, &p->body2 ); }
    p->body2_end = s->count;
    return rc;
}

/** @brief Převrátí bit (S <-> L) na datové půlvlně idx (dvojice HIGH+LOW). */
static void flip_bit ( st_WAV_PULSE_SEQUENCE *s, uint32_t idx ) {
    int was_long = s->pulses[idx].duration_us > 400.0;
    s->pulses[idx].duration_us = was_long ? SHORT_HIGH_US : LONG_HIGH_US;
    s->pulses[idx + 1].duration_us = was_long ? SHORT_LOW_US : LONG_LOW_US;
}

/** @brief Výpadek: n půlvln od idx nahradí jednou dlouhou půlvlnou (sekvence se zkrátí). */
static void dropout ( st_WAV_PULSE_SEQUENCE *s, uint32_t idx, uint32_t n ) {
    double sum = 0;
    for ( uint32_t i = 0; i < n; i++ ) sum += s->pulses[idx + i].duration_us;
    s->pulses[idx].duration_us = sum;
    memmove ( &s->pulses[idx + 1], &s->pulses[idx + n], ( s->count - idx - n ) * sizeof ( st_WAV_PULSE ) );
    s->count -= n - 1;
}

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
