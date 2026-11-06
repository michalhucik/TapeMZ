/**
 * @file   dat2bsd.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.1.0
 * @brief  Import binárního souboru do TMZ jako datový soubor BASICu (blok 0x46, BASIC-1Z016).
 *
 * Načte libovolný binární soubor, rozřeže ho na kusy po 256 B
 * s rostoucími ID (0x0000, 0x0001, ...); poslední kus má ID 0xFFFF
 * a nese zbytek dat (jako BASIC 1Z-016). Sestaví MZF hlavičku
 * (ftype 0x04 nebo 0x03, fsize/fstrt/fexec 0) a vytvoří TMZ soubor
 * s blokem 0x46 profilu BASIC-1Z016 (časování BASICu 1Z-016 změřené
 * v emulátoru).
 *
 * @par Použití:
 * @code
 *   dat2bsd input.dat output.tmz [volby]
 * @endcode
 *
 * @par Volby:
 * - --name <název>      : název souboru v MZF hlavičce (výchozí: odvozeno ze vstupu)
 * - --ftype <typ>       : brd (0x04, zapisuje WOPEN# BASICu 1Z-016) nebo bsd (0x03) (výchozí: brd)
 * - --machine <typ>     : generic, mz700, mz800, mz1500, mz80b (výchozí: mz800), jen do PROFILE_ARGS
 * - --pause <ms>        : pauza za posledním kusem v ms (výchozí: 1000)
 * - --version           : zobrazit verzi programu
 * - --lib-versions      : zobrazit verze knihoven
 *
 * Volby --speed a --pulseset (v1.0) jsou chyba: profil BASIC-1Z016 má
 * pevné časování. Nejvýš 254 kusů
 * (65 024 B dat, blok 0x46 má nejvýš 255 částí).
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
#include "libs/tmz/tmz_profile.h"
#include "libs/tzx/tzx.h"
#include "libs/mzf/mzf.h"
#include "libs/mzf/mzf_tools.h"
#include "libs/endianity/endianity.h"
#include "libs/cmtspeed/cmtspeed.h"


/** @brief Verze programu dat2bsd. */
#define DAT2BSD_VERSION "1.1.0"


/**
 * @brief Nejvíc kusů včetně posledního (ID 0xFFFF).
 *
 * Blok 0x46 má nejvýš 255 částí: hlavička a 254 kusů (profil BASIC-1Z016).
 */
#define MAX_CHUNKS  254


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
 * @brief Extrahuje bazovy nazev souboru z cesty (bez adresare a pripony).
 *
 * Z cesty "path/to/file.dat" vrati "file". Vysledek se zapise do bufferu
 * o velikosti buf_size.
 *
 * @param path Vstupni cesta k souboru.
 * @param[out] buf Vystupni buffer pro bazovy nazev.
 * @param buf_size Velikost vystupniho bufferu.
 *
 * @pre path != NULL, buf != NULL, buf_size > 0.
 * @post buf obsahuje nulou ukonceny retezec s bazovym nazvem.
 */
static void extract_basename ( const char *path, char *buf, size_t buf_size ) {
    const char *name = path;
    const char *p;
    for ( p = path; *p; p++ ) {
        if ( *p == '/' || *p == '\\' ) name = p + 1;
    }
    strncpy ( buf, name, buf_size - 1 );
    buf[buf_size - 1] = '\0';
    char *dot = strrchr ( buf, '.' );
    if ( dot && dot != buf ) {
        *dot = '\0';
    }
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
    fprintf ( stderr, "Usage: %s <input.dat> <output.tmz> [options]\n\n", prog_name );
    fprintf ( stderr, "Imports a binary file into TMZ as a BASIC data file (block 0x46,\n" );
    fprintf ( stderr, "profile BASIC-1Z016: BASIC 1Z-016 timing measured in the emulator).\n\n" );
    fprintf ( stderr, "The input file is split into 256-byte chunks with sequential IDs\n" );
    fprintf ( stderr, "(0x0000, 0x0001, ...); the last chunk has ID 0xFFFF and carries the\n" );
    fprintf ( stderr, "rest of the data. At most 254 chunks (65024 bytes).\n\n" );
    fprintf ( stderr, "Options:\n" );
    fprintf ( stderr, "  --name <name>       Filename in MZF header (default: from input)\n" );
    fprintf ( stderr, "  --ftype <type>      File type: brd (0x04, written by BASIC WOPEN#) or\n" );
    fprintf ( stderr, "                      bsd (0x03) (default: brd)\n" );
    fprintf ( stderr, "  --machine <type>    Target machine: generic, mz700, mz800, mz1500, mz80b\n" );
    fprintf ( stderr, "                      (default: mz800, stored in PROFILE_ARGS only)\n" );
    fprintf ( stderr, "  --pause <ms>        Pause after the last chunk in ms (default: 1000)\n" );
    fprintf ( stderr, "  --version           Show program version\n" );
    fprintf ( stderr, "  --lib-versions      Show library versions\n" );
}


