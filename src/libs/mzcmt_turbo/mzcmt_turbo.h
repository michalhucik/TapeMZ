/**
 * @file   mzcmt_turbo.h
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 2.0.0
 * @brief  TURBO koder pro kazetovy zaznam Sharp MZ (NORMAL FM s konfigurovatelnym casovanim).
 *
 * Knihovna implementuje TURBO kodovani pro Sharp MZ. TURBO je standardni
 * FM modulace (1 bit na pulz, MSB first, stop bit za kazdym bajtem) se
 * zmenenymi pulznimi casovanimi oproti ROM vychozim hodnotam. Pouzivaji
 * jej programy TurboCopy v1.0 a v1.2x, ktere kopirouji ROM do RAM a meni
 * readpoint prodlevu pro rychlejsi cteni z pasky.
 *
 * Na rozdil od FSK/SLOW/DIRECT koderu, ktere generuji signal pouze pro
 * datove telo, TURBO koder generuje kompletni paskovy ramec vcetne GAP,
 * tapemarku, hlavicky, tela, checksumu a volitelnych kopii. Duvod: TURBO
 * pouziva stejnou ramcovou strukturu jako standardni NORMAL format.
 *
 * @par Struktura TURBO signalu (SANE - bez kopii):
 * @verbatim
 *   [LGAP]      lgap_length kratkych pulzu (vychozi 22000)
 *   [LTM]       40 long + 40 short pulzu (dlouhy tapemark)
 *   [2L]        2 long pulzy
 *   [HDR]       128 bajtu hlavicky (8 bitu + 1 stop bit na bajt, MSB first)
 *   [CHKH]      2 bajty checksum hlavicky (big-endian, pocet 1-bitu)
 *   [2L]        2 long pulzy
 *   [SGAP]      sgap_length kratkych pulzu (vychozi 11000)
 *   [STM]       20 long + 20 short pulzu (kratky tapemark)
 *   [2L]        2 long pulzy
 *   [BODY]      N bajtu tela (8 bitu + 1 stop bit na bajt, MSB first)
 *   [CHKB]      2 bajty checksum tela (big-endian, pocet 1-bitu)
 *   [2L]        2 long pulzy
 * @endverbatim
 *
 * @par Volitelne kopie (dle flagu):
 * - TMZ_TURBO_FLAG_HEADER_COPY: za CHKH+2L nasleduje 256S + HDR + CHKH + 2L
 * - TMZ_TURBO_FLAG_BODY_COPY: za CHKB+2L nasleduje 256S + BODY + CHKB + 2L
 *
 * @par Pulzni casovani:
 * Konfiguracni struktura umoznuje explicitni nastaveni delek pulzu
 * (v jednotkach us*100 = 10 ns rozliseni). Pokud jsou nulove, pouziji se
 * vychozi hodnoty odvozene z pulsesetu a rychlosti (cmtspeed divisor).
 *
 * @par Checksum:
 * NORMAL FM checksum = 16bitovy pocet jednickovych bitu v datech,
 * odesilany big-endian (horni bajt prvni). Shodny s ROM algoritmem.
 *
 * @par Referencni implementace:
 * - docs/detailne-prozkoumat/cmttool/src/libs/cmttool/cmttool_turbo.c (detekce loaderu)
 * - docs/detailne-prozkoumat/mzftools/src/loader.c (readpoint delay tabulky)
 * - docs/detailne-prozkoumat/mzftools/src/turbo.asm (Z80 loader)
 * - src/libs/mztape/mztape.c (referencni NORMAL FM implementace)
 *
 * @par Licence:
 * Odvozeno z projektu MZFTools v0.2.2 (mz-fuzzy@users.sf.net),
 * sireneho pod GNU General Public License v3 (GPLv3).
 *
 * Copyright (C) MZFTools authors (mz-fuzzy@users.sf.net)
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


#ifndef MZCMT_TURBO_H
#define MZCMT_TURBO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "libs/cmt_stream/cmt_stream.h"
#include "libs/cmtspeed/cmtspeed.h"
#include "libs/mzf/mzf.h"


    /**
     * @brief Pulzni sady pro vychozi casovani (indexovano pulseset hodnotou).
     *
     * Mapuje se na en_MZTAPE_PULSESET z mztape.h. Definujeme vlastni enum,
     * aby mzcmt_turbo nezaviselo na mztape.
     *
     * Invariant: hodnota musi byt < MZCMT_TURBO_PULSESET_COUNT.
     */
    typedef enum en_MZCMT_TURBO_PULSESET {
        MZCMT_TURBO_PULSESET_700 = 0, /**< MZ-700, MZ-80K, MZ-80A. */
        MZCMT_TURBO_PULSESET_800,     /**< MZ-800, MZ-1500. */
        MZCMT_TURBO_PULSESET_80B,     /**< MZ-80B. */
        MZCMT_TURBO_PULSESET_COUNT    /**< Pocet platnych hodnot (sentinel). */
    } en_MZCMT_TURBO_PULSESET;


    /**
     * @brief Chybove kody TURBO koderu.
     */
    typedef enum en_MZCMT_TURBO_ERROR {
        MZCMT_TURBO_OK = 0,               /**< Bez chyby. */
        MZCMT_TURBO_ERROR_NULL_HEADER,     /**< Ukazatel na hlavicku je NULL. */
        MZCMT_TURBO_ERROR_NULL_BODY,       /**< Ukazatel na body je NULL pri body_size > 0. */
        MZCMT_TURBO_ERROR_NULL_CONFIG,     /**< Konfiguracni struktura je NULL. */
        MZCMT_TURBO_ERROR_INVALID_PULSESET, /**< Neplatna pulzni sada. */
        MZCMT_TURBO_ERROR_INVALID_SPEED,   /**< Neplatna rychlost. */
        MZCMT_TURBO_ERROR_INVALID_RATE,    /**< Neplatna vzorkovaci frekvence (0). */
        MZCMT_TURBO_ERROR_VSTREAM,         /**< Chyba pri vytvareni nebo zapisu do vstreamu. */
    } en_MZCMT_TURBO_ERROR;


