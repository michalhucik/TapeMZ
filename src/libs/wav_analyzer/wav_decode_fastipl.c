/**
 * @file   wav_decode_fastipl.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.3.0
 * @brief  Implementace vrstvy 4c - FASTIPL dekoder.
 *
 * FASTIPL signal je standardni dvou-blokovy MZF zaznam:
 * - Blok 1 (header): LGAP + LTM + $BB hlavicka(128B) + CRC
 *   Zapsan pri NORMAL rychlosti (1:1).
 * - Blok 2 (body): LGAP + STM + body data + CRC
 *   Zapsan pri TURBO rychlosti (1:1 az 8:3).
 *
 * $BB hlavicka obsahuje embedded loader a realne parametry
 * (fsize, fstrt, fexec) na nestandardnich offsetech ($1A-$1F).
 * Standardni fsize pole ($12-$13) obsahuje bajty loader kodu.
 *
 * Dekoder extrahuje realne parametry z $BB hlavicky, najde body
 * LGAP leader a dekoduje telo pomoci FM dekoderu.
 *
 * Intercopy pise body LGAP kratsi nez header LGAP (5500 vs 11000
 * pulzu) a body tapemark je STM (20+20), ne LTM (40+40).
 *
 * @par Licence:
 * GNU General Public License v3 (GPLv3)
 *
 * Copyright (C) 2026 Michal Hucik <hucik@ordoz.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/endianity/endianity.h"

#include "wav_decode_fastipl.h"
#include "wav_decode_fm.h"
#include "wav_leader.h"


/**
 * @brief Minimalni pocet pulzu pro detekci body LGAP leaderu.
 */
#define FASTIPL_DATA_LEADER_MIN_PULSES  500

/**
 * @brief FM threshold faktor pro FASTIPL body pri turbo rychlostech.
 *
 * Nizsi nez globalni WAV_ANALYZER_FM_THRESHOLD_FACTOR (1.6) kvuli
 * kvantizacnimu jitteru a asymetrickemu duty cycle pri vysokych
 * rychlostech (2800+ Bd pri 44100 Hz). Jednotlive LONG pul-periody
 * mohou klisnout pod prah 1.6x, ale 1.4x je dostatecne bezpecne.
 */
#define FASTIPL_FM_THRESHOLD_FACTOR     1.4

/** @brief Offset fname v MZF hlavicce (1 bajt za ftype). */
#define MZF_FNAME_OFFSET    1
/** @brief Delka fname v MZF hlavicce. */
#define MZF_FNAME_LENGTH    17


