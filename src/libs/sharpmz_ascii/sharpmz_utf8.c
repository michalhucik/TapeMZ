/**
 * @file sharpmz_utf8.c
 * @brief Implementace obousmerne konverze mezi Sharp MZ znakovou sadou a UTF-8.
 *
 * Obsahuje kompletni konverzni tabulky pro EU i JP variantu znakove sady
 * Sharp MZ (256 znaku) a funkce pro konverzi jednotlivych znaku i celych
 * retezcu. Tabulky byly sestaveny na zaklade referenci z docs/.
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

#include "sharpmz_utf8.h"
#include <string.h>

/**
 * @brief UTF-8 reprezentace znaku U+FFFD (replacement character).
 *
 * Pouziva se pro graficke znaky Sharp MZ, ktere nemaji ekvivalent v Unicode.
 */
#define SHARPMZ_REPLACEMENT "\xEF\xBF\xBD"

/* ======================================================================== */
/*  Dopredne konverzni tabulky (MZ kod -> UTF-8 retezec)                    */
/* ======================================================================== */

/**
 * @brief Konverzni tabulka Sharp MZ EU -> UTF-8.
 *
 * Mapuje vsech 256 kodu Sharp MZ evropske varianty na UTF-8 retezce.
 * Index pole odpovida Sharp MZ kodu (0x00-0xFF).
 *
 * Kategorie znaku:
 * - 0x00: DEL (mapovano na U+007F)
 * - 0x01: SP (mezera, mapovano na U+0020)
 * - 0x02-0x0F: graficke symboly
 * - 0x10-0x1F: graficke symboly (sipky, cary, apod.)
 * - 0x20-0x5D: ASCII-kompatibilni znaky (mezera, cislice, velka pismena, atd.)
 * - 0x5E-0x7F: graficke symboly
 * - 0x80: } (prava slozena zavorka)
 * - 0x81-0x8F: graficke/specialni symboly
 * - 0x90-0xBF: mala pismena a-z, podtrzitko, specialni ASCII znaky
 * - 0xC0: | (svicla cara)
 * - 0xC1-0xFF: evropske znaky, graficke symboly
 */
