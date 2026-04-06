/* 
 * File:   sharpmz_ascii.h
 * Author: Michal Hucik <hucik@ordoz.com>
 *
 * Created on 11. srpna 2015, 12:26
 * 
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

#ifndef SHARPMZ_ASCII_H
#define	SHARPMZ_ASCII_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
    
    extern uint8_t sharpmz_cnv_from ( uint8_t c );
    extern uint8_t sharpmz_cnv_to ( uint8_t c );

/** @brief Verze knihovny sharpmz_ascii. */
#define SHARPMZ_ASCII_VERSION "1.0.0"

/**
 * @brief Vrátí řetězec s verzí knihovny sharpmz_ascii.
 * @return Statický řetězec s verzí (např. "1.0.0").
 */
extern const char* sharpmz_ascii_version ( void );

#ifdef	__cplusplus
}
#endif

#endif	/* SHARPMZ_ASCII_H */

