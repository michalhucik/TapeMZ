/**
 * @file   tmz_player.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.1.0
 * @brief  Implementace TMZ playeru - generovani CMT audio streamu z TMZ/TZX bloku.
 *
 * Player prehrava jednotlive TMZ/TZX bloky na CMT audio streamy pomoci
 * existujicich knihoven mztape (Sharp MZ formaty) a zxtape (ZX Spectrum).
 *
 * Pro MZ-specificke bloky (0x40-0x4F) se vyuziva mztape knihovna
 * s parametry z bloku. Pro standardni TZX bloky (0x10) se vyuziva zxtape.
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
#include <limits.h>

#include "../cmt_stream/cmt_stream.h"
#include "../mztape/mztape.h"
#include "../zxtape/zxtape.h"
#include "../tzx/tzx.h"
#include "../endianity/endianity.h"
#include "../mzcmt_turbo/mzcmt_turbo.h"
#include "../mzcmt_fastipl/mzcmt_fastipl.h"
#include "../mzcmt_fsk/mzcmt_fsk.h"
#include "../mzcmt_slow/mzcmt_slow.h"
#include "../mzcmt_direct/mzcmt_direct.h"
#include "../mzcmt_bsd/mzcmt_bsd.h"
#include "../mzcmt_cpmtape/mzcmt_cpmtape.h"

#include "tmz.h"
#include "tmz_blocks.h"
#include "tmz_player.h"


/**
 * @brief Vychozi error callback pro tmz_player modul.
 * @param func Nazev volajici funkce.
 * @param line Cislo radku.
 * @param fmt Formatovaci retezec (printf styl).
 */
static void tmz_player_default_error_cb ( const char *func, int line, const char *fmt, ... ) {
    va_list args;
    fprintf ( stderr, "%s():%d - ", func, line );
    va_start ( args, fmt );
    vfprintf ( stderr, fmt, args );
    va_end ( args );
}

/** @brief Aktualne aktivni error callback. */
static void (*g_player_error_cb)( const char*, int, const char*, ... ) = tmz_player_default_error_cb;


/* ========================================================================= */
/*  Chybove retezce                                                          */
/* ========================================================================= */

/**
 * @brief Vrati textovy popis chyboveho kodu TMZ playeru.
 *
 * Vzdy vraci platny retezec (nikdy NULL).
 *
 * @param err Chybovy kod.
 * @return Textovy popis chyby.
 */
const char* tmz_player_error_string ( en_TMZ_PLAYER_ERROR err ) {
    switch ( err ) {
        case TMZ_PLAYER_OK:               return "OK";
        case TMZ_PLAYER_ERROR_NULL_INPUT:  return "NULL input parameter";
        case TMZ_PLAYER_ERROR_NO_BLOCKS:   return "No blocks in file";
        case TMZ_PLAYER_ERROR_UNSUPPORTED: return "Unsupported block type";
        case TMZ_PLAYER_ERROR_STREAM_CREATE: return "Failed to create CMT stream";
        case TMZ_PLAYER_ERROR_ALLOC:       return "Memory allocation failed";
        default:                           return "Unknown player error";
    }
}


/* ========================================================================= */
/*  Konfigurace                                                              */
/* ========================================================================= */

/**
 * @brief Inicializuje konfiguraci playeru na vychozi hodnoty.
 *
 * Nastavi: sample_rate = 44100, stream_type = vstream,
 * default_pulseset = MZ-800, default_speed = 1:1,
 * default_formatset = MZ800_SANE.
 *
 * @param config Ukazatel na konfiguraci k inicializaci.
 *
 * @pre config nesmi byt NULL.
 * @post Vsechna pole konfigurace jsou nastavena na rozumne vychozi hodnoty.
 */
void tmz_player_config_init ( st_TMZ_PLAYER_CONFIG *config ) {
    if ( !config ) return;
    config->sample_rate = CMTSTREAM_DEFAULT_RATE;
    config->stream_type = CMT_STREAM_TYPE_VSTREAM;
    config->default_pulseset = MZTAPE_PULSESET_800;
    config->default_speed = CMTSPEED_1_1;
    config->default_formatset = MZTAPE_FORMATSET_MZ800_SANE;
}


/* ========================================================================= */
/*  Pomocne funkce                                                           */
/* ========================================================================= */

/**
 * @brief Prevede en_TMZ_PULSESET na en_MZCMT_TURBO_PULSESET.
 *
 * @param pulseset TMZ pulsni sada.
 * @return Odpovidajici TURBO encoder pulsni sada.
 */
static en_MZCMT_TURBO_PULSESET tmz_to_turbo_enc_pulseset ( uint8_t pulseset ) {
    switch ( pulseset ) {
        case TMZ_PULSESET_700: return MZCMT_TURBO_PULSESET_700;
        case TMZ_PULSESET_800: return MZCMT_TURBO_PULSESET_800;
        case TMZ_PULSESET_80B: return MZCMT_TURBO_PULSESET_80B;
        default: return MZCMT_TURBO_PULSESET_800;
    }
}


/**
 * @brief Prevede en_TMZ_PULSESET na en_MZCMT_FASTIPL_PULSESET.
 *
 * @param pulseset TMZ pulsni sada.
 * @return Odpovidajici FASTIPL encoder pulsni sada.
 */
static en_MZCMT_FASTIPL_PULSESET tmz_to_fastipl_pulseset ( uint8_t pulseset ) {
    switch ( pulseset ) {
        case TMZ_PULSESET_700: return MZCMT_FASTIPL_PULSESET_700;
        case TMZ_PULSESET_800: return MZCMT_FASTIPL_PULSESET_800;
        case TMZ_PULSESET_80B: return MZCMT_FASTIPL_PULSESET_80B;
        default: return MZCMT_FASTIPL_PULSESET_800;
    }
}


/**
 * @brief Prevede en_TMZ_PULSESET na en_MZCMT_BSD_PULSESET.
 *
 * @param pulseset TMZ pulsni sada.
 * @return Odpovidajici BSD encoder pulsni sada.
 */
static en_MZCMT_BSD_PULSESET tmz_to_bsd_pulseset ( uint8_t pulseset ) {
    switch ( pulseset ) {
        case TMZ_PULSESET_700: return MZCMT_BSD_PULSESET_700;
        case TMZ_PULSESET_800: return MZCMT_BSD_PULSESET_800;
        case TMZ_PULSESET_80B: return MZCMT_BSD_PULSESET_80B;
        default: return MZCMT_BSD_PULSESET_800;
    }
}


/* tmz_to_cpm_pulseset: smazano spolu s mzcmt_cpm */


/**
 * @brief Prevede MZF hlavicku z host byte order zpet do LE pro raw kodery.
 *
 * Kodery mzcmt_turbo a mzcmt_bsd ocekavaji 128B hlavicku v originalni
 * endianite (LE). Parse funkce vsak konvertuji do host byte order.
 * Tato funkce vytvori LE kopii.
 *
 * @param header_host MZF hlavicka v host byte order.
 * @param header_le   Vystupni buffer pro 128B LE hlavicku (musi mit >= sizeof(st_MZF_HEADER)).
 */
