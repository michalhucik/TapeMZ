/**
 * @file   test_fm_recovery.c
 * @brief  Obnova hlavičky a těla FM z poškozené sekvence půlvln.
 *
 * Sestaví sekvenci podle zápisu ROM (fm_test_seq.h), nasimuluje poškození
 * (useknutí nahrávky, chybějící tělo, převrácený bit) a ověří chování dekodéru
 * bez voleb obnovy i s nimi. Data nepotřebuje.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/wav_analyzer/wav_analyzer_types.h"
#include "libs/wav_analyzer/wav_leader.h"
#include "libs/wav_analyzer/wav_decode_fm.h"
#include "libs/wav_analyzer/wav_decode_bsd.h"

#include "fm_test_seq.h"

/** @brief Počet selhaných kontrol. */
static int g_fails = 0;

/**
 * @brief Ověří podmínku a při selhání ji vypíše.
 * @param name Název případu. @param cond Podmínka. @param what Popis podmínky.
 */
static void ck ( const char *name, int cond, const char *what ) {
    if ( !cond ) {
        printf ( "FAIL [%s]: %s\n", name, what );
        g_fails++;
    }
}

/** @brief Výsledek jednoho dekódování (vlastní paměť, uvolní res_free()). */
typedef struct {
    en_WAV_ANALYZER_ERROR err;  /**< návratový kód dekodéru */
    st_MZF *mzf;                /**< MZF, nebo NULL */
    st_WAV_DECODE_RESULT hr;    /**< výsledek hlavičky */
    st_WAV_DECODE_RESULT br;    /**< výsledek těla */
    st_WAV_FM_DECODE_INFO info; /**< informace o obnově */
} st_RES;

/**
 * @brief Dekóduje sekvenci.
 * @param s Sekvence. @param opts Volby obnovy (NULL = žádné). @param[out] r Výsledek.
 * @return 0, nebo -1 když se nenašel leader.
 */
static int dec ( const st_WAV_PULSE_SEQUENCE *s, const st_WAV_FM_DECODE_OPTIONS *opts, st_RES *r ) {
    st_WAV_LEADER_INFO leader;
    memset ( r, 0, sizeof ( *r ) );
    if ( wav_leader_detect ( s, 0, 500, WAV_ANALYZER_DEFAULT_TOLERANCE, &leader ) != WAV_ANALYZER_OK ) return -1;
    r->err = wav_decode_fm_decode_mzf_ex ( s, &leader, opts, &r->mzf, &r->hr, &r->br, &r->info );
    return 0;
}

/** @brief Uvolní výsledek dec(). @param r Výsledek. */
static void res_free ( st_RES *r ) {
    mzf_free ( r->mzf );
    free ( r->hr.data );
    free ( r->br.data );
}

/**
 * @brief Vyplní testovací hlavičku a tělo.
 * @param[out] hdr Hlavička 128 B (surová, jak se zapisuje). @param[out] body Tělo BODY_SIZE B.
 * @param fsize Velikost deklarovaná v hlavičce.
 */
static void fill ( uint8_t *hdr, uint8_t *body, uint16_t fsize ) {
    memset ( hdr, 0, 128 );
    hdr[0] = 1;
    memcpy ( &hdr[1], "RECOVERY\r", 9 );
    hdr[0x12] = ( uint8_t ) fsize;
    hdr[0x13] = ( uint8_t ) ( fsize >> 8 );
    hdr[0x15] = 0x12;
    hdr[0x17] = 0x12;
    for ( uint32_t i = 0; i < BODY_SIZE; i++ ) body[i] = ( uint8_t ) ( i * 7 + 3 );
}

/** @brief Tělo useknuté po 700 bajtech, bez voleb: soubor se zahodí, informace zůstane. */
static void case_a ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *hdr, const uint8_t *body ) {
    const char *n = "A truncated body, no options";
    st_POS p; st_RES r;
    build ( s, hdr, body, 0, &p );
    truncate_at ( s, p.body1 + 18 * 700 );
    if ( dec ( s, NULL, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.err == WAV_ANALYZER_ERROR_DECODE_DATA, "err == DECODE_DATA" );
    ck ( n, r.mzf == NULL, "no MZF" );
    ck ( n, r.info.header_decoded == 1, "header_decoded" );
    ck ( n, r.info.body_error == WAV_ANALYZER_ERROR_DECODE_INCOMPLETE, "body_error == INCOMPLETE" );
    ck ( n, r.info.recovered_bytes == 700, "recovered_bytes == 700" );
    ck ( n, r.info.expected_bytes == BODY_SIZE, "expected_bytes == fsize" );
    ck ( n, r.info.recovery_status == WAV_RECOVERY_NONE, "status NONE" );
    res_free ( &r );
}

/**
 * @brief Ověří výsledek částečného těla: n bajtů dat, zbytek 00h, CRC ERROR, PARTIAL_BODY.
 * @param name Název případu. @param r Výsledek. @param body Očekávané tělo. @param n Přečtených bajtů.
 */
static void ck_partial ( const char *name, const st_RES *r, const uint8_t *body, uint32_t n ) {
    ck ( name, r->err == WAV_ANALYZER_OK && r->mzf != NULL, "decoded" );
    if ( !r->mzf ) return;
    ck ( name, r->mzf->body_size == BODY_SIZE, "body_size == fsize" );
    ck ( name, r->mzf->body && memcmp ( r->mzf->body, body, n ) == 0, "bytes read match" );
    int zeros = 1;
    for ( uint32_t i = n; r->mzf->body && i < BODY_SIZE; i++ ) if ( r->mzf->body[i] ) zeros = 0;
    ck ( name, zeros, "rest is 00h" );
    ck ( name, r->br.crc_status == WAV_CRC_ERROR, "body CRC ERROR" );
    ck ( name, r->info.recovery_status == WAV_RECOVERY_PARTIAL_BODY, "status PARTIAL_BODY" );
    ck ( name, r->info.recovered_bytes == n, "recovered_bytes" );
}

/** @brief Tělo useknuté po 700 bajtech s recover_body. */
static void case_b ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *hdr, const uint8_t *body ) {
    const char *n = "B truncated body, recover_body";
    st_WAV_FM_DECODE_OPTIONS o = { 1, 0 };
    st_POS p; st_RES r;
    build ( s, hdr, body, 0, &p );
    truncate_at ( s, p.body1 + 18 * 700 );
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck_partial ( n, &r, body, 700 );
    res_free ( &r );
}