/**
 * @brief Nacte cely binarni soubor do pameti.
 *
 * Otevre soubor, zjisti velikost, alokuje buffer a nacte data.
 *
 * @param path Cesta k souboru.
 * @param[out] out_data Vystupni ukazatel na alokovana data.
 * @param[out] out_size Vystupni velikost dat v bajtech.
 * @return 0 pri uspechu, -1 pri chybe.
 *
 * @post Pri uspechu volajici vlastni *out_data a musi ho uvolnit pres free().
 */
static int load_binary_file ( const char *path, uint8_t **out_data, size_t *out_size ) {
    FILE *fp = fopen ( path, "rb" );
    if ( !fp ) return -1;

    fseek ( fp, 0, SEEK_END );
    long file_size = ftell ( fp );
    fseek ( fp, 0, SEEK_SET );

    if ( file_size < 0 ) {
        fclose ( fp );
        return -1;
    }

    if ( file_size == 0 ) {
        fclose ( fp );
        *out_data = NULL;
        *out_size = 0;
        return 0;
    }

    uint8_t *data = malloc ( (size_t) file_size );
    if ( !data ) {
        fclose ( fp );
        return -1;
    }

    size_t read = fread ( data, 1, (size_t) file_size, fp );
    fclose ( fp );

    if ( read != (size_t) file_size ) {
        free ( data );
        return -1;
    }

    *out_data = data;
    *out_size = (size_t) file_size;
    return 0;
}


/**
 * @brief Sestavi pole BSD/BRD chunku z raw dat.
 *
 * Rozreze vstupni data na 256B bloky s 2B chunk ID (LE).
 * Poslední kus dostane ID=0xFFFF a nese poslední porci dat (doplněnou
 * nulami). Tak zapisuje BASIC 1Z-016; BSD dekodér
 * jeho data zahrnuje do těla.
 *
 * Pokud data_size == 0, vytvoří se jediný kus (ID 0xFFFF, nulová data).
 *
 * @param data Vstupní binární data (může být NULL při data_size==0).
 * @param data_size Velikost vstupních dat v bajtech.
 * @param[out] out_chunks Výstupní pole kusů (chunk_count * 258 B).
 * @param[out] out_chunk_count Počet kusů (včetně posledního).
 * @return 0 při úspěchu, -1 při chybě (alokace, víc než MAX_CHUNKS kusů; hlášku vypíše).
 *
 * @post Pri uspechu volajici vlastni *out_chunks a musi ho uvolnit pres free().
 */
