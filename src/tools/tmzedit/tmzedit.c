/**
 * @file   tmzedit.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Editor a spravce TZX/TMZ tape souboru.
 *
 * Nastroj pro manipulaci s bloky v TZX/TMZ souborech:
 * pridavani, odebirani, presun, spojovani, rozdelovani,
 * editace metadat a validace integrity.
 *
 * @par Pouziti:
 * @code
 *   tmzedit list     <file>
 *   tmzedit dump     <file> <index>
 *   tmzedit remove   <file> <index> -o <output>
 *   tmzedit move     <file> <from> <to> -o <output>
 *   tmzedit merge    <file1> <file2> ... -o <output>
 *   tmzedit split    <file> [-o <prefix>]
 *   tmzedit add-text <file> --text "..." -o <output>
 *   tmzedit add-message <file> --text "..." [--time N] -o <output>
 *   tmzedit archive-info <file> [--title X] [--author X] [--year X] [--publisher X] [--comment X] -o <output>
 *   tmzedit set      <file> <index> [--format <fmt>] [--speed <spd>] -o <output>
 *   tmzedit validate <file>
 * @endcode
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
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tzx/tzx.h"
#include "libs/mzf/mzf.h"
#include "libs/mzf/mzf_tools.h"
#include "libs/cmtspeed/cmtspeed.h"
#include "libs/endianity/endianity.h"


/** @brief Kodovani nazvu souboru pro zobrazeni (file-level, nastaveno z --name-encoding). */
static en_MZF_NAME_ENCODING name_encoding = MZF_NAME_ASCII;


/* ========================================================================= */
/*  Pomocne funkce                                                           */
/* ========================================================================= */


/**
 * @brief Nacte TZX/TMZ soubor z cesty.
 * @param filename Cesta k souboru.
 * @param err Vystupni chybovy kod.
 * @return Nacteny soubor, nebo NULL pri chybe (hlaseni na stderr).
 */
static st_TZX_FILE* load_file ( const char *filename, en_TZX_ERROR *err ) {

    st_HANDLER handler;
    st_DRIVER driver;
    generic_driver_file_init ( &driver );

    st_HANDLER *h = generic_driver_open_file ( &handler, &driver,
                                                (char*) filename, FILE_DRIVER_OPMODE_RO );
    if ( !h ) {
        fprintf ( stderr, "Error: cannot open file '%s'\n", filename );
        return NULL;
    }

    st_TZX_FILE *file = tzx_load ( h, err );
    generic_driver_close ( h );

    if ( !file ) {
        fprintf ( stderr, "Error: %s\n", tzx_error_string ( *err ) );
    }
    return file;
}


/**
 * @brief Ulozi TZX/TMZ soubor do cesty.
 * @param filename Cesta k vystupnimu souboru.
 * @param file TZX data k ulozeni.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int save_file ( const char *filename, const st_TZX_FILE *file ) {

    st_HANDLER handler;
    st_DRIVER driver;
    generic_driver_file_init ( &driver );

    st_HANDLER *h = generic_driver_open_file ( &handler, &driver,
                                                (char*) filename, FILE_DRIVER_OPMODE_W );
    if ( !h ) {
        fprintf ( stderr, "Error: cannot create output file '%s'\n", filename );
        return EXIT_FAILURE;
    }

    en_TZX_ERROR err = tzx_save ( h, file );
    generic_driver_close ( h );

    if ( err != TZX_OK ) {
        fprintf ( stderr, "Error: %s\n", tzx_error_string ( err ) );
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


/**
 * @brief Parsuje index z retezce.
 * @param str Textova reprezentace cisla.
 * @param[out] value Vystupni hodnota.
 * @return true pri uspechu, false pri chybe.
 */
static bool parse_uint32 ( const char *str, uint32_t *value ) {
    char *end;
    unsigned long v = strtoul ( str, &end, 10 );
    if ( end == str || *end != '\0' ) return false;
    *value = (uint32_t) v;
    return true;
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
        default: return "Unknown";
    }
}


/**
 * @brief Vrati textovy nazev typu Archive Info zaznamu.
 * @param id ID textu dle TZX specifikace.
 * @return Staticky retezec s nazvem.
 */
static const char* archive_text_id_name ( uint8_t id ) {
    switch ( id ) {
        case 0x00: return "Title";
        case 0x01: return "Publisher";
        case 0x02: return "Author(s)";
        case 0x03: return "Year";
        case 0x04: return "Language";
        case 0x05: return "Type";
        case 0x06: return "Price";
        case 0x07: return "Protection";
        case 0x08: return "Origin";
        case 0xFF: return "Comment";
        default:   return "Unknown";
    }
}


/* ========================================================================= */
/*  Subcommand: list                                                         */
/* ========================================================================= */


/**
 * @brief Vypise strucny prehled vsech bloku v souboru.
 *
 * Kazdy blok zobrazuje index, ID, nazev, velikost dat
 * a zakladni info (nazev programu pro MZ bloky, text
 * pro informacni bloky apod.)
 *
 * @param argc Pocet argumentu (za subcommand).
 * @param argv Argumenty.
 * @return EXIT_SUCCESS nebo EXIT_FAILURE.
 */
