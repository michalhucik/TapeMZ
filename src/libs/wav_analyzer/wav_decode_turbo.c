/**
 * @file   wav_decode_turbo.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.2.0
 * @brief  Implementace vrstvy 4b - TURBO dekoder.
 *
 * TURBO signal ma dve casti:
 * - Cast 1: preloader hlavicka + telo (NORMAL 1:1)
 * - Cast 2: skutecna data v TURBO rychlosti (FM, kompletni MZF ramec)
 *
 * Dekoder najde prechod z preloader rychlosti (~249 us) na TURBO
 * rychlost (kratsi pulzy) a dekoduje Cast 2 pomoci FM dekoderu.
 *
 * Postup dekodovani:
 * 1. Posuvne okno najde oblast s kratkymi pulzy (TURBO)
 * 2. Leader detektor najde skutecny TURBO LGAP
 * 3. Vlastni tapemark detekce (TURBO pouziva kratky tapemark 20+20
 *    pulzu misto standardniho 40+40)
 * 4. FM dekoder dekoduje hlavicku + telo
 *
 * Dva typy TURBO preloaderu:
 * a) mzftools (fsize=0, loader v comment): pouziva ROM rutinu $04F8 (RDATA),
 *    standardni FM kodovani. Dekoduje wav_decode_turbo_decode_mzf().
 * b) TurboCopy (fsize=90, fstrt=$D400): 90B loader patchne ROM rychlost
 *    a vola CMT read $002A - cte pouze body (bez hlavicky), standardni FM.
 *    Dekoduje wav_decode_turbo_turbocopy_mzf().
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
#include "wav_decode_turbo.h"
#include "wav_decode_fm.h"
#include "wav_leader.h"
#include "wav_histogram.h"


/**
 * @brief Max delka pulzu pro TURBO data (us).
 *
 * Pulzy kratsi nez tato hodnota jsou povazovany za TURBO.
 * Preloader ma SHORT ~227-278 us, TURBO data maji SHORT < 150 us.
 */
#define TURBO_SHORT_MAX_US  150.0

/** @brief Velikost posuvneho okna pro detekci prechodu (pulzy). */
#define TURBO_WINDOW        500

/** @brief Minimalni podil kratkych pulzu v okne pro detekci TURBO oblasti. */
#define TURBO_MIN_RATIO     0.90

/**
 * @brief Minimalni pocet pulzu pro detekci TURBO leaderu.
 *
 * TURBO LGAP ma typicky 10000-22000 pulzu. Pouzijeme nizsi prah,
 * aby detektor zachytil i kratsi leadery (napr. body LGAP).
 */
#define TURBO_LEADER_MIN_PULSES     500


