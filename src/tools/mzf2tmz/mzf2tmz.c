/**
 * @file   mzf2tmz.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.11.0
 * @brief  Konverzni utility MZF -> TMZ.
 *
 * Nacte MZF soubor (nebo MZT soubor s vice MZF) a vytvori z nej
 * TMZ soubor. Pri formatu NORMAL a rychlosti 1:1 pouziva blok 0x40
 * (MZ Standard Data), jinak blok 0x41 (MZ Turbo Data).
 * Pokud vystupni TMZ soubor jiz existuje, prida bloky na konec
 * existujici pasky. Pokud neexistuje, vytvori novy TMZ soubor.
 *
 * Vstupni soubor s priponou .mzt/.MZT je zpracovan jako sekvence
 * vice MZF souboru poskladanych za sebou (kazdy 128B hlavicka + telo).
 * Bez voleb formátu a --machine (nebo s --profile UNICMT) dá každý soubor blok 0x46
 * s profilem UNICMT a rychlostí z hlaviček CMTSPEED UniCMT (spec 10.2,
 * tmz_mzt_build_unicmt_blocks). MZT s CMTSPEED a jinými volbami je chyba.
 *
 * @par Pouziti:
 * @code
 *   mzf2tmz input.mzf output.tmz [volby]
 *   mzf2tmz input.mzt output.tmz [volby]
 * @endcode
 *
 * @par Volby:
 * - --machine <typ>        : generic, mz700, mz800, mz1500, mz80b (vychozi: mz800)
 * - --pulseset <sada>      : 700, 800, 80b, auto (vychozi: auto z machine)
 * - --format <format>      : normal, turbo, fastipl, fsk, slow, direct, cpm-tape
 *                            (sinclair se odmítne: ZX záznam patří do bloků TZX 0x10/0x11)
 *                            (vychozi: normal)
 * - --speed <rychlost>     : 1:1, 2:1, 2:1cpm, 3:1, 3:2, 7:3, 8:3, 9:7, 25:14;
 *                            s --profile TC-1.21 i jiný poměr TurboCopy a:b (číslice 1-9, 1/2 až 3/1)
 *                            (vychozi: 1:1; neplatny pro FSK a SLOW formaty)
 * - --fsk-speed <level>    : rychlostni uroven FSK koderu 0-6 (0=nejpomalejsi,
 *                            6=nejrychlejsi; vychozi: 0; pouze pro --format fsk)
 * - --slow-speed <level>   : rychlostni uroven SLOW koderu 0-4 (0=nejpomalejsi,
 *                            4=nejrychlejsi; vychozi: 0; pouze pro --format slow)
 * - --pause <ms>           : pauza po bloku v ms (vychozi: 1000); s --profile pauza za
 *                            posledním dílem bloku 0x46
 * - --profile <ID>         : vytvořit blok 0x46 (TMZ v1.1) podle profilu z registru
 *                            (ROM-800-NOMINAL, ROM-800-EXACT, TC-1.21); "list" vypíše registr.
 *                            Nelze kombinovat s --format, --pulseset, --fsk-speed, --slow-speed.
 * - --loader y|n           : TC-1.21: s předzavaděčem (y, výchozí) nebo bez (n)
 * - --gap rom|short        : ROM profily: tóny ROM 22000/11000, nebo poloviční 11000/5500
 * - --bd <rychlost>        : profily IC-10.2-*: 600, 1200 (výchozí), 2400, 2800, 3200;
 *                            IC-10.2-CPM 1500, 2400 (výchozí), 2800, 3200, 3600;
 *                            PEZIK-2.0A (i --format cpm-tape) 2400 nebo 3200
 * - --charset <mode>       : znakova sada: eu (vychozi), jp, utf8-eu, utf8-jp
 * - --version              : zobrazit verzi programu
 * - --lib-versions         : zobrazit verze knihoven
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tzx/tzx.h"
#include "libs/mzf/mzf.h"
#include "libs/mzf/mzf_tools.h"
#include "libs/cmtspeed/cmtspeed.h"
#include "libs/tmz/tmz_profile.h"
#include "libs/tmz/tmz_mzt.h"
#include "libs/endianity/endianity.h"
#include "libs/mzcmt_turbo/mzcmt_turbo.h"

/** @brief Verze programu mzf2tmz (z @version v hlavicce souboru). */
#define MZF2TMZ_VERSION  "1.11.0"


/**
 * @brief Vrati textovy nazev ciloveho stroje.
 * @param machine Hodnota en_TMZ_MACHINE.
 * @return Staticky retezec s nazvem.
 */
static const char* machine_name ( en_TMZ_MACHINE machine ) {
    switch ( machine ) {
        case TMZ_MACHINE_GENERIC: return "Generic";
        case TMZ_MACHINE_MZ700:   return "MZ-700";
        case TMZ_MACHINE_MZ800:   return "MZ-800";
        case TMZ_MACHINE_MZ1500:  return "MZ-1500";
        case TMZ_MACHINE_MZ80B:   return "MZ-80B";
        default:                  return "Unknown";
    }
}


/**
 * @brief Vrati textovy nazev pulzni sady.
 * @param pulseset Hodnota en_TMZ_PULSESET.
 * @return Staticky retezec s nazvem.
 */
static const char* pulseset_name ( en_TMZ_PULSESET pulseset ) {
    switch ( pulseset ) {
        case TMZ_PULSESET_700: return "MZ-700/80K/80A";
        case TMZ_PULSESET_800: return "MZ-800/1500";
        case TMZ_PULSESET_80B: return "MZ-80B";
        default:               return "Unknown";
    }
}


/**
 * @brief Vrati textovy nazev typu MZF souboru.
 * @param ftype Hodnota MZF ftype.
 * @return Staticky retezec s nazvem.
 */
static const char* mzf_ftype_name ( uint8_t ftype ) {
    switch ( ftype ) {
        case 0x01: return "OBJ (machine code)";
        case 0x02: return "BTX (BASIC text)";
        case 0x03: return "BSD (BASIC data)";
        case 0x04: return "BRD (BASIC data, WOPEN#)";   /* zapisuje WOPEN# BASICu 1Z-016 */
        case 0x05: return "RB (read and branch)";
        default:   return "Unknown";
    }
}


/**
 * @brief Automaticky urci pulzni sadu podle ciloveho stroje.
 *
 * MZ-700/80K/80A -> TMZ_PULSESET_700,
 * MZ-800/1500    -> TMZ_PULSESET_800,
 * MZ-80B         -> TMZ_PULSESET_80B,
 * Generic        -> TMZ_PULSESET_700 (vychozi).
 *
 * @param machine Cilovy stroj.
 * @return Odpovidajici pulzni sada.
 */
static en_TMZ_PULSESET pulseset_from_machine ( en_TMZ_MACHINE machine ) {
    switch ( machine ) {
        case TMZ_MACHINE_MZ800:  return TMZ_PULSESET_800;
        case TMZ_MACHINE_MZ1500: return TMZ_PULSESET_800;
        case TMZ_MACHINE_MZ80B:  return TMZ_PULSESET_80B;
        default:                 return TMZ_PULSESET_700;
    }
}


/**
 * @brief Naparsuje retezec na hodnotu en_TMZ_MACHINE.
 *
 * Rozpoznava: "generic", "mz700", "mz800", "mz1500", "mz80b".
 * Porovnani je case-insensitive.
 *
 * @param str Vstupni retezec.
 * @param[out] machine Vystupni hodnota.
 * @return 0 pri uspechu, -1 pokud retezec neodpovida zadnemu stroji.
 */
static int parse_machine ( const char *str, en_TMZ_MACHINE *machine ) {
    if ( strcasecmp ( str, "generic" ) == 0 ) { *machine = TMZ_MACHINE_GENERIC; return 0; }
    if ( strcasecmp ( str, "mz700" ) == 0 )   { *machine = TMZ_MACHINE_MZ700;   return 0; }
    if ( strcasecmp ( str, "mz800" ) == 0 )   { *machine = TMZ_MACHINE_MZ800;   return 0; }
    if ( strcasecmp ( str, "mz1500" ) == 0 )  { *machine = TMZ_MACHINE_MZ1500;  return 0; }
    if ( strcasecmp ( str, "mz80b" ) == 0 )   { *machine = TMZ_MACHINE_MZ80B;   return 0; }
    return -1;
}


