/**
 * @file sharpmz_utf8.h
 * @brief Verejne API pro obousmernou konverzi mezi Sharp MZ znakovou sadou a UTF-8.
 *
 * Poskytuje plnohodnotnou konverzi kompletni 256-znakove sady Sharp MZ (EU i JP
 * varianta) na UTF-8/Unicode a zpet. Pro graficke znaky bez Unicode ekvivalentu
 * se pouziva U+FFFD (replacement character). Pri zpetne konverzi se nezname znaky
 * nahrazuji mezerou (0x20).
 *
 * @author Michal Hucik <hucik@ordoz.com>
 *
 * ----------------------------- License -------------------------------------
 *
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
 *
 * ---------------------------------------------------------------------------
 */

#ifndef SHARPMZ_UTF8_H
#define SHARPMZ_UTF8_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Volba znakove sady Sharp MZ.
 *
 * Sharp MZ pocitace pouzivaly dve varianty znakove sady - evropskou (EU)
 * a japonskou (JP). Hlavni rozdil je v rozsahu 0x90-0xC0, kde EU ma mala
 * pismena a-z a JP ma znaky katakana.
 */
typedef enum {
    SHARPMZ_CHARSET_EU = 0, /**< Evropska varianta znakove sady */
    SHARPMZ_CHARSET_JP = 1  /**< Japonska varianta znakove sady */
} sharpmz_charset_t;

/**
 * @brief Polozka zpetne konverzni tabulky (Unicode code point -> MZ kod).
 *
 * Pouziva se v serazenem poli pro binarni vyhledavani pri konverzi
 * z UTF-8 na Sharp MZ kod.
 *
 * @note Pole musi byt serazene vzestupne podle clenu codepoint.
 */
typedef struct {
    uint32_t codepoint; /**< Unicode code point */
    uint8_t mz_code;    /**< Odpovidajici Sharp MZ kod */
} sharpmz_reverse_entry_t;

/**
 * @brief Konvertuje jeden Sharp MZ znak na UTF-8 retezec.
 *
 * Vraci ukazatel na staticky null-terminated UTF-8 retezec odpovidajici
 * zadanemu Sharp MZ kodu. Pro graficke znaky bez Unicode ekvivalentu
 * vraci "\xEF\xBF\xBD" (U+FFFD).
 *
 * @param[in] mz_code  Sharp MZ kod znaku (0x00-0xFF).
 * @param[in] charset  Varianta znakove sady (EU nebo JP).
 * @return Ukazatel na staticky UTF-8 retezec. Nikdy nevraci NULL.
 *         Vraceny retezec je platny po celou dobu behu programu.
 *
 * @pre charset musi byt SHARPMZ_CHARSET_EU nebo SHARPMZ_CHARSET_JP.
 */
const char *sharpmz_to_utf8(uint8_t mz_code, sharpmz_charset_t charset);

/**
 * @brief Konvertuje jeden UTF-8 znak na Sharp MZ kod.
 *
 * Precte jeden UTF-8 znak ze vstupu a hleda odpovidajici Sharp MZ kod
 * v dane znakove sade. Pouziva binarni vyhledavani ve zpetne tabulce.
 *
 * @param[in] utf8     Ukazatel na UTF-8 retezec (musi byt validni UTF-8).
 * @param[in] charset  Varianta znakove sady (EU nebo JP).
 * @return Sharp MZ kod (0x00-0xFF) pri uspechu, -1 pokud znak neni
 *         v dane znakove sade nalezen.
 *
 * @pre utf8 nesmi byt NULL a musi ukazovat na validni UTF-8 sekvenci.
 * @pre charset musi byt SHARPMZ_CHARSET_EU nebo SHARPMZ_CHARSET_JP.
 */
int sharpmz_from_utf8(const char *utf8, sharpmz_charset_t charset);

/**
 * @brief Konvertuje retezec Sharp MZ znaku na UTF-8.
 *
 * Iteruje pres zdrojovy retezec Sharp MZ kodu a zapisuje odpovidajici
 * UTF-8 znaky do ciloveho bufferu. Vystup je vzdy null-terminated
 * (pokud dst_size > 0).
 *
 * @param[in]  src       Zdrojovy retezec Sharp MZ kodu.
 * @param[in]  src_len   Delka zdrojoveho retezce v bajtech.
 * @param[out] dst       Cilovy buffer pro UTF-8 vystup.
 * @param[in]  dst_size  Velikost ciloveho bufferu v bajtech (vcetne mista
 *                       pro null terminator).
 * @param[in]  charset   Varianta znakove sady (EU nebo JP).
 * @return Pocet zapsanych bajtu (bez null terminatoru) pri uspechu,
 *         -1 pokud dst nebo src je NULL, nebo pokud dst_size je 0.
 *
 * @pre src nesmi byt NULL.
 * @pre dst nesmi byt NULL.
 * @pre dst_size musi byt > 0.
 * @post dst je vzdy null-terminated (pokud dst_size > 0).
 * @note Pokud cilovy buffer nestaci, konverze se zastavi pred znakem,
 *       ktery by zpusobil preteceni. Vystup zustane null-terminated.
 */
int sharpmz_str_to_utf8(const uint8_t *src, size_t src_len,
                        char *dst, size_t dst_size,
                        sharpmz_charset_t charset);

/**
 * @brief Konvertuje UTF-8 retezec na retezec Sharp MZ kodu.
 *
 * Parsuje UTF-8 vstup a pro kazdy Unicode znak hleda odpovidajici
 * Sharp MZ kod. Nezname znaky se nahrazuji mezerou (0x20).
 * Vystup je vzdy null-terminated (pokud dst_size > 0).
 *
 * @param[in]  src       Zdrojovy UTF-8 retezec (null-terminated).
 * @param[out] dst       Cilovy buffer pro Sharp MZ kody.
 * @param[in]  dst_size  Velikost ciloveho bufferu v bajtech (vcetne mista
 *                       pro null terminator).
 * @param[in]  charset   Varianta znakove sady (EU nebo JP).
 * @return Pocet zapsanych bajtu (bez null terminatoru) pri uspechu,
 *         -1 pokud dst nebo src je NULL, nebo pokud dst_size je 0.
 *
 * @pre src nesmi byt NULL a musi byt validni null-terminated UTF-8.
 * @pre dst nesmi byt NULL.
 * @pre dst_size musi byt > 0.
 * @post dst je vzdy null-terminated (pokud dst_size > 0).
 * @note Nevalidni UTF-8 sekvence se preskakuji (posun o 1 bajt).
 */
int sharpmz_str_from_utf8(const char *src,
                          uint8_t *dst, size_t dst_size,
                          sharpmz_charset_t charset);

#ifdef __cplusplus
}
#endif

#endif /* SHARPMZ_UTF8_H */
