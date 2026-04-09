/**
 * @file   tmz2mzf.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.1.0
 * @brief  Konverzni utility TMZ/TZX -> MZF.
 *
 * Nacte TMZ nebo TZX soubor, najde bloky obsahujici MZF data
 * (0x40 MZ Standard Data, 0x41 MZ Turbo Data) a extrahuje
 * z nich MZF soubory. Podporuje extrakci vsech bloku najednou
 * nebo vyber konkretniho bloku podle indexu.
 *
 * Ve vychozim rezimu odmitne prepsat existujici vystupni soubor.
 * Pro prepis je nutne pouzit --overwrite, pro pripojeni na konec
 * existujiciho souboru (multi-MZF/MZT) --append.
 *
 * @par Pouziti:
 * @code
 *   tmz2mzf input.tmz [volby]
 * @endcode
 *
 * @par Volby:
 * - --output <soubor>      : vystupni soubor (vychozi: odvozeno ze vstupu)
 * - --index <N>            : extrahovat jen blok na indexu N (0-based)
 * - --list                 : vypsat extrahovatelne bloky bez extrakce
 * - --overwrite            : povolit prepis existujiciho souboru
 * - --append               : pripojit na konec existujiciho souboru (multi-MZF)
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
#include <stdbool.h>
#include <string.h>

#include "libs/tmz/tmz.h"
#include "libs/tzx/tzx.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/mzf/mzf.h"
#include "libs/mzf/mzf_tools.h"

/** @brief Verze programu tmz2mzf (z @version v hlavicce souboru). */
#define TMZ2MZF_VERSION  "1.1.0"


/**
 * @brief Maximalni delka cesty k vystupnimu souboru.
 */
#define MAX_PATH_LENGTH  1024


/**
 * @brief Rezim zapisu vystupniho souboru.
 *
 * Urcuje chovani pri kolizi s existujicim souborem.
 */
typedef enum en_WRITE_MODE {
    WRITE_MODE_DEFAULT,     /**< odmitne prepsat existujici soubor */
    WRITE_MODE_OVERWRITE,   /**< prepise existujici soubor */
    WRITE_MODE_APPEND,      /**< pripoji na konec existujiciho souboru (multi-MZF) */
} en_WRITE_MODE;


/** @brief Kodovani nazvu souboru pro zobrazeni (file-level, nastaveno z --name-encoding). */
static en_MZF_NAME_ENCODING name_encoding = MZF_NAME_ASCII;


/**
 * @brief Vrati textovy nazev typu MZF souboru.
 * @param ftype Hodnota MZF ftype pole.
 * @return Staticky retezec s nazvem typu.
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
 * @brief Overi, zda blok obsahuje extrahovatelna MZF data.
 *
 * Extrahovatelne jsou bloky 0x40 (MZ Standard Data) a 0x41 (MZ Turbo Data),
 * ktere obsahuji kompletni MZF hlavicku a datove telo.
 *
 * @param block_id Identifikator bloku.
 * @return true pokud blok obsahuje MZF data, false jinak.
 */
static bool is_extractable ( uint8_t block_id ) {
    return block_id == TMZ_BLOCK_ID_MZ_STANDARD_DATA ||
           block_id == TMZ_BLOCK_ID_MZ_TURBO_DATA;
}


/**
 * @brief Extrahuje bazovy nazev souboru z cesty (bez adresare a pripony).
 *
 * Z cesty "path/to/file.tmz" vrati "file". Vysledek se zapise do bufferu
 * o velikosti buf_size. Pokud cesta neobsahuje priponu, pouzije se cely
 * nazev souboru. Pokud cesta neobsahuje adresar, pouzije se cely vstup.
 *
 * @param path Vstupni cesta k souboru.
 * @param[out] buf Vystupni buffer pro bazovy nazev.
 * @param buf_size Velikost vystupniho bufferu.
 *
 * @pre path != NULL, buf != NULL, buf_size > 0.
 * @post buf obsahuje nulou ukonceny retezec s bazovym nazvem.
 */
