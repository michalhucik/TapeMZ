/**
 * @file   wav_decode_fm.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.3.0
 * @brief  Implementace vrstvy 4a - FM dekodér.
 *
 * Dekóduje FM modulovaný signál (NORMAL/SINCLAIR/CPM/MZ-80B).
 * Algoritmus inspirován wavdec.c a Intercopy V10.2:
 * - práh SHORT/LONG = leader_avg * 1.6 (WAV_ANALYZER_FM_THRESHOLD_FACTOR)
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
/** @brief Nejvíc dlouhých period tapemarku těla při hledání po periodách (STM má 20, LTM 40). */
#define BODY_STM_MAX_LONG       30

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


/**
 * @brief Průměr půlvlny v dvojici začínající na idx (us).
 * @param seq Sekvence. @param idx Index první půlvlny dvojice (idx + 1 < count).
 * @return Průměrná délka půlvlny dvojice.
 */
static inline double pair_avg_us ( const st_WAV_PULSE_SEQUENCE *seq, uint32_t idx ) {
    return ( seq->pulses[idx].duration_us + seq->pulses[idx + 1].duration_us ) / 2.0;
}


en_WAV_ANALYZER_ERROR wav_decode_fm_find_tapemark_period (
    st_WAV_FM_DECODER *dec,
    uint32_t min_long,
    uint32_t min_short,
    uint32_t max_long,
    uint32_t max_skip
) {
    if ( !dec || !dec->seq ) return WAV_ANALYZER_ERROR_INVALID_PARAM;
    const st_WAV_PULSE_SEQUENCE *seq = dec->seq;
    uint32_t q = dec->pos;
    while ( q + 1 < seq->count ) {
        while ( q + 1 < seq->count && pair_avg_us ( seq, q ) < dec->threshold_us ) q += 2;
        /* tapemark musí začít nejvýš max_skip period od začátku hledání */
        if ( max_skip && ( q - dec->pos ) / 2 > max_skip ) return WAV_ANALYZER_ERROR_DECODE_TAPEMARK;
        uint32_t n_long = 0;
        while ( q + 1 < seq->count && pair_avg_us ( seq, q ) >= dec->threshold_us ) {
            n_long++;
            q += 2;
        }
        if ( n_long < min_long ) continue;
        if ( max_long && n_long > max_long ) return WAV_ANALYZER_ERROR_DECODE_TAPEMARK;
        uint32_t n_short = 0;
        while ( q + 1 < seq->count && pair_avg_us ( seq, q ) < dec->threshold_us ) {
            n_short++;
            q += 2;
        }
        if ( n_short < min_short ) continue;
        /* synchronizace: nejvýš 2 dlouhé periody (2L) */
        for ( int sk = 0; sk < 2 && q + 1 < seq->count && pair_avg_us ( seq, q ) >= dec->threshold_us; sk++ ) q += 2;
        dec->pos = q;
        return WAV_ANALYZER_OK;
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
 * @brief Nejkratší oddělovač druhé kopie (půlvlny SHORT).
 *
 * ROM MZ-800 zapisuje mezi kopie 256 SHORT pulzů (512 půlvln) a při čtení
 * kopie čeká na 100 souvislých nul (@RINTR v ROM), tedy 200 půlvln. V datech FM je nejdelší
 * běh SHORT 16 půlvln (8 nul a start bit LONG), takže kratší běh je data.
 */
#define COPY2_MIN_SEP_HALVES    200

/**
 * @brief Nejdelší oddělovač druhé kopie (půlvlny SHORT).
 *
 * Delší běh SHORT je tón další části záznamu (GAP těla nebo další nahrávka),
 * hledání kopie tím končí.
 */
#define COPY2_MAX_SEP_HALVES    3000

/** @brief Rezerva okna hledání kopie za dvojnásobkem nominální délky sekce (půlvlny). */
#define COPY2_SEARCH_MARGIN     4000


/**
 * @brief Pokusí se dekódovat druhou kopii bloku (Copy2) podle zápisu ROM.
 *
 * ROM zapíše za checksum 1. kopie 1 LONG, 256 SHORT a hned druhou kopii
 * (každý bajt start bit LONG a 8 bitů, pak checksum a 1 LONG), bez tónu
 * a tapemarku (kód ROM; záznam ROM SAVE z emulátoru; generátor mzcmt_fm). Funkce hledá od začátku
 * 1. kopie běh SHORT dlouhý COPY2_MIN_SEP_HALVES až COPY2_MAX_SEP_HALVES
 * půlvln (kratší jsou data, delší tón další části), za ním přeskočí start
 * bit (2 půlvlny) a přečte blok a checksum. Kandidát s chybou CRC se
 * přeskočí a zkusí se další běh v okně (šum v 1. kopii umí vytvořit
 * falešný dlouhý běh SHORT).
 *
 * Hledá se od začátku sekce, ne od konce 1. kopie: výpadek v 1. kopii
 * posune čtení přes oddělovač do 2. kopie. Za oddělovačem se nehledá
 * tapemark, ROM ho před kopii nezapisuje.
 *
 * @param seq Sekvence pulzů.
 * @param section_start První datová půlvlna 1. kopie sekce (za start bitem).
 * @param threshold_us Práh SHORT/LONG v mikrosekundách.
 * @param expected_size Velikost dat sekce v bajtech (bez checksumu).
 * @param[out] out_data Data 2. kopie s CRC OK (alokována, uvolní volající),
 *             jinak NULL.
 * @param[out] out_crc_status WAV_CRC_OK (kopie s platným checksumem),
 *             WAV_CRC_ERROR (kopie strukturálně nalezena, ale checksum
 *             nesedí; data se nevracejí), WAV_CRC_NOT_AVAILABLE (nenalezena).
 * @param[out] out_crc_stored Uložený checksum. Může být NULL.
 * @param[out] out_crc_computed Vypočtený checksum. Může být NULL.
 * @param[out] out_end_pos Pozice za checksumem 2. kopie (i při WAV_CRC_ERROR,
 *             aby se oddělovač nehlásil jako leader). Může být NULL.
 * @return WAV_ANALYZER_OK, když byla kopie nalezena (CRC viz out_crc_status);
 *         WAV_ANALYZER_ERROR_NO_LEADER, když v okně není žádný oddělovač
 *         s čitelnou kopií; WAV_ANALYZER_ERROR_ALLOC; WAV_ANALYZER_ERROR_INVALID_PARAM.
 */
en_WAV_ANALYZER_ERROR wav_decode_fm_try_copy2 (
    const st_WAV_PULSE_SEQUENCE *seq,
    uint32_t section_start,
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
    if ( out_end_pos ) *out_end_pos = section_start;

    /* okno: 2x nominální délka sekce (data + checksum, 18 půlvln na bajt) + rezerva */
    uint64_t sec_halves = ( ( uint64_t ) expected_size + 2 ) * 18 + 2;
    uint64_t lim64 = ( uint64_t ) section_start + 2 * sec_halves + COPY2_SEARCH_MARGIN;
    uint32_t limit = ( lim64 > seq->count ) ? seq->count : ( uint32_t ) lim64;

    int structural_found = 0;       /* kopie s chybou CRC (pro konec spotřebovaného úseku) */
    uint32_t structural_end = section_start;
    uint16_t structural_stored = 0, structural_computed = 0;

    uint32_t pos = section_start;
    while ( pos < limit ) {
        if ( seq->pulses[pos].duration_us >= threshold_us ) {
            pos++;
            continue;
        }
        uint32_t run_start = pos;
        while ( pos < seq->count && seq->pulses[pos].duration_us < threshold_us ) pos++;
        uint32_t run = pos - run_start;
        if ( run < COPY2_MIN_SEP_HALVES ) continue;          /* krátký běh SHORT = data */
        if ( run > COPY2_MAX_SEP_HALVES ) break;             /* tón další části */

        /* start bit prvního bajtu kopie = 1 LONG (2 půlvlny) */
        if ( pos + 1 >= seq->count ) break;
        st_WAV_FM_DECODER dec;
        dec.seq = seq;
        dec.pos = pos + 2;
        dec.threshold_us = threshold_us;
        dec.crc_accumulator = 0;

        uint8_t *data = ( uint8_t* ) malloc ( expected_size ? expected_size : 1 );
        if ( !data ) return WAV_ANALYZER_ERROR_ALLOC;
        if ( wav_decode_fm_read_block ( &dec, data, expected_size ) != WAV_ANALYZER_OK ) {
            free ( data );
            continue;
        }
        en_WAV_CRC_STATUS st;
        uint16_t cs = 0, cc = 0;
        wav_decode_fm_verify_checksum ( &dec, &st, &cs, &cc );
        if ( st == WAV_CRC_OK ) {
            *out_data = data;
            *out_crc_status = WAV_CRC_OK;
            if ( out_crc_stored ) *out_crc_stored = cs;
            if ( out_crc_computed ) *out_crc_computed = cc;
            if ( out_end_pos ) *out_end_pos = dec.pos;
            return WAV_ANALYZER_OK;
        }
        free ( data );
        if ( !structural_found ) {
            structural_found = 1;
            structural_end = dec.pos;
            structural_stored = cs;
            structural_computed = cc;
        }
        /* chyba CRC: další kandidát (např. šum v 1. kopii) */
    }

    if ( structural_found ) {
        *out_crc_status = WAV_CRC_ERROR;
        if ( out_crc_stored ) *out_crc_stored = structural_stored;
        if ( out_crc_computed ) *out_crc_computed = structural_computed;
        if ( out_end_pos ) *out_end_pos = structural_end;
        return WAV_ANALYZER_OK;
    }
    return WAV_ANALYZER_ERROR_NO_LEADER;
}


/**
 * @brief Tapemark po celých periodách v obou fázích dvojic půlvln.
 *
 * Konec leaderu může ležet na sudé i liché půlvlně; při nesouměrných
 * půlvlnách jen jedna fáze dvojic dá správné bity. Zkusí fázi od from
 * a od from + 1; za tapemarkem přečte data_size bajtů a checksum a vybere
 * fázi s platným checksumem. Bez platného checksumu vrátí první fázi,
 * ve které se tapemark našel (data a CRC pak vyhodnotí volající).
 *
 * @param dec Kontext dekodéru (seq, threshold_us); nemění se.
 * @param from Začátek hledání (pulz). @param min_long, min_short, max_long Jako
 *        wav_decode_fm_find_tapemark_period(). @param data_size Bajtů dat za
 *        tapemarkem (0 = jen najít tapemark ve fázi from).
 * @param[out] pos Pozice první půlvlny dat (za synchronizací).
 * @return WAV_ANALYZER_OK, nebo WAV_ANALYZER_ERROR_DECODE_TAPEMARK, když se
 *         tapemark nenašel v žádné fázi.
 */
static en_WAV_ANALYZER_ERROR find_tapemark_period_phase ( const st_WAV_FM_DECODER *dec, uint32_t from,
                                                         uint32_t min_long, uint32_t min_short, uint32_t max_long,
                                                         uint32_t data_size, uint32_t *pos ) {
    int found = 0;
    for ( uint32_t phase = 0; phase < 2; phase++ ) {
        st_WAV_FM_DECODER t = *dec;
        t.pos = from + phase;
        if ( wav_decode_fm_find_tapemark_period ( &t, min_long, min_short, max_long, 0 ) != WAV_ANALYZER_OK ) continue;
        if ( !found ) *pos = t.pos;
        found = 1;
        if ( data_size == 0 ) break;
        uint8_t *probe = ( uint8_t* ) malloc ( data_size );
        if ( !probe ) break;
        st_WAV_FM_DECODER p = t;
        p.crc_accumulator = 0;
        en_WAV_CRC_STATUS st = WAV_CRC_ERROR;
        uint16_t stored = 0, computed = 0;
        if ( wav_decode_fm_read_block ( &p, probe, data_size ) == WAV_ANALYZER_OK ) {
            wav_decode_fm_verify_checksum ( &p, &st, &stored, &computed );
        }
        free ( probe );
        if ( st == WAV_CRC_OK ) {
            *pos = t.pos;
            break;
        }
    }
    return found ? WAV_ANALYZER_OK : WAV_ANALYZER_ERROR_DECODE_TAPEMARK;
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
    int sync_done = 0;
    if ( err != WAV_ANALYZER_OK ) {
        /*
         * Nesouměrné půlvlny (generátor Intercopy NORMAL 2800/3200 podle kódu programu:
         * dlouhá půlvlna 7 a 10 vzorků při 44,1 kHz) se po kvantizaci dostanou pod práh
         * po půlvlnách; podle celých period se tapemark najde. Synchronizaci přeskočí.
         */
        uint32_t found = 0;
        if ( find_tapemark_period_phase ( &dec, data_start, TAPEMARK_LONG_MIN_LTM, TAPEMARK_SHORT_MIN_LTM, 0,
                                          MZF_HEADER_SIZE, &found ) != WAV_ANALYZER_OK ) {
            return WAV_ANALYZER_ERROR_DECODE_TAPEMARK;
        }
        dec.pos = found;
        sync_done = 1;
    }

    /*
     * === 2. Sync blok (2L = max 4 LONG pul-periody) ===
     * Po tapemarku preskocime LONG pul-periody (sync/tapemark koncovka).
     */
    if ( !sync_done ) {
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
     * === 4b. Copy2 - druhá kopie hlavičky při CRC chybě ===
     * Hledá se od začátku 1. kopie (wav_decode_fm_try_copy2).
     */
    int hdr_copy2_used = 0;

    if ( hdr_crc_status == WAV_CRC_ERROR ) {
        uint8_t *hdr_copy2_data = NULL;
        en_WAV_CRC_STATUS hdr_c2_crc;
        uint16_t hdr_c2_stored = 0, hdr_c2_computed = 0;
        uint32_t hdr_c2_end = 0;

        en_WAV_ANALYZER_ERROR c2err = wav_decode_fm_try_copy2 (
            seq, header_pulse_start, dec.threshold_us, MZF_HEADER_SIZE,
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
    mzf_header_swap_endianity ( &mzf_header );   /* 11h (fname.terminator) zůstává podle pásky */

    uint16_t body_size = mzf_header.fsize;

    /*
     * Deklarace před prvním skokem na create_mzf_no_body - goto nesmí
     * přeskočit inicializaci (dříve zde vznikal free() neurčeného ukazatele).
     */
    uint8_t *body_data = NULL;
    uint32_t body_pulse_start = 0;

    /* soubor bez těla: CRC těla se neověřuje (volající výsledek nuluje, 0 = WAV_CRC_OK) */
    if ( body_size == 0 && out_body_result ) {
        out_body_result->crc_status = WAV_CRC_NOT_AVAILABLE;
    }

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
    uint32_t stm_from = dec.pos;
    err = wav_decode_fm_find_tapemark ( &dec, WAV_TAPEMARK_SHORT );
    sync_done = 0;
    if ( err != WAV_ANALYZER_OK ) {
        /*
         * Podle celých period (nesouměrné půlvlny, viz LTM). Delší běh než
         * BODY_STM_MAX_LONG period je LTM další hlavičky, ne tapemark těla.
         */
        uint32_t found = 0;
        err = find_tapemark_period_phase ( &dec, stm_from, TAPEMARK_LONG_MIN_STM, TAPEMARK_SHORT_MIN_STM,
                                           BODY_STM_MAX_LONG, body_size, &found );
        if ( err == WAV_ANALYZER_OK ) {
            dec.pos = found;
            sync_done = 1;
        } else {
            dec.pos = stm_from;
        }
    }
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
    if ( !sync_done ) {
        int sync_skip = 0;
        while ( dec.pos < seq->count && is_long_pulse ( &dec, dec.pos ) && sync_skip < 4 ) {
            dec.pos++;
            sync_skip++;
        }
    }

    /* === 7. Dekódování těla === */
    body_pulse_start = dec.pos;

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
         * === 8b. Copy2 - druhá kopie těla ===
         * ROM zapisuje kopii bez tónu a tapemarku (wav_decode_fm_try_copy2). Při CRC
         * chybě 1. kopie se použije 2. kopie s platným checksumem. Konec 2. kopie
         * se hledá vždy (i s chybou CRC), aby se oddělovač nehlásil jako leader.
         */
        int body_copy2_used = 0;
        uint32_t body_consumed_end = dec.pos;

        {
            uint8_t *copy2_data = NULL;
            en_WAV_CRC_STATUS copy2_crc;
            uint16_t copy2_stored = 0, copy2_computed = 0;
            uint32_t copy2_end = 0;

            en_WAV_ANALYZER_ERROR c2err = wav_decode_fm_try_copy2 (
                seq, body_pulse_start, dec.threshold_us, body_size,
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
