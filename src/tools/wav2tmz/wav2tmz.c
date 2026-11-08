/**
 * @file   wav2tmz.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 2.13.0
 * @brief  Konverzni utilita WAV -> MZF/TMZ - dekodovani Sharp MZ kazetovych nahravek.
 *
 * Analyzuje WAV soubor obsahujici nahravku magnetofonove kazety
 * pocitacu Sharp MZ. Implicitne provadi pouze analyzu a vypis vysledku.
 * S volbou -o nebo --output-format extrahuje MZF soubory nebo TMZ archiv.
 *
 * Podporovane formaty: NORMAL, CPM-CMT, CPM-TAPE, MZ-80B, TURBO,
 * FASTIPL, BSD, FSK, SLOW, DIRECT.
 *
 * @par Pouziti:
 * @code
 *   wav2tmz input.wav [-o output] [volby]
 * @endcode
 *
 * @par Volby:
 * - -o <soubor>                : vystupni soubor (aktivuje ukladani, vychozi: input_N.mzf nebo input.tmz)
 * - --output-format <mzf|tmz> : format vystupu (aktivuje ukladani, vychozi: mzf)
 * - --append-tmz               : pridat bloky do existujiciho TMZ souboru
 * - --overwrite-mzf                : prepsat existujici vystupni soubory
 * - --schmitt                  : pouzit Schmitt trigger misto zero-crossing
 * - --tolerance <N>            : tolerance detekce leaderu (0.02-0.35, výchozí 0.20)
 * - --preprocess               : zapnout preprocessing (zpetna kompatibilita)
 * - --no-preprocess            : vypnout preprocessing (DC offset + HP filtr + normalizace)
 * - --histogram                : vypsat histogram delek pulzu
 * - --verbose (-v)             : podrobny vystup
 * - --channel <L|R>            : vyber kanalu ze sterea (vychozi: L)
 * - --invert                   : invertovat polaritu signalu
 * - --keep-unknown             : ulozit neidentifikovane bloky jako Direct Recording
 * - --raw-format <direct>      : format pro neidentifikovane bloky (vychozi: direct)
 * - --pass <N>                 : pocet pruchodu (vychozi: 1, zatim nepouzit)
 * - --pulse-mode <mode>        : rezim ukladani delek pulzu: approximate, exact (vychozi: approximate)
 * - --charset <mode>           : znakova sada: eu (vychozi), jp, utf8-eu, utf8-jp
 * - --recover                  : zapnout vsechny recovery mody
 * - --recover-bsd              : obnovit nekompletni BSD soubory (chybejici terminator, chybejici kusy)
 * - --recover-body             : ponechat neuplne telo (FM, TURBO, FASTIPL, CPM-TAPE, zbytek 00h)
 *                                a slozit telo FM ze dvou poskozenych kopii
 * - --recover-header           : ponechat hlavicku zaznamu FM bez tela (telo 00h)
 * - --version                  : zobrazit verzi programu
 * - --lib-versions             : zobrazit verze knihoven
 * - --help (-h)                : zobrazit napovedu
 *
 * @par Licence:
 * GNU General Public License v3 (GPLv3)
 *
 * Copyright (C) 2026 Michal Hucik <hucik@ordoz.com>
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
#include <math.h>

#include "libs/generic_driver/generic_driver.h"
#include "libs/wav/wav.h"
#include "libs/mzf/mzf.h"
#include "libs/mzf/mzf_tools.h"
#include "libs/endianity/endianity.h"
#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_profile.h"
#include "libs/tzx/tzx.h"
#include "libs/cmtspeed/cmtspeed.h"
#include "libs/wav_analyzer/wav_analyzer.h"


/** @brief Verze programu wav2tmz. */
#define WAV2TMZ_VERSION "2.13.0"


/** @brief Znakova sada pro zobrazeni nazvu souboru (nastaveno z --charset). */
static en_MZF_NAME_ENCODING name_encoding = MZF_NAME_ASCII_EU;


/** @brief Vystupni format programu. */
typedef enum en_OUTPUT_FORMAT {
    OUTPUT_FORMAT_MZF = 0,  /**< jednotlive MZF soubory (vychozi) */
    OUTPUT_FORMAT_TMZ,      /**< jeden TMZ archiv se vsemi bloky */
} en_OUTPUT_FORMAT;


/** @brief Rezim ukladani delek pulzu do TMZ bloku 0x41. */
typedef enum en_PULSE_MODE {
    PULSE_MODE_APPROXIMATE = 0, /**< tabulkovy rezim - kvantizace na en_CMTSPEED (vychozi) */
    PULSE_MODE_EXACT,           /**< custom rezim - presne delky pulzu z histogramu */
} en_PULSE_MODE;

/** @brief Aktualni rezim ukladani delek pulzu (nastaveno z --pulse-mode). */
static en_PULSE_MODE pulse_mode = PULSE_MODE_APPROXIMATE;

/** @brief Vychozi pauza po bloku v milisekundach (1 s). */
#define DEFAULT_PAUSE_MS    1000

/** @brief Velikost datove casti jednoho BSD chunku v bajtech. */
#define BSD_CHUNK_DATA_SIZE 256

/** @brief Velikost jednoho BSD chunku v bajtech (2B ID + 256B data). */
#define BSD_CHUNK_SIZE      258


/**
 * @brief Vypise verze vsech pouzitych knihoven na stdout.
 */
static void print_lib_versions ( void ) {
    printf ( "Library versions:\n" );
    printf ( "  wav_analyzer   %s\n", wav_analyzer_version () );
    printf ( "  tmz            %s (TMZ format v%s)\n", tmz_version (), tmz_format_version () );
    printf ( "  tzx            %s (TZX format v%s)\n", tzx_version (), tzx_format_version () );
    printf ( "  mzf            %s\n", mzf_version () );
    printf ( "  wav            %s\n", wav_version () );
    printf ( "  cmtspeed       %s\n", cmtspeed_version () );
    printf ( "  generic_driver %s\n", generic_driver_version () );
    printf ( "  endianity      %s\n", endianity_version () );
}


/**
 * @brief Zobrazi napovedu programu.
 *
 * @param prog_name Nazev programu (argv[0]).
 */
static void print_usage ( const char *prog_name ) {
    fprintf ( stderr,
              "Usage: %s input.wav [-o output] [options]\n"
              "\n"
              "Analyze WAV file with Sharp MZ tape recordings.\n"
              "Without -o or --output-format, only analysis is performed (no files saved).\n"
              "\n"
              "Output options:\n"
              "  -o <file>                 Output file (enables saving, default: input_N.mzf or input.tmz)\n"
              "  --output-format <mzf|tmz> Output format (enables saving, default: mzf)\n"
              "  --append-tmz              Append blocks to existing TMZ file (without this, existing TMZ is error)\n"
              "  --overwrite-mzf               Overwrite existing output files (without this, existing MZF is error)\n"
              "\n"
              "Analysis options:\n"
              "  --schmitt                 Use Schmitt trigger instead of zero-crossing\n"
              "  --tolerance <N>           Leader detection tolerance (0.02-0.35, default 0.20)\n"
              "  --no-preprocess           Disable preprocessing (DC offset + HP filter + normalize)\n"
              "  --preprocess              Enable preprocessing (default, for backward compatibility)\n"
              "  --histogram               Print pulse histogram\n"
              "  --verbose                 Verbose output (real speed, approx speed, pulse set)\n"
              "  --channel <L|R>           Select stereo channel (default: L)\n"
              "  --invert                  Invert signal polarity\n"
              "  --keep-unknown            Save unidentified blocks as Direct Recording\n"
              "  --raw-format <direct>     Format for unidentified blocks (default: direct)\n"
              "  --pass <N>               Number of passes (default: 1, not yet used)\n"
              "  --pulse-mode <mode>      Pulse width mode: approximate, exact (default: approximate)\n"
              "  --charset <mode>            Character set: eu (default), jp, utf8-eu, utf8-jp\n"
              "\n"
              "Recovery options:\n"
              "  --recover                 Enable all partial data recovery\n"
              "  --recover-bsd             Recover incomplete BSD files (missing terminator or chunks)\n"
              "  --recover-body            Keep a body cut short on the tape (rest filled with 00h),\n"
              "                            merge an FM body from two damaged copies\n"
              "  --recover-header          Keep a valid header whose body is missing (body filled with 00h)\n"
              "\n"
              "  --version                 Show program version\n"
              "  --lib-versions            Show library versions\n"
              "  --help                    Show this help\n",
              prog_name );
}


/**
 * @brief Vygeneruje vystupni jmeno souboru z vstupniho.
 *
 * Nahradi priponu za _N.mzf kde N je index souboru.
 *
 * @param input_path Vstupni cesta k souboru.
 * @param index Index souboru (0-based).
 * @param[out] output Vystupni buffer pro cestu.
 * @param output_size Velikost vystupniho bufferu.
 */
static void generate_mzf_output_name ( const char *input_path, uint32_t index,
                                       char *output, size_t output_size ) {
    const char *dot = strrchr ( input_path, '.' );
    size_t base_len;

    if ( dot ) {
        base_len = ( size_t ) ( dot - input_path );
    } else {
        base_len = strlen ( input_path );
    }

    if ( base_len >= output_size - 10 ) {
        base_len = output_size - 10;
    }

    snprintf ( output, output_size, "%.*s_%u.mzf", ( int ) base_len, input_path, index + 1 );
}


/**
 * @brief Vygeneruje vystupni jmeno TMZ souboru z vstupniho WAV.
 *
 * Nahradi priponu .wav za .tmz.
 *
 * @param input_path Vstupni cesta k souboru.
 * @param[out] output Vystupni buffer pro cestu.
 * @param output_size Velikost vystupniho bufferu.
 */
