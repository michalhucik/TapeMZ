/**
 * @file   wav_decode_fm.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Implementace vrstvy 4a - FM dekodér.
 *
 * Dekóduje FM modulovaný signál (NORMAL/SINCLAIR/CPM/MZ-80B).
 * Algoritmus inspirován wavdec.c a Intercopy V10.2:
 * - práh SHORT/LONG = leader_avg * 1.5
 * - tapemark: N LONG + N SHORT pulzů
 * - bajty: MSB first, 1 stop bit (LONG)
 * - CRC: 16-bit popcount (počet jedničkových bitů), big-endian
 *
 * @par Licence:
 * GNU General Public License v3 (GPLv3)
 *
 * Copyright (C) 2026 Michal Hucik <hucik@ordoz.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/endianity/endianity.h"

#include "wav_decode_fm.h"
#include "wav_leader.h"


/**
 * @brief Minimální počty LONG a SHORT pulzů pro detekci tapemarku.
 *
 * Skutečné hodnoty jsou 40/40 (hlavička) a 20/20 (tělo).
 * Používáme nižší prahy kvůli možným chybám v signálu.
 */
#define TAPEMARK_LONG_MIN_LTM   36  /**< min LONG pro hlavičku */
#define TAPEMARK_SHORT_MIN_LTM  30  /**< min SHORT pro hlavičku */
#define TAPEMARK_LONG_MIN_STM   18  /**< min LONG pro tělo */
#define TAPEMARK_SHORT_MIN_STM  18  /**< min SHORT pro tělo */

/**
 * @brief Minimální počet pulzů leader tónu pro hledání body leaderu.
 *
 * Body leader je kratší než header leader (11000 vs. 22000),
 * takže používáme nižší práh.
 */
#define BODY_LEADER_MIN_PULSES  500


void wav_decode_fm_init (
    st_WAV_FM_DECODER *dec,
    const st_WAV_PULSE_SEQUENCE *seq,
    uint32_t start_pos,
    double leader_avg_us
) {
    if ( !dec || !seq ) return;

    dec->seq = seq;
    dec->pos = start_pos;
    dec->threshold_us = leader_avg_us * WAV_ANALYZER_FM_THRESHOLD_FACTOR;
    dec->crc_accumulator = 0;
}


/**
 * @brief Zjistí, zda je pulz na dané pozici LONG (bit 1).
 *
 * @param dec Kontext dekodéru.
 * @param pos Pozice pulzu v sekvenci.
 * @return 1 pokud je LONG, 0 pokud je SHORT.
 */
static inline int is_long_pulse ( const st_WAV_FM_DECODER *dec, uint32_t pos ) {
    return dec->seq->pulses[pos].duration_us >= dec->threshold_us;
}


