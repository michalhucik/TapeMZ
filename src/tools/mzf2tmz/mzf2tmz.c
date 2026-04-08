/**
 * @file   mzf2tmz.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.2.0
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
 * - --format <format>      : normal, turbo, fastipl, sinclair, fsk, slow, direct, cpm-tape
 *                            (vychozi: normal)
 * - --speed <rychlost>     : 1:1, 2:1, 2:1cpm, 3:1, 3:2, 7:3, 8:3, 9:7, 25:14
 *                            (vychozi: 1:1; neplatny pro FSK a SLOW formaty)
 * - --fsk-speed <level>    : rychlostni uroven FSK koderu 0-6 (0=nejpomalejsi,
 *                            6=nejrychlejsi; vychozi: 0; pouze pro --format fsk)
 * - --slow-speed <level>   : rychlostni uroven SLOW koderu 0-4 (0=nejpomalejsi,
 *                            4=nejrychlejsi; vychozi: 0; pouze pro --format slow)
 * - --pause <ms>           : pauza po bloku v ms (vychozi: 1000)
 * - --name-encoding <enc>  : kodovani nazvu: ascii, utf8-eu, utf8-jp (vychozi: ascii)
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
#include "libs/endianity/endianity.h"

/** @brief Verze programu mzf2tmz (z @version v hlavicce souboru). */
#define MZF2TMZ_VERSION  "1.2.0"


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
        case 0x04: return "BRD (BASIC read-after-run)";
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
 * Rozpoznava: "normal", "turbo", "fastipl", "sinclair", "fsk",
 * "slow", "direct", "cpm-tape".
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
    if ( strcasecmp ( str, "sinclair" ) == 0 ) { *format = TMZ_FORMAT_SINCLAIR; return 0; }
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
    fprintf ( stderr, "  --format <fmt>      Recording format: normal, turbo, fastipl, sinclair,\n" );
    fprintf ( stderr, "                      fsk, slow, direct, cpm-tape (default: normal)\n" );
    fprintf ( stderr, "  --speed <ratio>     Speed ratio: 1:1, 2:1, 2:1cpm, 3:1, 3:2, 7:3,\n" );
    fprintf ( stderr, "                      8:3, 9:7, 25:14 (default: 1:1)\n" );
    fprintf ( stderr, "                      Not valid for FSK/SLOW formats (use --fsk-speed/--slow-speed)\n" );
    fprintf ( stderr, "  --fsk-speed <level> FSK speed level: 0-6 (0=slowest, 6=fastest, default: 0)\n" );
    fprintf ( stderr, "                      Only valid with --format fsk\n" );
    fprintf ( stderr, "  --slow-speed <level> SLOW speed level: 0-4 (0=slowest, 4=fastest, default: 0)\n" );
    fprintf ( stderr, "                      Only valid with --format slow\n" );
    fprintf ( stderr, "  --pause <ms>        Pause after block in ms (default: 1000)\n" );
    fprintf ( stderr, "  --name-encoding <enc> Filename encoding: ascii, utf8-eu, utf8-jp (default: ascii)\n" );
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
    int fsk_speed = -1;     /* -1 = nezadano; platne hodnoty 0-6 */
    int slow_speed = -1;    /* -1 = nezadano; platne hodnoty 0-4 */
    uint16_t pause_ms = 1000;
    en_MZF_NAME_ENCODING name_encoding = MZF_NAME_ASCII;

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
        } else if ( strcmp ( argv[i], "--pulseset" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --pulseset requires a value\n" );
                return EXIT_FAILURE;
            }
            if ( parse_pulseset ( argv[i], &pulseset ) != 0 ) {
                fprintf ( stderr, "Error: unknown pulseset '%s'\n", argv[i] );
                return EXIT_FAILURE;
            }
        } else if ( strcmp ( argv[i], "--format" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --format requires a value\n" );
                return EXIT_FAILURE;
            }
            if ( parse_format ( argv[i], &format ) != 0 ) {
                fprintf ( stderr, "Error: unknown format '%s'\n", argv[i] );
                return EXIT_FAILURE;
            }
        } else if ( strcmp ( argv[i], "--speed" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --speed requires a value\n" );
                return EXIT_FAILURE;
            }
            if ( parse_speed ( argv[i], &speed ) != 0 ) {
                fprintf ( stderr, "Error: unknown speed '%s'\n", argv[i] );
                return EXIT_FAILURE;
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
        } else if ( strcmp ( argv[i], "--name-encoding" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --name-encoding requires a value\n" );
                return EXIT_FAILURE;
            }
            if ( strcmp ( argv[i], "ascii" ) == 0 ) {
                name_encoding = MZF_NAME_ASCII;
            } else if ( strcmp ( argv[i], "utf8-eu" ) == 0 ) {
                name_encoding = MZF_NAME_UTF8_EU;
            } else if ( strcmp ( argv[i], "utf8-jp" ) == 0 ) {
                name_encoding = MZF_NAME_UTF8_JP;
            } else {
                fprintf ( stderr, "Error: unknown name encoding '%s' (use: ascii, utf8-eu, utf8-jp)\n", argv[i] );
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

    if ( !input_file || !output_file ) {
        print_usage ( argv[0] );
        return EXIT_FAILURE;
    }

    /* autodetekce pulzni sady z machine */
    if ( pulseset == TMZ_PULSESET_COUNT ) {
        pulseset = pulseset_from_machine ( machine );
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

    if ( load_mzf_files ( input_file, is_mzt, mzfs, 4096, &mzf_count ) != EXIT_SUCCESS ) {
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
                for ( uint32_t j = 0; j < mzf_count; j++ ) mzf_free ( mzfs[j] );
                return EXIT_FAILURE;
            }
            old_block_count = tmz_file->block_count;
        }
    }

    if ( !tmz_file ) {
        tmz_file = calloc ( 1, sizeof ( st_TZX_FILE ) );
        if ( !tmz_file ) {
            fprintf ( stderr, "Error: memory allocation failed\n" );
            for ( uint32_t j = 0; j < mzf_count; j++ ) mzf_free ( mzfs[j] );
            return EXIT_FAILURE;
        }
        tmz_header_init ( &tmz_file->header );
        tmz_file->is_tmz = true;
    }

    /* vytvoreni a pridani TMZ bloku pro kazdy MZF */
    int use_turbo = ( format != TMZ_FORMAT_NORMAL || speed_byte != ( uint8_t ) CMTSPEED_1_1 );

    for ( uint32_t idx = 0; idx < mzf_count; idx++ ) {
        st_TZX_BLOCK *block = create_block_for_mzf ( mzfs[idx], machine, pulseset,
                                                       format, speed_byte, pause_ms );
        if ( !block ) {
            fprintf ( stderr, "Error: failed to create TMZ block from MZF #%u\n", idx + 1 );
            for ( uint32_t j = 0; j < mzf_count; j++ ) mzf_free ( mzfs[j] );
            tzx_free ( tmz_file );
            return EXIT_FAILURE;
        }

        en_TZX_ERROR tmz_err = tzx_file_append_block ( tmz_file, block );
        if ( tmz_err != TZX_OK ) {
            fprintf ( stderr, "Error: failed to append block: %s\n", tzx_error_string ( tmz_err ) );
            tmz_block_free ( block );
            for ( uint32_t j = 0; j < mzf_count; j++ ) mzf_free ( mzfs[j] );
            tzx_free ( tmz_file );
            return EXIT_FAILURE;
        }
        free ( block ); /* wrapper uvolnit, data vlastni tmz_file */
    }

    /* zapis vystupniho souboru */
    st_HANDLER out_handler;
    st_DRIVER out_driver;
    generic_driver_file_init ( &out_driver );

    st_HANDLER *h_out = generic_driver_open_file ( &out_handler, &out_driver,
                                                    (char*) output_file, FILE_DRIVER_OPMODE_W );
    if ( !h_out ) {
        fprintf ( stderr, "Error: cannot create output file '%s'\n", output_file );
        for ( uint32_t j = 0; j < mzf_count; j++ ) mzf_free ( mzfs[j] );
        tzx_free ( tmz_file );
        return EXIT_FAILURE;
    }

    en_TZX_ERROR save_err = tzx_save ( h_out, tmz_file );
    generic_driver_close ( h_out );

    if ( save_err != TZX_OK ) {
        fprintf ( stderr, "Error: failed to write TMZ file: %s\n", tzx_error_string ( save_err ) );
        for ( uint32_t j = 0; j < mzf_count; j++ ) mzf_free ( mzfs[j] );
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

        if ( use_turbo ) {
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
