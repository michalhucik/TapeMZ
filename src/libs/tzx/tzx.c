/**
 * @file   tzx.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Implementace TZX knihovny - parsovani bloku a generovani CMT vstreamu.
 *
 * Implementuje parsovani vsech audio bloku TZX v1.20 (0x10-0x15, 0x18, 0x20)
 * a generovani CMT vstreamu z naparsovanych dat.
 *
 * Generovani pouziva vstream (RLE kodovane pulzy) pro maximalni presnost.
 * Kazdy pulz (half-period) je preveden z T-states na pocet vzorku
 * pri dane vzorkovaci frekvenci a CPU taktu.
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
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#include "../cmt_stream/cmt_stream.h"
#include "../generic_driver/generic_driver.h"
#include "../endianity/endianity.h"

#include "tzx.h"


/* ========================================================================= */
/*  Alokatory                                                                */
/* ========================================================================= */

/** @brief Vychozi alokace pameti (obaluje malloc). */
static void* tzx_default_alloc ( size_t size ) { return malloc ( size ); }
/** @brief Vychozi alokace s nulovanim (obaluje calloc). */
static void* tzx_default_alloc0 ( size_t size ) { return calloc ( 1, size ); }
/** @brief Vychozi uvolneni pameti (obaluje free). */
static void  tzx_default_free ( void *ptr ) { free ( ptr ); }

/** @brief Vychozi alokator vyuzivajici standardni knihovni funkce. */
static const st_TZX_ALLOCATOR g_tzx_default_allocator = {
    tzx_default_alloc,
    tzx_default_alloc0,
    tzx_default_free,
};

/** @brief Aktualne aktivni alokator (vychozi = stdlib). */
static const st_TZX_ALLOCATOR *g_tzx_allocator = &g_tzx_default_allocator;


/**
 * @brief Nastavi vlastni alokator, nebo resetuje na vychozi pri NULL.
 * @param allocator Ukazatel na strukturu alokatoru, nebo NULL pro reset.
 */
void tzx_set_allocator ( const st_TZX_ALLOCATOR *allocator ) {
    g_tzx_allocator = allocator ? allocator : &g_tzx_default_allocator;
}


/* ========================================================================= */
/*  Error callback                                                           */
/* ========================================================================= */

/**
 * @brief Vychozi error callback - vypisuje chyby na stderr.
 * @param func Nazev volajici funkce.
 * @param line Cislo radku.
 * @param fmt Formatovaci retezec (printf styl).
 */
static void tzx_default_error_cb ( const char *func, int line, const char *fmt, ... ) {
    va_list args;
    fprintf ( stderr, "%s():%d - ", func, line );
    va_start ( args, fmt );
    vfprintf ( stderr, fmt, args );
    va_end ( args );
}

/** @brief Aktualne aktivni error callback. */
static tzx_error_cb g_tzx_error_cb = tzx_default_error_cb;


/**
 * @brief Nastavi vlastni error callback, nebo resetuje na vychozi pri NULL.
 * @param cb Ukazatel na callback funkci, nebo NULL pro reset.
 */
void tzx_set_error_callback ( tzx_error_cb cb ) {
    g_tzx_error_cb = cb ? cb : tzx_default_error_cb;
}


/* ========================================================================= */
/*  Konfigurace                                                              */
/* ========================================================================= */

/**
 * @brief Inicializuje konfiguraci TZX generatoru na vychozi hodnoty.
 * @param config Ukazatel na konfiguraci k inicializaci.
 */
void tzx_config_init ( st_TZX_CONFIG *config ) {
    if ( !config ) return;
    config->sample_rate = CMTSTREAM_DEFAULT_RATE;
    config->stream_type = CMT_STREAM_TYPE_VSTREAM;
    config->cpu_clock = TZX_DEFAULT_CPU_CLOCK;
}


/* ========================================================================= */
/*  Chybove retezce                                                          */
/* ========================================================================= */

/**
 * @brief Vrati textovy popis chyboveho kodu en_TZX_ERROR.
 * @param err Chybovy kod.
 * @return Ukazatel na staticky retezec s popisem chyby.
 */
const char* tzx_error_string ( en_TZX_ERROR err ) {
    switch ( err ) {
        case TZX_OK:                      return "OK";
        case TZX_ERROR_NULL_INPUT:        return "NULL input parameter";
        case TZX_ERROR_INVALID_BLOCK:     return "Invalid block data";
        case TZX_ERROR_UNSUPPORTED:       return "Unsupported block type";
        case TZX_ERROR_STREAM_CREATE:     return "Failed to create CMT stream";
        case TZX_ERROR_ALLOC:             return "Memory allocation failed";
        case TZX_ERROR_IO:                return "I/O error";
        case TZX_ERROR_INVALID_SIGNATURE: return "Invalid file signature (expected ZXTape! or TapeMZ!)";
        case TZX_ERROR_INVALID_VERSION:   return "Unsupported file version";
        case TZX_ERROR_UNEXPECTED_EOF:    return "Unexpected end of file";
        case TZX_ERROR_UNKNOWN_BLOCK:     return "Unknown block ID";
        default:                          return "Unknown TZX error";
    }
}


/* ========================================================================= */
/*  Pomocne funkce                                                           */
/* ========================================================================= */

/** @brief Precte little-endian WORD z raw dat. */
static inline uint16_t read_le16 ( const uint8_t *p ) {
    return (uint16_t) p[0] | ( (uint16_t) p[1] << 8 );
}

/** @brief Precte little-endian 3-bajtove cislo z raw dat. */
static inline uint32_t read_le24 ( const uint8_t *p ) {
    return (uint32_t) p[0] | ( (uint32_t) p[1] << 8 ) | ( (uint32_t) p[2] << 16 );
}

/** @brief Precte little-endian DWORD z raw dat. */
static inline uint32_t read_le32 ( const uint8_t *p ) {
    return (uint32_t) p[0] | ( (uint32_t) p[1] << 8 ) | ( (uint32_t) p[2] << 16 ) | ( (uint32_t) p[3] << 24 );
}


/**
 * @brief Vytvori novy prazdny vstream pro generovani.
 * @param config Konfigurace generatoru.
 * @return Novy vstream, nebo NULL pri chybe.
 */
static st_CMT_VSTREAM* tzx_create_vstream ( const st_TZX_CONFIG *config ) {
    uint32_t rate = config->sample_rate ? config->sample_rate : CMTSTREAM_DEFAULT_RATE;
    return cmt_vstream_new ( rate, CMT_VSTREAM_BYTELENGTH8, 1, CMT_STREAM_POLARITY_NORMAL );
}


/**
 * @brief Prida jeden pulz (half-period) do vstreamu.
 *
 * Prevede T-states na pocet vzorku a zapise do vstreamu.
 *
 * @param vs Vstream.
 * @param tstates Delka pulzu v T-states.
 * @param level Uroven signalu (0=low, 1=high).
 * @param config Konfigurace generatoru.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static inline int tzx_add_pulse ( st_CMT_VSTREAM *vs, uint32_t tstates, int level, const st_TZX_CONFIG *config ) {
    uint32_t rate = config->sample_rate ? config->sample_rate : CMTSTREAM_DEFAULT_RATE;
    uint32_t clock = config->cpu_clock ? config->cpu_clock : TZX_DEFAULT_CPU_CLOCK;
    uint32_t samples = (uint32_t) ( (double) tstates * (double) rate / (double) clock + 0.5 );
    if ( samples == 0 ) samples = 1;
    return cmt_vstream_add_value ( vs, level, samples );
}


/**
 * @brief Prida datove bajty do vstreamu s danymi pulznimi delkami.
 *
 * Kazdy bajt se koduje od MSb k LSb. Za kazdym bitem nasleduji
 * dva pulzy (full period): prvni s aktualni urovni, druhy s opacnou.
 *
 * @param vs Vstream.
 * @param data Data k zakodovani.
 * @param data_length Pocet bajtu.
 * @param used_bits_last Pouzite bity v poslednim bajtu (1-8).
 * @param zero_tstates Delka half-period bitu ZERO v T-states.
 * @param one_tstates Delka half-period bitu ONE v T-states.
 * @param config Konfigurace generatoru.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int tzx_add_data_bits ( st_CMT_VSTREAM *vs, const uint8_t *data, uint32_t data_length,
                                uint8_t used_bits_last, uint16_t zero_tstates, uint16_t one_tstates,
                                const st_TZX_CONFIG *config ) {

    if ( data_length == 0 ) return EXIT_SUCCESS;

    for ( uint32_t i = 0; i < data_length; i++ ) {
        uint8_t byte = data[i];
        int bits = ( i == data_length - 1 ) ? used_bits_last : 8;

        for ( int b = 0; b < bits; b++ ) {
            uint16_t pulse_len = ( byte & 0x80 ) ? one_tstates : zero_tstates;

            /* full period = 2 half-periods (high + low) */
            if ( EXIT_FAILURE == tzx_add_pulse ( vs, pulse_len, 1, config ) ) return EXIT_FAILURE;
            if ( EXIT_FAILURE == tzx_add_pulse ( vs, pulse_len, 0, config ) ) return EXIT_FAILURE;

            byte <<= 1;
        }
    }

    return EXIT_SUCCESS;
}