static void generate_tmz_output_name ( const char *input_path,
                                       char *output, size_t output_size ) {
    const char *dot = strrchr ( input_path, '.' );
    size_t base_len;

    if ( dot ) {
        base_len = ( size_t ) ( dot - input_path );
    } else {
        base_len = strlen ( input_path );
    }

    if ( base_len >= output_size - 5 ) {
        base_len = output_size - 5;
    }

    snprintf ( output, output_size, "%.*s.tmz", ( int ) base_len, input_path );
}


/**
 * @brief Zjistí, zda se má do bloku zapsat checksum jiný než vypočtený.
 *
 * Platí pro záznam s chybou CRC a pro obnovený soubor, jehož tělo je doplněné nulami:
 * přehrání TMZ se pak chová jako poškozená páska a ROM ohlásí chybu.
 *
 * @param fr Výsledek souboru. Nesmí být NULL.
 * @return 1 = aspoň jedna část má poškozený checksum, 0 = ne.
 */
static int has_damaged_checksum ( const st_WAV_ANALYZER_FILE_RESULT *fr ) {
    return fr->header_crc_stored_valid || fr->body_crc_stored_valid
           || ( fr->recovery_status & ( WAV_RECOVERY_PARTIAL_BODY | WAV_RECOVERY_HEADER_ONLY ) );
}


/**
 * @brief Nastaví v argumentech profilu bajty checksumu poškozených částí.
 *
 * Checksum přečtený z pásky se zapíše beze změny. U neúplného těla checksum na
 * pásce není: zapíše se počet jedniček v těle zvýšený o 1, tedy záměrně neplatná hodnota.
 *
 * @param fr Výsledek souboru. Nesmí být NULL.
 * @param[in,out] a Argumenty profilu; ukazatele míří do ck_h a ck_b.
 * @param[out] ck_h Buffer 2 B pro hlavičku, musí žít do volání tmz_profile_build_block().
 * @param[out] ck_b Buffer 2 B pro tělo, totéž.
 */
static void set_tape_checksums ( const st_WAV_ANALYZER_FILE_RESULT *fr, st_TMZ_PROFILE_ARGS *a, uint8_t *ck_h, uint8_t *ck_b ) {
    if ( fr->header_crc_stored_valid ) {
        ck_h[0] = ( uint8_t ) ( fr->header_crc_stored >> 8 );
        ck_h[1] = ( uint8_t ) fr->header_crc_stored;
        a->ck_header = ck_h;
    }
    uint16_t body_ck = fr->body_crc_stored;
    if ( !fr->body_crc_stored_valid ) {
        if ( !( fr->recovery_status & ( WAV_RECOVERY_PARTIAL_BODY | WAV_RECOVERY_HEADER_ONLY ) ) || !fr->mzf ) return;
        body_ck = 1;
        for ( uint32_t i = 0; i < fr->mzf->body_size; i++ ) {
            for ( uint8_t v = fr->mzf->body[i]; v; v &= ( uint8_t ) ( v - 1 ) ) body_ck++;
        }
    }
    ck_b[0] = ( uint8_t ) ( body_ck >> 8 );
    ck_b[1] = ( uint8_t ) body_ck;
    a->ck_body = ck_b;
}


/**
 * @brief Blok 0x46 ROM-800-NOMINAL pro poškozený záznam NORMAL 1:1.
 *
 * @param fr Výsledek souboru s MZF. Nesmí být NULL.
 * @return Nový blok, nebo NULL při chybě (vypíše ji na stderr).
 */
static st_TZX_BLOCK* create_block_damaged ( const st_WAV_ANALYZER_FILE_RESULT *fr ) {
    st_TMZ_PROFILE_ARGS a;
    uint8_t ck_h[2], ck_b[2];
    en_TMZ_PROFILE_ERROR pe;
    tmz_profile_args_init ( &a );
    a.pause_ms = DEFAULT_PAUSE_MS;
    set_tape_checksums ( fr, &a, ck_h, ck_b );
    st_TZX_BLOCK *b = tmz_profile_build_block ( "ROM-800-NOMINAL", &a, &fr->mzf->header, fr->mzf->body, fr->mzf->body_size, &pe );
    if ( !b ) fprintf ( stderr, "Error: profile ROM-800-NOMINAL: %s\n", tmz_profile_error_string ( pe ) );
    return b;
}


/**
 * @brief Popíše obnovu souboru jednou větou (pro blok 0x30).
 *
 * @param fr Výsledek souboru. Nesmí být NULL.
 * @param[out] buf Buffer pro text.
 * @param n Velikost bufferu.
 * @return buf.
 */
static const char* recovery_describe ( const st_WAV_ANALYZER_FILE_RESULT *fr, char *buf, size_t n ) {
    if ( fr->recovery_status & WAV_RECOVERY_PARTIAL_BODY ) {
        snprintf ( buf, n, "partial body (%u of %u bytes read, rest filled with 00h)",
                   fr->recovered_bytes, fr->expected_bytes );
    } else if ( fr->recovery_status & WAV_RECOVERY_HEADER_ONLY ) {
        snprintf ( buf, n, "header only (body %u bytes filled with 00h)", fr->expected_bytes );
    } else if ( fr->recovery_status & WAV_RECOVERY_FSIZE_REPAIRED ) {
        snprintf ( buf, n, "fsize repaired from body length (%u bytes on tape)", fr->expected_bytes );
    } else if ( fr->recovery_status & WAV_RECOVERY_COPY_MERGED ) {
        snprintf ( buf, n, "body merged from two damaged copies (%u bytes from the second copy)", fr->merged_bytes );
    } else if ( fr->recovery_status & WAV_RECOVERY_BSD_GAPS ) {
        snprintf ( buf, n, "%u BSD chunks missing, filled with 00h", fr->bsd_gap_chunks );
    } else {
        snprintf ( buf, n, "%s", wav_recovery_status_string ( fr->recovery_status ) );
    }
    return buf;
}


/**
 * @brief Sestaví krátkou značku obnovy do souhrnu (" [RECOVERED: ...]").
 *
 * @param fr Výsledek souboru s recovery_status != 0. Nesmí být NULL.
 * @param[out] buf Buffer pro text.
 * @param n Velikost bufferu.
 */
static void recovery_tag ( const st_WAV_ANALYZER_FILE_RESULT *fr, char *buf, size_t n ) {
    if ( fr->recovery_status & WAV_RECOVERY_PARTIAL_BODY ) {
        snprintf ( buf, n, " [RECOVERED: partial body %u/%u]", fr->recovered_bytes, fr->expected_bytes );
    } else if ( fr->recovery_status & WAV_RECOVERY_HEADER_ONLY ) {
        snprintf ( buf, n, " [RECOVERED: header only]" );
    } else if ( fr->recovery_status & WAV_RECOVERY_FSIZE_REPAIRED ) {
        snprintf ( buf, n, " [RECOVERED: fsize repaired]" );
    } else if ( fr->recovery_status & WAV_RECOVERY_COPY_MERGED ) {
        snprintf ( buf, n, " [RECOVERED: copies merged]" );
    } else if ( fr->recovery_status & WAV_RECOVERY_BSD_GAPS ) {
        snprintf ( buf, n, " [RECOVERED: %u BSD chunks missing]", fr->bsd_gap_chunks );
    } else {
        snprintf ( buf, n, " [RECOVERED]" );
    }
}


/**
 * @brief Ulozi MZF soubor na disk.
 *
 * @param mzf MZF data k ulozeni. Nesmi byt NULL.
 * @param output_path Cesta k vystupnimu souboru.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int save_mzf ( const st_MZF *mzf, const char *output_path ) {
    st_HANDLER h;
    st_DRIVER d;

    generic_driver_file_init ( &d );

    if ( !generic_driver_open_file ( &h, &d, ( char* ) output_path, FILE_DRIVER_OPMODE_W ) ) {
        fprintf ( stderr, "Error: cannot create output file '%s'\n", output_path );
        return EXIT_FAILURE;
    }

    en_MZF_ERROR err = mzf_save ( &h, mzf );

    generic_driver_close ( &h );

    if ( err != MZF_OK ) {
        fprintf ( stderr, "Error: failed to save MZF: %s\n", mzf_error_string ( err ) );
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


/**
 * @brief Prevede WAV kazetovy format na cilovy stroj TMZ.
 *
 * MZ-80B ma vlastni model, ostatni formaty jsou MZ-800 (vychozi).
 *
 * @param fmt WAV kazetovy format.
 * @return Odpovidajici en_TMZ_MACHINE hodnota.
 */
static en_TMZ_MACHINE wav_format_to_machine ( en_WAV_TAPE_FORMAT fmt ) {
    if ( fmt == WAV_TAPE_FORMAT_MZ80B ) return TMZ_MACHINE_MZ80B;
    return TMZ_MACHINE_MZ800;
}


/**
 * @brief Prevede WAV kazetovy format na pulzni sadu TMZ.
 *
 * MZ-80B pouziva vlastni pulzni sadu, ostatni formaty pouzivaji MZ-800.
 *
 * @param fmt WAV kazetovy format.
 * @return Odpovidajici en_TMZ_PULSESET hodnota.
 */
static en_TMZ_PULSESET wav_format_to_pulseset ( en_WAV_TAPE_FORMAT fmt ) {
    if ( fmt == WAV_TAPE_FORMAT_MZ80B ) return TMZ_PULSESET_80B;
    return TMZ_PULSESET_800;
}


/**
 * @brief Prevede WAV kazetovy format na TMZ formatovou variantu.
 *
 * Mapuje detekovany format z analyzeru na odpovidajici en_TMZ_FORMAT.
 * CPM-CMT se mapuje na TMZ_FORMAT_NORMAL (pouziva standardni FM kodovani).
 *
 * @param fmt WAV kazetovy format.
 * @return Odpovidajici en_TMZ_FORMAT hodnota.
 */
