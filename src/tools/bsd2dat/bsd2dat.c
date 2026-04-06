/**
 * @file   bsd2dat.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.1
 * @brief  Export BSD/BRD dat z TMZ bloku 0x45 do binarniho souboru.
 *
 * Nacte TMZ soubor, najde bloky 0x45 (MZ BASIC Data), extrahuje
 * z nich chunky a sestavi binarni vystup. Podporuje dva rezimy:
 * --solid (vychozi) spoji data ze vsech chunku do jednoho souboru,
 * --chunks ulozi kazdy chunk jako samostatny soubor do adresare.
 *
 * @par Pouziti:
 * @code
 *   bsd2dat input.tmz [volby]
 * @endcode
 *
 * @par Volby:
 * - --output <cesta>       : vystupni soubor (solid) nebo adresar (chunks) (vychozi: odvozeno ze vstupu)
 * - --index <N>            : extrahovat jen blok na indexu N (0-based)
 * - --list                 : vypsat BSD bloky bez extrakce
 * - --chunks               : kazdy chunk jako samostatny soubor do adresare
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
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tzx/tzx.h"
#include "libs/mzf/mzf.h"
#include "libs/mzf/mzf_tools.h"
#include "libs/endianity/endianity.h"


/** @brief Verze programu bsd2dat. */
#define BSD2DAT_VERSION "1.0.1"


/** @brief Maximalni delka cesty k vystupnimu souboru. */
#define MAX_PATH_LENGTH  1024


/** @brief Kodovani nazvu souboru pro zobrazeni (file-level, nastaveno z --name-encoding). */
static en_MZF_NAME_ENCODING name_encoding = MZF_NAME_ASCII;


/**
 * @brief Vrati textovy nazev typu MZF souboru.
 * @param ftype Hodnota MZF ftype pole.
 * @return Staticky retezec s nazvem typu.
 */
static const char* mzf_ftype_name ( uint8_t ftype ) {
    switch ( ftype ) {
        case MZF_FTYPE_BSD: return "BSD (BASIC data)";
        case MZF_FTYPE_BRD: return "BRD (BASIC read-after-run)";
        default:            return "Unknown";
    }
}


/**
 * @brief Extrahuje bazovy nazev souboru z cesty (bez adresare a pripony).
 *
 * Z cesty "path/to/file.tmz" vrati "file".
 *
 * @param path Vstupni cesta k souboru.
 * @param[out] buf Vystupni buffer pro bazovy nazev.
 * @param buf_size Velikost vystupniho bufferu.
 *
 * @pre path != NULL, buf != NULL, buf_size > 0.
 * @post buf obsahuje nulou ukonceny retezec.
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
 * @brief Vytvori kopii TMZ bloku pro bezpecne parsovani.
 *
 * Parse funkce modifikuji data bloku (konverze endianity),
 * takze pro opakovane cteni je nutne pracovat s kopii.
 *
 * @param block Zdrojovy blok (nesmi byt NULL).
 * @param[out] copy Vystupni struktura kopie bloku.
 * @param[out] data_copy Alokovany buffer s kopii dat.
 * @return true pri uspechu, false pri chybe alokace.
 *
 * @post Pri uspechu data_copy ukazuje na novy buffer, ktery musi volajici uvolnit.
 */
static bool copy_block ( const st_TZX_BLOCK *block, st_TZX_BLOCK *copy, uint8_t **data_copy ) {
    *data_copy = malloc ( block->length );
    if ( !*data_copy ) return false;
    memcpy ( *data_copy, block->data, block->length );
    *copy = *block;
    copy->data = *data_copy;
    return true;
}


