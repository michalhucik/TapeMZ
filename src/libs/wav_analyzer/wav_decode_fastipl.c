/**
 * @file   wav_decode_fastipl.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Implementace vrstvy 4c - FASTIPL dekoder.
 *
 * FASTIPL signal ma dve casti:
 * - Cast 1: $BB hlavicka (1:1 rychlost, fsize=0, loader + realne parametry)
 * - Pauza (typicky 1000 ms ticha)
 * - Cast 2: datove telo (TURBO rychlost, LTM + body + CRC, bez hlavicky)
 *
 * Dekoder extrahuje realne parametry z $BB hlavicky, najde Cast 2 a
 * dekoduje telo pomoci FM dekoderu. Rekonstruuje MZF z parametru a dat.
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

#include "wav_decode_fastipl.h"
#include "wav_decode_fm.h"
#include "wav_leader.h"


/**
 * @brief Minimalni pocet pulzu pro detekci leaderu Cast 2.
 *
 * Cast 2 zacina LGAP. Pouzivame nizsi prah kvuli mozne degradaci.
 */
#define FASTIPL_DATA_LEADER_MIN_PULSES  500

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
    uint32_t *out_consumed_until
) {
    if ( !seq || !bb_header_raw || !out_mzf ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    *out_mzf = NULL;
    if ( out_consumed_until ) *out_consumed_until = 0;

    /* === 1. Extrakce realnych parametru z $BB hlavicky === */

    /*
     * $BB hlavicka (128 bajtu):
     * offset 0x00: ftype = $BB
     * offset 0x01-0x11: fname (17 bajtu, originalni nazev)
     * offset 0x1A-0x1B: skutecny fsize (LE)
     * offset 0x1C-0x1D: skutecny fstrt (LE)
     * offset 0x1E-0x1F: skutecny fexec (LE)
     *
     * Realne parametry jsou v LE (Z80) formatu - prevedeme na host byte-order.
     */
    uint16_t real_fsize, real_fstrt, real_fexec;
    memcpy ( &real_fsize, &bb_header_raw[WAV_FASTIPL_OFF_FSIZE], 2 );
    memcpy ( &real_fstrt, &bb_header_raw[WAV_FASTIPL_OFF_FSTRT], 2 );
    memcpy ( &real_fexec, &bb_header_raw[WAV_FASTIPL_OFF_FEXEC], 2 );
    real_fsize = endianity_bswap16_LE ( real_fsize );
    real_fstrt = endianity_bswap16_LE ( real_fstrt );
    real_fexec = endianity_bswap16_LE ( real_fexec );

    /* === 2. Hledani leaderu Cast 2 === */

    /*
     * Za Cast 1 nasleduje pauza (ticho) a pak Cast 2 s LGAP.
     * wav_leader_detect preskoci ticho a najde dalsi leader.
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

    /* === 3. Dekodovani Cast 2 === */

    /*
     * Intercopy FASTIPL Cast 2 muze mit dve struktury:
     * a) Kompletni MZF ramec (LTM + header + CHKH + SGAP + STM + body + CHKB)
     *    - header muze byt $BB nebo originalni
     * b) Pouze telo (LTM + body + CHKB) - nase mzcmt_fastipl varianta
     *
     * Zkusime nejdriv dekodovat jako kompletni MZF (varianta a).
     * Pokud se header CRC potvrdi a fsize odpovida, pouzijeme ho.
     */
    st_MZF *cast2_mzf = NULL;
    st_WAV_DECODE_RESULT cast2_hdr, cast2_body;
    memset ( &cast2_hdr, 0, sizeof ( cast2_hdr ) );
    memset ( &cast2_body, 0, sizeof ( cast2_body ) );

    err = wav_decode_fm_decode_mzf ( seq, &data_leader,
                                      &cast2_mzf, &cast2_hdr, &cast2_body );

    if ( err == WAV_ANALYZER_OK && cast2_mzf ) {
        /*
         * Uspesne dekodovani hlavicky Cast 2.
         *
         * Intercopy FASTIPL Cast 2 = kompletni MZF s $BB hlavickou,
         * kde fsize=0 v standardni pozici. FM dekoder precte header OK
         * ale body_size=0. Telo musime precist separatne pomoci
         * real_fsize z $BB parametru Cast 1.
         */
        if ( cast2_mzf->header.ftype == 0xBB && cast2_mzf->body_size == 0 &&
             real_fsize > 0 ) {
            /*
             * FM dekoder precetl $BB header (fsize=0) + prazdne body.
             * Za tim nasleduje skutecne body s vlastnim STM.
             * Pozice: cast2_body.pulse_end (za prazdnym body CRC)
             * nebo cast2_hdr.pulse_end (za header CRC).
             * Pouzijeme pozici, ktera je dal (za prazdnym body).
             */
            uint32_t body_search = cast2_body.pulse_end > 0
                                   ? cast2_body.pulse_end
                                   : cast2_hdr.pulse_end;

            /* najdeme body leader (SGAP) */
            st_WAV_LEADER_INFO body_leader;
            err = wav_leader_detect ( seq, body_search, 200,
                                      WAV_ANALYZER_DEFAULT_TOLERANCE, &body_leader );

            if ( err == WAV_ANALYZER_OK ) {
                /* FM dekodovani body z body leaderu */
                st_WAV_FM_DECODER body_dec;
                uint32_t body_start = body_leader.start_index + body_leader.pulse_count;
                wav_decode_fm_init ( &body_dec, seq, body_start, body_leader.avg_period_us );

                err = wav_decode_fm_find_tapemark ( &body_dec, WAV_TAPEMARK_SHORT );
                if ( err == WAV_ANALYZER_OK ) {
                    /* sync skip */
                    int sync_skip = 0;
                    while ( body_dec.pos < seq->count &&
                            seq->pulses[body_dec.pos].duration_us >= body_dec.threshold_us &&
                            sync_skip < 4 ) {
                        body_dec.pos++;
                        sync_skip++;
                    }

                    /* cteni body dat */
                    uint8_t *real_body = ( uint8_t* ) malloc ( real_fsize );
                    if ( real_body ) {
                        body_dec.crc_accumulator = 0;
                        err = wav_decode_fm_read_block ( &body_dec, real_body, real_fsize );
                        if ( err == WAV_ANALYZER_OK ) {
                            /* CRC verifikace */
                            en_WAV_CRC_STATUS bcrc;
                            uint16_t bcs = 0, bcc = 0;
                            wav_decode_fm_verify_checksum ( &body_dec, &bcrc, &bcs, &bcc );

                            /* sestaveni MZF */
                            cast2_mzf->header.ftype = WAV_FASTIPL_DEFAULT_FTYPE;
                            cast2_mzf->header.fsize = real_fsize;
                            cast2_mzf->header.fstrt = real_fstrt;
                            cast2_mzf->header.fexec = real_fexec;
                            cast2_mzf->body = real_body;
                            cast2_mzf->body_size = real_fsize;

                            if ( out_body_result ) {
                                out_body_result->format = WAV_TAPE_FORMAT_FASTIPL;
                                out_body_result->crc_status = bcrc;
                                out_body_result->pulse_end = body_dec.pos;
                            }

                            free ( cast2_hdr.data );
                            free ( cast2_body.data );
                            *out_mzf = cast2_mzf;
                            if ( out_consumed_until ) *out_consumed_until = body_dec.pos;
                            return WAV_ANALYZER_OK;
                        }
                        free ( real_body );
                    }
                }
            }
        } else if ( cast2_mzf->body_size > 0 ) {
            /* Cast 2 neni $BB a ma body - pouzijeme primo */
            free ( cast2_hdr.data );
            if ( out_body_result ) {
                *out_body_result = cast2_body;
            } else {
                free ( cast2_body.data );
            }
            *out_mzf = cast2_mzf;
            if ( out_consumed_until ) {
                *out_consumed_until = cast2_body.pulse_end > 0
                                      ? cast2_body.pulse_end : cast2_hdr.pulse_end;
            }
            return WAV_ANALYZER_OK;
        }
    }

    /* Varianta b): Cast 2 = pouze telo (bez hlavicky) - fallback */
    if ( cast2_mzf ) mzf_free ( cast2_mzf );
    free ( cast2_hdr.data );
    free ( cast2_body.data );
    memset ( &cast2_body, 0, sizeof ( cast2_body ) );

    st_WAV_FM_DECODER dec;
    uint32_t data_start = data_leader.start_index + data_leader.pulse_count;
    wav_decode_fm_init ( &dec, seq, data_start, data_leader.avg_period_us );

    err = wav_decode_fm_find_tapemark ( &dec, WAV_TAPEMARK_LONG );
    if ( err != WAV_ANALYZER_OK ) {
        return WAV_ANALYZER_ERROR_DECODE_TAPEMARK;
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

    /* dekodovani tela */
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
            out_body_result->data = ( uint8_t* ) malloc ( real_fsize );
            if ( out_body_result->data ) {
                memcpy ( out_body_result->data, body_data, real_fsize );
            }
            out_body_result->data_size = real_fsize;
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

    /* === 5. Consumed until === */

    if ( out_consumed_until ) {
        *out_consumed_until = dec.pos;
    }

    return WAV_ANALYZER_OK;
}
