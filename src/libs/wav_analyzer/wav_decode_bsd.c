/**
 * @file   wav_decode_bsd.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Implementace vrstvy 4d - BSD dekoder.
 *
 * BSD signal: hlavicka (fsize=0) nasledovana sekvenci chunku.
 * Kazdy chunk ma vlastni kratky tapemark (STM) a obsahuje
 * 258 bajtu (2B ID + 256B data) s checksumem.
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

#include "libs/endianity/endianity.h"

#include "wav_decode_bsd.h"
#include "wav_decode_fm.h"


/**
 * @brief Pocatecni kapacita bufferu pro akumulaci dat chunku.
 *
 * Buffer roste dynamicky - zacina na 4 chunky (4 * 256 = 1024 B)
 * a zdvojnasobuje se dle potreby.
 */
#define BSD_BODY_INITIAL_CAPACITY   ( 4 * WAV_BSD_CHUNK_DATA_SIZE )


en_WAV_ANALYZER_ERROR wav_decode_bsd_decode_mzf (
    const st_WAV_PULSE_SEQUENCE *seq,
    const st_WAV_LEADER_INFO *leader,
    const st_MZF_HEADER *header,
    uint32_t search_from_pulse,
    st_MZF **out_mzf,
    st_WAV_DECODE_RESULT *out_body_result,
    uint32_t *out_consumed_until
) {
    if ( !seq || !leader || !header || !out_mzf ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    *out_mzf = NULL;
    if ( out_consumed_until ) *out_consumed_until = 0;

    /* === 1. Inicializace FM dekoderu === */

    /*
     * Kalibrujeme prah z leaderu hlavicky (BSD pouziva
     * stejnou rychlost pro hlavicku i chunky).
     */
    st_WAV_FM_DECODER dec;
    wav_decode_fm_init ( &dec, seq, search_from_pulse, leader->avg_period_us );

    /* === 2. Dynamicky buffer pro akumulaci dat chunku === */

    uint8_t *body_data = ( uint8_t* ) malloc ( BSD_BODY_INITIAL_CAPACITY );
    if ( !body_data ) return WAV_ANALYZER_ERROR_ALLOC;

    uint32_t body_size = 0;
    uint32_t body_capacity = BSD_BODY_INITIAL_CAPACITY;
    uint32_t chunk_count = 0;

    en_WAV_CRC_STATUS last_crc_status = WAV_CRC_NOT_AVAILABLE;
    uint16_t last_crc_stored = 0, last_crc_computed = 0;
    uint32_t first_chunk_pulse_start = 0;

    /* === 3. Dekodovani chunku === */

    en_WAV_ANALYZER_ERROR err;

    for ( ;; ) {
        /* ochrana proti nekonecne smycce */
        if ( chunk_count >= WAV_BSD_MAX_CHUNKS ) {
            free ( body_data );
            return WAV_ANALYZER_ERROR_BUFFER_OVERFLOW;
        }

        /* hledame kratky tapemark (STM: 20 LONG + 20 SHORT) */
        err = wav_decode_fm_find_tapemark ( &dec, WAV_TAPEMARK_SHORT );
        if ( err != WAV_ANALYZER_OK ) {
            /* zadny dalsi tapemark - konec chunku */
            break;
        }

        /* sync blok (2L = max 4 LONG pul-periody) */
        {
            int sync_skip = 0;
            while ( dec.pos < seq->count &&
                    seq->pulses[dec.pos].duration_us >= dec.threshold_us &&
                    sync_skip < 4 ) {
                dec.pos++;
                sync_skip++;
            }
        }

        if ( chunk_count == 0 ) {
            first_chunk_pulse_start = dec.pos;
        }

        /* precteme 258B chunk (2B ID + 256B data) */
        uint8_t chunk[WAV_BSD_CHUNK_SIZE];
        dec.crc_accumulator = 0;

        err = wav_decode_fm_read_block ( &dec, chunk, WAV_BSD_CHUNK_SIZE );
        if ( err != WAV_ANALYZER_OK ) {
            free ( body_data );
            return WAV_ANALYZER_ERROR_DECODE_DATA;
        }

        /* verifikace CRC chunku */
        wav_decode_fm_verify_checksum ( &dec, &last_crc_status,
                                        &last_crc_stored, &last_crc_computed );

        /* extrakce chunk ID (prvni 2 bajty, LE) */
        uint16_t chunk_id;
        memcpy ( &chunk_id, chunk, 2 );
        chunk_id = endianity_bswap16_LE ( chunk_id );

        /* zvetsime buffer pokud je potreba */
        if ( body_size + WAV_BSD_CHUNK_DATA_SIZE > body_capacity ) {
            uint32_t new_cap = body_capacity * 2;
            uint8_t *new_data = ( uint8_t* ) realloc ( body_data, new_cap );
            if ( !new_data ) {
                free ( body_data );
                return WAV_ANALYZER_ERROR_ALLOC;
            }
            body_data = new_data;
            body_capacity = new_cap;
        }

        /* pripojime datovou cast chunku (256 bajtu za ID) */
        memcpy ( &body_data[body_size], &chunk[2], WAV_BSD_CHUNK_DATA_SIZE );
        body_size += WAV_BSD_CHUNK_DATA_SIZE;
        chunk_count++;

        /* chunk ID == 0xFFFF znamena posledni chunk */
        if ( chunk_id == WAV_BSD_LAST_CHUNK_ID ) {
            break;
        }
    }

    /* === 4. Sestaveni MZF struktury === */

    if ( chunk_count == 0 ) {
        free ( body_data );
        return WAV_ANALYZER_ERROR_DECODE_DATA;
    }

    st_MZF *mzf = ( st_MZF* ) calloc ( 1, sizeof ( st_MZF ) );
    if ( !mzf ) {
        free ( body_data );
        return WAV_ANALYZER_ERROR_ALLOC;
    }

    /* kopirujeme hlavicku a aktualizujeme fsize */
    memcpy ( &mzf->header, header, sizeof ( st_MZF_HEADER ) );
    mzf->header.fsize = ( uint16_t ) body_size;

    mzf->body = body_data;
    mzf->body_size = body_size;

    *out_mzf = mzf;

    /* vyplnime body result (posledni chunk) */
    if ( out_body_result ) {
        out_body_result->format = WAV_TAPE_FORMAT_BSD;
        out_body_result->data = NULL;   /* data jsou primo v mzf->body */
        out_body_result->data_size = body_size;
        out_body_result->crc_status = last_crc_status;
        out_body_result->crc_stored = last_crc_stored;
        out_body_result->crc_computed = last_crc_computed;
        out_body_result->pulse_start = first_chunk_pulse_start;
        out_body_result->pulse_end = dec.pos;
        out_body_result->is_header = 0;
    }

    /* === 5. Consumed until === */

    if ( out_consumed_until ) {
        *out_consumed_until = dec.pos;
    }

    return WAV_ANALYZER_OK;
}