/**
 * @brief Prida pauzu (ticho) do vstreamu.
 *
 * Pauza je nizka uroven po dany pocet milisekund.
 * Dle specifikace: pred pauzou >= 1ms opacne urovne.
 *
 * @param vs Vstream.
 * @param pause_ms Doba pauzy v milisekundach.
 * @param config Konfigurace generatoru.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int tzx_add_pause_to_vstream ( st_CMT_VSTREAM *vs, uint16_t pause_ms, const st_TZX_CONFIG *config ) {
    if ( pause_ms == 0 ) return EXIT_SUCCESS;

    uint32_t rate = config->sample_rate ? config->sample_rate : CMTSTREAM_DEFAULT_RATE;

    /* 1ms na vysoke urovni pred prechodem na nizkou (dle specifikace) */
    uint32_t edge_samples = rate / 1000;
    if ( edge_samples == 0 ) edge_samples = 1;
    if ( EXIT_FAILURE == cmt_vstream_add_value ( vs, 1, edge_samples ) ) return EXIT_FAILURE;

    /* zbytek pauzy na nizke urovni */
    uint32_t pause_samples = (uint32_t) ( (double)( pause_ms - 1 ) * rate / 1000.0 + 0.5 );
    if ( pause_samples > 0 ) {
        if ( EXIT_FAILURE == cmt_vstream_add_value ( vs, 0, pause_samples ) ) return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


/* ========================================================================= */
/*  Parsovani bloku                                                          */
/* ========================================================================= */

en_TZX_ERROR tzx_parse_standard_speed ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_STANDARD_SPEED *result ) {
    if ( !raw_data || !result ) { g_tzx_error_cb ( __func__, __LINE__, "NULL input\n" ); return TZX_ERROR_NULL_INPUT; }
    if ( raw_length < 4 ) { g_tzx_error_cb ( __func__, __LINE__, "Block 0x10 too short: %u bytes\n", raw_length ); return TZX_ERROR_INVALID_BLOCK; }

    result->pause_ms = read_le16 ( raw_data );
    result->data_length = read_le16 ( raw_data + 2 );
    result->data = (uint8_t*) raw_data + 4;

    if ( raw_length < (uint32_t)( 4 + result->data_length ) ) {
        g_tzx_error_cb ( __func__, __LINE__, "Block 0x10 data truncated: have %u, need %u bytes\n", raw_length, 4 + result->data_length );
        return TZX_ERROR_INVALID_BLOCK;
    }

    return TZX_OK;
}


en_TZX_ERROR tzx_parse_turbo_speed ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_TURBO_SPEED *result ) {
    if ( !raw_data || !result ) { g_tzx_error_cb ( __func__, __LINE__, "NULL input\n" ); return TZX_ERROR_NULL_INPUT; }
    if ( raw_length < 0x12 ) { g_tzx_error_cb ( __func__, __LINE__, "Block 0x11 too short: %u bytes\n", raw_length ); return TZX_ERROR_INVALID_BLOCK; }

    result->pilot_pulse = read_le16 ( raw_data + 0x00 );
    result->sync1_pulse = read_le16 ( raw_data + 0x02 );
    result->sync2_pulse = read_le16 ( raw_data + 0x04 );
    result->zero_pulse  = read_le16 ( raw_data + 0x06 );
    result->one_pulse   = read_le16 ( raw_data + 0x08 );
    result->pilot_count = read_le16 ( raw_data + 0x0A );
    result->used_bits   = raw_data[0x0C];
    result->pause_ms    = read_le16 ( raw_data + 0x0D );
    result->data_length = read_le24 ( raw_data + 0x0F );
    result->data = (uint8_t*) raw_data + 0x12;

    if ( raw_length < 0x12 + result->data_length ) {
        g_tzx_error_cb ( __func__, __LINE__, "Block 0x11 data truncated: have %u, need %u bytes\n", raw_length, 0x12 + result->data_length );
        return TZX_ERROR_INVALID_BLOCK;
    }
    if ( result->used_bits == 0 ) result->used_bits = 8;

    return TZX_OK;
}


en_TZX_ERROR tzx_parse_pure_tone ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_PURE_TONE *result ) {
    if ( !raw_data || !result ) { g_tzx_error_cb ( __func__, __LINE__, "NULL input\n" ); return TZX_ERROR_NULL_INPUT; }
    if ( raw_length < 4 ) { g_tzx_error_cb ( __func__, __LINE__, "Block 0x12 too short: %u bytes\n", raw_length ); return TZX_ERROR_INVALID_BLOCK; }

    result->pulse_length = read_le16 ( raw_data );
    result->pulse_count  = read_le16 ( raw_data + 2 );

    return TZX_OK;
}


en_TZX_ERROR tzx_parse_pulse_sequence ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_PULSE_SEQUENCE *result ) {
    if ( !raw_data || !result ) { g_tzx_error_cb ( __func__, __LINE__, "NULL input\n" ); return TZX_ERROR_NULL_INPUT; }
    if ( raw_length < 1 ) { g_tzx_error_cb ( __func__, __LINE__, "Block 0x13 too short\n" ); return TZX_ERROR_INVALID_BLOCK; }

    result->pulse_count = raw_data[0];
    result->pulses = (uint16_t*) ( raw_data + 1 );

    if ( raw_length < (uint32_t)( 1 + result->pulse_count * 2 ) ) {
        g_tzx_error_cb ( __func__, __LINE__, "Block 0x13 data truncated\n" );
        return TZX_ERROR_INVALID_BLOCK;
    }

    return TZX_OK;
}


en_TZX_ERROR tzx_parse_pure_data ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_PURE_DATA *result ) {
    if ( !raw_data || !result ) { g_tzx_error_cb ( __func__, __LINE__, "NULL input\n" ); return TZX_ERROR_NULL_INPUT; }
    if ( raw_length < 0x0A ) { g_tzx_error_cb ( __func__, __LINE__, "Block 0x14 too short: %u bytes\n", raw_length ); return TZX_ERROR_INVALID_BLOCK; }

    result->zero_pulse  = read_le16 ( raw_data + 0x00 );
    result->one_pulse   = read_le16 ( raw_data + 0x02 );
    result->used_bits   = raw_data[0x04];
    result->pause_ms    = read_le16 ( raw_data + 0x05 );
    result->data_length = read_le24 ( raw_data + 0x07 );
    result->data = (uint8_t*) raw_data + 0x0A;

    if ( raw_length < 0x0A + result->data_length ) {
        g_tzx_error_cb ( __func__, __LINE__, "Block 0x14 data truncated\n" );
        return TZX_ERROR_INVALID_BLOCK;
    }
    if ( result->used_bits == 0 ) result->used_bits = 8;

    return TZX_OK;
}