static const char *sharpmz_utf8_table_eu[256] = {
    /* 0x00-0x0F */
    "\x7F",                     /* 0x00: DEL */
    " ",                        /* 0x01: SP (mezera zobrazena jako grafika) */
    SHARPMZ_REPLACEMENT,        /* 0x02: graficky symbol (obdelnik s diagonalou) */
    SHARPMZ_REPLACEMENT,        /* 0x03: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x04: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x05: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x06: graficky symbol (hvezdicka/snowflake) */
    SHARPMZ_REPLACEMENT,        /* 0x07: graficky symbol (ctverecek) */
    SHARPMZ_REPLACEMENT,        /* 0x08: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x09: graficky symbol (kosoctverec) */
    SHARPMZ_REPLACEMENT,        /* 0x0A: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x0B: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x0C: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x0D: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x0E: graficky symbol (I s crou) */
    SHARPMZ_REPLACEMENT,        /* 0x0F: graficky symbol */

    /* 0x10-0x1F */
    SHARPMZ_REPLACEMENT,        /* 0x10: graficky symbol */
    "!",                        /* 0x11: vykricnik */
    "\"",                       /* 0x12: uvozovky */
    "#",                        /* 0x13: krizek (hash) */
    "$",                        /* 0x14: dolar */
    "%",                        /* 0x15: procento */
    "&",                        /* 0x16: ampersand */
    "'",                        /* 0x17: apostrof */
    "(",                        /* 0x18: leva zavorka */
    ")",                        /* 0x19: prava zavorka */
    SHARPMZ_REPLACEMENT,        /* 0x1A: graficky symbol */
    "+",                        /* 0x1B: plus */
    ",",                        /* 0x1C: carka */
    "-",                        /* 0x1D: pomlcka/minus */
    ".",                        /* 0x1E: tecka */
    "/",                        /* 0x1F: lomitko */

    /* 0x20-0x2F: cislice a dalsi ASCII */
    "0",                        /* 0x20: nula */
    "1",                        /* 0x21: jednicka */
    "2",                        /* 0x22: dvojka */
    "3",                        /* 0x23: trojka */
    "4",                        /* 0x24: ctyrka */
    "5",                        /* 0x25: petka */
    "6",                        /* 0x26: sestka */
    "7",                        /* 0x27: sedmicka */
    "8",                        /* 0x28: osmicka */
    "9",                        /* 0x29: devitka */
    ":",                        /* 0x2A: dvojtecka */
    ";",                        /* 0x2B: strednik */
    "<",                        /* 0x2C: mensi nez */
    "=",                        /* 0x2D: rovnitko */
    ">",                        /* 0x2E: vetsi nez */
    "?",                        /* 0x2F: otaznik */

    /* 0x30-0x3F: @ a velka pismena A-O */
    "@",                        /* 0x30: zavinac */
    "A",                        /* 0x31: velke A */
    "B",                        /* 0x32: velke B */
    "C",                        /* 0x33: velke C */
    "D",                        /* 0x34: velke D */
    "E",                        /* 0x35: velke E */
    "F",                        /* 0x36: velke F */
    "G",                        /* 0x37: velke G */
    "H",                        /* 0x38: velke H */
    "I",                        /* 0x39: velke I */
    "J",                        /* 0x3A: velke J */
    "K",                        /* 0x3B: velke K */
    "L",                        /* 0x3C: velke L */
    "M",                        /* 0x3D: velke M */
    "N",                        /* 0x3E: velke N */
    "O",                        /* 0x3F: velke O */

    /* 0x40-0x4F: velka pismena P-Z a specialni znaky */
    "P",                        /* 0x40: velke P */
    "Q",                        /* 0x41: velke Q */
    "R",                        /* 0x42: velke R */
    "S",                        /* 0x43: velke S */
    "T",                        /* 0x44: velke T */
    "U",                        /* 0x45: velke U */
    "V",                        /* 0x46: velke V */
    "W",                        /* 0x47: velke W */
    "X",                        /* 0x48: velke X */
    "Y",                        /* 0x49: velke Y */
    "Z",                        /* 0x4A: velke Z */
    "[",                        /* 0x4B: leva hranata zavorka */
    "\\",                       /* 0x4C: zpetne lomitko */
    "]",                        /* 0x4D: prava hranata zavorka */
    "^",                        /* 0x4E: stricka */
    "-",                        /* 0x4F: pomlcka (MZ specificky tvar) */

    /* 0x50-0x5F */
    SHARPMZ_REPLACEMENT,        /* 0x50: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x51: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x52: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x53: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x54: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x55: graficky symbol (sipka nahoru) */
    SHARPMZ_REPLACEMENT,        /* 0x56: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x57: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x58: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x59: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x5A: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x5B: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x5C: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x5D: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x5E: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x5F: graficky symbol */

    /* 0x60-0x6F */
    SHARPMZ_REPLACEMENT,        /* 0x60: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x61: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x62: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x63: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x64: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x65: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x66: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x67: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x68: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x69: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x6A: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x6B: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x6C: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x6D: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x6E: graficky symbol (blok) */
    SHARPMZ_REPLACEMENT,        /* 0x6F: graficky symbol */

    /* 0x70-0x7F */
    SHARPMZ_REPLACEMENT,        /* 0x70: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x71: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x72: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x73: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x74: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x75: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x76: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x77: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x78: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x79: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x7A: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x7B: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x7C: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x7D: graficky symbol (plny blok) */
    SHARPMZ_REPLACEMENT,        /* 0x7E: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x7F: graficky symbol */

    /* 0x80-0x8F */
    "}",                        /* 0x80: prava slozena zavorka */
    SHARPMZ_REPLACEMENT,        /* 0x81: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x82: graficky symbol (INS) */
    SHARPMZ_REPLACEMENT,        /* 0x83: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x84: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x85: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x86: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x87: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x88: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x89: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x8A: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x8B: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x8C: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x8D: plny blok */
    SHARPMZ_REPLACEMENT,        /* 0x8E: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x8F: graficky symbol */

    /* 0x90-0x9F: mala pismena a dalsi znaky */
    "_",                        /* 0x90: podtrzitko */
    " ",                        /* 0x91: mezera */
    "e",                        /* 0x92: male e */
    " ",                        /* 0x93: mezera */
    "~",                        /* 0x94: vlnovka (tilde) */
    " ",                        /* 0x95: mezera */
    "t",                        /* 0x96: male t */
    "g",                        /* 0x97: male g */
    "h",                        /* 0x98: male h */
    " ",                        /* 0x99: mezera */
    "b",                        /* 0x9A: male b */
    "x",                        /* 0x9B: male x */
    "d",                        /* 0x9C: male d */
    "r",                        /* 0x9D: male r */
    "p",                        /* 0x9E: male p */
    "c",                        /* 0x9F: male c */

    /* 0xA0-0xAF */
    "q",                        /* 0xA0: male q */
    "a",                        /* 0xA1: male a */
    "z",                        /* 0xA2: male z */
    "w",                        /* 0xA3: male w */
    "s",                        /* 0xA4: male s */
    "u",                        /* 0xA5: male u */
    "i",                        /* 0xA6: male i */
    " ",                        /* 0xA7: mezera */
    " ",                        /* 0xA8: mezera */
    "k",                        /* 0xA9: male k */
    "f",                        /* 0xAA: male f */
    "v",                        /* 0xAB: male v */
    " ",                        /* 0xAC: mezera */
    " ",                        /* 0xAD: mezera */
    " ",                        /* 0xAE: mezera */
    "j",                        /* 0xAF: male j */

    /* 0xB0-0xBF */
    "n",                        /* 0xB0: male n */
    " ",                        /* 0xB1: mezera */
    " ",                        /* 0xB2: mezera */
    "m",                        /* 0xB3: male m */
    " ",                        /* 0xB4: mezera */
    " ",                        /* 0xB5: mezera */
    " ",                        /* 0xB6: mezera */
    "o",                        /* 0xB7: male o */
    "l",                        /* 0xB8: male l */
    " ",                        /* 0xB9: mezera */
    " ",                        /* 0xBA: mezera */
    " ",                        /* 0xBB: mezera */
    " ",                        /* 0xBC: mezera */
    "y",                        /* 0xBD: male y */
    "{",                        /* 0xBE: leva slozena zavorka */
    " ",                        /* 0xBF: mezera */

    /* 0xC0-0xCF */
    "|",                        /* 0xC0: svicla cara (pipe) */
    SHARPMZ_REPLACEMENT,        /* 0xC1: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xC2: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xC3: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xC4: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xC5: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xC6: graficky symbol */
    "\xC3\xA4",                 /* 0xC7: a s diaeresis */
    "\xC3\xBC",                 /* 0xC8: u s diaeresis */
    "\xC3\xB6",                 /* 0xC9: o s diaeresis */
    SHARPMZ_REPLACEMENT,        /* 0xCA: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xCB: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xCC: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xCD: graficky symbol */
    "\xC2\xA3",                 /* 0xCE: symbol libry (pound sign) */
    SHARPMZ_REPLACEMENT,        /* 0xCF: graficky symbol */

    /* 0xD0-0xDF */
    SHARPMZ_REPLACEMENT,        /* 0xD0: CR symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD1: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD2: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD3: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD4: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD5: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD6: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD7: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD8: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD9: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xDA: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xDB: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xDC: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xDD: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xDE: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xDF: graficky symbol */

    /* 0xE0-0xEF */
    SHARPMZ_REPLACEMENT,        /* 0xE0: graficky symbol */
    "\xC3\x9F",                 /* 0xE1: ostri s (Eszett) */
    SHARPMZ_REPLACEMENT,        /* 0xE2: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xE3: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xE4: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xE5: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xE6: graficky symbol */
    "\xC3\x84",                 /* 0xE7: A s diaeresis (velke) */
    "\xC3\x9C",                 /* 0xE8: U s diaeresis (velke) */
    "\xC3\x96",                 /* 0xE9: O s diaeresis (velke) */
    SHARPMZ_REPLACEMENT,        /* 0xEA: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xEB: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xEC: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xED: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xEE: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xEF: graficky symbol */

    /* 0xF0-0xFF */
    SHARPMZ_REPLACEMENT,        /* 0xF0: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF1: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF2: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF3: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF4: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF5: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF6: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF7: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF8: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF9: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xFA: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xFB: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xFC: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xFD: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xFE: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xFF: graficky symbol */
};

