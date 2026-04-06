/**
 * @file   cmtspeed.h
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 2.0.0
 * @brief  Definice rychlostních poměrů CMT pásky a inline funkce pro výpočet baudových rychlostí.
 *
 * Hlavičkový soubor knihovny cmtspeed poskytuje enum rychlostních režimů,
 * deklarace externích datových polí (divisory, textové poměry) a sadu
 * inline funkcí pro validaci, výpočet a formátování rychlostních parametrů
 * kazetové pásky počítačů Sharp MZ.
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


#ifndef CMTSPEED_H
#define CMTSPEED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <math.h>


    /** @brief Výčet rychlostních poměrů CMT pásky. */
    typedef enum en_CMTSPEED {
        CMTSPEED_NONE = 0,  /**< Žádná rychlost (neplatná hodnota) */
        CMTSPEED_1_1,       /**< Poměr 1:1 — standardní rychlost (1200 Bd) */
        CMTSPEED_2_1,       /**< Poměr 2:1 — dvojnásobná rychlost */
        CMTSPEED_2_1_CPM,   /**< Poměr 2:1 — CP/M varianta */
        CMTSPEED_3_1,       /**< Poměr 3:1 — trojnásobná rychlost */
        CMTSPEED_3_2,       /**< Poměr 3:2 */
        CMTSPEED_7_3,       /**< Poměr 7:3 — Intercopy 10.2 */
        CMTSPEED_8_3,       /**< Poměr 8:3 — CP/M cmt.com */
        CMTSPEED_9_7,       /**< Poměr 9:7 */
        CMTSPEED_25_14,     /**< Poměr 25:14 */
        CMTSPEED_COUNT      /**< Počet platných hodnot (sentinel — nepoužívat jako rychlost) */
    } en_CMTSPEED;

    /** @brief Pole násobitelů rychlosti indexované hodnotami en_CMTSPEED. */
    extern const double g_cmtspeed_divisor[];

    /** @brief Pole textových popisů poměrů rychlosti indexované hodnotami en_CMTSPEED. */
    extern const char *g_cmtspeed_ratio[];


    /**
     * @brief Ověří, zda je zadaná rychlost platná.
     *
     * Platná rychlost je v rozsahu CMTSPEED_1_1 .. CMTSPEED_25_14 (včetně).
     * CMTSPEED_NONE a CMTSPEED_COUNT nejsou platné.
     *
     * @param cmtspeed Rychlostní poměr k ověření.
     * @return 1 pokud je rychlost platná, 0 jinak.
     */
    static inline int cmtspeed_is_valid ( en_CMTSPEED cmtspeed ) {
        return ( cmtspeed > CMTSPEED_NONE && cmtspeed < CMTSPEED_COUNT );
    }


    /**
     * @brief Vrátí násobitel rychlosti pro zadaný rychlostní poměr.
     *
     * Např. pro CMTSPEED_2_1 vrací 2.0, pro CMTSPEED_7_3 vrací 2.333...
     *
     * @param cmtspeed Rychlostní poměr.
     * @return Násobitel jako double. Pro neplatné hodnoty vrací 0.0.
     */
    static inline double cmtspeed_get_divisor ( en_CMTSPEED cmtspeed ) {
        if ( cmtspeed < 0 || cmtspeed >= CMTSPEED_COUNT ) return 0;
        return g_cmtspeed_divisor[cmtspeed];
    }


    /**
     * @brief Vypočítá baudovou rychlost pro zadaný poměr a základní rychlost.
     *
     * Výpočet: round(divisor * base_bdspeed). Typická base: 1200 Bd (MZ-800 standard).
     *
     * @param cmtspeed Rychlostní poměr.
     * @param base_bdspeed Základní baudová rychlost (typicky 1200).
     * @return Vypočtená baudová rychlost. Pro neplatné hodnoty vrací 0.
     */
    static inline uint16_t cmtspeed_get_bdspeed ( en_CMTSPEED cmtspeed, uint16_t base_bdspeed ) {
        if ( cmtspeed < 0 || cmtspeed >= CMTSPEED_COUNT ) return 0;
        return (uint16_t) round ( g_cmtspeed_divisor[cmtspeed] * base_bdspeed );
    }


    /**
     * @brief Vrátí textový řetězec s poměrem rychlosti.
     *
     * Např. "7:3", "2:1 (cp/m)". Pro neplatné hodnoty vrací "?:?".
     *
     * @param cmtspeed Rychlostní poměr.
     * @return Ukazatel na statický řetězec s poměrem.
     */
    static inline const char* cmtspeed_get_ratiotxt ( en_CMTSPEED cmtspeed ) {
        if ( cmtspeed < 0 || cmtspeed >= CMTSPEED_COUNT ) return "?:?";
        return g_cmtspeed_ratio[cmtspeed];
    }


    /**
     * @brief Formátuje řetězec s baudovou rychlostí.
     *
     * Výstup: "2400 Bd" nebo "2400 Bd (cp/m)" pro CP/M variantu.
     * Pro neplatné hodnoty "? Bd".
     *
     * @param dsttxt Cílový buffer pro výstupní řetězec.
     * @param size Velikost cílového bufferu v bajtech.
     * @param cmtspeed Rychlostní poměr.
     * @param base_bdspeed Základní baudová rychlost (typicky 1200).
     */
    static inline void cmtspeed_get_speedtxt ( char *dsttxt, int size, en_CMTSPEED cmtspeed, uint16_t base_bdspeed ) {
        if ( cmtspeed < 0 || cmtspeed >= CMTSPEED_COUNT ) { snprintf ( dsttxt, size, "? Bd" ); return; }
        if ( cmtspeed == CMTSPEED_2_1_CPM ) {
            snprintf ( dsttxt, size, "%d Bd (cp/m)", cmtspeed_get_bdspeed ( cmtspeed, base_bdspeed ) );
        } else {
            snprintf ( dsttxt, size, "%d Bd", cmtspeed_get_bdspeed ( cmtspeed, base_bdspeed ) );
        };
    }


    /**
     * @brief Formátuje kombinovaný řetězec "poměr - rychlost Bd".
     *
     * Výstup: např. "7:3 - 2800 Bd". Pro neplatné hodnoty "?:? - ? Bd".
     *
     * @param dsttxt Cílový buffer pro výstupní řetězec.
     * @param size Velikost cílového bufferu v bajtech.
     * @param cmtspeed Rychlostní poměr.
     * @param base_bdspeed Základní baudová rychlost (typicky 1200).
     */
    static inline void cmtspeed_get_ratiospeedtxt ( char *dsttxt, int size, en_CMTSPEED cmtspeed, uint16_t base_bdspeed ) {
        if ( cmtspeed < 0 || cmtspeed >= CMTSPEED_COUNT ) { snprintf ( dsttxt, size, "?:? - ? Bd" ); return; }
        snprintf ( dsttxt, size, "%s - %d Bd", cmtspeed_get_ratiotxt ( cmtspeed ), cmtspeed_get_bdspeed ( cmtspeed, base_bdspeed ) );
    }

    /** @brief Verze knihovny cmtspeed. */
#define CMTSPEED_VERSION "2.0.0"

    /**
     * @brief Vrátí řetězec s verzí knihovny cmtspeed.
     * @return Statický řetězec s verzí (např. "2.0.0").
     */
    extern const char* cmtspeed_version ( void );

#ifdef __cplusplus
}
#endif

#endif /* CMTSPEED_H */