static int cmd_list ( int argc, char *argv[] ) {

    if ( argc < 1 ) {
        fprintf ( stderr, "Usage: tmzedit list [--name-encoding <enc>] <file>\n" );
        return EXIT_FAILURE;
    }

    /* parsovani voleb specifickych pro list */
    const char *input_file = NULL;
    for ( int i = 0; i < argc; i++ ) {
        if ( strcmp ( argv[i], "--name-encoding" ) == 0 ) {
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
        } else if ( argv[i][0] != '-' ) {
            input_file = argv[i];
        }
    }

    if ( !input_file ) {
        fprintf ( stderr, "Usage: tmzedit list [--name-encoding <enc>] <file>\n" );
        return EXIT_FAILURE;
    }

    en_TZX_ERROR err;
    st_TZX_FILE *file = load_file ( input_file, &err );
    if ( !file ) return EXIT_FAILURE;

    printf ( "File: %s\n", input_file );
    printf ( "Type: %s, version %u.%u, %u block(s)\n\n",
             file->is_tmz ? "TMZ (TapeMZ!)" : "TZX (ZXTape!)",
             file->header.ver_major, file->header.ver_minor,
             file->block_count );

    for ( uint32_t i = 0; i < file->block_count; i++ ) {
        const st_TZX_BLOCK *b = &file->blocks[i];
        bool is_mz = tmz_block_is_mz_extension ( b->id );

        printf ( "  [%3u] 0x%02X  %-25s  %6u B%s",
                 i, b->id, tmz_block_id_name ( b->id ),
                 b->length, is_mz ? "  [MZ]" : "" );

        /* doplnkova informace pro bezne bloky */
        switch ( b->id ) {

            case TMZ_BLOCK_ID_MZ_STANDARD_DATA:
            case TMZ_BLOCK_ID_MZ_TURBO_DATA:
            {
                /* zobrazime nazev programu z MZF hlavicky */
                uint8_t *copy = malloc ( b->length );
                if ( copy ) {
                    memcpy ( copy, b->data, b->length );
                    st_TZX_BLOCK bc = *b;
                    bc.data = copy;
                    uint8_t *body;
                    if ( b->id == TMZ_BLOCK_ID_MZ_STANDARD_DATA ) {
                        st_TMZ_MZ_STANDARD_DATA *d = tmz_block_parse_mz_standard ( &bc, &body, &err );
                        if ( d ) {
                            char fname[MZF_FNAME_UTF8_BUF_SIZE];
                            mzf_tools_get_fname_ex ( &d->mzf_header, fname, sizeof ( fname ), name_encoding );
                            printf ( "  \"%s\"", fname );
                        }
                    } else {
                        st_TMZ_MZ_TURBO_DATA *d = tmz_block_parse_mz_turbo ( &bc, &body, &err );
                        if ( d ) {
                            char fname[MZF_FNAME_UTF8_BUF_SIZE];
                            mzf_tools_get_fname_ex ( &d->mzf_header, fname, sizeof ( fname ), name_encoding );
                            printf ( "  \"%s\"", fname );
                        }
                    }
                    free ( copy );
                }
                break;
            }

            case TZX_BLOCK_ID_STANDARD_SPEED:
                if ( b->length >= 5 ) {
                    uint16_t dlen = b->data[2] | ( b->data[3] << 8 );
                    uint8_t flag = b->data[4];
                    if ( flag == 0x00 && dlen >= 18 ) {
                        /* ZX TAP header: extrakce jmena programu */
                        char zxname[11];
                        memcpy ( zxname, &b->data[6], 10 );
                        zxname[10] = '\0';
                        for ( int j = 9; j >= 0 && zxname[j] == ' '; j-- ) zxname[j] = '\0';
                        printf ( "  \"%s\" (%u B, header)", zxname, dlen );
                    } else {
                        printf ( "  %u B, flag=0x%02X (%s)", dlen, flag,
                                 flag < 128 ? "header" : "data" );
                    }
                }
                break;

            case TZX_BLOCK_ID_PAUSE:
                if ( b->length >= 2 ) {
                    uint16_t p = b->data[0] | ( b->data[1] << 8 );
                    printf ( "  %u ms%s", p, p == 0 ? " (STOP)" : "" );
                }
                break;

            case TZX_BLOCK_ID_GROUP_START:
                if ( b->length >= 1 ) {
                    uint8_t len = b->data[0];
                    if ( 1 + (uint32_t) len <= b->length ) {
                        printf ( "  \"%.*s\"", len, (const char*) ( b->data + 1 ) );
                    }
                }
                break;

            case TZX_BLOCK_ID_TEXT_DESCRIPTION:
                if ( b->length >= 1 ) {
                    uint8_t len = b->data[0];
                    if ( 1 + (uint32_t) len <= b->length ) {
                        int show = len > 50 ? 50 : len;
                        printf ( "  \"%.*s%s\"", show, (const char*) ( b->data + 1 ),
                                 len > 50 ? "..." : "" );
                    }
                }
                break;

            case TZX_BLOCK_ID_ARCHIVE_INFO:
                if ( b->length >= 3 ) {
                    uint8_t cnt = b->data[2];
                    printf ( "  (%u entries)", cnt );
                }
                break;

            default:
                break;
        }

        printf ( "\n" );
    }

    printf ( "\nTotal: %u block(s)\n", file->block_count );
    tzx_free ( file );
    return EXIT_SUCCESS;
}


/* ========================================================================= */
/*  Subcommand: dump                                                         */
/* ========================================================================= */


/**
 * @brief Vypise hex dump dat bloku na danem indexu.
 *
 * Format: 16 bajtu na radek, offset | hex | ASCII.
 *
 * @param argc Pocet argumentu (za subcommand).
 * @param argv Argumenty.
 * @return EXIT_SUCCESS nebo EXIT_FAILURE.
 */
