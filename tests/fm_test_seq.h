/**
 * @file   fm_test_seq.h
 * @brief  Sestavení a poškození sekvence půlvln FM pro testy dekodéru.
 *
 * Sekvence odpovídá zápisu ROM MZ-800 (kód ROM, shodně ROM-800-NOMINAL):
 * tón, tapemark, 1 L, data + checksum (každý bajt start bit L + 8 bitů MSB),
 * 1 L, volitelně 256 S a kopie. Funkce jsou static, hlavička je jen pro testy;
 * nepoužitá funkce v konkrétním testu nevadí (FM_TEST_UNUSED).
 *
 * Test musí před vložením mít stdint, string a typy wav_analyzer_types.h.
 */

#ifndef FM_TEST_SEQ_H
#define FM_TEST_SEQ_H

/** @brief Potlačí varování u pomocné funkce, kterou daný test nevolá. */
#define FM_TEST_UNUSED __attribute__ ( ( unused ) )

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
FM_TEST_UNUSED static int add_half ( st_WAV_PULSE_SEQUENCE *s, double us, int level ) {
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
FM_TEST_UNUSED static int add_pulse ( st_WAV_PULSE_SEQUENCE *s, int is_long ) {
    if ( add_half ( s, is_long ? LONG_HIGH_US : SHORT_HIGH_US, 1 ) ) return -1;
    return add_half ( s, is_long ? LONG_LOW_US : SHORT_LOW_US, 0 );
}

/**
 * @brief Přidá n stejných pulzů.
 * @param s Sekvence. @param is_long Druh pulzu. @param n Počet.
 * @return 0, nebo nenulové při plné sekvenci.
 */
FM_TEST_UNUSED static int add_run ( st_WAV_PULSE_SEQUENCE *s, int is_long, int n ) {
    int rc = 0;
    for ( int i = 0; i < n; i++ ) rc |= add_pulse ( s, is_long );
    return rc;
}

/** @brief Datová sekce ROM: [L + 8 bitů] x n, checksum BE stejně, 1 L. Vrací index 1. datové půlvlny. */
FM_TEST_UNUSED static int add_section ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *d, uint32_t n, uint32_t *first ) {
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
FM_TEST_UNUSED static int build ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *hdr, const uint8_t *body, int copies, st_POS *p ) {
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
FM_TEST_UNUSED static void flip_bit ( st_WAV_PULSE_SEQUENCE *s, uint32_t idx ) {
    int was_long = s->pulses[idx].duration_us > 400.0;
    s->pulses[idx].duration_us = was_long ? SHORT_HIGH_US : LONG_HIGH_US;
    s->pulses[idx + 1].duration_us = was_long ? SHORT_LOW_US : LONG_LOW_US;
}

/** @brief Výpadek: n půlvln od idx nahradí jednou dlouhou půlvlnou (sekvence se zkrátí). */
FM_TEST_UNUSED static void dropout ( st_WAV_PULSE_SEQUENCE *s, uint32_t idx, uint32_t n ) {
    double sum = 0;
    for ( uint32_t i = 0; i < n; i++ ) sum += s->pulses[idx + i].duration_us;
    s->pulses[idx].duration_us = sum;
    memmove ( &s->pulses[idx + 1], &s->pulses[idx + n], ( s->count - idx - n ) * sizeof ( st_WAV_PULSE ) );
    s->count -= n - 1;
}

/** @brief Usekne sekvenci na idx půlvln (konec nahrávky). */
FM_TEST_UNUSED static void truncate_at ( st_WAV_PULSE_SEQUENCE *s, uint32_t idx ) {
    if ( idx < s->count ) s->count = idx;
}

/**
 * @brief Sestaví jen hlavičku (tón, LTM, 1 L, sekce), bez těla; za ní konec sekvence.
 * @param s Sekvence (přepíše se). @param hdr Hlavička 128 B.
 * @param[out] hdr1 Index první datové půlvlny hlavičky.
 * @return 0, nebo nenulové při plné sekvenci.
 */
FM_TEST_UNUSED static int build_header_only ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *hdr, uint32_t *hdr1 ) {
    int rc = 0;
    s->count = 0;
    rc |= add_run ( s, 0, 4000 );
    rc |= add_run ( s, 1, 40 ); rc |= add_run ( s, 0, 40 ); rc |= add_pulse ( s, 1 );
    return rc | add_section ( s, hdr, 128, hdr1 );
}

#endif /* FM_TEST_SEQ_H */
