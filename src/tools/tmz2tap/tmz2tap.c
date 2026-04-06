/**
 * @file   tmz2tap.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Konverzni utility TMZ/TZX -> TAP (ZX Spectrum).
 *
 * Nacte TMZ nebo TZX soubor, najde vsechny TZX bloky 0x10
 * (Standard Speed Data) a extrahuje z nich data do TAP formatu.
 * TAP format: sekvence bloku, kazdy = 2B delka LE + data.
 *
 * @par Pouziti:
 * @code
 *   tmz2tap input.tzx output.tap
 * @endcode
 *
 * @par Volby:
 * - --version       : zobrazit verzi programu
 * - --lib-versions  : zobrazit verze knihoven
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
#include "libs/tzx/tzx.h"

/** @brief Verze programu tmz2tap (z @version v hlavicce souboru). */
#define TMZ2TAP_VERSION  "1.0.0"


/**
 * @brief Vrati textovy popis TAP flag bajtu.
 * @param flag Flag bajt (0x00 = header, 0xFF = data).
 * @return Staticky retezec s popisem.
 */
static const char* tap_flag_name ( uint8_t flag ) {
    if ( flag == 0x00 ) return "header";
    if ( flag == 0xFF ) return "data";
    return "custom";
}


/**
 * @brief Vypise verze vsech pouzitych knihoven na stdout.
 */
static void print_lib_versions ( void ) {
    printf ( "Library versions:\n" );
    printf ( "  tmz            %s (TMZ format v%s)\n", tmz_version (), tmz_format_version () );
    printf ( "  tzx            %s (TZX format v%s)\n", tzx_version (), tzx_format_version () );
}


/**
 * @brief Vypise napovedu programu.
 * @param prog_name Nazev spusteneho programu (argv[0]).
 */
static void print_usage ( const char *prog_name ) {
    fprintf ( stderr, "Usage: %s <input.tmz|input.tzx> <output.tap>\n\n", prog_name );
    fprintf ( stderr, "Extracts TAP blocks from TMZ/TZX file.\n" );
    fprintf ( stderr, "Only Standard Speed Data (0x10) blocks are extracted.\n\n" );
    fprintf ( stderr, "Options:\n" );
    fprintf ( stderr, "  --version             Show program version\n" );
    fprintf ( stderr, "  --lib-versions        Show library versions\n" );
}


/**
 * @brief Hlavni funkce - extrakce TAP z TMZ/TZX.
 *
 * Nacte TMZ/TZX soubor, projde vsechny bloky a kazdy blok 0x10
 * (Standard Speed Data) zapise do TAP souboru jako TAP blok
 * (2B delka LE + data).
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

    /* parsovani argumentu */
    const char *input_file = NULL;
    const char *output_file = NULL;
    int positional = 0;

    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp ( argv[i], "--version" ) == 0 ) {
            printf ( "tmz2tap %s\n", TMZ2TAP_VERSION );
            return EXIT_SUCCESS;
        } else if ( strcmp ( argv[i], "--lib-versions" ) == 0 ) {
            print_lib_versions ();
            return EXIT_SUCCESS;
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

    /* nacteni TMZ/TZX souboru */
    st_HANDLER in_handler;
    st_DRIVER in_driver;
    generic_driver_file_init ( &in_driver );

    st_HANDLER *h_in = generic_driver_open_file ( &in_handler, &in_driver,
                                                   (char*) input_file, FILE_DRIVER_OPMODE_RO );
    if ( !h_in ) {
        fprintf ( stderr, "Error: cannot open input file '%s'\n", input_file );
        return EXIT_FAILURE;
    }

    en_TZX_ERROR tmz_err;
    st_TZX_FILE *file = tzx_load ( h_in, &tmz_err );
    generic_driver_close ( h_in );

    if ( !file ) {
        fprintf ( stderr, "Error: %s\n", tzx_error_string ( tmz_err ) );
        return EXIT_FAILURE;
    }

    /* spocitat 0x10 bloky */
    uint32_t tap_count = 0;
    for ( uint32_t i = 0; i < file->block_count; i++ ) {
        if ( file->blocks[i].id == TZX_BLOCK_ID_STANDARD_SPEED ) {
            tap_count++;
        }
    }

    if ( tap_count == 0 ) {
        fprintf ( stderr, "Error: no Standard Speed Data (0x10) blocks found in '%s'\n", input_file );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    /* otevrit vystupni soubor */
    FILE *f_out = fopen ( output_file, "wb" );
    if ( !f_out ) {
        fprintf ( stderr, "Error: cannot create output file '%s'\n", output_file );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    /* extrahovat bloky 0x10 do TAP formatu */
    printf ( "Extracting from: %s (%s, v%u.%u, %u blocks)\n\n",
             input_file,
             file->is_tmz ? "TMZ" : "TZX",
             file->header.ver_major, file->header.ver_minor,
             file->block_count );

    uint32_t extracted = 0;
    uint32_t total_bytes = 0;

    for ( uint32_t i = 0; i < file->block_count; i++ ) {
        const st_TZX_BLOCK *block = &file->blocks[i];

        if ( block->id != TZX_BLOCK_ID_STANDARD_SPEED ) continue;

        /* blok 0x10: [2B pause_ms] [2B data_len] [data] */
        if ( block->length < 4 ) {
            fprintf ( stderr, "  [%3u] 0x10 - SKIPPED: block too short (%u bytes)\n",
                      i, block->length );
            continue;
        }

        uint16_t data_len = (uint16_t) block->data[2] | ( (uint16_t) block->data[3] << 8 );

        if ( 4 + (uint32_t) data_len > block->length ) {
            fprintf ( stderr, "  [%3u] 0x10 - SKIPPED: data truncated (need %u, have %u)\n",
                      i, 4 + data_len, block->length );
            continue;
        }

        /* zapsat TAP blok: 2B length LE + data */
        uint8_t len_buf[2];
        len_buf[0] = (uint8_t) ( data_len & 0xFF );
        len_buf[1] = (uint8_t) ( ( data_len >> 8 ) & 0xFF );

        if ( fwrite ( len_buf, 1, 2, f_out ) != 2 ||
             fwrite ( block->data + 4, 1, data_len, f_out ) != data_len ) {
            fprintf ( stderr, "Error: write failed at block %u\n", i );
            fclose ( f_out );
            tzx_free ( file );
            return EXIT_FAILURE;
        }

        uint8_t flag = ( data_len > 0 ) ? block->data[4] : 0;
        printf ( "  [%3u] 0x10 Standard Speed Data  %5u bytes  flag=0x%02X (%s)\n",
                 i, data_len, flag, tap_flag_name ( flag ) );

        extracted++;
        total_bytes += 2 + data_len;
    }

    fclose ( f_out );

    printf ( "\nExtracted %u TAP block(s), %u bytes total.\n", extracted, total_bytes );
    printf ( "Output: %s\n", output_file );

    tzx_free ( file );
    return EXIT_SUCCESS;
}