/**
 * @brief Exportuje BSD data z bloku 0x45 do jednoho binarniho souboru.
 *
 * Spoji datove casti vsech chunku (vcetne terminacniho) do jednoho
 * binarniho souboru. Terminacni chunk (ID=0xFFFF) nese data stejne
 * jako ostatni chunky - to odpovida chovani BSD dekoderu i realneho
 * hardwaru (MZ-800 BASIC).
 *
 * @param block TMZ blok 0x45.
 * @param block_index Index bloku v TMZ souboru (pro diagnostiku).
 * @param output_path Cesta k vystupnimu souboru.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int export_solid ( const st_TZX_BLOCK *block, uint32_t block_index, const char *output_path ) {

    /* kopie bloku pro bezpecne parsovani */
    st_TZX_BLOCK copy;
    uint8_t *data_copy;
    if ( !copy_block ( block, &copy, &data_copy ) ) {
        fprintf ( stderr, "Error: memory allocation failed for block [%u]\n", block_index );
        return EXIT_FAILURE;
    }

    en_TZX_ERROR err;
    uint8_t *chunks_data = NULL;
    st_TMZ_MZ_BASIC_DATA *bsd = tmz_block_parse_mz_basic_data ( &copy, &chunks_data, &err );

    if ( !bsd ) {
        fprintf ( stderr, "Error: failed to parse BSD block [%u]: %s\n",
                  block_index, tzx_error_string ( err ) );
        free ( data_copy );
        return EXIT_FAILURE;
    }

    uint16_t chunk_count = bsd->chunk_count;
    size_t total_size = (size_t) chunk_count * TMZ_BASIC_CHUNK_DATA_SIZE;

    /* zapis souboru - data vsech chunku vcetne terminacniho */
    FILE *fp = fopen ( output_path, "wb" );
    if ( !fp ) {
        fprintf ( stderr, "Error: cannot create output file '%s'\n", output_path );
        free ( data_copy );
        return EXIT_FAILURE;
    }

    for ( uint16_t i = 0; i < chunk_count; i++ ) {
        const uint8_t *chunk = chunks_data + (size_t) i * TMZ_BASIC_CHUNK_SIZE;
        fwrite ( chunk + 2, 1, TMZ_BASIC_CHUNK_DATA_SIZE, fp );
    }

    fclose ( fp );

    /* souhrn */
    char fname[MZF_FNAME_UTF8_BUF_SIZE];
    mzf_tools_get_fname_ex ( &bsd->mzf_header, fname, sizeof ( fname ), name_encoding );

    printf ( "  Block [%u] -> %s\n", block_index, output_path );
    printf ( "    Filename : \"%s\"\n", fname );
    printf ( "    Type     : 0x%02X (%s)\n", bsd->mzf_header.ftype, mzf_ftype_name ( bsd->mzf_header.ftype ) );
    printf ( "    Chunks   : %u (%u data + termination)\n", chunk_count, chunk_count - 1 );
    printf ( "    Data size: %zu bytes\n", total_size );

    free ( data_copy );
    return EXIT_SUCCESS;
}