static en_TMZ_FORMAT wav_format_to_tmz_format ( en_WAV_TAPE_FORMAT fmt ) {
    switch ( fmt ) {
        case WAV_TAPE_FORMAT_NORMAL:    return TMZ_FORMAT_NORMAL;
        case WAV_TAPE_FORMAT_MZ80B:     return TMZ_FORMAT_NORMAL;
        case WAV_TAPE_FORMAT_CPM_CMT:   return TMZ_FORMAT_NORMAL;
        case WAV_TAPE_FORMAT_TURBO:     return TMZ_FORMAT_TURBO;
        case WAV_TAPE_FORMAT_FASTIPL:   return TMZ_FORMAT_FASTIPL;
        case WAV_TAPE_FORMAT_SINCLAIR:  return TMZ_FORMAT_SINCLAIR;
        case WAV_TAPE_FORMAT_FSK:       return TMZ_FORMAT_FSK;
        case WAV_TAPE_FORMAT_SLOW:      return TMZ_FORMAT_SLOW;
        case WAV_TAPE_FORMAT_DIRECT:    return TMZ_FORMAT_DIRECT;
        case WAV_TAPE_FORMAT_CPM_TAPE:  return TMZ_FORMAT_CPM_TAPE;
        default:                        return TMZ_FORMAT_NORMAL;
    }
}


/**
 * @brief Prevede WAV kazetovy format na rychlost CMTSPEED pro blok 0x41.
 *
 * CPM-CMT pouziva 2:1 rychlost (CMTSPEED_2_1_CPM).
 * Ostatni formaty nemaji presnou rychlost z dekoderu, pouzivaji CMTSPEED_NONE.
 *
 * @param fmt WAV kazetovy format.
 * @return Odpovidajici en_CMTSPEED hodnota.
 */
static en_CMTSPEED wav_format_to_speed ( en_WAV_TAPE_FORMAT fmt ) {
    if ( fmt == WAV_TAPE_FORMAT_CPM_CMT ) return CMTSPEED_2_1_CPM;
    return CMTSPEED_NONE;
}


/**
 * @brief Referenci SHORT pul-perioda NORMAL FM pri 1200 Bd (us).
 *
 * Odvozeno z mereni realnych nahravek: leader avg ~249 us
 * pri 44100 Hz odpovidajici standardni rychlosti 1:1.
 */
#define NORMAL_1200BD_SHORT_US  249.0

/** @brief Tolerancni okno pro detekci 1:1 rychlosti (+-15 %). */
#define SPEED_1_1_TOLERANCE     0.15

/**
 * @brief Nejmenší relativní odchylka odhadu rychlosti NORMAL (bez zapisovače) od nejbližší
 *        en_CMTSPEED, nad kterou wav2tmz varuje.
 *
 * Skutečný práh je větší z této hodnoty a rozlišení vzorkování (1 vzorek na půlvlnu
 * leaderu): při 44,1 kHz má půlvlna 2:1 asi 5 vzorků a odhad z průměru se liší
 * o desítky procent, aniž by rychlost byla nevyjádřitelná.
 */
#define SPEED_APPROX_WARN       0.05


/**
 * @brief Odhadne rychlostni pomer z prumerne pul-periody leaderu.
 *
 * Porovna prumer leaderu s referencni hodnotou NORMAL 1200 Bd
 * (~249 us) a najde nejblizsi en_CMTSPEED. Pokud je prumer
 * v tolerancnim okne 1:1, vrati CMTSPEED_1_1 (standardni rychlost).
 *
 * @param leader_avg_us Prumerna pul-perioda leaderu (us).
 * @return Nejblizsi en_CMTSPEED hodnota.
 */
static en_CMTSPEED estimate_speed_from_leader ( double leader_avg_us ) {
    if ( leader_avg_us <= 0.0 ) return CMTSPEED_1_1;

    double ratio = NORMAL_1200BD_SHORT_US / leader_avg_us;

    /* 1:1 tolerance: ~212-286 us */
    if ( ratio >= ( 1.0 - SPEED_1_1_TOLERANCE ) &&
         ratio <= ( 1.0 + SPEED_1_1_TOLERANCE ) ) {
        return CMTSPEED_1_1;
    }

    /* najdeme nejblizsi en_CMTSPEED */
    en_CMTSPEED best = CMTSPEED_2_1;
    double best_diff = 1e9;

    for ( int s = CMTSPEED_1_1; s < CMTSPEED_COUNT; s++ ) {
        double divisor = cmtspeed_get_divisor ( ( en_CMTSPEED ) s );
        if ( divisor <= 0.0 ) continue;
        double diff = fabs ( ratio - divisor );
        if ( diff < best_diff ) {
            best_diff = diff;
            best = ( en_CMTSPEED ) s;
        }
    }

    return best;
}


/**
 * @brief Vytvori TMZ blok 0x40 (MZ Standard Data) z dekodovaneho souboru.
 *
 * Pouziva se pro formaty NORMAL a MZ-80B, ktere pouzivaji standardni
 * rychlost pro svuj model pocitace.
 *
 * @param file_result Vysledek dekodovani jednoho souboru. Nesmi byt NULL.
 * @return Novy alokovy TMZ blok, nebo NULL pri chybe.
 *         Volajici musi uvolnit data po appendu do TMZ souboru.
 */
static st_TZX_BLOCK* create_block_standard ( const st_WAV_ANALYZER_FILE_RESULT *file_result ) {
    return tmz_block_from_mzf (
        file_result->mzf,
        wav_format_to_machine ( file_result->format ),
        wav_format_to_pulseset ( file_result->format ),
        DEFAULT_PAUSE_MS
    );
}


/**
 * @brief Vytvori TMZ blok 0x41 (MZ Turbo Data) z dekodovaneho souboru.
 *
 * Pouziva se pro nestandardni formaty: TURBO, FASTIPL, SINCLAIR,
 * CPM-CMT, CPM-TAPE, FSK, SLOW, DIRECT. Vsechna volitelna casovaci
 * pole jsou nastavena na 0 (= pouzit vychozi hodnoty pro dany format).
 * U FSK a SLOW nese pole speed úroveň rychlosti z file_result->fsl_speed;
 * nezjištěnou úroveň (-1) zapíše jako 0 a vypíše varování na stderr.
 *
 * @param file_result Vysledek dekodovani jednoho souboru. Nesmi byt NULL.
 * @return Novy alokovany TMZ blok, nebo NULL pri chybe.
 *         Volajici musi uvolnit data po appendu do TMZ souboru.
 */
static st_TZX_BLOCK* create_block_turbo ( const st_WAV_ANALYZER_FILE_RESULT *file_result ) {
    const st_MZF *mzf = file_result->mzf;

    st_TMZ_MZ_TURBO_DATA params;
    memset ( &params, 0, sizeof ( params ) );

    params.machine = ( uint8_t ) wav_format_to_machine ( file_result->format );
    params.pulseset = ( uint8_t ) wav_format_to_pulseset ( file_result->format );
    params.format = ( uint8_t ) wav_format_to_tmz_format ( file_result->format );

    /*
     * Rychlost: pro NORMAL FM odhadneme z leader avg,
     * pro CPM-CMT je pevne 2:1, ostatni CMTSPEED_NONE.
     */
    if ( file_result->format == WAV_TAPE_FORMAT_NORMAL ||
         file_result->format == WAV_TAPE_FORMAT_MZ80B ||
         file_result->format == WAV_TAPE_FORMAT_TURBO ||
         file_result->format == WAV_TAPE_FORMAT_FASTIPL ) {
        /*
         * NORMAL/MZ-80B/TURBO/FASTIPL: rychlost odhadneme z leader avg.
         * Pro TURBO a FASTIPL file_result->leader obsahuje datovy leader
         * (nastaveno wav_analyzerem), ne preloader/header leader.
         */
        params.speed = ( uint8_t ) estimate_speed_from_leader (
                                       file_result->leader.avg_period_us );
    } else if ( file_result->format == WAV_TAPE_FORMAT_FSK || file_result->format == WAV_TAPE_FORMAT_SLOW ) {
        /* FSK/SLOW: pole speed nese úroveň rychlosti, určenou z bajtu zpoždění v zavaděči */
        if ( file_result->fsl_speed >= 0 ) {
            params.speed = ( uint8_t ) file_result->fsl_speed;
        } else {
            params.speed = 0;
            fprintf ( stderr, "Warning: %s speed level not recognized from the loader, level 0 written\n",
                      wav_tape_format_name ( file_result->format ) );
        }
    } else {
        params.speed = ( uint8_t ) wav_format_to_speed ( file_result->format );
    }

    params.pause_ms = DEFAULT_PAUSE_MS;

    /*
     * Exact pulse mode: ulozit namerene delky pulzu z histogramu.
     * Pro FM formaty (NORMAL, MZ-80B) s dostupnymi histogramovymi daty
     * pouzijeme custom pulse rezim (speed=0, pulse fields != 0).
     */
    if ( pulse_mode == PULSE_MODE_EXACT &&
         file_result->short_pulse_us > 0.0 && file_result->long_pulse_us > 0.0 &&
         ( file_result->format == WAV_TAPE_FORMAT_NORMAL ||
           file_result->format == WAV_TAPE_FORMAT_MZ80B ) ) {
        params.speed = ( uint8_t ) CMTSPEED_NONE;
        /* jednotka 0,1 us: seconds = value / 1e7, tj. value = us * 10 */
        uint16_t short_val = ( uint16_t ) round ( file_result->short_pulse_us * 10.0 );
        uint16_t long_val = ( uint16_t ) round ( file_result->long_pulse_us * 10.0 );
        params.long_high = long_val;
        params.long_low = long_val;
        params.short_high = short_val;
        params.short_low = short_val;
    }

    params.flags = 0;
    memcpy ( &params.mzf_header, &mzf->header, sizeof ( st_MZF_HEADER ) );
    params.body_size = ( uint16_t ) mzf->body_size;

    return tmz_block_create_mz_turbo ( &params, mzf->body, ( uint16_t ) mzf->body_size );
}