/** @brief Všechna data přečtena, checksum useknutý: stále částečné tělo, ne úspěch. */
static void case_c ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *hdr, const uint8_t *body ) {
    const char *n = "C checksum cut off, recover_body";
    st_WAV_FM_DECODE_OPTIONS o = { 1, 0 };
    st_POS p; st_RES r;
    build ( s, hdr, body, 0, &p );
    truncate_at ( s, p.body1 + 18 * BODY_SIZE + 4 );
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck_partial ( n, &r, body, BODY_SIZE );
    res_free ( &r );
}

/** @brief Z těla se nepřečetl ani bajt: recover_body samo o sobě soubor neponechá. */
static void case_d ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *hdr, const uint8_t *body ) {
    const char *n = "D no body byte, recover_body";
    st_WAV_FM_DECODE_OPTIONS o = { 1, 0 };
    st_POS p; st_RES r;
    build ( s, hdr, body, 0, &p );
    truncate_at ( s, p.body1 );
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.err == WAV_ANALYZER_ERROR_DECODE_DATA, "err == DECODE_DATA" );
    ck ( n, r.mzf == NULL, "no MZF" );
    res_free ( &r );
}

/** @brief Tělo přemazané tónem dalšího záznamu: konec dat je začátek tónu, tón se nespotřebuje. */
static void case_k ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *hdr, const uint8_t *body ) {
    const char *n = "K body overwritten by next tone, recover_body";
    st_WAV_FM_DECODE_OPTIONS o = { 1, 0 };
    st_POS p; st_RES r;
    build ( s, hdr, body, 0, &p );
    uint32_t cut = p.body1 + 18 * 600;
    truncate_at ( s, cut );
    add_run ( s, 0, 12000 );
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck_partial ( n, &r, body, 600 );
    ck ( n, r.br.pulse_end <= cut + 2, "tone of the next recording not consumed" );
    res_free ( &r );
}

/** @brief Tělo přemazané krátkým tónem, za kterým signál skončí dřív než fsize: hranice je stejná. */
static void case_l ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *hdr, const uint8_t *body ) {
    const char *n = "L body overwritten by a short tone, signal ends before fsize";
    st_WAV_FM_DECODE_OPTIONS o = { 1, 0 };
    st_POS p; st_RES r;
    build ( s, hdr, body, 0, &p );
    uint32_t cut = p.body1 + 18 * 600;
    truncate_at ( s, cut );
    add_run ( s, 0, 1000 );
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck_partial ( n, &r, body, 600 );
    ck ( n, r.br.pulse_end <= cut + 2, "tone not consumed" );
    res_free ( &r );
}

/**
 * @brief Hlavička deklaruje jiné fsize a má chybu CRC: délka se opraví podle pásky.
 * @param s Sekvence. @param body Tělo BODY_SIZE B. @param name Název případu.
 * @param fsize Deklarované fsize. @param silent_tail 1 = za tělem ticho (poslední půlvlna splyne s pauzou).
 * @param expect_repair 1 = čeká se oprava na BODY_SIZE, 0 = fsize zůstává.
 */