en_TZX_ERROR tzx_parse_direct_recording ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_DIRECT_RECORDING *result ) {
    if ( !raw_data || !result ) { g_tzx_error_cb ( __func__, __LINE__, "NULL input\n" ); return TZX_ERROR_NULL_INPUT; }
    if ( raw_length < 0x08 ) { g_tzx_error_cb ( __func__, __LINE__, "Block 0x15 too short: %u bytes\n", raw_length ); return TZX_ERROR_INVALID_BLOCK; }

    result->tstates_per_sample = read_le16 ( raw_data + 0x00 );
    result->pause_ms           = read_le16 ( raw_data + 0x02 );
    result->used_bits           = raw_data[0x04];
    result->data_length         = read_le24 ( raw_data + 0x05 );
    result->data = (uint8_t*) raw_data + 0x08;

    if ( raw_length < 0x08 + result->data_length ) {
        g_tzx_error_cb ( __func__, __LINE__, "Block 0x15 data truncated\n" );
        return TZX_ERROR_INVALID_BLOCK;
    }
    if ( result->used_bits == 0 ) result->used_bits = 8;

    return TZX_OK;
}


en_TZX_ERROR tzx_parse_csw_recording ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_CSW_RECORDING *result ) {
    if ( !raw_data || !result ) { g_tzx_error_cb ( __func__, __LINE__, "NULL input\n" ); return TZX_ERROR_NULL_INPUT; }
    if ( raw_length < 0x0E ) { g_tzx_error_cb ( __func__, __LINE__, "Block 0x18 too short: %u bytes\n", raw_length ); return TZX_ERROR_INVALID_BLOCK; }

    /* prvni 4 bajty jsou DWORD delka bloku (uz zpracovano parserem vys) */
    uint32_t block_len = read_le32 ( raw_data );

    result->pause_ms         = read_le16 ( raw_data + 0x04 );
    result->sample_rate      = read_le24 ( raw_data + 0x06 );
    result->compression_type = raw_data[0x09];
    result->pulse_count      = read_le32 ( raw_data + 0x0A );
    result->data = (uint8_t*) raw_data + 0x0E;
    result->data_length = ( block_len > 10 ) ? block_len - 10 : 0;

    return TZX_OK;
}


en_TZX_ERROR tzx_parse_pause ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_PAUSE *result ) {
    if ( !raw_data || !result ) { g_tzx_error_cb ( __func__, __LINE__, "NULL input\n" ); return TZX_ERROR_NULL_INPUT; }
    if ( raw_length < 2 ) { g_tzx_error_cb ( __func__, __LINE__, "Block 0x20 too short: %u bytes\n", raw_length ); return TZX_ERROR_INVALID_BLOCK; }

    result->pause_ms = read_le16 ( raw_data );

    return TZX_OK;
}


/* ========================================================================= */
/*  Generovani audio streamu                                                 */
/* ========================================================================= */

st_CMT_VSTREAM* tzx_generate_standard_speed ( const st_TZX_STANDARD_SPEED *block, const st_TZX_CONFIG *config, en_TZX_ERROR *err ) {

    en_TZX_ERROR local_err = TZX_OK;
    if ( !err ) err = &local_err;

    if ( !block || !config ) { *err = TZX_ERROR_NULL_INPUT; return NULL; }

    st_CMT_VSTREAM *vs = tzx_create_vstream ( config );
    if ( !vs ) { *err = TZX_ERROR_ALLOC; return NULL; }

    /* pocet pilot pulzu podle flag bajtu */
    uint16_t pilot_count = ( block->data_length > 0 && block->data[0] < 128 )
        ? TZX_PILOT_COUNT_HEADER : TZX_PILOT_COUNT_DATA;

    /* pilot ton */
    for ( uint16_t i = 0; i < pilot_count; i++ ) {
        if ( EXIT_FAILURE == tzx_add_pulse ( vs, TZX_PILOT_PULSE_LENGTH, 1, config ) ) goto fail;
        if ( EXIT_FAILURE == tzx_add_pulse ( vs, TZX_PILOT_PULSE_LENGTH, 0, config ) ) goto fail;
    }

    /* sync pulzy */
    if ( EXIT_FAILURE == tzx_add_pulse ( vs, TZX_SYNC1_PULSE_LENGTH, 1, config ) ) goto fail;
    if ( EXIT_FAILURE == tzx_add_pulse ( vs, TZX_SYNC2_PULSE_LENGTH, 0, config ) ) goto fail;

    /* data */
    if ( EXIT_FAILURE == tzx_add_data_bits ( vs, block->data, block->data_length, 8,
                                              TZX_ZERO_BIT_PULSE_LENGTH, TZX_ONE_BIT_PULSE_LENGTH, config ) ) goto fail;

    /* pauza */
    if ( EXIT_FAILURE == tzx_add_pause_to_vstream ( vs, block->pause_ms, config ) ) goto fail;

    *err = TZX_OK;
    return vs;

fail:
    cmt_vstream_destroy ( vs );
    *err = TZX_ERROR_STREAM_CREATE;
    return NULL;
}


st_CMT_VSTREAM* tzx_generate_turbo_speed ( const st_TZX_TURBO_SPEED *block, const st_TZX_CONFIG *config, en_TZX_ERROR *err ) {

    en_TZX_ERROR local_err = TZX_OK;
    if ( !err ) err = &local_err;

    if ( !block || !config ) { *err = TZX_ERROR_NULL_INPUT; return NULL; }

    st_CMT_VSTREAM *vs = tzx_create_vstream ( config );
    if ( !vs ) { *err = TZX_ERROR_ALLOC; return NULL; }

    /* pilot ton */
    for ( uint16_t i = 0; i < block->pilot_count; i++ ) {
        if ( EXIT_FAILURE == tzx_add_pulse ( vs, block->pilot_pulse, 1, config ) ) goto fail;
        if ( EXIT_FAILURE == tzx_add_pulse ( vs, block->pilot_pulse, 0, config ) ) goto fail;
    }

    /* sync pulzy */
    if ( EXIT_FAILURE == tzx_add_pulse ( vs, block->sync1_pulse, 1, config ) ) goto fail;
    if ( EXIT_FAILURE == tzx_add_pulse ( vs, block->sync2_pulse, 0, config ) ) goto fail;

    /* data */
    if ( EXIT_FAILURE == tzx_add_data_bits ( vs, block->data, block->data_length, block->used_bits,
                                              block->zero_pulse, block->one_pulse, config ) ) goto fail;

    /* pauza */
    if ( EXIT_FAILURE == tzx_add_pause_to_vstream ( vs, block->pause_ms, config ) ) goto fail;

    *err = TZX_OK;
    return vs;

fail:
    cmt_vstream_destroy ( vs );
    *err = TZX_ERROR_STREAM_CREATE;
    return NULL;
}


st_CMT_VSTREAM* tzx_generate_pure_tone ( const st_TZX_PURE_TONE *block, const st_TZX_CONFIG *config, en_TZX_ERROR *err ) {

    en_TZX_ERROR local_err = TZX_OK;
    if ( !err ) err = &local_err;

    if ( !block || !config ) { *err = TZX_ERROR_NULL_INPUT; return NULL; }

    st_CMT_VSTREAM *vs = tzx_create_vstream ( config );
    if ( !vs ) { *err = TZX_ERROR_ALLOC; return NULL; }

    /* pulzy alternuji high/low - kazdy pulz je half-period */
    for ( uint16_t i = 0; i < block->pulse_count; i++ ) {
        int level = ( i % 2 == 0 ) ? 1 : 0;
        if ( EXIT_FAILURE == tzx_add_pulse ( vs, block->pulse_length, level, config ) ) goto fail;
    }

    *err = TZX_OK;
    return vs;

fail:
    cmt_vstream_destroy ( vs );
    *err = TZX_ERROR_STREAM_CREATE;
    return NULL;
}