/**
 * @brief Exportuje BSD data z bloku 0x45 jako jednotlive chunk soubory.
 *
 * Vytvori adresar a do nej ulozi kazdy chunk jako samostatny soubor
 * pojmenovany prefix_NNNN.dat, kde NNNN je chunk ID.
 *
 * @param block TMZ blok 0x45.
 * @param block_index Index bloku v TMZ souboru (pro diagnostiku).
 * @param output_dir Cesta k vystupnimu adresari.
 * @param prefix Prefix nazvu chunk souboru.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int export_chunks ( const st_TZX_BLOCK *block, uint32_t block_index,
                            const char *output_dir, const char *prefix ) {

    /* kopie bloku pro bezpecne parsovani */
    st_TZX_BLOCK copy;
    uint8_t *data_copy;
    if ( !copy_block ( block, &copy, &data_copy ) ) {
        fprintf ( stderr, "Error: memory allocation failed for block [%u]\n", block_index );
        return EXIT_FAILURE;
    }

    en_TZX_ERROR err;
    uint8_t *chunks_data = NULL;
    st_TMZ_MZ_BASIC_DATA *bsd = tmz_block_parse_mz_basic_data ( &copy, &chunks_data, &err );

    if ( !bsd ) {
        fprintf ( stderr, "Error: failed to parse BSD block [%u]: %s\n",
                  block_index, tzx_error_string ( err ) );
        free ( data_copy );
        return EXIT_FAILURE;
    }

    /* vytvoreni vystupniho adresare */
    if ( mkdir ( output_dir ) != 0 && errno != EEXIST ) {
        fprintf ( stderr, "Error: cannot create output directory '%s': %s\n",
                  output_dir, strerror ( errno ) );
        free ( data_copy );
        return EXIT_FAILURE;
    }

    /* export chunku */
    uint16_t chunk_count = bsd->chunk_count;
    uint16_t exported = 0;

    for ( uint16_t i = 0; i < chunk_count; i++ ) {
        const uint8_t *chunk = chunks_data + (size_t) i * TMZ_BASIC_CHUNK_SIZE;
        uint16_t chunk_id;
        memcpy ( &chunk_id, chunk, 2 );
        chunk_id = endianity_bswap16_LE ( chunk_id );

        char chunk_path[MAX_PATH_LENGTH];
        snprintf ( chunk_path, sizeof ( chunk_path ), "%s/%s_%04X.dat",
                   output_dir, prefix, chunk_id );

        FILE *fp = fopen ( chunk_path, "wb" );
        if ( !fp ) {
            fprintf ( stderr, "Error: cannot create chunk file '%s'\n", chunk_path );
            free ( data_copy );
            return EXIT_FAILURE;
        }

        fwrite ( chunk + 2, 1, TMZ_BASIC_CHUNK_DATA_SIZE, fp );
        fclose ( fp );
        exported++;

        if ( chunk_id == TMZ_BASIC_LAST_CHUNK_ID ) break;
    }

    /* souhrn */
    char fname[MZF_FNAME_UTF8_BUF_SIZE];
    mzf_tools_get_fname_ex ( &bsd->mzf_header, fname, sizeof ( fname ), name_encoding );

    printf ( "  Block [%u] -> %s/\n", block_index, output_dir );
    printf ( "    Filename : \"%s\"\n", fname );
    printf ( "    Type     : 0x%02X (%s)\n", bsd->mzf_header.ftype, mzf_ftype_name ( bsd->mzf_header.ftype ) );
    printf ( "    Chunks   : %u files exported\n", exported );

    free ( data_copy );
    return EXIT_SUCCESS;
}


/**
 * @brief Vypise verze vsech pouzitych knihoven na stdout.
 */
static void print_lib_versions ( void ) {
    printf ( "Library versions:\n" );
    printf ( "  tmz            %s (TMZ format v%s)\n", tmz_version (), tmz_format_version () );
    printf ( "  tzx            %s (TZX format v%s)\n", tzx_version (), tzx_format_version () );
    printf ( "  mzf            %s\n", mzf_version () );
    printf ( "  endianity      %s\n", endianity_version () );
}


/**
 * @brief Vypise napovedu programu.
 * @param prog_name Nazev spusteneho programu (argv[0]).
 */
static void print_usage ( const char *prog_name ) {
    fprintf ( stderr, "Usage: %s <input.tmz> [options]\n\n", prog_name );
    fprintf ( stderr, "Exports BSD/BRD data from TMZ block 0x45 to binary file(s).\n\n" );
    fprintf ( stderr, "Options:\n" );
    fprintf ( stderr, "  --output <path>   Output file (solid) or directory (chunks)\n" );
    fprintf ( stderr, "                    (default: derived from input)\n" );
    fprintf ( stderr, "  --index <N>       Extract only block at index N (0-based)\n" );
    fprintf ( stderr, "  --list            List BSD blocks without extracting\n" );
    fprintf ( stderr, "  --chunks          Export each chunk as separate file into directory\n" );
    fprintf ( stderr, "  --name-encoding <enc> Filename encoding: ascii, utf8-eu, utf8-jp (default: ascii)\n" );
    fprintf ( stderr, "  --version             Show program version\n" );
    fprintf ( stderr, "  --lib-versions        Show library versions\n" );
}