/**
 * @brief Konverzni tabulka Sharp MZ JP -> UTF-8.
 *
 * Mapuje vsech 256 kodu Sharp MZ japonske varianty na UTF-8 retezce.
 * Index pole odpovida Sharp MZ kodu (0x00-0xFF).
 *
 * Hlavni rozdily oproti EU variante:
 * - 0x90-0xBF: katakana misto malych pismen
 * - 0xC0-0xFF: japonske specificke znaky a grafika
 */
static const char *sharpmz_utf8_table_jp[256] = {
    /* 0x00-0x0F */
    "\x7F",                     /* 0x00: DEL */
    " ",                        /* 0x01: SP */
    SHARPMZ_REPLACEMENT,        /* 0x02: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x03: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x04: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x05: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x06: graficky symbol (hvezdicka) */
    SHARPMZ_REPLACEMENT,        /* 0x07: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x08: graficky symbol */
    "-",                        /* 0x09: pomlcka */
    SHARPMZ_REPLACEMENT,        /* 0x0A: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x0B: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x0C: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x0D: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x0E: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x0F: graficky symbol */

    /* 0x10-0x1F: shodne s EU pro interpunkci */
    SHARPMZ_REPLACEMENT,        /* 0x10: graficky symbol */
    "!",                        /* 0x11: vykricnik */
    "\"",                       /* 0x12: uvozovky */
    "#",                        /* 0x13: krizek */
    "$",                        /* 0x14: dolar */
    "%",                        /* 0x15: procento */
    "&",                        /* 0x16: ampersand */
    "'",                        /* 0x17: apostrof */
    "(",                        /* 0x18: leva zavorka */
    ")",                        /* 0x19: prava zavorka */
    SHARPMZ_REPLACEMENT,        /* 0x1A: graficky symbol */
    "+",                        /* 0x1B: plus */
    ",",                        /* 0x1C: carka */
    "-",                        /* 0x1D: pomlcka/minus */
    ".",                        /* 0x1E: tecka */
    "/",                        /* 0x1F: lomitko */

    /* 0x20-0x2F: cislice - shodne s EU */
    "0",                        /* 0x20: nula */
    "1",                        /* 0x21: jednicka */
    "2",                        /* 0x22: dvojka */
    "3",                        /* 0x23: trojka */
    "4",                        /* 0x24: ctyrka */
    "5",                        /* 0x25: petka */
    "6",                        /* 0x26: sestka */
    "7",                        /* 0x27: sedmicka */
    "8",                        /* 0x28: osmicka */
    "9",                        /* 0x29: devitka */
    ":",                        /* 0x2A: dvojtecka */
    ";",                        /* 0x2B: strednik */
    "<",                        /* 0x2C: mensi nez */
    "=",                        /* 0x2D: rovnitko */
    ">",                        /* 0x2E: vetsi nez */
    "?",                        /* 0x2F: otaznik */

    /* 0x30-0x3F: velka pismena - shodne s EU */
    "@",                        /* 0x30: zavinac */
    "A",                        /* 0x31: velke A */
    "B",                        /* 0x32: velke B */
    "C",                        /* 0x33: velke C */
    "D",                        /* 0x34: velke D */
    "E",                        /* 0x35: velke E */
    "F",                        /* 0x36: velke F */
    "G",                        /* 0x37: velke G */
    "H",                        /* 0x38: velke H */
    "I",                        /* 0x39: velke I */
    "J",                        /* 0x3A: velke J */
    "K",                        /* 0x3B: velke K */
    "L",                        /* 0x3C: velke L */
    "M",                        /* 0x3D: velke M */
    "N",                        /* 0x3E: velke N */
    "O",                        /* 0x3F: velke O */

    /* 0x40-0x4F: velka pismena pokracovani */
    "P",                        /* 0x40: velke P */
    "Q",                        /* 0x41: velke Q */
    "R",                        /* 0x42: velke R */
    "S",                        /* 0x43: velke S */
    "T",                        /* 0x44: velke T */
    "U",                        /* 0x45: velke U */
    "V",                        /* 0x46: velke V */
    "W",                        /* 0x47: velke W */
    "X",                        /* 0x48: velke X */
    "Y",                        /* 0x49: velke Y */
    "Z",                        /* 0x4A: velke Z */
    "[",                        /* 0x4B: leva hranata zavorka */
    "\xC2\xA5",                 /* 0x4C: symbol yenu (misto backslash) */
    "]",                        /* 0x4D: prava hranata zavorka */
    "^",                        /* 0x4E: stricka */
    "-",                        /* 0x4F: pomlcka */

    /* 0x50-0x5F: graficke symboly */
    SHARPMZ_REPLACEMENT,        /* 0x50: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x51: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x52: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x53: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x54: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x55: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x56: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x57: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x58: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x59: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x5A: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x5B: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x5C: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x5D: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x5E: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x5F: graficky symbol */

    /* 0x60-0x6F: graficke symboly */
    SHARPMZ_REPLACEMENT,        /* 0x60: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x61: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x62: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x63: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x64: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x65: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x66: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x67: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x68: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x69: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x6A: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x6B: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x6C: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x6D: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x6E: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x6F: graficky symbol */

    /* 0x70-0x7F: graficke symboly */
    SHARPMZ_REPLACEMENT,        /* 0x70: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x71: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x72: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x73: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x74: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x75: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x76: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x77: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x78: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x79: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x7A: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x7B: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x7C: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x7D: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x7E: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x7F: graficky symbol */

    /* 0x80-0x8F */
    "}",                        /* 0x80: prava slozena zavorka */
    SHARPMZ_REPLACEMENT,        /* 0x81: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x82: INS */
    SHARPMZ_REPLACEMENT,        /* 0x83: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x84: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x85: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x86: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x87: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x88: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x89: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x8A: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x8B: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x8C: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x8D: plny blok */
    SHARPMZ_REPLACEMENT,        /* 0x8E: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0x8F: graficky symbol */

    /* 0x90-0x9F: katakana */
    "\xE3\x83\xB2",            /* 0x90: katakana WO */
    "\xE3\x82\xA2",            /* 0x91: katakana A */
    "\xE3\x82\xA4",            /* 0x92: katakana I */
    "\xE3\x82\xA6",            /* 0x93: katakana U */
    "\xE3\x82\xA8",            /* 0x94: katakana E */
    "\xE3\x82\xAA",            /* 0x95: katakana O */
    "\xE3\x82\xAB",            /* 0x96: katakana KA */
    "\xE3\x82\xAD",            /* 0x97: katakana KI */
    "\xE3\x82\xAF",            /* 0x98: katakana KU */
    "\xE3\x82\xB1",            /* 0x99: katakana KE */
    "\xE3\x82\xB3",            /* 0x9A: katakana KO */
    "\xE3\x82\xB5",            /* 0x9B: katakana SA */
    "\xE3\x82\xB7",            /* 0x9C: katakana SI */
    "\xE3\x82\xB9",            /* 0x9D: katakana SU */
    "\xE3\x82\xBB",            /* 0x9E: katakana SE */
    "\xE3\x82\xBD",            /* 0x9F: katakana SO */

    /* 0xA0-0xAF: katakana pokracovani */
    "\xE3\x82\xBF",            /* 0xA0: katakana TA */
    "\xE3\x83\x81",            /* 0xA1: katakana TI */
    "\xE3\x83\x84",            /* 0xA2: katakana TU */
    "\xE3\x83\x86",            /* 0xA3: katakana TE */
    "\xE3\x83\x88",            /* 0xA4: katakana TO */
    "\xE3\x83\x8A",            /* 0xA5: katakana NA */
    "\xE3\x83\x8B",            /* 0xA6: katakana NI */
    "\xE3\x83\x8C",            /* 0xA7: katakana NU */
    "\xE3\x83\x8D",            /* 0xA8: katakana NE */
    "\xE3\x83\x8E",            /* 0xA9: katakana NO */
    "\xE3\x83\x8F",            /* 0xAA: katakana HA */
    "\xE3\x83\x92",            /* 0xAB: katakana HI */
    "\xE3\x83\x95",            /* 0xAC: katakana HU */
    "\xE3\x83\x98",            /* 0xAD: katakana HE */
    "\xE3\x83\x9B",            /* 0xAE: katakana HO */
    "\xE3\x83\x9E",            /* 0xAF: katakana MA */

    /* 0xB0-0xBF: katakana pokracovani */
    "\xE3\x83\x9F",            /* 0xB0: katakana MI */
    "\xE3\x83\xA0",            /* 0xB1: katakana MU */
    "\xE3\x83\xA1",            /* 0xB2: katakana ME */
    "\xE3\x83\xA2",            /* 0xB3: katakana MO */
    "\xE3\x83\xA4",            /* 0xB4: katakana YA */
    "\xE3\x83\xA6",            /* 0xB5: katakana YU */
    "\xE3\x83\xA8",            /* 0xB6: katakana YO */
    "\xE3\x83\xA9",            /* 0xB7: katakana RA */
    "\xE3\x83\xAA",            /* 0xB8: katakana RI */
    "\xE3\x83\xAB",            /* 0xB9: katakana RU */
    "\xE3\x83\xAC",            /* 0xBA: katakana RE */
    "\xE3\x83\xAD",            /* 0xBB: katakana RO */
    "\xE3\x83\xAF",            /* 0xBC: katakana WA */
    "\xE3\x83\xB3",            /* 0xBD: katakana N */
    "\xE3\x82\x9B",            /* 0xBE: dakuten (voiced mark) */
    "\xE3\x82\x9C",            /* 0xBF: handakuten (semi-voiced mark) */

    /* 0xC0-0xCF: japonske specificke znaky */
    SHARPMZ_REPLACEMENT,        /* 0xC0: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xC1: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xC2: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xC3: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xC4: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xC5: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xC6: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xC7: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xC8: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xC9: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xCA: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xCB: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xCC: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xCD: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xCE: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xCF: graficky symbol */

    /* 0xD0-0xDF */
    SHARPMZ_REPLACEMENT,        /* 0xD0: CR symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD1: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD2: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD3: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD4: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD5: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD6: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD7: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD8: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xD9: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xDA: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xDB: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xDC: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xDD: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xDE: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xDF: graficky symbol */

    /* 0xE0-0xEF */
    SHARPMZ_REPLACEMENT,        /* 0xE0: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xE1: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xE2: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xE3: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xE4: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xE5: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xE6: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xE7: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xE8: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xE9: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xEA: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xEB: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xEC: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xED: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xEE: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xEF: graficky symbol */

    /* 0xF0-0xFF */
    SHARPMZ_REPLACEMENT,        /* 0xF0: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF1: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF2: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF3: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF4: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF5: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF6: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF7: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF8: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xF9: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xFA: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xFB: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xFC: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xFD: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xFE: graficky symbol */
    SHARPMZ_REPLACEMENT,        /* 0xFF: graficky symbol */
};

