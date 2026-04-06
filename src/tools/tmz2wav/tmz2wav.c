/**
 * @file   tmz2wav.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Konverzni utility TMZ/TZX -> WAV.
 *
 * Nacte TMZ nebo TZX soubor, prehaje vsechny audio bloky
 * pomoci TMZ playeru a vysledny CMT audio signal ulozi
 * jako WAV soubor (mono, 8-bit PCM).
 *
 * Podporuje vsechny bloky, ktere TMZ player umi prehrat:
 * - MZ bloky 0x40 (Standard Data) a 0x41 (Turbo Data) pres mztape
 * - TZX bloky 0x10-0x15 a 0x20 pres TZX knihovnu
 *
 * Ridici a informacni bloky jsou preskoceny.
 *
 * @par Pouziti:
 * @code
 *   tmz2wav input.tmz output.wav [volby]
 * @endcode
 *
 * @par Volby:
 * - --rate <Hz>        : vzorkovaci frekvence (vychozi: 44100)
 * - --pulseset <sada>  : vychozi pulzni sada: 700, 800, 80b (vychozi: 800)
 * - --version          : zobrazit verzi programu
 * - --lib-versions     : zobrazit verze knihoven
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
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_player.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/generic_driver/generic_driver.h"
#include "libs/generic_driver/memory_driver.h"

/** @brief Verze programu tmz2wav (z @version v hlavicce souboru). */
#define TMZ2WAV_VERSION  "1.0.0"


/**
 * @brief Pripoji vsechny pulzy ze zdrojoveho vstreamu do ciloveho vstreamu.
 *
 * Cte pulzy ze zdrojoveho vstreamu pres cmt_vstream_read_pulse()
 * a zapisuje je do ciloveho vstreamu pres cmt_vstream_add_value().
 * Po zavolani je cteci pozice zdrojoveho vstreamu na konci.
 *
 * @param dst Cilovy vstream (nesmi byt NULL).
 * @param src Zdrojovy vstream (nesmi byt NULL).
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe zapisu.
 *
 * @pre Oba vstreamy musi mit stejnou vzorkovaci frekvenci.
 * @post Vsechny pulzy ze src jsou pridany na konec dst.
 */
