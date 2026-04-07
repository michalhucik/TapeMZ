/**
 * @file   wav_analyzer.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.2.0
 * @brief  Implementace hlavního API knihovny wav_analyzer.
 *
 * Orchestruje všechny vrstvy analyzéru:
 * preprocessing -> pulse extraction -> leader detection ->
 * histogram analysis -> format classification -> decoding.
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

#include "libs/generic_driver/generic_driver.h"
#include "libs/wav/wav.h"
#include "libs/mzf/mzf.h"
#include "libs/endianity/endianity.h"
#include "wav_analyzer.h"


/** @brief Počáteční kapacita pole výsledků souborů. */
#define FILE_RESULT_INITIAL_CAPACITY    8

/**
 * @brief Minimální počet pulzů pro histogramovou analýzu datového úseku.
 *
 * Pokud je datový úsek za leaderem kratší, přeskočíme histogram.
 */
#define HISTOGRAM_MIN_PULSES    100


/**
 * @brief Přidá výsledek souboru do pole výsledků.
 *
 * @param result Celkový výsledek analýzy.
 * @param file Výsledek jednoho souboru k přidání.
 * @return WAV_ANALYZER_OK při úspěchu, WAV_ANALYZER_ERROR_ALLOC při selhání.
 */
static en_WAV_ANALYZER_ERROR result_add_file (
    st_WAV_ANALYZER_RESULT *result,
    const st_WAV_ANALYZER_FILE_RESULT *file
) {
    if ( result->file_count >= result->file_capacity ) {
        uint32_t new_cap = ( result->file_capacity == 0 )
                           ? FILE_RESULT_INITIAL_CAPACITY
                           : result->file_capacity * 2;
        st_WAV_ANALYZER_FILE_RESULT *new_files = ( st_WAV_ANALYZER_FILE_RESULT* ) realloc (
                    result->files, new_cap * sizeof ( st_WAV_ANALYZER_FILE_RESULT )
                );
        if ( !new_files ) return WAV_ANALYZER_ERROR_ALLOC;
        result->files = new_files;
        result->file_capacity = new_cap;
    }

    result->files[result->file_count] = *file;
    result->file_count++;
    return WAV_ANALYZER_OK;
}


/**
 * @brief Minimální počet pulzů pro detekci leaderu Cast 2 (loader body).
 *
 * Cast 2 začíná SGAP (4000-10000 krátkých pulzů). Používáme nižší práh
 * kvůli možné degradaci signálu.
 */
#define NONSTANDARD_LOADER_LEADER_MIN_PULSES    500

/**
 * @brief Offset fsize v loader body (společný pro FSK/SLOW/DIRECT).
 *
 * Uživatelská velikost dat (2B, LE) je na stejném offsetu
 * ve všech třech formátech.
 */
#define LOADER_BODY_OFF_SIZE    5


/**
 * @brief Offsety parametrů v loader body pro jednotlivé formáty.
 *
 * Uživatelská data (fstrt, fexec) jsou na různých offsetech
 * v závislosti na formátu loaderu (FSK/SLOW/DIRECT).
 */
/** @name FSK loader body offsety (191B) */
/** @{ */
#define FSK_LDR_OFF_EXEC       11   /**< offset fexec (2B LE) */
#define FSK_LDR_OFF_FROM       33   /**< offset fstrt (2B LE) */
/** @} */

/** @name SLOW loader body offsety (249B) */
/** @{ */
#define SLOW_LDR_OFF_EXEC      12   /**< offset fexec (2B LE) */
#define SLOW_LDR_OFF_FROM      76   /**< offset fstrt (2B LE) */
/** @} */

/** @name DIRECT loader body offsety (360B) */
/** @{ */
#define DIRECT_LDR_OFF_EXEC    12   /**< offset fexec (2B LE) */
#define DIRECT_LDR_OFF_FROM    34   /**< offset fstrt (2B LE) */
/** @} */


/**
 * @brief Extrahuje uživatelské parametry z dekódovaného loader body.
 *
 * Loader body obsahuje patchované hodnoty: fsize, fstrt, fexec.
 * Offset fsize je společný (5), offsety fstrt a fexec se liší
 * podle formátu.
 *
 * @param loader_body Dekódovaný loader body.
 * @param loader_size Velikost loader body v bajtech.
 * @param format Detekovaný formát (FSK/SLOW/DIRECT).
 * @param[out] out_fsize Uživatelská velikost dat.
 * @param[out] out_fstrt Uživatelská startovní adresa.
 * @param[out] out_fexec Uživatelská exec adresa.
 * @return WAV_ANALYZER_OK při úspěchu, WAV_ANALYZER_ERROR_INVALID_PARAM při chybě.
 */