static void tmz_player_header_to_le ( const st_MZF_HEADER *header_host, uint8_t *header_le ) {
    memcpy ( header_le, header_host, sizeof ( st_MZF_HEADER ) );
    mzf_header_items_correction ( (st_MZF_HEADER*) header_le );
}


/**
 * @brief Vytvori st_MZTAPE_MZF z MZF hlavicky a tela.
 *
 * Alokuje strukturu, zkopiruje hlavicku a telo, spocita checksums.
 *
 * @param mzf_header MZF hlavicka (v host byte order).
 * @param body Data tela.
 * @param body_size Velikost tela.
 * @return Nova struktura, nebo NULL pri chybe.
 *
 * @post Volajici musi uvolnit pres mztape_mztmzf_destroy().
 */
static st_MZTAPE_MZF* tmz_player_create_mztmzf ( const st_MZF_HEADER *mzf_header,
                                                   const uint8_t *body, uint16_t body_size ) {

    st_MZTAPE_MZF *mztmzf = calloc ( 1, sizeof ( st_MZTAPE_MZF ) );
    if ( !mztmzf ) return NULL;

    /* hlavicka se uklada v raw (LE) formatu pro mztape */
    memcpy ( mztmzf->header, mzf_header, sizeof ( st_MZF_HEADER ) );
    /* konverze zpet do LE pro mztape, ktera ocekava raw hlavicku */
    mzf_header_items_correction ( (st_MZF_HEADER*) mztmzf->header );

    mztmzf->size = body_size;

    if ( body_size > 0 && body ) {
        mztmzf->body = malloc ( body_size );
        if ( !mztmzf->body ) {
            free ( mztmzf );
            return NULL;
        }
        memcpy ( mztmzf->body, body, body_size );
    }

    /* vypocet checksumu (z raw dat) */
    uint32_t chkh = 0;
    for ( uint32_t i = 0; i < sizeof ( st_MZF_HEADER ); i++ ) {
        uint8_t byte = mztmzf->header[i];
        for ( int bit = 0; bit < 8; bit++ ) {
            if ( byte & 1 ) chkh++;
            byte >>= 1;
        }
    }
    mztmzf->chkh = chkh;

    uint32_t chkb = 0;
    if ( mztmzf->body ) {
        for ( uint32_t i = 0; i < body_size; i++ ) {
            uint8_t byte = mztmzf->body[i];
            for ( int bit = 0; bit < 8; bit++ ) {
                if ( byte & 1 ) chkb++;
                byte >>= 1;
            }
        }
    }
    mztmzf->chkb = chkb;

    return mztmzf;
}


/* ========================================================================= */
/*  Prehravani jednotlivych bloku                                            */
/* ========================================================================= */

/**
 * @brief Prehaje blok 0x40 (MZ Standard Data) na CMT stream.
 *
 * Naparsuje blok, vytvori st_MZTAPE_MZF a pouzije mztape knihovnu
 * pro generovani CMT streamu ve standardni rychlosti 1:1.
 *
 * @param block TMZ blok s ID 0x40.
 * @param config Konfigurace playeru.
 * @param err Vystupni chybovy kod (muze byt NULL).
 * @return Novy CMT stream, nebo NULL pri chybe.
 *
 * @pre block nesmi byt NULL a musi mit id == TMZ_BLOCK_ID_MZ_STANDARD_DATA.
 * @post Volajici je vlastnikem vraceneho streamu a musi ho uvolnit pres cmt_stream_destroy().
 */
st_CMT_STREAM* tmz_player_play_mz_standard ( const st_TZX_BLOCK *block,
                                              const st_TMZ_PLAYER_CONFIG *config,
                                              en_TMZ_PLAYER_ERROR *err ) {

    en_TMZ_PLAYER_ERROR local_err = TMZ_PLAYER_OK;
    if ( !err ) err = &local_err;

    if ( !block || !config ) {
        *err = TMZ_PLAYER_ERROR_NULL_INPUT;
        return NULL;
    }

    /* parsovani bloku */
    en_TZX_ERROR parse_err;
    uint8_t *body_data = NULL;

    /* musime pracovat s kopii bloku, protoze parse funkce modifikuji data (endianita) */
    st_TZX_BLOCK block_copy = *block;
    uint8_t *data_copy = malloc ( block->length );
    if ( !data_copy ) {
        *err = TMZ_PLAYER_ERROR_ALLOC;
        return NULL;
    }
    memcpy ( data_copy, block->data, block->length );
    block_copy.data = data_copy;

    st_TMZ_MZ_STANDARD_DATA *std_data = tmz_block_parse_mz_standard ( &block_copy, &body_data, &parse_err );
    if ( !std_data ) {
        g_player_error_cb ( __func__, __LINE__, "Failed to parse MZ Standard Data block\n" );
        free ( data_copy );
        *err = TMZ_PLAYER_ERROR_STREAM_CREATE;
        return NULL;
    }

    /* vytvoreni mztape MZF */
    st_MZTAPE_MZF *mztmzf = tmz_player_create_mztmzf ( &std_data->mzf_header, body_data, std_data->body_size );
    free ( data_copy );

    if ( !mztmzf ) {
        *err = TMZ_PLAYER_ERROR_ALLOC;
        return NULL;
    }

    /* generovani CMT streamu */
    st_CMT_STREAM *stream = mztape_create_stream_from_mztapemzf (
        mztmzf, CMTSPEED_1_1, config->stream_type,
        config->default_formatset, config->sample_rate );

    mztape_mztmzf_destroy ( mztmzf );

    if ( !stream ) {
        g_player_error_cb ( __func__, __LINE__, "Failed to create CMT stream from MZ Standard Data\n" );
        *err = TMZ_PLAYER_ERROR_STREAM_CREATE;
        return NULL;
    }

    *err = TMZ_PLAYER_OK;
    return stream;
}


/* ========================================================================= */
/*  Format-specificke helpery pro blok 0x41                                  */
/* ========================================================================= */

/**
 * @brief Prehaje blok 0x41 s formatem NORMAL (standardni FM).
 *
 * Pouzije mztape knihovnu se zadanou rychlosti. Odpovida puvodnimu
 * chovani playeru, kdy format pole bylo ignorovano.
 *
 * @param td         Rozparsovana turbo data (host byte order).
 * @param body_data  Datove telo.
 * @param config     Konfigurace playeru.
 * @return Novy CMT stream, nebo NULL pri chybe.
 */