static int build_chunks ( const uint8_t *data, size_t data_size,
                           uint8_t **out_chunks, uint16_t *out_chunk_count ) {

    /* pocet chunku: minimalne 1 (terminacni) */
    uint32_t total_chunks;
    if ( data_size == 0 ) {
        total_chunks = 1;
    } else {
        total_chunks = (uint32_t) ( ( data_size + TMZ_BASIC_CHUNK_DATA_SIZE - 1 ) / TMZ_BASIC_CHUNK_DATA_SIZE );
    }

    if ( total_chunks > MAX_CHUNKS ) {
        fprintf ( stderr, "Error: input file too large (%zu bytes, max %u chunks * 256 = %u bytes, profile BASIC-1Z016)\n",
                  data_size, (unsigned) MAX_CHUNKS, (unsigned) ( MAX_CHUNKS * TMZ_BASIC_CHUNK_DATA_SIZE ) );
        return -1;
    }

    size_t chunks_bytes = (size_t) total_chunks * TMZ_BASIC_CHUNK_SIZE;

    uint8_t *chunks = calloc ( 1, chunks_bytes );
    if ( !chunks ) return -1;

    for ( uint32_t i = 0; i < total_chunks; i++ ) {
        uint8_t *chunk = chunks + (size_t) i * TMZ_BASIC_CHUNK_SIZE;

        /* posledni chunk = terminacni (ID=0xFFFF), ostatni sekvencni */
        uint16_t chunk_id = ( i == total_chunks - 1 )
                            ? TMZ_BASIC_LAST_CHUNK_ID
                            : (uint16_t) i;
        uint16_t id_le = endianity_bswap16_LE ( chunk_id );
        memcpy ( chunk, &id_le, 2 );

        /* chunk data (256B, zbytek nulovy z calloc) */
        if ( data && data_size > 0 ) {
            size_t offset = (size_t) i * TMZ_BASIC_CHUNK_DATA_SIZE;
            if ( offset < data_size ) {
                size_t remaining = data_size - offset;
                size_t copy_size = ( remaining < TMZ_BASIC_CHUNK_DATA_SIZE )
                                   ? remaining : TMZ_BASIC_CHUNK_DATA_SIZE;
                memcpy ( chunk + 2, data + offset, copy_size );
            }
        }
    }

    *out_chunks = chunks;
    *out_chunk_count = (uint16_t) total_chunks;
    return 0;
}