/**
 * @brief Hlavni funkce - export BSD/BRD dat z TMZ.
 *
 * Zpracuje argumenty prikazove radky, nacte TMZ soubor,
 * najde bloky 0x45 a extrahuje z nich data.
 *
 * @param argc Pocet argumentu.
 * @param argv Pole argumentu.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
int main ( int argc, char *argv[] ) {

    /* kontrola --version a --lib-versions pred kontrolou minimalniho poctu argumentu */
    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp ( argv[i], "--version" ) == 0 ) {
            printf ( "bsd2dat %s\n", BSD2DAT_VERSION );
            return EXIT_SUCCESS;
        }
        if ( strcmp ( argv[i], "--lib-versions" ) == 0 ) {
            print_lib_versions ();
            return EXIT_SUCCESS;
        }
    }

    if ( argc < 2 ) {
        print_usage ( argv[0] );
        return EXIT_FAILURE;
    }

    /* vychozi parametry */
    const char *input_file = NULL;
    const char *output_path = NULL;
    int selected_index = -1;
    bool list_only = false;
    bool chunks_mode = false;

    /* parsovani argumentu */
    int positional = 0;
    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp ( argv[i], "--output" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --output requires a value\n" );
                return EXIT_FAILURE;
            }
            output_path = argv[i];
        } else if ( strcmp ( argv[i], "--index" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --index requires a value\n" );
                return EXIT_FAILURE;
            }
            char *endptr;
            long val = strtol ( argv[i], &endptr, 10 );
            if ( *endptr != '\0' || val < 0 || val > 0xFFFFFF ) {
                fprintf ( stderr, "Error: invalid block index '%s'\n", argv[i] );
                return EXIT_FAILURE;
            }
            selected_index = (int) val;
        } else if ( strcmp ( argv[i], "--list" ) == 0 ) {
            list_only = true;
        } else if ( strcmp ( argv[i], "--chunks" ) == 0 ) {
            chunks_mode = true;
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
            else {
                fprintf ( stderr, "Error: too many arguments\n" );
                return EXIT_FAILURE;
            }
            positional++;
        }
    }

    if ( !input_file ) {
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

    en_TZX_ERROR err;
    st_TZX_FILE *file = tzx_load ( h_in, &err );
    generic_driver_close ( h_in );

    if ( !file ) {
        fprintf ( stderr, "Error: %s\n", tzx_error_string ( err ) );
        return EXIT_FAILURE;
    }

    /* spocitat BSD bloky */
    uint32_t bsd_count = 0;
    for ( uint32_t i = 0; i < file->block_count; i++ ) {
        if ( file->blocks[i].id == TMZ_BLOCK_ID_MZ_BASIC_DATA ) {
            bsd_count++;
        }
    }

    /* kontrola --index */
    if ( selected_index >= 0 ) {
        if ( (uint32_t) selected_index >= file->block_count ) {
            fprintf ( stderr, "Error: block index %d out of range (0-%u)\n",
                      selected_index, file->block_count - 1 );
            tzx_free ( file );
            return EXIT_FAILURE;
        }
        if ( file->blocks[selected_index].id != TMZ_BLOCK_ID_MZ_BASIC_DATA ) {
            fprintf ( stderr, "Error: block [%d] is 0x%02X (%s), not a BSD block (0x45)\n",
                      selected_index, file->blocks[selected_index].id,
                      tmz_block_id_name ( file->blocks[selected_index].id ) );
            tzx_free ( file );
            return EXIT_FAILURE;
        }
    }

    /* rezim --list */
    if ( list_only ) {
        printf ( "=== %s ===\n\n", input_file );
        printf ( "File type: %s, Version: %u.%u, Blocks: %u\n\n",
                 file->is_tmz ? "TMZ" : "TZX",
                 file->header.ver_major, file->header.ver_minor,
                 file->block_count );

        if ( bsd_count == 0 ) {
            printf ( "No BSD blocks (0x45) found.\n" );
        } else {
            printf ( "BSD blocks (%u):\n\n", bsd_count );
            for ( uint32_t i = 0; i < file->block_count; i++ ) {
                if ( file->blocks[i].id != TMZ_BLOCK_ID_MZ_BASIC_DATA ) continue;

                st_TZX_BLOCK copy;
                uint8_t *dc;
                if ( !copy_block ( &file->blocks[i], &copy, &dc ) ) continue;

                en_TZX_ERROR parse_err;
                uint8_t *chunks_data = NULL;
                st_TMZ_MZ_BASIC_DATA *bsd = tmz_block_parse_mz_basic_data ( &copy, &chunks_data, &parse_err );

                if ( bsd ) {
                    char fname[MZF_FNAME_UTF8_BUF_SIZE];
                    mzf_tools_get_fname_ex ( &bsd->mzf_header, fname, sizeof ( fname ), name_encoding );

                    printf ( "  [%3u] 0x45 MZ BASIC Data  \"%s\"  type=0x%02X (%s)  chunks=%u  data=%u bytes\n",
                             i, fname, bsd->mzf_header.ftype,
                             mzf_ftype_name ( bsd->mzf_header.ftype ),
                             bsd->chunk_count,
                             (unsigned) ( bsd->chunk_count * TMZ_BASIC_CHUNK_DATA_SIZE ) );
                }

                free ( dc );
            }
        }

        tzx_free ( file );
        return EXIT_SUCCESS;
    }

    /* extrakce */
    if ( bsd_count == 0 && selected_index < 0 ) {
        fprintf ( stderr, "Error: no BSD blocks (0x45) found in '%s'\n", input_file );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    /* bazovy nazev pro vystup */
    char basename[MAX_PATH_LENGTH / 2];
    if ( output_path ) {
        extract_basename ( output_path, basename, sizeof ( basename ) - 16 );
    } else {
        extract_basename ( input_file, basename, sizeof ( basename ) - 16 );
    }

    printf ( "Extracting from: %s\n\n", input_file );

    int result = EXIT_SUCCESS;
    uint32_t target_count = ( selected_index >= 0 ) ? 1 : bsd_count;
    uint32_t extracted = 0;

    for ( uint32_t i = 0; i < file->block_count; i++ ) {

        if ( selected_index >= 0 ) {
            if ( (uint32_t) selected_index != i ) continue;
        } else {
            if ( file->blocks[i].id != TMZ_BLOCK_ID_MZ_BASIC_DATA ) continue;
        }

        if ( chunks_mode ) {
            /* rezim chunku - export do adresare */
            char dir_path[MAX_PATH_LENGTH];
            char prefix[MAX_PATH_LENGTH];

            if ( target_count == 1 ) {
                if ( output_path ) {
                    strncpy ( dir_path, output_path, sizeof ( dir_path ) - 1 );
                    dir_path[sizeof ( dir_path ) - 1] = '\0';
                } else {
                    snprintf ( dir_path, sizeof ( dir_path ), "%s_chunks", basename );
                }
                strncpy ( prefix, basename, sizeof ( prefix ) - 1 );
                prefix[sizeof ( prefix ) - 1] = '\0';
            } else {
                snprintf ( dir_path, sizeof ( dir_path ), "%s_%03u_chunks",
                           basename, extracted + 1 );
                snprintf ( prefix, sizeof ( prefix ), "%s_%03u",
                           basename, extracted + 1 );
            }

            if ( export_chunks ( &file->blocks[i], i, dir_path, prefix ) != EXIT_SUCCESS ) {
                result = EXIT_FAILURE;
            }
        } else {
            /* rezim solid - export do jednoho souboru */
            char out_path[MAX_PATH_LENGTH];

            if ( target_count == 1 ) {
                if ( output_path ) {
                    strncpy ( out_path, output_path, sizeof ( out_path ) - 1 );
                    out_path[sizeof ( out_path ) - 1] = '\0';
                } else {
                    snprintf ( out_path, sizeof ( out_path ), "%s.dat", basename );
                }
            } else {
                snprintf ( out_path, sizeof ( out_path ), "%s_%03u.dat",
                           basename, extracted + 1 );
            }

            if ( export_solid ( &file->blocks[i], i, out_path ) != EXIT_SUCCESS ) {
                result = EXIT_FAILURE;
            }
        }

        extracted++;
        if ( selected_index >= 0 ) break;
    }

    printf ( "\nExtracted %u BSD block(s).\n", extracted );

    tzx_free ( file );
    return result;
}
