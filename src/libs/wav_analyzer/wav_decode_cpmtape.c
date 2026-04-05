/**
 * @file   wav_decode_cpmtape.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Implementace vrstvy 4e2 - CPM-TAPE dekoder (Pezik/MarVan).
 *
 * Dekoduje manchestersky modulovany signal CPM-TAPE z WAV nahravky.
 *
 * Algoritmus dekodovani:
 * 1. Detekce sync - hledame par velmi dlouhych pulzu (5*T/2) predchazeny
 *    pilotem s charakteristickym vzorem (smes T/2 a 3T/2 pulzu)
 * 2. Kalibrace bitove periody T z kratkych pilotních pulzu (T/2)
 * 3. Manchester cteni bitu: uroven signalu na zacatku bitove periody
 *    urcuje hodnotu bitu (HIGH = 1, LOW = 0)
 * 4. Bajty se ctou LSB first (8 bitu, bez stop bitu)
 * 5. Checksum: 16-bit soucet bajtu (vcetne flag bajtu), low byte first
 * 6. Posledni bit body bloku se rekonstruuje z checksumu
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
#include <math.h>

#include "libs/mzf/mzf.h"
#include "libs/endianity/endianity.h"

#include "wav_decode_cpmtape.h"


/**
 * @brief Maximalni povolena odchylka mezi dvema sync pul-periodami.
 *
 * Sync bit 1 = HIGH(5T/2) + LOW(5T/2). Obe pul-periody by mely byt
 * priblizne stejne. Povolujeme 40% relativni odchylku.
 */
#define SYNC_PAIR_TOLERANCE     0.4

/**
 * @brief Nasobitel pro rozliseni SHORT/LONG pulzu v pilotu.
 *
 * Pulzy < sync_avg * PILOT_SHORT_FACTOR jsou SHORT (T/2),
 * pulzy >= nez tato hranice a < sync jsou LONG (3T/2).
 */
#define PILOT_SHORT_FACTOR      0.5


/* =========================================================================
 *  Interni kontext dekoderu
 * ========================================================================= */

/**
 * @brief Kontext Manchester dekoderu pro CPM-TAPE.
 *
 * Sleduje pozici v sekvenci pulzu s presnosti na cas uvnitr
 * aktualniho pulzu (time-budget pristup). Tím spravne dekoduje
 * i pulzy, ktere zasahuji pres hranice bitu (RLE slevani).
 *
 * @par Invarianty:
 * - pos < seq->count (nebo na konci)
 * - time_offset_us >= 0 a < aktualni pulz duration
 * - bit_period_us > 0
 */
typedef struct {
    const st_WAV_PULSE_SEQUENCE *seq;   /**< zdrojova sekvence pulzu */
    uint32_t pos;                       /**< aktualni index v sekvenci */
    double time_offset_us;              /**< cas uplynuly v aktualnim pulzu (us) */
    double bit_period_us;               /**< perioda jednoho bitu (us) = T */
    uint16_t checksum;                  /**< akumulator 16-bit souctu */
    int bit1_level;                     /**< uroven signalu odpovídající bitu 1
                                             (1 pri normalni polarite, 0 pri invertovane) */
} st_CPMTAPE_DEC;


/* =========================================================================
 *  Pomocne funkce - cas a pozice
 * ========================================================================= */

/**
 * @brief Posune pozici dekoderu o zadany cas (us).
 *
 * Konzumuje pulzy a aktualizuje time_offset. Pokud cas presahuje
 * zbyvajici delku aktualniho pulzu, presune se na dalsi.
 *
 * @param dec Kontext dekoderu.
 * @param duration_us Cas k posunuti (us).
 */
static void cpmtape_advance ( st_CPMTAPE_DEC *dec, double duration_us ) {
    while ( duration_us > 0.1 && dec->pos < dec->seq->count ) {
        double remaining = dec->seq->pulses[dec->pos].duration_us - dec->time_offset_us;
        if ( duration_us < remaining - 0.1 ) {
            /* cas se vejde do aktualniho pulzu */
            dec->time_offset_us += duration_us;
            return;
        }
        /* konzumujeme zbytek aktualniho pulzu a pokracujeme dalsim */
        duration_us -= remaining;
        dec->pos++;
        dec->time_offset_us = 0.0;
    }
}