/**
 * @brief Naparsuje retezec na hodnotu en_TMZ_PULSESET.
 *
 * Rozpoznava: "700", "800", "80b", "auto".
 * "auto" nastavi pulseset na TMZ_PULSESET_COUNT jako signal pro autodetekci.
 *
 * @param str Vstupni retezec.
 * @param[out] pulseset Vystupni hodnota.
 * @return 0 pri uspechu, -1 pokud retezec neodpovida zadne sade.
 */
static int parse_pulseset ( const char *str, en_TMZ_PULSESET *pulseset ) {
    if ( strcasecmp ( str, "700" ) == 0 )  { *pulseset = TMZ_PULSESET_700; return 0; }
    if ( strcasecmp ( str, "800" ) == 0 )  { *pulseset = TMZ_PULSESET_800; return 0; }
    if ( strcasecmp ( str, "80b" ) == 0 )  { *pulseset = TMZ_PULSESET_80B; return 0; }
    if ( strcasecmp ( str, "auto" ) == 0 ) { *pulseset = TMZ_PULSESET_COUNT; return 0; }
    return -1;
}


/**
 * @brief Naparsuje retezec na hodnotu en_TMZ_FORMAT.
 *
 * Rozpoznava: "normal", "turbo", "fastipl", "fsk", "slow", "direct",
 * "cpm-tape". "sinclair" nerozpozná: blok 0x41 SINCLAIR player nepřehraje
 *, volající vypíše vlastní hlášku.
 * Porovnani je case-insensitive.
 *
 * @param str Vstupni retezec.
 * @param[out] format Vystupni hodnota.
 * @return 0 pri uspechu, -1 pokud retezec neodpovida zadnemu formatu.
 */
static int parse_format ( const char *str, en_TMZ_FORMAT *format ) {
    if ( strcasecmp ( str, "normal" ) == 0 )   { *format = TMZ_FORMAT_NORMAL;   return 0; }
    if ( strcasecmp ( str, "turbo" ) == 0 )    { *format = TMZ_FORMAT_TURBO;    return 0; }
    if ( strcasecmp ( str, "fastipl" ) == 0 )  { *format = TMZ_FORMAT_FASTIPL;  return 0; }
    if ( strcasecmp ( str, "fsk" ) == 0 )      { *format = TMZ_FORMAT_FSK;      return 0; }
    if ( strcasecmp ( str, "slow" ) == 0 )     { *format = TMZ_FORMAT_SLOW;     return 0; }
    if ( strcasecmp ( str, "direct" ) == 0 )   { *format = TMZ_FORMAT_DIRECT;   return 0; }
    if ( strcasecmp ( str, "cpm-tape" ) == 0 ) { *format = TMZ_FORMAT_CPM_TAPE; return 0; }
    return -1;
}


/**
 * @brief Vrati textovy nazev formatu zaznamu.
 * @param format Hodnota en_TMZ_FORMAT.
 * @return Staticky retezec s nazvem.
 */
static const char* format_name ( en_TMZ_FORMAT format ) {
    switch ( format ) {
        case TMZ_FORMAT_NORMAL:   return "NORMAL";
        case TMZ_FORMAT_TURBO:    return "TURBO";
        case TMZ_FORMAT_FASTIPL:  return "FASTIPL";
        case TMZ_FORMAT_SINCLAIR: return "SINCLAIR";
        case TMZ_FORMAT_FSK:      return "FSK";
        case TMZ_FORMAT_SLOW:     return "SLOW";
        case TMZ_FORMAT_DIRECT:   return "DIRECT";
        case TMZ_FORMAT_CPM_TAPE: return "CPM-TAPE";
        default:                  return "Unknown";
    }
}


/**
 * @brief Naparsuje retezec na hodnotu en_CMTSPEED.
 *
 * Rozpoznava pomery: "1:1", "2:1", "2:1cpm", "3:1", "3:2", "7:3", "8:3", "9:7", "25:14".
 * Rozpoznava take baudrate: "1200", "2400", "2800", "3200", "3600" atd.
 * Baudrate se mapuje na nejblizsi en_CMTSPEED s bazi 1200 Bd.
 *
 * @param str Vstupni retezec.
 * @param[out] speed Vystupni hodnota.
 * @return 0 pri uspechu, -1 pokud retezec neodpovida zadne rychlosti.
 */
static int parse_speed ( const char *str, en_CMTSPEED *speed ) {
    /* pomery */
    if ( strcmp ( str, "1:1" ) == 0 )    { *speed = CMTSPEED_1_1;    return 0; }
    if ( strcmp ( str, "2:1" ) == 0 )    { *speed = CMTSPEED_2_1;    return 0; }
    if ( strcasecmp ( str, "2:1cpm" ) == 0 ) { *speed = CMTSPEED_2_1_CPM; return 0; }
    if ( strcmp ( str, "3:1" ) == 0 )    { *speed = CMTSPEED_3_1;    return 0; }
    if ( strcmp ( str, "3:2" ) == 0 )    { *speed = CMTSPEED_3_2;    return 0; }
    if ( strcmp ( str, "7:3" ) == 0 )    { *speed = CMTSPEED_7_3;    return 0; }
    if ( strcmp ( str, "8:3" ) == 0 )    { *speed = CMTSPEED_8_3;    return 0; }
    if ( strcmp ( str, "9:7" ) == 0 )    { *speed = CMTSPEED_9_7;    return 0; }
    if ( strcmp ( str, "25:14" ) == 0 )  { *speed = CMTSPEED_25_14;  return 0; }

    /* baudrate (ciselna hodnota > 100) */
    char *endptr;
    long val = strtol ( str, &endptr, 10 );
    if ( *endptr == '\0' && val > 100 && val <= 10000 ) {
        *speed = cmtspeed_from_bdspeed ( ( uint16_t ) val, 1200 );
        return 0;
    }

    return -1;
}


/**
 * @brief Zjisti, zda ma soubor priponu .mzt nebo .MZT.
 *
 * MZT soubor obsahuje vice MZF souboru poskladanych za sebou.
 *
 * @param path Cesta k souboru.
 * @return 1 pokud je pripona .mzt/.MZT, jinak 0.
 */
static int has_mzt_extension ( const char *path ) {
    const char *dot = strrchr ( path, '.' );
    if ( !dot ) return 0;
    return ( strcasecmp ( dot, ".mzt" ) == 0 );
}


/**
 * @brief Ověří, že volby zadané k --profile profil opravdu používá.
 *
 * Pravidla odpovídají knihovně (tmz_profile_build_block() odmítne parametr,
 * který profil nepoužívá); tady jen s hláškou, která jmenuje volbu.
 *
 * @param id ID profilu (existující v registru).
 * @param speed_set Zadáno --speed.
 * @param loader_set Zadáno --loader.
 * @param gap_set Zadáno --gap.
 * @return Text chyby (anglicky, bez "Error: "), nebo NULL, když volby sedí.
 */
static const char* profile_option_error ( const char *id, int speed_set, int loader_set, int gap_set ) {
    int rom = ( strncmp ( id, "ROM-800-", 8 ) == 0 );
    int unicmt = ( strcmp ( id, "UNICMT" ) == 0 );
    int tc = ( strcmp ( id, "TC-1.21" ) == 0 );
    int ic = ( strncmp ( id, "IC-10.2-", 8 ) == 0 );
    int pezik = ( strcmp ( id, "PEZIK-2.0A" ) == 0 );
    if ( strcmp ( id, "TAPEMZ-V1-LEGACY" ) == 0 ) {
        return "profile TAPEMZ-V1-LEGACY is produced only by block conversion (tmzconv --upgrade)";
    }
    if ( speed_set && ic ) return "--speed is not used with the IC-10.2-* profiles (use --bd)";
    if ( speed_set && pezik ) return "--speed is not used with PEZIK-2.0A (use --bd 2400 or 3200)";
    if ( speed_set && unicmt ) return "UNICMT takes the speed from the CMTSPEED headers of an MZT input; --speed is not used";
    if ( loader_set && !tc ) return "--loader is only valid with TC-1.21";
    if ( gap_set && !rom && !unicmt ) return "--gap is only valid with the ROM-800-* and UNICMT profiles";
    return NULL;
}


