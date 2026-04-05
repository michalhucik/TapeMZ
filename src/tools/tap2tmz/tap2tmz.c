/**
 * @file   tap2tmz.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Konverzni utility TAP (ZX Spectrum) -> TMZ/TZX.
 *
 * Nacte TAP soubor (ZX Spectrum kazetovy format) a prekonvertuje
 * kazdy TAP blok na TZX blok 0x10 (Standard Speed Data) v TMZ/TZX
 * souboru. TAP format: sekvence bloku, kazdy = 2B delka LE + data.
 * Data zacinaji flag bajtem (0x00 = header, 0xFF = data blok).
 *
 * Pokud vystupni soubor jiz existuje, nacte ho a prida nove bloky
 * na konec existujici pasky. Pokud neexistuje, vytvori novy soubor.
 *
 * Vystupni soubor je TZX (signatura "ZXTape!") nebo TMZ (signatura
 * "TapeMZ!") podle volby --tmz.
 *
 * @par Pouziti:
 * @code
 *   tap2tmz input.tap output.tzx [volby]
 * @endcode
 *
 * @par Volby:
 * - --pause <ms>  : pauza po kazdem bloku (vychozi: 1000)
 * - --tmz         : pouzit TMZ signaturu misto TZX
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
#include <stdint.h>

#include "libs/tmz/tmz.h"
#include "libs/tzx/tzx.h"


/**
 * @brief Maximalni pocet TAP bloku v jednom souboru.
 */
#define MAX_TAP_BLOCKS  4096


/**
 * @brief Vytvori TZX blok 0x10 (Standard Speed Data) z TAP bloku.
 *
 * Blok 0x10 ma format: [2B pause_ms LE] [2B data_len LE] [data].
 * Alokuje novy st_TZX_BLOCK na heapu.
 *
 * @param data TAP data bloku (flag + payload + checksum).
 * @param data_size Velikost TAP dat v bajtech.
 * @param pause_ms Pauza po bloku v milisekundach.
 * @return Novy blok, nebo NULL pri chybe alokace.
 *
 * @pre data != NULL, data_size > 0.
 * @post Volajici vlastni vraceny blok a musi uvolnit jeho data i strukturu.
 */
static st_TZX_BLOCK* create_standard_speed_block ( const uint8_t *data, uint16_t data_size, uint16_t pause_ms ) {

    uint32_t total = 4 + data_size; /* 2B pause + 2B data_len + data */

    st_TZX_BLOCK *block = calloc ( 1, sizeof ( st_TZX_BLOCK ) );
    if ( !block ) return NULL;

    block->id = TZX_BLOCK_ID_STANDARD_SPEED;
    block->length = total;
    block->data = calloc ( 1, total );
    if ( !block->data ) {
        free ( block );
        return NULL;
    }

    /* 2B pause LE */
    block->data[0] = (uint8_t) ( pause_ms & 0xFF );
    block->data[1] = (uint8_t) ( ( pause_ms >> 8 ) & 0xFF );

    /* 2B data length LE */
    block->data[2] = (uint8_t) ( data_size & 0xFF );
    block->data[3] = (uint8_t) ( ( data_size >> 8 ) & 0xFF );

    /* data */
    memcpy ( block->data + 4, data, data_size );

    return block;
}


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
 * @brief Vypise napovedu programu.
 * @param prog_name Nazev spusteneho programu (argv[0]).
 */
static void print_usage ( const char *prog_name ) {
    fprintf ( stderr, "Usage: %s <input.tap> <output.tzx|output.tmz> [options]\n\n", prog_name );
    fprintf ( stderr, "Converts ZX Spectrum TAP file to TZX/TMZ format.\n\n" );
    fprintf ( stderr, "Options:\n" );
    fprintf ( stderr, "  --pause <ms>   Pause after each block in ms (default: 1000)\n" );
    fprintf ( stderr, "  --tmz          Use TMZ signature instead of TZX\n" );
}


