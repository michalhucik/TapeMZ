/**
 * @file   mzcmt_cpmtape.h
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  CPM-TAPE koder pro kazetovy zaznam Sharp MZ (Pezik/MarVan format).
 *
 * Knihovna implementuje CPM-TAPE modulaci pouzivnou programy ZTAPE V2.0
 * (Pezik/MarVan, 1987) a TAPE.COM V2.0a pro CP/M na Sharp MZ-800.
 *
 * CPM-TAPE je unikatni format odlisny od standardniho FM (NORMAL) i od
 * ZX Spectrum kodovani. Pouziva manchesterske kodovani (biphase):
 * bit 1 = HIGH-LOW, bit 0 = LOW-HIGH. Bajty se kodují LSB first,
 * bez stop bitu (8 bitu na bajt).
 *
 * @par Struktura CPM-TAPE signalu:
 * @verbatim
 *   [HEADER BLOK]
 *     Pilot:      N opakovani sekvence "011" (bit0 s prodlouzenym timingem)
 *     Sync:       1 bit "1" s velmi prodlouzenym timingem
 *     Flag:       $00 (8 bitu LSB first)
 *     Data:       128 bajtu MZF hlavicky (8 bitu LSB first na bajt)
 *     Checksum:   16-bit soucet, low byte first
 *     Separator:  1 bit "1" s prodlouzenym timingem
 *
 *   [GAP - ticho]
 *
 *   [BODY BLOK]
 *     Pilot:      M opakovani sekvence "011" (bit0 s prodlouzenym timingem)
 *     Sync:       1 bit "1" s velmi prodlouzenym timingem
 *     Flag:       $01 (8 bitu LSB first)
 *     Data:       body_size bajtu (body_size*8-1 bitu, posledni bit vynechan)
 *     Checksum:   16-bit soucet plnych dat, low byte first
 *     Separator:  1 bit "1" s prodlouzenym timingem
 * @endverbatim
 *
 * @par Kodovani bitu:
 * @verbatim
 *   Bit 1: HIGH(T/2) pak LOW(T/2)
 *   Bit 0: LOW(T/2) pak HIGH(T/2)
 * @endverbatim
 * kde T = perioda bitu (1/baud_rate). Oba pul-periody maji stejnou delku.
 *
 * @par Bajty:
 * LSB first (extrakce pres RRC D instrukci), 8 bitu, BEZ stop bitu.
 * Toto je klicovy rozdil oproti NORMAL FM (MSB first, 1 stop bit).
 *
 * @par Pilot:
 * Opakujici se sekvence "011" = bit0(extended) + bit1(normal) + bit1(normal).
 * Bit 0 v pilotu ma prodlouzeny timing (2x normalni pulperioda), coz
 * vytvari rozpoznatelny vzor pro detektor pilotu v TAPE.COM dekoderu.
 *
 * @par Sync:
 * Jediny bit 1 s velmi prodlouzenym timingem (cca 5x normalni pulperioda).
 * Oznacuje konec pilotu a zacatek datoveho bloku.
 *
 * @par Chybejici posledni bit v body bloku:
 * Body blok se na pasce posila zkraceny o 1 bit - MSB posledniho bajtu
 * (protoze se koduje LSB first, je to 8. odesílany bit) se neprenasi.
 * Dekoder rekonstruuje chybejici bit z 16-bitoveho checksumu.
 *
 * @par Rychlosti:
 * - 1200 Bd (kompatibilni s ROM rychlosti)
 * - 2400 Bd (vychozi pro TAPE.COM)
 * - 3200 Bd
 *
 * @par Pouziti v kontextu TMZ:
 * CPM-TAPE se pouziva vyhradne pod CP/M (programy ZTAPE/TAPE.COM).
 * Na pasce neni zadny loader - TAPE.COM je jiz nacteny v pameti.
 * Koder generuje kompletni signal (header blok + body blok).
 * TMZ blok 0x41 s format=CPM_TAPE.
 *
 * @par Referencni implementace:
 * - docs/detailne-prozkoumat/cpm/progs/ZTAPE.COM (17024 B)
 * - docs/detailne-prozkoumat/cpm/progs/Tape.com (2944 B)
 * - docs/detailne-prozkoumat/cpm/takze-hotovo.txt (analyza formatu)
 * - docs/detailne-prozkoumat/cpm/cpm-tape-formats.md
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


#ifndef MZCMT_CPMTAPE_H
#define MZCMT_CPMTAPE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "libs/cmt_stream/cmt_stream.h"
#include "libs/mzf/mzf.h"


    /**
     * @brief Rychlostni urovne CPM-TAPE koderu.
     *
     * TAPE.COM podporuje 3 rychlosti. Vychozi je 2400 Bd.
     * Rychlost urcuje periodu jednoho bitu (1/baud_rate).
     */
    typedef enum en_MZCMT_CPMTAPE_SPEED {
        MZCMT_CPMTAPE_SPEED_1200 = 0, /**< 1200 Bd (kompatibilni s ROM rychlosti) */
        MZCMT_CPMTAPE_SPEED_2400,      /**< 2400 Bd (vychozi pro TAPE.COM) */
        MZCMT_CPMTAPE_SPEED_3200,      /**< 3200 Bd */
        MZCMT_CPMTAPE_SPEED_COUNT      /**< Pocet platnych hodnot (sentinel). */
    } en_MZCMT_CPMTAPE_SPEED;


    /**
     * @brief Chybove kody CPM-TAPE koderu.
     */
    typedef enum en_MZCMT_CPMTAPE_ERROR {
        MZCMT_CPMTAPE_OK = 0,              /**< Bez chyby. */
        MZCMT_CPMTAPE_ERROR_NULL_INPUT,     /**< Vstupni data jsou NULL. */
        MZCMT_CPMTAPE_ERROR_ZERO_SIZE,      /**< Velikost dat je 0. */
        MZCMT_CPMTAPE_ERROR_INVALID_RATE,   /**< Neplatna vzorkovaci frekvence (0). */
        MZCMT_CPMTAPE_ERROR_INVALID_SPEED,  /**< Neplatna rychlost. */
        MZCMT_CPMTAPE_ERROR_VSTREAM,        /**< Chyba pri vytvareni nebo zapisu do vstreamu. */
    } en_MZCMT_CPMTAPE_ERROR;


    /** @brief Pocet opakovani "011" sekvence v pilotu hlavickoveho bloku. */