st_CMT_VSTREAM* tzx_generate_pulse_sequence ( const st_TZX_PULSE_SEQUENCE *block, const st_TZX_CONFIG *config, en_TZX_ERROR *err ) {

    en_TZX_ERROR local_err = TZX_OK;
    if ( !err ) err = &local_err;

    if ( !block || !config ) { *err = TZX_ERROR_NULL_INPUT; return NULL; }

    st_CMT_VSTREAM *vs = tzx_create_vstream ( config );
    if ( !vs ) { *err = TZX_ERROR_ALLOC; return NULL; }

    /* kazdy pulz alternuje uroven */
    for ( uint8_t i = 0; i < block->pulse_count; i++ ) {
        int level = ( i % 2 == 0 ) ? 1 : 0;
        uint16_t pulse_len = read_le16 ( (const uint8_t*) &block->pulses[i] );
        if ( EXIT_FAILURE == tzx_add_pulse ( vs, pulse_len, level, config ) ) goto fail;
    }

    *err = TZX_OK;
    return vs;

fail:
    cmt_vstream_destroy ( vs );
    *err = TZX_ERROR_STREAM_CREATE;
    return NULL;
}


st_CMT_VSTREAM* tzx_generate_pure_data ( const st_TZX_PURE_DATA *block, const st_TZX_CONFIG *config, en_TZX_ERROR *err ) {

    en_TZX_ERROR local_err = TZX_OK;
    if ( !err ) err = &local_err;

    if ( !block || !config ) { *err = TZX_ERROR_NULL_INPUT; return NULL; }

    st_CMT_VSTREAM *vs = tzx_create_vstream ( config );
    if ( !vs ) { *err = TZX_ERROR_ALLOC; return NULL; }

    /* data bity (bez pilot/sync) */
    if ( EXIT_FAILURE == tzx_add_data_bits ( vs, block->data, block->data_length, block->used_bits,
                                              block->zero_pulse, block->one_pulse, config ) ) goto fail;

    /* pauza */
    if ( EXIT_FAILURE == tzx_add_pause_to_vstream ( vs, block->pause_ms, config ) ) goto fail;

    *err = TZX_OK;
    return vs;

fail:
    cmt_vstream_destroy ( vs );
    *err = TZX_ERROR_STREAM_CREATE;
    return NULL;
}


st_CMT_VSTREAM* tzx_generate_direct_recording ( const st_TZX_DIRECT_RECORDING *block, const st_TZX_CONFIG *config, en_TZX_ERROR *err ) {

    en_TZX_ERROR local_err = TZX_OK;
    if ( !err ) err = &local_err;

    if ( !block || !config ) { *err = TZX_ERROR_NULL_INPUT; return NULL; }

    st_CMT_VSTREAM *vs = tzx_create_vstream ( config );
    if ( !vs ) { *err = TZX_ERROR_ALLOC; return NULL; }

    uint32_t rate = config->sample_rate ? config->sample_rate : CMTSTREAM_DEFAULT_RATE;
    uint32_t clock = config->cpu_clock ? config->cpu_clock : TZX_DEFAULT_CPU_CLOCK;
    uint32_t samples_per_bit = (uint32_t) ( (double) block->tstates_per_sample * rate / clock + 0.5 );
    if ( samples_per_bit == 0 ) samples_per_bit = 1;

    /*
     * Kazdy bit = jedna uroven signalu (0=low, 1=high), MSb first.
     * Sousedni bity se stejnou urovni se slucuji do jednoho pulzu
     * pro efektivnejsi RLE kodovani ve vstreamu.
     */
    int current_level = -1;
    uint32_t current_samples = 0;

    for ( uint32_t i = 0; i < block->data_length; i++ ) {
        uint8_t byte = block->data[i];
        int bits = ( i == block->data_length - 1 ) ? block->used_bits : 8;

        for ( int b = 0; b < bits; b++ ) {
            int level = ( byte & 0x80 ) ? 1 : 0;

            if ( level == current_level ) {
                current_samples += samples_per_bit;
            } else {
                /* zapsat predchozi pulz */
                if ( current_level >= 0 && current_samples > 0 ) {
                    if ( EXIT_FAILURE == cmt_vstream_add_value ( vs, current_level, current_samples ) ) goto fail;
                }
                current_level = level;
                current_samples = samples_per_bit;
            }

            byte <<= 1;
        }
    }

    /* posledni pulz */
    if ( current_level >= 0 && current_samples > 0 ) {
        if ( EXIT_FAILURE == cmt_vstream_add_value ( vs, current_level, current_samples ) ) goto fail;
    }

    /* pauza */
    if ( EXIT_FAILURE == tzx_add_pause_to_vstream ( vs, block->pause_ms, config ) ) goto fail;

    *err = TZX_OK;
    return vs;

fail:
    cmt_vstream_destroy ( vs );
    *err = TZX_ERROR_STREAM_CREATE;
    return NULL;
}


st_CMT_VSTREAM* tzx_generate_pause ( uint16_t pause_ms, const st_TZX_CONFIG *config, en_TZX_ERROR *err ) {

    en_TZX_ERROR local_err = TZX_OK;
    if ( !err ) err = &local_err;

    if ( !config ) { *err = TZX_ERROR_NULL_INPUT; return NULL; }
    if ( pause_ms == 0 ) { *err = TZX_OK; return NULL; }

    st_CMT_VSTREAM *vs = tzx_create_vstream ( config );
    if ( !vs ) { *err = TZX_ERROR_ALLOC; return NULL; }

    if ( EXIT_FAILURE == tzx_add_pause_to_vstream ( vs, pause_ms, config ) ) {
        cmt_vstream_destroy ( vs );
        *err = TZX_ERROR_STREAM_CREATE;
        return NULL;
    }

    *err = TZX_OK;
    return vs;
}


/* ========================================================================= */
/*  Nazvy bloku                                                              */
/* ========================================================================= */

/**
 * @brief Vrati lidsky citelny nazev TZX bloku podle jeho ID.
 *
 * Pokryva standardni TZX bloky (0x10-0x5A).
 * Pro nezname ID vraci "Unknown".
 *
 * @param id ID bloku.
 * @return Retezec s nazvem bloku, nebo "Unknown" pro nezname ID.
 */
const char* tzx_block_id_name ( uint8_t id ) {
    switch ( id ) {
        case TZX_BLOCK_ID_STANDARD_SPEED:    return "Standard Speed Data";
        case TZX_BLOCK_ID_TURBO_SPEED:       return "Turbo Speed Data";
        case TZX_BLOCK_ID_PURE_TONE:         return "Pure Tone";
        case TZX_BLOCK_ID_PULSE_SEQUENCE:    return "Pulse Sequence";
        case TZX_BLOCK_ID_PURE_DATA:         return "Pure Data";
        case TZX_BLOCK_ID_DIRECT_RECORDING:  return "Direct Recording";
        case TZX_BLOCK_ID_CSW_RECORDING:     return "CSW Recording";
        case TZX_BLOCK_ID_GENERALIZED_DATA:  return "Generalized Data";
        case TZX_BLOCK_ID_PAUSE:             return "Pause / Stop the Tape";
        case TZX_BLOCK_ID_GROUP_START:       return "Group Start";
        case TZX_BLOCK_ID_GROUP_END:         return "Group End";
        case TZX_BLOCK_ID_JUMP:              return "Jump to Block";
        case TZX_BLOCK_ID_LOOP_START:        return "Loop Start";
        case TZX_BLOCK_ID_LOOP_END:          return "Loop End";
        case TZX_BLOCK_ID_CALL_SEQUENCE:     return "Call Sequence";
        case TZX_BLOCK_ID_RETURN_FROM_SEQ:   return "Return from Sequence";
        case TZX_BLOCK_ID_SELECT_BLOCK:      return "Select Block";
        case TZX_BLOCK_ID_STOP_48K:          return "Stop the Tape if 48K";
        case TZX_BLOCK_ID_SET_SIGNAL_LEVEL:  return "Set Signal Level";
        case TZX_BLOCK_ID_TEXT_DESCRIPTION:  return "Text Description";
        case TZX_BLOCK_ID_MESSAGE:           return "Message Block";
        case TZX_BLOCK_ID_ARCHIVE_INFO:      return "Archive Info";
        case TZX_BLOCK_ID_HARDWARE_TYPE:     return "Hardware Type";
        case TZX_BLOCK_ID_CUSTOM_INFO:       return "Custom Info";
        case TZX_BLOCK_ID_GLUE:              return "Glue Block";
        default:                             return "Unknown";
    }
}