/* =========================================================================
 *  Pomocne funkce - cteni bitu a bajtu
 * ========================================================================= */

/**
 * @brief Precte jeden Manchester bit.
 *
 * V manchesterskem kodovani urcuje uroven signalu v prvni polovine
 * bitove periody hodnotu bitu:
 * - HIGH = bit 1 (Manchester: HIGH-LOW)
 * - LOW = bit 0 (Manchester: LOW-HIGH)
 *
 * Funkce precte uroven na aktualni pozici (zacatek bitove periody)
 * a pak posune pozici o celou bitovou periodu T.
 *
 * @param dec Kontext dekoderu.
 * @param[out] bit Hodnota bitu (0 nebo 1).
 * @return WAV_ANALYZER_OK pri uspechu,
 *         WAV_ANALYZER_ERROR_DECODE_INCOMPLETE na konci sekvence.
 */
static en_WAV_ANALYZER_ERROR cpmtape_read_bit ( st_CPMTAPE_DEC *dec, int *bit ) {
    if ( dec->pos >= dec->seq->count ) {
        return WAV_ANALYZER_ERROR_DECODE_INCOMPLETE;
    }

    /*
     * Uroven na zacatku bitove periody urcuje hodnotu bitu.
     * Manchester bit 1 = prvni polovina v polarite bit1_level.
     * bit1_level se urcuje ze sync bitu (sync je bit 1).
     */
    *bit = ( dec->seq->pulses[dec->pos].level == dec->bit1_level ) ? 1 : 0;

    /* posuneme se o celou bitovou periodu */
    cpmtape_advance ( dec, dec->bit_period_us );

    return WAV_ANALYZER_OK;
}


/**
 * @brief Precte jeden bajt (8 bitu LSB first) a akumuluje checksum.
 *
 * @param dec Kontext dekoderu.
 * @param[out] byte Vystupni hodnota bajtu.
 * @return WAV_ANALYZER_OK pri uspechu, jinak chybovy kod.
 *
 * @post dec->checksum je aktualizovan o hodnotu bajtu.
 */
static en_WAV_ANALYZER_ERROR cpmtape_read_byte ( st_CPMTAPE_DEC *dec, uint8_t *byte ) {
    uint8_t value = 0;
    int i;

    for ( i = 0; i < 8; i++ ) {
        int bit;
        en_WAV_ANALYZER_ERROR err = cpmtape_read_bit ( dec, &bit );
        if ( err != WAV_ANALYZER_OK ) return err;
        if ( bit ) value |= ( uint8_t ) ( 1 << i );
    }

    *byte = value;
    dec->checksum += value;
    return WAV_ANALYZER_OK;
}


/**
 * @brief Precte jeden bajt (8 bitu LSB first) BEZ akumulace checksumu.
 *
 * Pouziva se pro cteni checksum bajtu z pasky, ktere se nesmi
 * akumulovat do kontrolniho souctu.
 *
 * @param dec Kontext dekoderu.
 * @param[out] byte Vystupni hodnota bajtu.
 * @return WAV_ANALYZER_OK pri uspechu, jinak chybovy kod.
 */
static en_WAV_ANALYZER_ERROR cpmtape_read_byte_raw ( st_CPMTAPE_DEC *dec, uint8_t *byte ) {
    uint8_t value = 0;
    int i;

    for ( i = 0; i < 8; i++ ) {
        int bit;
        en_WAV_ANALYZER_ERROR err = cpmtape_read_bit ( dec, &bit );
        if ( err != WAV_ANALYZER_OK ) return err;
        if ( bit ) value |= ( uint8_t ) ( 1 << i );
    }

    *byte = value;
    return WAV_ANALYZER_OK;
}