static void extract_basename ( const char *path, char *buf, size_t buf_size ) {
    /* najit posledni oddelovac adresare */
    const char *name = path;
    const char *p;
    for ( p = path; *p; p++ ) {
        if ( *p == '/' || *p == '\\' ) name = p + 1;
    }
    /* kopirovat do bufferu */
    strncpy ( buf, name, buf_size - 1 );
    buf[buf_size - 1] = '\0';
    /* odriznout priponu */
    char *dot = strrchr ( buf, '.' );
    if ( dot && dot != buf ) {
        *dot = '\0';
    }
}


/**
 * @brief Zjisti, zda soubor na dane ceste existuje.
 *
 * Pouziva fopen("rb") pro platformne nezavisly test.
 *
 * @param path Cesta k souboru.
 * @return true pokud soubor existuje, false pokud ne.
 *
 * @pre path != NULL.
 */
static bool file_exists ( const char *path ) {
    FILE *f = fopen ( path, "rb" );
    if ( f ) {
        fclose ( f );
        return true;
    }
    return false;
}


/**
 * @brief Zjisti velikost existujiciho souboru v bajtech.
 *
 * Pouziva fseek/ftell pro zjisteni velikosti.
 *
 * @param path Cesta k souboru.
 * @return Velikost souboru v bajtech, nebo 0 pokud soubor neexistuje ci doslo k chybe.
 *
 * @pre path != NULL.
 */
static uint32_t get_file_size ( const char *path ) {
    FILE *f = fopen ( path, "rb" );
    if ( !f ) return 0;
    fseek ( f, 0, SEEK_END );
    long size = ftell ( f );
    fclose ( f );
    return ( size > 0 ) ? (uint32_t) size : 0;
}


/**
 * @brief Vytvori kopii TMZ bloku pro bezpecne parsovani.
 *
 * Parse funkce (tmz_block_parse_mz_standard, tmz_block_parse_mz_turbo)
 * modifikuji data bloku (konverze endianity), takze pro opakovane cteni
 * je nutne pracovat s kopii.
 *
 * @param block Zdrojovy blok (nesmi byt NULL).
 * @param[out] copy Vystupni struktura kopie bloku.
 * @param[out] data_copy Alokovany buffer s kopii dat.
 * @return true pri uspechu, false pri chybe alokace.
 *
 * @pre block != NULL, copy != NULL, data_copy != NULL.
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
 * @brief Extrahuje MZF soubor z TMZ bloku a ulozi ho na disk.
 *
 * Vytvori kopii bloku (kvuli destruktivnimu parsovani), extrahuje MZF
 * pres tmz_block_to_mzf() a zapise vysledek do vystupniho souboru.
 *
 * V rezimu WRITE_MODE_APPEND pripoji data na konec existujiciho souboru
 * (vytvori multi-MZF/MZT). Pokud soubor neexistuje, vytvori novy.
 *
 * V rezimu WRITE_MODE_DEFAULT odmitne prepsat existujici soubor.
 * V rezimu WRITE_MODE_OVERWRITE existujici soubor prepise.
 *
 * @param block Zdrojovy TMZ blok (0x40 nebo 0x41).
 * @param block_index Index bloku v TMZ souboru (pro diagnosticke zpravy).
 * @param output_path Cesta k vystupnimu MZF souboru.
 * @param write_mode Rezim zapisu (default/overwrite/append).
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 *
 * @pre block != NULL, output_path != NULL.
 * @pre block->id musi byt 0x40 nebo 0x41.
 */