static en_WAV_ANALYZER_ERROR extract_user_params (
    const uint8_t *loader_body,
    uint32_t loader_size,
    en_WAV_TAPE_FORMAT format,
    uint16_t *out_fsize,
    uint16_t *out_fstrt,
    uint16_t *out_fexec
) {
    uint32_t off_from, off_exec;

    switch ( format ) {
        case WAV_TAPE_FORMAT_FSK:
            off_from = FSK_LDR_OFF_FROM;
            off_exec = FSK_LDR_OFF_EXEC;
            break;
        case WAV_TAPE_FORMAT_SLOW:
            off_from = SLOW_LDR_OFF_FROM;
            off_exec = SLOW_LDR_OFF_EXEC;
            break;
        case WAV_TAPE_FORMAT_DIRECT:
            off_from = DIRECT_LDR_OFF_FROM;
            off_exec = DIRECT_LDR_OFF_EXEC;
            break;
        default:
            return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    /* kontrola rozsahu */
    if ( LOADER_BODY_OFF_SIZE + 2 > loader_size ||
         off_from + 2 > loader_size ||
         off_exec + 2 > loader_size ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    /* čtení LE hodnot */
    uint16_t fsize, fstrt, fexec;
    memcpy ( &fsize, &loader_body[LOADER_BODY_OFF_SIZE], 2 );
    memcpy ( &fstrt, &loader_body[off_from], 2 );
    memcpy ( &fexec, &loader_body[off_exec], 2 );

    *out_fsize = endianity_bswap16_LE ( fsize );
    *out_fstrt = endianity_bswap16_LE ( fstrt );
    *out_fexec = endianity_bswap16_LE ( fexec );

    return WAV_ANALYZER_OK;
}


/**
 * @brief Dekóduje dvourychlostní FSK/SLOW/DIRECT tape signál.
 *
 * Struktura signálu:
 * 1. Cast 1: Preloader hlavička (NORMAL FM 1:1) - již dekódovaná
 * 2. Cast 2: Loader body (NORMAL FM, konfigurovatelná rychlost)
 *    - SGAP + STM(20+20) + 2L + body + CRC + 2L
 * 3. Cast 3: Uživatelská data (FSK/SLOW/DIRECT)
 *    - GAP + polarity + sync + data + fadeout
 *
 * Funkce najde Cast 2, dekóduje loader body pomocí FM dekodéru,
 * extrahuje uživatelské parametry, pak najde Cast 3 a dekóduje
 * data příslušným dekodérem.
 *
 * @param seq Sekvence pulzů.
 * @param preloader_raw Surová data preloader hlavičky (128B).
 * @param search_from_pulse Pozice za preloader hlavičkou.
 * @param format Detekovaný formát (FSK/SLOW/DIRECT).
 * @param[out] out_mzf Výstupní MZF soubor (volající uvolní přes mzf_free).
 * @param[out] out_body_result Výsledek dekódování uživatelských dat.
 * @param[out] out_consumed_until Pozice za posledním zpracovaným pulzem.
 * @return WAV_ANALYZER_OK při úspěchu, jinak chybový kód.
 */
static en_WAV_ANALYZER_ERROR decode_nonstandard_tape_mzf (
    const st_WAV_PULSE_SEQUENCE *seq,
    const uint8_t *preloader_raw,
    uint32_t search_from_pulse,
    en_WAV_TAPE_FORMAT format,
    st_MZF **out_mzf,
    st_WAV_DECODE_RESULT *out_body_result,
    uint32_t *out_consumed_until
) {
    if ( !seq || !preloader_raw || !out_mzf ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    *out_mzf = NULL;
    if ( out_consumed_until ) *out_consumed_until = 0;

    /* === 1. Extrakce loader_body_size z preloader cmnt[1..2] (LE) === */

    uint16_t loader_body_size;
    memcpy ( &loader_body_size, &preloader_raw[24 + 1], 2 );
    loader_body_size = endianity_bswap16_LE ( loader_body_size );

    if ( loader_body_size == 0 || loader_body_size > 1024 ) {
        return WAV_ANALYZER_ERROR_DECODE_DATA;
    }

    /* === 2. Hledání leaderu Cast 2 (loader body) === */

    st_WAV_LEADER_INFO loader_leader;
    en_WAV_ANALYZER_ERROR err = wav_leader_detect (
                                    seq, search_from_pulse,
                                    NONSTANDARD_LOADER_LEADER_MIN_PULSES,
                                    WAV_ANALYZER_DEFAULT_TOLERANCE,
                                    &loader_leader
                                );

    if ( err != WAV_ANALYZER_OK ) {
        return WAV_ANALYZER_ERROR_NO_LEADER;
    }

    /* === 3. Dekódování Cast 2 (loader body) pomocí FM dekodéru === */

    st_WAV_FM_DECODER dec;
    uint32_t data_start = loader_leader.start_index + loader_leader.pulse_count;
    wav_decode_fm_init ( &dec, seq, data_start, loader_leader.avg_period_us );

    /* krátký tapemark (STM: 20 LONG + 20 SHORT, minimum 18+18) */
    err = wav_decode_fm_find_tapemark ( &dec, WAV_TAPEMARK_SHORT );
    if ( err != WAV_ANALYZER_OK ) {
        return WAV_ANALYZER_ERROR_DECODE_TAPEMARK;
    }

    /* sync blok (2L = max 4 LONG půl-periody) */
    {
        int sync_skip = 0;
        while ( dec.pos < seq->count &&
                seq->pulses[dec.pos].duration_us >= dec.threshold_us &&
                sync_skip < 4 ) {
            dec.pos++;
            sync_skip++;
        }
    }

    /* dekódování loader body */
    uint8_t *loader_body = ( uint8_t* ) malloc ( loader_body_size );
    if ( !loader_body ) return WAV_ANALYZER_ERROR_ALLOC;

    dec.crc_accumulator = 0;
    err = wav_decode_fm_read_block ( &dec, loader_body, loader_body_size );
    if ( err != WAV_ANALYZER_OK ) {
        free ( loader_body );
        return WAV_ANALYZER_ERROR_DECODE_DATA;
    }

    /* verifikace CRC loader body */
    en_WAV_CRC_STATUS loader_crc;
    wav_decode_fm_verify_checksum ( &dec, &loader_crc, NULL, NULL );

    uint32_t after_loader = dec.pos;

    /* === 4. Extrakce uživatelských parametrů z loader body === */

    uint16_t user_fsize, user_fstrt, user_fexec;
    err = extract_user_params ( loader_body, loader_body_size, format,
                                &user_fsize, &user_fstrt, &user_fexec );
    free ( loader_body );

    if ( err != WAV_ANALYZER_OK ) {
        return err;
    }

    if ( user_fsize == 0 ) {
        return WAV_ANALYZER_ERROR_DECODE_DATA;
    }

    /* === 5. Dekódování Cast 3 (uživatelská data) === */

    uint8_t *user_data = NULL;
    uint32_t decoded_size = 0;
    uint32_t data_consumed = 0;

    switch ( format ) {
        case WAV_TAPE_FORMAT_FSK:
            err = wav_decode_fsk_decode_bytes (
                      seq, after_loader,
                      &user_data, &decoded_size, &data_consumed
                  );
            break;
        case WAV_TAPE_FORMAT_SLOW:
            err = wav_decode_slow_decode_bytes (
                      seq, after_loader,
                      &user_data, &decoded_size, &data_consumed
                  );
            break;
        case WAV_TAPE_FORMAT_DIRECT:
            err = wav_decode_direct_decode_bytes (
                      seq, after_loader,
                      &user_data, &decoded_size, &data_consumed
                  );
            break;
        default:
            return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    if ( err != WAV_ANALYZER_OK || !user_data ) {
        free ( user_data );
        return ( err != WAV_ANALYZER_OK ) ? err : WAV_ANALYZER_ERROR_DECODE_DATA;
    }

    /*
     * Dekodéry dekódují až do fadeoutu, takže mohou vrátit více dat
     * než user_fsize. Ořežeme na očekávanou velikost.
     */
    uint32_t actual_size = ( decoded_size >= user_fsize ) ? user_fsize : decoded_size;

    /* === 6. Sestavení MZF struktury === */

    st_MZF *mzf = ( st_MZF* ) calloc ( 1, sizeof ( st_MZF ) );
    if ( !mzf ) {
        free ( user_data );
        return WAV_ANALYZER_ERROR_ALLOC;
    }

    /*
     * Rekonstruujeme hlavičku z preloaderu (fname) a loader body (fsize/fstrt/fexec).
     * Preloader ftype je 0x01 (OBJ) - to je typ loaderu, ne původního programu.
     * Použijeme 0x01 jako výchozí (shodně s FASTIPL).
     */
    mzf->header.ftype = 0x01;
    memcpy ( &mzf->header.fname, &preloader_raw[1], sizeof ( mzf->header.fname ) );
    mzf->header.fsize = actual_size;
    mzf->header.fstrt = user_fstrt;
    mzf->header.fexec = user_fexec;

    /* zkopírujeme data s ořezem */
    uint8_t *body_copy = ( uint8_t* ) malloc ( actual_size );
    if ( !body_copy ) {
        free ( mzf );
        free ( user_data );
        return WAV_ANALYZER_ERROR_ALLOC;
    }
    memcpy ( body_copy, user_data, actual_size );
    free ( user_data );

    mzf->body = body_copy;
    mzf->body_size = actual_size;

    *out_mzf = mzf;

    /* výsledek dekódování */
    if ( out_body_result ) {
        out_body_result->format = format;
        out_body_result->data = ( uint8_t* ) malloc ( actual_size );
        if ( out_body_result->data ) {
            memcpy ( out_body_result->data, body_copy, actual_size );
        }
        out_body_result->data_size = actual_size;
        /*
         * FSK/SLOW/DIRECT nemají checksum v signálu (je v Z80 loaderu),
         * proto CRC neověřujeme.
         */
        out_body_result->crc_status = WAV_CRC_NOT_AVAILABLE;
        out_body_result->pulse_start = after_loader;
        out_body_result->pulse_end = data_consumed;
        out_body_result->is_header = 0;
    }

    if ( out_consumed_until ) {
        *out_consumed_until = data_consumed;
    }

    return WAV_ANALYZER_OK;
}


/**
 * @brief Zpracuje jeden leader tón - klasifikuje formát a dekóduje data.
 *
 * @param seq Sekvence pulzů.
 * @param leader Informace o leader tónu.
 * @param config Konfigurace analyzéru.
 * @param result Celkový výsledek (přidá se nový soubor).
 * @return WAV_ANALYZER_OK při úspěchu, jinak chybový kód.
 */
static en_WAV_ANALYZER_ERROR process_leader (
    const st_WAV_PULSE_SEQUENCE *seq,
    const st_WAV_LEADER_INFO *leader,
    const st_WAV_ANALYZER_CONFIG *config,
    st_WAV_ANALYZER_RESULT *result,
    uint32_t *out_consumed_until
) {
    if ( out_consumed_until ) *out_consumed_until = 0;

    /* volitelná histogramová analýza datového úseku za leaderem */
    st_WAV_HISTOGRAM hist;
    memset ( &hist, 0, sizeof ( hist ) );
    int have_hist = 0;

    uint32_t data_start = leader->start_index + leader->pulse_count;
    uint32_t remaining = ( data_start < seq->count ) ? seq->count - data_start : 0;

    if ( remaining >= HISTOGRAM_MIN_PULSES ) {
        /* analyzujeme prvních N pulzů za leaderem pro klasifikaci */
        uint32_t hist_count = ( remaining > 2000 ) ? 2000 : remaining;
        en_WAV_ANALYZER_ERROR herr = wav_histogram_analyze (
                                         seq, data_start, hist_count,
                                         config->histogram_bin_width, &hist
                                     );
        if ( herr == WAV_ANALYZER_OK ) {
            have_hist = 1;
        }
    }

    /* stupeň A: klasifikace z leader tónu */
    en_WAV_TAPE_FORMAT format = wav_classify_from_leader (
                                    leader, have_hist ? &hist : NULL
                                );

    en_WAV_SPEED_CLASS speed = wav_classify_speed_class ( leader->avg_period_us );

    if ( config->verbose ) {
        /* čas leaderu v sekundách: průměrná délka pulzu * počet pulzů / 1e6 */
        double leader_time_sec = ( leader->avg_period_us * leader->pulse_count ) / 1000000.0;
        fprintf ( stderr, "Leader at pulse #%u: %u pulses, avg %.1f us, %.3f sec, class %s -> %s\n",
                  leader->start_index, leader->pulse_count,
                  leader->avg_period_us, leader_time_sec,
                  wav_speed_class_name ( speed ),
                  wav_tape_format_name ( format ) );

        if ( have_hist ) {
            wav_histogram_print ( &hist, stderr );
        }
    }

    /* dekódování podle formátu */
    en_WAV_ANALYZER_ERROR err = WAV_ANALYZER_OK;

    switch ( format ) {
        case WAV_TAPE_FORMAT_NORMAL:
        case WAV_TAPE_FORMAT_SINCLAIR:
        case WAV_TAPE_FORMAT_MZ80B:
        case WAV_TAPE_FORMAT_CPM_CMT:
        {
            /* všechny FM formáty sdílí stejný dekodér */
            st_MZF *mzf = NULL;
            st_WAV_DECODE_RESULT hdr_res, body_res;
            memset ( &hdr_res, 0, sizeof ( hdr_res ) );
            memset ( &body_res, 0, sizeof ( body_res ) );

            err = wav_decode_fm_decode_mzf ( seq, leader, &mzf, &hdr_res, &body_res );

            if ( err == WAV_ANALYZER_OK && mzf ) {
                /* stupeň B: zpřesnění formátu z hlavičky */
                en_WAV_TAPE_FORMAT refined = wav_classify_from_header (
                                                 &mzf->header, format
                                             );

                /*
                 * Pokud zpřesněný formát je TURBO/FASTIPL/BSD,
                 * přepneme na specializovaný dekodér.
                 */
                uint32_t consumed_until = 0;
                uint32_t bsd_recovery = WAV_RECOVERY_NONE;
                st_WAV_LEADER_INFO turbo_data_leader;
                memset ( &turbo_data_leader, 0, sizeof ( turbo_data_leader ) );

                if ( refined == WAV_TAPE_FORMAT_TURBO ) {
                    if ( mzf->header.fsize > 0 && mzf->header.fstrt == 0xD400 ) {
                        /*
                         * TurboCopy TURBO preloader (kategorie 4):
                         * fstrt=$D400, fsize=90 (realne loader body).
                         *
                         * TurboCopy loader patchne ROM a vola standardni CMT read
                         * rutinu $002A, ktera cte POUZE telo (bez hlavicky).
                         * Metadata (fsize/fstrt/fexec) jsou v preloader body
                         * na offsetu $4D.
                         */
                        uint32_t turbo_search = body_res.pulse_end > 0
                                                ? body_res.pulse_end
                                                : hdr_res.pulse_end;
                        if ( config->verbose ) {
                            fprintf ( stderr, "  TurboCopy preloader (fsize=%u) -> TURBO body-only decode from pulse %u\n",
                                      mzf->header.fsize, turbo_search );
                        }

                        /* ulozime preloader metadata pred uvolnenim */
                        st_MZF_HEADER saved_header;
                        memcpy ( &saved_header, &mzf->header, sizeof ( st_MZF_HEADER ) );
                        uint8_t *saved_body = body_res.data;
                        uint32_t saved_body_size = body_res.data_size;
                        body_res.data = NULL; /* vlastnictvi predano */

                        mzf_free ( mzf );
                        mzf = NULL;
                        free ( hdr_res.data ); hdr_res.data = NULL;
                        memset ( &hdr_res, 0, sizeof ( hdr_res ) );
                        memset ( &body_res, 0, sizeof ( body_res ) );

                        err = wav_decode_turbo_turbocopy_mzf (
                                  seq, turbo_search,
                                  &saved_header, saved_body, saved_body_size,
                                  &mzf, &body_res, &consumed_until,
                                  &turbo_data_leader
                              );

                        /* aktualizujeme speed na TURBO hodnoty */
                        if ( err == WAV_ANALYZER_OK && turbo_data_leader.pulse_count > 0 ) {
                            speed = wav_classify_speed_class ( turbo_data_leader.avg_period_us );

                            /*
                             * Spocteme rychlostni pomer z speed_val v preloader body.
                             * speed_val je ROM delay na $0A4B. Referencni delay pro 1:1
                             * je ~82 ($52), odvozeno z mereni: 82/41=2.0 (2:1),
                             * 82/35=2.34 (7:3), 82/30=2.73 (8:3), 82/27=3.04 (3:1).
                             * Dosazene pomery presne odpovidaji TurboCopy rychlostem.
                             * Pouzijeme speed_val pro presnejsi odhad nez leader avg
                             * (ktery trpi kvantizaci pri 44100 Hz).
                             */
                            if ( saved_body_size > 0x4B ) {
                                uint8_t speed_val = saved_body[0x4B];
                                if ( speed_val > 0 ) {
                                    double ratio = 82.0 / ( double ) speed_val;
                                    turbo_data_leader.avg_period_us = 249.0 / ratio;
                                }
                            }
                        }

                        free ( saved_body );
                    } else {
                        /* mzftools TURBO (kategorie 3): fsize=0, loader v comment.
                         * Metadata (fsize/fstrt/fexec) v comment oblasti hlavicky.
                         * TURBO data = body-only (loader vola RDATA). */
                        uint32_t turbo_search = hdr_res.pulse_end > 0
                                                ? hdr_res.pulse_end
                                                : leader->start_index + leader->pulse_count;
                        if ( config->verbose ) {
                            fprintf ( stderr, "  mzftools TURBO preloader -> TURBO body-only decode from pulse %u\n",
                                      turbo_search );
                        }

                        st_MZF_HEADER saved_mzf_header;
                        memcpy ( &saved_mzf_header, &mzf->header, sizeof ( st_MZF_HEADER ) );

                        mzf_free ( mzf );
                        mzf = NULL;
                        free ( hdr_res.data ); hdr_res.data = NULL;
                        free ( body_res.data ); body_res.data = NULL;
                        memset ( &hdr_res, 0, sizeof ( hdr_res ) );
                        memset ( &body_res, 0, sizeof ( body_res ) );

                        err = wav_decode_turbo_mzftools_mzf (
                                  seq, turbo_search, &saved_mzf_header,
                                  &mzf, &body_res, &consumed_until, &turbo_data_leader
                              );
                    }
                } else if ( refined == WAV_TAPE_FORMAT_FASTIPL ) {
                    /* FASTIPL: Cast 2 obsahuje pouze telo */
                    uint8_t *bb_raw = hdr_res.data;
                    uint32_t pulse_end = hdr_res.pulse_end;

                    mzf_free ( mzf );
                    mzf = NULL;
                    free ( body_res.data ); body_res.data = NULL;
                    memset ( &body_res, 0, sizeof ( body_res ) );

                    err = wav_decode_fastipl_decode_mzf (
                              seq, bb_raw, pulse_end, &mzf,
                              &body_res, &consumed_until
                          );

                    free ( bb_raw );
                    hdr_res.data = NULL;
                } else if ( refined == WAV_TAPE_FORMAT_BSD ) {
                    /* BSD: chunkovane telo za hlavicku */
                    st_MZF_HEADER saved_header;
                    memcpy ( &saved_header, &mzf->header, sizeof ( st_MZF_HEADER ) );
                    uint32_t pulse_end = hdr_res.pulse_end;

                    mzf_free ( mzf );
                    mzf = NULL;
                    free ( body_res.data ); body_res.data = NULL;
                    memset ( &body_res, 0, sizeof ( body_res ) );

                    err = wav_decode_bsd_decode_mzf (
                              seq, leader, &saved_header, pulse_end,
                              config->recover_bsd,
                              &mzf, &body_res, &consumed_until,
                              &bsd_recovery
                          );

                    free ( hdr_res.data );
                    hdr_res.data = NULL;

                    /* diagnosticky vypis - VZDY pri selhani BSD decode */
                    if ( err != WAV_ANALYZER_OK ) {
                        char fname[MZF_FILE_NAME_LENGTH + 1];
                        memcpy ( fname, saved_header.fname.name, MZF_FILE_NAME_LENGTH );
                        fname[MZF_FILE_NAME_LENGTH] = '\0';
                        for ( int j = 0; j < MZF_FILE_NAME_LENGTH; j++ ) {
                            if ( fname[j] < 0x20 || fname[j] > 0x7E ) fname[j] = '.';
                        }
                        fprintf ( stderr, "  BSD header found: \"%s\", ftype=0x%02X, decode failed: %s\n"
                                          "  Hint: use --recover-bsd to salvage partial data\n",
                                  fname, saved_header.ftype,
                                  wav_analyzer_error_string ( err ) );
                    }

                    /* informace o recovery (vzdy, i pri uspechu) */
                    if ( err == WAV_ANALYZER_OK && bsd_recovery != WAV_RECOVERY_NONE ) {
                        fprintf ( stderr, "  WARNING: BSD recovered - %s (%u bytes salvaged)\n",
                                  wav_recovery_status_string ( bsd_recovery ),
                                  mzf ? mzf->body_size : 0 );
                    }
                } else if ( refined == WAV_TAPE_FORMAT_FSK ||
                            refined == WAV_TAPE_FORMAT_SLOW ||
                            refined == WAV_TAPE_FORMAT_DIRECT ) {
                    /*
                     * FSK/SLOW/DIRECT: dvourychlostní signál.
                     * Cast 1 = preloader hlavička (právě dekódovaná, fsize=0)
                     * Cast 2 = loader body v NORMAL FM
                     * Cast 3 = uživatelská data v FSK/SLOW/DIRECT
                     */
                    uint8_t *preloader_raw = hdr_res.data;
                    uint32_t pulse_end = hdr_res.pulse_end;
                    hdr_res.data = NULL;

                    mzf_free ( mzf );
                    mzf = NULL;
                    free ( body_res.data ); body_res.data = NULL;
                    memset ( &hdr_res, 0, sizeof ( hdr_res ) );
                    memset ( &body_res, 0, sizeof ( body_res ) );

                    err = decode_nonstandard_tape_mzf (
                              seq, preloader_raw, pulse_end, refined,
                              &mzf, &body_res, &consumed_until
                          );

                    free ( preloader_raw );
                } else {
                    /*
                     * Zadny redispatch - format zustava NORMAL/CPM-CMT/atd.
                     * consumed_until nastavime z FM dekoderu (pulse_end
                     * hlavicky nebo tela, podle toho co je dale).
                     */
                    consumed_until = body_res.pulse_end > 0
                                     ? body_res.pulse_end
                                     : hdr_res.pulse_end;
                }

                if ( err == WAV_ANALYZER_OK && mzf ) {
                    st_WAV_ANALYZER_FILE_RESULT file_result;
                    memset ( &file_result, 0, sizeof ( file_result ) );
                    file_result.mzf = mzf;
                    file_result.format = refined;
                    file_result.header_crc = hdr_res.crc_status;
                    file_result.body_crc = body_res.crc_status;
                    /* pro TURBO pouzijeme datovy leader (rychlost), ne preloader */
                    file_result.leader = ( turbo_data_leader.pulse_count > 0 )
                                         ? turbo_data_leader : *leader;
                    file_result.speed_class = speed;

                    file_result.copy2_used = hdr_res.copy2_used || body_res.copy2_used;
                    file_result.consumed_until_pulse = consumed_until;
                    file_result.recovery_status = bsd_recovery;
                    if ( bsd_recovery != WAV_RECOVERY_NONE && mzf ) {
                        file_result.recovered_bytes = mzf->body_size;
                        file_result.expected_bytes = 0; /* u BSD neznáme */
                    }

                    err = result_add_file ( result, &file_result );
                    if ( err != WAV_ANALYZER_OK ) {
                        mzf_free ( mzf );
                    }

                    if ( config->verbose ) {
                        fprintf ( stderr, "  Decoded: format=%s, header CRC=%s, body CRC=%s, body_size=%u%s\n",
                                  wav_tape_format_name ( refined ),
                                  ( hdr_res.crc_status == WAV_CRC_OK ) ? "OK" : "ERROR",
                                  ( body_res.crc_status == WAV_CRC_OK ) ? "OK" : "ERROR",
                                  mzf->body_size,
                                  file_result.copy2_used ? " (Copy2)" : "" );
                    }
                } else {
                    if ( config->verbose ) {
                        fprintf ( stderr, "  Decode failed: %s\n", wav_analyzer_error_string ( err ) );
                    }
                    /* pokračujeme - nepovažujeme za fatální chybu */
                    err = WAV_ANALYZER_OK;
                }

                /* uvolníme kopie dat ve výsledcích dekódování */
                free ( hdr_res.data );
                free ( body_res.data );
            } else {
                if ( config->verbose ) {
                    fprintf ( stderr, "  Decode failed: %s\n", wav_analyzer_error_string ( err ) );
                }
                err = WAV_ANALYZER_OK;
            }
            break;
        }

        case WAV_TAPE_FORMAT_ZX_SPECTRUM:
        {
            uint8_t *tap_data = NULL;
            uint32_t tap_size = 0;
            en_WAV_CRC_STATUS crc = WAV_CRC_NOT_AVAILABLE;
            uint32_t consumed = 0;

            err = wav_decode_zx_decode_block (
                      seq, leader, &tap_data, &tap_size, &crc, &consumed );

            if ( err == WAV_ANALYZER_OK && tap_data ) {
                st_WAV_ANALYZER_FILE_RESULT file_result;
                memset ( &file_result, 0, sizeof ( file_result ) );
                file_result.tap_data = tap_data;
                file_result.tap_data_size = tap_size;
                file_result.format = WAV_TAPE_FORMAT_ZX_SPECTRUM;
                file_result.header_crc = crc;
                file_result.body_crc = WAV_CRC_NOT_AVAILABLE;
                file_result.leader = *leader;
                file_result.speed_class = speed;
                file_result.consumed_until_pulse = consumed;

                err = result_add_file ( result, &file_result );
                if ( err != WAV_ANALYZER_OK ) {
                    free ( tap_data );
                }

                if ( config->verbose ) {
                    fprintf ( stderr, "  Decoded: format=ZX SPECTRUM, flag=0x%02X, size=%u, CRC=%s\n",
                              tap_data[0], tap_size,
                              ( crc == WAV_CRC_OK ) ? "OK" : "ERROR" );
                }
            } else {
                if ( config->verbose ) {
                    fprintf ( stderr, "  ZX decode failed: %s\n",
                              wav_analyzer_error_string ( err ) );
                }
                err = WAV_ANALYZER_OK;
            }
            break;
        }

        default:
            if ( config->verbose ) {
                fprintf ( stderr, "  Format %s: no decoder available\n",
                          wav_tape_format_name ( format ) );
            }
            break;
    }

    wav_histogram_destroy ( &hist );
    return err;
}


/** @brief Minimální počet pulzů pro uznání mezery jako raw bloku. */
#define RAW_BLOCK_MIN_PULSES    50

/** @brief Počáteční kapacita pole raw bloků. */
#define RAW_BLOCK_INITIAL_CAPACITY  4


/**
 * @brief Konvertuje úsek pulzů na bitová data pro TZX Direct Recording (0x15).
 *
 * Expanduje pulzy na jednotlivé vzorky podle duration_samples.
 * Každý vzorek odpovídá jednomu bitu: level 1 = HIGH = bit 1,
 * level 0 = LOW = bit 0. Bity se balí do bajtů MSb first
 * (dle specifikace TZX bloku 0x15).
 *
 * @param seq Sekvence pulzů.
 * @param start_pulse Index prvního pulzu.
 * @param end_pulse Index za posledním pulzem.
 * @param sample_rate Vzorkovací frekvence (Hz).
 * @param[out] out_data Výstupní bitová data (alokována na heapu).
 * @param[out] out_data_size Velikost výstupních dat v bajtech.
 * @param[out] out_used_bits_last Použité bity v posledním bajtu (1-8).
 * @return WAV_ANALYZER_OK při úspěchu, WAV_ANALYZER_ERROR_ALLOC při chybě.
 *
 * @post Při úspěchu *out_data ukazuje na alokovaný buffer.
 *       Volající musí uvolnit přes free().
 */
static en_WAV_ANALYZER_ERROR pulses_to_direct_recording (
    const st_WAV_PULSE_SEQUENCE *seq,
    uint32_t start_pulse,
    uint32_t end_pulse,
    uint32_t sample_rate,
    uint8_t **out_data,
    uint32_t *out_data_size,
    uint8_t *out_used_bits_last
) {
    (void) sample_rate;

    /* spočítáme celkový počet vzorků */
    uint64_t total_samples = 0;
    for ( uint32_t i = start_pulse; i < end_pulse; i++ ) {
        total_samples += seq->pulses[i].duration_samples;
    }

    if ( total_samples == 0 ) {
        *out_data = NULL;
        *out_data_size = 0;
        *out_used_bits_last = 8;
        return WAV_ANALYZER_OK;
    }

    /* alokace bufferu */
    uint32_t byte_count = ( uint32_t ) ( ( total_samples + 7 ) / 8 );
    uint8_t *data = ( uint8_t* ) calloc ( byte_count, 1 );
    if ( !data ) return WAV_ANALYZER_ERROR_ALLOC;

    /* expandujeme pulzy na bity MSb first */
    uint64_t bit_pos = 0;
    for ( uint32_t i = start_pulse; i < end_pulse; i++ ) {
        uint32_t samples = seq->pulses[i].duration_samples;
        int level = seq->pulses[i].level;

        for ( uint32_t s = 0; s < samples && bit_pos < total_samples; s++ ) {
            if ( level ) {
                uint32_t byte_idx = ( uint32_t ) ( bit_pos / 8 );
                uint32_t bit_idx = 7 - ( uint32_t ) ( bit_pos % 8 );
                data[byte_idx] |= ( 1 << bit_idx );
            }
            bit_pos++;
        }
    }

    /* použité bity v posledním bajtu */
    uint8_t used = ( uint8_t ) ( total_samples % 8 );
    if ( used == 0 ) used = 8;

    *out_data = data;
    *out_data_size = byte_count;
    *out_used_bits_last = used;

    return WAV_ANALYZER_OK;
}


/**
 * @brief Sesbírá mezery mezi dekódovanými soubory a konvertuje je na raw bloky.
 *
 * Sestaví seznam "pokrytých" oblastí z dekódovaných souborů
 * (leader.start_index .. consumed_until_pulse), identifikuje mezery
 * a pro každou dostatečně velkou mezeru vytvoří Direct Recording blok.
 *
 * @param result Výsledek analýzy (přidá raw_blocks).
 * @param seq Sekvence pulzů.
 * @param config Konfigurace analyzéru.
 * @return WAV_ANALYZER_OK při úspěchu, WAV_ANALYZER_ERROR_ALLOC při chybě.
 */
static en_WAV_ANALYZER_ERROR collect_gaps (
    st_WAV_ANALYZER_RESULT *result,
    const st_WAV_PULSE_SEQUENCE *seq,
    const st_WAV_ANALYZER_CONFIG *config
) {
    /*
     * === 1. Sestavení pokrytých oblastí ===
     * Prosté pole (start, end) z dekódovaných souborů.
     */
    typedef struct { uint32_t start; uint32_t end; } coverage_t;

    uint32_t cov_count = result->file_count;
    coverage_t *cov = NULL;

    if ( cov_count > 0 ) {
        cov = ( coverage_t* ) malloc ( cov_count * sizeof ( coverage_t ) );
        if ( !cov ) return WAV_ANALYZER_ERROR_ALLOC;

        for ( uint32_t i = 0; i < cov_count; i++ ) {
            cov[i].start = result->files[i].leader.start_index;
            cov[i].end = result->files[i].consumed_until_pulse;
            /* pokud consumed_until_pulse == 0, nemáme info o konci */
            if ( cov[i].end == 0 ) {
                cov[i].end = cov[i].start;
            }
        }

        /* seřadíme dle start (jednoduchý insertion sort, souborů je málo) */
        for ( uint32_t i = 1; i < cov_count; i++ ) {
            coverage_t tmp = cov[i];
            uint32_t j = i;
            while ( j > 0 && cov[j - 1].start > tmp.start ) {
                cov[j] = cov[j - 1];
                j--;
            }
            cov[j] = tmp;
        }
    }

    /*
     * === 2. Identifikace mezer ===
     * Procházíme od pulzu 0 do seq->count. Oblasti mimo
     * pokrytí jsou kandidáti na raw bloky.
     */
    uint32_t current = 0;

    for ( uint32_t i = 0; i <= cov_count; i++ ) {
        uint32_t gap_start = current;
        uint32_t gap_end;

        if ( i < cov_count ) {
            gap_end = cov[i].start;
        } else {
            gap_end = seq->count;
        }

        /* přeskočíme překrývající se nebo příliš krátké mezery */
        if ( gap_end > gap_start && ( gap_end - gap_start ) >= RAW_BLOCK_MIN_PULSES ) {
            /* konvertujeme na Direct Recording */
            uint8_t *raw_data = NULL;
            uint32_t raw_size = 0;
            uint8_t used_bits = 8;

            en_WAV_ANALYZER_ERROR err = pulses_to_direct_recording (
                seq, gap_start, gap_end, seq->sample_rate,
                &raw_data, &raw_size, &used_bits
            );

            if ( err == WAV_ANALYZER_OK && raw_data && raw_size > 0 ) {
                /* přidáme do pole raw_blocks */
                if ( result->raw_block_count >= result->raw_block_capacity ) {
                    uint32_t new_cap = ( result->raw_block_capacity == 0 )
                                       ? RAW_BLOCK_INITIAL_CAPACITY
                                       : result->raw_block_capacity * 2;
                    st_WAV_ANALYZER_RAW_BLOCK *new_blocks = ( st_WAV_ANALYZER_RAW_BLOCK* ) realloc (
                        result->raw_blocks, new_cap * sizeof ( st_WAV_ANALYZER_RAW_BLOCK )
                    );
                    if ( !new_blocks ) {
                        free ( raw_data );
                        free ( cov );
                        return WAV_ANALYZER_ERROR_ALLOC;
                    }
                    result->raw_blocks = new_blocks;
                    result->raw_block_capacity = new_cap;
                }

                /* spočítáme čas a délku */
                double start_time = 0.0;
                for ( uint32_t p = 0; p < gap_start && p < seq->count; p++ ) {
                    start_time += seq->pulses[p].duration_us;
                }
                start_time /= 1000000.0;

                double duration = 0.0;
                for ( uint32_t p = gap_start; p < gap_end && p < seq->count; p++ ) {
                    duration += seq->pulses[p].duration_us;
                }
                duration /= 1000000.0;

                st_WAV_ANALYZER_RAW_BLOCK *rb = &result->raw_blocks[result->raw_block_count];
                rb->pulse_start = gap_start;
                rb->pulse_end = gap_end;
                rb->start_time_sec = start_time;
                rb->duration_sec = duration;
                rb->sample_rate = seq->sample_rate;
                rb->data = raw_data;
                rb->data_size = raw_size;
                rb->used_bits_last = used_bits;

                result->raw_block_count++;

                if ( config->verbose ) {
                    fprintf ( stderr, "Raw block: pulses %u-%u, %.3f sec at %.3f sec, %u bytes\n",
                              gap_start, gap_end, duration, start_time, raw_size );
                }
            } else {
                free ( raw_data );
            }
        }

        /* posuneme current za konec aktuální pokryté oblasti */
        if ( i < cov_count && cov[i].end > current ) {
            current = cov[i].end;
        }
    }

    free ( cov );
    return WAV_ANALYZER_OK;
}


en_WAV_ANALYZER_ERROR wav_analyzer_analyze (
    st_HANDLER *h,
    const st_WAV_ANALYZER_CONFIG *config,
    st_WAV_ANALYZER_RESULT *out_result
) {
    if ( !h || !config || !out_result ) {
        return WAV_ANALYZER_ERROR_INVALID_PARAM;
    }

    memset ( out_result, 0, sizeof ( *out_result ) );

    /* === 1. Parsování WAV hlavičky === */
    en_WAV_ERROR wav_err;
    st_WAV_SIMPLE_HEADER *sh = wav_simple_header_new_from_handler ( h, &wav_err );
    if ( !sh ) {
        return WAV_ANALYZER_ERROR_WAV_FORMAT;
    }

    out_result->sample_rate = sh->sample_rate;
    out_result->wav_duration_sec = sh->count_sec;

    if ( config->verbose ) {
        fprintf ( stderr, "WAV: %u Hz, %u-bit, %u ch, %.2f sec, %u samples\n",
                  sh->sample_rate, sh->bits_per_sample,
                  sh->channels, sh->count_sec, sh->blocks );
    }

    /* === 2. Preprocessing === */
    double *samples = NULL;
    uint32_t sample_count = 0;

    en_WAV_ANALYZER_ERROR err = wav_preprocess_run ( h, sh, config, &samples, &sample_count );
    wav_simple_header_destroy ( sh );

    if ( err != WAV_ANALYZER_OK ) {
        return err;
    }

    if ( config->verbose ) {
        fprintf ( stderr, "Preprocessing done: %u samples\n", sample_count );
    }

    /* === 3. Extrakce pulzů === */
    st_WAV_PULSE_SEQUENCE pulse_seq;
    memset ( &pulse_seq, 0, sizeof ( pulse_seq ) );

    err = wav_pulse_extract ( samples, sample_count, out_result->sample_rate,
                              config, &pulse_seq );
    free ( samples );

    if ( err != WAV_ANALYZER_OK ) {
        return err;
    }

    out_result->total_pulses = pulse_seq.count;

    /* výpočet statistik pulzů */
    wav_pulse_stats_compute ( &pulse_seq, &out_result->pulse_stats );

    if ( config->verbose ) {
        fprintf ( stderr, "Pulse extraction done: %u pulses\n", pulse_seq.count );
        if ( pulse_seq.count > 0 ) {
            fprintf ( stderr, "Pulse stats: avg=%.1f us, stddev=%.1f us, min=%.1f us, max=%.1f us\n",
                      out_result->pulse_stats.avg_us,
                      out_result->pulse_stats.stddev_us,
                      out_result->pulse_stats.min_us,
                      out_result->pulse_stats.max_us );
        }
    }

    /* === 4. Detekce leader tónů === */
    err = wav_leader_find_all ( &pulse_seq, config->min_leader_pulses,
                                config->tolerance, &out_result->leaders );
    if ( err != WAV_ANALYZER_OK ) {
        wav_pulse_sequence_destroy ( &pulse_seq );
        return err;
    }

    if ( config->verbose ) {
        fprintf ( stderr, "Leader detection done: %u leaders found\n",
                  out_result->leaders.count );
    }

    /* === 5. Pro každý leader: klasifikace + dekódování === */

    /*
     * Dvoudílné formáty (TURBO, FASTIPL) konzumují leadery
     * svých datových částí. Sledujeme consumed_until_pulse
     * pro přeskočení těchto leaderů.
     */
    uint32_t skip_until_pulse = 0;

    for ( uint32_t i = 0; i < out_result->leaders.count; i++ ) {
        /* přeskočíme leadery spotřebované předchozím dekodérem.
         * Leader přeskočíme jen pokud jeho KONEC je v consumed oblasti.
         * Leader, který začíná těsně před consumed ale většinou leží za ní,
         * je platný (nastává na hranici TURBO body → další preloader LGAP). */
        uint32_t leader_end = out_result->leaders.leaders[i].start_index
                              + out_result->leaders.leaders[i].pulse_count;
        if ( leader_end < skip_until_pulse ) {
            if ( config->verbose ) {
                fprintf ( stderr, "Leader at pulse #%u: skipped (consumed by previous decode)\n",
                          out_result->leaders.leaders[i].start_index );
            }
            continue;
        }

        uint32_t leader_consumed = 0;
        err = process_leader ( &pulse_seq, &out_result->leaders.leaders[i],
                               config, out_result, &leader_consumed );
        if ( err != WAV_ANALYZER_OK ) {
            /* chyba při zpracování jednoho leaderu - pokračujeme s dalšími */
            if ( config->verbose ) {
                fprintf ( stderr, "  Leader #%u processing failed: %s\n",
                          i, wav_analyzer_error_string ( err ) );
            }
        }

        /* aktualizujeme skip_until z consumed_until (i bez pridaneho souboru) */
        if ( leader_consumed > skip_until_pulse ) {
            skip_until_pulse = leader_consumed;
        }
        if ( out_result->file_count > 0 ) {
            uint32_t cu = out_result->files[out_result->file_count - 1].consumed_until_pulse;
            if ( cu > skip_until_pulse ) {
                skip_until_pulse = cu;
            }
        }
    }

    /*
     * === 5b. Sekundarni FM sken nepokrytych oblasti ===
     *
     * Rychle signaly (3200+ Bd pri 44100 Hz) maji tak malo vzorku
     * na pul-periodu (~4), ze kvantizacni jitter (~25%) znemoznuje
     * detekci leaderu. Pro tyto signaly skenujeme nepokryte oblasti
     * pomoci histogramove analyzy - najdeme SHORT/LONG peaky a
     * vytvorime synteticky leader pro FM dekoder.
     */
    {
        /* sestavime pokryte oblasti z dekodovanych souboru */
        uint32_t covered_end = 0;
        for ( uint32_t i = 0; i < out_result->file_count; i++ ) {
            uint32_t ce = out_result->files[i].consumed_until_pulse;
            if ( ce > covered_end ) covered_end = ce;
        }

        /* skenujeme oblast za poslednim dekodovanym souborem */
        if ( covered_end > 0 && covered_end < pulse_seq.count ) {
            uint32_t uncov_pulses = pulse_seq.count - covered_end;

            /* minimalne 50000 pulzu (cca 5 sec pri 200 us) = neprazdna oblast */
            if ( uncov_pulses >= 50000 ) {
                /*
                 * Histogramova analyza nepokryte oblasti -
                 * hledame FM signal (2 klastry, pomer 1.4-2.5).
                 */
                uint32_t hist_start = covered_end;
                uint32_t hist_count = uncov_pulses > 4000 ? 4000 : uncov_pulses;

                /* preskocime prvnich ~10000 pulzu (audio prefix, ticho) */
                uint32_t skip = uncov_pulses > 20000 ? 10000 : 0;
                hist_start += skip;
                hist_count = ( hist_start + hist_count <= pulse_seq.count )
                             ? hist_count
                             : pulse_seq.count - hist_start;

                st_WAV_HISTOGRAM uncov_hist;
                memset ( &uncov_hist, 0, sizeof ( uncov_hist ) );
                en_WAV_ANALYZER_ERROR herr = wav_histogram_analyze (
                    &pulse_seq, hist_start, hist_count,
                    config->histogram_bin_width, &uncov_hist
                );

                if ( herr == WAV_ANALYZER_OK &&
                     uncov_hist.modulation == WAV_MODULATION_FM &&
                     uncov_hist.peak_count >= 2 ) {
                    /*
                     * Nasli jsme FM signal. Vytvorime synteticky leader
                     * s prumerem z prvniho peaku (SHORT) a zavolame
                     * process_leader na zacatek FM oblasti.
                     */
                    double short_peak = uncov_hist.peaks[0].center_us;

                    /* najdeme zacatek FM signalu - prvni konzistentni pulz */
                    uint32_t fm_start = covered_end + skip;
                    for ( uint32_t p = fm_start; p < pulse_seq.count; p++ ) {
                        double dur = pulse_seq.pulses[p].duration_us;
                        if ( dur >= short_peak * 0.5 && dur <= short_peak * 3.0 ) {
                            fm_start = p;
                            break;
                        }
                    }

                    st_WAV_LEADER_INFO synth_leader;
                    memset ( &synth_leader, 0, sizeof ( synth_leader ) );
                    synth_leader.start_index = fm_start;
                    synth_leader.pulse_count = 200; /* minimalni */
                    synth_leader.avg_period_us = short_peak;

                    if ( config->verbose ) {
                        fprintf ( stderr, "FM scan: uncovered region at pulse #%u, "
                                  "histogram SHORT=%.1f us, trying FM decode\n",
                                  fm_start, short_peak );
                    }

                    uint32_t synth_consumed = 0;
                    err = process_leader ( &pulse_seq, &synth_leader,
                                           config, out_result, &synth_consumed );

                    if ( err == WAV_ANALYZER_OK &&
                         out_result->file_count > 0 &&
                         out_result->files[out_result->file_count - 1].leader.start_index == fm_start ) {
                        if ( config->verbose ) {
                            st_WAV_ANALYZER_FILE_RESULT *last = &out_result->files[out_result->file_count - 1];
                            fprintf ( stderr, "  FM scan decoded: %s, body_size=%u, CRC=%s/%s\n",
                                      wav_tape_format_name ( last->format ),
                                      last->mzf ? last->mzf->body_size : 0,
                                      last->header_crc == WAV_CRC_OK ? "OK" : "ERR",
                                      last->body_crc == WAV_CRC_OK ? "OK" : "ERR" );
                        }
                    }
                }

                wav_histogram_destroy ( &uncov_hist );
            }
        }
    }

    /*
     * === 6. Dodatecny pruchod: CPM-TAPE detekce ===
     *
     * CPM-TAPE pilot ("011" sekvence) neni detekovatelny standardnim
     * leader detektorem (variace T/2 a 3T/2 = 200%).
     * Pouzivame vlastni sync detekci, ktera hleda velmi dlouhe pulzy
     * predchazene charakteristickym pilotem.
     */
    {
        uint32_t cpmtape_search = 0;

        while ( cpmtape_search < pulse_seq.count ) {
            st_MZF *mzf = NULL;
            st_WAV_DECODE_RESULT hdr_res, body_res;
            memset ( &hdr_res, 0, sizeof ( hdr_res ) );
            memset ( &body_res, 0, sizeof ( body_res ) );
            uint32_t consumed_until = 0;

            err = wav_decode_cpmtape_decode_mzf (
                      &pulse_seq, cpmtape_search,
                      &mzf, &hdr_res, &body_res, &consumed_until
                  );

            if ( err != WAV_ANALYZER_OK || !mzf ) {
                /* zadny dalsi CPM-TAPE blok */
                free ( hdr_res.data );
                free ( body_res.data );
                break;
            }

            /* kontrola prekryvu s jiz dekodovanymi soubory */
            int overlap = 0;
            for ( uint32_t j = 0; j < out_result->file_count; j++ ) {
                uint32_t existing_start = out_result->files[j].leader.start_index;
                uint32_t existing_end = out_result->files[j].consumed_until_pulse;
                if ( existing_end == 0 ) continue;

                if ( hdr_res.pulse_start < existing_end &&
                     consumed_until > existing_start ) {
                    overlap = 1;
                    break;
                }
            }

            if ( overlap ) {
                mzf_free ( mzf );
            } else {
                st_WAV_ANALYZER_FILE_RESULT file_result;
                memset ( &file_result, 0, sizeof ( file_result ) );
                file_result.mzf = mzf;
                file_result.format = WAV_TAPE_FORMAT_CPM_TAPE;
                file_result.header_crc = hdr_res.crc_status;
                file_result.body_crc = body_res.crc_status;
                file_result.speed_class = WAV_SPEED_CLASS_FAST;
                file_result.consumed_until_pulse = consumed_until;

                err = result_add_file ( out_result, &file_result );
                if ( err != WAV_ANALYZER_OK ) {
                    mzf_free ( mzf );
                }

                if ( config->verbose ) {
                    fprintf ( stderr, "CPM-TAPE decoded: header CRC=%s, body CRC=%s, body_size=%u\n",
                              ( hdr_res.crc_status == WAV_CRC_OK ) ? "OK" : "ERROR",
                              ( body_res.crc_status == WAV_CRC_OK ) ? "OK" : "ERROR",
                              mzf->body_size );
                }
            }

            free ( hdr_res.data );
            free ( body_res.data );

            cpmtape_search = ( consumed_until > cpmtape_search )
                             ? consumed_until
                             : cpmtape_search + 1;
        }
    }

    /* === 7. Sběr neidentifikovaných bloků (raw blocks) === */
    if ( config->keep_unknown ) {
        err = collect_gaps ( out_result, &pulse_seq, config );
        if ( err != WAV_ANALYZER_OK && config->verbose ) {
            fprintf ( stderr, "Warning: raw block collection failed: %s\n",
                      wav_analyzer_error_string ( err ) );
        }
        /* chyba sběru raw bloků není fatální */
        err = WAV_ANALYZER_OK;
    }

    wav_pulse_sequence_destroy ( &pulse_seq );
    return WAV_ANALYZER_OK;
}


void wav_analyzer_result_destroy ( st_WAV_ANALYZER_RESULT *result ) {
    if ( !result ) return;

    for ( uint32_t i = 0; i < result->file_count; i++ ) {
        mzf_free ( result->files[i].mzf );
        result->files[i].mzf = NULL;
        free ( result->files[i].tap_data );
        result->files[i].tap_data = NULL;
    }
    free ( result->files );
    result->files = NULL;
    result->file_count = 0;
    result->file_capacity = 0;

    for ( uint32_t i = 0; i < result->raw_block_count; i++ ) {
        wav_analyzer_raw_block_destroy ( &result->raw_blocks[i] );
    }
    free ( result->raw_blocks );
    result->raw_blocks = NULL;
    result->raw_block_count = 0;
    result->raw_block_capacity = 0;

    wav_leader_list_destroy ( &result->leaders );
}


void wav_analyzer_print_summary (
    const st_WAV_ANALYZER_RESULT *result,
    FILE *stream
) {
    if ( !result || !stream ) return;

    fprintf ( stream, "=== WAV Analysis Summary ===\n" );
    fprintf ( stream, "Sample rate: %u Hz\n", result->sample_rate );
    fprintf ( stream, "Duration: %.2f sec\n", result->wav_duration_sec );
    fprintf ( stream, "Total pulses: %u\n", result->total_pulses );
    fprintf ( stream, "Leaders found: %u\n", result->leaders.count );
    fprintf ( stream, "Files decoded: %u\n", result->file_count );

    /* statistiky pulzů */
    if ( result->pulse_stats.count > 0 ) {
        fprintf ( stream, "\nPulse statistics:\n" );
        fprintf ( stream, "  Count:  %u\n", result->pulse_stats.count );
        fprintf ( stream, "  Avg:    %.1f us\n", result->pulse_stats.avg_us );
        fprintf ( stream, "  Stddev: %.1f us\n", result->pulse_stats.stddev_us );
        fprintf ( stream, "  Min:    %.1f us\n", result->pulse_stats.min_us );
        fprintf ( stream, "  Max:    %.1f us\n", result->pulse_stats.max_us );
    }

    for ( uint32_t i = 0; i < result->file_count; i++ ) {
        const st_WAV_ANALYZER_FILE_RESULT *f = &result->files[i];

        fprintf ( stream, "\n--- File #%u ---\n", i + 1 );
        fprintf ( stream, "Format: %s\n", wav_tape_format_name ( f->format ) );
        fprintf ( stream, "Speed class: %s\n", wav_speed_class_name ( f->speed_class ) );
        fprintf ( stream, "Leader: %u pulses, avg %.1f us, stddev %.1f us\n",
                  f->leader.pulse_count, f->leader.avg_period_us, f->leader.stddev_us );

        if ( f->tap_data && f->tap_data_size > 0 ) {
            /* ZX Spectrum blok */
            fprintf ( stream, "Checksum: %s\n",
                      ( f->header_crc == WAV_CRC_OK ) ? "OK" :
                      ( f->header_crc == WAV_CRC_ERROR ) ? "ERROR" : "N/A" );
            fprintf ( stream, "TAP flag: 0x%02X (%s)\n",
                      f->tap_data[0],
                      f->tap_data[0] == 0x00 ? "header" :
                      f->tap_data[0] == 0xFF ? "data" : "custom" );
            fprintf ( stream, "TAP size: %u bytes\n", f->tap_data_size );
        } else if ( f->mzf ) {
            fprintf ( stream, "Header CRC: %s\n",
                      ( f->header_crc == WAV_CRC_OK ) ? "OK" :
                      ( f->header_crc == WAV_CRC_ERROR ) ? "ERROR" : "N/A" );
            fprintf ( stream, "Body CRC: %s\n",
                      ( f->body_crc == WAV_CRC_OK ) ? "OK" :
                      ( f->body_crc == WAV_CRC_ERROR ) ? "ERROR" : "N/A" );

            fprintf ( stream, "MZF type: 0x%02X\n", f->mzf->header.ftype );
            fprintf ( stream, "MZF size: %u bytes\n", f->mzf->header.fsize );
            fprintf ( stream, "MZF start: 0x%04X\n", f->mzf->header.fstrt );
            fprintf ( stream, "MZF exec: 0x%04X\n", f->mzf->header.fexec );

            /* název souboru - Sharp MZ ASCII -> standardní ASCII */
            char fname[MZF_FILE_NAME_LENGTH + 1];
            memcpy ( fname, f->mzf->header.fname.name, MZF_FILE_NAME_LENGTH );
            fname[MZF_FILE_NAME_LENGTH] = '\0';
            /* nahradíme non-printable znaky tečkami */
            for ( int j = 0; j < MZF_FILE_NAME_LENGTH; j++ ) {
                if ( fname[j] < 0x20 || fname[j] > 0x7E ) fname[j] = '.';
            }
            fprintf ( stream, "MZF name: \"%s\"\n", fname );
        }

        if ( f->recovery_status != WAV_RECOVERY_NONE ) {
            fprintf ( stream, "*** RECOVERED: %s", wav_recovery_status_string ( f->recovery_status ) );
            if ( f->recovered_bytes > 0 ) {
                fprintf ( stream, " (%u bytes salvaged)", f->recovered_bytes );
            }
            fprintf ( stream, " ***\n" );
        }

        if ( f->copy2_used ) {
            fprintf ( stream, "Copy2: yes (data from second copy)\n" );
        }

        fflush ( stream );
    }

    /* raw bloky */
    if ( result->raw_block_count > 0 ) {
        double total_raw_duration = 0.0;
        for ( uint32_t i = 0; i < result->raw_block_count; i++ ) {
            total_raw_duration += result->raw_blocks[i].duration_sec;
        }

        fprintf ( stream, "\nRaw blocks: %u (%.2f sec total)\n", result->raw_block_count, total_raw_duration );
        for ( uint32_t i = 0; i < result->raw_block_count; i++ ) {
            const st_WAV_ANALYZER_RAW_BLOCK *rb = &result->raw_blocks[i];
            fprintf ( stream, "  [%u] pulses %u-%u, %.3f sec at %.3f sec, %u bytes\n",
                      i + 1, rb->pulse_start, rb->pulse_end,
                      rb->duration_sec, rb->start_time_sec, rb->data_size );
        }
    }

    fprintf ( stream, "\n" );
}


const char* wav_analyzer_version ( void ) {
    return WAV_ANALYZER_VERSION;
}