static st_CMT_STREAM* play_turbo_fmt_normal ( const st_TMZ_MZ_TURBO_DATA *td,
                                               const uint8_t *body_data,
                                               const st_TMZ_PLAYER_CONFIG *config ) {

    en_CMTSPEED speed = (en_CMTSPEED) td->speed;
    if ( !cmtspeed_is_valid ( speed ) ) speed = config->default_speed;

    st_MZTAPE_MZF *mztmzf = tmz_player_create_mztmzf ( &td->mzf_header, body_data, td->body_size );
    if ( !mztmzf ) return NULL;

    st_CMT_STREAM *stream = mztape_create_stream_from_mztapemzf_ex (
        mztmzf, speed, config->stream_type,
        config->default_formatset, config->sample_rate,
        td->long_high, td->long_low,
        td->short_high, td->short_low );

    mztape_mztmzf_destroy ( mztmzf );
    return stream;
}


/**
 * @brief Prehaje blok 0x41 s formatem TURBO (FM s konfigurovatelnym casovanim).
 *
 * Pouzije mzcmt_turbo koder s kompletnim paskovym signalem vcetne
 * TURBO loaderu v NORMAL FM 1:1 (loader modifikuje readpoint a cte
 * uzivatelska data pri TURBO rychlosti).
 *
 * @param td         Rozparsovana turbo data (host byte order).
 * @param body_data  Datove telo.
 * @param config     Konfigurace playeru.
 * @return Novy CMT stream, nebo NULL pri chybe.
 */
static st_CMT_STREAM* play_turbo_fmt_turbo ( const st_TMZ_MZ_TURBO_DATA *td,
                                              const uint8_t *body_data,
                                              const st_TMZ_PLAYER_CONFIG *config ) {

    /* konfigurace TURBO koderu z parametru bloku */
    st_MZCMT_TURBO_CONFIG tcfg;
    memset ( &tcfg, 0, sizeof ( tcfg ) );
    tcfg.pulseset = tmz_to_turbo_enc_pulseset ( td->pulseset );
    tcfg.speed = (en_CMTSPEED) td->speed;
    if ( !cmtspeed_is_valid ( tcfg.speed ) ) tcfg.speed = config->default_speed;
    tcfg.lgap_length = td->lgap_length;
    tcfg.sgap_length = td->sgap_length;
    tcfg.long_high_us100 = td->long_high;
    tcfg.long_low_us100 = td->long_low;
    tcfg.short_high_us100 = td->short_high;
    tcfg.short_low_us100 = td->short_low;
    tcfg.flags = td->flags;

    return mzcmt_turbo_create_tape_stream (
        &td->mzf_header, body_data, td->body_size,
        &tcfg, config->stream_type, config->sample_rate );
}


/**
 * @brief Prehaje blok 0x41 s formatem FASTIPL ($BB prefix, Intercopy).
 *
 * Pouzije mzcmt_fastipl koder. Generuje dvoudilny signal:
 * $BB hlavicka pri 1:1 rychlosti, pauza, datove telo pri turbo rychlosti.
 * Verze loaderu je V07 (InterCopy v7+), pokud neni v bloku specifikovano jinak.
 *
 * @param td         Rozparsovana turbo data (host byte order).
 * @param body_data  Datove telo.
 * @param config     Konfigurace playeru.
 * @return Novy CMT stream, nebo NULL pri chybe.
 */
static st_CMT_STREAM* play_turbo_fmt_fastipl ( const st_TMZ_MZ_TURBO_DATA *td,
                                                const uint8_t *body_data,
                                                const st_TMZ_PLAYER_CONFIG *config ) {

    /* konfigurace FASTIPL koderu */
    st_MZCMT_FASTIPL_CONFIG fcfg;
    memset ( &fcfg, 0, sizeof ( fcfg ) );
    fcfg.version = MZCMT_FASTIPL_VERSION_V07;
    fcfg.pulseset = tmz_to_fastipl_pulseset ( td->pulseset );
    fcfg.speed = (en_CMTSPEED) td->speed;
    if ( !cmtspeed_is_valid ( fcfg.speed ) ) fcfg.speed = config->default_speed;
    fcfg.lgap_length = td->lgap_length;
    fcfg.long_high_us100 = td->long_high;
    fcfg.long_low_us100 = td->long_low;
    fcfg.short_high_us100 = td->short_high;
    fcfg.short_low_us100 = td->short_low;

    /* FASTIPL ocekava MZF hlavicku v host byte order */
    return mzcmt_fastipl_create_stream (
        &td->mzf_header, body_data, td->body_size,
        &fcfg, config->stream_type, config->sample_rate );
}


/**
 * @brief Prehaje blok 0x41 s formatem FSK (Frequency Shift Keying).
 *
 * Pouzije mzcmt_fsk koder s kompletnim paskovym signalem: preloader
 * v NORMAL FM 1:1 + FSK loader body v NORMAL FM + uzivatelska data
 * v FSK formatu.
 *
 * @param td         Rozparsovana turbo data (host byte order).
 * @param body_data  Datove telo.
 * @param config     Konfigurace playeru.
 * @return Novy CMT stream, nebo NULL pri chybe.
 */
static st_CMT_STREAM* play_turbo_fmt_fsk ( const st_TMZ_MZ_TURBO_DATA *td,
                                            const uint8_t *body_data,
                                            const st_TMZ_PLAYER_CONFIG *config ) {

    /* konfigurace FSK tape koderu */
    st_MZCMT_FSK_TAPE_CONFIG fcfg;
    memset ( &fcfg, 0, sizeof ( fcfg ) );
    fcfg.pulseset = (en_MZCMT_FSK_PULSESET) tmz_to_turbo_enc_pulseset ( td->pulseset );
    fcfg.speed = (en_MZCMT_FSK_SPEED) td->speed;
    if ( fcfg.speed >= MZCMT_FSK_SPEED_COUNT ) fcfg.speed = MZCMT_FSK_SPEED_0;
    fcfg.loader_speed = 0; /* standardni ROM rychlost pro nacteni FSK loaderu */

    return mzcmt_fsk_create_tape_stream (
        &td->mzf_header, body_data, td->body_size,
        &fcfg, config->stream_type, config->sample_rate );
}


/**
 * @brief Prehaje blok 0x41 s formatem SLOW (2 bity na pulz).
 *
 * Pouzije mzcmt_slow koder s kompletnim paskovym signalem: preloader
 * v NORMAL FM 1:1 + SLOW loader body v NORMAL FM + uzivatelska data
 * v SLOW formatu.
 *
 * @param td         Rozparsovana turbo data (host byte order).
 * @param body_data  Datove telo.
 * @param config     Konfigurace playeru.
 * @return Novy CMT stream, nebo NULL pri chybe.
 */
static st_CMT_STREAM* play_turbo_fmt_slow ( const st_TMZ_MZ_TURBO_DATA *td,
                                             const uint8_t *body_data,
                                             const st_TMZ_PLAYER_CONFIG *config ) {

    /* konfigurace SLOW tape koderu */
    st_MZCMT_SLOW_TAPE_CONFIG scfg;
    memset ( &scfg, 0, sizeof ( scfg ) );
    scfg.pulseset = (en_MZCMT_SLOW_PULSESET) tmz_to_turbo_enc_pulseset ( td->pulseset );
    scfg.speed = (en_MZCMT_SLOW_SPEED) td->speed;
    if ( scfg.speed >= MZCMT_SLOW_SPEED_COUNT ) scfg.speed = MZCMT_SLOW_SPEED_0;
    scfg.loader_speed = 0;

    return mzcmt_slow_create_tape_stream (
        &td->mzf_header, body_data, td->body_size,
        &scfg, config->stream_type, config->sample_rate );
}