/**
 * @brief Hlavni funkce - konverze TAP -> TMZ/TZX.
 *
 * Nacte TAP soubor (sekvence bloku: 2B delka LE + data),
 * prekonvertuje kazdy blok na TZX 0x10. Pokud vystupni soubor
 * jiz existuje, prida bloky na konec. Pokud ne, vytvori novy.
 *
 * @param argc Pocet argumentu.
 * @param argv Pole argumentu.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
int main ( int argc, char *argv[] ) {

    if ( argc < 3 ) {
        print_usage ( argv[0] );
        return EXIT_FAILURE;
    }

    /* vychozi parametry */
    const char *input_file = NULL;
    const char *output_file = NULL;
    uint16_t pause_ms = 1000;
    bool use_tmz = false;

    /* parsovani argumentu */
    int positional = 0;
    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp ( argv[i], "--pause" ) == 0 ) {
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
        } else if ( strcmp ( argv[i], "--tmz" ) == 0 ) {
            use_tmz = true;
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

    /* nacteni TAP souboru do pameti */
    FILE *f = fopen ( input_file, "rb" );
    if ( !f ) {
        fprintf ( stderr, "Error: cannot open input file '%s'\n", input_file );
        return EXIT_FAILURE;
    }

    fseek ( f, 0, SEEK_END );
    long file_size = ftell ( f );
    fseek ( f, 0, SEEK_SET );

    if ( file_size <= 0 || file_size > 16 * 1024 * 1024 ) {
        fprintf ( stderr, "Error: invalid file size (%ld bytes)\n", file_size );
        fclose ( f );
        return EXIT_FAILURE;
    }

    uint8_t *tap_data = malloc ( (size_t) file_size );
    if ( !tap_data ) {
        fprintf ( stderr, "Error: memory allocation failed\n" );
        fclose ( f );
        return EXIT_FAILURE;
    }

    if ( fread ( tap_data, 1, (size_t) file_size, f ) != (size_t) file_size ) {
        fprintf ( stderr, "Error: failed to read file\n" );
        free ( tap_data );
        fclose ( f );
        return EXIT_FAILURE;
    }
    fclose ( f );

    /* parsovani TAP bloku */
    st_TZX_BLOCK *blocks[MAX_TAP_BLOCKS];
    uint32_t block_count = 0;
    uint32_t offset = 0;

    while ( offset + 2 <= (uint32_t) file_size && block_count < MAX_TAP_BLOCKS ) {
        /* 2B block length LE */
        uint16_t block_len = (uint16_t) tap_data[offset] | ( (uint16_t) tap_data[offset + 1] << 8 );
        offset += 2;

        if ( block_len == 0 ) {
            /* prazdny blok - preskocit */
            continue;
        }

        if ( offset + block_len > (uint32_t) file_size ) {
            fprintf ( stderr, "Warning: truncated TAP block at offset %u (need %u bytes, have %u)\n",
                      offset - 2, block_len, (uint32_t) file_size - offset );
            break;
        }

        /* vytvorit TZX blok 0x10 */
        st_TZX_BLOCK *block = create_standard_speed_block ( tap_data + offset, block_len, pause_ms );
        if ( !block ) {
            fprintf ( stderr, "Error: memory allocation failed for block %u\n", block_count );
            break;
        }

        blocks[block_count] = block;
        block_count++;
        offset += block_len;
    }

    free ( tap_data );

    if ( block_count == 0 ) {
        fprintf ( stderr, "Error: no TAP blocks found in '%s'\n", input_file );
        return EXIT_FAILURE;
    }

    /* nacteni existujiciho souboru nebo vytvoreni noveho */
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
                for ( uint32_t i = 0; i < block_count; i++ ) {
                    free ( blocks[i]->data );
                    free ( blocks[i] );
                }
                return EXIT_FAILURE;
            }
            old_block_count = tmz_file->block_count;
        }
    }

    if ( !tmz_file ) {
        /* soubor neexistuje - vytvorit novy */
        tmz_file = calloc ( 1, sizeof ( st_TZX_FILE ) );
        if ( !tmz_file ) {
            fprintf ( stderr, "Error: memory allocation failed\n" );
            for ( uint32_t i = 0; i < block_count; i++ ) {
                free ( blocks[i]->data );
                free ( blocks[i] );
            }
            return EXIT_FAILURE;
        }
        if ( use_tmz ) {
            tmz_header_init ( &tmz_file->header );
            tmz_file->is_tmz = true;
        } else {
            memcpy ( tmz_file->header.signature, TZX_SIGNATURE, TZX_SIGNATURE_LENGTH );
            tmz_file->header.eof_marker = TZX_EOF_MARKER;
            tmz_file->header.ver_major = 1;
            tmz_file->header.ver_minor = 20;
            tmz_file->is_tmz = false;
        }
    }

    /* pridani vsech bloku na konec pasky */
    for ( uint32_t i = 0; i < block_count; i++ ) {
        en_TZX_ERROR append_err = tzx_file_append_block ( tmz_file, blocks[i] );
        if ( append_err != TZX_OK ) {
            fprintf ( stderr, "Error: failed to append block %u: %s\n",
                      i, tzx_error_string ( append_err ) );
            /* neuvolnujeme data bloku, ktere uz byly predane */
            for ( uint32_t j = i; j < block_count; j++ ) {
                free ( blocks[j]->data );
                free ( blocks[j] );
            }
            tzx_free ( tmz_file );
            return EXIT_FAILURE;
        }
        free ( blocks[i] ); /* wrapper uvolnit, data vlastni tmz_file */
    }

    /* otevreni vystupniho souboru a zapis */
    st_HANDLER out_handler;
    st_DRIVER out_driver;
    generic_driver_file_init ( &out_driver );

    st_HANDLER *h_out = generic_driver_open_file ( &out_handler, &out_driver,
                                                    (char*) output_file, FILE_DRIVER_OPMODE_W );
    if ( !h_out ) {
        fprintf ( stderr, "Error: cannot create output file '%s'\n", output_file );
        tzx_free ( tmz_file );
        return EXIT_FAILURE;
    }

    en_TZX_ERROR tmz_err = tzx_save ( h_out, tmz_file );
    generic_driver_close ( h_out );

    if ( tmz_err != TZX_OK ) {
        fprintf ( stderr, "Error: failed to write output: %s\n", tzx_error_string ( tmz_err ) );
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
    printf ( "  Format: %s\n", tmz_file->is_tmz ? "TMZ (TapeMZ!)" : "TZX (ZXTape!) v1.20" );
    printf ( "  Blocks: %u (new: %u)\n", tmz_file->block_count, block_count );
    printf ( "  Pause : %u ms\n\n", pause_ms );

    for ( uint32_t i = 0; i < block_count; i++ ) {
        uint32_t idx = old_block_count + i;
        uint16_t data_len = tmz_file->blocks[idx].length - 4;
        uint8_t flag = tmz_file->blocks[idx].data[4]; /* prvni bajt dat */
        printf ( "  [%3u] 0x10 Standard Speed Data  %5u bytes  flag=0x%02X (%s)\n",
                 idx, data_len, flag, tap_flag_name ( flag ) );
    }

    /* uklid */
    tzx_free ( tmz_file );

    return EXIT_SUCCESS;
}