static int cmd_dump ( int argc, char *argv[] ) {

    if ( argc < 2 ) {
        fprintf ( stderr, "Usage: tmzedit dump <file> <index>\n" );
        return EXIT_FAILURE;
    }

    uint32_t index;
    if ( !parse_uint32 ( argv[1], &index ) ) {
        fprintf ( stderr, "Error: invalid block index '%s'\n", argv[1] );
        return EXIT_FAILURE;
    }

    en_TZX_ERROR err;
    st_TZX_FILE *file = load_file ( argv[0], &err );
    if ( !file ) return EXIT_FAILURE;

    if ( index >= file->block_count ) {
        fprintf ( stderr, "Error: block index %u out of range (0-%u)\n",
                  index, file->block_count - 1 );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    const st_TZX_BLOCK *b = &file->blocks[index];
    printf ( "Block [%u]: ID 0x%02X (%s), %u bytes\n\n",
             index, b->id, tmz_block_id_name ( b->id ), b->length );

    if ( b->length == 0 || !b->data ) {
        printf ( "  (no data)\n" );
        tzx_free ( file );
        return EXIT_SUCCESS;
    }

    /* hex dump: 16 bajtu na radek */
    for ( uint32_t off = 0; off < b->length; off += 16 ) {
        printf ( "  %04X  ", off );

        /* hex cast */
        for ( uint32_t j = 0; j < 16; j++ ) {
            if ( off + j < b->length ) {
                printf ( "%02X ", b->data[off + j] );
            } else {
                printf ( "   " );
            }
            if ( j == 7 ) printf ( " " );
        }

        /* ASCII cast */
        printf ( " |" );
        for ( uint32_t j = 0; j < 16 && off + j < b->length; j++ ) {
            uint8_t c = b->data[off + j];
            printf ( "%c", ( c >= 0x20 && c <= 0x7E ) ? c : '.' );
        }
        printf ( "|\n" );
    }

    tzx_free ( file );
    return EXIT_SUCCESS;
}


/* ========================================================================= */
/*  Subcommand: remove                                                       */
/* ========================================================================= */


/**
 * @brief Odebere blok na danem indexu a ulozi vysledek.
 *
 * @param argc Pocet argumentu (za subcommand).
 * @param argv Argumenty.
 * @return EXIT_SUCCESS nebo EXIT_FAILURE.
 */
static int cmd_remove ( int argc, char *argv[] ) {

    const char *output = NULL;
    const char *input = NULL;
    const char *idx_str = NULL;

    /* parsovani argumentu */
    for ( int i = 0; i < argc; i++ ) {
        if ( strcmp ( argv[i], "-o" ) == 0 && i + 1 < argc ) {
            output = argv[++i];
        } else if ( !input ) {
            input = argv[i];
        } else if ( !idx_str ) {
            idx_str = argv[i];
        }
    }

    if ( !input || !idx_str ) {
        fprintf ( stderr, "Usage: tmzedit remove <file> <index> -o <output>\n" );
        return EXIT_FAILURE;
    }

    uint32_t index;
    if ( !parse_uint32 ( idx_str, &index ) ) {
        fprintf ( stderr, "Error: invalid block index '%s'\n", idx_str );
        return EXIT_FAILURE;
    }

    /* pokud neni -o, pouzijeme vstupni soubor (in-place) */
    if ( !output ) output = input;

    en_TZX_ERROR err;
    st_TZX_FILE *file = load_file ( input, &err );
    if ( !file ) return EXIT_FAILURE;

    if ( index >= file->block_count ) {
        fprintf ( stderr, "Error: block index %u out of range (0-%u)\n",
                  index, file->block_count - 1 );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    printf ( "Removing block [%u]: 0x%02X (%s)\n",
             index, file->blocks[index].id,
             tmz_block_id_name ( file->blocks[index].id ) );

    err = tzx_file_remove_block ( file, index );
    if ( err != TZX_OK ) {
        fprintf ( stderr, "Error: %s\n", tzx_error_string ( err ) );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    int ret = save_file ( output, file );
    if ( ret == EXIT_SUCCESS ) {
        printf ( "Saved: %s (%u blocks)\n", output, file->block_count );
    }
    tzx_free ( file );
    return ret;
}


/* ========================================================================= */
/*  Subcommand: move                                                         */
/* ========================================================================= */


/**
 * @brief Presune blok z pozice from na pozici to.
 *
 * @param argc Pocet argumentu (za subcommand).
 * @param argv Argumenty.
 * @return EXIT_SUCCESS nebo EXIT_FAILURE.
 */
static int cmd_move ( int argc, char *argv[] ) {

    const char *output = NULL;
    const char *input = NULL;
    const char *from_str = NULL;
    const char *to_str = NULL;

    for ( int i = 0; i < argc; i++ ) {
        if ( strcmp ( argv[i], "-o" ) == 0 && i + 1 < argc ) {
            output = argv[++i];
        } else if ( !input ) {
            input = argv[i];
        } else if ( !from_str ) {
            from_str = argv[i];
        } else if ( !to_str ) {
            to_str = argv[i];
        }
    }

    if ( !input || !from_str || !to_str ) {
        fprintf ( stderr, "Usage: tmzedit move <file> <from> <to> -o <output>\n" );
        return EXIT_FAILURE;
    }

    uint32_t from, to;
    if ( !parse_uint32 ( from_str, &from ) || !parse_uint32 ( to_str, &to ) ) {
        fprintf ( stderr, "Error: invalid index\n" );
        return EXIT_FAILURE;
    }

    if ( !output ) output = input;

    en_TZX_ERROR err;
    st_TZX_FILE *file = load_file ( input, &err );
    if ( !file ) return EXIT_FAILURE;

    if ( from >= file->block_count || to >= file->block_count ) {
        fprintf ( stderr, "Error: index out of range (0-%u)\n", file->block_count - 1 );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    printf ( "Moving block [%u] -> [%u]: 0x%02X (%s)\n",
             from, to, file->blocks[from].id,
             tmz_block_id_name ( file->blocks[from].id ) );

    err = tzx_file_move_block ( file, from, to );
    if ( err != TZX_OK ) {
        fprintf ( stderr, "Error: %s\n", tzx_error_string ( err ) );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    int ret = save_file ( output, file );
    if ( ret == EXIT_SUCCESS ) {
        printf ( "Saved: %s (%u blocks)\n", output, file->block_count );
    }
    tzx_free ( file );
    return ret;
}


/* ========================================================================= */
/*  Subcommand: merge                                                        */
/* ========================================================================= */


/**
 * @brief Spoji vice TZX/TMZ souboru do jednoho.
 *
 * Bloky vsech vstupnich souboru se spoji v zadanem poradi.
 * Hlavicka vystupu odpovida prvnimu souboru.
 *
 * @param argc Pocet argumentu (za subcommand).
 * @param argv Argumenty.
 * @return EXIT_SUCCESS nebo EXIT_FAILURE.
 */
static int cmd_merge ( int argc, char *argv[] ) {

    const char *output = NULL;

    /* najdi -o */
    int file_count = 0;
    for ( int i = 0; i < argc; i++ ) {
        if ( strcmp ( argv[i], "-o" ) == 0 && i + 1 < argc ) {
            output = argv[++i];
        } else {
            file_count++;
        }
    }

    if ( file_count < 2 || !output ) {
        fprintf ( stderr, "Usage: tmzedit merge <file1> <file2> ... -o <output>\n" );
        return EXIT_FAILURE;
    }

    /* nacteni prvniho souboru jako zaklad */
    en_TZX_ERROR err;
    st_TZX_FILE *result = NULL;
    int files_merged = 0;

    for ( int i = 0; i < argc; i++ ) {
        if ( strcmp ( argv[i], "-o" ) == 0 ) {
            i++;  /* preskocit hodnotu -o */
            continue;
        }

        st_TZX_FILE *f = load_file ( argv[i], &err );
        if ( !f ) {
            if ( result ) tzx_free ( result );
            return EXIT_FAILURE;
        }

        if ( !result ) {
            result = f;
            printf ( "Base: %s (%u blocks)\n", argv[i], f->block_count );
        } else {
            printf ( "Merge: %s (%u blocks)\n", argv[i], f->block_count );
            err = tzx_file_merge ( result, f );
            tzx_free ( f );
            if ( err != TZX_OK ) {
                fprintf ( stderr, "Error: %s\n", tzx_error_string ( err ) );
                tzx_free ( result );
                return EXIT_FAILURE;
            }
        }
        files_merged++;
    }

    int ret = save_file ( output, result );
    if ( ret == EXIT_SUCCESS ) {
        printf ( "Saved: %s (%u blocks from %d files)\n",
                 output, result->block_count, files_merged );
    }
    tzx_free ( result );
    return ret;
}


/* ========================================================================= */
/*  Subcommand: split                                                        */
/* ========================================================================= */


/**
 * @brief Rozdeli tape soubor na jednotlive programy.
 *
 * Rozdeluje podle Group Start (0x21) / Group End (0x22) bloku.
 * Pokud skupiny neexistuji, kazdy datovy blok je samostatny soubor.
 * Vystupni soubory: <prefix>_000.tzx, <prefix>_001.tzx, ...
 *
 * @param argc Pocet argumentu (za subcommand).
 * @param argv Argumenty.
 * @return EXIT_SUCCESS nebo EXIT_FAILURE.
 */
static int cmd_split ( int argc, char *argv[] ) {

    const char *input = NULL;
    const char *prefix = NULL;

    for ( int i = 0; i < argc; i++ ) {
        if ( strcmp ( argv[i], "-o" ) == 0 && i + 1 < argc ) {
            prefix = argv[++i];
        } else if ( !input ) {
            input = argv[i];
        }
    }

    if ( !input ) {
        fprintf ( stderr, "Usage: tmzedit split <file> [-o <prefix>]\n" );
        return EXIT_FAILURE;
    }

    /* vychozi prefix = vstupni soubor bez pripony */
    char default_prefix[256];
    if ( !prefix ) {
        strncpy ( default_prefix, input, sizeof ( default_prefix ) - 1 );
        default_prefix[sizeof ( default_prefix ) - 1] = '\0';
        char *dot = strrchr ( default_prefix, '.' );
        if ( dot ) *dot = '\0';
        prefix = default_prefix;
    }

    en_TZX_ERROR err;
    st_TZX_FILE *file = load_file ( input, &err );
    if ( !file ) return EXIT_FAILURE;

    /* detekce skupin */
    bool has_groups = false;
    for ( uint32_t i = 0; i < file->block_count; i++ ) {
        if ( file->blocks[i].id == TZX_BLOCK_ID_GROUP_START ) {
            has_groups = true;
            break;
        }
    }

    int part = 0;
    uint32_t start = 0;

    if ( has_groups ) {
        /* rozdeleni podle Group Start / Group End */
        uint32_t i = 0;
        while ( i < file->block_count ) {

            if ( file->blocks[i].id != TZX_BLOCK_ID_GROUP_START ) {
                i++;
                continue;
            }

            /* najdi odpovidajici Group End */
            start = i;
            uint32_t end = i + 1;
            while ( end < file->block_count &&
                    file->blocks[end].id != TZX_BLOCK_ID_GROUP_END ) {
                end++;
            }
            if ( end < file->block_count ) end++; /* vcetne Group End */

            /* vytvorit novy soubor s bloky start..end-1 */
            st_TZX_FILE out = { 0 };
            out.header = file->header;
            out.is_tmz = file->is_tmz;

            /* docasne nastavime bloky primo (bez kopirovani dat) */
            out.blocks = &file->blocks[start];
            out.block_count = end - start;

            char outname[300];
            snprintf ( outname, sizeof ( outname ), "%s_%03d.%s",
                       prefix, part, file->is_tmz ? "tmz" : "tzx" );

            int ret = save_file ( outname, &out );
            if ( ret != EXIT_SUCCESS ) {
                tzx_free ( file );
                return EXIT_FAILURE;
            }

            printf ( "  Part %d: blocks [%u-%u] -> %s\n",
                     part, start, end - 1, outname );
            part++;
            i = end;
        }
    } else {
        /* bez skupin: kazdy audio/datovy blok je samostatny */
        for ( uint32_t i = 0; i < file->block_count; i++ ) {
            uint8_t id = file->blocks[i].id;

            /* preskocit ciste informacni a ridici bloky */
            if ( id == TZX_BLOCK_ID_TEXT_DESCRIPTION ||
                 id == TZX_BLOCK_ID_MESSAGE ||
                 id == TZX_BLOCK_ID_ARCHIVE_INFO ||
                 id == TZX_BLOCK_ID_HARDWARE_TYPE ||
                 id == TZX_BLOCK_ID_CUSTOM_INFO ||
                 id == TZX_BLOCK_ID_GLUE ) continue;

            st_TZX_FILE out = { 0 };
            out.header = file->header;
            out.is_tmz = file->is_tmz;
            out.blocks = &file->blocks[i];
            out.block_count = 1;

            char outname[300];
            snprintf ( outname, sizeof ( outname ), "%s_%03d.%s",
                       prefix, part, file->is_tmz ? "tmz" : "tzx" );

            int ret = save_file ( outname, &out );
            if ( ret != EXIT_SUCCESS ) {
                tzx_free ( file );
                return EXIT_FAILURE;
            }

            printf ( "  Part %d: block [%u] 0x%02X (%s) -> %s\n",
                     part, i, id, tmz_block_id_name ( id ), outname );
            part++;
        }
    }

    printf ( "Split into %d part(s)\n", part );
    tzx_free ( file );
    return EXIT_SUCCESS;
}


/* ========================================================================= */
/*  Subcommand: add-text                                                     */
/* ========================================================================= */


/**
 * @brief Prida blok 0x30 (Text Description) do souboru.
 *
 * @param argc Pocet argumentu (za subcommand).
 * @param argv Argumenty.
 * @return EXIT_SUCCESS nebo EXIT_FAILURE.
 */
static int cmd_add_text ( int argc, char *argv[] ) {

    const char *input = NULL;
    const char *output = NULL;
    const char *text = NULL;

    for ( int i = 0; i < argc; i++ ) {
        if ( strcmp ( argv[i], "-o" ) == 0 && i + 1 < argc ) {
            output = argv[++i];
        } else if ( strcmp ( argv[i], "--text" ) == 0 && i + 1 < argc ) {
            text = argv[++i];
        } else if ( !input ) {
            input = argv[i];
        }
    }

    if ( !input || !text ) {
        fprintf ( stderr, "Usage: tmzedit add-text <file> --text \"...\" [-o <output>]\n" );
        return EXIT_FAILURE;
    }
    if ( !output ) output = input;

    en_TZX_ERROR err;
    st_TZX_FILE *file = load_file ( input, &err );
    if ( !file ) return EXIT_FAILURE;

    st_TZX_BLOCK block;
    err = tzx_block_create_text_description ( text, &block );
    if ( err != TZX_OK ) {
        fprintf ( stderr, "Error: %s\n", tzx_error_string ( err ) );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    err = tzx_file_append_block ( file, &block );
    if ( err != TZX_OK ) {
        fprintf ( stderr, "Error: %s\n", tzx_error_string ( err ) );
        free ( block.data );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    printf ( "Added Text Description: \"%s\"\n", text );

    int ret = save_file ( output, file );
    if ( ret == EXIT_SUCCESS ) {
        printf ( "Saved: %s (%u blocks)\n", output, file->block_count );
    }
    tzx_free ( file );
    return ret;
}


/* ========================================================================= */
/*  Subcommand: add-message                                                  */
/* ========================================================================= */


/**
 * @brief Prida blok 0x31 (Message) do souboru.
 *
 * @param argc Pocet argumentu (za subcommand).
 * @param argv Argumenty.
 * @return EXIT_SUCCESS nebo EXIT_FAILURE.
 */
static int cmd_add_message ( int argc, char *argv[] ) {

    const char *input = NULL;
    const char *output = NULL;
    const char *text = NULL;
    uint8_t time_s = 5;

    for ( int i = 0; i < argc; i++ ) {
        if ( strcmp ( argv[i], "-o" ) == 0 && i + 1 < argc ) {
            output = argv[++i];
        } else if ( strcmp ( argv[i], "--text" ) == 0 && i + 1 < argc ) {
            text = argv[++i];
        } else if ( strcmp ( argv[i], "--time" ) == 0 && i + 1 < argc ) {
            uint32_t v;
            if ( parse_uint32 ( argv[i + 1], &v ) && v <= 255 ) {
                time_s = (uint8_t) v;
            }
            i++;
        } else if ( !input ) {
            input = argv[i];
        }
    }

    if ( !input || !text ) {
        fprintf ( stderr, "Usage: tmzedit add-message <file> --text \"...\" [--time N] [-o <output>]\n" );
        return EXIT_FAILURE;
    }
    if ( !output ) output = input;

    en_TZX_ERROR err;
    st_TZX_FILE *file = load_file ( input, &err );
    if ( !file ) return EXIT_FAILURE;

    st_TZX_BLOCK block;
    err = tzx_block_create_message ( text, time_s, &block );
    if ( err != TZX_OK ) {
        fprintf ( stderr, "Error: %s\n", tzx_error_string ( err ) );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    err = tzx_file_append_block ( file, &block );
    if ( err != TZX_OK ) {
        fprintf ( stderr, "Error: %s\n", tzx_error_string ( err ) );
        free ( block.data );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    printf ( "Added Message: \"%s\" (%u s)\n", text, time_s );

    int ret = save_file ( output, file );
    if ( ret == EXIT_SUCCESS ) {
        printf ( "Saved: %s (%u blocks)\n", output, file->block_count );
    }
    tzx_free ( file );
    return ret;
}


/* ========================================================================= */
/*  Subcommand: archive-info                                                 */
/* ========================================================================= */


/** @brief Maximalni pocet Archive Info zaznamu */
#define MAX_ARCHIVE_ENTRIES 16


/**
 * @brief Prida nebo nahradi blok 0x32 (Archive Info) v souboru.
 *
 * Pokud soubor uz obsahuje blok 0x32, novy ho nahradi.
 * Podporovane volby: --title, --publisher, --author, --year,
 * --language, --type, --price, --protection, --origin, --comment.
 *
 * @param argc Pocet argumentu (za subcommand).
 * @param argv Argumenty.
 * @return EXIT_SUCCESS nebo EXIT_FAILURE.
 */
static int cmd_archive_info ( int argc, char *argv[] ) {

    const char *input = NULL;
    const char *output = NULL;

    st_TZX_ARCHIVE_ENTRY entries[MAX_ARCHIVE_ENTRIES];
    uint8_t entry_count = 0;

    /* mapovani nazvu voleb na type_id */
    struct { const char *opt; uint8_t type_id; } field_map[] = {
        { "--title",      0x00 },
        { "--publisher",  0x01 },
        { "--author",     0x02 },
        { "--year",       0x03 },
        { "--language",   0x04 },
        { "--type",       0x05 },
        { "--price",      0x06 },
        { "--protection", 0x07 },
        { "--origin",     0x08 },
        { "--comment",    0xFF },
        { NULL, 0 }
    };

    for ( int i = 0; i < argc; i++ ) {
        if ( strcmp ( argv[i], "-o" ) == 0 && i + 1 < argc ) {
            output = argv[++i];
            continue;
        }

        /* zkusit matchovat --title, --author atd. */
        bool matched = false;
        for ( int f = 0; field_map[f].opt; f++ ) {
            if ( strcmp ( argv[i], field_map[f].opt ) == 0 && i + 1 < argc ) {
                if ( entry_count >= MAX_ARCHIVE_ENTRIES ) {
                    fprintf ( stderr, "Error: too many archive info entries\n" );
                    return EXIT_FAILURE;
                }
                entries[entry_count].type_id = field_map[f].type_id;
                entries[entry_count].text = argv[++i];
                entry_count++;
                matched = true;
                break;
            }
        }

        if ( !matched && argv[i][0] != '-' ) {
            if ( !input ) input = argv[i];
        }
    }

    if ( !input || entry_count == 0 ) {
        fprintf ( stderr, "Usage: tmzedit archive-info <file> [--title X] [--author X] "
                  "[--year X] [--publisher X] [--comment X] [-o <output>]\n" );
        return EXIT_FAILURE;
    }
    if ( !output ) output = input;

    en_TZX_ERROR err;
    st_TZX_FILE *file = load_file ( input, &err );
    if ( !file ) return EXIT_FAILURE;

    /* odebrat existujici Archive Info blok (0x32) */
    for ( uint32_t i = 0; i < file->block_count; i++ ) {
        if ( file->blocks[i].id == TZX_BLOCK_ID_ARCHIVE_INFO ) {
            printf ( "Replacing existing Archive Info block [%u]\n", i );
            tzx_file_remove_block ( file, i );
            break;
        }
    }

    /* vytvorit novy blok */
    st_TZX_BLOCK block;
    err = tzx_block_create_archive_info ( entries, entry_count, &block );
    if ( err != TZX_OK ) {
        fprintf ( stderr, "Error: %s\n", tzx_error_string ( err ) );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    /* vlozit na zacatek (za hlavicku = index 0) */
    err = tzx_file_insert_block ( file, 0, &block );
    if ( err != TZX_OK ) {
        fprintf ( stderr, "Error: %s\n", tzx_error_string ( err ) );
        free ( block.data );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    printf ( "Added Archive Info (%u entries):\n", entry_count );
    for ( uint8_t i = 0; i < entry_count; i++ ) {
        printf ( "  %-12s: \"%s\"\n",
                 archive_text_id_name ( entries[i].type_id ), entries[i].text );
    }

    int ret = save_file ( output, file );
    if ( ret == EXIT_SUCCESS ) {
        printf ( "Saved: %s (%u blocks)\n", output, file->block_count );
    }
    tzx_free ( file );
    return ret;
}


/* ========================================================================= */
/*  Subcommand: validate                                                     */
/* ========================================================================= */


/**
 * @brief Validuje integritu TZX/TMZ souboru.
 *
 * Kontroluje:
 * - Platnost signatury a verze
 * - Zname blokove ID
 * - Parovanost Group Start / Group End
 * - Parovanost Loop Start / Loop End
 * - MZF hlavicky v MZ blocich (ftype, fsize vs body_size)
 *
 * @param argc Pocet argumentu (za subcommand).
 * @param argv Argumenty.
 * @return EXIT_SUCCESS pokud je soubor validni, EXIT_FAILURE jinak.
 */
static int cmd_validate ( int argc, char *argv[] ) {

    if ( argc < 1 ) {
        fprintf ( stderr, "Usage: tmzedit validate <file>\n" );
        return EXIT_FAILURE;
    }

    en_TZX_ERROR err;
    st_TZX_FILE *file = load_file ( argv[0], &err );
    if ( !file ) return EXIT_FAILURE;

    int warnings = 0;
    int errors = 0;

    printf ( "Validating: %s\n", argv[0] );
    printf ( "  Type: %s, version %u.%u\n",
             file->is_tmz ? "TMZ" : "TZX",
             file->header.ver_major, file->header.ver_minor );

    /* kontrola verze */
    if ( !file->is_tmz && ( file->header.ver_major != 1 || file->header.ver_minor > 20 ) ) {
        printf ( "  WARNING: unusual TZX version %u.%u\n",
                 file->header.ver_major, file->header.ver_minor );
        warnings++;
    }

    /* kontrola bloku */
    int group_depth = 0;
    int loop_depth = 0;

    for ( uint32_t i = 0; i < file->block_count; i++ ) {
        const st_TZX_BLOCK *b = &file->blocks[i];

        /* neznamy blok? */
        const char *name = tmz_block_id_name ( b->id );
        if ( strcmp ( name, "Unknown" ) == 0 ) {
            printf ( "  WARNING: block [%u] unknown ID 0x%02X (%u bytes)\n",
                     i, b->id, b->length );
            warnings++;
        }

        /* group tracking */
        if ( b->id == TZX_BLOCK_ID_GROUP_START ) {
            group_depth++;
            if ( group_depth > 1 ) {
                printf ( "  ERROR: block [%u] nested Group Start (depth %d)\n",
                         i, group_depth );
                errors++;
            }
        } else if ( b->id == TZX_BLOCK_ID_GROUP_END ) {
            group_depth--;
            if ( group_depth < 0 ) {
                printf ( "  ERROR: block [%u] Group End without matching Group Start\n", i );
                errors++;
                group_depth = 0;
            }
        }

        /* loop tracking */
        if ( b->id == TZX_BLOCK_ID_LOOP_START ) {
            loop_depth++;
            if ( loop_depth > 1 ) {
                printf ( "  ERROR: block [%u] nested Loop Start (depth %d)\n",
                         i, loop_depth );
                errors++;
            }
        } else if ( b->id == TZX_BLOCK_ID_LOOP_END ) {
            loop_depth--;
            if ( loop_depth < 0 ) {
                printf ( "  ERROR: block [%u] Loop End without matching Loop Start\n", i );
                errors++;
                loop_depth = 0;
            }
        }

        /* MZ Standard Data (0x40) - validace MZF hlavicky */
        if ( b->id == TMZ_BLOCK_ID_MZ_STANDARD_DATA && b->length > 0 ) {
            uint8_t *copy = malloc ( b->length );
            if ( copy ) {
                memcpy ( copy, b->data, b->length );
                st_TZX_BLOCK bc = *b;
                bc.data = copy;
                uint8_t *body;
                st_TMZ_MZ_STANDARD_DATA *d = tmz_block_parse_mz_standard ( &bc, &body, &err );
                if ( !d ) {
                    printf ( "  ERROR: block [%u] MZ Standard Data parse failed\n", i );
                    errors++;
                } else {
                    if ( d->mzf_header.ftype == 0 || d->mzf_header.ftype > 0x05 ) {
                        printf ( "  WARNING: block [%u] unusual MZF ftype 0x%02X (%s)\n",
                                 i, d->mzf_header.ftype,
                                 mzf_ftype_name ( d->mzf_header.ftype ) );
                        warnings++;
                    }
                    if ( d->mzf_header.fsize != d->body_size ) {
                        printf ( "  WARNING: block [%u] MZF fsize=%u but body_size=%u\n",
                                 i, d->mzf_header.fsize, d->body_size );
                        warnings++;
                    }
                }
                free ( copy );
            }
        }
    }

    /* neuzavrene skupiny/smycky */
    if ( group_depth > 0 ) {
        printf ( "  ERROR: %d unclosed Group Start block(s)\n", group_depth );
        errors++;
    }
    if ( loop_depth > 0 ) {
        printf ( "  ERROR: %d unclosed Loop Start block(s)\n", loop_depth );
        errors++;
    }

    /* souhrn */
    printf ( "\n  Blocks: %u, Errors: %d, Warnings: %d\n", file->block_count, errors, warnings );
    if ( errors == 0 && warnings == 0 ) {
        printf ( "  Result: VALID\n" );
    } else if ( errors == 0 ) {
        printf ( "  Result: VALID (with warnings)\n" );
    } else {
        printf ( "  Result: INVALID\n" );
    }

    tzx_free ( file );
    return errors > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}


/* ========================================================================= */
/*  Subcommand: set                                                          */
/* ========================================================================= */


/**
 * @brief Naparsuje retezec na hodnotu en_TMZ_FORMAT.
 *
 * Rozpoznava: "normal", "turbo", "fastipl", "sinclair", "fsk",
 * "slow", "direct", "cpm-tape".
 *
 * @param str Vstupni retezec.
 * @param[out] format Vystupni hodnota.
 * @return 0 pri uspechu, -1 pokud retezec neodpovida.
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
 * @brief Naparsuje retezec na hodnotu en_CMTSPEED.
 *
 * Rozpoznava: "1:1", "2:1", "2:1cpm", "3:1", "3:2", "7:3",
 * "8:3", "9:7", "25:14".
 *
 * @param str Vstupni retezec.
 * @param[out] speed Vystupni hodnota.
 * @return 0 pri uspechu, -1 pokud retezec neodpovida.
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
 * @brief Zmeni format a/nebo rychlost na bloku 0x40 (MZ Standard Data).
 *
 * Pokud cilovy format je NORMAL a rychlost 1:1, blok zustane jako 0x40.
 * Jinak se prekonvertuje na blok 0x41 (MZ Turbo Data).
 *
 * @param file TZX soubor.
 * @param index Index bloku v souboru.
 * @param format Novy format (nebo -1 pro ponechani).
 * @param speed Nova rychlost (nebo -1 pro ponechani).
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int set_block_0x40 ( st_TZX_FILE *file, uint32_t index,
                             int format, int speed ) {
    st_TZX_BLOCK *b = &file->blocks[index];

    /* parsovani bloku do pracovni kopie */
    uint8_t *copy = malloc ( b->length );
    if ( !copy ) {
        fprintf ( stderr, "Error: memory allocation failed\n" );
        return EXIT_FAILURE;
    }
    memcpy ( copy, b->data, b->length );

    st_TZX_BLOCK bc = *b;
    bc.data = copy;
    uint8_t *body_data;
    en_TZX_ERROR err;
    st_TMZ_MZ_STANDARD_DATA *std = tmz_block_parse_mz_standard ( &bc, &body_data, &err );
    if ( !std ) {
        fprintf ( stderr, "Error: failed to parse block [%u]: %s\n",
                  index, tzx_error_string ( err ) );
        free ( copy );
        return EXIT_FAILURE;
    }

    en_TMZ_FORMAT new_format = ( format >= 0 ) ? ( en_TMZ_FORMAT ) format : TMZ_FORMAT_NORMAL;
    en_CMTSPEED new_speed = ( speed >= 0 ) ? ( en_CMTSPEED ) speed : CMTSPEED_1_1;

    if ( new_format == TMZ_FORMAT_NORMAL && new_speed == CMTSPEED_1_1 ) {
        /* neni co menit - blok 0x40 uz odpovida */
        printf ( "Block [%u]: already NORMAL 1:1 (0x40), no change needed\n", index );
        free ( copy );
        return EXIT_SUCCESS;
    }

    /* konverze na blok 0x41 */
    st_TMZ_MZ_TURBO_DATA params;
    memset ( &params, 0, sizeof ( params ) );
    params.machine = std->machine;
    params.pulseset = std->pulseset;
    params.format = ( uint8_t ) new_format;
    params.speed = ( uint8_t ) new_speed;
    params.pause_ms = std->pause_ms;
    params.flags = 0;
    memcpy ( &params.mzf_header, &std->mzf_header, sizeof ( st_MZF_HEADER ) );
    params.body_size = std->body_size;

    st_TZX_BLOCK *new_block = tmz_block_create_mz_turbo (
        &params, body_data, std->body_size );
    free ( copy );

    if ( !new_block ) {
        fprintf ( stderr, "Error: failed to create block 0x41\n" );
        return EXIT_FAILURE;
    }

    /* nahradit stary blok novym */
    free ( b->data );
    *b = *new_block;
    free ( new_block );

    printf ( "Block [%u]: converted 0x40 -> 0x41 (format=%s, speed=%s)\n",
             index, format_name ( new_format ), g_cmtspeed_ratio[new_speed] );

    return EXIT_SUCCESS;
}


/**
 * @brief Zmeni format a/nebo rychlost na bloku 0x41 (MZ Turbo Data).
 *
 * Modifikuje format a speed primo v binarnim obsahu bloku.
 * Pokud cilovy format je NORMAL a rychlost 1:1, blok se
 * prekonvertuje na 0x40 (MZ Standard Data).
 *
 * @param file TZX soubor.
 * @param index Index bloku v souboru.
 * @param format Novy format (nebo -1 pro ponechani).
 * @param speed Nova rychlost (nebo -1 pro ponechani).
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int set_block_0x41 ( st_TZX_FILE *file, uint32_t index,
                             int format, int speed ) {
    st_TZX_BLOCK *b = &file->blocks[index];

    /* parsovani bloku do pracovni kopie */
    uint8_t *copy = malloc ( b->length );
    if ( !copy ) {
        fprintf ( stderr, "Error: memory allocation failed\n" );
        return EXIT_FAILURE;
    }
    memcpy ( copy, b->data, b->length );

    st_TZX_BLOCK bc = *b;
    bc.data = copy;
    uint8_t *body_data;
    en_TZX_ERROR err;
    st_TMZ_MZ_TURBO_DATA *turbo = tmz_block_parse_mz_turbo ( &bc, &body_data, &err );
    if ( !turbo ) {
        fprintf ( stderr, "Error: failed to parse block [%u]: %s\n",
                  index, tzx_error_string ( err ) );
        free ( copy );
        return EXIT_FAILURE;
    }

    en_TMZ_FORMAT old_format = ( en_TMZ_FORMAT ) turbo->format;
    en_CMTSPEED old_speed = ( en_CMTSPEED ) turbo->speed;
    en_TMZ_FORMAT new_format = ( format >= 0 ) ? ( en_TMZ_FORMAT ) format : old_format;
    en_CMTSPEED new_speed = ( speed >= 0 ) ? ( en_CMTSPEED ) speed : old_speed;

    if ( new_format == TMZ_FORMAT_NORMAL && new_speed == CMTSPEED_1_1 ) {
        /* konverze zpet na blok 0x40 */
        st_MZF mzf;
        memcpy ( &mzf.header, &turbo->mzf_header, sizeof ( st_MZF_HEADER ) );
        mzf.body = body_data;
        mzf.body_size = turbo->body_size;

        st_TZX_BLOCK *new_block = tmz_block_from_mzf (
            &mzf, ( en_TMZ_MACHINE ) turbo->machine,
            ( en_TMZ_PULSESET ) turbo->pulseset, turbo->pause_ms );
        free ( copy );

        if ( !new_block ) {
            fprintf ( stderr, "Error: failed to create block 0x40\n" );
            return EXIT_FAILURE;
        }

        free ( b->data );
        *b = *new_block;
        free ( new_block );

        printf ( "Block [%u]: converted 0x41 -> 0x40 (NORMAL 1:1)\n", index );
        return EXIT_SUCCESS;
    }

    /* rekonstrukce bloku 0x41 s novymi parametry */
    st_TMZ_MZ_TURBO_DATA params;
    memset ( &params, 0, sizeof ( params ) );
    params.machine = turbo->machine;
    params.pulseset = turbo->pulseset;
    params.format = ( uint8_t ) new_format;
    params.speed = ( uint8_t ) new_speed;
    params.lgap_length = turbo->lgap_length;
    params.sgap_length = turbo->sgap_length;
    params.pause_ms = turbo->pause_ms;
    params.long_high = turbo->long_high;
    params.long_low = turbo->long_low;
    params.short_high = turbo->short_high;
    params.short_low = turbo->short_low;
    params.flags = turbo->flags;
    memcpy ( &params.mzf_header, &turbo->mzf_header, sizeof ( st_MZF_HEADER ) );
    params.body_size = turbo->body_size;

    st_TZX_BLOCK *new_block = tmz_block_create_mz_turbo (
        &params, body_data, turbo->body_size );
    free ( copy );

    if ( !new_block ) {
        fprintf ( stderr, "Error: failed to create block 0x41\n" );
        return EXIT_FAILURE;
    }

    free ( b->data );
    *b = *new_block;
    free ( new_block );

    printf ( "Block [%u]: 0x41 set format=%s speed=%s",
             index, format_name ( new_format ), g_cmtspeed_ratio[new_speed] );
    if ( new_format != old_format )
        printf ( " (format: %s -> %s)", format_name ( old_format ), format_name ( new_format ) );
    if ( new_speed != old_speed )
        printf ( " (speed: %s -> %s)", g_cmtspeed_ratio[old_speed], g_cmtspeed_ratio[new_speed] );
    printf ( "\n" );

    return EXIT_SUCCESS;
}


/**
 * @brief Zmeni format a/nebo rychlost na MZ bloku.
 *
 * Podporovane bloky:
 * - 0x40 (MZ Standard Data): --format a --speed. Pokud se nastavi
 *   nestandardni hodnoty, blok se konvertuje na 0x41.
 * - 0x41 (MZ Turbo Data): --format a --speed. Pokud se nastavi
 *   NORMAL 1:1, blok se konvertuje na 0x40.
 * - 0x45 (MZ BASIC Data): pouze --speed (format je vzdy NORMAL).
 *   Blok 0x45 nema pole speed - pokud je rychlost jina nez 1:1,
 *   vypise varovani.
 *
 * @param argc Pocet argumentu (za subcommand).
 * @param argv Argumenty.
 * @return EXIT_SUCCESS nebo EXIT_FAILURE.
 */
static int cmd_set ( int argc, char *argv[] ) {

    const char *input = NULL;
    const char *output = NULL;
    const char *idx_str = NULL;
    int format = -1;  /* -1 = nezmeneno */
    int speed = -1;

    for ( int i = 0; i < argc; i++ ) {
        if ( strcmp ( argv[i], "-o" ) == 0 && i + 1 < argc ) {
            output = argv[++i];
        } else if ( strcmp ( argv[i], "--format" ) == 0 && i + 1 < argc ) {
            en_TMZ_FORMAT f;
            if ( parse_format ( argv[++i], &f ) != 0 ) {
                fprintf ( stderr, "Error: unknown format '%s'\n", argv[i] );
                return EXIT_FAILURE;
            }
            format = ( int ) f;
        } else if ( strcmp ( argv[i], "--speed" ) == 0 && i + 1 < argc ) {
            en_CMTSPEED s;
            if ( parse_speed ( argv[++i], &s ) != 0 ) {
                fprintf ( stderr, "Error: unknown speed '%s'\n", argv[i] );
                return EXIT_FAILURE;
            }
            speed = ( int ) s;
        } else if ( !input ) {
            input = argv[i];
        } else if ( !idx_str ) {
            idx_str = argv[i];
        }
    }

    if ( !input || !idx_str || ( format < 0 && speed < 0 ) ) {
        fprintf ( stderr, "Usage: tmzedit set <file> <index> [--format <fmt>] [--speed <spd>] [-o <output>]\n\n" );
        fprintf ( stderr, "Formats: normal, turbo, fastipl, sinclair, fsk, slow, direct, cpm-tape\n" );
        fprintf ( stderr, "Speeds:  1:1, 2:1, 2:1cpm, 3:1, 3:2, 7:3, 8:3, 9:7, 25:14\n\n" );
        fprintf ( stderr, "Block 0x40: --format and --speed (converts to 0x41 if non-standard)\n" );
        fprintf ( stderr, "Block 0x41: --format and --speed\n" );
        fprintf ( stderr, "Block 0x45: --speed only (format is always NORMAL)\n" );
        return EXIT_FAILURE;
    }

    uint32_t index;
    if ( !parse_uint32 ( idx_str, &index ) ) {
        fprintf ( stderr, "Error: invalid block index '%s'\n", idx_str );
        return EXIT_FAILURE;
    }

    if ( !output ) output = input;

    en_TZX_ERROR err;
    st_TZX_FILE *file = load_file ( input, &err );
    if ( !file ) return EXIT_FAILURE;

    if ( index >= file->block_count ) {
        fprintf ( stderr, "Error: block index %u out of range (0-%u)\n",
                  index, file->block_count - 1 );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    uint8_t block_id = file->blocks[index].id;
    int ret;

    switch ( block_id ) {

        case TMZ_BLOCK_ID_MZ_STANDARD_DATA:
            ret = set_block_0x40 ( file, index, format, speed );
            break;

        case TMZ_BLOCK_ID_MZ_TURBO_DATA:
            ret = set_block_0x41 ( file, index, format, speed );
            break;

        case TMZ_BLOCK_ID_MZ_BASIC_DATA:
            if ( format >= 0 ) {
                fprintf ( stderr, "Error: block 0x45 (MZ BASIC Data) does not support --format "
                          "(format is always NORMAL)\n" );
                tzx_free ( file );
                return EXIT_FAILURE;
            }
            if ( speed >= 0 ) {
                fprintf ( stderr, "Error: block 0x45 (MZ BASIC Data) does not have a speed field\n" );
                tzx_free ( file );
                return EXIT_FAILURE;
            }
            ret = EXIT_SUCCESS;
            break;

        default:
            fprintf ( stderr, "Error: block [%u] is 0x%02X (%s) - not an MZ data block\n",
                      index, block_id, tmz_block_id_name ( block_id ) );
            tzx_free ( file );
            return EXIT_FAILURE;
    }

    if ( ret != EXIT_SUCCESS ) {
        tzx_free ( file );
        return ret;
    }

    ret = save_file ( output, file );
    if ( ret == EXIT_SUCCESS ) {
        printf ( "Saved: %s (%u blocks)\n", output, file->block_count );
    }
    tzx_free ( file );
    return ret;
}


/* ========================================================================= */
/*  Hlavni funkce                                                            */
/* ========================================================================= */


/**
 * @brief Vypise napovedu programu.
 * @param prog_name Nazev spusteneho programu (argv[0]).
 */
static void print_usage ( const char *prog_name ) {
    fprintf ( stderr, "Usage: %s <command> [options] <file> [args...]\n\n", prog_name );
    fprintf ( stderr, "Commands:\n" );
    fprintf ( stderr, "  list          List all blocks in the file\n" );
    fprintf ( stderr, "  dump          Hex dump of a block's data\n" );
    fprintf ( stderr, "  remove        Remove a block by index\n" );
    fprintf ( stderr, "  move          Move a block from one position to another\n" );
    fprintf ( stderr, "  merge         Merge multiple tape files into one\n" );
    fprintf ( stderr, "  split         Split tape into separate programs\n" );
    fprintf ( stderr, "  add-text      Add a Text Description block (0x30)\n" );
    fprintf ( stderr, "  add-message   Add a Message block (0x31)\n" );
    fprintf ( stderr, "  archive-info  Add/replace Archive Info block (0x32)\n" );
    fprintf ( stderr, "  set           Set format/speed on MZ data blocks (0x40/0x41/0x45)\n" );
    fprintf ( stderr, "  validate      Validate file integrity\n" );
    fprintf ( stderr, "\nOptions:\n" );
    fprintf ( stderr, "  -o <output>   Output file (default: overwrite input)\n" );
    fprintf ( stderr, "  --name-encoding <enc> Filename encoding: ascii, utf8-eu, utf8-jp (default: ascii)\n" );
}


/**
 * @brief Hlavni funkce - dispatch na subcommand.
 *
 * Prvni argument za nazvem programu je subcommand.
 * Zbyle argumenty se predaji odpovidajici funkci.
 *
 * @param argc Pocet argumentu.
 * @param argv Pole argumentu.
 * @return EXIT_SUCCESS nebo EXIT_FAILURE.
 */
int main ( int argc, char *argv[] ) {

    if ( argc < 2 ) {
        print_usage ( argv[0] );
        return EXIT_FAILURE;
    }

    const char *cmd = argv[1];
    int sub_argc = argc - 2;
    char **sub_argv = argv + 2;

    if ( strcmp ( cmd, "list" ) == 0 )          return cmd_list ( sub_argc, sub_argv );
    if ( strcmp ( cmd, "dump" ) == 0 )          return cmd_dump ( sub_argc, sub_argv );
    if ( strcmp ( cmd, "remove" ) == 0 )        return cmd_remove ( sub_argc, sub_argv );
    if ( strcmp ( cmd, "move" ) == 0 )          return cmd_move ( sub_argc, sub_argv );
    if ( strcmp ( cmd, "merge" ) == 0 )         return cmd_merge ( sub_argc, sub_argv );
    if ( strcmp ( cmd, "split" ) == 0 )         return cmd_split ( sub_argc, sub_argv );
    if ( strcmp ( cmd, "add-text" ) == 0 )      return cmd_add_text ( sub_argc, sub_argv );
    if ( strcmp ( cmd, "add-message" ) == 0 )   return cmd_add_message ( sub_argc, sub_argv );
    if ( strcmp ( cmd, "archive-info" ) == 0 )  return cmd_archive_info ( sub_argc, sub_argv );
    if ( strcmp ( cmd, "set" ) == 0 )           return cmd_set ( sub_argc, sub_argv );
    if ( strcmp ( cmd, "validate" ) == 0 )      return cmd_validate ( sub_argc, sub_argv );

    if ( strcmp ( cmd, "help" ) == 0 || strcmp ( cmd, "--help" ) == 0 || strcmp ( cmd, "-h" ) == 0 ) {
        print_usage ( argv[0] );
        return EXIT_SUCCESS;
    }

    fprintf ( stderr, "Error: unknown command '%s'\n\n", cmd );
    print_usage ( argv[0] );
    return EXIT_FAILURE;
}
