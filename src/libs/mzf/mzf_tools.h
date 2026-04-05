/**
 * @file   mzf_tools.h
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 2.0.0
 * @brief  Pomocné funkce pro práci s MZF hlavičkou.
 *
 * Konverze jmen souborů mezi Sharp MZ ASCII a standardním ASCII,
 * tovární funkce pro vytvoření hlavičky, debug výpis.
 *
 * @par Changelog:
 * - 2026-03-14: Proběhla kompletní revize a refaktorizace. Vytvořeny unit testy.
 *
 * @par Licence:
 * GNU General Public License v3 (GPLv3)
 *
 * Copyright (C) 2017-2026 Michal Hucik <hucik@ordoz.com>
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


#ifndef MZF_TOOLS_H
#define MZF_TOOLS_H

#include <stdio.h>
#include <stddef.h>
#include "mzf.h"

#ifdef __cplusplus
extern "C" {
#endif


    /**
     * @brief Režim kódování jména souboru z MZF hlavičky.
     *
     * Určuje, jakým způsobem se konvertuje jméno souboru ze Sharp MZ
     * znakové sady do cílového kódování.
     */
    typedef enum en_MZF_NAME_ENCODING {
        MZF_NAME_ASCII = 0,   /**< Sharp MZ -> ASCII (výchozí, jednobajtová konverze) */
        MZF_NAME_UTF8_EU,     /**< Sharp MZ -> UTF-8, evropská varianta znakové sady */
        MZF_NAME_UTF8_JP,     /**< Sharp MZ -> UTF-8, japonská varianta znakové sady */
    } en_MZF_NAME_ENCODING;


    /** @brief Maximální velikost bufferu pro UTF-8 jméno (17 znaků * max 4 bajty + 1). */
    #define MZF_FNAME_UTF8_BUF_SIZE  ( ( MZF_FNAME_FULL_LENGTH * 4 ) + 1 )


    /**
     * @brief Nastaví jméno souboru v hlavičce z ASCII řetězce.
     *
     * Konvertuje ASCII na Sharp MZ ASCII, ořeže na 16 znaků,
     * zbytek vyplní terminátorem 0x0d.
     *
     * @param mzfhdr         Ukazatel na hlavičku k úpravě
     * @param ascii_filename ASCII řetězec se jménem souboru
     */
    extern void mzf_tools_set_fname ( st_MZF_HEADER *mzfhdr, const char *ascii_filename );

    /**
     * @brief Vrátí délku jména souboru (počet znaků před prvním terminátorem 0x0d).
     *
     * Pokud terminátor chybí, vrátí MZF_FILE_NAME_LENGTH (16).
     *
     * @param mzfhdr Ukazatel na hlavičku
     * @return Délka jména v rozsahu 0..16
     */
    extern uint8_t mzf_tools_get_fname_length ( const st_MZF_HEADER *mzfhdr );

    /**
     * @brief Extrahuje jméno souboru z hlavičky do ASCII řetězce.
     *
     * Konvertuje Sharp MZ ASCII na ASCII, přeskakuje netisknutelné znaky (< 0x20).
     * Výstupní buffer musí mít minimálně MZF_FILE_NAME_LENGTH + 1 (17) bajtů.
     *
     * @param mzfhdr         Ukazatel na hlavičku
     * @param ascii_filename Výstupní buffer pro ASCII řetězec (nulou ukončený)
     */
    extern void mzf_tools_get_fname ( const st_MZF_HEADER *mzfhdr, char *ascii_filename );

    /**
     * @brief Extrahuje jméno souboru z hlavičky s volitelným kódováním.
     *
     * Podle zadaného kódování provede konverzi:
     * - MZF_NAME_ASCII: Sharp MZ -> ASCII (shodné s mzf_tools_get_fname)
     * - MZF_NAME_UTF8_EU: Sharp MZ -> UTF-8, evropská znaková sada
     * - MZF_NAME_UTF8_JP: Sharp MZ -> UTF-8, japonská znaková sada
     *
     * @param mzfhdr   Ukazatel na hlavičku. Nesmí být NULL.
     * @param filename Výstupní buffer pro nulou ukončený řetězec.
     * @param buf_size Velikost výstupního bufferu v bajtech.
     *                 Pro ASCII stačí MZF_FILE_NAME_LENGTH + 1 (17).
     *                 Pro UTF-8 doporučeno MZF_FNAME_UTF8_BUF_SIZE (69).
     * @param encoding Požadované kódování výstupu.
     */
    extern void mzf_tools_get_fname_ex ( const st_MZF_HEADER *mzfhdr, char *filename,
                                          size_t buf_size, en_MZF_NAME_ENCODING encoding );

    /**
     * @brief Vytvoří novou MZF hlavičku alokovanou na heapu.
     *
     * Volající musí uvolnit přes free(). Pokud cmnt == NULL, komentář
     * se vynuluje. Jméno se vyplní terminátory a pak překopíruje zadané znaky.
     *
     * @param ftype        Typ souboru (viz MZF_FTYPE_*)
     * @param fsize        Velikost datové části
     * @param fstrt        Startovací adresa v paměti Z80
     * @param fexec        Adresa spuštění v paměti Z80
     * @param fname        Surové bajty jména (Sharp MZ ASCII)
     * @param fname_length Délka fname (max. 16, delší se ořízne)
     * @param cmnt         Komentář (104 bajtů), nebo NULL pro vynulování
     * @return Ukazatel na st_MZF_HEADER, nebo NULL při selhání alokace
     */
    extern st_MZF_HEADER* mzf_tools_create_mzfhdr ( uint8_t ftype, uint16_t fsize, uint16_t fstrt, uint16_t fexec, const uint8_t *fname, unsigned fname_length, const uint8_t *cmnt );

    /**
     * @brief Vypíše obsah MZF hlavičky na zadaný FILE* (pro debug účely).
     *
     * Formát: typ, jméno (s délkou), velikost (hex+dec), adresy (hex),
     * komentář (prvních 16 bajtů hex dump). NULL-safe — oba parametry
     * mohou být NULL (no-op).
     *
     * @param mzfhdr Ukazatel na hlavičku (může být NULL)
     * @param fp     Výstupní FILE* stream (může být NULL)
     */
    extern void mzf_tools_dump_header ( const st_MZF_HEADER *mzfhdr, FILE *fp );

#ifdef __cplusplus
}
#endif

#endif /* MZF_TOOLS_H */