/**
 * @brief Vytvoří blok 0x46 profilu BASIC-1Z016 z dekódovaného souboru BSD.
 *
 * Převede ploché datové tělo MZF (z dekodéru) zpět na kusy: každý kus má
 * 2 B ID (LE) + 256 B dat. Poslední kus dostane ID=0xFFFF a nese poslední
 * porci dat (tak zapisuje BASIC 1Z-016). Blok sestaví
 * profil BASIC-1Z016 s výchozí pauzou; struktura zdroje (tón, kopie) se
 * nezachová.
 *
 * @param file_result Výsledek dekódování jednoho souboru BSD. Nesmí být NULL.
 * @return Nový alokovaný blok, nebo NULL při chybě (víc než 254 kusů nebo chyba
 *         profilu; důvod vypíše na stderr).
 *         Volající musí uvolnit data po appendu do TMZ souboru.
 *
 * @note Alokuje docasny buffer pro chunky, ktery je uvolnen pred navratem.
 */
static st_TZX_BLOCK* create_block_bsd ( const st_WAV_ANALYZER_FILE_RESULT *file_result ) {
    const st_MZF *mzf = file_result->mzf;

    /* pocet chunku: minimalne 1 (terminacni) */
    uint16_t total_chunks;
    if ( mzf->body_size == 0 ) {
        total_chunks = 1;
    } else {
        total_chunks = ( uint16_t ) ( ( mzf->body_size + BSD_CHUNK_DATA_SIZE - 1 ) / BSD_CHUNK_DATA_SIZE );
    }

    /* alokace bufferu pro chunky */
    uint32_t chunks_size = ( uint32_t ) total_chunks * BSD_CHUNK_SIZE;
    uint8_t *chunks = ( uint8_t* ) calloc ( 1, chunks_size );
    if ( !chunks ) return NULL;

    /* naplneni chunku - posledni = terminacni (ID=0xFFFF) */
    for ( uint32_t i = 0; i < total_chunks; i++ ) {
        uint8_t *chunk = &chunks[i * BSD_CHUNK_SIZE];

        /* posledni chunk = terminacni, ostatni sekvencni */
        uint16_t chunk_id = ( i == ( uint32_t ) ( total_chunks - 1 ) )
                            ? 0xFFFF
                            : ( uint16_t ) i;

        /* 2B ID v little-endian */
        chunk[0] = ( uint8_t ) ( chunk_id & 0xFF );
        chunk[1] = ( uint8_t ) ( chunk_id >> 8 );

        /* 256B dat (zbytek nulovy z calloc) */
        if ( mzf->body && mzf->body_size > 0 ) {
            uint32_t offset = i * BSD_CHUNK_DATA_SIZE;
            if ( offset < mzf->body_size ) {
                uint32_t remaining = mzf->body_size - offset;
                uint32_t copy_size = remaining < BSD_CHUNK_DATA_SIZE ? remaining : BSD_CHUNK_DATA_SIZE;
                memcpy ( &chunk[2], &mzf->body[offset], copy_size );
            }
        }
    }

    /* priprava hlavicky: fsize/fstrt/fexec musi byt 0 pro BSD */
    st_MZF_HEADER bsd_header;
    memcpy ( &bsd_header, &mzf->header, sizeof ( st_MZF_HEADER ) );
    bsd_header.fsize = 0;
    bsd_header.fstrt = 0;
    bsd_header.fexec = 0;

    st_TMZ_PROFILE_ARGS a;
    tmz_profile_args_init ( &a );
    a.pause_ms = DEFAULT_PAUSE_MS;
    en_TMZ_PROFILE_ERROR pe;
    st_TZX_BLOCK *block = tmz_profile_build_block ( "BASIC-1Z016", &a, &bsd_header, chunks,
                                                    chunks_size, &pe );
    if ( !block ) {
        if ( total_chunks > 254 ) {
            fprintf ( stderr, "Error: BSD file with %u chunks cannot be stored (BASIC-1Z016 allows 254)\n",
                      ( unsigned ) total_chunks );
        } else {
            fprintf ( stderr, "Error: profile BASIC-1Z016: %s\n", tmz_profile_error_string ( pe ) );
        }
    }

    free ( chunks );
    return block;
}


/**
 * @brief Vytvori TZX blok 0x10 (Standard Speed Data) z TAP dat ZX bloku.
 *
 * Struktura bloku 0x10: [2B pause_ms LE] [2B data_len LE] [data].
 * TAP data obsahuji flag + payload + checksum (primo z dekoderu).
 *
 * @param file_result Vysledek dekodovani jednoho ZX bloku. Nesmi byt NULL.
 * @return Novy alokovany TZX blok, nebo NULL pri chybe.
 *         Volajici musi uvolnit data po appendu do TMZ souboru.
 */
static st_TZX_BLOCK* create_block_zx ( const st_WAV_ANALYZER_FILE_RESULT *file_result ) {
    if ( !file_result->tap_data || file_result->tap_data_size == 0 ) return NULL;

    uint16_t pause_ms = DEFAULT_PAUSE_MS;
    uint16_t data_len = ( uint16_t ) file_result->tap_data_size;
    uint32_t total = 4 + data_len; /* 2B pause + 2B data_len + data */

    st_TZX_BLOCK *block = ( st_TZX_BLOCK* ) calloc ( 1, sizeof ( st_TZX_BLOCK ) );
    if ( !block ) return NULL;

    block->id = TZX_BLOCK_ID_STANDARD_SPEED;
    block->length = total;
    block->data = ( uint8_t* ) calloc ( 1, total );
    if ( !block->data ) {
        free ( block );
        return NULL;
    }

    /* 2B pause LE */
    block->data[0] = ( uint8_t ) ( pause_ms & 0xFF );
    block->data[1] = ( uint8_t ) ( ( pause_ms >> 8 ) & 0xFF );

    /* 2B data length LE */
    block->data[2] = ( uint8_t ) ( data_len & 0xFF );
    block->data[3] = ( uint8_t ) ( ( data_len >> 8 ) & 0xFF );

    /* TAP data */
    memcpy ( block->data + 4, file_result->tap_data, data_len );

    return block;
}


/** @brief Standardni ZX Spectrum pilot pulz (T-states pri 3.5 MHz). */
#define ZX_PILOT_TSTATES    2168
/** @brief Standardni ZX Spectrum 1. sync pulz (T-states). */
#define ZX_SYNC1_TSTATES    667
/** @brief Standardni ZX Spectrum 2. sync pulz (T-states). */
#define ZX_SYNC2_TSTATES    735
/** @brief Standardni ZX Spectrum ZERO bit pulz (T-states). */
#define ZX_ZERO_TSTATES     855
/** @brief Standardni ZX Spectrum ONE bit pulz (T-states). */
#define ZX_ONE_TSTATES      1710
/** @brief Standardni ZX Spectrum pilot pulz (us). */
#define ZX_PILOT_US         619.4
/** @brief TZX CPU takt (Hz). 1 T-state = 1/3500000 s = 0.2857 us. */
#define TZX_CPU_FREQ        3500000.0


/**
 * @brief Vytvori TZX blok 0x11 (Turbo Speed Data) z TAP dat SINCLAIR bloku.
 *
 * SINCLAIR format pouziva ZX Spectrum protokol pri ruznych rychlostech.
 * Casovaci parametry jsou skalovany z prumerne pul-periody leader tonu
 * vuci standardnimu ZX casovani (pilot = 2168 T-states = 619.4 us).
 *
 * Struktura bloku 0x11:
 * [2B pilot] [2B sync1] [2B sync2] [2B zero] [2B one]
 * [2B pilot_count] [1B used_bits] [2B pause] [3B data_len] [data]
 *
 * @param file_result Vysledek dekodovani jednoho SINCLAIR bloku. Nesmi byt NULL.
 * @return Novy alokovany TZX blok, nebo NULL pri chybe.
 *         Volajici musi uvolnit data po appendu do TMZ souboru.
 */