/* ======================================================================== */
/*  Zpetne konverzni tabulky (Unicode code point -> MZ kod)                 */
/* ======================================================================== */

/**
 * @brief Zpetna konverzni tabulka pro EU variantu.
 *
 * Serazena vzestupne podle codepoint pro binarni vyhledavani.
 * Obsahuje vsechny Unicode code pointy, ktere maji mapovani na Sharp MZ EU.
 * Duplicitni mapovani (napr. mezera 0x01/0x20 se stejnym codepoint)
 * pouziva prvni vyskyt.
 */
static const sharpmz_reverse_entry_t sharpmz_reverse_eu[] = {
    { 0x0020, 0x01 },  /* mezera -> SP */
    { 0x0021, 0x11 },  /* ! */
    { 0x0022, 0x12 },  /* " */
    { 0x0023, 0x13 },  /* # */
    { 0x0024, 0x14 },  /* $ */
    { 0x0025, 0x15 },  /* % */
    { 0x0026, 0x16 },  /* & */
    { 0x0027, 0x17 },  /* ' */
    { 0x0028, 0x18 },  /* ( */
    { 0x0029, 0x19 },  /* ) */
    { 0x002B, 0x1B },  /* + */
    { 0x002C, 0x1C },  /* , */
    { 0x002D, 0x1D },  /* - */
    { 0x002E, 0x1E },  /* . */
    { 0x002F, 0x1F },  /* / */
    { 0x0030, 0x20 },  /* 0 */
    { 0x0031, 0x21 },  /* 1 */
    { 0x0032, 0x22 },  /* 2 */
    { 0x0033, 0x23 },  /* 3 */
    { 0x0034, 0x24 },  /* 4 */
    { 0x0035, 0x25 },  /* 5 */
    { 0x0036, 0x26 },  /* 6 */
    { 0x0037, 0x27 },  /* 7 */
    { 0x0038, 0x28 },  /* 8 */
    { 0x0039, 0x29 },  /* 9 */
    { 0x003A, 0x2A },  /* : */
    { 0x003B, 0x2B },  /* ; */
    { 0x003C, 0x2C },  /* < */
    { 0x003D, 0x2D },  /* = */
    { 0x003E, 0x2E },  /* > */
    { 0x003F, 0x2F },  /* ? */
    { 0x0040, 0x30 },  /* @ */
    { 0x0041, 0x31 },  /* A */
    { 0x0042, 0x32 },  /* B */
    { 0x0043, 0x33 },  /* C */
    { 0x0044, 0x34 },  /* D */
    { 0x0045, 0x35 },  /* E */
    { 0x0046, 0x36 },  /* F */
    { 0x0047, 0x37 },  /* G */
    { 0x0048, 0x38 },  /* H */
    { 0x0049, 0x39 },  /* I */
    { 0x004A, 0x3A },  /* J */
    { 0x004B, 0x3B },  /* K */
    { 0x004C, 0x3C },  /* L */
    { 0x004D, 0x3D },  /* M */
    { 0x004E, 0x3E },  /* N */
    { 0x004F, 0x3F },  /* O */
    { 0x0050, 0x40 },  /* P */
    { 0x0051, 0x41 },  /* Q */
    { 0x0052, 0x42 },  /* R */
    { 0x0053, 0x43 },  /* S */
    { 0x0054, 0x44 },  /* T */
    { 0x0055, 0x45 },  /* U */
    { 0x0056, 0x46 },  /* V */
    { 0x0057, 0x47 },  /* W */
    { 0x0058, 0x48 },  /* X */
    { 0x0059, 0x49 },  /* Y */
    { 0x005A, 0x4A },  /* Z */
    { 0x005B, 0x4B },  /* [ */
    { 0x005C, 0x4C },  /* \ */
    { 0x005D, 0x4D },  /* ] */
    { 0x005E, 0x4E },  /* ^ */
    { 0x005F, 0x90 },  /* _ */
    { 0x0061, 0xA1 },  /* a */
    { 0x0062, 0x9A },  /* b */
    { 0x0063, 0x9F },  /* c */
    { 0x0064, 0x9C },  /* d */
    { 0x0065, 0x92 },  /* e */
    { 0x0066, 0xAA },  /* f */
    { 0x0067, 0x97 },  /* g */
    { 0x0068, 0x98 },  /* h */
    { 0x0069, 0xA6 },  /* i */
    { 0x006A, 0xAF },  /* j */
    { 0x006B, 0xA9 },  /* k */
    { 0x006C, 0xB8 },  /* l */
    { 0x006D, 0xB3 },  /* m */
    { 0x006E, 0xB0 },  /* n */
    { 0x006F, 0xB7 },  /* o */
    { 0x0070, 0x9E },  /* p */
    { 0x0071, 0xA0 },  /* q */
    { 0x0072, 0x9D },  /* r */
    { 0x0073, 0xA4 },  /* s */
    { 0x0074, 0x96 },  /* t */
    { 0x0075, 0xA5 },  /* u */
    { 0x0076, 0xAB },  /* v */
    { 0x0077, 0xA3 },  /* w */
    { 0x0078, 0x9B },  /* x */
    { 0x0079, 0xBD },  /* y */
    { 0x007A, 0xA2 },  /* z */
    { 0x007B, 0xBE },  /* { */
    { 0x007C, 0xC0 },  /* | */
    { 0x007D, 0x80 },  /* } */
    { 0x007E, 0x94 },  /* ~ */
    { 0x007F, 0x00 },  /* DEL */
    { 0x00A3, 0xCE },  /* symbol libry */
    { 0x00C4, 0xE7 },  /* velke A s diaeresis */
    { 0x00D6, 0xE9 },  /* velke O s diaeresis */
    { 0x00DC, 0xE8 },  /* velke U s diaeresis */
    { 0x00DF, 0xE1 },  /* ostri s (Eszett) */
    { 0x00E4, 0xC7 },  /* male a s diaeresis */
    { 0x00F6, 0xC9 },  /* male o s diaeresis */
    { 0x00FC, 0xC8 },  /* male u s diaeresis */
};