en_WAV_ANALYZER_ERROR wav_decode_fastipl_decode_mzf (
    const st_WAV_PULSE_SEQUENCE *seq,
    const uint8_t *bb_header_raw,
    uint32_t search_from_pulse,
    st_MZF **out_mzf,
    st_WAV_DECODE_RESULT *out_body_result,
    uint32_t *out_consumed_until,
    st_WAV_LEADER_INFO *out_data_leader
) {
    if ( !seq || !bb_header_raw || !out_mzf ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    *out_mzf = NULL;
    if ( out_consumed_until ) *out_consumed_until = 0;
    if ( out_data_leader ) memset ( out_data_leader, 0, sizeof ( *out_data_leader ) );

    /* === 1. Extrakce realnych parametru z $BB hlavicky === */

    uint16_t real_fsize, real_fstrt, real_fexec;
    memcpy ( &real_fsize, &bb_header_raw[WAV_FASTIPL_OFF_FSIZE], 2 );
    memcpy ( &real_fstrt, &bb_header_raw[WAV_FASTIPL_OFF_FSTRT], 2 );
    memcpy ( &real_fexec, &bb_header_raw[WAV_FASTIPL_OFF_FEXEC], 2 );
    real_fsize = endianity_bswap16_LE ( real_fsize );
    real_fstrt = endianity_bswap16_LE ( real_fstrt );
    real_fexec = endianity_bswap16_LE ( real_fexec );

    /* === 2. Hledani body LGAP leaderu === */

    /*
     * search_from_pulse = hdr_res.pulse_end (za header CRC).
     * Prvni leader je body LGAP - kratsi nez header LGAP
     * (5500 pulzu = 11000 pul-period vs 11000 pulzu = 22000 pul-period).
     *
     * Body LGAP muze byt pri NORMAL rychlosti (1:1) nebo
     * pri turbo rychlosti (2:1 az 8:3) - zavisi na nastaveni
     * rychlosti pri zapisu.
     */
    st_WAV_LEADER_INFO data_leader;
    en_WAV_ANALYZER_ERROR err = wav_leader_detect (
                                    seq, search_from_pulse,
                                    FASTIPL_DATA_LEADER_MIN_PULSES,
                                    WAV_ANALYZER_DEFAULT_TOLERANCE,
                                    &data_leader
                                );

    if ( err != WAV_ANALYZER_OK ) {
        return WAV_ANALYZER_ERROR_NO_LEADER;
    }

    /* vratime body LGAP leader info volajicimu (pro speed_class urceni) */
    if ( out_data_leader ) {
        *out_data_leader = data_leader;
    }

    /* === 3. Dekodovani body dat === */

    /*
     * Body blok: LGAP + STM(20+20) + sync + body(real_fsize B) + CRC.
     * Pouzijeme FM dekoder se snizenym threshold faktorem
     * pro lepsi kompatibilitu s turbo rychlostmi (2800+ Bd).
     *
     * Tapemark za body LGAP je STM (20 LONG + 20 SHORT),
     * ale nase find_tapemark s WAV_TAPEMARK_SHORT (min 18+18)
     * ho spolehlivě najde (20 >= 18).
     */
    st_WAV_FM_DECODER dec;
    uint32_t data_start = data_leader.start_index + data_leader.pulse_count;
    wav_decode_fm_init ( &dec, seq, data_start, data_leader.avg_period_us );

    /* snizeny threshold pro turbo rychlosti */
    dec.threshold_us = data_leader.avg_period_us * FASTIPL_FM_THRESHOLD_FACTOR;

    err = wav_decode_fm_find_tapemark ( &dec, WAV_TAPEMARK_SHORT );
    if ( err != WAV_ANALYZER_OK ) {
        return WAV_ANALYZER_ERROR_DECODE_TAPEMARK;
    }

    /* sync skip (max 4 LONG pul-periody) */
    {
        int sync_skip = 0;
        while ( dec.pos < seq->count &&
                seq->pulses[dec.pos].duration_us >= dec.threshold_us &&
                sync_skip < 4 ) {
            dec.pos++;
            sync_skip++;
        }
    }

    /* cteni body dat */
    uint32_t body_pulse_start = dec.pos;
    uint8_t *body_data = NULL;

    if ( real_fsize > 0 ) {
        body_data = ( uint8_t* ) malloc ( real_fsize );
        if ( !body_data ) return WAV_ANALYZER_ERROR_ALLOC;

        dec.crc_accumulator = 0;
        err = wav_decode_fm_read_block ( &dec, body_data, real_fsize );
        if ( err != WAV_ANALYZER_OK ) {
            free ( body_data );
            return WAV_ANALYZER_ERROR_DECODE_DATA;
        }

        en_WAV_CRC_STATUS body_crc_status;
        uint16_t body_crc_stored = 0, body_crc_computed = 0;
        wav_decode_fm_verify_checksum ( &dec, &body_crc_status,
                                        &body_crc_stored, &body_crc_computed );

        if ( out_body_result ) {
            out_body_result->format = WAV_TAPE_FORMAT_FASTIPL;
            out_body_result->crc_status = body_crc_status;
            out_body_result->crc_stored = body_crc_stored;
            out_body_result->crc_computed = body_crc_computed;
            out_body_result->pulse_start = body_pulse_start;
            out_body_result->pulse_end = dec.pos;
            out_body_result->is_header = 0;
        }
    }

    /* === 4. Sestaveni MZF struktury === */

    st_MZF *mzf = ( st_MZF* ) calloc ( 1, sizeof ( st_MZF ) );
    if ( !mzf ) {
        free ( body_data );
        return WAV_ANALYZER_ERROR_ALLOC;
    }

    mzf->header.ftype = WAV_FASTIPL_DEFAULT_FTYPE;
    memcpy ( &mzf->header.fname, &bb_header_raw[MZF_FNAME_OFFSET], MZF_FNAME_LENGTH );
    mzf->header.fsize = real_fsize;
    mzf->header.fstrt = real_fstrt;
    mzf->header.fexec = real_fexec;

    mzf->body = body_data;
    mzf->body_size = real_fsize;

    *out_mzf = mzf;

    if ( out_consumed_until ) {
        *out_consumed_until = dec.pos;
    }

    return WAV_ANALYZER_OK;
}