en_WAV_ANALYZER_ERROR wav_decode_turbo_decode_mzf (
    const st_WAV_PULSE_SEQUENCE *seq,
    uint32_t search_from_pulse,
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

    /*
     * === 1. Detekce prechodu preloader -> TURBO ===
     *
     * Preloader data maji pulzy 227-499 us. TURBO LGAP ma ~116 us.
     * Pouzijeme posuvne okno TURBO_WINDOW pulzu a hledame oblast
     * kde >= 90 % pulzu je < 150 us.
     */
    uint32_t turbo_start = search_from_pulse;
    int found = 0;

    if ( search_from_pulse + TURBO_WINDOW < seq->count ) {
        uint32_t short_count = 0;
        uint32_t win_end = search_from_pulse + TURBO_WINDOW;

        for ( uint32_t p = search_from_pulse; p < win_end; p++ ) {
            if ( seq->pulses[p].duration_us < TURBO_SHORT_MAX_US ) short_count++;
        }

        if ( ( double ) short_count / TURBO_WINDOW >= TURBO_MIN_RATIO ) {
            turbo_start = search_from_pulse;
            found = 1;
        } else {
            for ( uint32_t p = search_from_pulse; p + TURBO_WINDOW < seq->count; p++ ) {
                if ( seq->pulses[p + TURBO_WINDOW].duration_us < TURBO_SHORT_MAX_US )
                    short_count++;
                if ( seq->pulses[p].duration_us < TURBO_SHORT_MAX_US )
                    short_count--;

                if ( ( double ) short_count / TURBO_WINDOW >= TURBO_MIN_RATIO ) {
                    turbo_start = p + 1;
                    found = 1;
                    break;
                }
            }
        }
    }

    if ( !found ) {
        return WAV_ANALYZER_ERROR_NO_LEADER;
    }

    /*
     * === 2. Detekce skutecneho TURBO leaderu ===
     *
     * Sliding window vraci pribliznou pozici TURBO oblasti, ale
     * turbo_start muze byt pred zacatkem skutecneho LGAP.
     * Leader detektor najde presny zacatek a delku TURBO LGAP.
     */
    st_WAV_LEADER_INFO turbo_leader;
    en_WAV_ANALYZER_ERROR lerr = wav_leader_detect (
        seq, turbo_start, TURBO_LEADER_MIN_PULSES,
        WAV_ANALYZER_DEFAULT_TOLERANCE, &turbo_leader
    );

    if ( lerr != WAV_ANALYZER_OK ) {
        return WAV_ANALYZER_ERROR_NO_LEADER;
    }

    /* === 3. FM dekodovani od konce TURBO leaderu === */

    st_MZF *mzf = NULL;
    st_WAV_DECODE_RESULT hdr_res, body_res;
    memset ( &hdr_res, 0, sizeof ( hdr_res ) );
    memset ( &body_res, 0, sizeof ( body_res ) );

    en_WAV_ANALYZER_ERROR err = wav_decode_fm_decode_mzf (
                  seq, &turbo_leader, &mzf,
                  &hdr_res, &body_res
              );

    if ( err != WAV_ANALYZER_OK || !mzf ) {
        free ( hdr_res.data );
        free ( body_res.data );
        return ( err != WAV_ANALYZER_OK ) ? err : WAV_ANALYZER_ERROR_NO_LEADER;
    }

    /* Pokud jsme nasli preloader copy2, preskocime a zkusime znovu */
    if ( mzf->header.fstrt == 0xD400 && mzf->header.fsize <= 500 ) {
        uint32_t retry_from = body_res.pulse_end > 0
                              ? body_res.pulse_end : hdr_res.pulse_end;
        mzf_free ( mzf ); mzf = NULL;
        free ( hdr_res.data );
        free ( body_res.data );
        memset ( &hdr_res, 0, sizeof ( hdr_res ) );
        memset ( &body_res, 0, sizeof ( body_res ) );

        /*
         * Pro retry pouzijeme novy leader detect - za preloader copy2
         * by mel byt dalsi TURBO LGAP.
         */
        st_WAV_LEADER_INFO retry_leader;
        lerr = wav_leader_detect (
            seq, retry_from, TURBO_LEADER_MIN_PULSES,
            WAV_ANALYZER_DEFAULT_TOLERANCE, &retry_leader
        );

        if ( lerr != WAV_ANALYZER_OK ) {
            return WAV_ANALYZER_ERROR_NO_LEADER;
        }

        err = wav_decode_fm_decode_mzf (
                  seq, &retry_leader, &mzf,
                  &hdr_res, &body_res
              );

        if ( err != WAV_ANALYZER_OK || !mzf ) {
            free ( hdr_res.data );
            free ( body_res.data );
            return ( err != WAV_ANALYZER_OK ) ? err : WAV_ANALYZER_ERROR_NO_LEADER;
        }
    }

    /* === 4. Vysledek === */

    *out_mzf = mzf;
    if ( out_header_result ) {
        *out_header_result = hdr_res;
    } else {
        free ( hdr_res.data );
    }
    if ( out_body_result ) {
        *out_body_result = body_res;
    } else {
        free ( body_res.data );
    }

    if ( out_consumed_until ) {
        if ( body_res.pulse_end > 0 ) {
            *out_consumed_until = body_res.pulse_end;
        } else if ( hdr_res.pulse_end > 0 ) {
            *out_consumed_until = hdr_res.pulse_end;
        }
    }

    return WAV_ANALYZER_OK;
}