/** @brief Pocet polozek ve zpetne tabulce EU. */
#define SHARPMZ_REVERSE_EU_COUNT \
    (sizeof(sharpmz_reverse_eu) / sizeof(sharpmz_reverse_eu[0]))

/**
 * @brief Zpetna konverzni tabulka pro JP variantu.
 *
 * Serazena vzestupne podle codepoint pro binarni vyhledavani.
 * Obsahuje vsechny Unicode code pointy, ktere maji mapovani na Sharp MZ JP.
 */
static const sharpmz_reverse_entry_t sharpmz_reverse_jp[] = {
    { 0x0020, 0x01 },  /* mezera -> SP */
    { 0x0021, 0x11 },  /* ! */
    { 0x0022, 0x12 },  /* " */
    { 0x0023, 0x13 },  /* # */
    { 0x0024, 0x14 },  /* $ */
    { 0x0025, 0x15 },  /* % */
    { 0x0026, 0x16 },  /* & */
    { 0x0027, 0x17 },  /* ' */
    { 0x0028, 0x18 },  /* ( */
    { 0x0029, 0x19 },  /* ) */
    { 0x002B, 0x1B },  /* + */
    { 0x002C, 0x1C },  /* , */
    { 0x002D, 0x1D },  /* - */
    { 0x002E, 0x1E },  /* . */
    { 0x002F, 0x1F },  /* / */
    { 0x0030, 0x20 },  /* 0 */
    { 0x0031, 0x21 },  /* 1 */
    { 0x0032, 0x22 },  /* 2 */
    { 0x0033, 0x23 },  /* 3 */
    { 0x0034, 0x24 },  /* 4 */
    { 0x0035, 0x25 },  /* 5 */
    { 0x0036, 0x26 },  /* 6 */
    { 0x0037, 0x27 },  /* 7 */
    { 0x0038, 0x28 },  /* 8 */
    { 0x0039, 0x29 },  /* 9 */
    { 0x003A, 0x2A },  /* : */
    { 0x003B, 0x2B },  /* ; */
    { 0x003C, 0x2C },  /* < */
    { 0x003D, 0x2D },  /* = */
    { 0x003E, 0x2E },  /* > */
    { 0x003F, 0x2F },  /* ? */
    { 0x0040, 0x30 },  /* @ */
    { 0x0041, 0x31 },  /* A */
    { 0x0042, 0x32 },  /* B */
    { 0x0043, 0x33 },  /* C */
    { 0x0044, 0x34 },  /* D */
    { 0x0045, 0x35 },  /* E */
    { 0x0046, 0x36 },  /* F */
    { 0x0047, 0x37 },  /* G */
    { 0x0048, 0x38 },  /* H */
    { 0x0049, 0x39 },  /* I */
    { 0x004A, 0x3A },  /* J */
    { 0x004B, 0x3B },  /* K */
    { 0x004C, 0x3C },  /* L */
    { 0x004D, 0x3D },  /* M */
    { 0x004E, 0x3E },  /* N */
    { 0x004F, 0x3F },  /* O */
    { 0x0050, 0x40 },  /* P */
    { 0x0051, 0x41 },  /* Q */
    { 0x0052, 0x42 },  /* R */
    { 0x0053, 0x43 },  /* S */
    { 0x0054, 0x44 },  /* T */
    { 0x0055, 0x45 },  /* U */
    { 0x0056, 0x46 },  /* V */
    { 0x0057, 0x47 },  /* W */
    { 0x0058, 0x48 },  /* X */
    { 0x0059, 0x49 },  /* Y */
    { 0x005A, 0x4A },  /* Z */
    { 0x005B, 0x4B },  /* [ */
    { 0x005D, 0x4D },  /* ] */
    { 0x005E, 0x4E },  /* ^ */
    { 0x007D, 0x80 },  /* } */
    { 0x007F, 0x00 },  /* DEL */
    { 0x00A5, 0x4C },  /* symbol yenu */
    { 0x309B, 0xBE },  /* dakuten */
    { 0x309C, 0xBF },  /* handakuten */
    { 0x30A2, 0x91 },  /* katakana A */
    { 0x30A4, 0x92 },  /* katakana I */
    { 0x30A6, 0x93 },  /* katakana U */
    { 0x30A8, 0x94 },  /* katakana E */
    { 0x30AA, 0x95 },  /* katakana O */
    { 0x30AB, 0x96 },  /* katakana KA */
    { 0x30AD, 0x97 },  /* katakana KI */
    { 0x30AF, 0x98 },  /* katakana KU */
    { 0x30B1, 0x99 },  /* katakana KE */
    { 0x30B3, 0x9A },  /* katakana KO */
    { 0x30B5, 0x9B },  /* katakana SA */
    { 0x30B7, 0x9C },  /* katakana SI */
    { 0x30B9, 0x9D },  /* katakana SU */
    { 0x30BB, 0x9E },  /* katakana SE */
    { 0x30BD, 0x9F },  /* katakana SO */
    { 0x30BF, 0xA0 },  /* katakana TA */
    { 0x30C1, 0xA1 },  /* katakana TI */
    { 0x30C4, 0xA2 },  /* katakana TU */
    { 0x30C6, 0xA3 },  /* katakana TE */
    { 0x30C8, 0xA4 },  /* katakana TO */
    { 0x30CA, 0xA5 },  /* katakana NA */
    { 0x30CB, 0xA6 },  /* katakana NI */
    { 0x30CC, 0xA7 },  /* katakana NU */
    { 0x30CD, 0xA8 },  /* katakana NE */
    { 0x30CE, 0xA9 },  /* katakana NO */
    { 0x30CF, 0xAA },  /* katakana HA */
    { 0x30D2, 0xAB },  /* katakana HI */
    { 0x30D5, 0xAC },  /* katakana HU */
    { 0x30D8, 0xAD },  /* katakana HE */
    { 0x30DB, 0xAE },  /* katakana HO */
    { 0x30DE, 0xAF },  /* katakana MA */
    { 0x30DF, 0xB0 },  /* katakana MI */
    { 0x30E0, 0xB1 },  /* katakana MU */
    { 0x30E1, 0xB2 },  /* katakana ME */
    { 0x30E2, 0xB3 },  /* katakana MO */
    { 0x30E4, 0xB4 },  /* katakana YA */
    { 0x30E6, 0xB5 },  /* katakana YU */
    { 0x30E8, 0xB6 },  /* katakana YO */
    { 0x30E9, 0xB7 },  /* katakana RA */
    { 0x30EA, 0xB8 },  /* katakana RI */
    { 0x30EB, 0xB9 },  /* katakana RU */
    { 0x30EC, 0xBA },  /* katakana RE */
    { 0x30ED, 0xBB },  /* katakana RO */
    { 0x30EF, 0xBC },  /* katakana WA */
    { 0x30F2, 0x90 },  /* katakana WO */
    { 0x30F3, 0xBD },  /* katakana N */
};