/**
 * @brief Prehaje blok 0x41 s formatem DIRECT (primy bitovy zapis).
 *
 * Pouzije mzcmt_direct koder s kompletnim paskovym signalem: preloader
 * v NORMAL FM 1:1 + DIRECT loader body v NORMAL FM + uzivatelska data
 * v DIRECT formatu.
 *
 * @param td         Rozparsovana turbo data (host byte order).
 * @param body_data  Datove telo.
 * @param config     Konfigurace playeru.
 * @return Novy CMT stream, nebo NULL pri chybe.
 */
static st_CMT_STREAM* play_turbo_fmt_direct ( const st_TMZ_MZ_TURBO_DATA *td,
                                               const uint8_t *body_data,
                                               const st_TMZ_PLAYER_CONFIG *config ) {

    /* konfigurace DIRECT tape koderu */
    st_MZCMT_DIRECT_TAPE_CONFIG dcfg;
    memset ( &dcfg, 0, sizeof ( dcfg ) );
    dcfg.pulseset = (en_MZCMT_DIRECT_PULSESET) tmz_to_turbo_enc_pulseset ( td->pulseset );
    dcfg.loader_speed = 0;

    return mzcmt_direct_create_tape_stream (
        &td->mzf_header, body_data, td->body_size,
        &dcfg, config->stream_type, config->sample_rate );
}


/* ========================================================================= */
/*  Prehravani jednotlivych bloku                                            */
/* ========================================================================= */

/**
 * @brief Prehaje blok 0x41 s formatem CPM-TAPE (Pezik/MarVan).
 *
 * Pouzije mzcmt_cpmtape koder - manchesterske kodovani, LSB first,
 * bez stop bitu. Kompletni signal: header blok + gap + body blok.
 * Na pasce neni loader (TAPE.COM bezi pod CP/M).
 *
 * @param td         Rozparsovana turbo data (host byte order).
 * @param body_data  Datove telo.
 * @param config     Konfigurace playeru.
 * @return Novy CMT stream, nebo NULL pri chybe.
 */
static st_CMT_STREAM* play_turbo_fmt_cpmtape ( const st_TMZ_MZ_TURBO_DATA *td,
                                                 const uint8_t *body_data,
                                                 const st_TMZ_PLAYER_CONFIG *config ) {

    /* konfigurace CPM-TAPE koderu */
    st_MZCMT_CPMTAPE_CONFIG ccfg;
    memset ( &ccfg, 0, sizeof ( ccfg ) );
    ccfg.speed = (en_MZCMT_CPMTAPE_SPEED) td->speed;
    if ( ccfg.speed >= MZCMT_CPMTAPE_SPEED_COUNT ) ccfg.speed = MZCMT_CPMTAPE_SPEED_2400;

    return mzcmt_cpmtape_create_stream (
        &td->mzf_header, body_data, td->body_size,
        &ccfg, config->stream_type, config->sample_rate );
}


/**
 * @brief Prehaje blok 0x41 (MZ Turbo Data) na CMT stream.
 *
 * Dispatcher - podle pole format v bloku zvoli prislusny koder:
 * - NORMAL: standardni FM pres mztape (puvodni chovani)
 * - TURBO: NORMAL FM se zmenenym casovanim (mzcmt_turbo)
 * - FASTIPL: $BB prefix kodovani s dvojdilnym signalem (mzcmt_fastipl)
 * - FSK: Frequency Shift Keying (mzcmt_fsk)
 * - SLOW: 2 bity na pulz (mzcmt_slow)
 * - DIRECT: primy bitovy zapis (mzcmt_direct)
 * - CPM-TAPE: Manchester kodovani, LSB first (mzcmt_cpmtape)
 * - SINCLAIR: nepodporovano (vyzaduje TZX blokovou generaci)
 *
 * @param block TMZ blok s ID 0x41.
 * @param config Konfigurace playeru.
 * @param err Vystupni chybovy kod (muze byt NULL).
 * @return Novy CMT stream, nebo NULL pri chybe.
 *
 * @pre block nesmi byt NULL a musi mit id == TMZ_BLOCK_ID_MZ_TURBO_DATA.
 * @post Volajici je vlastnikem vraceneho streamu a musi ho uvolnit pres cmt_stream_destroy().
 */
st_CMT_STREAM* tmz_player_play_mz_turbo ( const st_TZX_BLOCK *block,
                                           const st_TMZ_PLAYER_CONFIG *config,
                                           en_TMZ_PLAYER_ERROR *err ) {

    en_TMZ_PLAYER_ERROR local_err = TMZ_PLAYER_OK;
    if ( !err ) err = &local_err;

    if ( !block || !config ) {
        *err = TMZ_PLAYER_ERROR_NULL_INPUT;
        return NULL;
    }

    /* parsovani bloku (kopie kvuli endianite) */
    en_TZX_ERROR parse_err;
    uint8_t *body_data = NULL;

    st_TZX_BLOCK block_copy = *block;
    uint8_t *data_copy = malloc ( block->length );
    if ( !data_copy ) {
        *err = TMZ_PLAYER_ERROR_ALLOC;
        return NULL;
    }
    memcpy ( data_copy, block->data, block->length );
    block_copy.data = data_copy;

    st_TMZ_MZ_TURBO_DATA *turbo_data = tmz_block_parse_mz_turbo ( &block_copy, &body_data, &parse_err );
    if ( !turbo_data ) {
        g_player_error_cb ( __func__, __LINE__, "Failed to parse MZ Turbo Data block\n" );
        free ( data_copy );
        *err = TMZ_PLAYER_ERROR_STREAM_CREATE;
        return NULL;
    }

    /* dispatching podle formatu zaznamu */
    st_CMT_STREAM *stream = NULL;

    switch ( turbo_data->format ) {

        case TMZ_FORMAT_NORMAL:
            stream = play_turbo_fmt_normal ( turbo_data, body_data, config );
            break;

        case TMZ_FORMAT_TURBO:
            stream = play_turbo_fmt_turbo ( turbo_data, body_data, config );
            break;

        case TMZ_FORMAT_FASTIPL:
            stream = play_turbo_fmt_fastipl ( turbo_data, body_data, config );
            break;

        case TMZ_FORMAT_FSK:
            stream = play_turbo_fmt_fsk ( turbo_data, body_data, config );
            break;

        case TMZ_FORMAT_SLOW:
            stream = play_turbo_fmt_slow ( turbo_data, body_data, config );
            break;

        case TMZ_FORMAT_DIRECT:
            stream = play_turbo_fmt_direct ( turbo_data, body_data, config );
            break;

        case TMZ_FORMAT_CPM_TAPE:
            stream = play_turbo_fmt_cpmtape ( turbo_data, body_data, config );
            break;

        case TMZ_FORMAT_SINCLAIR:
            g_player_error_cb ( __func__, __LINE__,
                "SINCLAIR format in MZ Turbo Data block not yet supported\n" );
            break;

        default:
            g_player_error_cb ( __func__, __LINE__,
                "Unknown format %d in MZ Turbo Data block\n", turbo_data->format );
            break;
    }

    free ( data_copy );

    if ( !stream ) {
        *err = TMZ_PLAYER_ERROR_UNSUPPORTED;
        return NULL;
    }

    *err = TMZ_PLAYER_OK;
    return stream;
}