/* ========================================================================= */
/*  Operace s hlavickou                                                      */
/* ========================================================================= */

/**
 * @brief Inicializuje TZX hlavicku na vychozi hodnoty.
 *
 * Nastavi signaturu "ZXTape!", EOF marker 0x1A a verzi 1.20.
 *
 * @param header Ukazatel na hlavicku k inicializaci.
 *
 * @pre header nesmi byt NULL.
 * @post Hlavicka obsahuje platne TZX hodnoty.
 */
void tzx_header_init ( st_TZX_HEADER *header ) {
    if ( !header ) return;
    memcpy ( header->signature, TZX_SIGNATURE, TZX_SIGNATURE_LENGTH );
    header->eof_marker = TZX_EOF_MARKER;
    header->ver_major = TZX_VERSION_MAJOR;
    header->ver_minor = TZX_VERSION_MINOR;
}


/**
 * @brief Zjisti, zda hlavicka obsahuje TZX signaturu "ZXTape!".
 *
 * @param header Ukazatel na hlavicku.
 * @return true pokud je signatura "ZXTape!", false jinak.
 */
bool tzx_header_is_tzx ( const st_TZX_HEADER *header ) {
    if ( !header ) return false;
    return ( memcmp ( header->signature, TZX_SIGNATURE, TZX_SIGNATURE_LENGTH ) == 0 );
}


/**
 * @brief Nacte TZX/TMZ hlavicku z handleru.
 *
 * Cte 10 bajtu od offsetu 0 a overuje signaturu (ZXTape! nebo TapeMZ!)
 * a EOF marker.
 *
 * @param h Otevreny handler.
 * @param header Vystupni buffer pro hlavicku.
 * @return TZX_OK pri uspechu, jinak chybovy kod.
 *
 * @pre h musi byt otevreny a pripraveny ke cteni.
 * @post Pri uspechu obsahuje header platnou hlavicku.
 */
en_TZX_ERROR tzx_read_header ( st_HANDLER *h, st_TZX_HEADER *header ) {
    if ( !h || !header ) return TZX_ERROR_IO;

    if ( EXIT_SUCCESS != generic_driver_read ( h, 0, header, sizeof ( st_TZX_HEADER ) ) ) {
        g_tzx_error_cb ( __func__, __LINE__, "Failed to read TZX header\n" );
        return TZX_ERROR_IO;
    }

    /* overeni signatury - akceptuje "ZXTape!" i "TapeMZ!" */
    static const char tmz_sig[] = "TapeMZ!";
    if ( memcmp ( header->signature, TZX_SIGNATURE, TZX_SIGNATURE_LENGTH ) != 0 &&
         memcmp ( header->signature, tmz_sig, TZX_SIGNATURE_LENGTH ) != 0 ) {
        g_tzx_error_cb ( __func__, __LINE__, "Invalid signature: expected ZXTape! or TapeMZ!\n" );
        return TZX_ERROR_INVALID_SIGNATURE;
    }

    /* overeni EOF markeru */
    if ( header->eof_marker != TZX_EOF_MARKER ) {
        g_tzx_error_cb ( __func__, __LINE__, "Invalid EOF marker: 0x%02X (expected 0x1A)\n", header->eof_marker );
        return TZX_ERROR_INVALID_SIGNATURE;
    }

    return TZX_OK;
}


/**
 * @brief Zapise TZX hlavicku do handleru na offset 0.
 *
 * @param h Otevreny handler pro zapis.
 * @param header Hlavicka k zapisu.
 * @return TZX_OK pri uspechu, jinak chybovy kod.
 *
 * @pre h musi byt otevreny pro zapis.
 */
en_TZX_ERROR tzx_write_header ( st_HANDLER *h, const st_TZX_HEADER *header ) {
    if ( !h || !header ) return TZX_ERROR_IO;

    if ( EXIT_SUCCESS != generic_driver_write ( h, 0, (void*) header, sizeof ( st_TZX_HEADER ) ) ) {
        g_tzx_error_cb ( __func__, __LINE__, "Failed to write TZX header\n" );
        return TZX_ERROR_IO;
    }

    return TZX_OK;
}


/* ========================================================================= */
/*  Pomocne funkce pro cteni bloku                                           */
/* ========================================================================= */

/**
 * @brief Zjisti delku dat bloku podle jeho ID (dle TZX/TMZ konvence).
 *
 * Vetsina bloku ma 4bajtovou delku za ID. Specialni pripady:
 * - 0x10: 2B pauza + 2B delka dat
 * - 0x20: 2B pauza (zadna dalsi data)
 * - 0x22, 0x25: zadna data (0 bajtu)
 * - 0x24: 2B repeat count
 *
 * @param h Otevreny handler.
 * @param offset Aktualni offset v handleru (ukazuje na bajt ZA blokovym ID).
 * @param id ID bloku.
 * @param[out] block_data_length Vystupni delka dat bloku.
 * @param[out] total_consumed Celkovy pocet bajtu spotrebovanych timto blokem
 *             (vcetne hlavicky, bez ID bajtu).
 * @return TZX_OK pri uspechu, jinak chybovy kod.
 */
