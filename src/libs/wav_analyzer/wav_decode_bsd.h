/**
 * @file   wav_decode_bsd.h
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Vrstva 4d analyzeru - BSD dekoder (chunkovany BASIC datovy format).
 *
 * Dekoduje BSD/BRD format z WAV signalu. BSD signal se sklada z:
 *
 * Hlavicka:
 *   LGAP + LTM + 2L + HDR(128B, ftype=0x03/0x04, fsize=0) + CHKH + 2L
 *   + [kopie] + SGAP
 *
 * Chunky (opakovane):
 *   STM + 2L + CHUNK(258B = 2B ID + 256B data) + CHK + 2L
 *
 * Chunk ID:
 * - Prvni chunk: 0x0000
 * - Nasledujici: inkrementuje se (0x0001, 0x0002, ...)
 * - Posledni chunk: 0xFFFF (ukoncujici marker)
 *
 * Vsechny chunky pouzivaji standardni NORMAL FM modulaci.
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


#ifndef WAV_DECODE_BSD_H
#define WAV_DECODE_BSD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "libs/mzf/mzf.h"

#include "wav_analyzer_types.h"


    /** @brief Velikost jednoho BSD chunku (2B ID + 256B data). */
#define WAV_BSD_CHUNK_SIZE          258

    /** @brief Velikost datove casti jednoho chunku. */
#define WAV_BSD_CHUNK_DATA_SIZE     256

    /** @brief Chunk ID posledniho chunku (ukoncujici marker). */
#define WAV_BSD_LAST_CHUNK_ID       0xFFFF

    /** @brief Maximalni pocet chunku (ochrana proti nekonecne smycce). */
#define WAV_BSD_MAX_CHUNKS          256


    /**
     * @brief Dekoduje kompletni MZF soubor z BSD signalu.
     *
     * Predpoklada, ze FM dekoder jiz dekodoval hlavicku
     * (ftype=0x03/0x04, fsize=0). Tato funkce:
     * 1. Najde prvni chunk (STM za SGAP)
     * 2. Iterativne dekoduje chunky (258B = 2B ID + 256B data)
     * 3. Konci pri chunk ID == 0xFFFF
     * 4. Sestavi MZF z hlavicky a zretezenych dat chunku
     *
     * @param seq Sekvence pulzu. Nesmi byt NULL.
     * @param leader Informace o leader tonu hlavicky (pro kalibraci prahu).
     *        Nesmi byt NULL.
     * @param header Dekodovana MZF hlavicka (po endianity korekci).
     *        Nesmi byt NULL.
     * @param search_from_pulse Pozice od ktere hledat chunky
     *        (typicky hdr_res.pulse_end z FM dekoderu).
     * @param[out] out_mzf Vystupni MZF struktura (volajici uvolni pres mzf_free).
     *             Nesmi byt NULL.
     * @param[out] out_body_result Vysledek dekodovani tela (posledni chunk).
     *             Muze byt NULL.
     * @param[out] out_consumed_until Pozice za poslednim zpracovanym pulzem.
     *             Muze byt NULL.
     * @return WAV_ANALYZER_OK pri uspechu, jinak chybovy kod.
     *
     * @pre FM dekoder jiz dekodoval hlavicku a klasifikator urcil
     *      format jako WAV_TAPE_FORMAT_BSD.
     * @post Pri uspechu *out_mzf obsahuje MZF s hlavickou a zretezenymi daty.
     */
    extern en_WAV_ANALYZER_ERROR wav_decode_bsd_decode_mzf (
        const st_WAV_PULSE_SEQUENCE *seq,
        const st_WAV_LEADER_INFO *leader,
        const st_MZF_HEADER *header,
        uint32_t search_from_pulse,
        st_MZF **out_mzf,
        st_WAV_DECODE_RESULT *out_body_result,
        uint32_t *out_consumed_until
    );


#ifdef __cplusplus
}
#endif

#endif /* WAV_DECODE_BSD_H */