/**
 * @brief Prehaje blok 0x45 (MZ BASIC Data) na CMT stream.
 *
 * BSD/BRD format - chunkovany datovy zaznam. Pouzije mzcmt_bsd koder,
 * ktery generuje kompletni signal vcetne hlavicky a chunku.
 * Kazdy chunk (258 B = 2B ID + 256B data) se na pasce odesila jako
 * samostatny body blok s kratkym tapemarkem.
 *
 * @param block TMZ blok s ID 0x45.
 * @param config Konfigurace playeru.
 * @param err Vystupni chybovy kod (muze byt NULL).
 * @return Novy CMT stream, nebo NULL pri chybe.
 *
 * @pre block nesmi byt NULL a musi mit id == TMZ_BLOCK_ID_MZ_BASIC_DATA.
 * @post Volajici je vlastnikem vraceneho streamu a musi ho uvolnit pres cmt_stream_destroy().
 */
st_CMT_STREAM* tmz_player_play_mz_basic ( const st_TZX_BLOCK *block,
                                            const st_TMZ_PLAYER_CONFIG *config,
                                            en_TMZ_PLAYER_ERROR *err ) {

    en_TMZ_PLAYER_ERROR local_err = TMZ_PLAYER_OK;
    if ( !err ) err = &local_err;

    if ( !block || !config ) {
        *err = TMZ_PLAYER_ERROR_NULL_INPUT;
        return NULL;
    }

    /* parsovani bloku (kopie kvuli endianite) */
    en_TZX_ERROR parse_err;
    uint8_t *chunks_data = NULL;

    st_TZX_BLOCK block_copy = *block;
    uint8_t *data_copy = malloc ( block->length );
    if ( !data_copy ) {
        *err = TMZ_PLAYER_ERROR_ALLOC;
        return NULL;
    }
    memcpy ( data_copy, block->data, block->length );
    block_copy.data = data_copy;

    st_TMZ_MZ_BASIC_DATA *basic_data = tmz_block_parse_mz_basic_data ( &block_copy, &chunks_data, &parse_err );
    if ( !basic_data ) {
        g_player_error_cb ( __func__, __LINE__, "Failed to parse MZ BASIC Data block\n" );
        free ( data_copy );
        *err = TMZ_PLAYER_ERROR_STREAM_CREATE;
        return NULL;
    }

    /* hlavicka v LE pro raw kodovani */
    uint8_t raw_header[sizeof ( st_MZF_HEADER )];
    tmz_player_header_to_le ( &basic_data->mzf_header, raw_header );

    /* konfigurace BSD koderu */
    st_MZCMT_BSD_CONFIG bsd_cfg;
    memset ( &bsd_cfg, 0, sizeof ( bsd_cfg ) );
    bsd_cfg.pulseset = tmz_to_bsd_pulseset ( basic_data->pulseset );
    bsd_cfg.speed = CMTSPEED_1_1;

    /* generovani CMT streamu */
    st_CMT_STREAM *stream = mzcmt_bsd_create_stream (
        raw_header, chunks_data, basic_data->chunk_count,
        &bsd_cfg, config->stream_type, config->sample_rate );

    free ( data_copy );

    if ( !stream ) {
        g_player_error_cb ( __func__, __LINE__, "Failed to create CMT stream from MZ BASIC Data\n" );
        *err = TMZ_PLAYER_ERROR_STREAM_CREATE;
        return NULL;
    }

    *err = TMZ_PLAYER_OK;
    return stream;
}


/**
 * @brief Prevede konfiguraci TMZ playeru na konfiguraci TZX generatoru.
 *
 * @param config TMZ player konfigurace.
 * @return TZX konfigurace.
 */
static st_TZX_CONFIG tmz_player_to_tzx_config ( const st_TMZ_PLAYER_CONFIG *config ) {
    st_TZX_CONFIG tzx_cfg;
    tzx_config_init ( &tzx_cfg );
    tzx_cfg.sample_rate = config->sample_rate;
    tzx_cfg.stream_type = config->stream_type;
    /* cpu_clock zustava na vychozich 3.5 MHz pro TZX bloky */
    return tzx_cfg;
}


/**
 * @brief Zabali vstream do CMT stream wrapperu.
 *
 * @param vs Vstream k zabaleni (vlastnictvi se prenasi).
 * @return Novy CMT stream, nebo NULL pri chybe.
 */
static st_CMT_STREAM* tmz_player_wrap_vstream ( st_CMT_VSTREAM *vs ) {
    st_CMT_STREAM *stream = cmt_stream_new ( CMT_STREAM_TYPE_VSTREAM );
    if ( !stream ) {
        cmt_vstream_destroy ( vs );
        return NULL;
    }
    stream->str.vstream = vs;
    return stream;
}


/**
 * @brief Prehaje libovolny TZX audio blok (0x10-0x15, 0x20) na CMT stream.
 *
 * Pouzije TZX knihovnu pro parsovani bloku a generovani vstreamu.
 * Podporuje bloky: Standard Speed (0x10), Turbo Speed (0x11),
 * Pure Tone (0x12), Pulse Sequence (0x13), Pure Data (0x14),
 * Direct Recording (0x15) a Pause (0x20).
 *
 * @param block TMZ blok s TZX audio ID.
 * @param config Konfigurace playeru.
 * @param err Vystupni chybovy kod (muze byt NULL).
 * @return Novy CMT stream, nebo NULL pri chybe / ridici blok bez audia.
 *
 * @pre block nesmi byt NULL.
 * @post Volajici je vlastnikem vraceneho streamu a musi ho uvolnit pres cmt_stream_destroy().
 */