static void run_fsize ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *body, const char *name, uint16_t fsize,
                        int silent_tail, int expect_repair ) {
    uint8_t h[128], b[BODY_SIZE];
    st_POS p; st_RES r;
    fill ( h, b, fsize );
    build ( s, h, body, 0, &p );
    flip_bit ( s, p.hdr1 + 18 * 0x20 );
    if ( silent_tail ) s->pulses[s->count - 1].duration_us = 2000000.0;
    if ( dec ( s, NULL, &r ) ) { ck ( name, 0, "leader" ); return; }
    ck ( name, r.err == WAV_ANALYZER_OK && r.mzf, "decoded" );
    ck ( name, r.hr.crc_status == WAV_CRC_ERROR, "header CRC stays ERROR" );
    if ( r.mzf ) {
        ck ( name, r.mzf->header.fsize == BODY_SIZE, "fsize == tape length" );
        ck ( name, r.mzf->body_size == BODY_SIZE && r.mzf->body && memcmp ( r.mzf->body, body, BODY_SIZE ) == 0, "body matches" );
    }
    ck ( name, r.br.crc_status == WAV_CRC_OK, "body CRC OK" );
    ck ( name, r.info.recovery_status == ( expect_repair ? WAV_RECOVERY_FSIZE_REPAIRED : WAV_RECOVERY_NONE ), "recovery status" );
    res_free ( &r );
}

/** @brief Sada po hlavičce: tělo chybí, bez voleb i s recover_header, i s vadnou hlavičkou. */
static void case_e ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *hdr, const uint8_t *body ) {
    const char *n = "E header only";
    st_WAV_FM_DECODE_OPTIONS o = { 0, 1 };
    uint32_t h1 = 0; st_RES r;
    build_header_only ( s, hdr, &h1 );
    if ( dec ( s, NULL, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.err == WAV_ANALYZER_ERROR_DECODE_TAPEMARK, "no options: DECODE_TAPEMARK" );
    ck ( n, r.info.header_decoded == 1, "no options: header_decoded" );
    ck ( n, r.info.body_error == WAV_ANALYZER_ERROR_DECODE_TAPEMARK, "no options: body_error" );
    ck ( n, memcmp ( r.info.header_raw, hdr, 128 ) == 0, "no options: header_raw" );
    res_free ( &r );

    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.err == WAV_ANALYZER_OK && r.mzf, "recover_header: decoded" );
    if ( r.mzf ) {
        ck ( n, r.mzf->body_size == BODY_SIZE && r.mzf->body, "recover_header: body_size == fsize" );
        int zeros = 1;
        for ( uint32_t i = 0; r.mzf->body && i < BODY_SIZE; i++ ) if ( r.mzf->body[i] ) zeros = 0;
        ck ( n, zeros, "recover_header: body 00h" );
    }
    ck ( n, r.br.crc_status == WAV_CRC_NOT_AVAILABLE, "recover_header: body CRC N/A" );
    ck ( n, r.info.recovery_status == WAV_RECOVERY_HEADER_ONLY, "recover_header: status HEADER_ONLY" );
    ck ( n, r.info.recovered_bytes == 0, "recover_header: recovered_bytes 0" );
    res_free ( &r );

    /* vadná hlavička bez těla je smetí: neponechá se */
    flip_bit ( s, h1 + 18 * 0x20 );
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.err == WAV_ANALYZER_ERROR_DECODE_TAPEMARK && !r.mzf, "bad header CRC is not kept" );
    res_free ( &r );

    /* tapemark těla nalezen, ale z těla ani bajt */
    st_POS p;
    build ( s, hdr, body, 0, &p );
    truncate_at ( s, p.body1 );
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.err == WAV_ANALYZER_OK && r.mzf, "no body byte: decoded" );
    ck ( n, r.info.recovery_status == WAV_RECOVERY_HEADER_ONLY, "no body byte: status" );
    res_free ( &r );
}

/**
 * @brief Sestaví hlavičku bez těla, za ní celý další záznam (s kopiemi).
 * @param s Sekvence (přepíše se). @param h1 Hlavička prvního záznamu. @param bad_crc 1 = chyba CRC hlavičky.
 * @param h2 Hlavička dalšího záznamu. @param body Tělo dalšího záznamu.
 * @return Index první půlvlny dalšího záznamu.
 */
static uint32_t build_two ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *h1, int bad_crc, const uint8_t *h2, const uint8_t *body ) {
    st_WAV_PULSE_SEQUENCE t = *s;
    st_POS p;
    uint32_t first = 0;
    t.pulses = calloc ( t.capacity, sizeof ( st_WAV_PULSE ) );
    build ( &t, h2, body, 1, &p );
    build_header_only ( s, h1, &first );
    if ( bad_crc ) flip_bit ( s, first + 18 * 0x20 );
    uint32_t next = s->count;
    memcpy ( &s->pulses[next], t.pulses, t.count * sizeof ( st_WAV_PULSE ) );
    s->count += t.count;
    free ( t.pulses );
    return next;
}