static st_TZX_BLOCK* create_block_sinclair ( const st_WAV_ANALYZER_FILE_RESULT *file_result ) {
    if ( !file_result->tap_data || file_result->tap_data_size == 0 ) return NULL;

    /* skalovaci faktor z leader avg vuci standardnimu ZX pilotu */
    double ratio = file_result->leader.avg_period_us / ZX_PILOT_US;

    uint16_t pilot_pulse = ( uint16_t ) round ( ZX_PILOT_TSTATES * ratio );
    uint16_t sync1_pulse = ( uint16_t ) round ( ZX_SYNC1_TSTATES * ratio );
    uint16_t sync2_pulse = ( uint16_t ) round ( ZX_SYNC2_TSTATES * ratio );
    uint16_t zero_pulse  = ( uint16_t ) round ( ZX_ZERO_TSTATES * ratio );
    uint16_t one_pulse   = ( uint16_t ) round ( ZX_ONE_TSTATES * ratio );
    uint16_t pilot_count = ( uint16_t ) file_result->leader.pulse_count;
    uint16_t pause_ms    = DEFAULT_PAUSE_MS;
    uint32_t data_len    = file_result->tap_data_size;

    /* blok 0x11: 18B hlavicka + data */
    uint32_t header_size = 18;
    uint32_t total = header_size + data_len;

    st_TZX_BLOCK *block = ( st_TZX_BLOCK* ) calloc ( 1, sizeof ( st_TZX_BLOCK ) );
    if ( !block ) return NULL;

    block->id = TZX_BLOCK_ID_TURBO_SPEED;
    block->length = total;
    block->data = ( uint8_t* ) calloc ( 1, total );
    if ( !block->data ) {
        free ( block );
        return NULL;
    }

    uint8_t *d = block->data;

    /* 00-01: pilot pulse LE */
    d[0] = ( uint8_t ) ( pilot_pulse & 0xFF );
    d[1] = ( uint8_t ) ( pilot_pulse >> 8 );
    /* 02-03: sync1 pulse LE */
    d[2] = ( uint8_t ) ( sync1_pulse & 0xFF );
    d[3] = ( uint8_t ) ( sync1_pulse >> 8 );
    /* 04-05: sync2 pulse LE */
    d[4] = ( uint8_t ) ( sync2_pulse & 0xFF );
    d[5] = ( uint8_t ) ( sync2_pulse >> 8 );
    /* 06-07: zero bit pulse LE */
    d[6] = ( uint8_t ) ( zero_pulse & 0xFF );
    d[7] = ( uint8_t ) ( zero_pulse >> 8 );
    /* 08-09: one bit pulse LE */
    d[8] = ( uint8_t ) ( one_pulse & 0xFF );
    d[9] = ( uint8_t ) ( one_pulse >> 8 );
    /* 0A-0B: pilot tone length (pulse count) LE */
    d[10] = ( uint8_t ) ( pilot_count & 0xFF );
    d[11] = ( uint8_t ) ( pilot_count >> 8 );
    /* 0C: used bits in last byte */
    d[12] = 8;
    /* 0D-0E: pause after block LE */
    d[13] = ( uint8_t ) ( pause_ms & 0xFF );
    d[14] = ( uint8_t ) ( pause_ms >> 8 );
    /* 0F-11: data length (3 bytes LE) */
    d[15] = ( uint8_t ) ( data_len & 0xFF );
    d[16] = ( uint8_t ) ( ( data_len >> 8 ) & 0xFF );
    d[17] = ( uint8_t ) ( ( data_len >> 16 ) & 0xFF );
    /* 12...: TAP data */
    memcpy ( &d[18], file_result->tap_data, data_len );

    return block;
}


/**
 * @brief Vytvoří blok 0x46 záznamu CPM-TAPE podle rozpoznané varianty.
 *
 * Intercopy -> profil IC-10.2-CPM (1500, 2400, 2800, 3200, 3600), Pezik ->
 * PEZIK-2.0A (2400/3200), obojí s rychlostí z dekodéru, pauza výchozí profilu. Hlavička a tělo jdou do bloku beze
 * změny (checksum generuje player z dat).
 *
 * @param file_result Výsledek dekódování (format CPM_TAPE, mzf). Nesmí být NULL.
 * @return Nový blok (uvolní volající), nebo NULL při chybě profilu (vypíše důvod,
 *         např. tělo Pezik nad 16 KB).
 */
static st_TZX_BLOCK* create_block_cpmtape ( const st_WAV_ANALYZER_FILE_RESULT *file_result ) {
    st_TMZ_PROFILE_ARGS a;
    tmz_profile_args_init ( &a );
    const char *id = ( file_result->cpmtape_variant == WAV_CPMTAPE_INTERCOPY ) ? "IC-10.2-CPM" : "PEZIK-2.0A";
    a.bd = file_result->cpmtape_bd ? file_result->cpmtape_bd : 2400;
    en_TMZ_PROFILE_ERROR pe;
    const st_MZF *m = file_result->mzf;
    st_TZX_BLOCK *b = tmz_profile_build_block ( id, &a, &m->header, m->body, m->body_size, &pe );
    if ( !b ) fprintf ( stderr, "Error: profile %s: %s\n", id, tmz_profile_error_string ( pe ) );
    return b;
}


/**
 * @brief Blok 0x46 profilem zapisovače, který analyzér rozpoznal.
 *
 * TurboCopy (format NORMAL = Loader N, TURBO = Loader Y) -> TC-1.21 s poměrem
 * a/b (tc_a, tc_b); Intercopy NORMAL / TURBO / FAST IPL -> IC-10.2-NORMAL /
 * -TURBO / -FASTIPL s bd. Ostatní případy (writer UNKNOWN, jiný formát) blok
 * nevytvoří a o bloku rozhodne formát jako dřív.
 *
 * @param file_result Výsledek dekódování. Nesmí být NULL.
 * @param[out] out_block Nový blok (NULL při chybě profilu, chyba vypsána na stderr).
 * @return 1 = případ patří profilu (blok v *out_block), 0 = nepatří.
 */
static int create_block_writer ( const st_WAV_ANALYZER_FILE_RESULT *file_result, st_TZX_BLOCK **out_block ) {
    const char *id = NULL;
    st_TMZ_PROFILE_ARGS a;
    uint8_t ck_h[2], ck_b[2];
    tmz_profile_args_init ( &a );
    a.pause_ms = DEFAULT_PAUSE_MS;
    set_tape_checksums ( file_result, &a, ck_h, ck_b );
    en_WAV_TAPE_FORMAT f = file_result->format;
    if ( file_result->writer == WAV_WRITER_TURBOCOPY && ( f == WAV_TAPE_FORMAT_NORMAL || f == WAV_TAPE_FORMAT_TURBO ) ) {
        id = "TC-1.21";
        a.tc_a = file_result->tc_a;
        a.tc_b = file_result->tc_b;
        a.loader = ( f == WAV_TAPE_FORMAT_TURBO );
    } else if ( file_result->writer == WAV_WRITER_INTERCOPY ) {
        if ( f == WAV_TAPE_FORMAT_NORMAL ) id = "IC-10.2-NORMAL";
        else if ( f == WAV_TAPE_FORMAT_TURBO ) id = "IC-10.2-TURBO";
        else if ( f == WAV_TAPE_FORMAT_FASTIPL ) id = "IC-10.2-FASTIPL";
        a.bd = file_result->ic_bd;
    }
    if ( !id ) return 0;
    en_TMZ_PROFILE_ERROR pe;
    const st_MZF *m = file_result->mzf;
    *out_block = tmz_profile_build_block ( id, &a, &m->header, m->body, m->body_size, &pe );
    if ( !*out_block ) fprintf ( stderr, "Error: profile %s: %s\n", id, tmz_profile_error_string ( pe ) );
    return 1;
}


/**
 * @brief Vytvori TMZ blok z dekodovaneho souboru podle jeho formatu.
 *
 * Mapovani formatu na TMZ bloky:
 *   - NORMAL, MZ-80B        -> blok 0x40 (MZ Standard Data); NORMAL 1:1 s poškozeným
 *                              checksumem -> blok 0x46 ROM-800-NOMINAL s bajty checksumu
 *   - TURBO, FASTIPL, CPM-CMT, FSK, SLOW, DIRECT
 *                            -> blok 0x41 (MZ Turbo Data)
 *   - CPM-TAPE               -> blok 0x46 (IC-10.2-CPM / PEZIK-2.0A)
 *   - BSD                    -> blok 0x46 (BASIC-1Z016)
 *   - ZX_SPECTRUM            -> blok 0x10 (Standard Speed Data)
 *   - SINCLAIR               -> blok 0x11 (Turbo Speed Data)
 *
 * Před tímto mapováním create_block_writer() zapíše rozpoznaný zapisovač
 * TurboCopy / Intercopy jako blok 0x46 (TC-1.21, IC-10.2-*).
 *
 * @param file_result Vysledek dekodovani jednoho souboru. Nesmi byt NULL.
 * @param sample_rate Vzorkovací frekvence WAV v Hz (rozlišení pro varování o rychlosti
 *                    NORMAL); 0 = neznámá, platí jen SPEED_APPROX_WARN.
 * @return Novy alokovany TMZ blok, nebo NULL pri chybe.
 *         Volajici musi uvolnit data po appendu do TMZ souboru.
 * @note Vedlejší efekt: varování na stderr (nevyjádřitelná rychlost), nemění výsledek.
 */