st_CMT_STREAM* tmz_player_play_tzx_block ( const st_TZX_BLOCK *block,
                                            const st_TMZ_PLAYER_CONFIG *config,
                                            en_TMZ_PLAYER_ERROR *err ) {

    en_TMZ_PLAYER_ERROR local_err = TMZ_PLAYER_OK;
    if ( !err ) err = &local_err;

    if ( !block || !config || !block->data ) {
        *err = TMZ_PLAYER_ERROR_NULL_INPUT;
        return NULL;
    }

    st_TZX_CONFIG tzx_cfg = tmz_player_to_tzx_config ( config );
    en_TZX_ERROR tzx_err;
    st_CMT_VSTREAM *vs = NULL;

    switch ( block->id ) {

        case TZX_BLOCK_ID_STANDARD_SPEED:
        {
            st_TZX_STANDARD_SPEED parsed;
            tzx_err = tzx_parse_standard_speed ( block->data, block->length, &parsed );
            if ( tzx_err != TZX_OK ) { *err = TMZ_PLAYER_ERROR_STREAM_CREATE; return NULL; }
            vs = tzx_generate_standard_speed ( &parsed, &tzx_cfg, &tzx_err );
            break;
        }

        case TZX_BLOCK_ID_TURBO_SPEED:
        {
            st_TZX_TURBO_SPEED parsed;
            tzx_err = tzx_parse_turbo_speed ( block->data, block->length, &parsed );
            if ( tzx_err != TZX_OK ) { *err = TMZ_PLAYER_ERROR_STREAM_CREATE; return NULL; }
            vs = tzx_generate_turbo_speed ( &parsed, &tzx_cfg, &tzx_err );
            break;
        }

        case TZX_BLOCK_ID_PURE_TONE:
        {
            st_TZX_PURE_TONE parsed;
            tzx_err = tzx_parse_pure_tone ( block->data, block->length, &parsed );
            if ( tzx_err != TZX_OK ) { *err = TMZ_PLAYER_ERROR_STREAM_CREATE; return NULL; }
            vs = tzx_generate_pure_tone ( &parsed, &tzx_cfg, &tzx_err );
            break;
        }

        case TZX_BLOCK_ID_PULSE_SEQUENCE:
        {
            st_TZX_PULSE_SEQUENCE parsed;
            tzx_err = tzx_parse_pulse_sequence ( block->data, block->length, &parsed );
            if ( tzx_err != TZX_OK ) { *err = TMZ_PLAYER_ERROR_STREAM_CREATE; return NULL; }
            vs = tzx_generate_pulse_sequence ( &parsed, &tzx_cfg, &tzx_err );
            break;
        }

        case TZX_BLOCK_ID_PURE_DATA:
        {
            st_TZX_PURE_DATA parsed;
            tzx_err = tzx_parse_pure_data ( block->data, block->length, &parsed );
            if ( tzx_err != TZX_OK ) { *err = TMZ_PLAYER_ERROR_STREAM_CREATE; return NULL; }
            vs = tzx_generate_pure_data ( &parsed, &tzx_cfg, &tzx_err );
            break;
        }

        case TZX_BLOCK_ID_DIRECT_RECORDING:
        {
            st_TZX_DIRECT_RECORDING parsed;
            tzx_err = tzx_parse_direct_recording ( block->data, block->length, &parsed );
            if ( tzx_err != TZX_OK ) { *err = TMZ_PLAYER_ERROR_STREAM_CREATE; return NULL; }
            vs = tzx_generate_direct_recording ( &parsed, &tzx_cfg, &tzx_err );
            break;
        }

        case TZX_BLOCK_ID_PAUSE:
        {
            st_TZX_PAUSE parsed;
            tzx_err = tzx_parse_pause ( block->data, block->length, &parsed );
            if ( tzx_err != TZX_OK ) { *err = TMZ_PLAYER_ERROR_STREAM_CREATE; return NULL; }
            if ( parsed.pause_ms == 0 ) { *err = TMZ_PLAYER_OK; return NULL; } /* stop tape */
            vs = tzx_generate_pause ( parsed.pause_ms, &tzx_cfg, &tzx_err );
            break;
        }

        default:
            *err = TMZ_PLAYER_ERROR_UNSUPPORTED;
            return NULL;
    }

    if ( !vs ) {
        g_player_error_cb ( __func__, __LINE__, "TZX block 0x%02X: failed to generate audio\n", block->id );
        *err = TMZ_PLAYER_ERROR_STREAM_CREATE;
        return NULL;
    }

    st_CMT_STREAM *stream = tmz_player_wrap_vstream ( vs );
    if ( !stream ) {
        *err = TMZ_PLAYER_ERROR_ALLOC;
        return NULL;
    }

    *err = TMZ_PLAYER_OK;
    return stream;
}


/**
 * @brief Prehaje jeden libovolny blok na CMT stream.
 *
 * Dispatcher - podle ID bloku zavola prislusnou specializovanou funkci.
 * Ridici bloky (pause, group, loop) jsou zpracovany, ale nevytvareji stream
 * (vraci NULL s TMZ_PLAYER_OK).
 *
 * @param block TMZ blok k prehrani.
 * @param config Konfigurace playeru.
 * @param err Vystupni chybovy kod (muze byt NULL).
 * @return Novy CMT stream, nebo NULL pokud blok negeneruje audio (ridici bloky)
 *         nebo pri chybe.
 *
 * @pre block nesmi byt NULL.
 * @post Volajici je vlastnikem vraceneho streamu a musi ho uvolnit pres cmt_stream_destroy().
 */
st_CMT_STREAM* tmz_player_play_block ( const st_TZX_BLOCK *block,
                                        const st_TMZ_PLAYER_CONFIG *config,
                                        en_TMZ_PLAYER_ERROR *err ) {

    en_TMZ_PLAYER_ERROR local_err = TMZ_PLAYER_OK;
    if ( !err ) err = &local_err;

    if ( !block || !config ) {
        *err = TMZ_PLAYER_ERROR_NULL_INPUT;
        return NULL;
    }

    switch ( block->id ) {

        /* MZ-specificke bloky */
        case TMZ_BLOCK_ID_MZ_STANDARD_DATA:
            return tmz_player_play_mz_standard ( block, config, err );

        case TMZ_BLOCK_ID_MZ_TURBO_DATA:
            return tmz_player_play_mz_turbo ( block, config, err );

        /* BSD/BRD - chunkovany datovy zaznam pres mzcmt_bsd */
        case TMZ_BLOCK_ID_MZ_BASIC_DATA:
            return tmz_player_play_mz_basic ( block, config, err );

        /* TZX audio bloky - delegovano na TZX knihovnu */
        case TZX_BLOCK_ID_STANDARD_SPEED:
        case TZX_BLOCK_ID_TURBO_SPEED:
        case TZX_BLOCK_ID_PURE_TONE:
        case TZX_BLOCK_ID_PULSE_SEQUENCE:
        case TZX_BLOCK_ID_PURE_DATA:
        case TZX_BLOCK_ID_DIRECT_RECORDING:
        case TZX_BLOCK_ID_PAUSE:
            return tmz_player_play_tzx_block ( block, config, err );

        /* ridici bloky - negeneruji audio */
        case TZX_BLOCK_ID_GROUP_START:
        case TZX_BLOCK_ID_GROUP_END:
        case TZX_BLOCK_ID_LOOP_START:
        case TZX_BLOCK_ID_LOOP_END:
        case TZX_BLOCK_ID_JUMP:
        case TZX_BLOCK_ID_SELECT_BLOCK:
        case TZX_BLOCK_ID_STOP_48K:
        case TZX_BLOCK_ID_SET_SIGNAL_LEVEL:
        case TZX_BLOCK_ID_TEXT_DESCRIPTION:
        case TZX_BLOCK_ID_MESSAGE:
        case TZX_BLOCK_ID_ARCHIVE_INFO:
        case TZX_BLOCK_ID_HARDWARE_TYPE:
        case TZX_BLOCK_ID_CUSTOM_INFO:
        case TZX_BLOCK_ID_GLUE:
        case TMZ_BLOCK_ID_MZ_MACHINE_INFO:
            *err = TMZ_PLAYER_OK;
            return NULL;

        /* nepodporovane bloky (CSW, Generalized Data, ...) */
        default:
            g_player_error_cb ( __func__, __LINE__, "Unsupported block ID 0x%02X for playback\n", block->id );
            *err = TMZ_PLAYER_ERROR_UNSUPPORTED;
            return NULL;
    }
}