/** @brief Pocet polozek ve zpetne tabulce JP. */
#define SHARPMZ_REVERSE_JP_COUNT \
    (sizeof(sharpmz_reverse_jp) / sizeof(sharpmz_reverse_jp[0]))

/* ======================================================================== */
/*  Pomocne interni funkce                                                  */
/* ======================================================================== */

/**
 * @brief Dekoduje jeden Unicode code point z UTF-8 sekvence.
 *
 * Precte 1-4 bajty ze vstupu a dekoduje Unicode code point.
 * Zaroven nastavi pocet prectenych bajtu.
 *
 * @param[in]  utf8       Ukazatel na UTF-8 retezec.
 * @param[out] out_bytes  Pocet prectenych bajtu (1-4). Pri nevalidnim
 *                        vstupu se nastavi na 1 (preskoceni jednoho bajtu).
 * @return Dekodovany Unicode code point, nebo 0xFFFFFFFF pri chybe.
 *
 * @pre utf8 nesmi byt NULL.
 * @pre out_bytes nesmi byt NULL.
 * @post *out_bytes je vzdy >= 1 (i pri chybe).
 */
static uint32_t sharpmz_utf8_decode(const char *utf8, int *out_bytes)
{
    const unsigned char *s = (const unsigned char *)utf8;
    uint32_t cp;

    if (s[0] < 0x80) {
        /* 1-bajtova sekvence (ASCII) */
        *out_bytes = 1;
        return s[0];
    } else if ((s[0] & 0xE0) == 0xC0) {
        /* 2-bajtova sekvence */
        if ((s[1] & 0xC0) != 0x80) {
            *out_bytes = 1;
            return 0xFFFFFFFF;
        }
        cp = ((uint32_t)(s[0] & 0x1F) << 6) |
             ((uint32_t)(s[1] & 0x3F));
        *out_bytes = 2;
        return cp;
    } else if ((s[0] & 0xF0) == 0xE0) {
        /* 3-bajtova sekvence */
        if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) {
            *out_bytes = 1;
            return 0xFFFFFFFF;
        }
        cp = ((uint32_t)(s[0] & 0x0F) << 12) |
             ((uint32_t)(s[1] & 0x3F) << 6) |
             ((uint32_t)(s[2] & 0x3F));
        *out_bytes = 3;
        return cp;
    } else if ((s[0] & 0xF8) == 0xF0) {
        /* 4-bajtova sekvence */
        if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 ||
            (s[3] & 0xC0) != 0x80) {
            *out_bytes = 1;
            return 0xFFFFFFFF;
        }
        cp = ((uint32_t)(s[0] & 0x07) << 18) |
             ((uint32_t)(s[1] & 0x3F) << 12) |
             ((uint32_t)(s[2] & 0x3F) << 6) |
             ((uint32_t)(s[3] & 0x3F));
        *out_bytes = 4;
        return cp;
    }

    /* Nevalidni uvodni bajt */
    *out_bytes = 1;
    return 0xFFFFFFFF;
}