/** @brief Vychozi delka dlouheho GAPu - 22 000 kratkych pulzu. */
#define MZCMT_TURBO_LGAP_DEFAULT   22000
/** @brief Vychozi delka kratkeho GAPu - 11 000 kratkych pulzu. */
#define MZCMT_TURBO_SGAP_DEFAULT   11000

/** @brief Pocet long pulzu v dlouhem tapemarku. */
#define MZCMT_TURBO_LTM_LONG       40
/** @brief Pocet short pulzu v dlouhem tapemarku. */
#define MZCMT_TURBO_LTM_SHORT      40
/** @brief Pocet long pulzu v kratkem tapemarku. */
#define MZCMT_TURBO_STM_LONG       20
/** @brief Pocet short pulzu v kratkem tapemarku. */
#define MZCMT_TURBO_STM_SHORT      20

/** @brief Pocet short pulzu v separacni sekci pred kopii. */
#define MZCMT_TURBO_COPY_SEP       256

/** @brief Priznak: blok obsahuje kopii hlavicky (header nahran 2x). */
#define MZCMT_TURBO_FLAG_HEADER_COPY  0x01
/** @brief Priznak: blok obsahuje kopii tela (body nahrano 2x). */
#define MZCMT_TURBO_FLAG_BODY_COPY    0x02


    /**
     * @brief Konfigurace TURBO koderu.
     *
     * Urcuje vsechny parametry generovaneho signalu. Nulove hodnoty
     * u volitelnych poli (lgap_length, sgap_length, long_high_us100 atd.)
     * znamenaji pouziti vychozich hodnot odvozenych z pulsesetu a rychlosti.
     *
     * @par Invarianty:
     * - pulseset musi byt < MZCMT_TURBO_PULSESET_COUNT
     * - speed musi byt platna en_CMTSPEED (overeni pres cmtspeed_is_valid)
     *
     * @par Jednotky pulznich delek:
     * Pole *_us100 jsou v jednotkach mikrosekundy * 100 (tj. 10 ns rozliseni).
     * Hodnota 4700 odpovida 470.0 us. Nulova hodnota = pouzit default.
     */
    typedef struct st_MZCMT_TURBO_CONFIG {
        en_MZCMT_TURBO_PULSESET pulseset; /**< pulzni sada pro vychozi casovani */
        en_CMTSPEED speed;                /**< rychlostni pomer (divisor) */
        uint32_t lgap_length;             /**< delka LGAP v kratkych pulzech (0 = 22000) */
        uint32_t sgap_length;             /**< delka SGAP v kratkych pulzech (0 = 11000) */
        uint16_t long_high_us100;         /**< high cast long pulzu (us*100, 0 = default) */
        uint16_t long_low_us100;          /**< low cast long pulzu (us*100, 0 = default) */
        uint16_t short_high_us100;        /**< high cast short pulzu (us*100, 0 = default) */
        uint16_t short_low_us100;         /**< low cast short pulzu (us*100, 0 = default) */
        uint8_t flags;                    /**< priznaky (MZCMT_TURBO_FLAG_*) */
    } st_MZCMT_TURBO_CONFIG;


    /**
     * @brief Vytvori CMT vstream z MZF dat TURBO kodovanim.
     *
     * Generuje kompletni paskovy ramec ve formatu NORMAL FM s pulznim
     * casovanim dle konfigurace:
     *
     * 1. LGAP (leader ton - kratke pulzy)
     * 2. Dlouhy tapemark (40 long + 40 short)
     * 3. 2 long + hlavicka (128 B, 8 bitu + stop bit) + checksum + 2 long
     * 4. [pokud HEADER_COPY: 256 short + hlavicka + checksum + 2 long]
     * 5. SGAP (kratke pulzy)
     * 6. Kratky tapemark (20 long + 20 short)
     * 7. 2 long + telo (N B, 8 bitu + stop bit) + checksum + 2 long
     * 8. [pokud BODY_COPY: 256 short + telo + checksum + 2 long]
     *
     * Signal zacina v HIGH stavu (shodne s mztape). Checksum je 16bitovy
     * pocet jednickovych bitu, odesilany big-endian.
     *
     * @param header    Ukazatel na 128B MZF hlavicku v originalni endianite.
     * @param body      Ukazatel na datove telo (muze byt NULL pokud body_size == 0).
     * @param body_size Velikost datoveho tela v bajtech.
     * @param config    Konfigurace koderu (nesmi byt NULL).
     * @param rate      Vzorkovaci frekvence vystupniho vstreamu (Hz, musi byt > 0).
     * @return Ukazatel na novy vstream, nebo NULL pri chybe.
     *
     * @pre @p header != NULL
     * @pre @p body != NULL pokud @p body_size > 0
     * @pre @p config != NULL
     * @pre @p config->pulseset < MZCMT_TURBO_PULSESET_COUNT
     * @pre cmtspeed_is_valid(@p config->speed)
     * @pre @p rate > 0
     *
     * @post Navratova hodnota != NULL => volajici vlastni vstream a musi
     *       jej uvolnit volanim cmt_vstream_destroy().
     * @post Navratova hodnota == NULL => zadna pamet nebyla alokovana.
     */
    extern st_CMT_VSTREAM* mzcmt_turbo_create_vstream (
        const uint8_t *header,
        const uint8_t *body,
        uint32_t body_size,
        const st_MZCMT_TURBO_CONFIG *config,
        uint32_t rate
    );


    /**
     * @brief Vytvori jednotny CMT stream z MZF dat TURBO kodovanim.
     *
     * Obaluje mzcmt_turbo_create_vstream() do polymorfniho st_CMT_STREAM.
     * Pro typ CMT_STREAM_TYPE_BITSTREAM interno vytvori vstream a konvertuje.
     *
     * @param header    Ukazatel na 128B MZF hlavicku v originalni endianite.
     * @param body      Ukazatel na datove telo (muze byt NULL pokud body_size == 0).
     * @param body_size Velikost datoveho tela v bajtech.
     * @param config    Konfigurace koderu (nesmi byt NULL).
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
    extern st_CMT_STREAM* mzcmt_turbo_create_stream (
        const uint8_t *header,
        const uint8_t *body,
        uint32_t body_size,
        const st_MZCMT_TURBO_CONFIG *config,
        en_CMT_STREAM_TYPE type,
        uint32_t rate
    );


    /**
     * @brief Spocita NORMAL FM checksum datoveho bloku.
     *
     * ROM algoritmus: scita pocet jednickovych bitu ve vsech bajtech bloku.
     * Vysledek je 16bitova hodnota (uint16_t). Checksum se na pasku
     * odesilala big-endian (horni bajt prvni).
     *
     * @param data Ukazatel na datovy blok.
     * @param size Velikost datoveho bloku v bajtech.
     * @return Pocet jednickovych bitu v bloku. Vraci 0 pro NULL nebo size==0.
     */
    extern uint16_t mzcmt_turbo_compute_checksum ( const uint8_t *data, uint16_t size );


    /**
     * @brief Vytvori CMT vstream s kompletnim paskovym signalem vcetne loaderu.
     *
     * Generuje dvoudilny signal:
     * 1. TURBO loader header v NORMAL FM 1:1 (ROM ho nacte, fsize=0, Z80 kod
     *    v komentari modifikuje readpoint a vola ROM cteci rutinu)
     * 2. Uzivatelska data v TURBO formatu (loader je nacte modifikovanym ROMem)
     *
     * Loader je automaticky patchovan s parametry ciloveho MZF programu
     * (velikost, startovni adresa, exec adresa, readpoint delay).
     *
     * @param original  Originalni MZF hlavicka (zdrojovy program).
     * @param body      Ukazatel na datove telo k zakodovani.
     * @param body_size Velikost datoveho tela v bajtech.
     * @param config    Konfigurace TURBO koderu (pulseset, speed, GAP delky, flags).
     * @param rate      Vzorkovaci frekvence (Hz).
     * @return Novy vstream s kompletnim signalem, nebo NULL pri chybe.
     *
     * @pre @p original != NULL
     * @pre @p body != NULL pokud @p body_size > 0
     * @pre @p config != NULL
     * @pre @p rate > 0
     *
     * @post Navratova hodnota != NULL => volajici vlastni vstream.
     */
    extern st_CMT_VSTREAM* mzcmt_turbo_create_tape_vstream (
        const st_MZF_HEADER *original,
        const uint8_t *body,
        uint32_t body_size,
        const st_MZCMT_TURBO_CONFIG *config,
        uint32_t rate
    );


    /**
     * @brief Vytvori jednotny CMT stream s kompletnim paskovym signalem vcetne loaderu.
     *
     * Obaluje mzcmt_turbo_create_tape_vstream() do polymorfniho st_CMT_STREAM.
     *
     * @param original  Originalni MZF hlavicka (zdrojovy program).
     * @param body      Ukazatel na datove telo k zakodovani.
     * @param body_size Velikost datoveho tela v bajtech.
     * @param config    Konfigurace TURBO koderu.
     * @param type      Typ vystupniho streamu (bitstream/vstream).
     * @param rate      Vzorkovaci frekvence (Hz).
     * @return Novy stream, nebo NULL pri chybe.
     */
    extern st_CMT_STREAM* mzcmt_turbo_create_tape_stream (
        const st_MZF_HEADER *original,
        const uint8_t *body,
        uint32_t body_size,
        const st_MZCMT_TURBO_CONFIG *config,
        en_CMT_STREAM_TYPE type,
        uint32_t rate
    );


    /** @brief Uzivatelsky alokator - umoznuje nahradit vychozi malloc/calloc/free. */
    typedef struct st_MZCMT_TURBO_ALLOCATOR {
        void* (*alloc)(size_t size);   /**< alokace pameti (jako malloc) */
        void* (*alloc0)(size_t size);  /**< alokace s nulovanim (jako calloc) */
        void  (*free)(void *ptr);      /**< uvolneni pameti */
    } st_MZCMT_TURBO_ALLOCATOR;

    /**
     * @brief Nastavi vlastni alokator pro knihovnu mzcmt_turbo.
     * @param allocator Ukazatel na strukturu alokatoru, nebo NULL pro reset na vychozi.
     */
    extern void mzcmt_turbo_set_allocator ( const st_MZCMT_TURBO_ALLOCATOR *allocator );


    /** @brief Typ callback funkce pro hlaseni chyb. */
    typedef void (*mzcmt_turbo_error_cb)(const char *func, int line, const char *fmt, ...);

    /**
     * @brief Nastavi vlastni callback pro chybova hlaseni.
     * @param cb Callback funkce, nebo NULL pro reset na vychozi (fprintf stderr).
     */
    extern void mzcmt_turbo_set_error_callback ( mzcmt_turbo_error_cb cb );


    /** @brief Verze knihovny mzcmt_turbo. */
#define MZCMT_TURBO_VERSION "2.0.0"

    /**
     * @brief Vrátí řetězec s verzí knihovny mzcmt_turbo.
     * @return Statický řetězec s verzí (např. "1.0.0").
     */
    extern const char* mzcmt_turbo_version ( void );


#ifdef __cplusplus
}
#endif

#endif /* MZCMT_TURBO_H */