/* ========================================================================= */
/*  Stavovy automat pro sekvencni prehravani                                 */
/* ========================================================================= */


/**
 * @brief Inicializuje stav playeru pro prehrani daneho souboru.
 *
 * Nastavi pocatecni pozici na blok 0, prazdne zasobniky,
 * signal_level = 0 (LOW), finished = false.
 *
 * @param state   Stav k inicializaci.
 * @param file    TMZ/TZX soubor (musi zustat platny po celou dobu prehravani).
 * @param config  Konfigurace playeru (zkopiruje se do stavu).
 *
 * @pre state, file, config nesmi byt NULL.
 * @post state je pripraven pro prvni volani tmz_player_play_next().
 */
void tmz_player_state_init ( st_TMZ_PLAYER_STATE *state,
                             const st_TZX_FILE *file,
                             const st_TMZ_PLAYER_CONFIG *config ) {
    if ( !state || !file || !config ) return;
    memset ( state, 0, sizeof ( *state ) );
    state->file = file;
    state->config = *config;
    state->signal_level = 0; /* LOW */
    state->finished = ( file->block_count == 0 );
}


/**
 * @brief Zjisti, zda je prehravani dokonceno.
 *
 * @param state Stav playeru.
 * @return true pokud vsechny bloky byly zpracovany, false jinak.
 */
bool tmz_player_state_finished ( const st_TMZ_PLAYER_STATE *state ) {
    if ( !state ) return true;
    return state->finished;
}


/**
 * @brief Zpracuje blok Loop Start (0x24).
 *
 * Nacte pocet opakovani z dat bloku (2B LE), ulozi zaznam
 * na zasobnik smycek a posune current_block za Loop Start.
 * Pokud je zasobnik plny nebo repeat_count == 0, blok se preskoci.
 *
 * @param state Stav playeru.
 * @param block Blok 0x24.
 */
static void handle_loop_start ( st_TMZ_PLAYER_STATE *state, const st_TZX_BLOCK *block ) {
    state->current_block++;

    if ( state->loop_depth >= TMZ_PLAYER_MAX_LOOP_DEPTH ) {
        g_player_error_cb ( __func__, __LINE__,
            "Loop nesting depth exceeded (max %d)\n", TMZ_PLAYER_MAX_LOOP_DEPTH );
        return;
    }

    uint16_t repeat_count = 0;
    if ( block->data && block->length >= 2 ) {
        repeat_count = (uint16_t) ( block->data[0] | ( block->data[1] << 8 ) );
    }

    if ( repeat_count == 0 ) return; /* nulovy repeat = preskocit */

    st_TMZ_PLAYER_LOOP_ENTRY *entry = &state->loop_stack[state->loop_depth];
    entry->start_block = state->current_block; /* blok hned za Loop Start */
    entry->remaining = repeat_count;
    state->loop_depth++;
}


/**
 * @brief Zpracuje blok Loop End (0x25).
 *
 * Dekrementuje pocitadlo na vrcholu zasobniku smycek.
 * Pokud je remaining > 1, skoci zpet na start_block.
 * Pokud je remaining == 1, smycka je hotova - pop ze zasobniku a pokracuje dal.
 * Pokud je zasobnik prazdny, blok se preskoci.
 *
 * @param state Stav playeru.
 */
static void handle_loop_end ( st_TMZ_PLAYER_STATE *state ) {
    if ( state->loop_depth <= 0 ) {
        /* Loop End bez Loop Start - preskocit */
        state->current_block++;
        return;
    }

    st_TMZ_PLAYER_LOOP_ENTRY *entry = &state->loop_stack[state->loop_depth - 1];
    entry->remaining--;

    if ( entry->remaining > 0 ) {
        /* dalsi iterace - skok zpet na zacatek smycky */
        state->current_block = entry->start_block;
    } else {
        /* smycka hotova - pop a pokracovat za Loop End */
        state->loop_depth--;
        state->current_block++;
    }
}


/**
 * @brief Zpracuje blok Jump (0x23).
 *
 * Nacte relativni offset z dat bloku (2B LE signed)
 * a upravi current_block. Offset je relativni vuci aktualnimu bloku.
 *
 * @param state Stav playeru.
 * @param block Blok 0x23.
 */
static void handle_jump ( st_TMZ_PLAYER_STATE *state, const st_TZX_BLOCK *block ) {
    int16_t offset = 0;
    if ( block->data && block->length >= 2 ) {
        offset = (int16_t) ( block->data[0] | ( block->data[1] << 8 ) );
    }

    int64_t target = (int64_t) state->current_block + offset;

    if ( target < 0 || target >= (int64_t) state->file->block_count ) {
        g_player_error_cb ( __func__, __LINE__,
            "Jump target out of range: block %u + offset %d = %lld\n",
            state->current_block, offset, (long long) target );
        state->finished = true;
        return;
    }

    state->current_block = (uint32_t) target;
}


/**
 * @brief Zpracuje blok Call Sequence (0x26).
 *
 * Nacte pocet offsetu a skoci na prvni z nich. Ulozi navratovy
 * zaznam na zasobnik volani. Offsety jsou relativni vuci Call bloku.
 *
 * @param state Stav playeru.
 * @param block Blok 0x26.
 */
static void handle_call_sequence ( st_TMZ_PLAYER_STATE *state, const st_TZX_BLOCK *block ) {
    if ( state->call_depth >= TMZ_PLAYER_MAX_CALL_DEPTH ) {
        g_player_error_cb ( __func__, __LINE__,
            "Call nesting depth exceeded (max %d)\n", TMZ_PLAYER_MAX_CALL_DEPTH );
        state->current_block++;
        return;
    }

    uint16_t count = 0;
    if ( block->data && block->length >= 2 ) {
        count = (uint16_t) ( block->data[0] | ( block->data[1] << 8 ) );
    }

    if ( count == 0 || block->length < 2 + (uint32_t) count * 2 ) {
        /* prazdna sekvence nebo nedostatek dat - preskocit */
        state->current_block++;
        return;
    }

    /* nacist prvni offset a skocit na nej */
    int16_t first_offset = (int16_t) ( block->data[2] | ( block->data[3] << 8 ) );
    int64_t target = (int64_t) state->current_block + first_offset;

    if ( target < 0 || target >= (int64_t) state->file->block_count ) {
        g_player_error_cb ( __func__, __LINE__,
            "Call target out of range: block %u + offset %d = %lld\n",
            state->current_block, first_offset, (long long) target );
        state->current_block++;
        return;
    }

    /* ulozit na zasobnik */
    st_TMZ_PLAYER_CALL_ENTRY *entry = &state->call_stack[state->call_depth];
    entry->call_block = state->current_block;
    entry->current_offset_idx = 0;
    entry->total_offsets = count;
    state->call_depth++;

    state->current_block = (uint32_t) target;
}