/**
 * @brief Hleda code point ve zpetne konverzni tabulce binarnim vyhledavanim.
 *
 * @param[in] table  Serazena zpetna tabulka.
 * @param[in] count  Pocet polozek v tabulce.
 * @param[in] cp     Hledany Unicode code point.
 * @return Index nalezene polozky, nebo -1 pokud neni nalezena.
 *
 * @pre table musi byt serazena vzestupne podle codepoint.
 */
static int sharpmz_reverse_lookup(const sharpmz_reverse_entry_t *table,
                                  size_t count, uint32_t cp)
{
    size_t low = 0;
    size_t high = count;

    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (table[mid].codepoint < cp) {
            low = mid + 1;
        } else if (table[mid].codepoint > cp) {
            high = mid;
        } else {
            return (int)mid;
        }
    }

    return -1;
}

/* ======================================================================== */
/*  Verejne API                                                             */
/* ======================================================================== */

const char *sharpmz_to_utf8(uint8_t mz_code, sharpmz_charset_t charset)
{
    if (charset == SHARPMZ_CHARSET_JP) {
        return sharpmz_utf8_table_jp[mz_code];
    }
    return sharpmz_utf8_table_eu[mz_code];
}

int sharpmz_from_utf8(const char *utf8, sharpmz_charset_t charset)
{
    int bytes;
    uint32_t cp;
    int idx;
    const sharpmz_reverse_entry_t *table;
    size_t count;

    if (utf8 == NULL || *utf8 == '\0') {
        return -1;
    }

    cp = sharpmz_utf8_decode(utf8, &bytes);
    if (cp == 0xFFFFFFFF) {
        return -1;
    }

    if (charset == SHARPMZ_CHARSET_JP) {
        table = sharpmz_reverse_jp;
        count = SHARPMZ_REVERSE_JP_COUNT;
    } else {
        table = sharpmz_reverse_eu;
        count = SHARPMZ_REVERSE_EU_COUNT;
    }

    idx = sharpmz_reverse_lookup(table, count, cp);
    if (idx < 0) {
        return -1;
    }

    return table[idx].mz_code;
}