/**
 * @brief Uvolní načtené MZF a bloky UNICMT, které nepřevzal soubor TMZ.
 * @param mzfs MZF. @param n Počet MZF.
 * @param blocks Bloky UNICMT (smí být NULL; převzaté jsou NULL). @param nb Počet bloků.
 * @param speed Rychlosti UNICMT (smí být NULL).
 */
static void free_inputs ( st_MZF **mzfs, uint32_t n, st_TZX_BLOCK **blocks, uint32_t nb, void *speed ) {
    for ( uint32_t j = 0; j < n; j++ ) mzf_free ( mzfs[j] );
    for ( uint32_t j = 0; blocks && j < nb; j++ ) {
        if ( blocks[j] ) tmz_block_free ( blocks[j] );
    }
    free ( blocks );
    free ( speed );
}


/**
 * @brief Spočítá hlavičky CMTSPEED v MZT tolerantně (zastaví se u useknutého konce).
 * @param data MZT. @param size Délka.
 * @return Počet hlaviček CMTSPEED v přečtené části.
 */
static uint32_t mzt_count_cmtspeed ( const uint8_t *data, uint32_t size ) {
    uint32_t pos = 0, n = 0;
    while ( size - pos >= TMZ_MZT_HEADER_SIZE ) {
        const uint8_t *h = data + pos;
        if ( tmz_mzt_is_cmtspeed ( h, NULL ) ) n++;
        uint32_t fsize = ( uint32_t ) ( h[0x12] | ( h[0x13] << 8 ) );
        if ( size - pos - TMZ_MZT_HEADER_SIZE < fsize ) break;
        pos += TMZ_MZT_HEADER_SIZE + fsize;
    }
    return n;
}


/**
 * @brief Načte celý soubor do paměti.
 * @param path Cesta.
 * @param[out] size Velikost.
 * @return Data (uvolnit free), nebo NULL při chybě (soubor nejde otevřít, je prázdný, přes 16 MB, alokace).
 */
static uint8_t* read_whole_file ( const char *path, uint32_t *size ) {
    *size = 0;
    FILE *fp = fopen ( path, "rb" );
    if ( !fp ) return NULL;
    uint8_t *data = NULL;
    if ( fseek ( fp, 0, SEEK_END ) == 0 ) {
        long n = ftell ( fp );
        if ( n > 0 && n <= 16L * 1024 * 1024 && fseek ( fp, 0, SEEK_SET ) == 0 ) {
            data = malloc ( ( size_t ) n );
            if ( data && fread ( data, 1, ( size_t ) n, fp ) != ( size_t ) n ) {
                free ( data );
                data = NULL;
            }
            if ( data ) *size = ( uint32_t ) n;
        }
    }
    fclose ( fp );
    return data;
}


/**
 * @brief Popis rychlosti UniCMT pro výpis.
 * @param us Pulzy LONG H, LONG L, SHORT H, SHORT L v µs.
 * @return "1x", "2x", "3x" (sady UniCMT), nebo "custom".
 */
static const char* unicmt_speed_name ( const uint16_t us[4] ) {
    static const uint16_t sets[3][4] = { { 470, 494, 240, 278 }, { 235, 247, 120, 139 }, { 156, 164, 80, 92 } };
    static const char *const names[3] = { "1x", "2x", "3x" };
    for ( int i = 0; i < 3; i++ ) {
        if ( memcmp ( us, sets[i], sizeof ( sets[i] ) ) == 0 ) return names[i];
    }
    return "custom";
}


/**
 * @brief Vytvori TMZ blok z MZF podle zadaneho formatu a rychlosti.
 *
 * Pokud format == NORMAL a speed_byte odpovida CMTSPEED_1_1, vytvori
 * blok 0x40 (MZ Standard Data). Jinak vytvori blok 0x41 (MZ Turbo Data).
 *
 * @param mzf MZF soubor. Nesmi byt NULL.
 * @param machine Cilovy stroj.
 * @param pulseset Pulzni sada.
 * @param format Format zaznamu.
 * @param speed_byte Surova hodnota rychlosti pro pole speed bloku 0x41.
 *        Interpretace zavisi na formatu: pro FM formaty (NORMAL/TURBO/
 *        FASTIPL/SINCLAIR) je to en_CMTSPEED, pro FSK je to nativni
 *        uroven 0-6, pro SLOW nativni uroven 0-4.
 * @param pause_ms Pauza po bloku v ms.
 * @return Novy alokovany blok, nebo NULL pri chybe.
 *         Volajici musi uvolnit pomoci tmz_block_free() nebo free().
 */
static st_TZX_BLOCK* create_block_for_mzf ( const st_MZF *mzf,
                                              en_TMZ_MACHINE machine,
                                              en_TMZ_PULSESET pulseset,
                                              en_TMZ_FORMAT format,
                                              uint8_t speed_byte,
                                              uint16_t pause_ms ) {
    if ( format == TMZ_FORMAT_NORMAL && speed_byte == ( uint8_t ) CMTSPEED_1_1 ) {
        /* standardni NORMAL 1:1 -> blok 0x40 */
        return tmz_block_from_mzf ( mzf, machine, pulseset, pause_ms );
    }

    /* nestandardni format/rychlost -> blok 0x41 */
    st_TMZ_MZ_TURBO_DATA params;
    memset ( &params, 0, sizeof ( params ) );
    params.machine = ( uint8_t ) machine;
    params.pulseset = ( uint8_t ) pulseset;
    params.format = ( uint8_t ) format;
    params.speed = speed_byte;
    params.pause_ms = pause_ms;
    params.flags = 0;
    memcpy ( &params.mzf_header, &mzf->header, sizeof ( st_MZF_HEADER ) );
    params.body_size = ( uint16_t ) mzf->body_size;

    return tmz_block_create_mz_turbo ( &params, mzf->body, ( uint16_t ) mzf->body_size );
}


/**
 * @brief Vypise verze vsech pouzitych knihoven na stdout.
 */
static void print_lib_versions ( void ) {
    printf ( "Library versions:\n" );
    printf ( "  tmz            %s (TMZ format v%s)\n", tmz_version (), tmz_format_version () );
    printf ( "  tzx            %s (TZX format v%s)\n", tzx_version (), tzx_format_version () );
    printf ( "  mzf            %s\n", mzf_version () );
    printf ( "  cmtspeed       %s\n", cmtspeed_version () );
    printf ( "  endianity      %s\n", endianity_version () );
}


/**
 * @brief Vypise napovedu programu.
 * @param prog_name Nazev spusteneho programu (argv[0]).
 */