static int extract_and_save ( const st_TZX_BLOCK *block, uint32_t block_index,
                              const char *output_path, en_WRITE_MODE write_mode ) {

    /* kontrola existence vystupniho souboru */
    bool exists = file_exists ( output_path );

    if ( exists && write_mode == WRITE_MODE_DEFAULT ) {
        fprintf ( stderr, "Error: output file '%s' already exists (use --overwrite or --append)\n",
                  output_path );
        return EXIT_FAILURE;
    }

    /* kopie bloku pro bezpecne parsovani */
    st_TZX_BLOCK copy;
    uint8_t *data_copy;
    if ( !copy_block ( block, &copy, &data_copy ) ) {
        fprintf ( stderr, "Error: memory allocation failed for block [%u]\n", block_index );
        return EXIT_FAILURE;
    }

    /* extrakce MZF */
    en_TZX_ERROR err;
    st_MZF *mzf = tmz_block_to_mzf ( &copy, &err );
    free ( data_copy );

    if ( !mzf ) {
        fprintf ( stderr, "Error: failed to extract MZF from block [%u]: %s\n",
                  block_index, tzx_error_string ( err ) );
        return EXIT_FAILURE;
    }

    /* urceni offsetu a rezimu otevreni souboru */
    uint32_t write_offset = 0;
    en_FILE_DRIVER_OPEN_MODE open_mode = FILE_DRIVER_OPMODE_W;

    if ( write_mode == WRITE_MODE_APPEND && exists ) {
        write_offset = get_file_size ( output_path );
        open_mode = FILE_DRIVER_OPMODE_RW;
    }

    /* otevreni vystupniho souboru a zapis MZF */
    st_HANDLER out_handler;
    st_DRIVER out_driver;
    generic_driver_file_init ( &out_driver );

    st_HANDLER *h_out = generic_driver_open_file ( &out_handler, &out_driver,
                                                    (char*) output_path, open_mode );
    if ( !h_out ) {
        fprintf ( stderr, "Error: cannot open output file '%s'\n", output_path );
        mzf_free ( mzf );
        return EXIT_FAILURE;
    }

    /* zapis hlavicky a tela na urceny offset */
    en_MZF_ERROR mzf_err = MZF_OK;

    if ( mzf_write_header_on_offset ( h_out, write_offset, &mzf->header ) != EXIT_SUCCESS ) {
        mzf_err = MZF_ERROR_IO;
    } else if ( mzf->body != NULL && mzf->header.fsize > 0 ) {
        if ( mzf_write_body_on_offset ( h_out, write_offset + MZF_HEADER_SIZE,
                                         mzf->body, mzf->header.fsize ) != EXIT_SUCCESS ) {
            mzf_err = MZF_ERROR_IO;
        }
    }

    generic_driver_close ( h_out );

    if ( mzf_err != MZF_OK ) {
        fprintf ( stderr, "Error: failed to write MZF file '%s': %s\n",
                  output_path, mzf_error_string ( mzf_err ) );
        mzf_free ( mzf );
        return EXIT_FAILURE;
    }

    /* vypis souhrnu */
    char fname[MZF_FNAME_UTF8_BUF_SIZE];
    mzf_tools_get_fname_ex ( &mzf->header, fname, sizeof ( fname ), name_encoding );

    printf ( "  Block [%u] -> %s%s\n", block_index, output_path,
             ( write_mode == WRITE_MODE_APPEND && write_offset > 0 ) ? " (append)" : "" );
    printf ( "    Filename : \"%s\"\n", fname );
    printf ( "    Type     : 0x%02X (%s)\n", mzf->header.ftype, mzf_ftype_name ( mzf->header.ftype ) );
    printf ( "    Size     : %u bytes\n", mzf->header.fsize );
    printf ( "    Load addr: 0x%04X\n", mzf->header.fstrt );
    printf ( "    Exec addr: 0x%04X\n", mzf->header.fexec );
    printf ( "    MZF file : %u bytes (offset 0x%04X)\n",
             (unsigned) ( MZF_HEADER_SIZE + mzf->header.fsize ), write_offset );

    mzf_free ( mzf );
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
}


/**
 * @brief Vypise napovedu programu.
 * @param prog_name Nazev spusteneho programu (argv[0]).
 */
static void print_usage ( const char *prog_name ) {
    fprintf ( stderr, "Usage: %s <input.tmz|input.tzx> [options]\n\n", prog_name );
    fprintf ( stderr, "Extracts MZF files from TMZ/TZX tape archives.\n\n" );
    fprintf ( stderr, "Options:\n" );
    fprintf ( stderr, "  --output <file>       Output filename (default: derived from input)\n" );
    fprintf ( stderr, "  --index <N>           Extract only block at index N (0-based)\n" );
    fprintf ( stderr, "  --list                List extractable blocks without extracting\n" );
    fprintf ( stderr, "  --overwrite           Overwrite existing output file(s)\n" );
    fprintf ( stderr, "  --append              Append to existing output file (multi-MZF)\n" );
    fprintf ( stderr, "  --name-encoding <enc> Filename encoding: ascii, utf8-eu, utf8-jp (default: ascii)\n" );
    fprintf ( stderr, "  --version             Show program version\n" );
    fprintf ( stderr, "  --lib-versions        Show library versions\n" );
}