/**
 * @brief Precte 7 bitu posledniho bajtu (LSB first) BEZ akumulace checksumu.
 *
 * CPM-TAPE neprenasi MSB (bit 7) posledniho bajtu body bloku.
 * Dekoder cte 7 bitu a MSB rekonstruuje z checksumu.
 *
 * @param dec Kontext dekoderu.
 * @param[out] partial Vystupni hodnota (bity 0-6, bit 7 = 0).
 * @return WAV_ANALYZER_OK pri uspechu, jinak chybovy kod.
 */
static en_WAV_ANALYZER_ERROR cpmtape_read_byte_partial ( st_CPMTAPE_DEC *dec, uint8_t *partial ) {
    uint8_t value = 0;
    int i;

    for ( i = 0; i < 7; i++ ) {
        int bit;
        en_WAV_ANALYZER_ERROR err = cpmtape_read_bit ( dec, &bit );
        if ( err != WAV_ANALYZER_OK ) return err;
        if ( bit ) value |= ( uint8_t ) ( 1 << i );
    }

    *partial = value;
    return WAV_ANALYZER_OK;
}


/* =========================================================================
 *  Sync detekce a kalibrace
 * ========================================================================= */

/**
 * @brief Najde CPM-TAPE sync v sekvenci pulzu.
 *
 * Sync bit 1 se sklada ze dvou velmi dlouhych pul-period:
 * HIGH(5*T/2) + LOW(5*T/2). Hledame par po sobe jdoucich pulzu,
 * ktere splnuji:
 * 1. Oba delsi nez WAV_CPMTAPE_SYNC_MIN_US
 * 2. Opacne polarity (Manchester bit = dve protipolarne pul-periody)
 * 3. Vzajemne podobne (do SYNC_PAIR_TOLERANCE)
 * 4. Predchazene pilotem s T/2 a 3T/2 pulzy (pomer ≈ 1:3)
 * 5. Pomer sync/short ≈ 5 (v rozsahu SYNC_RATIO_MIN - SYNC_RATIO_MAX)
 *
 * @param seq Sekvence pulzu.
 * @param start_pos Pozice od ktere zacit hledat.
 * @param[out] out_sync_pos Index prvniho sync pulzu.
 * @param[out] out_bit_period Kalibrovana bitova perioda T (us).
 * @param[out] out_bit1_level Uroven signalu odpovídající bitu 1
 *             (urcena z polarity prvni pul-periody sync bitu).
 * @return WAV_ANALYZER_OK pri nalezeni, jinak chybovy kod.
 */