static void print_usage ( const char *prog_name ) {
    fprintf ( stderr, "Usage: %s <input.mzf|input.mzt> <output.tmz> [options]\n\n", prog_name );
    fprintf ( stderr, "Converts MZF/MZT file(s) to TMZ format. MZT files may contain\n" );
    fprintf ( stderr, "multiple MZF files concatenated together.\n\n" );
    fprintf ( stderr, "Options:\n" );
    fprintf ( stderr, "  --machine <type>    Target machine: generic, mz700, mz800, mz1500, mz80b\n" );
    fprintf ( stderr, "                      (default: mz800)\n" );
    fprintf ( stderr, "  --pulseset <set>    Pulse set: 700, 800, 80b, auto (default: auto)\n" );
    fprintf ( stderr, "  --format <fmt>      Recording format: normal, turbo, fastipl, fsk, slow,\n" );
    fprintf ( stderr, "                      direct, cpm-tape (default: normal);\n" );
    fprintf ( stderr, "                      cpm-tape = --profile PEZIK-2.0A (block 0x46, speed --bd)\n" );
    fprintf ( stderr, "  --speed <ratio>     Speed ratio: 1:1, 2:1, 2:1cpm, 3:1, 3:2, 7:3,\n" );
    fprintf ( stderr, "                      8:3, 9:7, 25:14 (default: 1:1); with --profile TC-1.21\n" );
    fprintf ( stderr, "                      any TurboCopy ratio a:b (digits 1-9, 1:2 to 3:1)\n" );
    fprintf ( stderr, "                      Not valid for FSK/SLOW formats (use --fsk-speed/--slow-speed)\n" );
    fprintf ( stderr, "  --fsk-speed <level> FSK speed level: 0-6 (0=slowest, 6=fastest, default: 0)\n" );
    fprintf ( stderr, "                      Only valid with --format fsk\n" );
    fprintf ( stderr, "  --slow-speed <level> SLOW speed level: 0-4 (0=slowest, 4=fastest, default: 0)\n" );
    fprintf ( stderr, "                      Only valid with --format slow\n" );
    fprintf ( stderr, "  --pause <ms>        Pause after block in ms (default: 1000)\n" );
    fprintf ( stderr, "  --profile <ID>      Create a TMZ v1.1 MZ File block (0x46) by a registry profile:\n" );
    fprintf ( stderr, "                      ROM-800-NOMINAL, ROM-800-EXACT, TC-1.21, UNICMT,\n" );
    fprintf ( stderr, "                      IC-10.2-NORMAL, IC-10.2-TURBO, IC-10.2-FASTIPL,\n" );
    fprintf ( stderr, "                      IC-10.2-CPM, PEZIK-2.0A\n" );
    fprintf ( stderr, "                      ('--profile list' prints them; MZT input defaults to UNICMT)\n" );
    fprintf ( stderr, "                      --machine is stored in the block; --speed: ROM 1:1, TC-1.21\n" );
    fprintf ( stderr, "  --loader y|n        TC-1.21: with preloader (y, default) or without (n)\n" );
    fprintf ( stderr, "  --gap rom|short     ROM-800-*, UNICMT (also MZT input without --profile\n" );
    fprintf ( stderr, "                      and without format options, including --machine):\n" );
    fprintf ( stderr, "                      ROM tones 22000/11000 or half 11000/5500\n" );
    fprintf ( stderr, "  --bd <speed>        IC-10.2-* profiles: 600, 1200 (default), 2400, 2800, 3200;\n" );
    fprintf ( stderr, "                      IC-10.2-CPM: 1500, 2400 (default), 2800, 3200, 3600;\n" );
    fprintf ( stderr, "                      PEZIK-2.0A (and --format cpm-tape): 2400, 3200\n" );
    fprintf ( stderr, "  --charset <mode>        Character set: eu (default), jp, utf8-eu, utf8-jp\n" );
    fprintf ( stderr, "  --version             Show program version\n" );
    fprintf ( stderr, "  --lib-versions        Show library versions\n" );
}


/**
 * @brief Nacte jeden nebo vice MZF souboru ze vstupu.
 *
 * Pokud je vstup MZT (pripona .mzt), nacte vsechny MZF soubory
 * z jednoho souboru (sekvence 128B hlavicka + telo).
 * Jinak nacte jeden MZF soubor.
 *
 * @param input_file Cesta k vstupnimu souboru.
 * @param is_mzt Nenulove pokud jde o MZT format.
 * @param[out] mzfs Pole ukazatelu na nactene MZF soubory.
 * @param max_mzfs Maximalni pocet MZF v poli.
 * @param[out] mzf_count Pocet nactenych MZF souboru.
 * @note Bajt 11h hlavičky (fname.terminator) zůstává podle souboru: knihovna mzf
 *       ho při čtení nastaví na 0Dh, proto se u MZF přečte znovu ze souboru.
 *       MZT se čte po bajtech, 11h tam zůstává také.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 *
 * @post Pri uspechu volajici vlastni mzfs[0..mzf_count-1] a musi
 *       je uvolnit pomoci mzf_free().
 */
