/**
 * @file   wav_decode_turbo.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Implementace vrstvy 4b - TURBO dekoder.
 *
 * TURBO signal ma dve casti:
 * - Cast 1: preloader hlavicka + telo (NORMAL 1:1)
 * - Cast 2: skutecna data v TURBO rychlosti (FM, kompletni MZF ramec)
 *
 * Dekoder najde prechod z preloader rychlosti (~249 us) na TURBO
 * rychlost (kratsi pulzy) a dekoduje Cast 2 pomoci FM dekoderu.
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

    /* === 2. Histogram TURBO oblasti pro presny threshold === */

    st_WAV_HISTOGRAM th_hist;
    memset ( &th_hist, 0, sizeof ( th_hist ) );
    uint32_t hist_count = 4000;
    if ( turbo_start + hist_count > seq->count )
        hist_count = seq->count - turbo_start;

    double avg_us = 125.0;
    en_WAV_ANALYZER_ERROR herr = wav_histogram_analyze (
        seq, turbo_start, hist_count, 5.0, &th_hist );
    if ( herr == WAV_ANALYZER_OK && th_hist.peak_count >= 1 ) {
        avg_us = th_hist.peaks[0].center_us;
    }
    wav_histogram_destroy ( &th_hist );

    /* === 3. FM dekodovani od TURBO LGAP === */

    st_WAV_LEADER_INFO synth_leader;
    memset ( &synth_leader, 0, sizeof ( synth_leader ) );
    synth_leader.start_index = turbo_start;
    synth_leader.pulse_count = 0;
    synth_leader.avg_period_us = avg_us;

    st_MZF *mzf = NULL;
    st_WAV_DECODE_RESULT hdr_res, body_res;
    memset ( &hdr_res, 0, sizeof ( hdr_res ) );
    memset ( &body_res, 0, sizeof ( body_res ) );

    en_WAV_ANALYZER_ERROR err = wav_decode_fm_decode_mzf (
                  seq, &synth_leader, &mzf,
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

        synth_leader.start_index = retry_from;
        err = wav_decode_fm_decode_mzf (
                  seq, &synth_leader, &mzf,
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