static st_TZX_BLOCK* create_tmz_block_from_result ( const st_WAV_ANALYZER_FILE_RESULT *file_result,
                                                    uint32_t sample_rate ) {
    st_TZX_BLOCK *pb = NULL;
    if ( create_block_writer ( file_result, &pb ) ) return pb;
    switch ( file_result->format ) {
        /* ZX Spectrum -> blok 0x10 (Standard Speed Data) */
        case WAV_TAPE_FORMAT_ZX_SPECTRUM:
            return create_block_zx ( file_result );

        /* SINCLAIR -> blok 0x11 (Turbo Speed Data s casovanim z leaderu) */
        case WAV_TAPE_FORMAT_SINCLAIR:
            return create_block_sinclair ( file_result );

        /* NORMAL FM - 1200 Bd -> blok 0x40, jina rychlost -> blok 0x41 */
        case WAV_TAPE_FORMAT_NORMAL:
        case WAV_TAPE_FORMAT_MZ80B:
        {
            /* v exact rezimu vzdy pouzijeme blok 0x41 s custom pulse fields */
            if ( pulse_mode == PULSE_MODE_EXACT &&
                 file_result->short_pulse_us > 0.0 &&
                 file_result->long_pulse_us > 0.0 ) {
                return create_block_turbo ( file_result );
            }

            en_CMTSPEED speed = estimate_speed_from_leader (
                                    file_result->leader.avg_period_us );
            /* zapisovač neznámý a rychlost daleko od en_CMTSPEED: blok 0x41 ji vyjádří jen přibližně */
            if ( speed != CMTSPEED_1_1 && file_result->leader.avg_period_us > 0.0 ) {
                double ratio = NORMAL_1200BD_SHORT_US / file_result->leader.avg_period_us;
                double div = cmtspeed_get_divisor ( speed );
                double tol = SPEED_APPROX_WARN;
                if ( sample_rate > 0 ) {
                    double res = 1e6 / ( sample_rate * file_result->leader.avg_period_us );
                    if ( res > tol ) tol = res;
                }
                if ( div > 0.0 && fabs ( ratio / div - 1.0 ) > tol ) {
                    fprintf ( stderr, "Warning: NORMAL speed estimate %.2fx differs from nearest %s by %.0f %%, %s written\n",
                              ratio, cmtspeed_get_ratiotxt ( speed ), fabs ( ratio / div - 1.0 ) * 100.0,
                              cmtspeed_get_ratiotxt ( speed ) );
                }
            }
            if ( speed == CMTSPEED_1_1 ) {
                /* poškozený záznam: blok 0x40 checksum nenese, 0x46 ho uloží věrně */
                if ( file_result->format == WAV_TAPE_FORMAT_NORMAL && has_damaged_checksum ( file_result ) ) {
                    return create_block_damaged ( file_result );
                }
                /* kategorie 1: standardni 1200 Bd -> blok 0x40 */
                return create_block_standard ( file_result );
            } else {
                /* kategorie 2: jina rychlost -> blok 0x41 format=NORMAL */
                return create_block_turbo ( file_result );
            }
        }

        /* BSD chunkovany format */
        case WAV_TAPE_FORMAT_BSD:
            return create_block_bsd ( file_result );

        /* CPM-TAPE -> blok 0x46 podle varianty */
        case WAV_TAPE_FORMAT_CPM_TAPE:
            return create_block_cpmtape ( file_result );

        /* nestandardni formaty (kategorie 3, 4, mimo) -> blok 0x41 */
        case WAV_TAPE_FORMAT_TURBO:
        case WAV_TAPE_FORMAT_FASTIPL:
        case WAV_TAPE_FORMAT_CPM_CMT:
        case WAV_TAPE_FORMAT_FSK:
        case WAV_TAPE_FORMAT_SLOW:
        case WAV_TAPE_FORMAT_DIRECT:
            return create_block_turbo ( file_result );

        default:
            /* nerozpoznany format -> pouzijeme standardni blok 0x40 */
            return create_block_standard ( file_result );
    }
}


/**
 * @brief Vytvori text description retezec s metadaty o zdroji.
 *
 * Format: "Decoded from <filename> (WAV <sample_rate> Hz, <duration>s, <file_count> files)"
 *
 * @param input_path Cesta k vstupnimu WAV souboru.
 * @param result Vysledek analyzy.
 * @param[out] buf Vystupni buffer pro text.
 * @param buf_size Velikost bufferu.
 */
static void format_description_text ( const char *input_path,
                                      const st_WAV_ANALYZER_RESULT *result,
                                      char *buf, size_t buf_size ) {
    /* extrahujeme jen jmeno souboru z cesty */
    const char *filename = strrchr ( input_path, '/' );
    if ( !filename ) filename = strrchr ( input_path, '\\' );
    if ( filename ) filename++;
    else filename = input_path;

    snprintf ( buf, buf_size,
               "Decoded from %s (WAV %u Hz, %.1fs, %u file%s)",
               filename,
               result->sample_rate,
               result->wav_duration_sec,
               result->file_count,
               result->file_count == 1 ? "" : "s" );
}


/**
 * @brief Ulozi vsechny dekodovane soubory jako TMZ archiv.
 *
 * Chovani podle append_tmz:
 *   - append_tmz=0: pokud soubor existuje -> ERROR.
 *     Pokud neexistuje, vytvori novy TMZ.
 *   - append_tmz=1: pokud soubor existuje, nacte ho a prida bloky.
 *     Pokud neexistuje, vytvori novy TMZ s warningem.
 *
 * TMZ soubor obsahuje:
 * 1. Text Description (blok 0x30) s metadaty o zdrojovem WAV souboru
 * 2. Pro kazdy dekodovany soubor odpovidajici datovy blok (0x40/0x41/0x46)
 *
 * @param result Vysledek analyzy WAV souboru. Nesmi byt NULL.
 * @param input_path Cesta k vstupnimu WAV souboru (pro metadata).
 * @param output_path Cesta k vystupnimu TMZ souboru.
 * @param append_tmz 1 = pripojit k existujicimu souboru, 0 = chyba pokud existuje.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int save_tmz ( const st_WAV_ANALYZER_RESULT *result,
                      const char *input_path,
                      const char *output_path,
                      int append_tmz ) {

    /* kontrola existence a nacteni existujiciho TMZ souboru */
    st_TZX_FILE *tmz_file = NULL;
    uint32_t old_block_count = 0;

    {
        st_HANDLER exist_handler;
        st_DRIVER exist_driver;
        generic_driver_file_init ( &exist_driver );
        st_HANDLER *h_exist = generic_driver_open_file ( &exist_handler, &exist_driver,
                                                          ( char* ) output_path, FILE_DRIVER_OPMODE_RO );
        if ( h_exist ) {
            if ( !append_tmz ) {
                /* soubor existuje a append neni povoleny -> chyba */
                generic_driver_close ( h_exist );
                fprintf ( stderr, "Error: output file '%s' already exists (use --append-tmz to append)\n",
                          output_path );
                return EXIT_FAILURE;
            }

            en_TZX_ERROR load_err;
            tmz_file = tzx_load ( h_exist, &load_err );
            generic_driver_close ( h_exist );

            if ( !tmz_file ) {
                fprintf ( stderr, "Error: failed to load existing file '%s': %s\n",
                          output_path, tzx_error_string ( load_err ) );
                return EXIT_FAILURE;
            }
            old_block_count = tmz_file->block_count;
        } else if ( append_tmz ) {
            /* append pozadovan, ale soubor neexistuje - warning a vytvorime novy */
            fprintf ( stderr, "Warning: --append-tmz specified but '%s' does not exist, creating new file\n",
                      output_path );
        }
    }

    if ( !tmz_file ) {
        /* soubor neexistuje - vytvorit novy */
        tmz_file = ( st_TZX_FILE* ) calloc ( 1, sizeof ( st_TZX_FILE ) );
        if ( !tmz_file ) {
            fprintf ( stderr, "Error: memory allocation failed\n" );
            return EXIT_FAILURE;
        }
        tmz_header_init ( &tmz_file->header );
        tmz_file->is_tmz = true;
    }

    en_TZX_ERROR tzx_err;

    /* 1. Text Description blok (0x30) s metadaty */
    char desc_text[256];
    format_description_text ( input_path, result, desc_text, sizeof ( desc_text ) );

    st_TZX_BLOCK desc_block;
    tzx_err = tzx_block_create_text_description ( desc_text, &desc_block );

    if ( tzx_err == TZX_OK ) {
        tzx_err = tzx_file_append_block ( tmz_file, &desc_block );
        if ( tzx_err != TZX_OK ) {
            fprintf ( stderr, "Error: failed to add text description block: %s\n",
                      tzx_error_string ( tzx_err ) );
            if ( desc_block.data ) free ( desc_block.data );
            tzx_free ( tmz_file );
            return EXIT_FAILURE;
        }
        /* data ownership predano na tmz_file, desc_block je lokalni na zasobniku */
    }

    /*
     * 2. datove bloky a raw bloky chronologicky prokladane.
     *
     * Dekodovane soubory (files[]) a neidentifikovane useky (raw_blocks[])
     * se radi podle pozice ve WAV: files[].leader.start_index
     * a raw_blocks[].pulse_start. Vypisujeme v poradi na pasce.
     */
    uint32_t fi = 0;   /* index do files[] */
    uint32_t ri = 0;   /* index do raw_blocks[] */

    while ( fi < result->file_count || ri < result->raw_block_count ) {
        /* urcime pozici dalsiho souboru a dalsiho raw bloku */
        uint32_t file_pos = ( fi < result->file_count )
                            ? result->files[fi].leader.start_index
                            : UINT32_MAX;
        uint32_t raw_pos = ( ri < result->raw_block_count )
                           ? result->raw_blocks[ri].pulse_start
                           : UINT32_MAX;

        if ( raw_pos < file_pos ) {
            /* raw blok je drive - zapsat Direct Recording */
            const st_WAV_ANALYZER_RAW_BLOCK *rb = &result->raw_blocks[ri];
            ri++;

            if ( !rb->data || rb->data_size == 0 ) continue;

            uint16_t tstates = ( uint16_t ) ( TZX_DEFAULT_CPU_CLOCK / rb->sample_rate );
            st_TZX_BLOCK raw_block;
            en_TZX_ERROR raw_err = tzx_block_create_direct_recording (
                tstates, 0, rb->used_bits_last,
                rb->data, rb->data_size, &raw_block
            );

            if ( raw_err == TZX_OK ) {
                tzx_err = tzx_file_append_block ( tmz_file, &raw_block );
                if ( tzx_err != TZX_OK ) {
                    fprintf ( stderr, "Error: failed to append raw block: %s\n",
                              tzx_error_string ( tzx_err ) );
                    free ( raw_block.data );
                    tzx_free ( tmz_file );
                    return EXIT_FAILURE;
                }
            }
        } else {
            /* dekodovany soubor je drive (nebo stejne) - zapsat datovy blok */
            const st_WAV_ANALYZER_FILE_RESULT *file_result = &result->files[fi];
            fi++;

            if ( !file_result->mzf && !file_result->tap_data ) continue;

            /* recovery metadata jako Text Description (blok 0x30) */
            if ( file_result->recovery_status != WAV_RECOVERY_NONE ) {
                char recovery_desc[256];
                char recovery_text[128];
                snprintf ( recovery_desc, sizeof ( recovery_desc ),
                           "WARNING: Recovered - %s",
                           recovery_describe ( file_result, recovery_text, sizeof ( recovery_text ) ) );

                st_TZX_BLOCK rec_desc_block;
                en_TZX_ERROR rec_err = tzx_block_create_text_description (
                    recovery_desc, &rec_desc_block );
                if ( rec_err == TZX_OK ) {
                    tzx_err = tzx_file_append_block ( tmz_file, &rec_desc_block );
                    if ( tzx_err != TZX_OK ) {
                        if ( rec_desc_block.data ) free ( rec_desc_block.data );
                    }
                }
            }

            st_TZX_BLOCK *block = create_tmz_block_from_result ( file_result, result->sample_rate );
            if ( !block ) {
                fprintf ( stderr, "Error: failed to create TMZ block for file %u (%s)\n",
                          fi, wav_tape_format_name ( file_result->format ) );
                tzx_free ( tmz_file );
                return EXIT_FAILURE;
            }

            tzx_err = tzx_file_append_block ( tmz_file, block );
            if ( tzx_err != TZX_OK ) {
                fprintf ( stderr, "Error: failed to append block: %s\n",
                          tzx_error_string ( tzx_err ) );
                tmz_block_free ( block );
                tzx_free ( tmz_file );
                return EXIT_FAILURE;
            }

            free ( block );
        }
    }

    /* blok 0x46 (CPM-TAPE, BSD) vyžaduje formát v1.1; připojení k souboru v1.0 verzi zvýší (jako mzf2tmz) */
    for ( uint32_t bi = 0; bi < tmz_file->block_count; bi++ ) {
        if ( tmz_file->blocks[bi].id == TMZ_BLOCK_ID_MZ_FILE && tmz_file->header.ver_major == 1
             && tmz_file->header.ver_minor < 1 ) {
            tmz_file->header.ver_minor = 1;
        }
    }

    /* 4. zapis TMZ souboru na disk */
    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );

    st_HANDLER *h_out = generic_driver_open_file ( &h, &d, ( char* ) output_path, FILE_DRIVER_OPMODE_W );
    if ( !h_out ) {
        fprintf ( stderr, "Error: cannot create output file '%s'\n", output_path );
        tzx_free ( tmz_file );
        return EXIT_FAILURE;
    }

    tzx_err = tzx_save ( h_out, tmz_file );
    generic_driver_close ( h_out );

    if ( tzx_err != TZX_OK ) {
        fprintf ( stderr, "Error: failed to write TMZ file: %s\n",
                  tzx_error_string ( tzx_err ) );
        tzx_free ( tmz_file );
        return EXIT_FAILURE;
    }

    /* vypis souhrnu */
    if ( old_block_count > 0 ) {
        fprintf ( stdout, "Appended to TMZ: %s (%u -> %u blocks)\n",
                  output_path, old_block_count, tmz_file->block_count );
    } else {
        fprintf ( stdout, "Saved TMZ: %s (%u block%s)\n",
                  output_path, tmz_file->block_count,
                  tmz_file->block_count == 1 ? "" : "s" );
    }

    for ( uint32_t i = 0; i < result->file_count; i++ ) {
        const st_WAV_ANALYZER_FILE_RESULT *fr = &result->files[i];

        if ( fr->tap_data && fr->tap_data_size > 0 ) {
            /* ZX Spectrum / SINCLAIR blok */
            if ( fr->format == WAV_TAPE_FORMAT_SINCLAIR ) {
                double ratio = fr->leader.avg_period_us / ZX_PILOT_US;
                fprintf ( stdout, "  [%u] SINCLAIR flag=0x%02X, %u bytes, speed=%.2fx (%.0f us), CRC: %s\n",
                          i + 1, fr->tap_data[0], fr->tap_data_size,
                          1.0 / ratio, fr->leader.avg_period_us,
                          fr->header_crc == WAV_CRC_OK ? "OK" : ( fr->header_crc == WAV_CRC_ERROR ? "ERROR" : "N/A" ) );
            } else {
                fprintf ( stdout, "  [%u] ZX SPECTRUM flag=0x%02X, %u bytes, CRC: %s\n",
                          i + 1, fr->tap_data[0], fr->tap_data_size,
                          fr->header_crc == WAV_CRC_OK ? "OK" : ( fr->header_crc == WAV_CRC_ERROR ? "ERROR" : "N/A" ) );
            }
        } else if ( fr->mzf ) {
            char fname[MZF_FNAME_UTF8_BUF_SIZE];
            mzf_tools_get_fname_ex ( &fr->mzf->header, fname, sizeof ( fname ), name_encoding );

            char recovered_tag[64] = "";
            if ( fr->recovery_status != WAV_RECOVERY_NONE ) {
                recovery_tag ( fr, recovered_tag, sizeof ( recovered_tag ) );
            }

            fprintf ( stdout, "  [%u] \"%s\" - %s, %u bytes, CRC: header=%s body=%s%s%s\n",
                      i + 1, fname,
                      wav_tape_format_name ( fr->format ),
                      fr->mzf->body_size,
                      fr->header_crc == WAV_CRC_OK ? "OK" : ( fr->header_crc == WAV_CRC_ERROR ? "ERROR" : "N/A" ),
                      fr->body_crc == WAV_CRC_OK ? "OK" : ( fr->body_crc == WAV_CRC_ERROR ? "ERROR" : "N/A" ),
                      fr->copy2_used ? " (Copy2)" : "",
                      recovered_tag );
        }
    }

    if ( result->raw_block_count > 0 ) {
        double total_raw_sec = 0.0;
        for ( uint32_t i = 0; i < result->raw_block_count; i++ ) {
            total_raw_sec += result->raw_blocks[i].duration_sec;
        }
        fprintf ( stdout, "  Raw blocks: %u (%.2f sec)\n", result->raw_block_count, total_raw_sec );
    }

    tzx_free ( tmz_file );
    return EXIT_SUCCESS;
}