static int load_mzf_files ( const char *input_file, int is_mzt,
                             st_MZF **mzfs, uint32_t max_mzfs,
                             uint32_t *mzf_count ) {
    *mzf_count = 0;

    if ( !is_mzt ) {
        /* jednoduchy MZF soubor */
        st_HANDLER in_handler;
        st_DRIVER in_driver;
        generic_driver_file_init ( &in_driver );

        st_HANDLER *h_in = generic_driver_open_file ( &in_handler, &in_driver,
                                                       (char*) input_file, FILE_DRIVER_OPMODE_RO );
        if ( !h_in ) {
            fprintf ( stderr, "Error: cannot open input file '%s'\n", input_file );
            return EXIT_FAILURE;
        }

        en_MZF_ERROR mzf_err;
        st_MZF *mzf = mzf_load ( h_in, &mzf_err );
        generic_driver_close ( h_in );

        if ( !mzf ) {
            fprintf ( stderr, "Error: failed to load MZF file: %d\n", mzf_err );
            return EXIT_FAILURE;
        }

        {
            /* 11h podle souboru (mzf_load ho nastavil na 0Dh) */
            FILE *fp = fopen ( input_file, "rb" );
            int c = ( fp && fseek ( fp, 0x11, SEEK_SET ) == 0 ) ? fgetc ( fp ) : EOF;
            if ( fp ) fclose ( fp );
            if ( c != EOF ) mzf->header.fname.terminator = ( uint8_t ) c;
        }

        mzfs[0] = mzf;
        *mzf_count = 1;
        return EXIT_SUCCESS;
    }

    /* MZT soubor - vice MZF za sebou */
    FILE *fp = fopen ( input_file, "rb" );
    if ( !fp ) {
        fprintf ( stderr, "Error: cannot open input file '%s'\n", input_file );
        return EXIT_FAILURE;
    }

    fseek ( fp, 0, SEEK_END );
    long file_size = ftell ( fp );
    fseek ( fp, 0, SEEK_SET );

    if ( file_size < (long) sizeof ( st_MZF_HEADER ) ) {
        fprintf ( stderr, "Error: file too small for MZT format (%ld bytes)\n", file_size );
        fclose ( fp );
        return EXIT_FAILURE;
    }

    uint32_t offset = 0;

    while ( offset < (uint32_t) file_size && *mzf_count < max_mzfs ) {
        /* zbyva dost dat pro hlavicku? */
        if ( offset + sizeof ( st_MZF_HEADER ) > (uint32_t) file_size ) break;

        /* nacteni hlavicky */
        st_MZF_HEADER hdr;
        fseek ( fp, (long) offset, SEEK_SET );
        if ( fread ( &hdr, 1, sizeof ( st_MZF_HEADER ), fp ) != sizeof ( st_MZF_HEADER ) ) break;

        /* korekce endianity (fsize, fstrt, fexec jsou LE) */
        hdr.fsize = endianity_bswap16_LE ( hdr.fsize );
        hdr.fstrt = endianity_bswap16_LE ( hdr.fstrt );
        hdr.fexec = endianity_bswap16_LE ( hdr.fexec );

        uint32_t body_offset = offset + sizeof ( st_MZF_HEADER );
        uint32_t body_size = hdr.fsize;

        /* kontrola, ze telo nepresahuje soubor */
        if ( body_offset + body_size > (uint32_t) file_size ) {
            fprintf ( stderr, "Warning: MZF #%u truncated at offset %u (need %u bytes, have %u)\n",
                      *mzf_count + 1, offset, body_size,
                      (uint32_t) file_size - body_offset );
            break;
        }

        /* alokace a naplneni MZF struktury */
        st_MZF *mzf = ( st_MZF* ) calloc ( 1, sizeof ( st_MZF ) );
        if ( !mzf ) {
            fprintf ( stderr, "Error: memory allocation failed\n" );
            fclose ( fp );
            return EXIT_FAILURE;
        }
        mzf->header = hdr;
        mzf->body_size = body_size;

        if ( body_size > 0 ) {
            mzf->body = ( uint8_t* ) malloc ( body_size );
            if ( !mzf->body ) {
                fprintf ( stderr, "Error: memory allocation failed\n" );
                free ( mzf );
                fclose ( fp );
                return EXIT_FAILURE;
            }
            fseek ( fp, (long) body_offset, SEEK_SET );
            if ( fread ( mzf->body, 1, body_size, fp ) != body_size ) {
                fprintf ( stderr, "Error: failed to read body of MZF #%u\n", *mzf_count + 1 );
                free ( mzf->body );
                free ( mzf );
                fclose ( fp );
                return EXIT_FAILURE;
            }
        }

        mzfs[*mzf_count] = mzf;
        ( *mzf_count )++;
        offset = body_offset + body_size;
    }

    fclose ( fp );

    if ( *mzf_count == 0 ) {
        fprintf ( stderr, "Error: no MZF files found in '%s'\n", input_file );
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


/**
 * @brief Hlavni funkce - konverze MZF/MZT -> TMZ.
 *
 * Zpracuje argumenty prikazove radky, nacte MZF/MZT soubor(y),
 * vytvori TMZ bloky. Pokud vystupni TMZ soubor jiz existuje,
 * nacte ho a prida bloky na konec. Pokud neexistuje, vytvori novy.
 *
 * @param argc Pocet argumentu.
 * @param argv Pole argumentu.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
int main ( int argc, char *argv[] ) {

    if ( argc < 2 ) {
        print_usage ( argv[0] );
        return EXIT_FAILURE;
    }

    /* vychozi parametry */
    const char *input_file = NULL;
    const char *output_file = NULL;
    en_TMZ_MACHINE machine = TMZ_MACHINE_MZ800;
    en_TMZ_PULSESET pulseset = TMZ_PULSESET_COUNT; /* auto */
    en_TMZ_FORMAT format = TMZ_FORMAT_NORMAL;
    en_CMTSPEED speed = CMTSPEED_1_1;
    int speed_set = 0;      /* priznak, ze uzivatel explicitne zadal --speed */
    const char *speed_tc = NULL;   /* --speed mimo en_CMTSPEED: poměr TurboCopy, platí jen s --profile TC-1.21 */
    int fsk_speed = -1;     /* -1 = nezadano; platne hodnoty 0-6 */
    int slow_speed = -1;    /* -1 = nezadano; platne hodnoty 0-4 */
    uint16_t pause_ms = 1000;
    const char *profile_id = NULL;   /* --profile: blok 0x46 podle registru */
    int loader_opt = -1;             /* --loader: -1 nezadáno, 1 y, 0 n */
    int gap_opt = -1;                /* --gap: -1 nezadáno, 1 rom, 0 short */
    int bd_opt = -1;                 /* --bd: -1 nezadáno, jinak rychlost Intercopy (profily IC-10.2-*) */
    int format_set = 0;              /* --format zadán (nelze s --profile) */
    int pulseset_set = 0;            /* --pulseset zadán (nelze s --profile) */
    int machine_set = 0;             /* --machine zadán (u MZT volí dosavadní cestu bloků v1.0) */
    en_MZF_NAME_ENCODING name_encoding = MZF_NAME_ASCII_EU;

    /* parsovani argumentu */
    int positional = 0;
    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp ( argv[i], "--version" ) == 0 ) {
            printf ( "mzf2tmz %s\n", MZF2TMZ_VERSION );
            return EXIT_SUCCESS;
        } else if ( strcmp ( argv[i], "--lib-versions" ) == 0 ) {
            print_lib_versions ();
            return EXIT_SUCCESS;
        } else if ( strcmp ( argv[i], "--machine" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --machine requires a value\n" );
                return EXIT_FAILURE;
            }
            if ( parse_machine ( argv[i], &machine ) != 0 ) {
                fprintf ( stderr, "Error: unknown machine type '%s'\n", argv[i] );
                return EXIT_FAILURE;
            }
            machine_set = 1;
        } else if ( strcmp ( argv[i], "--pulseset" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --pulseset requires a value\n" );
                return EXIT_FAILURE;
            }
            if ( parse_pulseset ( argv[i], &pulseset ) != 0 ) {
                fprintf ( stderr, "Error: unknown pulseset '%s'\n", argv[i] );
                return EXIT_FAILURE;
            }
            pulseset_set = 1;
        } else if ( strcmp ( argv[i], "--format" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --format requires a value\n" );
                return EXIT_FAILURE;
            }
            if ( strcasecmp ( argv[i], "sinclair" ) == 0 ) {
                fprintf ( stderr, "Error: format sinclair is not supported in MZ blocks; ZX Spectrum recordings"
                                  " use TZX blocks 0x10/0x11 (tap2tmz)\n" );
                return EXIT_FAILURE;
            }
            if ( parse_format ( argv[i], &format ) != 0 ) {
                fprintf ( stderr, "Error: unknown format '%s'\n", argv[i] );
                return EXIT_FAILURE;
            }
            format_set = 1;
        } else if ( strcmp ( argv[i], "--profile" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --profile requires a value\n" );
                return EXIT_FAILURE;
            }
            profile_id = argv[i];
        } else if ( strcmp ( argv[i], "--loader" ) == 0 ) {
            if ( ++i >= argc || ( strcmp ( argv[i], "y" ) != 0 && strcmp ( argv[i], "n" ) != 0 ) ) {
                fprintf ( stderr, "Error: --loader requires y or n\n" );
                return EXIT_FAILURE;
            }
            loader_opt = ( argv[i][0] == 'y' );
        } else if ( strcmp ( argv[i], "--bd" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --bd requires a value\n" );
                return EXIT_FAILURE;
            }
            char *endp;
            long v = strtol ( argv[i], &endp, 10 );
            /* sjednocení rychlostí všech profilů; rychlost pro zvolený profil ověří jeho sestavení */
            if ( *endp != '\0' || ( v != 600 && v != 1200 && v != 1500 && v != 2400 && v != 2800 && v != 3200
                                     && v != 3600 ) ) {
                fprintf ( stderr, "Error: --bd must be 600, 1200, 1500, 2400, 2800, 3200 or 3600\n" );
                return EXIT_FAILURE;
            }
            bd_opt = ( int ) v;
        } else if ( strcmp ( argv[i], "--gap" ) == 0 ) {
            if ( ++i >= argc || ( strcmp ( argv[i], "rom" ) != 0 && strcmp ( argv[i], "short" ) != 0 ) ) {
                fprintf ( stderr, "Error: --gap requires rom or short\n" );
                return EXIT_FAILURE;
            }
            gap_opt = ( strcmp ( argv[i], "rom" ) == 0 );
        } else if ( strcmp ( argv[i], "--speed" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --speed requires a value\n" );
                return EXIT_FAILURE;
            }
            if ( parse_speed ( argv[i], &speed ) != 0 ) {
                speed_tc = argv[i];     /* ověří se až s --profile TC-1.21 */
            }
            speed_set = 1;
        } else if ( strcmp ( argv[i], "--fsk-speed" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --fsk-speed requires a value (0-6)\n" );
                return EXIT_FAILURE;
            }
            char *endptr;
            long val = strtol ( argv[i], &endptr, 10 );
            if ( *endptr != '\0' || val < 0 || val > 6 ) {
                fprintf ( stderr, "Error: --fsk-speed must be 0-6, got '%s'\n", argv[i] );
                return EXIT_FAILURE;
            }
            fsk_speed = ( int ) val;
        } else if ( strcmp ( argv[i], "--slow-speed" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --slow-speed requires a value (0-4)\n" );
                return EXIT_FAILURE;
            }
            char *endptr;
            long val = strtol ( argv[i], &endptr, 10 );
            if ( *endptr != '\0' || val < 0 || val > 4 ) {
                fprintf ( stderr, "Error: --slow-speed must be 0-4, got '%s'\n", argv[i] );
                return EXIT_FAILURE;
            }
            slow_speed = ( int ) val;
        } else if ( strcmp ( argv[i], "--pause" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --pause requires a value\n" );
                return EXIT_FAILURE;
            }
            long val = strtol ( argv[i], NULL, 10 );
            if ( val < 0 || val > 65535 ) {
                fprintf ( stderr, "Error: pause must be 0-65535 ms\n" );
                return EXIT_FAILURE;
            }
            pause_ms = (uint16_t) val;
        } else if ( strcmp ( argv[i], "--charset" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --charset requires a value\n" );
                return EXIT_FAILURE;
            }
            if ( strcmp ( argv[i], "eu" ) == 0 ) {
                name_encoding = MZF_NAME_ASCII_EU;
            } else if ( strcmp ( argv[i], "jp" ) == 0 ) {
                name_encoding = MZF_NAME_ASCII_JP;
            } else if ( strcmp ( argv[i], "utf8-eu" ) == 0 ) {
                name_encoding = MZF_NAME_UTF8_EU;
            } else if ( strcmp ( argv[i], "utf8-jp" ) == 0 ) {
                name_encoding = MZF_NAME_UTF8_JP;
            } else {
                fprintf ( stderr, "Error: unknown charset '%s' (use: eu, jp, utf8-eu, utf8-jp)\n", argv[i] );
                return EXIT_FAILURE;
            }
        } else if ( argv[i][0] == '-' ) {
            fprintf ( stderr, "Error: unknown option '%s'\n", argv[i] );
            return EXIT_FAILURE;
        } else {
            if ( positional == 0 ) input_file = argv[i];
            else if ( positional == 1 ) output_file = argv[i];
            else {
                fprintf ( stderr, "Error: too many arguments\n" );
                return EXIT_FAILURE;
            }
            positional++;
        }
    }

    /* --profile list: výpis registru profilů */
    if ( profile_id && strcmp ( profile_id, "list" ) == 0 ) {
        printf ( "Profiles (TMZ v1.1 MZ File block 0x46):\n" );
        for ( size_t k = 0; k < tmz_profile_count (); k++ ) {
            const st_TMZ_PROFILE *pr = tmz_profile_get ( k );
            printf ( "  %-16s %-18s %s\n", pr->id, pr->confidence, pr->description );
        }
        return EXIT_SUCCESS;
    }

    if ( !input_file || !output_file ) {
        print_usage ( argv[0] );
        return EXIT_FAILURE;
    }

    /* rychlost mimo en_CMTSPEED zná jen profil TC-1.21 (poměr TurboCopy) */
    if ( speed_tc && ( !profile_id || strcmp ( profile_id, "TC-1.21" ) != 0 ) ) {
        fprintf ( stderr, "Error: unknown speed '%s'\n", speed_tc );
        return EXIT_FAILURE;
    }

    /*
     * --format cpm-tape = blok 0x46 profilem PEZIK-2.0A (TAPE.COM);
     * rychlost jen --bd 2400 nebo 3200
     */
    if ( format_set && format == TMZ_FORMAT_CPM_TAPE ) {
        if ( speed_set ) {
            fprintf ( stderr, "Error: --speed is not used with --format cpm-tape (use --bd 2400 or 3200)\n" );
            return EXIT_FAILURE;
        }
        if ( profile_id ) {
            fprintf ( stderr, "Error: --format cpm-tape is the profile PEZIK-2.0A; do not combine it with --profile\n" );
            return EXIT_FAILURE;
        }
        profile_id = "PEZIK-2.0A";
        format_set = 0;
    }

    /* --profile: kontrola kombinace voleb */
    st_TMZ_PROFILE_ARGS profile_args;
    tmz_profile_args_init ( &profile_args );
    if ( profile_id ) {
        if ( !tmz_profile_find ( profile_id ) ) {
            fprintf ( stderr, "Error: unknown profile '%s' (use --profile list)\n", profile_id );
            return EXIT_FAILURE;
        }
        if ( format_set || pulseset_set || fsk_speed >= 0 || slow_speed >= 0 ) {
            fprintf ( stderr, "Error: --format, --pulseset, --fsk-speed and --slow-speed are not used with --profile\n" );
            return EXIT_FAILURE;
        }
        const char *opt_err = profile_option_error ( profile_id, speed_set, loader_opt >= 0, gap_opt >= 0 );
        if ( opt_err ) {
            fprintf ( stderr, "Error: %s\n", opt_err );
            return EXIT_FAILURE;
        }
        if ( machine_set ) profile_args.machine = ( int ) machine;
        if ( speed_tc ) {
            /* poměr TurboCopy a:b mimo en_CMTSPEED (tmz_profile_args_parse ho uloží do tc_a, tc_b) */
            char txt[32];
            st_TMZ_PROFILE_ARGS tc;
            snprintf ( txt, sizeof ( txt ), "speed=%s", speed_tc );
            if ( strcmp ( profile_id, "TC-1.21" ) != 0 || strlen ( speed_tc ) > 8
                 || tmz_profile_args_parse ( txt, strlen ( txt ), &tc ) != EXIT_SUCCESS || !tc.tc_a ) {
                fprintf ( stderr, "Error: unknown speed '%s' (TC-1.21 also takes a TurboCopy ratio a:b, "
                                  "digits 1-9, 1/2 to 3/1)\n", speed_tc );
                return EXIT_FAILURE;
            }
            profile_args.tc_a = tc.tc_a;
            profile_args.tc_b = tc.tc_b;
        } else if ( speed_set ) {
            profile_args.speed = speed;
        }
        if ( loader_opt >= 0 ) profile_args.loader = loader_opt;
        if ( gap_opt >= 0 ) profile_args.gap_rom = gap_opt;
        if ( bd_opt >= 0 ) {
            if ( strncmp ( profile_id, "IC-10.2-", 8 ) != 0 && strcmp ( profile_id, "PEZIK-2.0A" ) != 0 ) {
                fprintf ( stderr, "Error: --bd is only valid with the IC-10.2-* and PEZIK-2.0A profiles\n" );
                return EXIT_FAILURE;
            }
            profile_args.bd = ( uint16_t ) bd_opt;
        }
        profile_args.pause_ms = pause_ms;
    } else {
        /* MZT bez voleb formátu = bloky UNICMT (níž), ty --gap používají */
        int mzt_default = has_mzt_extension ( input_file ) && !( format_set || pulseset_set || speed_set || machine_set
                                                                 || fsk_speed >= 0 || slow_speed >= 0 );
        /*
         * MZT s volbou formátu jde cestou bloků v1.0, které --gap nepoužívají;
         * hláška jmenuje volby formátu (i --machine) a radí --profile UNICMT
         */
        if ( gap_opt >= 0 && has_mzt_extension ( input_file ) && !mzt_default
             && loader_opt < 0 && bd_opt < 0 ) {
            fprintf ( stderr, "Error: --gap with an MZT input applies to UNICMT blocks; --format, --pulseset,\n"
                              "       --speed, --machine, --fsk-speed and --slow-speed select v1.0 blocks,\n"
                              "       which do not use --gap (use --profile UNICMT to combine --gap with --machine)\n" );
            return EXIT_FAILURE;
        }
        if ( loader_opt >= 0 || bd_opt >= 0 || ( gap_opt >= 0 && !mzt_default ) ) {
            fprintf ( stderr, "Error: --loader and --bd require --profile; --gap requires --profile or an MZT input\n"
                              "       without format options\n" );
            return EXIT_FAILURE;
        }
        if ( gap_opt >= 0 ) profile_args.gap_rom = gap_opt;
    }

    /* autodetekce pulzni sady z machine */
    if ( pulseset == TMZ_PULSESET_COUNT ) {
        pulseset = pulseset_from_machine ( machine );
    }

    /*
     * FSK/SLOW/DIRECT fungují na MZ-700, MZ-800 i MZ-1500, na MZ-80B to není ověřené
     */
    if ( ( format == TMZ_FORMAT_FSK || format == TMZ_FORMAT_SLOW || format == TMZ_FORMAT_DIRECT )
         && ( machine == TMZ_MACHINE_MZ80B || pulseset == TMZ_PULSESET_80B ) ) {
        fprintf ( stderr, "Warning: formats fsk, slow and direct are not verified on the MZ-80B\n" );
    }

    /* cross-validace parametru rychlosti vs. format */
    if ( format == TMZ_FORMAT_FSK ) {
        if ( speed_set ) {
            fprintf ( stderr, "Error: --speed is not valid for FSK format, use --fsk-speed 0-6\n" );
            return EXIT_FAILURE;
        }
        if ( slow_speed >= 0 ) {
            fprintf ( stderr, "Error: --slow-speed is not valid for FSK format, use --fsk-speed 0-6\n" );
            return EXIT_FAILURE;
        }
    } else if ( format == TMZ_FORMAT_SLOW ) {
        if ( speed_set ) {
            fprintf ( stderr, "Error: --speed is not valid for SLOW format, use --slow-speed 0-4\n" );
            return EXIT_FAILURE;
        }
        if ( fsk_speed >= 0 ) {
            fprintf ( stderr, "Error: --fsk-speed is not valid for SLOW format, use --slow-speed 0-4\n" );
            return EXIT_FAILURE;
        }
    } else {
        if ( fsk_speed >= 0 ) {
            fprintf ( stderr, "Error: --fsk-speed is only valid with --format fsk\n" );
            return EXIT_FAILURE;
        }
        if ( slow_speed >= 0 ) {
            fprintf ( stderr, "Error: --slow-speed is only valid with --format slow\n" );
            return EXIT_FAILURE;
        }
    }

    /* TURBO (profil TurboCopy) umí jen poměry s číslicemi 1-9 - odmítnout hned, ne až při přehrávání */
    if ( format == TMZ_FORMAT_TURBO &&
         !mzcmt_turbo_speed_is_supported ( MZCMT_TURBO_PROFILE_TURBOCOPY_V12X, speed ) ) {
        fprintf ( stderr, "Error: speed %s is not supported by TURBO format (TurboCopy ratios: 1:1, 2:1, 3:2, 7:3, 8:3, 9:7, 3:1)\n",
                  cmtspeed_get_ratiotxt ( speed ) );
        return EXIT_FAILURE;
    }

    /* resolve surove hodnoty speed_byte pro blok 0x41 */
    uint8_t speed_byte;
    if ( format == TMZ_FORMAT_FSK ) {
        speed_byte = ( uint8_t ) ( fsk_speed >= 0 ? fsk_speed : 0 );
    } else if ( format == TMZ_FORMAT_SLOW ) {
        speed_byte = ( uint8_t ) ( slow_speed >= 0 ? slow_speed : 0 );
    } else {
        speed_byte = ( uint8_t ) speed;
    }

    /* nacteni MZF/MZT souboru */
    int is_mzt = has_mzt_extension ( input_file );
    st_MZF *mzfs[4096];
    uint32_t mzf_count = 0;

    /*
     * MZT bez voleb formátu (nebo s --profile UNICMT): bloky 0x46 UNICMT s pulzy
     * z hlaviček CMTSPEED (spec 10.2). S jinými
     * volbami platí dosavadní cesta, CMTSPEED by se ale ztratily: chyba.
     */
    st_TZX_BLOCK **unicmt_blocks = NULL;
    int mzt_unicmt = 0;   /* MZT na bloky UNICMT (spec 10.2) */
    uint16_t ( *unicmt_speed )[4] = NULL;
    uint32_t unicmt_count = 0;   /* počet bloků v unicmt_blocks */
    if ( is_mzt ) {
        /* --machine určuje pulzní sadu bloků v1.0: s ním platí dosavadní cesta */
        int explicit_fmt = format_set || pulseset_set || speed_set || machine_set || fsk_speed >= 0 || slow_speed >= 0
                           || profile_id;
        mzt_unicmt = !explicit_fmt || ( profile_id && strcmp ( profile_id, "UNICMT" ) == 0 );
        if ( !mzt_unicmt ) {
            /* dosavadní cesta (load_mzf_files) toleruje i useknutý konec; CMTSPEED jen odmítnout */
            uint32_t sz = 0;
            uint8_t *raw = read_whole_file ( input_file, &sz );
            uint32_t ncs = raw ? mzt_count_cmtspeed ( raw, sz ) : 0;
            free ( raw );
            if ( ncs > 0 ) {
                fprintf ( stderr, "Error: MZT contains CMTSPEED headers; use --profile UNICMT or no format options\n" );
                return EXIT_FAILURE;
            }
        }
    }
    if ( mzt_unicmt ) {
        uint32_t mzt_size = 0;
        uint8_t *mzt_data = read_whole_file ( input_file, &mzt_size );
        st_TMZ_MZT mzt;
        uint32_t tail = 0;
        if ( !mzt_data ) {
            fprintf ( stderr, "Error: cannot open input file '%s' (or it is empty or larger than 16 MB)\n", input_file );
            return EXIT_FAILURE;
        }
        if ( tmz_mzt_parse_tail ( mzt_data, mzt_size, &mzt, &tail ) != EXIT_SUCCESS ) {
            fprintf ( stderr, "Error: '%s' is not a valid MZT file (unreadable, empty, or a body runs past the end)\n",
                      input_file );
            free ( mzt_data );
            return EXIT_FAILURE;
        }
        if ( tail ) fprintf ( stderr, "Warning: %u trailing byte(s) after the last file ignored (padding?)\n", tail );
        if ( mzt.count == mzt.cmtspeed_count ) {
            fprintf ( stderr, "Error: '%s' contains no MZF file (only CMTSPEED headers)\n", input_file );
            tmz_mzt_free ( &mzt );
            free ( mzt_data );
            return EXIT_FAILURE;
        }
        {
            int leading = 0, trailing = 0;
            uint32_t nb = 0;
            if ( !profile_id ) profile_args.pause_ms = pause_ms;
            if ( tmz_mzt_build_unicmt_blocks ( &mzt, &profile_args, &unicmt_blocks, &nb, &leading, &trailing ) != EXIT_SUCCESS
                 || nb == 0 || nb > 4096 ) {
                fprintf ( stderr, "Error: cannot build UNICMT blocks from '%s'\n", input_file );
                free_inputs ( mzfs, 0, unicmt_blocks, nb, NULL );
                tmz_mzt_free ( &mzt );
                free ( mzt_data );
                return EXIT_FAILURE;
            }
            unicmt_count = nb;
            if ( leading ) fprintf ( stderr, "Warning: CMTSPEED at the start of the MZT is ignored (UniCMT does not apply it)\n" );
            if ( trailing ) fprintf ( stderr, "Warning: CMTSPEED at the end of the MZT has no file to apply to\n" );
            unicmt_speed = calloc ( nb, sizeof ( *unicmt_speed ) );
            /* hlavičky pro souhrn (bez těl) a rychlost každého souboru (jako tmz_mzt_build_unicmt_blocks) */
            uint16_t cur[4] = TMZ_MZT_CMTSPEED_PULSES_1X;
            for ( uint32_t k = 0, f = 0; k < mzt.count && unicmt_speed; k++ ) {
                const st_TMZ_MZT_ITEM *it = &mzt.items[k];
                if ( it->is_cmtspeed ) {
                    if ( k > 0 ) memcpy ( cur, it->pulses_us, sizeof ( cur ) );
                    continue;
                }
                st_MZF *m = calloc ( 1, sizeof ( st_MZF ) );
                if ( !m ) break;
                memcpy ( &m->header, it->header, sizeof ( m->header ) );
                m->header.fsize = endianity_bswap16_LE ( m->header.fsize );
                m->header.fstrt = endianity_bswap16_LE ( m->header.fstrt );
                m->header.fexec = endianity_bswap16_LE ( m->header.fexec );
                memcpy ( unicmt_speed[f], cur, sizeof ( unicmt_speed[f] ) );
                mzfs[f++] = m;
                mzf_count = f;
            }
            if ( !unicmt_speed || mzf_count != nb ) {
                fprintf ( stderr, "Error: memory allocation failed\n" );
                free_inputs ( mzfs, mzf_count, unicmt_blocks, unicmt_count, unicmt_speed );
                tmz_mzt_free ( &mzt );
                free ( mzt_data );
                return EXIT_FAILURE;
            }
            if ( !profile_id ) profile_id = "UNICMT";
        }
        tmz_mzt_free ( &mzt );
        free ( mzt_data );
    }

    if ( !unicmt_blocks && load_mzf_files ( input_file, is_mzt, mzfs, 4096, &mzf_count ) != EXIT_SUCCESS ) {
        return EXIT_FAILURE;
    }

    /* nacteni existujiciho TMZ souboru nebo vytvoreni noveho */
    st_TZX_FILE *tmz_file = NULL;
    uint32_t old_block_count = 0;

    {
        st_HANDLER exist_handler;
        st_DRIVER exist_driver;
        generic_driver_file_init ( &exist_driver );
        st_HANDLER *h_exist = generic_driver_open_file ( &exist_handler, &exist_driver,
                                                          (char*) output_file, FILE_DRIVER_OPMODE_RO );
        if ( h_exist ) {
            en_TZX_ERROR load_err;
            tmz_file = tzx_load ( h_exist, &load_err );
            generic_driver_close ( h_exist );

            if ( !tmz_file ) {
                fprintf ( stderr, "Error: failed to load existing file '%s': %s\n",
                          output_file, tzx_error_string ( load_err ) );
                free_inputs ( mzfs, mzf_count, unicmt_blocks, unicmt_count, unicmt_speed );
                return EXIT_FAILURE;
            }
            old_block_count = tmz_file->block_count;
        }
    }

    if ( !tmz_file ) {
        tmz_file = calloc ( 1, sizeof ( st_TZX_FILE ) );
        if ( !tmz_file ) {
            fprintf ( stderr, "Error: memory allocation failed\n" );
            free_inputs ( mzfs, mzf_count, unicmt_blocks, unicmt_count, unicmt_speed );
            return EXIT_FAILURE;
        }
        tmz_header_init ( &tmz_file->header );
        tmz_file->is_tmz = true;
    }

    /* vytvoreni a pridani TMZ bloku pro kazdy MZF */
    int use_turbo = ( format != TMZ_FORMAT_NORMAL || speed_byte != ( uint8_t ) CMTSPEED_1_1 );

    for ( uint32_t idx = 0; idx < mzf_count; idx++ ) {
        st_TZX_BLOCK *block = NULL;
        if ( unicmt_blocks ) {
            block = unicmt_blocks[idx];   /* vlastnictví přechází na tmz_file */
            unicmt_blocks[idx] = NULL;
        } else if ( profile_id ) {
            en_TMZ_PROFILE_ERROR perr = TMZ_PROFILE_OK;
            block = tmz_profile_build_block ( profile_id, &profile_args, &mzfs[idx]->header,
                                              mzfs[idx]->body, mzfs[idx]->body_size, &perr );
            if ( !block ) {
                fprintf ( stderr, "Error: profile %s: %s\n", profile_id, tmz_profile_error_string ( perr ) );
            }
        } else {
            block = create_block_for_mzf ( mzfs[idx], machine, pulseset, format, speed_byte, pause_ms );
        }
        if ( !block ) {
            fprintf ( stderr, "Error: failed to create TMZ block from MZF #%u\n", idx + 1 );
            free_inputs ( mzfs, mzf_count, unicmt_blocks, unicmt_count, unicmt_speed );
            tzx_free ( tmz_file );
            return EXIT_FAILURE;
        }

        en_TZX_ERROR tmz_err = tzx_file_append_block ( tmz_file, block );
        if ( tmz_err != TZX_OK ) {
            fprintf ( stderr, "Error: failed to append block: %s\n", tzx_error_string ( tmz_err ) );
            tmz_block_free ( block );
            free_inputs ( mzfs, mzf_count, unicmt_blocks, unicmt_count, unicmt_speed );
            tzx_free ( tmz_file );
            return EXIT_FAILURE;
        }
        free ( block ); /* wrapper uvolnit, data vlastni tmz_file */
    }

    /* blok 0x46 vyžaduje verzi formátu 1.1 (spec kap. 2), i u existujícího souboru 1.0 */
    if ( profile_id && tmz_file->header.ver_major == 1 && tmz_file->header.ver_minor < 1 ) {
        tmz_file->header.ver_minor = 1;
    }

    /* zapis vystupniho souboru */
    st_HANDLER out_handler;
    st_DRIVER out_driver;
    generic_driver_file_init ( &out_driver );

    st_HANDLER *h_out = generic_driver_open_file ( &out_handler, &out_driver,
                                                    (char*) output_file, FILE_DRIVER_OPMODE_W );
    if ( !h_out ) {
        fprintf ( stderr, "Error: cannot create output file '%s'\n", output_file );
        free_inputs ( mzfs, mzf_count, unicmt_blocks, unicmt_count, unicmt_speed );
        tzx_free ( tmz_file );
        return EXIT_FAILURE;
    }

    en_TZX_ERROR save_err = tzx_save ( h_out, tmz_file );
    generic_driver_close ( h_out );

    if ( save_err != TZX_OK ) {
        fprintf ( stderr, "Error: failed to write TMZ file: %s\n", tzx_error_string ( save_err ) );
        free_inputs ( mzfs, mzf_count, unicmt_blocks, unicmt_count, unicmt_speed );
        tzx_free ( tmz_file );
        return EXIT_FAILURE;
    }

    /* vypis souhrnu */
    if ( old_block_count > 0 ) {
        printf ( "Appended to: %s (%u -> %u blocks)\n\n", output_file,
                 old_block_count, tmz_file->block_count );
    } else {
        printf ( "Converted: %s -> %s\n\n", input_file, output_file );
    }

    for ( uint32_t idx = 0; idx < mzf_count; idx++ ) {
        char fname[MZF_FNAME_UTF8_BUF_SIZE];
        mzf_tools_get_fname_ex ( &mzfs[idx]->header, fname, sizeof ( fname ), name_encoding );

        printf ( "  [%u] \"%s\"  type=0x%02X (%s)  %u bytes  0x%04X-0x%04X",
                 old_block_count + idx, fname,
                 mzfs[idx]->header.ftype, mzf_ftype_name ( mzfs[idx]->header.ftype ),
                 mzfs[idx]->header.fsize,
                 mzfs[idx]->header.fstrt, mzfs[idx]->header.fexec );

        if ( unicmt_blocks ) {
            printf ( "  -> 0x46 UNICMT %s\n", unicmt_speed_name ( unicmt_speed[idx] ) );
        } else if ( profile_id ) {
            printf ( "  -> 0x46 %s\n", profile_id );
        } else if ( use_turbo ) {
            if ( format == TMZ_FORMAT_FSK ) {
                printf ( "  -> 0x41 %s level %d\n", format_name ( format ), speed_byte );
            } else if ( format == TMZ_FORMAT_SLOW ) {
                printf ( "  -> 0x41 %s level %d\n", format_name ( format ), speed_byte );
            } else if ( format == TMZ_FORMAT_FASTIPL ) {
                printf ( "  -> 0x41 %s %u Bd\n", format_name ( format ),
                         cmtspeed_get_bdspeed ( speed, 1200 ) );
            } else {
                printf ( "  -> 0x41 %s %s\n", format_name ( format ), g_cmtspeed_ratio[speed] );
            }
        } else {
            printf ( "  -> 0x40\n" );
        }
    }

    if ( profile_id ) {
        printf ( "\n  Profile  : %s\n", profile_id );
        if ( machine_set ) printf ( "  Machine  : %s\n", machine_name ( machine ) );
        printf ( "  Pause    : %u ms\n", pause_ms );
        free_inputs ( mzfs, mzf_count, unicmt_blocks, unicmt_count, unicmt_speed );
        tzx_free ( tmz_file );
        return EXIT_SUCCESS;
    }
    printf ( "\n  Machine  : %s\n", machine_name ( machine ) );
    printf ( "  Pulseset : %s\n", pulseset_name ( pulseset ) );
    printf ( "  Format   : %s\n", format_name ( format ) );
    if ( format == TMZ_FORMAT_FSK ) {
        printf ( "  Speed    : FSK level %d (0=slowest, 6=fastest)\n", speed_byte );
    } else if ( format == TMZ_FORMAT_SLOW ) {
        printf ( "  Speed    : SLOW level %d (0=slowest, 4=fastest)\n", speed_byte );
    } else if ( format == TMZ_FORMAT_FASTIPL ) {
        char speedtxt[64];
        cmtspeed_get_speedtxt ( speedtxt, sizeof ( speedtxt ), speed, 1200 );
        printf ( "  Speed    : %s\n", speedtxt );
    } else {
        printf ( "  Speed    : %s\n", g_cmtspeed_ratio[speed] );
    }
    printf ( "  Pause    : %u ms\n", pause_ms );

    /* uklid */
    for ( uint32_t j = 0; j < mzf_count; j++ ) mzf_free ( mzfs[j] );
    tzx_free ( tmz_file );

    return EXIT_SUCCESS;
}
