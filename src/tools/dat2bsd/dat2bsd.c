/**
 * @file   dat2bsd.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.1
 * @brief  Import binarniho souboru do TMZ jako BSD/BRD blok 0x45.
 *
 * Nacte libovolny binarni soubor, rozreze ho na 256B chunky
 * s inkrementujicimi ID (0x0000, 0x0001, ...) a terminacnim
 * chunkem 0xFFFF, sestavi MZF hlavicku (ftype=0x03/0x04, fsize=0)
 * a vytvori TMZ soubor s blokem 0x45 (MZ BASIC Data).
 *
 * @par Pouziti:
 * @code
 *   dat2bsd input.dat output.tmz [volby]
 * @endcode
 *
 * @par Volby:
 * - --name <nazev>      : nazev souboru v MZF hlavicce (vychozi: odvozeno ze vstupu)
 * - --ftype <typ>       : bsd (0x03) nebo brd (0x04) (vychozi: bsd)
 * - --machine <typ>     : generic, mz700, mz800, mz1500, mz80b (vychozi: mz800)
 * - --pulseset <sada>   : 700, 800, 80b, auto (vychozi: auto z machine)
 * - --speed <rychlost>  : 1:1, 2:1, 3:1, 3:2, 7:3, 8:3, 9:7, 25:14 (vychozi: 1:1)
 * - --pause <ms>        : pauza po bloku v ms (vychozi: 1000)
 * - --version           : zobrazit verzi programu
 * - --lib-versions      : zobrazit verze knihoven
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
#include "libs/endianity/endianity.h"
#include "libs/cmtspeed/cmtspeed.h"


/** @brief Verze programu dat2bsd. */
#define DAT2BSD_VERSION "1.0.1"


/**
 * @brief Maximalni pocet datovych chunku (bez terminacniho).
 *
 * BSD/BRD chunky maji 16-bit ID (0x0000..0xFFFE), coz dava max 65535 chunku.
 * Omezujeme na 65534, protoze 0xFFFF je rezervovano pro terminacni chunk.
 */
#define MAX_DATA_CHUNKS  65534


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
 * @brief Naparsuje retezec na hodnotu en_CMTSPEED.
 *
 * Rozpoznava: "1:1", "2:1", "2:1cpm", "3:1", "3:2", "7:3", "8:3", "9:7", "25:14".
 * Porovnani je case-insensitive.
 *
 * @param str Vstupni retezec.
 * @param[out] speed Vystupni hodnota.
 * @return 0 pri uspechu, -1 pokud retezec neodpovida zadne rychlosti.
 */