/**
 * @brief Hlavni funkce - parsovani argumentu a spusteni analyzy.
 *
 * @param argc Pocet argumentu prikazove radky.
 * @param argv Pole argumentu prikazove radky.
 * @return EXIT_SUCCESS při úspěchu, EXIT_FAILURE při chybě. V režimu MZF je
 *         chyba i to, že analyzer našel bloky, ale žádný nešlo uložit
 *         (bloky bez dat MZF, např. SINCLAIR).
 */
int main ( int argc, char *argv[] ) {
    if ( argc < 2 ) {
        print_usage ( argv[0] );
        return EXIT_FAILURE;
    }

    const char *input_path = NULL;
    const char *output_path = NULL;
    int show_histogram = 0;
    int output_requested = 0;   /**< 1 = ukladani pozadovano (-o nebo --output-format) */
    int append_tmz = 0;         /**< 1 = pripojit k existujicimu TMZ (--append-tmz) */
    int overwrite = 0;          /**< 1 = prepsat existujici vystupni soubory (--overwrite-mzf) */
    en_OUTPUT_FORMAT output_format = OUTPUT_FORMAT_MZF;

    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );

    /* parsovani argumentu */
    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp ( argv[i], "--version" ) == 0 ) {
            printf ( "wav2tmz %s\n", WAV2TMZ_VERSION );
            return EXIT_SUCCESS;
        } else if ( strcmp ( argv[i], "--lib-versions" ) == 0 ) {
            print_lib_versions ();
            return EXIT_SUCCESS;
        } else if ( strcmp ( argv[i], "--help" ) == 0 || strcmp ( argv[i], "-h" ) == 0 ) {
            print_usage ( argv[0] );
            return EXIT_SUCCESS;
        } else if ( strcmp ( argv[i], "-o" ) == 0 ) {
            if ( i + 1 >= argc ) {
                fprintf ( stderr, "Error: -o requires an argument\n" );
                return EXIT_FAILURE;
            }
            output_path = argv[++i];
            output_requested = 1;
        } else if ( strcmp ( argv[i], "--output-format" ) == 0 ) {
            if ( i + 1 >= argc ) {
                fprintf ( stderr, "Error: --output-format requires mzf or tmz\n" );
                return EXIT_FAILURE;
            }
            i++;
            if ( strcmp ( argv[i], "tmz" ) == 0 || strcmp ( argv[i], "TMZ" ) == 0 ) {
                output_format = OUTPUT_FORMAT_TMZ;
                output_requested = 1;
            } else if ( strcmp ( argv[i], "mzf" ) == 0 || strcmp ( argv[i], "MZF" ) == 0 ) {
                output_format = OUTPUT_FORMAT_MZF;
                output_requested = 1;
            } else {
                fprintf ( stderr, "Error: unknown output format '%s' (use mzf or tmz)\n", argv[i] );
                return EXIT_FAILURE;
            }
        } else if ( strcmp ( argv[i], "--schmitt" ) == 0 ) {
            config.pulse_mode = WAV_PULSE_MODE_SCHMITT_TRIGGER;
        } else if ( strcmp ( argv[i], "--tolerance" ) == 0 ) {
            if ( i + 1 >= argc ) {
                fprintf ( stderr, "Error: --tolerance requires an argument\n" );
                return EXIT_FAILURE;
            }
            config.tolerance = atof ( argv[++i] );
            if ( config.tolerance < WAV_ANALYZER_MIN_TOLERANCE ||
                 config.tolerance > WAV_ANALYZER_MAX_TOLERANCE ) {
                fprintf ( stderr, "Error: tolerance must be %.2f-%.2f\n",
                          WAV_ANALYZER_MIN_TOLERANCE, WAV_ANALYZER_MAX_TOLERANCE );
                return EXIT_FAILURE;
            }
        } else if ( strcmp ( argv[i], "--preprocess" ) == 0 ) {
            /* zpetna kompatibilita - preprocessing je nyni ve vychozim stavu zapnuty */
            config.enable_dc_offset = 1;
            config.enable_highpass = 1;
            config.enable_normalize = 1;
        } else if ( strcmp ( argv[i], "--no-preprocess" ) == 0 ) {
            config.enable_dc_offset = 0;
            config.enable_highpass = 0;
            config.enable_normalize = 0;
        } else if ( strcmp ( argv[i], "--histogram" ) == 0 ) {
            show_histogram = 1;
        } else if ( strcmp ( argv[i], "--verbose" ) == 0 || strcmp ( argv[i], "-v" ) == 0 ) {
            config.verbose = 1;
        } else if ( strcmp ( argv[i], "--channel" ) == 0 ) {
            if ( i + 1 >= argc ) {
                fprintf ( stderr, "Error: --channel requires L or R\n" );
                return EXIT_FAILURE;
            }
            i++;
            if ( argv[i][0] == 'R' || argv[i][0] == 'r' ) {
                config.channel = WAV_CHANNEL_RIGHT;
            } else {
                config.channel = WAV_CHANNEL_LEFT;
            }
        } else if ( strcmp ( argv[i], "--invert" ) == 0 ) {
            config.polarity = WAV_SIGNAL_POLARITY_INVERTED;
        } else if ( strcmp ( argv[i], "--keep-unknown" ) == 0 ) {
            config.keep_unknown = 1;
        } else if ( strcmp ( argv[i], "--raw-format" ) == 0 ) {
            if ( i + 1 >= argc ) {
                fprintf ( stderr, "Error: --raw-format requires an argument\n" );
                return EXIT_FAILURE;
            }
            i++;
            if ( strcmp ( argv[i], "direct" ) == 0 || strcmp ( argv[i], "DIRECT" ) == 0 ) {
                config.raw_format = WAV_RAW_FORMAT_DIRECT;
            } else {
                fprintf ( stderr, "Error: unknown raw format '%s' (use: direct)\n", argv[i] );
                return EXIT_FAILURE;
            }
        } else if ( strcmp ( argv[i], "--pass" ) == 0 ) {
            if ( i + 1 >= argc ) {
                fprintf ( stderr, "Error: --pass requires a number\n" );
                return EXIT_FAILURE;
            }
            config.pass_count = atoi ( argv[++i] );
            if ( config.pass_count < 1 ) config.pass_count = 1;
        } else if ( strcmp ( argv[i], "--pulse-mode" ) == 0 ) {
            if ( i + 1 >= argc ) {
                fprintf ( stderr, "Error: --pulse-mode requires approximate or exact\n" );
                return EXIT_FAILURE;
            }
            i++;
            if ( strcmp ( argv[i], "approximate" ) == 0 ) {
                pulse_mode = PULSE_MODE_APPROXIMATE;
            } else if ( strcmp ( argv[i], "exact" ) == 0 ) {
                pulse_mode = PULSE_MODE_EXACT;
            } else {
                fprintf ( stderr, "Error: unknown pulse mode '%s' (use: approximate, exact)\n", argv[i] );
                return EXIT_FAILURE;
            }
        } else if ( strcmp ( argv[i], "--recover" ) == 0 ) {
            config.recover_bsd = 1;
            config.recover_body = 1;
            config.recover_header = 1;
        } else if ( strcmp ( argv[i], "--recover-bsd" ) == 0 ) {
            config.recover_bsd = 1;
        } else if ( strcmp ( argv[i], "--recover-body" ) == 0 ) {
            config.recover_body = 1;
        } else if ( strcmp ( argv[i], "--recover-header" ) == 0 ) {
            config.recover_header = 1;
        } else if ( strcmp ( argv[i], "--append-tmz" ) == 0 ) {
            append_tmz = 1;
        } else if ( strcmp ( argv[i], "--overwrite-mzf" ) == 0 ) {
            overwrite = 1;
        } else if ( strcmp ( argv[i], "--charset" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --charset requires a value\n" );
                return EXIT_FAILURE;
            }
            if ( strcmp ( argv[i], "eu" ) == 0 ) {
                name_encoding = MZF_NAME_ASCII_EU;
            } else if ( strcmp ( argv[i], "jp" ) == 0 ) {
                name_encoding = MZF_NAME_ASCII_JP;
            } else if ( strcmp ( argv[i], "utf8-eu" ) == 0 ) {
                name_encoding = MZF_NAME_UTF8_EU;
            } else if ( strcmp ( argv[i], "utf8-jp" ) == 0 ) {
                name_encoding = MZF_NAME_UTF8_JP;
            } else {
                fprintf ( stderr, "Error: unknown charset '%s' (use: eu, jp, utf8-eu, utf8-jp)\n", argv[i] );
                return EXIT_FAILURE;
            }
        } else if ( argv[i][0] != '-' ) {
            if ( !input_path ) {
                input_path = argv[i];
            } else {
                fprintf ( stderr, "Error: unexpected argument '%s'\n", argv[i] );
                return EXIT_FAILURE;
            }
        } else {
            fprintf ( stderr, "Error: unknown option '%s'\n", argv[i] );
            return EXIT_FAILURE;
        }
    }

    if ( !input_path ) {
        fprintf ( stderr, "Error: no input file specified\n" );
        print_usage ( argv[0] );
        return EXIT_FAILURE;
    }

    /* otevreme WAV soubor */
    st_HANDLER h;
    st_DRIVER d;

    generic_driver_file_init ( &d );

    st_HANDLER *h_in = generic_driver_open_file ( &h, &d, ( char* ) input_path, FILE_DRIVER_OPMODE_RO );
    if ( !h_in ) {
        fprintf ( stderr, "Error: cannot open input file '%s'\n", input_path );
        return EXIT_FAILURE;
    }

    /* spustime analyzu */
    st_WAV_ANALYZER_RESULT result;
    en_WAV_ANALYZER_ERROR err = wav_analyzer_analyze ( h_in, &config, &result );

    generic_driver_close ( h_in );

    if ( err != WAV_ANALYZER_OK ) {
        fprintf ( stderr, "Error: analysis failed: %s\n", wav_analyzer_error_string ( err ) );
        wav_analyzer_result_destroy ( &result );
        return EXIT_FAILURE;
    }

    /* vypiseme shrnuti */
    wav_analyzer_print_summary ( &result, stdout, config.verbose );

    /* volitelny histogram */
    if ( show_histogram ) {
        fprintf ( stdout, "(Histogram is printed in verbose mode during analysis.)\n" );
        fprintf ( stdout, "Use --verbose to see pulse histograms.\n" );
    }

    /* bez -o / --output-format: pouze analyza, nic neukladame */
    if ( !output_requested ) {
        int has_files = ( result.file_count > 0 );
        wav_analyzer_result_destroy ( &result );
        return has_files ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    /* overime, ze byly dekodovany nejake soubory */
    if ( result.file_count == 0 ) {
        fprintf ( stderr, "No files decoded.\n" );
        wav_analyzer_result_destroy ( &result );
        return EXIT_FAILURE;
    }

    int exit_code;

    if ( output_format == OUTPUT_FORMAT_TMZ ) {
        /* TMZ vystup: jeden soubor se vsemi bloky */
        char out_name[512];

        if ( output_path ) {
            snprintf ( out_name, sizeof ( out_name ), "%s", output_path );
        } else {
            generate_tmz_output_name ( input_path, out_name, sizeof ( out_name ) );
        }

        exit_code = save_tmz ( &result, input_path, out_name, append_tmz );
    } else {
        /* MZF vystup: kazdy soubor zvlast */
        exit_code = EXIT_SUCCESS;
        uint32_t saved = 0;   /* uložené soubory MZF */

        for ( uint32_t i = 0; i < result.file_count; i++ ) {
            char out_name[512];

            if ( output_path && result.file_count == 1 ) {
                snprintf ( out_name, sizeof ( out_name ), "%s", output_path );
            } else if ( output_path ) {
                generate_mzf_output_name ( output_path, i, out_name, sizeof ( out_name ) );
            } else {
                generate_mzf_output_name ( input_path, i, out_name, sizeof ( out_name ) );
            }

            if ( result.files[i].format == WAV_TAPE_FORMAT_ZX_SPECTRUM ) {
                fprintf ( stderr, "Warning: skipping ZX Spectrum block #%u (not MZF format, use --output-format tmz)\n", i + 1 );
            } else if ( result.files[i].mzf ) {
                /* kontrola existence MZF souboru */
                FILE *test_fp = fopen ( out_name, "rb" );
                if ( test_fp ) {
                    fclose ( test_fp );
                    if ( !overwrite ) {
                        fprintf ( stderr, "Error: output file '%s' already exists (use --overwrite-mzf)\n", out_name );
                        exit_code = EXIT_FAILURE;
                        continue;
                    }
                    fprintf ( stderr, "Warning: overwriting existing file '%s'\n", out_name );
                }

                if ( save_mzf ( result.files[i].mzf, out_name ) == EXIT_SUCCESS ) {
                    saved++;
                    fprintf ( stdout, "Saved: %s (%s, %u bytes%s)\n",
                              out_name,
                              wav_tape_format_name ( result.files[i].format ),
                              result.files[i].mzf->body_size,
                              result.files[i].recovery_status != WAV_RECOVERY_NONE ? " [RECOVERED]" : "" );
                } else {
                    exit_code = EXIT_FAILURE;
                }
            } else {
                /* blok bez dat MZF (např. SINCLAIR): dřív se tiše zahodil */
                fprintf ( stderr, "Warning: skipping block #%u (%s, no MZF data; use --output-format tmz)\n",
                          i + 1, wav_tape_format_name ( result.files[i].format ) );
            }
        }

        /* analyzer něco našel, ale nic se neuložilo: neúspěch */
        if ( result.file_count > 0 && saved == 0 && exit_code == EXIT_SUCCESS ) {
            fprintf ( stderr, "Error: no MZF file saved (all blocks skipped)\n" );
            exit_code = EXIT_FAILURE;
        }
    }

    wav_analyzer_result_destroy ( &result );
    return exit_code;
}