#define MZCMT_CPMTAPE_PILOT_HDR_COUNT  2000

    /** @brief Pocet opakovani "011" sekvence v pilotu body bloku. */
#define MZCMT_CPMTAPE_PILOT_BODY_COUNT  800

    /**
     * @brief Nasobitel pulperiody pro prodlouzeny bit v pilotu.
     *
     * Bit 0 v pilotni sekvenci "011" ma prodlouzeny timing, aby
     * detektor pilotu v TAPE.COM dokázal rozpoznat pilotni vzor.
     */
#define MZCMT_CPMTAPE_PILOT_EXTEND  2.0

    /**
     * @brief Nasobitel pulperiody pro sync bit.
     *
     * Sync bit (bit 1 s prodlouzeným timingem) oznacuje konec pilotu
     * a zacatek datoveho bloku. V originalnim TAPE.COM ma B=$FF,
     * coz odpovida cca 5x normalnimu timingu.
     */
#define MZCMT_CPMTAPE_SYNC_EXTEND   5.0

    /**
     * @brief Delka ticha (gap) mezi header a body bloky v sekundach.
     *
     * Umoznuje dekoderu zpracovat hlavicku pred ctenim body bloku.
     */
#define MZCMT_CPMTAPE_GAP_SECONDS   0.5


    /**
     * @brief Konfigurace CPM-TAPE koderu.
     *
     * @par Invarianty:
     * - speed musi byt < MZCMT_CPMTAPE_SPEED_COUNT
     */
    typedef struct st_MZCMT_CPMTAPE_CONFIG {
        en_MZCMT_CPMTAPE_SPEED speed; /**< Rychlost prenosu (1200/2400/3200 Bd). */
    } st_MZCMT_CPMTAPE_CONFIG;


    /**
     * @brief Vytvori CMT vstream s kompletnim CPM-TAPE signalem.
     *
     * Generuje dvoudilny signal na pasce:
     * 1. Header blok: pilot + sync + flag($00) + 128B hlavicky + checksum + separator
     * 2. Gap (ticho)
     * 3. Body blok: pilot + sync + flag($01) + data + checksum + separator
     *
     * Body blok je zkraceny o posledni bit (MSB posledniho bajtu).
     * Dekoder rekonstruuje chybejici bit z 16-bitoveho checksumu.
     *
     * Signal zacina v LOW stavu.
     *
     * @param header    MZF hlavicka (bude serializovana do 128B LE formatu).
     * @param body      Ukazatel na datove telo k zakodovani (muze byt NULL pokud body_size==0).
     * @param body_size Velikost datoveho tela v bajtech.
     * @param config    Konfigurace koderu (rychlost).
     * @param rate      Vzorkovaci frekvence vystupniho vstreamu (Hz, musi byt > 0).
     * @return Ukazatel na novy vstream, nebo NULL pri chybe.
     *
     * @pre @p header != NULL
     * @pre @p body != NULL pokud @p body_size > 0
     * @pre @p config != NULL
     * @pre @p config->speed < MZCMT_CPMTAPE_SPEED_COUNT
     * @pre @p rate > 0
     *
     * @post Navratova hodnota != NULL => volajici vlastni vstream a musi
     *       jej uvolnit volanim cmt_vstream_destroy().
     * @post Navratova hodnota == NULL => zadna pamet nebyla alokovana.
     */
    extern st_CMT_VSTREAM* mzcmt_cpmtape_create_vstream (
        const st_MZF_HEADER *header,
        const uint8_t *body,
        uint32_t body_size,
        const st_MZCMT_CPMTAPE_CONFIG *config,
        uint32_t rate
    );


    /**
     * @brief Vytvori jednotny CMT stream s kompletnim CPM-TAPE signalem.
     *
     * Obaluje mzcmt_cpmtape_create_vstream() do polymorfniho st_CMT_STREAM.
     * Pro typ CMT_STREAM_TYPE_BITSTREAM interno vytvori vstream a konvertuje.
     *
     * @param header    MZF hlavicka.
     * @param body      Ukazatel na datove telo.
     * @param body_size Velikost datoveho tela v bajtech.
     * @param config    Konfigurace koderu.
     * @param type      Typ vystupniho streamu (bitstream/vstream).
     * @param rate      Vzorkovaci frekvence (Hz, musi byt > 0).
     * @return Ukazatel na novy stream, nebo NULL pri chybe.
     *
     * @pre @p header != NULL
     * @pre @p body != NULL pokud @p body_size > 0
     * @pre @p config != NULL
     * @pre @p rate > 0
     *
     * @post Navratova hodnota != NULL => volajici vlastni stream a musi
     *       jej uvolnit volanim cmt_stream_destroy().
     */
    extern st_CMT_STREAM* mzcmt_cpmtape_create_stream (
        const st_MZF_HEADER *header,
        const uint8_t *body,
        uint32_t body_size,
        const st_MZCMT_CPMTAPE_CONFIG *config,
        en_CMT_STREAM_TYPE type,
        uint32_t rate
    );


    /**
     * @brief Spocita 16-bitovy checksum (soucet bajtu).
     *
     * CPM-TAPE pouziva jednoduchy 16-bitovy soucet vsech bajtu
     * v datovem bloku (vcetne flag bajtu). Soucet se pocita na plnych
     * datech (vsech 8 bitu kazdeho bajtu), i kdyz se posledni bit
     * body bloku neprenasi.
     *
     * @param data Ukazatel na datovy blok (vcetne flag bajtu na zacatku).
     * @param size Velikost bloku v bajtech.
     * @return 16-bitovy soucet. Vraci 0 pro NULL nebo size==0.
     */
    extern uint16_t mzcmt_cpmtape_compute_checksum ( const uint8_t *data, uint32_t size );


    /** @brief Uzivatelsky alokator - umoznuje nahradit vychozi malloc/calloc/free. */
    typedef struct st_MZCMT_CPMTAPE_ALLOCATOR {
        void* (*alloc)(size_t size);   /**< alokace pameti (jako malloc) */
        void* (*alloc0)(size_t size);  /**< alokace s nulovanim (jako calloc) */
        void  (*free)(void *ptr);      /**< uvolneni pameti */
    } st_MZCMT_CPMTAPE_ALLOCATOR;

    /**
     * @brief Nastavi vlastni alokator pro knihovnu mzcmt_cpmtape.
     * @param allocator Ukazatel na strukturu alokatoru, nebo NULL pro reset na vychozi.
     */
    extern void mzcmt_cpmtape_set_allocator ( const st_MZCMT_CPMTAPE_ALLOCATOR *allocator );


    /** @brief Typ callback funkce pro hlaseni chyb. */
    typedef void (*mzcmt_cpmtape_error_cb)(const char *func, int line, const char *fmt, ...);

    /**
     * @brief Nastavi vlastni callback pro chybova hlaseni.
     * @param cb Callback funkce, nebo NULL pro reset na vychozi (fprintf stderr).
     */
    extern void mzcmt_cpmtape_set_error_callback ( mzcmt_cpmtape_error_cb cb );


    /** @brief Verze knihovny mzcmt_cpmtape. */
#define MZCMT_CPMTAPE_VERSION "1.0.0"

    /**
     * @brief Vrátí řetězec s verzí knihovny mzcmt_cpmtape.
     * @return Statický řetězec s verzí (např. "1.0.0").
     */
    extern const char* mzcmt_cpmtape_version ( void );


#ifdef __cplusplus
}
#endif

#endif /* MZCMT_CPMTAPE_H */
