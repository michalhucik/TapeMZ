/**
 * @file   wav_decode_cpmtape.h
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Vrstva 4e2 analyzeru - CPM-TAPE dekoder (Pezik/MarVan format).
 *
 * Dekoduje CPM-TAPE signal z WAV nahravky. CPM-TAPE je unikatni format
 * pouzivany programy ZTAPE V2.0 a TAPE.COM pro CP/M na Sharp MZ-800.
 *
 * Klicove odlisnosti od FM formatu (NORMAL/TURBO/FASTIPL/BSD):
 * - Manchesterske kodovani (bit 1 = HIGH-LOW, bit 0 = LOW-HIGH)
 * - Bajty se kodují LSB first (ne MSB first jako FM)
 * - Bez stop bitu (8 bitu na bajt, ne 9)
 * - Pilot: opakujici se "011" sekvence (bit 0 s prodlouzenym timingem)
 * - Sync: bit 1 s 5x prodlouzenym timingem
 * - Flag bajt: $00 = hlavicka, $01 = telo
 * - Checksum: 16-bit soucet bajtu (vcetne flag bajtu)
 * - Posledni bit body bloku se neprenasi (rekonstrukce z checksumu)
 *
 * Standardni leader detektor CPM-TAPE pilot NEDETEKUJE, protoze pilot
 * obsahuje pulzy T/2 a 3T/2 (variace 200%, daleko za toleranci).
 * Dekoder proto pouziva vlastni sync detekci - hleda velmi dlouhe
 * pulzy (5*T/2) predchazene pilotem s charakteristickym vzorem.
 *
 * @par Rychlosti:
 * - 1200 Bd (T = 833 us, sync = 2083 us)
 * - 2400 Bd (T = 417 us, sync = 1042 us, vychozi TAPE.COM)
 * - 3200 Bd (T = 312 us, sync = 781 us)
 *
 * @par Struktura signalu:
 * @verbatim
 *   [HEADER BLOK]
 *     Pilot:      2000x "011" (bit0 s 2x timingem, bit1 normal, bit1 normal)
 *     Sync:       bit 1 s 5x timingem (HIGH(5T/2) + LOW(5T/2))
 *     Flag:       $00 (8 bitu LSB first)
 *     Data:       128 bajtu MZF hlavicky
 *     Checksum:   16-bit soucet (low byte first)
 *     Separator:  bit 1 s 5x timingem
 *
 *   [GAP - ticho 0.5 s]
 *
 *   [BODY BLOK]
 *     Pilot:      800x "011"
 *     Sync:       bit 1 s 5x timingem
 *     Flag:       $01 (8 bitu LSB first)
 *     Data:       body_size*8-1 bitu (MSB posledniho bajtu vynechan)
 *     Checksum:   16-bit soucet plnych dat (low byte first)
 *     Separator:  bit 1 s 5x timingem
 * @endverbatim
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


#ifndef WAV_DECODE_CPMTAPE_H
#define WAV_DECODE_CPMTAPE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "libs/mzf/mzf.h"

#include "wav_analyzer_types.h"


    /**
     * @brief Minimalni absolutni delka sync pul-periody v us.
     *
     * Sync ma 5*T/2. Pri 3200 Bd (nejrychlejsi) je to 781 us.
     * 400 us je bezpecny dolni limit, ktery vylucuje vse krome CPM-TAPE sync.
     */
#define WAV_CPMTAPE_SYNC_MIN_US        400.0

    /**
     * @brief Pocet pulzu pred sync pro verifikaci pilotu.
     *
     * Kontrolujeme, ze pred sync je pilot s charakteristickym vzorem
     * (smes pulzu T/2 a 3T/2 v pomeru 1:3).
     */
#define WAV_CPMTAPE_PILOT_WINDOW       40

    /**
     * @brief Minimalni pocet kratkych pulzu v pilonim okne.
     */
#define WAV_CPMTAPE_PILOT_MIN_SHORT    4

    /**
     * @brief Minimalni pocet dlouhych (3T/2) pulzu v pilotnim okne.
     */
#define WAV_CPMTAPE_PILOT_MIN_LONG     4

    /**
     * @brief Minimalni pomer sync/short pro validni sync.
     *
     * Sync = 5*T/2, short = T/2, takze idealní pomer = 5.0.
     * Povolujeme rozsah 3.5 - 7.0 pro toleranci.
     */
#define WAV_CPMTAPE_SYNC_RATIO_MIN     3.5

    /** @brief Maximalni pomer sync/short. */
#define WAV_CPMTAPE_SYNC_RATIO_MAX     7.0

    /**
     * @brief Minimalni pomer long/short pulzu v pilotu.
     *
     * Pilot ma pulzy T/2 (short) a 3T/2 (long), idealní pomer = 3.0.
     * Povolujeme rozsah 2.0 - 4.5.
     */
#define WAV_CPMTAPE_PILOT_RATIO_MIN    2.0

    /** @brief Maximalni pomer long/short pulzu v pilotu. */
#define WAV_CPMTAPE_PILOT_RATIO_MAX    4.5

    /** @brief Ocekavany flag bajt hlavickoveho bloku. */
#define WAV_CPMTAPE_FLAG_HEADER        0x00

    /** @brief Ocekavany flag bajt body bloku. */
#define WAV_CPMTAPE_FLAG_BODY          0x01


    /**
     * @brief Dekoduje kompletni MZF soubor z CPM-TAPE signalu.
     *
     * Prohledava sekvenci pulzu od pozice @p search_from a hleda
     * CPM-TAPE sync predchazeny pilotem. Dekoduje hlavickovy blok,
     * pak hleda body sync a dekoduje body blok.
     *
     * Algoritmus:
     * 1. Najde header sync (dlouhy puls predchazeny pilotem "011")
     * 2. Kalibruje bitovou periodu T z kratkych pulzu pilotu
     * 3. Dekoduje flag ($00) + 128B hlavicky + checksum
     * 4. Najde body sync (pokud fsize > 0)
     * 5. Dekoduje flag ($01) + data + checksum
     * 6. Rekonstruuje chybejici MSB posledniho bajtu z checksumu
     * 7. Sestavi MZF strukturu
     *
     * @param seq Sekvence pulzu. Nesmi byt NULL.
     * @param search_from Pozice od ktere zacit hledat sync.
     * @param[out] out_mzf Vystupni MZF struktura. Volajici uvolni pres mzf_free().
     *             Nesmi byt NULL.
     * @param[out] out_header_result Vysledek dekodovani hlavicky. Muze byt NULL.
     * @param[out] out_body_result Vysledek dekodovani tela. Muze byt NULL.
     * @param[out] out_consumed_until Pozice za poslednim zpracovanym pulzem.
     *             Muze byt NULL.
     * @return WAV_ANALYZER_OK pri uspechu, jinak chybovy kod.
     *
     * @post Pri uspechu *out_mzf obsahuje dekodovany MZF soubor.
     */
    extern en_WAV_ANALYZER_ERROR wav_decode_cpmtape_decode_mzf (
        const st_WAV_PULSE_SEQUENCE *seq,
        uint32_t search_from,
        st_MZF **out_mzf,
        st_WAV_DECODE_RESULT *out_header_result,
        st_WAV_DECODE_RESULT *out_body_result,
        uint32_t *out_consumed_until
    );


#ifdef __cplusplus
}
#endif

#endif /* WAV_DECODE_CPMTAPE_H */