static en_TZX_ERROR tzx_read_block_length ( st_HANDLER *h, uint32_t offset, uint8_t id,
                                             uint32_t *block_data_length, uint32_t *total_consumed ) {

    switch ( id ) {

        /* Blok 0x10 - Standard Speed Data: 2B pauza + 2B delka + data */
        case TZX_BLOCK_ID_STANDARD_SPEED:
        {
            uint8_t buf[4];
            if ( EXIT_SUCCESS != generic_driver_read ( h, offset, buf, 4 ) ) {
                return TZX_ERROR_IO;
            }
            uint16_t data_len = (uint16_t) buf[2] | ( (uint16_t) buf[3] << 8 );
            *block_data_length = 4 + data_len;
            *total_consumed = *block_data_length;
            return TZX_OK;
        }

        /* Blok 0x20 - Pause: 2B pauza */
        case TZX_BLOCK_ID_PAUSE:
        {
            *block_data_length = 2;
            *total_consumed = 2;
            return TZX_OK;
        }

        /* Bloky bez dat */
        case TZX_BLOCK_ID_GROUP_END:
        case TZX_BLOCK_ID_LOOP_END:
        {
            *block_data_length = 0;
            *total_consumed = 0;
            return TZX_OK;
        }

        /* Blok 0x24 - Loop Start: 2B repeat count */
        case TZX_BLOCK_ID_LOOP_START:
        /* Blok 0x23 - Jump: 2B relative offset */
        case TZX_BLOCK_ID_JUMP:
        {
            *block_data_length = 2;
            *total_consumed = 2;
            return TZX_OK;
        }

        /* Blok 0x27 - Return from Sequence: zadna data */
        case TZX_BLOCK_ID_RETURN_FROM_SEQ:
        {
            *block_data_length = 0;
            *total_consumed = 0;
            return TZX_OK;
        }

        /*
         * Blok 0x11 - Turbo Speed Data:
         * 2B pilot + 2B sync1 + 2B sync2 + 2B zero + 2B one +
         * 2B pilot_tone + 1B used + 2B pause + 3B data_len + data
         * Hlavicka: 15B, data_len na offsetu 0x0F-0x11.
         */
        case TZX_BLOCK_ID_TURBO_SPEED:
        {
            uint8_t buf[18];
            if ( EXIT_SUCCESS != generic_driver_read ( h, offset, buf, 18 ) ) {
                return TZX_ERROR_IO;
            }
            uint32_t data_len = (uint32_t) buf[15]
                              | ( (uint32_t) buf[16] << 8 )
                              | ( (uint32_t) buf[17] << 16 );
            *block_data_length = 18 + data_len;
            *total_consumed = *block_data_length;
            return TZX_OK;
        }

        /*
         * Blok 0x14 - Pure Data:
         * 2B zero + 2B one + 1B used + 2B pause + 3B data_len + data
         * Hlavicka: 10B, data_len na offsetu 0x07-0x09.
         */
        case TZX_BLOCK_ID_PURE_DATA:
        {
            uint8_t buf[10];
            if ( EXIT_SUCCESS != generic_driver_read ( h, offset, buf, 10 ) ) {
                return TZX_ERROR_IO;
            }
            uint32_t data_len = (uint32_t) buf[7]
                              | ( (uint32_t) buf[8] << 8 )
                              | ( (uint32_t) buf[9] << 16 );
            *block_data_length = 10 + data_len;
            *total_consumed = *block_data_length;
            return TZX_OK;
        }

        /*
         * Blok 0x15 - Direct Recording:
         * 2B tstates + 2B pause + 1B used + 3B data_len + data
         * Hlavicka: 8B, data_len na offsetu 0x05-0x07.
         */
        case TZX_BLOCK_ID_DIRECT_RECORDING:
        {
            uint8_t buf[8];
            if ( EXIT_SUCCESS != generic_driver_read ( h, offset, buf, 8 ) ) {
                return TZX_ERROR_IO;
            }
            uint32_t data_len = (uint32_t) buf[5]
                              | ( (uint32_t) buf[6] << 8 )
                              | ( (uint32_t) buf[7] << 16 );
            *block_data_length = 8 + data_len;
            *total_consumed = *block_data_length;
            return TZX_OK;
        }

        /* Blok 0x12 - Pure Tone: 2B delka + 2B pocet = fix 4B */
        case TZX_BLOCK_ID_PURE_TONE:
        {
            *block_data_length = 4;
            *total_consumed = 4;
            return TZX_OK;
        }

        /* Blok 0x13 - Pulse Sequence: 1B count + count*2B */
        case TZX_BLOCK_ID_PULSE_SEQUENCE:
        {
            uint8_t count;
            if ( EXIT_SUCCESS != generic_driver_read ( h, offset, &count, 1 ) ) {
                return TZX_ERROR_IO;
            }
            *block_data_length = 1 + (uint32_t) count * 2;
            *total_consumed = *block_data_length;
            return TZX_OK;
        }

        /* Blok 0x21 - Group Start: 1B len + text */
        case TZX_BLOCK_ID_GROUP_START:
        /* Blok 0x30 - Text Description: 1B len + text */
        case TZX_BLOCK_ID_TEXT_DESCRIPTION:
        {
            uint8_t len;
            if ( EXIT_SUCCESS != generic_driver_read ( h, offset, &len, 1 ) ) {
                return TZX_ERROR_IO;
            }
            *block_data_length = 1 + len;
            *total_consumed = *block_data_length;
            return TZX_OK;
        }

        /* Blok 0x31 - Message: 1B time + 1B len + text */
        case TZX_BLOCK_ID_MESSAGE:
        {
            uint8_t buf[2];
            if ( EXIT_SUCCESS != generic_driver_read ( h, offset, buf, 2 ) ) {
                return TZX_ERROR_IO;
            }
            *block_data_length = 2 + buf[1];
            *total_consumed = *block_data_length;
            return TZX_OK;
        }

        /* Blok 0x33 - Hardware Type: 1B count + count*3B */
        case TZX_BLOCK_ID_HARDWARE_TYPE:
        {
            uint8_t count;
            if ( EXIT_SUCCESS != generic_driver_read ( h, offset, &count, 1 ) ) {
                return TZX_ERROR_IO;
            }
            *block_data_length = 1 + (uint32_t) count * 3;
            *total_consumed = *block_data_length;
            return TZX_OK;
        }

        /* Blok 0x26 - Call Sequence: 2B count + count*2B offsets */
        case TZX_BLOCK_ID_CALL_SEQUENCE:
        {
            uint8_t buf[2];
            if ( EXIT_SUCCESS != generic_driver_read ( h, offset, buf, 2 ) ) {
                return TZX_ERROR_IO;
            }
            uint16_t count = (uint16_t) buf[0] | ( (uint16_t) buf[1] << 8 );
            *block_data_length = 2 + (uint32_t) count * 2;
            *total_consumed = *block_data_length;
            return TZX_OK;
        }

        /* Blok 0x28 - Select Block: 2B delka + data */
        case TZX_BLOCK_ID_SELECT_BLOCK:
        /* Blok 0x32 - Archive Info: 2B delka + data */
        case TZX_BLOCK_ID_ARCHIVE_INFO:
        {
            uint8_t buf[2];
            if ( EXIT_SUCCESS != generic_driver_read ( h, offset, buf, 2 ) ) {
                return TZX_ERROR_IO;
            }
            uint16_t len = (uint16_t) buf[0] | ( (uint16_t) buf[1] << 8 );
            *block_data_length = 2 + len;
            *total_consumed = *block_data_length;
            return TZX_OK;
        }

        /* Blok 0x35 - Custom Info: 10B ASCII ID + 4B DWORD delka + data */
        case TZX_BLOCK_ID_CUSTOM_INFO:
        {
            uint32_t length;
            if ( EXIT_SUCCESS != generic_driver_read ( h, offset + 10, &length, 4 ) ) {
                return TZX_ERROR_IO;
            }
            length = endianity_bswap32_LE ( length );
            *block_data_length = 10 + 4 + length;
            *total_consumed = *block_data_length;
            return TZX_OK;
        }

        /* Blok 0x5A - Glue Block: fix 9B (bez DWORD delky) */
        case TZX_BLOCK_ID_GLUE:
        {
            *block_data_length = 9;
            *total_consumed = 9;
            return TZX_OK;
        }

        /* Vsechny ostatni bloky: 4B delka za ID */
        default:
        {
            uint32_t length;
            if ( EXIT_SUCCESS != generic_driver_read ( h, offset, &length, 4 ) ) {
                return TZX_ERROR_IO;
            }
            length = endianity_bswap32_LE ( length );
            *block_data_length = length;
            *total_consumed = 4 + length;
            return TZX_OK;
        }
    }
}


/* ========================================================================= */
/*  Nacitani a ukladani TZX souboru                                          */
/* ========================================================================= */

/**
 * @brief Nacte cely TZX/TMZ soubor (hlavicku + vsechny bloky) z handleru.
 *
 * Prochazi bloky od offsetu 10 (za hlavickou) az do konce souboru.
 * Kazdy blok alokuje samostatne a uklada jeho surova data.
 * Pole bloku se dynamicky zvetsuje.
 *
 * @param h Otevreny handler s TZX/TMZ daty.
 * @param err Vystupni chybovy kod (muze byt NULL).
 * @return Ukazatel na novou st_TZX_FILE strukturu, nebo NULL pri chybe.
 *
 * @pre h musi byt otevreny a pripraveny ke cteni.
 * @post Volajici vlastni vracenou strukturu a musi ji uvolnit pres tzx_free().
 */
