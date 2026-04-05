/**
 * @file   wav_leader.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Implementace vrstvy 2 - detekce leader tónů.
 *
 * Algoritmus detekce leader tónu:
 * 1. Sekvenční průchod pulzy od zadaného indexu
 * 2. Akumulace pulzů se shodnou délkou (v rámci tolerance od running average)
 * 3. Reset při překročení tolerance
 * 4. Nalezení leaderu při >= min_leader_pulses
 * 5. Zpřesnění statistik (průměr, směrodatná odchylka, min, max)
 *
 * Inspirováno Intercopy V10.2 a wavdec.c.
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

#include "wav_leader.h"


/** @brief Počáteční kapacita pole leader tónů. */
#define LEADER_LIST_INITIAL_CAPACITY    16


/**
 * @brief Zpřesní statistiky nalezeného leader tónu.
 *
 * Přepočítá průměr, směrodatnou odchylku, minimum a maximum
 * z finální sady pulzů leaderu.
 *
 * @param seq Sekvence pulzů.
 * @param leader Leader k zpřesnění. Musí mít platné start_index a pulse_count.
 */
static void leader_refine_stats ( const st_WAV_PULSE_SEQUENCE *seq, st_WAV_LEADER_INFO *leader ) {
    if ( leader->pulse_count == 0 ) return;

    double sum = 0.0;
    double min_val = 1e15;
    double max_val = 0.0;

    uint32_t end = leader->start_index + leader->pulse_count;
    if ( end > seq->count ) end = seq->count;

    for ( uint32_t i = leader->start_index; i < end; i++ ) {
        double d = seq->pulses[i].duration_us;
        sum += d;
        if ( d < min_val ) min_val = d;
        if ( d > max_val ) max_val = d;
    }

    leader->avg_period_us = sum / leader->pulse_count;
    leader->min_period_us = min_val;
    leader->max_period_us = max_val;

    /* směrodatná odchylka */
    double sum_sq = 0.0;
    for ( uint32_t i = leader->start_index; i < end; i++ ) {
        double diff = seq->pulses[i].duration_us - leader->avg_period_us;
        sum_sq += diff * diff;
    }
    leader->stddev_us = sqrt ( sum_sq / leader->pulse_count );
}