en_WAV_ANALYZER_ERROR wav_decode_fm_find_tapemark (
    st_WAV_FM_DECODER *dec,
    en_WAV_TAPEMARK_TYPE type
) {
    if ( !dec || !dec->seq ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    uint32_t min_long = ( type == WAV_TAPEMARK_LONG ) ? TAPEMARK_LONG_MIN_LTM : TAPEMARK_LONG_MIN_STM;
    uint32_t min_short = ( type == WAV_TAPEMARK_LONG ) ? TAPEMARK_SHORT_MIN_LTM : TAPEMARK_SHORT_MIN_STM;

    const st_WAV_PULSE_SEQUENCE *seq = dec->seq;
    uint32_t pos = dec->pos;

    /*
     * Hledáme sekvenci: N LONG pulzů + N SHORT pulzů.
     * Každý pulz = 2 půl-periody (HIGH + LOW).
     * Počítáme půl-periody, ale minimum odpovídá celým pulzům.
     *
     * Tapemark: 40 LONG (= 80 LONG půl-period) + 40 SHORT (= 80 SHORT půl-period)
     * Minimum: 36 LONG pulzů = 72 LONG půl-period, 30 SHORT = 60 SHORT půl-period
     *
     * Ale protože obě fáze LONG pulzu (HIGH i LOW) jsou nad prahem
     * a obě fáze SHORT pulzu jsou pod prahem, stačí počítat
     * po sobě jdoucí půl-periody nad/pod prahem.
     */
    uint32_t min_long_hp = min_long * 2;    /* minimum v půl-periodách */
    uint32_t min_short_hp = min_short * 2;

    while ( pos < seq->count ) {
        /* přeskočíme SHORT půl-periody (zbytek leaderu) */
        while ( pos < seq->count && !is_long_pulse ( dec, pos ) ) {
            pos++;
        }

        /* počítáme LONG půl-periody */
        uint32_t long_hp_count = 0;
        while ( pos < seq->count && is_long_pulse ( dec, pos ) ) {
            long_hp_count++;
            pos++;
        }

        if ( long_hp_count < min_long_hp ) {
            /* nedostatek LONG pul-period - pokracujeme */
            continue;
        }

        /* počítáme SHORT půl-periody */
        uint32_t short_hp_count = 0;
        while ( pos < seq->count && !is_long_pulse ( dec, pos ) ) {
            short_hp_count++;
            pos++;
        }

        if ( short_hp_count >= min_short_hp ) {
            /* tapemark nalezen - pozice je za ním */
            dec->pos = pos;
            return WAV_ANALYZER_OK;
        }

        /* nedostatek SHORT půl-period - pokračujeme od aktuální pozice */
    }

    return WAV_ANALYZER_ERROR_DECODE_TAPEMARK;
}


en_WAV_ANALYZER_ERROR wav_decode_fm_read_bit (
    st_WAV_FM_DECODER *dec,
    int *bit
) {
    if ( !dec || !bit ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    /*
     * Každý bit na pásce je celý pulz = 2 půl-periody (HIGH + LOW).
     * Přečteme 2 po sobě jdoucí půl-periody a zprůměrujeme.
     * Průměr porovnáme s prahem.
     *
     * Inspirováno wavdec.c a Intercopy: "Měří 2 půl-periody -> průměr
     * -> porovnání s thresholdem"
     */
    if ( dec->pos + 1 >= dec->seq->count ) {
        return WAV_ANALYZER_ERROR_DECODE_INCOMPLETE;
    }

    double avg = ( dec->seq->pulses[dec->pos].duration_us +
                   dec->seq->pulses[dec->pos + 1].duration_us ) / 2.0;

    *bit = ( avg >= dec->threshold_us ) ? 1 : 0;

    /* aktualizace popcount */
    if ( *bit ) {
        dec->crc_accumulator++;
    }

    dec->pos += 2;
    return WAV_ANALYZER_OK;
}


en_WAV_ANALYZER_ERROR wav_decode_fm_read_byte (
    st_WAV_FM_DECODER *dec,
    uint8_t *byte
) {
    if ( !dec || !byte ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    uint8_t value = 0;

    /* 8 datových bitů, MSB first */
    for ( int i = 7; i >= 0; i-- ) {
        int bit;
        en_WAV_ANALYZER_ERROR err = wav_decode_fm_read_bit ( dec, &bit );
        if ( err != WAV_ANALYZER_OK ) return err;
        if ( bit ) {
            value |= ( 1 << i );
        }
    }

    /*
     * 1 stop bit (LONG) = 2 půl-periody.
     * Ověříme, ale neselháváme při chybě - některé záznamy
     * mají poškozený stop bit, ale data jsou v pořádku.
     */
    if ( dec->pos + 1 < dec->seq->count ) {
        /* přečteme stop bit (2 půl-periody), neakumulujeme do CRC */
        dec->pos += 2;
    }

    *byte = value;
    return WAV_ANALYZER_OK;
}


en_WAV_ANALYZER_ERROR wav_decode_fm_read_block (
    st_WAV_FM_DECODER *dec,
    uint8_t *data,
    uint32_t size
) {
    if ( !dec || !data ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    for ( uint32_t i = 0; i < size; i++ ) {
        en_WAV_ANALYZER_ERROR err = wav_decode_fm_read_byte ( dec, &data[i] );
        if ( err != WAV_ANALYZER_OK ) return err;
    }

    return WAV_ANALYZER_OK;
}


en_WAV_ANALYZER_ERROR wav_decode_fm_verify_checksum (
    st_WAV_FM_DECODER *dec,
    en_WAV_CRC_STATUS *crc_status,
    uint16_t *crc_stored,
    uint16_t *crc_computed
) {
    if ( !dec || !crc_status ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    /* uložíme vypočtený checksum */
    uint16_t computed = dec->crc_accumulator;
    if ( crc_computed ) *crc_computed = computed;

    /*
     * Přečteme 2 bajty checksumu z pásky (big-endian).
     * CRC bajty přečteme BEZ akumulace do popcountem,
     * proto resetujeme akumulátor a přečteme přímo.
     */
    dec->crc_accumulator = 0;

    uint8_t crc_hi, crc_lo;
    en_WAV_ANALYZER_ERROR err;

    err = wav_decode_fm_read_byte ( dec, &crc_hi );
    if ( err != WAV_ANALYZER_OK ) {
        dec->crc_accumulator = 0;
        *crc_status = WAV_CRC_NOT_AVAILABLE;
        return WAV_ANALYZER_OK;
    }

    err = wav_decode_fm_read_byte ( dec, &crc_lo );
    if ( err != WAV_ANALYZER_OK ) {
        dec->crc_accumulator = 0;
        *crc_status = WAV_CRC_NOT_AVAILABLE;
        return WAV_ANALYZER_OK;
    }

    uint16_t stored = ( ( uint16_t ) crc_hi << 8 ) | crc_lo;
    if ( crc_stored ) *crc_stored = stored;

    /* porovnáme */
    *crc_status = ( computed == stored ) ? WAV_CRC_OK : WAV_CRC_ERROR;

    /* resetujeme akumulátor pro další blok */
    dec->crc_accumulator = 0;

    return WAV_ANALYZER_OK;
}


/**
 * @brief Minimální počet SHORT půl-period v GAP mezi první a druhou kopií.
 *
 * GAP je sekvence krátkých pulzů oddělující dvě kopie bloku.
 * Sharp MZ typicky generuje 256 SHORT pulzů = 512 SHORT půl-period.
 */
#define COPY2_MIN_GAP_SHORT_PULSES  100

/**
 * @brief Maximální počet SHORT půl-period v GAP mezi kopiemi.
 *
 * Standardní mini-gap je 256 SHORT pulzů = 512 půl-period.
 * Limit 3000 dává dostatečnou rezervu pro zašuměné nahrávky,
 * ale zabraňuje záměně s LGAP dalšího záznamu (22000+ půl-period).
 * Bez tohoto limitu copy2 search omylem sežere další nahrávku
 * na vícesouborových páskách (TurboCopy, Intercopy).
 */
#define COPY2_MAX_GAP_SHORT_PULSES  3000

/**
 * @brief Maximální vzdálenost (v půl-periodách) pro hledání druhé kopie za první CRC.
 *
 * Omezuje prohledávanou oblast, aby se nehledalo příliš daleko
 * v případě poškozených záznamů. Reálná copy2 začíná do ~600
 * půl-period za CRC (2L term + 256S mini-gap + STM + 2L sync).
 */
#define COPY2_MAX_SEARCH_PULSES     5000

/**
 * @brief Maximální celková vzdálenost (v půl-periodách) od search_from
 *        po nalezený tapemark copy2 (včetně GAP + STM).
 *
 * Standardní vzdálenost: 2L term (4) + 256S mini-gap (512)
 * + STM (80) + 2L sync (4) = ~600 půl-period. Limit 2000
 * dává dostatečnou rezervu pro šum, ale zabraňuje záměně
 * s tapemarks dalšího záznamu (vzdálený 10000+ půl-period).
 */
#define COPY2_MAX_TOTAL_DISTANCE    2000


/**
 * @brief Pokusí se dekódovat data z druhé kopie bloku (Copy2).
 *
 * Sharp MZ zapisuje každý blok dvakrát (hlavička i tělo). Pokud
 * první kopie má CRC chybu, pokusíme se najít a dekódovat
 * druhou kopii. Mezi kopiemi je GAP (sekvence SHORT pulzů),
 * pak krátký tapemark (STM: 18+ LONG + 18+ SHORT) a sync.
 *
 * @param seq Sekvence pulzů.
 * @param search_from Pozice za CRC první kopie (začátek hledání).
 * @param threshold_us Práh SHORT/LONG v mikrosekundách.
 * @param expected_size Očekávaná velikost dat v bajtech.
 * @param[out] out_data Dekódovaná data (alokována na heapu). Může být NULL pokud chyba.
 * @param[out] out_crc_status CRC status druhé kopie.
 * @param[out] out_crc_stored Uložený checksum. Může být NULL.
 * @param[out] out_crc_computed Vypočtený checksum. Může být NULL.
 * @param[out] out_end_pos Pozice za koncem druhé kopie (za CRC bajty).
 * @return WAV_ANALYZER_OK při úspěšném dekódování,
 *         WAV_ANALYZER_ERROR_NO_LEADER pokud nebyla nalezena druhá kopie,
 *         WAV_ANALYZER_ERROR_DECODE_TAPEMARK pokud nebyl nalezen tapemark,
 *         WAV_ANALYZER_ERROR_DECODE_DATA při chybě dekódování.
 */
static en_WAV_ANALYZER_ERROR try_decode_copy2 (
    const st_WAV_PULSE_SEQUENCE *seq,
    uint32_t search_from,
    double threshold_us,
    uint32_t expected_size,
    uint8_t **out_data,
    en_WAV_CRC_STATUS *out_crc_status,
    uint16_t *out_crc_stored,
    uint16_t *out_crc_computed,
    uint32_t *out_end_pos
) {
    if ( !seq || !out_data || !out_crc_status ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    *out_data = NULL;
    *out_crc_status = WAV_CRC_NOT_AVAILABLE;
    if ( out_end_pos ) *out_end_pos = search_from;

    /*
     * === 1. Hledáme GAP (sekvenci SHORT pulzů) ===
     * Tolerance: max COPY2_MAX_SEARCH_PULSES pulzů od search_from.
     */
    uint32_t search_limit = search_from + COPY2_MAX_SEARCH_PULSES;
    if ( search_limit > seq->count ) search_limit = seq->count;

    uint32_t pos = search_from;
    uint32_t short_count = 0;

    /* přeskočíme případné LONG pulzy (zbytek CRC) */
    while ( pos < search_limit && seq->pulses[pos].duration_us >= threshold_us ) {
        pos++;
    }

    /* počítáme SHORT pulzy (GAP) */
    while ( pos < search_limit && seq->pulses[pos].duration_us < threshold_us ) {
        short_count++;
        pos++;
    }

    if ( short_count < COPY2_MIN_GAP_SHORT_PULSES ) {
        return WAV_ANALYZER_ERROR_NO_LEADER;
    }

    if ( short_count > COPY2_MAX_GAP_SHORT_PULSES ) {
        /*
         * GAP je příliš dlouhý - pravděpodobně jsme narazili
         * na LGAP dalšího záznamu, ne na mini-gap mezi kopiemi.
         */
        return WAV_ANALYZER_ERROR_NO_LEADER;
    }

    /* === 2. Hledáme STM tapemark (min 18 LONG + 18 SHORT) === */
    st_WAV_FM_DECODER dec;
    dec.seq = seq;
    dec.pos = pos;
    dec.threshold_us = threshold_us;
    dec.crc_accumulator = 0;

    en_WAV_ANALYZER_ERROR err = wav_decode_fm_find_tapemark ( &dec, WAV_TAPEMARK_SHORT );
    if ( err != WAV_ANALYZER_OK ) {
        return WAV_ANALYZER_ERROR_DECODE_TAPEMARK;
    }

    /*
     * Ověříme, že tapemark nebyl nalezen příliš daleko od search_from.
     * Reálný copy2 tapemark je do ~600 půl-period za CRC.
     * Pokud je dále, pravděpodobně jsme našli tapemark dalšího záznamu.
     */
    if ( dec.pos - search_from > COPY2_MAX_TOTAL_DISTANCE ) {
        return WAV_ANALYZER_ERROR_NO_LEADER;
    }

    /* === 3. Přeskočíme sync (max 4 LONG pulzy) === */
    {
        int sync_skip = 0;
        while ( dec.pos < seq->count &&
                seq->pulses[dec.pos].duration_us >= dec.threshold_us &&
                sync_skip < 4 ) {
            dec.pos++;
            sync_skip++;
        }
    }

    /* === 4. Dekódujeme data === */
    uint8_t *data = ( uint8_t* ) malloc ( expected_size );
    if ( !data ) return WAV_ANALYZER_ERROR_ALLOC;

    dec.crc_accumulator = 0;
    err = wav_decode_fm_read_block ( &dec, data, expected_size );
    if ( err != WAV_ANALYZER_OK ) {
        free ( data );
        return WAV_ANALYZER_ERROR_DECODE_DATA;
    }

    /* === 5. Ověříme CRC === */
    wav_decode_fm_verify_checksum ( &dec, out_crc_status, out_crc_stored, out_crc_computed );

    *out_data = data;
    if ( out_end_pos ) *out_end_pos = dec.pos;

    return WAV_ANALYZER_OK;
}


en_WAV_ANALYZER_ERROR wav_decode_fm_decode_mzf (
    const st_WAV_PULSE_SEQUENCE *seq,
    const st_WAV_LEADER_INFO *leader,
    st_MZF **out_mzf,
    st_WAV_DECODE_RESULT *out_header_result,
    st_WAV_DECODE_RESULT *out_body_result
) {
    if ( !seq || !leader || !out_mzf ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    *out_mzf = NULL;

    /* inicializujeme dekodér za koncem leader tónu */
    st_WAV_FM_DECODER dec;
    uint32_t data_start = leader->start_index + leader->pulse_count;
    wav_decode_fm_init ( &dec, seq, data_start, leader->avg_period_us );

    /* === 1. Detekce tapemarku hlavicky (LTM) === */
    en_WAV_ANALYZER_ERROR err = wav_decode_fm_find_tapemark ( &dec, WAV_TAPEMARK_LONG );
    if ( err != WAV_ANALYZER_OK ) {
        return WAV_ANALYZER_ERROR_DECODE_TAPEMARK;
    }

    /*
     * === 2. Sync blok (2L = max 4 LONG pul-periody) ===
     * Po tapemarku preskocime LONG pul-periody (sync/tapemark koncovka).
     */
    {
        int sync_skip = 0;
        while ( dec.pos < seq->count && is_long_pulse ( &dec, dec.pos ) && sync_skip < 4 ) {
            dec.pos++;
            sync_skip++;
        }
    }

    /* === 3. Dekódování 128B hlavičky === */
    uint32_t header_pulse_start = dec.pos;
    uint8_t header_raw[MZF_HEADER_SIZE];
    dec.crc_accumulator = 0;

    err = wav_decode_fm_read_block ( &dec, header_raw, MZF_HEADER_SIZE );
    if ( err != WAV_ANALYZER_OK ) {
        return WAV_ANALYZER_ERROR_DECODE_DATA;
    }

    /* === 4. Verifikace CRC hlavičky === */
    en_WAV_CRC_STATUS hdr_crc_status;
    uint16_t hdr_crc_stored = 0, hdr_crc_computed = 0;
    err = wav_decode_fm_verify_checksum ( &dec, &hdr_crc_status, &hdr_crc_stored, &hdr_crc_computed );

    if ( hdr_crc_status == WAV_CRC_ERROR ) {
        fprintf ( stderr, "WARNING: header CRC mismatch (stored=0x%04X, computed=0x%04X)\n",
                  hdr_crc_stored, hdr_crc_computed );
    }

    /*
     * === 4b. Copy2 - pokus o dekódování druhé kopie hlavičky ===
     * Hlavička je jen 128B a zřídka se poškodí, ale pro úplnost
     * zkusíme druhou kopii při CRC chybě.
     */
    int hdr_copy2_used = 0;

    if ( hdr_crc_status == WAV_CRC_ERROR ) {
        uint8_t *hdr_copy2_data = NULL;
        en_WAV_CRC_STATUS hdr_c2_crc;
        uint16_t hdr_c2_stored = 0, hdr_c2_computed = 0;
        uint32_t hdr_c2_end = 0;

        en_WAV_ANALYZER_ERROR c2err = try_decode_copy2 (
            seq, dec.pos, dec.threshold_us, MZF_HEADER_SIZE,
            &hdr_copy2_data, &hdr_c2_crc, &hdr_c2_stored, &hdr_c2_computed, &hdr_c2_end
        );

        if ( c2err == WAV_ANALYZER_OK && hdr_c2_crc == WAV_CRC_OK ) {
            memcpy ( header_raw, hdr_copy2_data, MZF_HEADER_SIZE );
            hdr_crc_status = WAV_CRC_OK;
            hdr_crc_stored = hdr_c2_stored;
            hdr_crc_computed = hdr_c2_computed;
            hdr_copy2_used = 1;
            dec.pos = hdr_c2_end;
        }

        free ( hdr_copy2_data );
    }

    if ( out_header_result ) {
        out_header_result->format = WAV_TAPE_FORMAT_NORMAL;
        out_header_result->data = ( uint8_t* ) malloc ( MZF_HEADER_SIZE );
        if ( out_header_result->data ) {
            memcpy ( out_header_result->data, header_raw, MZF_HEADER_SIZE );
        }
        out_header_result->data_size = MZF_HEADER_SIZE;
        out_header_result->crc_status = hdr_crc_status;
        out_header_result->crc_stored = hdr_crc_stored;
        out_header_result->crc_computed = hdr_crc_computed;
        out_header_result->pulse_start = header_pulse_start;
        out_header_result->pulse_end = dec.pos;
        out_header_result->is_header = 1;
        out_header_result->copy2_used = hdr_copy2_used;
    }

    /* parsujeme hlavičku */
    st_MZF_HEADER mzf_header;
    memcpy ( &mzf_header, header_raw, MZF_HEADER_SIZE );
    mzf_header_items_correction ( &mzf_header );

    uint16_t body_size = mzf_header.fsize;

    /*
     * === 5. Hledání body leaderu ===
     * Mezi hlavičkou a tělem je krátký GAP (256 SHORT + kopie hlavičky
     * v plném formátu, nebo přímo krátký leader).
     *
     * Hledáme další leader nebo tapemark.
     * Pokud je plný formát (s kopiemi), přeskočíme kopii hlavičky.
     */

    /*
     * Přeskočíme případné LONG pulzy (2L) a krátký blok (256S).
     * Pak hledáme kratký tapemark (STM).
     * Pokud je tam kopie hlavičky, přeskočíme ji hledáním dalšího leaderu.
     */

    /* hledáme krátký tapemark (STM) */
    err = wav_decode_fm_find_tapemark ( &dec, WAV_TAPEMARK_SHORT );
    if ( err != WAV_ANALYZER_OK ) {
        /*
         * Pokud nenajdeme krátký tapemark, zkusíme najít leader.
         * To se stane u formátu s kopií, kde je mezi tapemarks
         * kopie hlavičky.
         */
        st_WAV_LEADER_INFO body_leader;
        err = wav_leader_detect ( seq, dec.pos, BODY_LEADER_MIN_PULSES,
                                  WAV_ANALYZER_DEFAULT_TOLERANCE, &body_leader );
        if ( err == WAV_ANALYZER_OK ) {
            dec.pos = body_leader.start_index + body_leader.pulse_count;
            /* rekalibrujeme práh z body leaderu */
            dec.threshold_us = body_leader.avg_period_us * WAV_ANALYZER_FM_THRESHOLD_FACTOR;
            err = wav_decode_fm_find_tapemark ( &dec, WAV_TAPEMARK_SHORT );
        }

        if ( err != WAV_ANALYZER_OK ) {
            /* stále nenalezen - nemáme tělo */
            if ( body_size == 0 ) {
                /* MZF bez těla - to je OK */
                goto create_mzf_no_body;
            }
            return WAV_ANALYZER_ERROR_DECODE_TAPEMARK;
        }
    }

    /* === 6. Sync blok za tapemarkem těla (2L = max 4 půl-periody) === */
    {
        int sync_skip = 0;
        while ( dec.pos < seq->count && is_long_pulse ( &dec, dec.pos ) && sync_skip < 4 ) {
            dec.pos++;
            sync_skip++;
        }
    }

    /* === 7. Dekódování těla === */
    uint32_t body_pulse_start = dec.pos;
    uint8_t *body_data = NULL;

    if ( body_size > 0 ) {
        body_data = ( uint8_t* ) malloc ( body_size );
        if ( !body_data ) return WAV_ANALYZER_ERROR_ALLOC;

        dec.crc_accumulator = 0;
        err = wav_decode_fm_read_block ( &dec, body_data, body_size );
        if ( err != WAV_ANALYZER_OK ) {
            free ( body_data );
            return WAV_ANALYZER_ERROR_DECODE_DATA;
        }

        /* === 8. Verifikace CRC těla === */
        en_WAV_CRC_STATUS body_crc_status;
        uint16_t body_crc_stored = 0, body_crc_computed = 0;
        wav_decode_fm_verify_checksum ( &dec, &body_crc_status, &body_crc_stored, &body_crc_computed );

        if ( body_crc_status == WAV_CRC_ERROR ) {
            fprintf ( stderr, "WARNING: body CRC mismatch (stored=0x%04X, computed=0x%04X)\n",
                      body_crc_stored, body_crc_computed );
        }

        /*
         * === 8b. Copy2 - pokus o dekódování druhé kopie těla ===
         * Sharp MZ zapisuje každý blok dvakrát. Pokud první kopie
         * má CRC chybu, zkusíme druhou kopii. Vždy se ale pokusíme
         * najít konec druhé kopie pro aktualizaci consumed_until.
         */
        int body_copy2_used = 0;
        uint32_t body_consumed_end = dec.pos;

        {
            uint8_t *copy2_data = NULL;
            en_WAV_CRC_STATUS copy2_crc;
            uint16_t copy2_stored = 0, copy2_computed = 0;
            uint32_t copy2_end = 0;

            en_WAV_ANALYZER_ERROR c2err = try_decode_copy2 (
                seq, dec.pos, dec.threshold_us, body_size,
                &copy2_data, &copy2_crc, &copy2_stored, &copy2_computed, &copy2_end
            );

            if ( c2err == WAV_ANALYZER_OK ) {
                /* aktualizujeme consumed pozici za druhou kopii */
                body_consumed_end = copy2_end;

                if ( body_crc_status == WAV_CRC_ERROR && copy2_crc == WAV_CRC_OK ) {
                    /* nahradíme data z první kopie daty z druhé */
                    free ( body_data );
                    body_data = copy2_data;
                    copy2_data = NULL;
                    body_crc_status = WAV_CRC_OK;
                    body_crc_stored = copy2_stored;
                    body_crc_computed = copy2_computed;
                    body_copy2_used = 1;
                }
            }
            free ( copy2_data );
        }

        if ( out_body_result ) {
            out_body_result->format = WAV_TAPE_FORMAT_NORMAL;
            out_body_result->data = ( uint8_t* ) malloc ( body_size );
            if ( out_body_result->data ) {
                memcpy ( out_body_result->data, body_data, body_size );
            }
            out_body_result->data_size = body_size;
            out_body_result->crc_status = body_crc_status;
            out_body_result->crc_stored = body_crc_stored;
            out_body_result->crc_computed = body_crc_computed;
            out_body_result->pulse_start = body_pulse_start;
            out_body_result->pulse_end = body_consumed_end;
            out_body_result->is_header = 0;
            out_body_result->copy2_used = body_copy2_used;
        }
    }

    /* === 9. Sestavíme MZF strukturu === */
create_mzf_no_body:
    ;

    st_MZF *mzf = ( st_MZF* ) calloc ( 1, sizeof ( st_MZF ) );
    if ( !mzf ) {
        free ( body_data );
        return WAV_ANALYZER_ERROR_ALLOC;
    }

    memcpy ( &mzf->header, &mzf_header, sizeof ( st_MZF_HEADER ) );
    mzf->body = body_data;
    mzf->body_size = body_size;

    *out_mzf = mzf;
    return WAV_ANALYZER_OK;
}