static en_WAV_ANALYZER_ERROR cpmtape_find_sync (
    const st_WAV_PULSE_SEQUENCE *seq,
    uint32_t start_pos,
    uint32_t *out_sync_pos,
    double *out_bit_period,
    int *out_bit1_level
) {
    if ( !seq || !out_sync_pos || !out_bit_period || !out_bit1_level ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    /* potrebujeme alespon PILOT_WINDOW pulzu pred sync + 2 pro sync */
    uint32_t min_start = start_pos + WAV_CPMTAPE_PILOT_WINDOW;

    uint32_t pos;
    for ( pos = min_start; pos + 1 < seq->count; pos++ ) {
        double d1 = seq->pulses[pos].duration_us;
        double d2 = seq->pulses[pos + 1].duration_us;

        /* obe pul-periody musi byt dostatecne dlouhe */
        if ( d1 < WAV_CPMTAPE_SYNC_MIN_US || d2 < WAV_CPMTAPE_SYNC_MIN_US ) continue;

        /* obe pul-periody musi mit opacnou polaritu (Manchester bit) */
        if ( seq->pulses[pos].level == seq->pulses[pos + 1].level ) continue;

        /* obe pul-periody musi byt podobne */
        double sync_avg = ( d1 + d2 ) / 2.0;
        if ( fabs ( d1 - d2 ) > sync_avg * SYNC_PAIR_TOLERANCE ) continue;

        /* kalibrace z predchazejiciho pilotu */
        uint32_t cal_start = ( pos > WAV_CPMTAPE_PILOT_WINDOW )
                             ? pos - WAV_CPMTAPE_PILOT_WINDOW
                             : 0;

        /* spocitame SHORT pulzy (T/2) v pilotnim okne */
        double short_sum = 0.0;
        int short_count = 0;
        double long_sum = 0.0;
        int long_count = 0;

        uint32_t i;
        for ( i = cal_start; i < pos; i++ ) {
            double d = seq->pulses[i].duration_us;
            if ( d < sync_avg * PILOT_SHORT_FACTOR ) {
                /* SHORT pulz (T/2) */
                short_sum += d;
                short_count++;
            } else if ( d < sync_avg * 0.8 ) {
                /* LONG pilot pulz (3T/2) - delsi nez short, kratsi nez sync */
                long_sum += d;
                long_count++;
            }
        }

        /* musime mit dostatek obou typu pulzu */
        if ( short_count < WAV_CPMTAPE_PILOT_MIN_SHORT ) continue;
        if ( long_count < WAV_CPMTAPE_PILOT_MIN_LONG ) continue;

        double short_avg = short_sum / short_count;
        double long_avg = long_sum / long_count;

        /* pomer long/short musi byt ≈ 3 (pilot vzor T/2 a 3T/2) */
        double pilot_ratio = long_avg / short_avg;
        if ( pilot_ratio < WAV_CPMTAPE_PILOT_RATIO_MIN ||
             pilot_ratio > WAV_CPMTAPE_PILOT_RATIO_MAX ) continue;

        /* pomer sync/short musi byt ≈ 5 (sync = 5*T/2, short = T/2) */
        double sync_ratio = sync_avg / short_avg;
        if ( sync_ratio < WAV_CPMTAPE_SYNC_RATIO_MIN ||
             sync_ratio > WAV_CPMTAPE_SYNC_RATIO_MAX ) continue;

        /*
         * Validni sync nalezen!
         * Sync je Manchester bit 1. Prvni pul-perioda sync bitu
         * ma polaritu odpovídající bitu 1. To muze byt HIGH nebo LOW
         * v zavislosti na kodovani WAV signalu.
         */
        *out_sync_pos = pos;
        *out_bit_period = short_avg * 2.0;  /* T = 2 * (T/2) */
        *out_bit1_level = seq->pulses[pos].level;
        return WAV_ANALYZER_OK;
    }

    return WAV_ANALYZER_ERROR_DECODE_TAPEMARK;
}


/**
 * @brief Inicializuje kontext dekoderu za sync bitem.
 *
 * Po nalezeni sync (HIGH pul-perioda na pozici sync_pos) nastavi
 * dekoder tak, aby ukazoval na zacatek prvniho datoveho bitu.
 *
 * Sync bit 1 = HIGH(5T/2) + LOW(5T/2). LOW cast muze byt sleita
 * s prvnim datovym bitem (pokud zacinaji stejnou polaritou).
 * Dekoder preskoci sync LOW pomoci time-budget pristpu.
 *
 * @param dec Kontext dekoderu k inicializaci.
 * @param seq Sekvence pulzu.
 * @param sync_pos Index sync HIGH pulzu.
 * @param bit_period Kalibrovana bitova perioda T (us).
 */
static void cpmtape_init_after_sync (
    st_CPMTAPE_DEC *dec,
    const st_WAV_PULSE_SEQUENCE *seq,
    uint32_t sync_pos,
    double bit_period,
    int bit1_level
) {
    dec->seq = seq;
    dec->bit_period_us = bit_period;
    dec->checksum = 0;
    dec->bit1_level = bit1_level;

    /* presuneme se na sync LOW pulz (pos + 1) */
    dec->pos = sync_pos + 1;
    dec->time_offset_us = 0.0;

    /*
     * Preskocime sync druhou pul-periodu.
     * Pouzijeme skutecnou delku sync HIGH pulzu (presnou, ne vypoctenou),
     * protoze zaokrouhlovani v koderu zpusobi, ze T*2.5 != skutecny sync.
     * Napr. pro 2400 Bd@44100: T/2=9 vzorku, ale round(T/2*5)=46, ne 45.
     */
    double sync_half = seq->pulses[sync_pos].duration_us;
    cpmtape_advance ( dec, sync_half );
}


/* =========================================================================
 *  Dekodovani bloku
 * ========================================================================= */

/**
 * @brief Dekoduje blok dat: flag + data_bytes + checksum.
 *
 * Precte flag bajt, overeni s ocekavanym, pak precte data a checksum.
 * Pro header blok cte plnych 128 bajtu. Pro body blok cte
 * (size-1) plnych bajtu a 7 bitu posledniho + rekonstruuje MSB.
 *
 * @param dec Kontext dekoderu (uz pozicovany za sync).
 * @param expected_flag Ocekavana hodnota flag bajtu ($00 nebo $01).
 * @param data_size Pocet datovych bajtu k precteni.
 * @param is_body 1 pro body blok (posledni bajt ma 7 bitu), 0 pro header.
 * @param[out] out_data Alokuje a naplni pole dekodovanych dat.
 * @param[out] out_crc_status Vysledek CRC verifikace.
 * @param[out] out_crc_stored Checksum precteny z pasky.
 * @param[out] out_crc_computed Checksum vypocteny z dat.
 * @return WAV_ANALYZER_OK pri uspechu, jinak chybovy kod.
 *
 * @post Pri uspechu *out_data ukazuje na alokovanou pamet s daty.
 */
static en_WAV_ANALYZER_ERROR cpmtape_decode_block (
    st_CPMTAPE_DEC *dec,
    uint8_t expected_flag,
    uint32_t data_size,
    int is_body,
    uint8_t **out_data,
    en_WAV_CRC_STATUS *out_crc_status,
    uint16_t *out_crc_stored,
    uint16_t *out_crc_computed
) {
    en_WAV_ANALYZER_ERROR err;

    /* === 1. Cteni flag bajtu === */
    dec->checksum = 0;
    uint8_t flag;
    err = cpmtape_read_byte ( dec, &flag );
    if ( err != WAV_ANALYZER_OK ) return err;

    if ( flag != expected_flag ) {
        return WAV_ANALYZER_ERROR_DECODE_DATA;
    }

    /* === 2. Alokace bufferu a cteni dat === */
    uint8_t *data = ( uint8_t* ) malloc ( data_size );
    if ( !data ) return WAV_ANALYZER_ERROR_ALLOC;

    if ( !is_body || data_size <= 1 ) {
        /* header blok: vsechny bajty plne (8 bitu) */
        /* nebo body s 0-1 bajty */
        if ( !is_body ) {
            /* header: vsech data_size bajtu plnych 8 bitu */
            uint32_t i;
            for ( i = 0; i < data_size; i++ ) {
                err = cpmtape_read_byte ( dec, &data[i] );
                if ( err != WAV_ANALYZER_OK ) {
                    free ( data );
                    return err;
                }
            }
        } else if ( data_size == 1 ) {
            /* body s 1 bajtem: jen 7 bitu */
            uint8_t partial;
            err = cpmtape_read_byte_partial ( dec, &partial );
            if ( err != WAV_ANALYZER_OK ) {
                free ( data );
                return err;
            }
            data[0] = partial; /* MSB rekonstruujeme nize */
        }
        /* data_size == 0: nic necteme (neocekavano pro body, ale bezpecne) */
    } else {
        /* body blok: (data_size - 1) plnych bajtu + 7 bitu posledniho */
        uint32_t i;
        for ( i = 0; i < data_size - 1; i++ ) {
            err = cpmtape_read_byte ( dec, &data[i] );
            if ( err != WAV_ANALYZER_OK ) {
                free ( data );
                return err;
            }
        }

        /* posledni bajt: jen 7 bitu */
        uint8_t partial;
        err = cpmtape_read_byte_partial ( dec, &partial );
        if ( err != WAV_ANALYZER_OK ) {
            free ( data );
            return err;
        }
        data[data_size - 1] = partial; /* MSB rekonstruujeme nize */
    }

    /* === 3. Cteni checksumu z pasky (2 bajty, low byte first) === */
    uint8_t chk_lo, chk_hi;
    err = cpmtape_read_byte_raw ( dec, &chk_lo );
    if ( err != WAV_ANALYZER_OK ) {
        free ( data );
        if ( out_crc_status ) *out_crc_status = WAV_CRC_NOT_AVAILABLE;
        return err;
    }

    err = cpmtape_read_byte_raw ( dec, &chk_hi );
    if ( err != WAV_ANALYZER_OK ) {
        free ( data );
        if ( out_crc_status ) *out_crc_status = WAV_CRC_NOT_AVAILABLE;
        return err;
    }

    uint16_t stored = ( uint16_t ) chk_lo | ( ( uint16_t ) chk_hi << 8 );

    /* === 4. Rekonstrukce chybejiciho MSB posledniho bajtu (body) === */
    if ( is_body && data_size > 0 ) {
        /*
         * stored = flag + sum(vsechny plne bajty)
         * dec->checksum = flag + sum(bajty 0..n-2)
         * diff = stored - dec->checksum = posledni bajt (plny, 8 bitu)
         *
         * Overime, ze dolních 7 bitu odpovida tomu, co jsme prectli.
         */
        uint16_t diff = ( uint16_t ) ( stored - dec->checksum );
        uint8_t reconstructed = ( uint8_t ) ( diff & 0xFF );
        uint8_t partial = data[data_size - 1];

        if ( ( reconstructed & 0x7F ) == partial ) {
            /* MSB uspesne rekonstruovan */
            data[data_size - 1] = reconstructed;
            /* aktualizujeme checksum se skutecnou hodnotou */
            dec->checksum += reconstructed;
        } else {
            /* rekonstrukce selhala - CRC chyba */
            /* ponechame 7-bitovou hodnotu */
            dec->checksum += partial;
        }
    }

    /* === 5. Verifikace checksumu === */
    uint16_t computed = dec->checksum;

    if ( out_crc_stored ) *out_crc_stored = stored;
    if ( out_crc_computed ) *out_crc_computed = computed;

    if ( out_crc_status ) {
        *out_crc_status = ( computed == stored ) ? WAV_CRC_OK : WAV_CRC_ERROR;
    }

    *out_data = data;
    return WAV_ANALYZER_OK;
}


/* =========================================================================
 *  Verejne API
 * ========================================================================= */

en_WAV_ANALYZER_ERROR wav_decode_cpmtape_decode_mzf (
    const st_WAV_PULSE_SEQUENCE *seq,
    uint32_t search_from,
    st_MZF **out_mzf,
    st_WAV_DECODE_RESULT *out_header_result,
    st_WAV_DECODE_RESULT *out_body_result,
    uint32_t *out_consumed_until
) {
    if ( !seq || !out_mzf ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    *out_mzf = NULL;
    if ( out_consumed_until ) *out_consumed_until = 0;

    /* === 1. Najdi header sync === */
    uint32_t hdr_sync_pos;
    double bit_period;
    int bit1_level;
    en_WAV_ANALYZER_ERROR err;

    err = cpmtape_find_sync ( seq, search_from, &hdr_sync_pos, &bit_period, &bit1_level );
    if ( err != WAV_ANALYZER_OK ) return err;

    /* === 2. Dekoduj header blok === */
    st_CPMTAPE_DEC dec;
    cpmtape_init_after_sync ( &dec, seq, hdr_sync_pos, bit_period, bit1_level );

    uint32_t hdr_pulse_start = dec.pos;
    uint8_t *hdr_data = NULL;
    en_WAV_CRC_STATUS hdr_crc_status = WAV_CRC_NOT_AVAILABLE;
    uint16_t hdr_crc_stored = 0, hdr_crc_computed = 0;

    err = cpmtape_decode_block ( &dec, WAV_CPMTAPE_FLAG_HEADER, MZF_HEADER_SIZE, 0,
                                  &hdr_data, &hdr_crc_status,
                                  &hdr_crc_stored, &hdr_crc_computed );
    if ( err != WAV_ANALYZER_OK ) return err;

    uint32_t hdr_pulse_end = dec.pos;

    /* vyplnime header result */
    if ( out_header_result ) {
        out_header_result->format = WAV_TAPE_FORMAT_CPM_TAPE;
        out_header_result->data = ( uint8_t* ) malloc ( MZF_HEADER_SIZE );
        if ( out_header_result->data ) {
            memcpy ( out_header_result->data, hdr_data, MZF_HEADER_SIZE );
        }
        out_header_result->data_size = MZF_HEADER_SIZE;
        out_header_result->crc_status = hdr_crc_status;
        out_header_result->crc_stored = hdr_crc_stored;
        out_header_result->crc_computed = hdr_crc_computed;
        out_header_result->pulse_start = hdr_pulse_start;
        out_header_result->pulse_end = hdr_pulse_end;
        out_header_result->is_header = 1;
    }

    /* === 3. Parsuj MZF hlavicku === */
    st_MZF_HEADER mzf_header;
    memcpy ( &mzf_header, hdr_data, MZF_HEADER_SIZE );
    free ( hdr_data );
    hdr_data = NULL;

    mzf_header_items_correction ( &mzf_header );
    uint16_t body_size = mzf_header.fsize;

    /* === 4. Dekoduj body blok (pokud existuje) === */
    uint8_t *body_data = NULL;

    if ( body_size > 0 ) {
        /* najdi body sync */
        uint32_t body_sync_pos;
        double body_bit_period;
        int body_bit1_level;

        err = cpmtape_find_sync ( seq, dec.pos, &body_sync_pos, &body_bit_period, &body_bit1_level );
        if ( err != WAV_ANALYZER_OK ) {
            /* body sync nenalezen */
            return WAV_ANALYZER_ERROR_DECODE_TAPEMARK;
        }

        /* inicializuj dekoder za body sync */
        cpmtape_init_after_sync ( &dec, seq, body_sync_pos, body_bit_period, body_bit1_level );

        uint32_t body_pulse_start = dec.pos;
        en_WAV_CRC_STATUS body_crc_status = WAV_CRC_NOT_AVAILABLE;
        uint16_t body_crc_stored = 0, body_crc_computed = 0;

        err = cpmtape_decode_block ( &dec, WAV_CPMTAPE_FLAG_BODY, body_size, 1,
                                      &body_data, &body_crc_status,
                                      &body_crc_stored, &body_crc_computed );
        if ( err != WAV_ANALYZER_OK ) return err;

        /* vyplnime body result */
        if ( out_body_result ) {
            out_body_result->format = WAV_TAPE_FORMAT_CPM_TAPE;
            out_body_result->data = NULL;   /* data jsou primo v mzf->body */
            out_body_result->data_size = body_size;
            out_body_result->crc_status = body_crc_status;
            out_body_result->crc_stored = body_crc_stored;
            out_body_result->crc_computed = body_crc_computed;
            out_body_result->pulse_start = body_pulse_start;
            out_body_result->pulse_end = dec.pos;
            out_body_result->is_header = 0;
        }
    }

    /* === 5. Sestav MZF strukturu === */
    st_MZF *mzf = ( st_MZF* ) calloc ( 1, sizeof ( st_MZF ) );
    if ( !mzf ) {
        free ( body_data );
        return WAV_ANALYZER_ERROR_ALLOC;
    }

    memcpy ( &mzf->header, &mzf_header, sizeof ( st_MZF_HEADER ) );
    mzf->body = body_data;
    mzf->body_size = body_size;

    *out_mzf = mzf;

    if ( out_consumed_until ) {
        *out_consumed_until = dec.pos;
    }

    return WAV_ANALYZER_OK;
}