/* =========================================================================
 * TURBO body-only dekoder (sdileny pro TurboCopy i mzftools)
 * ========================================================================= */

/**
 * @brief Offset user_params v TurboCopy preloader body (relativne k $D400).
 *
 * TurboCopy TURBO preloader ma na offsetu $4D od $D400:
 *   $D44D: fsize (2B LE)
 *   $D44F: fstrt (2B LE)
 *   $D451: fexec (2B LE)
 */
#define TC_LOADER_PARAMS_OFFSET     0x4D

/** @brief Minimalni velikost TurboCopy preloader body pro extrakci parametru. */
#define TC_LOADER_MIN_SIZE          ( TC_LOADER_PARAMS_OFFSET + 6 )

/**
 * @brief Offsety metadat v mzftools preloader comment oblasti.
 *
 * mzftools TURBO preloader (fsize=0) uklada metadata v comment:
 *   cmnt[1..2]: user fsize (LE)
 *   cmnt[3..4]: user fstrt (LE)
 *   cmnt[5..6]: user fexec (LE)
 */
#define MZF_LOADER_OFF_SIZE    1
#define MZF_LOADER_OFF_FROM    3
#define MZF_LOADER_OFF_EXEC   5


/**
 * @brief Dekoduje TURBO body-only signal.
 *
 * Sdilena implementace pro oba typy TURBO preloaderu (TurboCopy i mzftools).
 * Oba loadery patchnou ROM a volaji RDATA, ktera cte pouze body data.
 *
 * @param seq Sekvence pulzu.
 * @param search_from_pulse Pozice od ktere hledat TURBO LGAP.
 * @param preloader_header Hlavicka preloaderu (ftype, fname).
 * @param user_body_size Velikost uzivatelskeho programu (bajty).
 * @param user_fstrt Load adresa programu.
 * @param user_fexec Exec adresa programu.
 * @param[out] out_mzf Vystupni MZF. Nesmi byt NULL.
 * @param[out] out_body_result Vysledek dekodovani tela. Muze byt NULL.
 * @param[out] out_consumed_until Pozice za poslednim pulzem. Muze byt NULL.
 * @param[out] out_turbo_leader Detekovany TURBO leader. Muze byt NULL.
 * @return WAV_ANALYZER_OK pri uspechu, jinak chybovy kod.
 */