st_TZX_FILE* tzx_load ( st_HANDLER *h, en_TZX_ERROR *err ) {

    en_TZX_ERROR local_err = TZX_OK;
    if ( !err ) err = &local_err;

    if ( !h ) {
        *err = TZX_ERROR_IO;
        return NULL;
    }

    /* alokace struktury souboru */
    st_TZX_FILE *file = g_tzx_allocator->alloc0 ( sizeof ( st_TZX_FILE ) );
    if ( !file ) {
        g_tzx_error_cb ( __func__, __LINE__, "Failed to allocate TZX file structure\n" );
        *err = TZX_ERROR_ALLOC;
        return NULL;
    }

    /* nacteni hlavicky */
    *err = tzx_read_header ( h, &file->header );
    if ( *err != TZX_OK ) {
        g_tzx_allocator->free ( file );
        return NULL;
    }

    /* rozpoznani signatury */
    static const char tmz_sig[] = "TapeMZ!";
    file->is_tmz = ( memcmp ( file->header.signature, tmz_sig, TZX_SIGNATURE_LENGTH ) == 0 );

    /* cteni bloku */
    uint32_t offset = sizeof ( st_TZX_HEADER );
    uint32_t capacity = 16;
    uint32_t count = 0;

    st_TZX_BLOCK *blocks = g_tzx_allocator->alloc0 ( capacity * sizeof ( st_TZX_BLOCK ) );
    if ( !blocks ) {
        g_tzx_error_cb ( __func__, __LINE__, "Failed to allocate block array\n" );
        g_tzx_allocator->free ( file );
        *err = TZX_ERROR_ALLOC;
        return NULL;
    }

    while ( 1 ) {
        /* cteni ID bloku */
        uint8_t id;
        if ( EXIT_SUCCESS != generic_driver_read ( h, offset, &id, 1 ) ) {
            /* konec souboru - normalni ukonceni */
            break;
        }
        offset += 1;

        /* zjisteni delky bloku */
        uint32_t block_data_length = 0;
        uint32_t total_consumed = 0;

        *err = tzx_read_block_length ( h, offset, id, &block_data_length, &total_consumed );
        if ( *err != TZX_OK ) {
            g_tzx_error_cb ( __func__, __LINE__, "Failed to read block 0x%02X length at offset %u\n", id, offset );
            break;
        }

        /* zvetseni pole pokud je potreba */
        if ( count >= capacity ) {
            capacity *= 2;
            st_TZX_BLOCK *new_blocks = g_tzx_allocator->alloc0 ( capacity * sizeof ( st_TZX_BLOCK ) );
            if ( !new_blocks ) {
                g_tzx_error_cb ( __func__, __LINE__, "Failed to grow block array\n" );
                for ( uint32_t i = 0; i < count; i++ ) {
                    if ( blocks[i].data ) g_tzx_allocator->free ( blocks[i].data );
                }
                g_tzx_allocator->free ( blocks );
                g_tzx_allocator->free ( file );
                *err = TZX_ERROR_ALLOC;
                return NULL;
            }
            memcpy ( new_blocks, blocks, count * sizeof ( st_TZX_BLOCK ) );
            g_tzx_allocator->free ( blocks );
            blocks = new_blocks;
        }

        /* naplneni bloku */
        blocks[count].id = id;
        blocks[count].length = block_data_length;
        blocks[count].data = NULL;

        if ( total_consumed > 0 ) {
            blocks[count].data = g_tzx_allocator->alloc ( total_consumed );
            if ( !blocks[count].data ) {
                g_tzx_error_cb ( __func__, __LINE__, "Failed to allocate block data (%u bytes)\n", total_consumed );
                for ( uint32_t i = 0; i < count; i++ ) {
                    if ( blocks[i].data ) g_tzx_allocator->free ( blocks[i].data );
                }
                g_tzx_allocator->free ( blocks );
                g_tzx_allocator->free ( file );
                *err = TZX_ERROR_ALLOC;
                return NULL;
            }

            if ( EXIT_SUCCESS != generic_driver_read ( h, offset, blocks[count].data, total_consumed ) ) {
                g_tzx_error_cb ( __func__, __LINE__, "Failed to read block 0x%02X data at offset %u\n", id, offset );
                g_tzx_allocator->free ( blocks[count].data );
                break;
            }

            blocks[count].length = total_consumed;
        }

        offset += total_consumed;
        count++;
    }

    file->block_count = count;
    file->block_capacity = capacity;
    file->blocks = blocks;
    *err = TZX_OK;

    return file;
}


/**
 * @brief Zapise cely TZX soubor (hlavicku + vsechny bloky) do handleru.
 *
 * @param h Otevreny handler pro zapis.
 * @param file TZX soubor k zapisu.
 * @return TZX_OK pri uspechu, jinak chybovy kod.
 *
 * @pre h musi byt otevreny pro zapis, file nesmi byt NULL.
 */
en_TZX_ERROR tzx_save ( st_HANDLER *h, const st_TZX_FILE *file ) {

    if ( !h || !file ) return TZX_ERROR_IO;

    /* zapis hlavicky */
    en_TZX_ERROR err = tzx_write_header ( h, &file->header );
    if ( err != TZX_OK ) return err;

    uint32_t offset = sizeof ( st_TZX_HEADER );

    /* zapis bloku */
    for ( uint32_t i = 0; i < file->block_count; i++ ) {

        const st_TZX_BLOCK *block = &file->blocks[i];

        /* zapis ID */
        if ( EXIT_SUCCESS != generic_driver_write ( h, offset, (void*) &block->id, 1 ) ) {
            g_tzx_error_cb ( __func__, __LINE__, "Failed to write block ID 0x%02X\n", block->id );
            return TZX_ERROR_IO;
        }
        offset += 1;

        /* zapis dat bloku (vcetne hlavicky bloku - delka je soucasti dat) */
        if ( block->length > 0 && block->data ) {
            if ( EXIT_SUCCESS != generic_driver_write ( h, offset, block->data, block->length ) ) {
                g_tzx_error_cb ( __func__, __LINE__, "Failed to write block 0x%02X data\n", block->id );
                return TZX_ERROR_IO;
            }
            offset += block->length;
        }
    }

    return TZX_OK;
}


/**
 * @brief Uvolni st_TZX_FILE strukturu vcetne vsech bloku a jejich dat.
 *
 * Bezpecne volat s NULL (no-op).
 *
 * @param file Ukazatel na strukturu k uvolneni (muze byt NULL).
 */
void tzx_free ( st_TZX_FILE *file ) {
    if ( !file ) return;

    if ( file->blocks ) {
        for ( uint32_t i = 0; i < file->block_count; i++ ) {
            if ( file->blocks[i].data ) {
                g_tzx_allocator->free ( file->blocks[i].data );
            }
        }
        g_tzx_allocator->free ( file->blocks );
    }

    g_tzx_allocator->free ( file );
}


/* ========================================================================= */
/*  Manipulace s bloky                                                       */
/* ========================================================================= */


/**
 * @brief Zajisti dostatecnou kapacitu pole bloku.
 *
 * Pokud je aktualni kapacita mensi nez pozadovana, zvetsuje pole
 * na dvojnasobek (nebo na min_capacity, pokud je vetsi).
 *
 * @param file TZX soubor.
 * @param min_capacity Minimalni pozadovana kapacita.
 * @return TZX_OK pri uspechu, TZX_ERROR_ALLOC pri selhani alokace.
 */
static en_TZX_ERROR tzx_ensure_capacity ( st_TZX_FILE *file, uint32_t min_capacity ) {

    if ( file->block_capacity >= min_capacity ) return TZX_OK;

    uint32_t new_cap = file->block_capacity;
    if ( new_cap == 0 ) new_cap = 16;
    while ( new_cap < min_capacity ) new_cap *= 2;

    st_TZX_BLOCK *new_blocks = g_tzx_allocator->alloc0 ( new_cap * sizeof ( st_TZX_BLOCK ) );
    if ( !new_blocks ) {
        g_tzx_error_cb ( __func__, __LINE__, "Failed to grow block array to %u\n", new_cap );
        return TZX_ERROR_ALLOC;
    }

    if ( file->blocks && file->block_count > 0 ) {
        memcpy ( new_blocks, file->blocks, file->block_count * sizeof ( st_TZX_BLOCK ) );
    }
    if ( file->blocks ) {
        g_tzx_allocator->free ( file->blocks );
    }

    file->blocks = new_blocks;
    file->block_capacity = new_cap;
    return TZX_OK;
}


en_TZX_ERROR tzx_file_remove_block ( st_TZX_FILE *file, uint32_t index ) {

    if ( !file ) return TZX_ERROR_NULL_INPUT;
    if ( index >= file->block_count ) return TZX_ERROR_INVALID_BLOCK;

    /* uvolneni dat bloku */
    if ( file->blocks[index].data ) {
        g_tzx_allocator->free ( file->blocks[index].data );
    }

    /* posun zbylych bloku doleva */
    for ( uint32_t i = index; i + 1 < file->block_count; i++ ) {
        file->blocks[i] = file->blocks[i + 1];
    }
    file->block_count--;

    return TZX_OK;
}