static int vstream_append ( st_CMT_VSTREAM *dst, st_CMT_VSTREAM *src ) {
    cmt_vstream_read_reset ( src );
    uint64_t samples;
    int value;
    while ( cmt_vstream_read_pulse ( src, &samples, &value ) == EXIT_SUCCESS ) {
        if ( samples == 0 ) continue;
        if ( cmt_vstream_add_value ( dst, value, (uint32_t) samples ) != EXIT_SUCCESS ) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}


/**
 * @brief Naparsuje retezec na hodnotu en_MZTAPE_PULSESET.
 *
 * Rozpoznava: "700", "800", "80b".
 *
 * @param str Vstupni retezec.
 * @param[out] pulseset Vystupni hodnota.
 * @return 0 pri uspechu, -1 pokud retezec neodpovida.
 */
static int parse_pulseset ( const char *str, en_MZTAPE_PULSESET *pulseset ) {
    if ( strcasecmp ( str, "700" ) == 0 )  { *pulseset = MZTAPE_PULSESET_700; return 0; }
    if ( strcasecmp ( str, "800" ) == 0 )  { *pulseset = MZTAPE_PULSESET_800; return 0; }
    if ( strcasecmp ( str, "80b" ) == 0 )  { *pulseset = MZTAPE_PULSESET_80B; return 0; }
    return -1;
}


/**
 * @brief Vrati textovy nazev pulsni sady.
 * @param pulseset Hodnota en_MZTAPE_PULSESET.
 * @return Staticky retezec s nazvem.
 */
static const char* pulseset_name ( en_MZTAPE_PULSESET pulseset ) {
    switch ( pulseset ) {
        case MZTAPE_PULSESET_700: return "MZ-700/80K/80A";
        case MZTAPE_PULSESET_800: return "MZ-800/1500";
        case MZTAPE_PULSESET_80B: return "MZ-80B";
        default:                  return "Unknown";
    }
}


/**
 * @brief Vypise verze vsech pouzitych knihoven na stdout.
 */
static void print_lib_versions ( void ) {
    printf ( "Library versions:\n" );
    printf ( "  tmz            %s (TMZ format v%s)\n", tmz_version (), tmz_format_version () );
    printf ( "  tzx            %s (TZX format v%s)\n", tzx_version (), tzx_format_version () );
    printf ( "  cmt_stream     %s\n", cmt_stream_version () );
    printf ( "  generic_driver %s\n", generic_driver_version () );
}


/**
 * @brief Vypise napovedu programu.
 * @param prog_name Nazev spusteneho programu (argv[0]).
 */
static void print_usage ( const char *prog_name ) {
    fprintf ( stderr, "Usage: %s <input.tmz|input.tzx> <output.wav> [options]\n\n", prog_name );
    fprintf ( stderr, "Converts TMZ/TZX tape archive to WAV audio file.\n\n" );
    fprintf ( stderr, "Options:\n" );
    fprintf ( stderr, "  --rate <Hz>          Sample rate (default: 44100)\n" );
    fprintf ( stderr, "  --pulseset <set>     Default pulse set: 700, 800, 80b (default: 800)\n" );
    fprintf ( stderr, "  --version            Show program version\n" );
    fprintf ( stderr, "  --lib-versions       Show library versions\n" );
}


/**
 * @brief Hlavni funkce - konverze TMZ/TZX -> WAV.
 *
 * Zpracuje argumenty prikazove radky, nacte TMZ/TZX soubor,
 * prehaje vsechny audio bloky pomoci TMZ playeru do jednoho
 * spojeneho vstreamu a ulozi vysledek jako WAV soubor.
 *
 * @param argc Pocet argumentu.
 * @param argv Pole argumentu.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
int main ( int argc, char *argv[] ) {

    /* inicializace memory driveru (nutne pro cmt_stream_save_wav) */
    memory_driver_init();

    if ( argc < 2 ) {
        print_usage ( argv[0] );
        return EXIT_FAILURE;
    }

    /* vychozi parametry */
    const char *input_file = NULL;
    const char *output_file = NULL;
    uint32_t sample_rate = CMTSTREAM_DEFAULT_RATE;
    en_MZTAPE_PULSESET pulseset = MZTAPE_PULSESET_800;

    /* parsovani argumentu */
    int positional = 0;
    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp ( argv[i], "--version" ) == 0 ) {
            printf ( "tmz2wav %s\n", TMZ2WAV_VERSION );
            return EXIT_SUCCESS;
        } else if ( strcmp ( argv[i], "--lib-versions" ) == 0 ) {
            print_lib_versions ();
            return EXIT_SUCCESS;
        } else if ( strcmp ( argv[i], "--rate" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --rate requires a value\n" );
                return EXIT_FAILURE;
            }
            char *endptr;
            long val = strtol ( argv[i], &endptr, 10 );
            if ( *endptr != '\0' || val < 8000 || val > 192000 ) {
                fprintf ( stderr, "Error: sample rate must be 8000-192000 Hz\n" );
                return EXIT_FAILURE;
            }
            sample_rate = (uint32_t) val;
        } else if ( strcmp ( argv[i], "--pulseset" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --pulseset requires a value\n" );
                return EXIT_FAILURE;
            }
            if ( parse_pulseset ( argv[i], &pulseset ) != 0 ) {
                fprintf ( stderr, "Error: unknown pulseset '%s'\n", argv[i] );
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

    printf ( "Input  : %s (%s, v%u.%u, %u blocks)\n",
             input_file,
             file->is_tmz ? "TMZ" : "TZX",
             file->header.ver_major, file->header.ver_minor,
             file->block_count );
    printf ( "Output : %s\n", output_file );
    printf ( "Rate   : %u Hz\n", sample_rate );
    printf ( "Pulseset: %s\n\n", pulseset_name ( pulseset ) );

    /* konfigurace playeru */
    st_TMZ_PLAYER_CONFIG config;
    tmz_player_config_init ( &config );
    config.sample_rate = sample_rate;
    config.stream_type = CMT_STREAM_TYPE_VSTREAM;
    config.default_pulseset = pulseset;

    /* vytvorit master vstream pro spojeni vsech bloku */
    st_CMT_VSTREAM *master = cmt_vstream_new ( sample_rate, CMT_VSTREAM_BYTELENGTH8, 0,
                                                CMT_STREAM_POLARITY_NORMAL );
    if ( !master ) {
        fprintf ( stderr, "Error: failed to create master vstream\n" );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    /* prehrat vsechny bloky a pripojit do master vstreamu */
    uint32_t audio_blocks = 0;
    uint32_t error_blocks = 0;

    st_TMZ_PLAYER_STATE state;
    tmz_player_state_init ( &state, file, &config );

    while ( !tmz_player_state_finished ( &state ) ) {
        en_TMZ_PLAYER_ERROR player_err;
        st_CMT_STREAM *stream = tmz_player_play_next ( &state, &player_err );

        if ( stream ) {
            /* prehrani bylo uspesne - pripojit do master vstreamu */
            uint32_t bi = state.last_played_block;
            const st_TZX_BLOCK *block = &file->blocks[bi];

            if ( stream->stream_type == CMT_STREAM_TYPE_VSTREAM && stream->str.vstream ) {
                if ( vstream_append ( master, stream->str.vstream ) != EXIT_SUCCESS ) {
                    fprintf ( stderr, "  [%3u] 0x%02X %-25s - ERROR: append failed\n",
                              bi, block->id, tmz_block_id_name ( block->id ) );
                }
            }

            printf ( "  [%3u] 0x%02X %-25s -> %.3f s\n",
                     bi, block->id, tmz_block_id_name ( block->id ),
                     cmt_stream_get_length ( stream ) );

            cmt_stream_destroy ( stream );
            audio_blocks++;
        } else if ( player_err != TMZ_PLAYER_OK ) {
            /* chyba prehravani */
            uint32_t bi = state.last_played_block;
            const st_TZX_BLOCK *block = &file->blocks[bi];
            fprintf ( stderr, "  [%3u] 0x%02X %-25s - ERROR: %s\n",
                      bi, block->id, tmz_block_id_name ( block->id ),
                      tmz_player_error_string ( player_err ) );
            error_blocks++;
        }
        /* player_err == OK && stream == NULL: konec souboru */
    }

    printf ( "\nAudio blocks: %u", audio_blocks );
    if ( error_blocks > 0 ) printf ( ", Errors: %u", error_blocks );
    printf ( "\n" );

    if ( audio_blocks == 0 ) {
        fprintf ( stderr, "Error: no audio blocks to export\n" );
        cmt_vstream_destroy ( master );
        tzx_free ( file );
        return EXIT_FAILURE;
    }

    /* zabalit master vstream do CMT stream wrapperu pro ulozeni */
    st_CMT_STREAM master_stream;
    master_stream.stream_type = CMT_STREAM_TYPE_VSTREAM;
    master_stream.str.vstream = master;

    double total_length = cmt_vstream_get_length ( master );
    uint64_t total_samples = cmt_vstream_get_count_scans ( master );

    printf ( "Total  : %.3f s (%llu samples, %u bytes vstream data)\n",
             total_length, (unsigned long long) total_samples,
             cmt_vstream_get_size ( master ) );

    /* ulozit jako WAV */
    int result = cmt_stream_save_wav ( &master_stream, sample_rate, (char*) output_file );

    if ( result == EXIT_SUCCESS ) {
        printf ( "Saved  : %s\n", output_file );
    } else {
        fprintf ( stderr, "Error: failed to save WAV file '%s'\n", output_file );
    }

    /* uklid - nerusit master pres cmt_stream_destroy, protoze wrapper je na stacku */
    cmt_vstream_destroy ( master );
    tzx_free ( file );

    return result;
}