static en_WAV_ANALYZER_ERROR turbo_decode_body_only (
    const st_WAV_PULSE_SEQUENCE *seq,
    uint32_t search_from_pulse,
    const st_MZF_HEADER *preloader_header,
    uint16_t user_body_size,
    uint16_t user_fstrt,
    uint16_t user_fexec,
    st_MZF **out_mzf,
    st_WAV_DECODE_RESULT *out_body_result,
    uint32_t *out_consumed_until,
    st_WAV_LEADER_INFO *out_turbo_leader
) {
    *out_mzf = NULL;
    if ( out_consumed_until ) *out_consumed_until = 0;
    if ( out_body_result ) memset ( out_body_result, 0, sizeof ( *out_body_result ) );
    if ( out_turbo_leader ) memset ( out_turbo_leader, 0, sizeof ( *out_turbo_leader ) );

    if ( user_body_size == 0 ) {
        return WAV_ANALYZER_ERROR_DECODE_DATA;
    }

    /* === 1. Nalezeni TURBO LGAP leaderu === */

    uint32_t turbo_start = search_from_pulse;
    int found = 0;

    if ( search_from_pulse + TURBO_WINDOW < seq->count ) {
        uint32_t short_count = 0;
        uint32_t win_end = search_from_pulse + TURBO_WINDOW;

        for ( uint32_t p = search_from_pulse; p < win_end; p++ ) {
            if ( seq->pulses[p].duration_us < TURBO_SHORT_MAX_US ) short_count++;
        }

        if ( ( double ) short_count / TURBO_WINDOW >= TURBO_MIN_RATIO ) {
            turbo_start = search_from_pulse;
            found = 1;
        } else {
            for ( uint32_t p = search_from_pulse; p + TURBO_WINDOW < seq->count; p++ ) {
                if ( seq->pulses[p + TURBO_WINDOW].duration_us < TURBO_SHORT_MAX_US )
                    short_count++;
                if ( seq->pulses[p].duration_us < TURBO_SHORT_MAX_US )
                    short_count--;

                if ( ( double ) short_count / TURBO_WINDOW >= TURBO_MIN_RATIO ) {
                    turbo_start = p + 1;
                    found = 1;
                    break;
                }
            }
        }
    }

    if ( !found ) {
        return WAV_ANALYZER_ERROR_NO_LEADER;
    }

    st_WAV_LEADER_INFO turbo_leader;
    en_WAV_ANALYZER_ERROR lerr = wav_leader_detect (
        seq, turbo_start, TURBO_LEADER_MIN_PULSES,
        WAV_ANALYZER_DEFAULT_TOLERANCE, &turbo_leader
    );

    if ( lerr != WAV_ANALYZER_OK ) {
        return WAV_ANALYZER_ERROR_NO_LEADER;
    }

    if ( out_turbo_leader ) {
        *out_turbo_leader = turbo_leader;
    }

    /* === 2. Tapemark (STM) + sync === */

    uint32_t data_start = turbo_leader.start_index + turbo_leader.pulse_count;

    st_WAV_FM_DECODER dec;
    wav_decode_fm_init ( &dec, seq, data_start, turbo_leader.avg_period_us );

    en_WAV_ANALYZER_ERROR err = wav_decode_fm_find_tapemark ( &dec, WAV_TAPEMARK_SHORT );
    if ( err != WAV_ANALYZER_OK ) {
        return WAV_ANALYZER_ERROR_DECODE_TAPEMARK;
    }

    /* sync skip */
    {
        int sync_skip = 0;
        while ( dec.pos < seq->count &&
                seq->pulses[dec.pos].duration_us >= dec.threshold_us &&
                sync_skip < 4 ) {
            dec.pos++;
            sync_skip++;
        }
    }

    /* === 3. Dekodovani body dat === */

    uint32_t body_pulse_start = dec.pos;
    uint8_t *body_data = ( uint8_t* ) malloc ( user_body_size );
    if ( !body_data ) return WAV_ANALYZER_ERROR_ALLOC;

    dec.crc_accumulator = 0;
    err = wav_decode_fm_read_block ( &dec, body_data, user_body_size );
    if ( err != WAV_ANALYZER_OK ) {
        free ( body_data );
        return WAV_ANALYZER_ERROR_DECODE_DATA;
    }

    /* === 4. CRC === */

    en_WAV_CRC_STATUS body_crc;
    uint16_t body_crc_stored = 0, body_crc_computed = 0;
    wav_decode_fm_verify_checksum ( &dec, &body_crc, &body_crc_stored, &body_crc_computed );

    if ( body_crc == WAV_CRC_ERROR ) {
        fprintf ( stderr, "WARNING: body CRC mismatch (stored=0x%04X, computed=0x%04X)\n",
                  body_crc_stored, body_crc_computed );
    }

    uint32_t body_end_pos = dec.pos;

    if ( out_body_result ) {
        out_body_result->format = WAV_TAPE_FORMAT_TURBO;
        out_body_result->data = ( uint8_t* ) malloc ( user_body_size );
        if ( out_body_result->data ) {
            memcpy ( out_body_result->data, body_data, user_body_size );
        }
        out_body_result->data_size = user_body_size;
        out_body_result->crc_status = body_crc;
        out_body_result->crc_stored = body_crc_stored;
        out_body_result->crc_computed = body_crc_computed;
        out_body_result->pulse_start = body_pulse_start;
        out_body_result->pulse_end = body_end_pos;
        out_body_result->is_header = 0;
    }

    /* === 5. Sestaveni MZF === */

    st_MZF *mzf = ( st_MZF* ) calloc ( 1, sizeof ( st_MZF ) );
    if ( !mzf ) {
        free ( body_data );
        return WAV_ANALYZER_ERROR_ALLOC;
    }

    memcpy ( &mzf->header, preloader_header, sizeof ( st_MZF_HEADER ) );
    mzf->header.fsize = user_body_size;
    mzf->header.fstrt = user_fstrt;
    mzf->header.fexec = user_fexec;

    mzf->body = body_data;
    mzf->body_size = user_body_size;

    *out_mzf = mzf;

    if ( out_consumed_until ) {
        *out_consumed_until = body_end_pos;
    }

    return WAV_ANALYZER_OK;
}