en_WAV_ANALYZER_ERROR wav_leader_detect (
    const st_WAV_PULSE_SEQUENCE *seq,
    uint32_t from_index,
    uint32_t min_pulses,
    double tolerance,
    st_WAV_LEADER_INFO *out_leader
) {
    if ( !seq || !seq->pulses || seq->count == 0 || !out_leader ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    if ( from_index >= seq->count ) {
        return WAV_ANALYZER_ERROR_NO_LEADER;
    }

    memset ( out_leader, 0, sizeof ( *out_leader ) );

    /*
     * Algoritmus:
     * - udržujeme running average z akumulovaných pulzů
     * - každý nový pulz porovnáme s running average
     * - pokud je v toleranci, přidáme do akumulátoru
     * - pokud není, resetujeme a začneme znovu
     * - leader nalezen, když akumulátor dosáhne min_pulses
     *
     * Kvantizační podlaha: pro rychlé signály (málo vzorků na
     * půl-periodu) je kvantizační chyba ±1 vzorek dominantní.
     * Např. při 3200 Bd / 44100 Hz má půl-perioda ~4 vzorky,
     * takže kvantizace dává ±22.7 us = ±25 % jitter.
     * Podlaha zajistí, že max_diff nebude menší než 1 vzorek.
     *
     * Bootstrap fáze: prvních BOOTSTRAP_COUNT pulzů používá
     * dvojnásobnou toleranci, protože running average je
     * nestabilní s málo vzorky (jeden odlehlý pulz driftuje
     * průměr příliš daleko).
     */
    double quant_floor_us = 1000000.0 / ( double ) seq->sample_rate;

    uint32_t run_start = from_index;
    double run_sum = seq->pulses[from_index].duration_us;
    uint32_t run_count = 1;

    /*
     * Bootstrap: pocet pulzu s dvojnasobnou toleranci.
     * Realne nahravky maji asymetricky duty cycle (HIGH != LOW
     * pul-perioda), takze prvnich min_pulses pulzu pouziva sirsi
     * toleranci. To zajisti uspesnou akumulaci i pro signaly
     * s vysokym jitterem (3200+ Bd pri 44100 Hz), kde kvantizace
     * a duty cycle asymetrie dohromady davaji ~25% variaci.
     * Po dosazeni min_pulses se running average stabilizuje
     * a rozsirovaci smycka pouziva standardni toleranci
     * s kvantizacni podlahou.
     */

    for ( uint32_t i = from_index + 1; i < seq->count; i++ ) {
        double pulse_us = seq->pulses[i].duration_us;
        double avg = run_sum / run_count;

        /* kontrola tolerance s kvantizační podlahou a bootstrap fází */
        double diff = fabs ( pulse_us - avg );
        double eff_tol = ( run_count < min_pulses )
                         ? tolerance * 2.0
                         : tolerance;
        double max_diff = avg * eff_tol;
        if ( max_diff < quant_floor_us ) max_diff = quant_floor_us;

        if ( diff <= max_diff ) {
            /* pulz je v toleranci - přidáme */
            run_sum += pulse_us;
            run_count++;

            /* zkontrolujeme, zda jsme dosáhli minimálního počtu */
            if ( run_count >= min_pulses ) {
                out_leader->start_index = run_start;
                out_leader->pulse_count = run_count;

                /*
                 * Pokračujeme dokud pulzy odpovídají - rozšíříme leader
                 * na maximální délku.
                 */
                for ( uint32_t j = i + 1; j < seq->count; j++ ) {
                    double next_us = seq->pulses[j].duration_us;
                    double cur_avg = run_sum / run_count;
                    double next_diff = fabs ( next_us - cur_avg );
                    double next_max = cur_avg * tolerance;
                    if ( next_max < quant_floor_us ) next_max = quant_floor_us;

                    if ( next_diff <= next_max ) {
                        run_sum += next_us;
                        run_count++;
                        out_leader->pulse_count = run_count;
                    } else {
                        break;
                    }
                }

                /* zpřesníme statistiky */
                leader_refine_stats ( seq, out_leader );
                return WAV_ANALYZER_OK;
            }
        } else {
            /* mimo toleranci - resetujeme */
            run_start = i;
            run_sum = pulse_us;
            run_count = 1;
        }
    }

    return WAV_ANALYZER_ERROR_NO_LEADER;
}


en_WAV_ANALYZER_ERROR wav_leader_find_all (
    const st_WAV_PULSE_SEQUENCE *seq,
    uint32_t min_pulses,
    double tolerance,
    st_WAV_LEADER_LIST *out_list
) {
    if ( !seq || !out_list ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    memset ( out_list, 0, sizeof ( *out_list ) );

    if ( seq->count == 0 ) {
        return WAV_ANALYZER_OK;
    }

    uint32_t search_from = 0;

    while ( search_from < seq->count ) {
        st_WAV_LEADER_INFO leader;
        en_WAV_ANALYZER_ERROR err = wav_leader_detect ( seq, search_from, min_pulses, tolerance, &leader );

        if ( err == WAV_ANALYZER_ERROR_NO_LEADER ) {
            /* žádný další leader nenalezen - konec */
            break;
        }

        if ( err != WAV_ANALYZER_OK ) {
            wav_leader_list_destroy ( out_list );
            return err;
        }

        /* přidáme leader do seznamu */
        if ( out_list->count >= out_list->capacity ) {
            uint32_t new_cap = ( out_list->capacity == 0 ) ? LEADER_LIST_INITIAL_CAPACITY : out_list->capacity * 2;
            st_WAV_LEADER_INFO *new_leaders = ( st_WAV_LEADER_INFO* ) realloc (
                                                  out_list->leaders, new_cap * sizeof ( st_WAV_LEADER_INFO )
                                              );
            if ( !new_leaders ) {
                wav_leader_list_destroy ( out_list );
                return WAV_ANALYZER_ERROR_ALLOC;
            }
            out_list->leaders = new_leaders;
            out_list->capacity = new_cap;
        }

        out_list->leaders[out_list->count] = leader;
        out_list->count++;

        /* pokračujeme za koncem nalezeného leaderu */
        search_from = leader.start_index + leader.pulse_count;
    }

    return WAV_ANALYZER_OK;
}