/** @brief Hlavička bez těla před dalším záznamem: cizí hlavička není tělo, další záznam zůstane. */
static void case_next ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *body ) {
    uint8_t h0[128], h1k[128], h2[128], b[BODY_SIZE];
    st_WAV_FM_DECODE_OPTIONS all = { 1, 1 };
    st_RES r;
    fill ( h0, b, 0 );
    fill ( h1k, b, BODY_SIZE );
    fill ( h2, b, BODY_SIZE );
    memcpy ( &h2[1], "SECOND\r", 7 );

    const char *n = "X fsize 0, bad header CRC, next record follows";
    uint32_t next = build_two ( s, h0, 1, h2, body );
    if ( dec ( s, NULL, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.err == WAV_ANALYZER_OK && r.mzf, "decoded" );
    if ( r.mzf ) ck ( n, r.mzf->header.fsize == 0 && r.mzf->body_size == 0, "fsize stays 0" );
    ck ( n, r.info.recovery_status == WAV_RECOVERY_NONE, "not repaired" );
    ck ( n, r.hr.pulse_end <= next && r.br.pulse_end <= next, "next record not consumed" );
    res_free ( &r );

    n = "Z fsize 1024, bad header CRC, body missing, next record follows";
    next = build_two ( s, h1k, 1, h2, body );
    if ( dec ( s, NULL, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.err == WAV_ANALYZER_ERROR_DECODE_TAPEMARK && !r.mzf, "foreign header is not a body" );
    ck ( n, r.info.recovery_status == WAV_RECOVERY_NONE, "not repaired" );
    ck ( n, r.info.header_decoded == 1 && r.info.header_crc_ok == 0, "header decoded with a bad CRC" );
    res_free ( &r );

    n = "Y fsize 1024, valid header, body missing, next record follows, all options";
    next = build_two ( s, h1k, 0, h2, body );
    if ( dec ( s, &all, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.err == WAV_ANALYZER_OK && r.mzf, "decoded" );
    ck ( n, r.info.recovery_status == WAV_RECOVERY_HEADER_ONLY, "status HEADER_ONLY" );
    ck ( n, r.info.header_crc_ok == 1, "header_crc_ok" );
    ck ( n, r.br.pulse_end <= next, "next record not consumed" );
    res_free ( &r );
}

/** @brief Za tapemarkem těla hned tón dalšího záznamu: 0 přečtených bajtů není částečné tělo. */
static void case_o ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *hdr, const uint8_t *body ) {
    const char *n = "O body tapemark followed by a tone";
    st_WAV_FM_DECODE_OPTIONS ob = { 1, 0 }, oh = { 1, 1 };
    st_POS p; st_RES r;
    build ( s, hdr, body, 0, &p );
    truncate_at ( s, p.body1 );
    add_run ( s, 0, 12000 );
    if ( dec ( s, &ob, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.err == WAV_ANALYZER_ERROR_DECODE_DATA && !r.mzf, "recover_body alone keeps nothing" );
    ck ( n, r.info.recovery_status == WAV_RECOVERY_NONE, "status NONE" );
    res_free ( &r );
    if ( dec ( s, &oh, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.err == WAV_ANALYZER_OK && r.mzf, "recover_header keeps the header" );
    ck ( n, r.info.recovery_status == WAV_RECOVERY_HEADER_ONLY, "status HEADER_ONLY" );
    res_free ( &r );
}

/** @brief Index první půlvlny bitu b (0 = MSB) bajtu k sekce začínající first. */
static uint32_t bit_hw ( uint32_t first, uint32_t k, int b ) {
    return first + 18 * k + 2 * ( uint32_t ) b;
}

/**
 * @brief Převrátí v sekci jednu jedničku a jednu nulu: součet se nezmění, data ano.
 * @param s Sekvence. @param first První datová půlvlna sekce. @param d Data sekce.
 * @param n Velikost dat. @return 0, nebo -1 když sekce nemá vhodné bity.
 */
static int flip_compensated ( st_WAV_PULSE_SEQUENCE *s, uint32_t first, const uint8_t *d, uint32_t n ) {
    uint32_t one = n, zero = n;
    int b1 = 0, b0 = 0;
    for ( uint32_t k = 10; k < n && ( one == n || zero == n ); k++ ) {
        for ( int b = 0; b < 8; b++ ) {
            int bit = ( d[k] >> ( 7 - b ) ) & 1;
            if ( bit && one == n ) { one = k; b1 = b; }
            if ( !bit && zero == n && one != n && k != one ) { zero = k; b0 = b; }
        }
    }
    if ( one == n || zero == n ) return -1;
    flip_bit ( s, bit_hw ( first, one, b1 ) );
    flip_bit ( s, bit_hw ( first, zero, b0 ) );
    return 0;
}

/** @brief Kopie se liší, ačkoli 1. kopie má platný součet (převrácení se vyruší). */
static void case_copy_diff ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *hdr, const uint8_t *body ) {
    const char *n = "P body copies differ, checksum valid";
    st_POS p; st_RES r;
    build ( s, hdr, body, 1, &p );
    flip_compensated ( s, p.body1, body, BODY_SIZE );
    if ( dec ( s, NULL, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.br.crc_status == WAV_CRC_OK, "body CRC OK" );
    ck ( n, r.info.body_copy_diff == 2, "body_copy_diff == 2" );
    ck ( n, r.info.header_copy_diff == 0, "header_copy_diff == 0" );
    res_free ( &r );

    n = "Q clean recording";
    build ( s, hdr, body, 1, &p );
    if ( dec ( s, NULL, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.info.body_copy_diff == 0 && r.info.header_copy_diff == 0, "no difference" );
    res_free ( &r );

    n = "R1 recording without copies";
    build ( s, hdr, body, 0, &p );
    if ( dec ( s, NULL, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.info.body_copy_diff == 0 && r.info.header_copy_diff == 0, "no false warning" );
    res_free ( &r );

    n = "R2 damaged copy 2: no warning about a valid copy 1";
    build ( s, hdr, body, 1, &p );
    dropout ( s, p.body2 + 18 * 900, 20 );
    if ( dec ( s, NULL, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.br.crc_status == WAV_CRC_OK && r.info.body_copy_diff == 0, "no warning" );
    res_free ( &r );

    n = "R3 no copies, zero-filled body, short tone and a next header";
    {
        uint8_t z[BODY_SIZE];
        uint32_t f;
        memset ( z, 0, sizeof ( z ) );
        memcpy ( z, body, 100 );
        build ( s, hdr, z, 0, &p );
        add_run ( s, 0, 500 );
        add_run ( s, 1, 40 ); add_run ( s, 0, 40 ); add_pulse ( s, 1 );
        add_section ( s, hdr, 128, &f );
        add_run ( s, 0, 12000 );
    }
    if ( dec ( s, NULL, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.info.body_copy_diff == 0, "no false warning" );
    res_free ( &r );

    n = "P2 header copies differ, checksum valid";
    build ( s, hdr, body, 1, &p );
    flip_compensated ( s, p.hdr1, hdr, 128 );
    if ( dec ( s, NULL, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.hr.crc_status == WAV_CRC_OK, "header CRC OK" );
    ck ( n, r.info.header_copy_diff == 2, "header_copy_diff == 2" );
    res_free ( &r );
}

/**
 * @brief Najde bajt s bitem dané hodnoty od indexu k.
 * @param d Data. @param n Velikost. @param k Začátek hledání. @param val Hledaná hodnota bitu.
 * @param[out] bit Pozice bitu (0 = MSB). @return Index bajtu.
 */
static uint32_t find_bit ( const uint8_t *d, uint32_t n, uint32_t k, int val, int *bit ) {
    for ( ; k < n; k++ ) {
        for ( int b = 0; b < 8; b++ ) {
            if ( ( ( d[k] >> ( 7 - b ) ) & 1 ) == val ) { *bit = b; return k; }
        }
    }
    return n;
}

/** @brief Ověří, že se tělo složilo ze dvou kopií. */
static void ck_merged ( const char *name, const st_RES *r, const uint8_t *body ) {
    ck ( name, r->err == WAV_ANALYZER_OK && r->mzf, "decoded" );
    if ( !r->mzf ) return;
    ck ( name, r->mzf->body_size == BODY_SIZE && memcmp ( r->mzf->body, body, BODY_SIZE ) == 0, "body matches" );
    ck ( name, r->br.crc_status == WAV_CRC_OK, "body CRC OK" );
    ck ( name, r->info.recovery_status == WAV_RECOVERY_COPY_MERGED, "status COPY_MERGED" );
    ck ( name, r->info.merged_bytes > 0, "merged_bytes > 0" );
}

/** @brief Sloučení dvou poškozených kopií těla. */
static void case_merge ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *hdr, const uint8_t *body ) {
    st_WAV_FM_DECODE_OPTIONS o = { 1, 0 };
    const char *n = "S1 dropout in each copy, no options";
    st_POS p; st_RES r;
    int b1 = 0, b2 = 0;

    build ( s, hdr, body, 1, &p );
    dropout ( s, p.body2 + 18 * 700, 40 );
    dropout ( s, p.body1 + 18 * 200, 30 );
    if ( dec ( s, NULL, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.err == WAV_ANALYZER_OK && r.br.crc_status == WAV_CRC_ERROR, "CRC ERROR without recovery" );
    ck ( n, r.info.recovery_status == WAV_RECOVERY_NONE, "status NONE" );
    res_free ( &r );
    n = "S2 dropout in each copy, recover_body";
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck_merged ( n, &r, body );
    res_free ( &r );

    n = "S3 a flipped bit in each copy, recover_body";
    build ( s, hdr, body, 1, &p );
    {
        uint32_t k1 = find_bit ( body, BODY_SIZE, 100, 1, &b1 );
        uint32_t k2 = find_bit ( body, BODY_SIZE, 800, 1, &b2 );
        flip_bit ( s, bit_hw ( p.body1, k1, b1 ) );
        flip_bit ( s, bit_hw ( p.body2, k2, b2 ) );
    }
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck_merged ( n, &r, body );
    res_free ( &r );

    n = "S4 flips of opposite direction are ambiguous, not merged";
    build ( s, hdr, body, 1, &p );
    {
        uint32_t k1 = find_bit ( body, BODY_SIZE, 100, 1, &b1 );
        uint32_t k2 = find_bit ( body, BODY_SIZE, 800, 0, &b2 );
        flip_bit ( s, bit_hw ( p.body1, k1, b1 ) );
        flip_bit ( s, bit_hw ( p.body2, k2, b2 ) );
    }
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.br.crc_status == WAV_CRC_ERROR, "CRC ERROR stays" );
    ck ( n, !( r.info.recovery_status & WAV_RECOVERY_COPY_MERGED ), "not merged" );
    res_free ( &r );

    n = "S5 dropout on the same place of both copies, not merged";
    build ( s, hdr, body, 1, &p );
    dropout ( s, p.body2 + 18 * 500, 60 );
    dropout ( s, p.body1 + 18 * 500, 60 );
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.err == WAV_ANALYZER_OK && r.br.crc_status == WAV_CRC_ERROR, "CRC ERROR stays" );
    ck ( n, r.info.recovery_status == WAV_RECOVERY_NONE, "nothing accepted" );
    res_free ( &r );

    n = "S9 silence and the next tone after copy 2, damage of copy 2 before damage of copy 1";
    build ( s, hdr, body, 1, &p );
    dropout ( s, p.body2 + 18 * 200, 30 );
    dropout ( s, p.body1 + 18 * 700, 40 );
    s->pulses[s->count - 1].duration_us = 2000000.0;
    add_run ( s, 0, 6000 );
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck_merged ( n, &r, body );
    res_free ( &r );

    n = "S10 a separate half-wave of silence after copy 2";
    build ( s, hdr, body, 1, &p );
    dropout ( s, p.body2 + 18 * 200, 30 );
    dropout ( s, p.body1 + 18 * 700, 40 );
    add_half ( s, 2000000.0, 1 );
    add_run ( s, 0, 6000 );
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck_merged ( n, &r, body );
    res_free ( &r );

    n = "S7 dropout shifting a copy by whole bytes";
    build ( s, hdr, body, 1, &p );
    dropout ( s, p.body2 + 18 * 700, 37 );
    dropout ( s, p.body1 + 18 * 200, 19 );
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck_merged ( n, &r, body );
    res_free ( &r );

    n = "S8 whole bytes lost without a gap are never merged wrongly";
    build ( s, hdr, body, 1, &p );
    /* půlvlny zmizí beze stopy, rámec zůstane na celých bajtech */
    memmove ( &s->pulses[p.body2 + 18 * 700], &s->pulses[p.body2 + 18 * 702], ( s->count - p.body2 - 18 * 702 ) * sizeof ( st_WAV_PULSE ) );
    s->count -= 36;
    memmove ( &s->pulses[p.body1 + 18 * 200], &s->pulses[p.body1 + 18 * 201], ( s->count - p.body1 - 18 * 201 ) * sizeof ( st_WAV_PULSE ) );
    s->count -= 18;
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, !( r.info.recovery_status & WAV_RECOVERY_COPY_MERGED ) || ( r.mzf && memcmp ( r.mzf->body, body, BODY_SIZE ) == 0 ),
         "merged only with the right data" );
    res_free ( &r );

    n = "S6 bad bit near the threshold is resolved by the clean copy";
    build ( s, hdr, body, 1, &p );
    {
        uint32_t k1 = find_bit ( body, BODY_SIZE, 100, 1, &b1 );
        uint32_t k2 = find_bit ( body, BODY_SIZE, 800, 0, &b2 );
        uint32_t h1 = bit_hw ( p.body1, k1, b1 ), h2 = bit_hw ( p.body2, k2, b2 );
        /* jednička přečtená jako nula a naopak, obě těsně za prahem */
        s->pulses[h1].duration_us = s->pulses[h1 + 1].duration_us = 410.0;
        s->pulses[h2].duration_us = s->pulses[h2 + 1].duration_us = 420.0;
    }
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck_merged ( n, &r, body );
    res_free ( &r );
}

/** @brief Krátké sekce: čtení z obou konců nesmí sahat mimo data ani se splést. */
static void case_merge_small ( st_WAV_PULSE_SEQUENCE *s ) {
    static const uint32_t sizes[] = { 1, 2, 8, 16, 40 };
    uint8_t d[40], out[40];
    for ( uint32_t i = 0; i < 40; i++ ) d[i] = ( uint8_t ) ( 0x91 + i * 13 );
    for ( uint32_t z = 0; z < sizeof ( sizes ) / sizeof ( sizes[0] ); z++ ) {
        uint32_t n = sizes[z], f1 = 0, f2 = 0, merged = 0;
        st_WAV_FM_DECODER fm;
        s->count = 0;
        add_section ( s, d, n, &f1 );
        add_run ( s, 0, 256 );
        add_section ( s, d, n, &f2 );
        add_run ( s, 0, 256 );
        wav_decode_fm_init ( &fm, s, f1, 259.0 );
        memset ( out, 0xEE, sizeof ( out ) );
        int ok = wav_decode_fm_merge_copies ( &fm, f1, f2, n, out, &merged );
        ck ( "T small section", ok == 1 && memcmp ( out, d, n ) == 0 && merged == 0, "clean copies merge" );
        ck ( "T small section", n >= 40 || out[n] == 0xEE, "no write past the end" );
    }
    uint32_t f1 = 0, f2 = 0, merged = 0;
    st_WAV_FM_DECODER fm;
    uint8_t out0[8];
    s->count = 0;
    add_section ( s, d, 8, &f1 );
    wav_decode_fm_init ( &fm, s, f1, 259.0 );
    f2 = s->count + 10;   /* kopie za koncem sekvence */
    /* 2. kopie mimo sekvenci se nečte; platná 1. kopie zůstane */
    ck ( "T small section", wav_decode_fm_merge_copies ( &fm, f1, f2, 8, out0, &merged ) == 1 && memcmp ( out0, d, 8 ) == 0,
         "copy past the end is not read" );
}

/**
 * @brief Přidá kus BSD: tón, krátký tapemark, 1 L, 258 B (ID + 256 B dat), součet, 1 L.
 * @param s Sekvence. @param id ID kusu. @param fill Hodnota dat. @param[out] first Index první datové půlvlny.
 */
static void add_bsd_chunk ( st_WAV_PULSE_SEQUENCE *s, uint16_t id, uint8_t fill, uint32_t *first ) {
    uint8_t c[258];
    c[0] = ( uint8_t ) id;
    c[1] = ( uint8_t ) ( id >> 8 );
    memset ( &c[2], fill, 256 );
    add_run ( s, 0, 2500 );   /* víc než oddělovač kopie */
    add_run ( s, 1, 20 ); add_run ( s, 0, 20 ); add_pulse ( s, 1 );
    add_section ( s, c, 258, first );
}

/** @brief Výsledek dekódování BSD. */
typedef struct {
    en_WAV_ANALYZER_ERROR err;  /**< návratový kód */
    st_MZF *mzf;                /**< MZF, nebo NULL */
    st_WAV_DECODE_RESULT br;    /**< výsledek těla */
    uint32_t rec;               /**< recovery_status */
    st_WAV_BSD_INFO info;       /**< počty kusů */
} st_BSDRES;

/** @brief Dekóduje kusy BSD od začátku sekvence (hlavička se předá přímo). */
static void bsd_dec ( const st_WAV_PULSE_SEQUENCE *s, int allow_partial, st_BSDRES *r ) {
    st_WAV_LEADER_INFO leader;
    st_MZF_HEADER h;
    uint32_t consumed = 0;
    memset ( r, 0, sizeof ( *r ) );
    memset ( &h, 0, sizeof ( h ) );
    h.ftype = 3;
    memset ( &leader, 0, sizeof ( leader ) );
    leader.avg_period_us = 259.0;
    r->err = wav_decode_bsd_decode_mzf ( s, &leader, &h, 0, allow_partial, &r->mzf, &r->br, &consumed, &r->rec, &r->info );
}

/** @brief Chybějící a poškozené kusy BSD. */
static void case_bsd ( st_WAV_PULSE_SEQUENCE *s ) {
    uint32_t f = 0, f2 = 0;
    st_BSDRES r;

    const char *n = "U1 missing chunk, recover";
    s->count = 0;
    add_bsd_chunk ( s, 0, 0x11, &f );
    add_bsd_chunk ( s, 2, 0x33, &f );
    add_bsd_chunk ( s, 0xFFFF, 0x44, &f );
    bsd_dec ( s, 1, &r );
    ck ( n, r.err == WAV_ANALYZER_OK && r.mzf, "decoded" );
    if ( r.mzf ) {
        ck ( n, r.mzf->body_size == 4 * 256, "4 chunks of data" );
        ck ( n, r.mzf->body[0] == 0x11 && r.mzf->body[256] == 0 && r.mzf->body[511] == 0 && r.mzf->body[512] == 0x33 && r.mzf->body[768] == 0x44,
             "gap filled with 00h, order kept" );
    }
    ck ( n, r.rec == WAV_RECOVERY_BSD_GAPS, "status BSD_GAPS" );
    ck ( n, r.info.gap_chunks == 1, "gap_chunks == 1" );
    mzf_free ( r.mzf ); free ( r.br.data );

    n = "U7 gaps and a missing terminator";
    ck ( n, strstr ( wav_recovery_status_string ( WAV_RECOVERY_BSD_INCOMPLETE | WAV_RECOVERY_BSD_GAPS ), "chunks missing" ) != NULL,
         "status text mentions the gaps" );

    n = "U2 missing chunk, no recovery";
    bsd_dec ( s, 0, &r );
    ck ( n, r.err != WAV_ANALYZER_OK && !r.mzf, "not kept without recovery" );
    mzf_free ( r.mzf ); free ( r.br.data );

    n = "U3 damaged ID with a checksum error stays in place";
    s->count = 0;
    add_bsd_chunk ( s, 0, 0x11, &f );
    add_bsd_chunk ( s, 1, 0x22, &f2 );
    add_bsd_chunk ( s, 2, 0x33, &f );
    add_bsd_chunk ( s, 0xFFFF, 0x44, &f );
    flip_bit ( s, f2 + 18 * 1 + 2 * 2 );   /* bit 5 vyššího bajtu ID: 0x0001 -> 0x2001 */
    bsd_dec ( s, 1, &r );
    ck ( n, r.err == WAV_ANALYZER_OK && r.mzf && r.mzf->body_size == 4 * 256, "all chunks kept" );
    if ( r.mzf ) ck ( n, r.mzf->body[256] == 0x22 && r.mzf->body[512] == 0x33, "order kept" );
    ck ( n, r.info.crc_errors == 1 && r.info.gap_chunks == 0, "1 CRC error, no gap" );
    mzf_free ( r.mzf ); free ( r.br.data );

    n = "U4 foreign record: an ID jump over the limit ends the file";
    s->count = 0;
    add_bsd_chunk ( s, 0, 0x11, &f );
    add_bsd_chunk ( s, 0x4101, 0x77, &f );
    bsd_dec ( s, 1, &r );
    ck ( n, r.err == WAV_ANALYZER_OK && r.mzf && r.mzf->body_size == 256, "foreign chunk not attached" );
    ck ( n, r.rec == WAV_RECOVERY_BSD_INCOMPLETE && r.info.gap_chunks == 0, "incomplete, no gap" );
    mzf_free ( r.mzf ); free ( r.br.data );

    n = "U6 foreign data with a checksum error and a distant ID ends the file";
    s->count = 0;
    add_bsd_chunk ( s, 0, 0x11, &f );
    add_bsd_chunk ( s, 0x4101, 0x77, &f2 );
    flip_bit ( s, f2 + 18 * 100 );
    bsd_dec ( s, 1, &r );
    ck ( n, r.err == WAV_ANALYZER_OK && r.mzf && r.mzf->body_size == 256, "foreign chunk not attached" );
    mzf_free ( r.mzf ); free ( r.br.data );

    n = "U5 ID going back ends the file";
    s->count = 0;
    add_bsd_chunk ( s, 0, 0x11, &f );
    add_bsd_chunk ( s, 1, 0x22, &f );
    add_bsd_chunk ( s, 0, 0x55, &f );
    bsd_dec ( s, 1, &r );
    ck ( n, r.err == WAV_ANALYZER_OK && r.mzf && r.mzf->body_size == 512, "next file not attached" );
    mzf_free ( r.mzf ); free ( r.br.data );
}

/** @brief Soubor s fsize 0 je správně bez těla, recover_header ho neoznačí. */
static void case_f ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *body ) {
    const char *n = "F fsize 0 with recover_header";
    st_WAV_FM_DECODE_OPTIONS o = { 1, 1 };
    uint8_t hdr0[128]; uint8_t b0[BODY_SIZE];
    uint32_t h1 = 0; st_RES r;
    ( void ) body;
    fill ( hdr0, b0, 0 );
    build_header_only ( s, hdr0, &h1 );
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.err == WAV_ANALYZER_OK && r.mzf, "decoded" );
    ck ( n, r.info.recovery_status == WAV_RECOVERY_NONE, "status NONE" );
    res_free ( &r );
}

/** @brief Nahrávka useknutá uvnitř hlavičky: nikdy se neuloží soubor z neúplné hlavičky. */
static void case_g ( st_WAV_PULSE_SEQUENCE *s, const uint8_t *hdr, const uint8_t *body ) {
    const char *n = "G truncated header, all options";
    st_WAV_FM_DECODE_OPTIONS o = { 1, 1 };
    st_POS p; st_RES r;
    build ( s, hdr, body, 0, &p );
    truncate_at ( s, p.hdr1 + 18 * 60 );
    if ( dec ( s, &o, &r ) ) { ck ( n, 0, "leader" ); return; }
    ck ( n, r.err == WAV_ANALYZER_ERROR_DECODE_DATA && !r.mzf, "DECODE_DATA, no MZF" );
    ck ( n, r.info.header_decoded == 0, "header_decoded 0" );
    ck ( n, r.info.body_error == WAV_ANALYZER_ERROR_DECODE_INCOMPLETE, "body_error INCOMPLETE" );
    res_free ( &r );
}

int main ( void ) {
    st_WAV_PULSE_SEQUENCE s;
    uint8_t hdr[128], body[BODY_SIZE];
    memset ( &s, 0, sizeof ( s ) );
    s.capacity = 300000;
    s.sample_rate = TEST_RATE;
    s.pulses = calloc ( s.capacity, sizeof ( st_WAV_PULSE ) );
    if ( !s.pulses ) return 1;

    fill ( hdr, body, BODY_SIZE );
    case_a ( &s, hdr, body );
    case_b ( &s, hdr, body );
    case_c ( &s, hdr, body );
    case_d ( &s, hdr, body );
    case_k ( &s, hdr, body );
    case_l ( &s, hdr, body );
    case_e ( &s, hdr, body );
    case_f ( &s, body );
    case_g ( &s, hdr, body );
    case_next ( &s, body );
    case_o ( &s, hdr, body );
    case_copy_diff ( &s, hdr, body );
    case_merge ( &s, hdr, body );
    case_merge_small ( &s );
    case_bsd ( &s );
    run_fsize ( &s, body, "H fsize corrupted up", 5120, 0, 1 );
    run_fsize ( &s, body, "I fsize corrupted down", 768, 0, 1 );
    run_fsize ( &s, body, "J fsize correct, header CRC bad", BODY_SIZE, 0, 0 );
    run_fsize ( &s, body, "M fsize corrupted to 0", 0, 0, 1 );
    run_fsize ( &s, body, "N fsize corrupted, silence after body", 5120, 1, 1 );

    free ( s.pulses );
    printf ( "\n=== FM Recovery Test: %s ===\n", g_fails ? "FAIL" : "PASS" );
    return g_fails ? 1 : 0;
}
