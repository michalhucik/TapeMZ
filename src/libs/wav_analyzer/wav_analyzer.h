/**
 * @file   wav_analyzer.h
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Hlavní API knihovny wav_analyzer - orchestrace všech vrstev.
 *
 * Poskytuje vysokoúrovňové API pro analýzu WAV nahrávek magnetofonových
 * kazet Sharp MZ. Interně orchestruje vrstvy 0-4:
 *   0. Preprocessing (DC offset, HP filtr, normalizace)
 *   1. Pulse Extraction (zero-crossing / Schmitt trigger)
 *   2. Leader Detection (detekce leader tónů)
 *   2b. Histogram Analysis (diagnostika, auto prahy)
 *   3. Format Classification (NORMAL/SINCLAIR/CPM/MZ-80B/TURBO/FASTIPL/BSD/FSK/SLOW/DIRECT)
 *   4. Specialized Decoding (FM, TURBO, FASTIPL, BSD, CPM-TAPE, FSK, SLOW, DIRECT dekodéry)
 *
 * Výstupem jsou dekódované MZF soubory a diagnostické informace.
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


#ifndef WAV_ANALYZER_H
#define WAV_ANALYZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "libs/generic_driver/generic_driver.h"
#include "libs/wav/wav.h"
#include "libs/mzf/mzf.h"

#include "wav_analyzer_types.h"
#include "wav_preprocess.h"
#include "wav_pulse.h"
#include "wav_leader.h"
#include "wav_histogram.h"
#include "wav_classify.h"
#include "wav_decode_fm.h"
#include "wav_decode_turbo.h"
#include "wav_decode_fastipl.h"
#include "wav_decode_bsd.h"
#include "wav_decode_cpmtape.h"
#include "wav_decode_fsk.h"
#include "wav_decode_slow.h"
#include "wav_decode_direct.h"
#include "wav_decode_zx.h"


    /**
     * @brief Výsledek analýzy jednoho MZF souboru z WAV.
     *
     * Obsahuje dekódovaný MZF soubor, detekovaný formát,
     * CRC status hlavičky a těla a diagnostické informace.
     */
    typedef struct st_WAV_ANALYZER_FILE_RESULT {
        st_MZF *mzf;                       /**< dekódovaný MZF soubor (vlastní paměť, NULL pro ZX bloky) */
        uint8_t *tap_data;                 /**< surová TAP data pro ZX bloky (flag + payload + checksum, heap) */
        uint32_t tap_data_size;            /**< velikost TAP dat v bajtech (0 pro MZ formáty) */
        en_WAV_TAPE_FORMAT format;          /**< detekovaný kazetový formát */
        en_WAV_CRC_STATUS header_crc;       /**< CRC status hlavičky */
        en_WAV_CRC_STATUS body_crc;         /**< CRC status těla */
        st_WAV_LEADER_INFO leader;          /**< informace o leader tónu */
        en_WAV_SPEED_CLASS speed_class;     /**< rychlostní třída */
        int copy2_used;                     /**< 1 = data pochází z Copy2 (druhé kopie) */
        uint32_t consumed_until_pulse;     /**< pozice za posledním zpracovaným pulzem
                                                (pro přeskočení leaderů spotřebovaných
                                                dvoudílnými formáty jako TURBO/FASTIPL) */
    } st_WAV_ANALYZER_FILE_RESULT;


    /**
     * @brief Výsledek kompletní analýzy WAV souboru.
     *
     * WAV soubor může obsahovat více MZF souborů (více leader tónů).
     * Výsledek obsahuje seznam všech nalezených souborů.
     *
     * @par Invarianty:
     * - file_count <= file_capacity
     * - pokud file_count > 0, files != NULL
     */
    typedef struct st_WAV_ANALYZER_RESULT {
        st_WAV_ANALYZER_FILE_RESULT *files;     /**< pole výsledků (alokováno na heapu) */
        uint32_t file_count;                    /**< počet nalezených souborů */
        uint32_t file_capacity;                 /**< alokovaná kapacita pole */
        st_WAV_LEADER_LIST leaders;             /**< všechny nalezené leader tóny */
        uint32_t total_pulses;                  /**< celkový počet extrahovaných pulzů */
        uint32_t sample_rate;                   /**< vzorkovací frekvence WAV */
        double wav_duration_sec;                /**< délka WAV souboru v sekundách */
        st_WAV_PULSE_STATS pulse_stats;         /**< statistiky pulzů (průměr, stddev, min, max) */
        st_WAV_ANALYZER_RAW_BLOCK *raw_blocks;  /**< neidentifikované bloky (alokováno na heapu) */
        uint32_t raw_block_count;               /**< počet neidentifikovaných bloků */
        uint32_t raw_block_capacity;            /**< alokovaná kapacita pole raw bloků */
    } st_WAV_ANALYZER_RESULT;


    /**
     * @brief Analyzuje WAV soubor a dekóduje všechny nalezené MZF soubory.
     *
     * Hlavní API funkce. Provádí kompletní pipeline:
     * 1. Parsování WAV hlavičky
     * 2. Preprocessing (dle konfigurace)
     * 3. Extrakce pulzů
     * 4. Detekce všech leader tónů
     * 5. Pro každý leader: klasifikace formátu + dekódování
     *
     * @param h Otevřený handler s WAV daty. Nesmí být NULL.
     * @param config Konfigurace analyzéru. Nesmí být NULL.
     * @param[out] out_result Výstupní výsledek. Nesmí být NULL.
     *             Volající musí uvolnit přes wav_analyzer_result_destroy().
     * @return WAV_ANALYZER_OK při úspěchu, jinak chybový kód.
     *
     * @post Při úspěchu out_result obsahuje všechny nalezené soubory.
     *       I bez nalezených souborů je výsledek platný (file_count=0).
     */
    extern en_WAV_ANALYZER_ERROR wav_analyzer_analyze (
        st_HANDLER *h,
        const st_WAV_ANALYZER_CONFIG *config,
        st_WAV_ANALYZER_RESULT *out_result
    );


    /**
     * @brief Uvolní výsledek analýzy včetně všech MZF souborů.
     *
     * Bezpečné volání s NULL (no-op).
     *
     * @param result Ukazatel na výsledek k uvolnění (může být NULL).
     */
    extern void wav_analyzer_result_destroy ( st_WAV_ANALYZER_RESULT *result );


    /**
     * @brief Vypíše shrnutí výsledku analýzy na výstupní proud.
     *
     * Diagnostický výpis s přehledem nalezených souborů,
     * formátů, CRC statusů a leader informací.
     *
     * @param result Výsledek analýzy. Nesmí být NULL.
     * @param stream Výstupní proud (typicky stdout nebo stderr).
     */
    extern void wav_analyzer_print_summary (
        const st_WAV_ANALYZER_RESULT *result,
        FILE *stream
    );


    /** @brief Verze knihovny wav_analyzer. */
#define WAV_ANALYZER_VERSION "1.0.0"

    /**
     * @brief Vrátí řetězec s verzí knihovny wav_analyzer.
     * @return Statický řetězec s verzí (např. "1.0.0").
     */
    extern const char* wav_analyzer_version ( void );

#ifdef __cplusplus
}
#endif

#endif /* WAV_ANALYZER_H */