/* =========================================================================
 * Verejne API - TurboCopy TURBO (metadata z preloader body)
 * ========================================================================= */

en_WAV_ANALYZER_ERROR wav_decode_turbo_turbocopy_mzf (
    const st_WAV_PULSE_SEQUENCE *seq,
    uint32_t search_from_pulse,
    const st_MZF_HEADER *preloader_header,
    const uint8_t *preloader_body,
    uint32_t preloader_body_size,
    st_MZF **out_mzf,
    st_WAV_DECODE_RESULT *out_body_result,
    uint32_t *out_consumed_until,
    st_WAV_LEADER_INFO *out_turbo_leader
) {
    if ( !seq || !out_mzf || !preloader_header || !preloader_body ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    if ( preloader_body_size < TC_LOADER_MIN_SIZE ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    /* extrakce metadat z TurboCopy preloader body ($D44D+) */
    const uint8_t *params = preloader_body + TC_LOADER_PARAMS_OFFSET;
    uint16_t body_size = ( uint16_t ) params[0] | ( ( uint16_t ) params[1] << 8 );
    uint16_t fstrt     = ( uint16_t ) params[2] | ( ( uint16_t ) params[3] << 8 );
    uint16_t fexec     = ( uint16_t ) params[4] | ( ( uint16_t ) params[5] << 8 );

    return turbo_decode_body_only (
        seq, search_from_pulse, preloader_header,
        body_size, fstrt, fexec,
        out_mzf, out_body_result, out_consumed_until, out_turbo_leader
    );
}


/* =========================================================================
 * Verejne API - mzftools TURBO (metadata z preloader header comment)
 * ========================================================================= */

en_WAV_ANALYZER_ERROR wav_decode_turbo_mzftools_mzf (
    const st_WAV_PULSE_SEQUENCE *seq,
    uint32_t search_from_pulse,
    const st_MZF_HEADER *preloader_header,
    st_MZF **out_mzf,
    st_WAV_DECODE_RESULT *out_body_result,
    uint32_t *out_consumed_until,
    st_WAV_LEADER_INFO *out_turbo_leader
) {
    if ( !seq || !out_mzf || !preloader_header ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    /* extrakce metadat z mzftools preloader comment oblasti */
    uint16_t body_size, fstrt, fexec;
    memcpy ( &body_size, &preloader_header->cmnt[MZF_LOADER_OFF_SIZE], 2 );
    memcpy ( &fstrt, &preloader_header->cmnt[MZF_LOADER_OFF_FROM], 2 );
    memcpy ( &fexec, &preloader_header->cmnt[MZF_LOADER_OFF_EXEC], 2 );

    /* cmnt hodnoty jsou v originalni LE endianite (Z80 poradi) */
    body_size = endianity_bswap16_LE ( body_size );
    fstrt = endianity_bswap16_LE ( fstrt );
    fexec = endianity_bswap16_LE ( fexec );

    return turbo_decode_body_only (
        seq, search_from_pulse, preloader_header,
        body_size, fstrt, fexec,
        out_mzf, out_body_result, out_consumed_until, out_turbo_leader
    );
}
