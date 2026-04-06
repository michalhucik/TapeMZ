/**
 * @file   tmzconv.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Konverze hlavicky mezi TMZ a TZX formaty.
 *
 * Nacte TMZ nebo TZX soubor a prepise signaturu v hlavicce
 * na opacny format: TZX ("ZXTape!") -> TMZ ("TapeMZ!") a naopak.
 * Bloky a data zustavaji beze zmeny.
 *
 * @par Pouziti:
 * @code
 *   tmzconv input.tzx output.tmz    (TZX -> TMZ)
 *   tmzconv input.tmz output.tzx    (TMZ -> TZX)
 *   tmzconv --to-tmz input.tzx output.tmz
 *   tmzconv --to-tzx input.tmz output.tzx
 * @endcode
 *
 * @par Volby:
 * - --to-tmz        : vynutit konverzi na TMZ format
 * - --to-tzx        : vynutit konverzi na TZX format
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
#include <stdbool.h>

#include "libs/tmz/tmz.h"
#include "libs/tzx/tzx.h"


/** @brief Verze programu tmzconv. */
#define TMZCONV_VERSION "1.0.0"


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
    fprintf ( stderr, "Usage: %s [--to-tmz|--to-tzx] <input> <output>\n\n", prog_name );
    fprintf ( stderr, "Converts between TMZ and TZX file formats (signature swap).\n\n" );
    fprintf ( stderr, "Without --to-tmz/--to-tzx, automatically converts to the opposite format.\n\n" );
    fprintf ( stderr, "  --version             Show program version\n" );
    fprintf ( stderr, "  --lib-versions        Show library versions\n" );
}


/**
 * @brief Hlavni funkce - konverze signatury TMZ <-> TZX.
 *
 * Nacte vstupni soubor, prepise signaturu a ulozi vysledek.
 * Bez explicitni volby automaticky konvertuje na opacny format:
 * TMZ -> TZX a TZX -> TMZ.
 *
 * @param argc Pocet argumentu.
 * @param argv Pole argumentu.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
int main ( int argc, char *argv[] ) {

    /* kontrola --version a --lib-versions pred kontrolou minimalniho poctu argumentu */
    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp ( argv[i], "--version" ) == 0 ) {
            printf ( "tmzconv %s\n", TMZCONV_VERSION );
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
    int force_tmz = -1; /* -1 = auto, 0 = to-tzx, 1 = to-tmz */

    /* parsovani argumentu */
    int positional = 0;
    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp ( argv[i], "--to-tmz" ) == 0 ) {
            force_tmz = 1;
        } else if ( strcmp ( argv[i], "--to-tzx" ) == 0 ) {
            force_tmz = 0;
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

    /* nacteni souboru */
    st_HANDLER in_handler;
    st_DRIVER in_driver;
    generic_driver_file_init ( &in_driver );

    st_HANDLER *h_in = generic_driver_open_file ( &in_handler, &in_driver,
                                                   (char*) input_file, FILE_DRIVER_OPMODE_RO );
    if ( !h_in ) {
        fprintf ( stderr, "Error: cannot open input file '%s'\n", input_file );
        return EXIT_FAILURE;
    }

    en_TZX_ERROR err;
    st_TZX_FILE *file = tzx_load ( h_in, &err );
    generic_driver_close ( h_in );

    if ( !file ) {
        fprintf ( stderr, "Error: %s\n", tzx_error_string ( err ) );
        return EXIT_FAILURE;
    }

    /* urcit cilovy format */
    bool to_tmz;
    if ( force_tmz == 1 ) {
        to_tmz = true;
    } else if ( force_tmz == 0 ) {
        to_tmz = false;
    } else {
        /* auto: opacny format nez vstup */
        to_tmz = !file->is_tmz;
    }

    /* zmenit signaturu */
    if ( to_tmz ) {
        memcpy ( file->header.signature, TMZ_SIGNATURE, TZX_SIGNATURE_LENGTH );
        file->header.ver_major = TMZ_VERSION_MAJOR;
        file->header.ver_minor = TMZ_VERSION_MINOR;
        file->is_tmz = true;
    } else {
        memcpy ( file->header.signature, TZX_SIGNATURE, TZX_SIGNATURE_LENGTH );
        file->header.ver_major = 1;
        file->header.ver_minor = 20;
        file->is_tmz = false;
    }

    /* ulozit */
    st_HANDLER out_handler;
    st_DRIVER out_driver;
    generic_driver_file_init ( &out_driver );

    st_HANDLER *h_out = generic_driver_open_file ( &out_handler, &out_driver,
                                                    (char*) output_file, FILE_DRIVER_OPMODE_W );
    if ( !h_out ) {
        fprintf ( stderr, "Error: cannot create output file '%s'\n", output_file );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    en_TZX_ERROR save_err = tzx_save ( h_out, file );
    generic_driver_close ( h_out );

    if ( save_err != TZX_OK ) {
        fprintf ( stderr, "Error: %s\n", tzx_error_string ( save_err ) );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    printf ( "Converted: %s -> %s\n", input_file, output_file );
    printf ( "  %s -> %s\n",
             to_tmz ? "TZX (ZXTape!)" : "TMZ (TapeMZ!)",
             to_tmz ? "TMZ (TapeMZ!) v1.0" : "TZX (ZXTape!) v1.20" );
    printf ( "  Blocks: %u\n", file->block_count );

    tzx_free ( file );
    return EXIT_SUCCESS;
}