en_TZX_ERROR tzx_file_insert_block ( st_TZX_FILE *file, uint32_t index, const st_TZX_BLOCK *block ) {

    if ( !file || !block ) return TZX_ERROR_NULL_INPUT;
    if ( index > file->block_count ) return TZX_ERROR_INVALID_BLOCK;

    /* zvetseni pole pokud je potreba */
    en_TZX_ERROR err = tzx_ensure_capacity ( file, file->block_count + 1 );
    if ( err != TZX_OK ) return err;

    /* posun bloku doprava */
    for ( uint32_t i = file->block_count; i > index; i-- ) {
        file->blocks[i] = file->blocks[i - 1];
    }

    /* vlozeni noveho bloku (kopie struktury, ownership dat predan) */
    file->blocks[index] = *block;
    file->block_count++;

    return TZX_OK;
}


en_TZX_ERROR tzx_file_append_block ( st_TZX_FILE *file, const st_TZX_BLOCK *block ) {

    if ( !file ) return TZX_ERROR_NULL_INPUT;
    return tzx_file_insert_block ( file, file->block_count, block );
}


en_TZX_ERROR tzx_file_move_block ( st_TZX_FILE *file, uint32_t from, uint32_t to ) {

    if ( !file ) return TZX_ERROR_NULL_INPUT;
    if ( from >= file->block_count ) return TZX_ERROR_INVALID_BLOCK;
    if ( to >= file->block_count ) return TZX_ERROR_INVALID_BLOCK;
    if ( from == to ) return TZX_OK;

    /* ulozeni bloku */
    st_TZX_BLOCK saved = file->blocks[from];

    /* posun bloku */
    if ( from < to ) {
        for ( uint32_t i = from; i < to; i++ ) {
            file->blocks[i] = file->blocks[i + 1];
        }
    } else {
        for ( uint32_t i = from; i > to; i-- ) {
            file->blocks[i] = file->blocks[i - 1];
        }
    }

    file->blocks[to] = saved;
    return TZX_OK;
}


en_TZX_ERROR tzx_file_merge ( st_TZX_FILE *dst, st_TZX_FILE *src ) {

    if ( !dst || !src ) return TZX_ERROR_NULL_INPUT;

    if ( src->block_count == 0 ) return TZX_OK;

    /* zvetseni ciloveho pole */
    en_TZX_ERROR err = tzx_ensure_capacity ( dst, dst->block_count + src->block_count );
    if ( err != TZX_OK ) return err;

    /* kopirovani bloku (ownership dat predan) */
    memcpy ( &dst->blocks[dst->block_count], src->blocks,
             src->block_count * sizeof ( st_TZX_BLOCK ) );
    dst->block_count += src->block_count;

    /* vyprazdneni src (data jsou nyni vlastnena dst) */
    g_tzx_allocator->free ( src->blocks );
    src->blocks = NULL;
    src->block_count = 0;
    src->block_capacity = 0;

    return TZX_OK;
}


/* ========================================================================= */
/*  Vytvareni TZX info bloku                                                 */
/* ========================================================================= */


en_TZX_ERROR tzx_block_create_text_description ( const char *text, st_TZX_BLOCK *block ) {

    if ( !text || !block ) return TZX_ERROR_NULL_INPUT;

    uint8_t len = (uint8_t) strlen ( text );
    if ( strlen ( text ) > 255 ) len = 255;

    uint32_t total = 1 + len;
    uint8_t *data = g_tzx_allocator->alloc ( total );
    if ( !data ) return TZX_ERROR_ALLOC;

    data[0] = len;
    memcpy ( data + 1, text, len );

    block->id = TZX_BLOCK_ID_TEXT_DESCRIPTION;
    block->length = total;
    block->data = data;

    return TZX_OK;
}


en_TZX_ERROR tzx_block_create_message ( const char *text, uint8_t time_seconds, st_TZX_BLOCK *block ) {

    if ( !text || !block ) return TZX_ERROR_NULL_INPUT;

    uint8_t len = (uint8_t) strlen ( text );
    if ( strlen ( text ) > 255 ) len = 255;

    uint32_t total = 2 + len;
    uint8_t *data = g_tzx_allocator->alloc ( total );
    if ( !data ) return TZX_ERROR_ALLOC;

    data[0] = time_seconds;
    data[1] = len;
    memcpy ( data + 2, text, len );

    block->id = TZX_BLOCK_ID_MESSAGE;
    block->length = total;
    block->data = data;

    return TZX_OK;
}


en_TZX_ERROR tzx_block_create_archive_info ( const st_TZX_ARCHIVE_ENTRY *entries, uint8_t count, st_TZX_BLOCK *block ) {

    if ( !entries || !block || count == 0 ) return TZX_ERROR_NULL_INPUT;

    /* spocitani celkove delky zaznamu */
    uint32_t records_size = 0;
    for ( uint8_t i = 0; i < count; i++ ) {
        if ( !entries[i].text ) return TZX_ERROR_NULL_INPUT;
        size_t slen = strlen ( entries[i].text );
        if ( slen > 255 ) slen = 255;
        records_size += 2 + (uint32_t) slen; /* 1B type + 1B len + text */
    }

    /* format bloku 0x32: [2B celkova delka za WORD][1B count][zaznamy...] */
    uint16_t body_length = (uint16_t) ( 1 + records_size );
    uint32_t total = 2 + body_length;

    uint8_t *data = g_tzx_allocator->alloc ( total );
    if ( !data ) return TZX_ERROR_ALLOC;

    /* WORD delka (LE) */
    data[0] = (uint8_t) ( body_length & 0xFF );
    data[1] = (uint8_t) ( body_length >> 8 );
    data[2] = count;

    uint32_t pos = 3;
    for ( uint8_t i = 0; i < count; i++ ) {
        size_t slen = strlen ( entries[i].text );
        if ( slen > 255 ) slen = 255;
        data[pos++] = entries[i].type_id;
        data[pos++] = (uint8_t) slen;
        memcpy ( data + pos, entries[i].text, slen );
        pos += (uint32_t) slen;
    }

    block->id = TZX_BLOCK_ID_ARCHIVE_INFO;
    block->length = total;
    block->data = data;

    return TZX_OK;
}


en_TZX_ERROR tzx_block_create_direct_recording (
    uint16_t tstates_per_sample,
    uint16_t pause_ms,
    uint8_t used_bits,
    const uint8_t *data,
    uint32_t data_length,
    st_TZX_BLOCK *block
) {
    if ( !block ) return TZX_ERROR_NULL_INPUT;
    if ( data_length > 0 && !data ) return TZX_ERROR_NULL_INPUT;
    if ( used_bits < 1 || used_bits > 8 ) return TZX_ERROR_INVALID_BLOCK;

    /*
     * Binární formát bloku 0x15:
     * [2B tstates_per_sample LE]
     * [2B pause_ms LE]
     * [1B used_bits_in_last_byte]
     * [3B data_length LE]
     * [data_length bajtu dat]
     */
    uint32_t header_size = 2 + 2 + 1 + 3;
    uint32_t total = header_size + data_length;

    uint8_t *buf = g_tzx_allocator->alloc ( total );
    if ( !buf ) return TZX_ERROR_ALLOC;

    /* tstates_per_sample (LE) */
    buf[0] = ( uint8_t ) ( tstates_per_sample & 0xFF );
    buf[1] = ( uint8_t ) ( tstates_per_sample >> 8 );

    /* pause_ms (LE) */
    buf[2] = ( uint8_t ) ( pause_ms & 0xFF );
    buf[3] = ( uint8_t ) ( pause_ms >> 8 );

    /* used_bits_in_last_byte */
    buf[4] = used_bits;

    /* data_length (3B LE) */
    buf[5] = ( uint8_t ) ( data_length & 0xFF );
    buf[6] = ( uint8_t ) ( ( data_length >> 8 ) & 0xFF );
    buf[7] = ( uint8_t ) ( ( data_length >> 16 ) & 0xFF );

    /* data */
    if ( data_length > 0 ) {
        memcpy ( buf + header_size, data, data_length );
    }

    block->id = TZX_BLOCK_ID_DIRECT_RECORDING;
    block->length = total;
    block->data = buf;

    return TZX_OK;
}


const char* tzx_version ( void ) {
    return TZX_VERSION;
}


const char* tzx_format_version ( void ) {
    return TZX_FORMAT_VERSION;
}