/**
 * @brief Hlavní funkce - import binárního souboru do TMZ jako blok 0x46 BASIC-1Z016.
 *
 * Zpracuje argumenty příkazové řádky, načte binární soubor,
 * rozřeže ho na kusy, sestaví MZF hlavičku a vytvoří TMZ soubor
 * s blokem 0x46 (tmz_profile_build_block, profil BASIC-1Z016).
 *
 * @param argc Pocet argumentu.
 * @param argv Pole argumentu.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
int main ( int argc, char *argv[] ) {

    /* kontrola --version a --lib-versions pred kontrolou minimalniho poctu argumentu */
    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp ( argv[i], "--version" ) == 0 ) {
            printf ( "dat2bsd %s\n", DAT2BSD_VERSION );
            return EXIT_SUCCESS;
        }
        if ( strcmp ( argv[i], "--lib-versions" ) == 0 ) {
            print_lib_versions ();
            return EXIT_SUCCESS;
        }
    }

    if ( argc < 3 ) {
        print_usage ( argv[0] );
        return EXIT_FAILURE;
    }

    /* vychozi parametry */
    const char *input_file = NULL;
    const char *output_file = NULL;
    const char *mzf_name = NULL;
    uint8_t ftype = MZF_FTYPE_BRD;   /* WOPEN# BASICu 1Z-016 zapisuje 04h */
    en_TMZ_MACHINE machine = TMZ_MACHINE_MZ800;
    uint16_t pause_ms = 1000;

    /* parsovani argumentu */
    int positional = 0;
    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp ( argv[i], "--name" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --name requires a value\n" );
                return EXIT_FAILURE;
            }
            mzf_name = argv[i];
        } else if ( strcmp ( argv[i], "--ftype" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --ftype requires a value\n" );
                return EXIT_FAILURE;
            }
            if ( strcasecmp ( argv[i], "bsd" ) == 0 ) {
                ftype = MZF_FTYPE_BSD;
            } else if ( strcasecmp ( argv[i], "brd" ) == 0 ) {
                ftype = MZF_FTYPE_BRD;
            } else {
                fprintf ( stderr, "Error: unknown ftype '%s' (use 'bsd' or 'brd')\n", argv[i] );
                return EXIT_FAILURE;
            }
        } else if ( strcmp ( argv[i], "--machine" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --machine requires a value\n" );
                return EXIT_FAILURE;
            }
            if ( parse_machine ( argv[i], &machine ) != 0 ) {
                fprintf ( stderr, "Error: unknown machine type '%s'\n", argv[i] );
                return EXIT_FAILURE;
            }
        } else if ( strcmp ( argv[i], "--pulseset" ) == 0 || strcmp ( argv[i], "--speed" ) == 0 ) {
            /* profil BASIC-1Z016 má pevné časování */
            fprintf ( stderr, "Error: %s is not supported: BSD is written with profile BASIC-1Z016 "
                      "(BASIC 1Z-016 timing)\n", argv[i] );
            return EXIT_FAILURE;
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


    /* nazev souboru pro MZF hlavicku - odvozeni ze vstupu pokud nezadan */
    char auto_name[MZF_FILE_NAME_LENGTH + 1];
    if ( !mzf_name ) {
        extract_basename ( input_file, auto_name, sizeof ( auto_name ) );
        mzf_name = auto_name;
    }

    /* nacteni vstupniho binarniho souboru */
    uint8_t *data = NULL;
    size_t data_size = 0;

    if ( load_binary_file ( input_file, &data, &data_size ) != 0 ) {
        fprintf ( stderr, "Error: cannot read input file '%s'\n", input_file );
        return EXIT_FAILURE;
    }

    /* sestaveni chunku */
    uint8_t *chunks = NULL;
    uint16_t chunk_count = 0;

    if ( build_chunks ( data, data_size, &chunks, &chunk_count ) != 0 ) {
        free ( data );
        return EXIT_FAILURE;
    }

    /* sestaveni MZF hlavicky (fsize=0, fstrt=0, fexec=0) */
    st_MZF_HEADER mzf_header;
    memset ( &mzf_header, 0, sizeof ( mzf_header ) );
    mzf_header.ftype = ftype;
    mzf_tools_set_fname ( &mzf_header, mzf_name );

    /* blok 0x46 profilu BASIC-1Z016; tmz_header_init dává verzi souboru 1.1 */
    st_TMZ_PROFILE_ARGS pa;
    tmz_profile_args_init ( &pa );
    pa.pause_ms = pause_ms;
    pa.machine = ( int ) machine;
    en_TMZ_PROFILE_ERROR pe;
    st_TZX_BLOCK *block = tmz_profile_build_block ( "BASIC-1Z016", &pa, &mzf_header, chunks,
                                                    ( uint32_t ) chunk_count * TMZ_BASIC_CHUNK_SIZE, &pe );

    free ( chunks );
    free ( data );

    if ( !block ) {
        fprintf ( stderr, "Error: profile BASIC-1Z016: %s\n", tmz_profile_error_string ( pe ) );
        return EXIT_FAILURE;
    }

    /* sestaveni TMZ souboru */
    st_TZX_FILE tmz_file;
    tmz_header_init ( &tmz_file.header );
    tmz_file.is_tmz = true;
    tmz_file.block_count = 1;
    tmz_file.blocks = block;

    /* zapis vystupniho souboru */
    st_HANDLER out_handler;
    st_DRIVER out_driver;
    generic_driver_file_init ( &out_driver );

    st_HANDLER *h_out = generic_driver_open_file ( &out_handler, &out_driver, (char*) output_file, FILE_DRIVER_OPMODE_W );
    if ( !h_out ) {
        fprintf ( stderr, "Error: cannot create output file '%s'\n", output_file );
        tmz_block_free ( block );
        return EXIT_FAILURE;
    }

    en_TZX_ERROR tmz_err = tzx_save ( h_out, &tmz_file );
    generic_driver_close ( h_out );

    if ( tmz_err != TZX_OK ) {
        fprintf ( stderr, "Error: failed to write TMZ file: %s\n", tzx_error_string ( tmz_err ) );
        tmz_block_free ( block );
        return EXIT_FAILURE;
    }

    /* souhrn */
    printf ( "Imported: %s -> %s\n\n", input_file, output_file );
    printf ( "  Filename   : \"%s\"\n", mzf_name );
    printf ( "  File type  : 0x%02X (%s)\n", ftype, ( ftype == MZF_FTYPE_BSD ) ? "BSD" : "BRD" );
    printf ( "  Input size : %zu bytes\n", data_size );
    printf ( "  Chunks     : %u (last ID 0xFFFF)\n", chunk_count );
    printf ( "  Machine    : %s\n", machine_name ( machine ) );
    printf ( "  Pause      : %u ms\n", pause_ms );
    printf ( "  Block      : 0x46 (MZ File, profile BASIC-1Z016)\n" );
    printf ( "  TMZ size   : %u bytes\n", (unsigned) ( sizeof ( st_TZX_HEADER ) + 1 + block->length ) );

    tmz_block_free ( block );
    return EXIT_SUCCESS;
}