/**
 * @brief Zpracuje blok Return from Sequence (0x27).
 *
 * Posune na dalsi offset v aktualnim Call Sequence.
 * Pokud jsou vsechny offsety zpracovany, pop ze zasobniku
 * a pokracuje blokem za Call Sequence.
 *
 * @param state Stav playeru.
 */
static void handle_return ( st_TMZ_PLAYER_STATE *state ) {
    if ( state->call_depth <= 0 ) {
        /* Return bez Call - preskocit */
        state->current_block++;
        return;
    }

    st_TMZ_PLAYER_CALL_ENTRY *entry = &state->call_stack[state->call_depth - 1];
    entry->current_offset_idx++;

    if ( entry->current_offset_idx < entry->total_offsets ) {
        /* dalsi volani v sekvenci */
        const st_TZX_BLOCK *call_block = &state->file->blocks[entry->call_block];
        uint16_t idx = entry->current_offset_idx;

        if ( call_block->data && call_block->length >= 2 + (uint32_t) ( idx + 1 ) * 2 ) {
            int16_t offset = (int16_t) ( call_block->data[2 + idx * 2] |
                                          ( call_block->data[3 + idx * 2] << 8 ) );
            int64_t target = (int64_t) entry->call_block + offset;

            if ( target >= 0 && target < (int64_t) state->file->block_count ) {
                state->current_block = (uint32_t) target;
                return;
            }
        }

        /* chybny offset - ukoncit call sekvenci */
        g_player_error_cb ( __func__, __LINE__,
            "Call sequence offset %u out of range\n", idx );
    }

    /* vsechny offsety zpracovany nebo chyba - pop a pokracovat za Call blokem */
    state->call_depth--;
    state->current_block = entry->call_block + 1;
}


/**
 * @brief Zpracuje blok Set Signal Level (0x2B).
 *
 * Nastavi uroven signalu (0=LOW, 1=HIGH) pro nasledujici audio blok.
 *
 * @param state Stav playeru.
 * @param block Blok 0x2B.
 */
static void handle_set_signal_level ( st_TMZ_PLAYER_STATE *state, const st_TZX_BLOCK *block ) {
    if ( block->data && block->length >= 5 ) {
        state->signal_level = block->data[4];
    }
    state->current_block++;
}


/**
 * @brief Prehaje dalsi audio blok v sekvenci.
 *
 * Interně zpracovava ridici bloky (Loop Start/End, Jump,
 * Call Sequence/Return, Set Signal Level, Group Start/End)
 * a vraci CMT stream az pri narazeni na audio blok.
 *
 * Ochrana proti nekonecne smycce: max 1 000 000 iteraci vnitrni smycky.
 *
 * @param state Stav playeru.
 * @param err   Vystupni chybovy kod (muze byt NULL).
 * @return Novy CMT stream, nebo NULL pokud konec / chyba.
 *
 * @pre state nesmi byt NULL a musi byt inicializovany.
 * @post Volajici je vlastnikem vraceneho streamu.
 */
st_CMT_STREAM* tmz_player_play_next ( st_TMZ_PLAYER_STATE *state,
                                       en_TMZ_PLAYER_ERROR *err ) {

    en_TMZ_PLAYER_ERROR local_err = TMZ_PLAYER_OK;
    if ( !err ) err = &local_err;

    if ( !state || !state->file ) {
        *err = TMZ_PLAYER_ERROR_NULL_INPUT;
        return NULL;
    }

    /* ochrana proti nekonecne smycce */
    uint32_t iterations = 0;
    const uint32_t max_iterations = 1000000;

    while ( !state->finished && iterations < max_iterations ) {
        iterations++;

        if ( state->current_block >= state->file->block_count ) {
            state->finished = true;
            *err = TMZ_PLAYER_OK;
            return NULL;
        }

        const st_TZX_BLOCK *block = &state->file->blocks[state->current_block];

        switch ( block->id ) {

            /* ridici bloky - zpracovat a pokracovat */
            case TZX_BLOCK_ID_LOOP_START:
                handle_loop_start ( state, block );
                continue;

            case TZX_BLOCK_ID_LOOP_END:
                handle_loop_end ( state );
                continue;

            case TZX_BLOCK_ID_JUMP:
                handle_jump ( state, block );
                continue;

            case TZX_BLOCK_ID_CALL_SEQUENCE:
                handle_call_sequence ( state, block );
                continue;

            case TZX_BLOCK_ID_RETURN_FROM_SEQ:
                handle_return ( state );
                continue;

            case TZX_BLOCK_ID_SET_SIGNAL_LEVEL:
                handle_set_signal_level ( state, block );
                continue;

            /* informacni/metadatove bloky - preskocit */
            case TZX_BLOCK_ID_GROUP_START:
            case TZX_BLOCK_ID_GROUP_END:
            case TZX_BLOCK_ID_SELECT_BLOCK:
            case TZX_BLOCK_ID_STOP_48K:
            case TZX_BLOCK_ID_TEXT_DESCRIPTION:
            case TZX_BLOCK_ID_MESSAGE:
            case TZX_BLOCK_ID_ARCHIVE_INFO:
            case TZX_BLOCK_ID_HARDWARE_TYPE:
            case TZX_BLOCK_ID_CUSTOM_INFO:
            case TZX_BLOCK_ID_GLUE:
            case TMZ_BLOCK_ID_MZ_MACHINE_INFO:
                state->current_block++;
                continue;

            /* audio a datove bloky - prehrat a vratit */
            default:
            {
                state->last_played_block = state->current_block;
                st_CMT_STREAM *stream = tmz_player_play_block ( block, &state->config, err );
                state->current_block++;

                if ( stream ) {
                    return stream;
                }

                /* blok nevygeneroval audio (neznamy typ, chyba) */
                if ( *err != TMZ_PLAYER_OK ) {
                    return NULL;
                }
                /* err == OK ale stream == NULL: neaudio blok - pokracovat */
                continue;
            }
        }
    }

    if ( iterations >= max_iterations ) {
        g_player_error_cb ( __func__, __LINE__,
            "Maximum iteration count exceeded (%u) - possible infinite loop\n",
            max_iterations );
        state->finished = true;
        *err = TMZ_PLAYER_ERROR_STREAM_CREATE;
        return NULL;
    }

    *err = TMZ_PLAYER_OK;
    return NULL;
}