/**
 * @brief Hlavni funkce - extrakce MZF z TMZ/TZX.
 *
 * Zpracuje argumenty prikazove radky, nacte TMZ/TZX soubor,
 * najde bloky s MZF daty a extrahuje je do souboru.
 *
 * Rezim --list pouze vypise prehled extrahovatelnych bloku.
 * Rezim --index extrahuje konkretni blok. Bez --index extrahuje vsechny.
 *
 * Ve vychozim rezimu odmitne prepsat existujici soubor.
 * --overwrite povoli prepis, --append pripoji na konec existujiciho souboru.
 *
 * V rezimu --append se vsechny bloky zapisuji do jednoho souboru
 * (multi-MZF/MZT format - zretezene MZF zaznamy).
 *
 * Vystupy se pojmenovavaji takto:
 * - Jeden blok s --output: pouzije se presny nazev.
 * - Jeden blok bez --output: odvozeno ze vstupu (input.tmz -> input.mzf).
 * - Vice bloku s --output: output_001.mzf, output_002.mzf, ...
 * - Vice bloku bez --output: input_001.mzf, input_002.mzf, ...
 * - --append: vsechny bloky do jednoho souboru (--output nebo odvozeno).
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
    int selected_index = -1;   /* -1 = vsechny */
    bool list_only = false;
    en_WRITE_MODE write_mode = WRITE_MODE_DEFAULT;

    /* parsovani argumentu */
    int positional = 0;
    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp ( argv[i], "--version" ) == 0 ) {
            printf ( "tmz2mzf %s\n", TMZ2MZF_VERSION );
            return EXIT_SUCCESS;
        } else if ( strcmp ( argv[i], "--lib-versions" ) == 0 ) {
            print_lib_versions ();
            return EXIT_SUCCESS;
        } else if ( strcmp ( argv[i], "--output" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --output requires a value\n" );
                return EXIT_FAILURE;
            }
            output_file = argv[i];
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
        } else if ( strcmp ( argv[i], "--overwrite" ) == 0 ) {
            write_mode = WRITE_MODE_OVERWRITE;
        } else if ( strcmp ( argv[i], "--append" ) == 0 ) {
            write_mode = WRITE_MODE_APPEND;
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

    /* spocitat extrahovatelne bloky */
    uint32_t extractable_count = 0;
    for ( uint32_t i = 0; i < file->block_count; i++ ) {
        if ( is_extractable ( file->blocks[i].id ) ) {
            extractable_count++;
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
        if ( !is_extractable ( file->blocks[selected_index].id ) ) {
            fprintf ( stderr, "Error: block [%d] is 0x%02X (%s), not an extractable MZF block\n",
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

        if ( extractable_count == 0 ) {
            printf ( "No extractable MZF blocks found.\n" );
        } else {
            printf ( "Extractable blocks (%u):\n\n", extractable_count );
            for ( uint32_t i = 0; i < file->block_count; i++ ) {
                if ( !is_extractable ( file->blocks[i].id ) ) continue;

                /* kopie bloku pro bezpecne parsovani */
                st_TZX_BLOCK copy;
                uint8_t *data_copy;
                if ( !copy_block ( &file->blocks[i], &copy, &data_copy ) ) continue;

                /* parsovani pro ziskani MZF hlavicky */
                en_TZX_ERROR parse_err;
                const st_MZF_HEADER *hdr = NULL;
                uint16_t body_size = 0;

                if ( copy.id == TMZ_BLOCK_ID_MZ_STANDARD_DATA ) {
                    uint8_t *body;
                    st_TMZ_MZ_STANDARD_DATA *d = tmz_block_parse_mz_standard ( &copy, &body, &parse_err );
                    if ( d ) {
                        hdr = &d->mzf_header;
                        body_size = d->body_size;
                    }
                } else if ( copy.id == TMZ_BLOCK_ID_MZ_TURBO_DATA ) {
                    uint8_t *body;
                    st_TMZ_MZ_TURBO_DATA *d = tmz_block_parse_mz_turbo ( &copy, &body, &parse_err );
                    if ( d ) {
                        hdr = &d->mzf_header;
                        body_size = d->body_size;
                    }
                }

                if ( hdr ) {
                    char fname[MZF_FNAME_UTF8_BUF_SIZE];
                    mzf_tools_get_fname_ex ( hdr, fname, sizeof ( fname ), name_encoding );

                    printf ( "  [%3u] 0x%02X %-22s  \"%s\"  type=0x%02X  size=%u  load=0x%04X  exec=0x%04X\n",
                             i, file->blocks[i].id,
                             tmz_block_id_name ( file->blocks[i].id ),
                             fname, hdr->ftype, body_size, hdr->fstrt, hdr->fexec );
                }

                free ( data_copy );
            }
        }

        tzx_free ( file );
        return EXIT_SUCCESS;
    }

    /* extrakce */
    if ( extractable_count == 0 && selected_index < 0 ) {
        fprintf ( stderr, "Error: no extractable MZF blocks found in '%s'\n", input_file );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    /* bazovy nazev pro vystup */
    char basename[MAX_PATH_LENGTH / 2];
    if ( output_file ) {
        extract_basename ( output_file, basename, sizeof ( basename ) - 16 );
    } else {
        extract_basename ( input_file, basename, sizeof ( basename ) - 16 );
    }

    printf ( "Extracting from: %s\n", input_file );
    if ( write_mode == WRITE_MODE_APPEND ) {
        printf ( "Mode: append (multi-MZF)\n" );
    } else if ( write_mode == WRITE_MODE_OVERWRITE ) {
        printf ( "Mode: overwrite\n" );
    }
    printf ( "\n" );

    int result = EXIT_SUCCESS;

    /* pocet bloku k extrakci (pro rozhodnuti o cislovani) */
    uint32_t target_count = ( selected_index >= 0 ) ? 1 : extractable_count;

    /*
     * V append rezimu vsechny bloky jdou do jednoho souboru.
     * V ostatnich rezimech kazdy blok do samostatneho souboru (s cisly pri vice blocich).
     */
    bool single_output = ( write_mode == WRITE_MODE_APPEND ) ||
                          ( target_count == 1 );

    /* extrahovat vybrany blok nebo vsechny */
    uint32_t extracted = 0;
    for ( uint32_t i = 0; i < file->block_count; i++ ) {

        if ( selected_index >= 0 ) {
            /* extrakce konkretniho bloku */
            if ( (uint32_t) selected_index != i ) continue;
        } else {
            /* extrakce vsech extrahovatelnych bloku */
            if ( !is_extractable ( file->blocks[i].id ) ) continue;
        }

        /* sestavit vystupni cestu */
        char output_path[MAX_PATH_LENGTH];
        if ( single_output ) {
            /* jediny soubor - pouzit presny nazev nebo odvozit ze vstupu */
            if ( output_file ) {
                strncpy ( output_path, output_file, sizeof ( output_path ) - 1 );
                output_path[sizeof ( output_path ) - 1] = '\0';
            } else {
                snprintf ( output_path, sizeof ( output_path ), "%s.mzf", basename );
            }
        } else {
            /* vice bloku do samostatnych souboru - pridavat cisla */
            snprintf ( output_path, sizeof ( output_path ), "%s_%03u.mzf",
                       basename, extracted + 1 );
        }

        /*
         * V append rezimu: prvni blok pouziva APPEND (pripoji na konec),
         * dalsi bloky taky APPEND (kazdy se pripoji za predchozi).
         *
         * V overwrite rezimu: prvni blok pouziva OVERWRITE,
         * pri vice souborech kazdy ma svuj soubor.
         *
         * V append rezimu s vice bloky do jednoho souboru:
         * prvni blok muze prepsat (pokud soubor neexistuje, vytvori novy),
         * dalsi se pripoji.
         */
        en_WRITE_MODE block_write_mode = write_mode;

        if ( extract_and_save ( &file->blocks[i], i, output_path, block_write_mode ) != EXIT_SUCCESS ) {
            result = EXIT_FAILURE;
        }

        extracted++;
        if ( selected_index >= 0 ) break;
    }

    printf ( "\nExtracted %u MZF file(s).\n", extracted );

    tzx_free ( file );
    return result;
}