static int parse_speed ( const char *str, en_CMTSPEED *speed ) {
    if ( strcmp ( str, "1:1" ) == 0 )    { *speed = CMTSPEED_1_1;    return 0; }
    if ( strcmp ( str, "2:1" ) == 0 )    { *speed = CMTSPEED_2_1;    return 0; }
    if ( strcasecmp ( str, "2:1cpm" ) == 0 ) { *speed = CMTSPEED_2_1_CPM; return 0; }
    if ( strcmp ( str, "3:1" ) == 0 )    { *speed = CMTSPEED_3_1;    return 0; }
    if ( strcmp ( str, "3:2" ) == 0 )    { *speed = CMTSPEED_3_2;    return 0; }
    if ( strcmp ( str, "7:3" ) == 0 )    { *speed = CMTSPEED_7_3;    return 0; }
    if ( strcmp ( str, "8:3" ) == 0 )    { *speed = CMTSPEED_8_3;    return 0; }
    if ( strcmp ( str, "9:7" ) == 0 )    { *speed = CMTSPEED_9_7;    return 0; }
    if ( strcmp ( str, "25:14" ) == 0 )  { *speed = CMTSPEED_25_14;  return 0; }
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
    fprintf ( stderr, "Imports a binary file into TMZ as BSD/BRD block 0x45.\n\n" );
    fprintf ( stderr, "The input file is split into 256-byte chunks with sequential IDs\n" );
    fprintf ( stderr, "(0x0000, 0x0001, ...) and a termination chunk (ID=0xFFFF).\n\n" );
    fprintf ( stderr, "Options:\n" );
    fprintf ( stderr, "  --name <name>       Filename in MZF header (default: from input)\n" );
    fprintf ( stderr, "  --ftype <type>      File type: bsd (0x03) or brd (0x04) (default: bsd)\n" );
    fprintf ( stderr, "  --machine <type>    Target machine: generic, mz700, mz800, mz1500, mz80b\n" );
    fprintf ( stderr, "                      (default: mz800)\n" );
    fprintf ( stderr, "  --pulseset <set>    Pulse set: 700, 800, 80b, auto (default: auto)\n" );
    fprintf ( stderr, "  --speed <ratio>     Speed ratio: 1:1, 2:1, 3:1, 3:2, 7:3, 8:3, 9:7, 25:14\n" );
    fprintf ( stderr, "                      (default: 1:1)\n" );
    fprintf ( stderr, "  --pause <ms>        Pause after block in ms (default: 1000)\n" );
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
 * Posledni chunk dostane ID=0xFFFF (terminacni marker) a nese
 * posledni porci dat (pripadne doplnenou nulami). To odpovida
 * chovani realneho hardwaru (MZ-800 BASIC), kde terminacni chunk
 * take nese data a BSD dekoder je zahrnuje do body.
 *
 * Pokud data_size == 0, vytvori se jediny chunk (terminacni,
 * nulova data).
 *
 * @param data Vstupni binarni data (muze byt NULL pri data_size==0).
 * @param data_size Velikost vstupnich dat v bajtech.
 * @param[out] out_chunks Vystupni pole chunku (chunk_count * 258B).
 * @param[out] out_chunk_count Pocet chunku (vcetne terminacniho).
 * @return 0 pri uspechu, -1 pri chybe (alokace, prilis velky soubor).
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

    if ( total_chunks > MAX_DATA_CHUNKS + 1 ) {
        fprintf ( stderr, "Error: input file too large (%zu bytes, max %u chunks * 256 = %u bytes)\n",
                  data_size, (unsigned) ( MAX_DATA_CHUNKS + 1 ),
                  (unsigned) ( ( MAX_DATA_CHUNKS + 1 ) * TMZ_BASIC_CHUNK_DATA_SIZE ) );
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
 * @brief Hlavni funkce - import binarniho souboru do TMZ jako blok 0x45.
 *
 * Zpracuje argumenty prikazove radky, nacte binarni soubor,
 * rozreze ho na BSD/BRD chunky, sestavi MZF hlavicku
 * a vytvori TMZ soubor s blokem 0x45.
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
    uint8_t ftype = MZF_FTYPE_BSD;
    en_TMZ_MACHINE machine = TMZ_MACHINE_MZ800;
    en_TMZ_PULSESET pulseset = TMZ_PULSESET_COUNT; /* auto */
    en_CMTSPEED speed = CMTSPEED_1_1;
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
        } else if ( strcmp ( argv[i], "--pulseset" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --pulseset requires a value\n" );
                return EXIT_FAILURE;
            }
            if ( parse_pulseset ( argv[i], &pulseset ) != 0 ) {
                fprintf ( stderr, "Error: unknown pulseset '%s'\n", argv[i] );
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

    /* autodetekce pulzni sady z machine */
    if ( pulseset == TMZ_PULSESET_COUNT ) {
        pulseset = pulseset_from_machine ( machine );
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

    /* vytvoreni TMZ bloku 0x45 */
    st_TZX_BLOCK *block = tmz_block_create_mz_basic_data (
        machine, pulseset, pause_ms, &mzf_header, chunks, chunk_count
    );

    free ( chunks );
    free ( data );

    if ( !block ) {
        fprintf ( stderr, "Error: failed to create TMZ block 0x45\n" );
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
    printf ( "  Chunks     : %u (%u data + termination)\n", chunk_count, chunk_count - 1 );
    printf ( "  Machine    : %s\n", machine_name ( machine ) );
    printf ( "  Pulseset   : %s\n", pulseset_name ( pulseset ) );
    printf ( "  Speed      : %s\n", g_cmtspeed_ratio[speed] );
    printf ( "  Pause      : %u ms\n", pause_ms );
    printf ( "  Block      : 0x45 (MZ BASIC Data)\n" );
    printf ( "  TMZ size   : %u bytes\n", (unsigned) ( sizeof ( st_TZX_HEADER ) + 1 + block->length ) );

    tmz_block_free ( block );
    return EXIT_SUCCESS;
}