int sharpmz_str_to_utf8(const uint8_t *src, size_t src_len,
                        char *dst, size_t dst_size,
                        sharpmz_charset_t charset)
{
    size_t i;
    size_t written = 0;

    if (src == NULL || dst == NULL || dst_size == 0) {
        return -1;
    }

    for (i = 0; i < src_len; i++) {
        const char *utf8_char = sharpmz_to_utf8(src[i], charset);
        size_t char_len = strlen(utf8_char);

        /* Overeni, ze se znak vejde do bufferu (+ misto pro null terminator) */
        if (written + char_len >= dst_size) {
            break;
        }

        memcpy(dst + written, utf8_char, char_len);
        written += char_len;
    }

    dst[written] = '\0';
    return (int)written;
}

int sharpmz_str_from_utf8(const char *src,
                          uint8_t *dst, size_t dst_size,
                          sharpmz_charset_t charset)
{
    size_t written = 0;

    if (src == NULL || dst == NULL || dst_size == 0) {
        return -1;
    }

    while (*src != '\0') {
        int bytes;
        int mz_code;

        /* Overeni, ze se znak vejde do bufferu (+ null terminator) */
        if (written + 1 >= dst_size) {
            break;
        }

        mz_code = sharpmz_from_utf8(src, charset);
        if (mz_code >= 0) {
            dst[written] = (uint8_t)mz_code;
        } else {
            /* Neznamý znak nahradit mezerou */
            dst[written] = 0x20;
        }
        written++;

        /* Posun na dalsi UTF-8 znak */
        (void)sharpmz_utf8_decode(src, &bytes);
        src += bytes;
    }

    dst[written] = '\0';
    return (int)written;
}
