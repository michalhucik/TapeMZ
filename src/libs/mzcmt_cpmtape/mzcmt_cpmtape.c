/**
 * @file   mzcmt_cpmtape.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Implementace CPM-TAPE koderu pro Sharp MZ (Pezik/MarVan format).
 *
 * CPM-TAPE je unikatni kazetovy format pouzivany programy ZTAPE a TAPE.COM
 * pro CP/M na Sharp MZ-800. Pouziva manchesterske kodovani s LSB-first
 * poradim bitu a bez stop bitu.
 *
 * @par Kodovani bitu (Manchester):
 * Kazdy bit zabira jeden celý period. Hodnota bitu urcuje polaritu:
 * - Bit 1: prvni pulperioda HIGH, druha LOW
 * - Bit 0: prvni pulperioda LOW, druha HIGH
 *
 * Pri sousednich bitech stejne hodnoty dochazi k prechodu na hranici
 * bitu (napr. 1-1: ...LOW-HIGH..., 0-0: ...HIGH-LOW...).
 * Pri sousednich bitech ruzne hodnoty se pulperiody na hranici spoji
 * (napr. 1-0: ...LOW-LOW... = "long" interval).
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
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include "libs/cmt_stream/cmt_stream.h"
#include "libs/endianity/endianity.h"
#include "libs/mzf/mzf.h"

#include "mzcmt_cpmtape.h"


/* =========================================================================
 *  Alokator a error callback
 * ========================================================================= */

/** @brief Vychozi alokace pameti (obaluje malloc). */
static void* mzcmt_cpmtape_default_alloc ( size_t size ) { return malloc ( size ); }
/** @brief Vychozi alokace s nulovanim (obaluje calloc). */
static void* mzcmt_cpmtape_default_alloc0 ( size_t size ) { return calloc ( 1, size ); }
/** @brief Vychozi uvolneni pameti (obaluje free). */
static void  mzcmt_cpmtape_default_free ( void *ptr ) { free ( ptr ); }

/** @brief Vychozi alokator vyuzivajici standardni knihovni funkce. */
static const st_MZCMT_CPMTAPE_ALLOCATOR g_mzcmt_cpmtape_default_allocator = {
    mzcmt_cpmtape_default_alloc,
    mzcmt_cpmtape_default_alloc0,
    mzcmt_cpmtape_default_free,
};

/** @brief Aktualne aktivni alokator (vychozi = stdlib). */
static const st_MZCMT_CPMTAPE_ALLOCATOR *g_allocator = &g_mzcmt_cpmtape_default_allocator;


/** @brief Nastavi vlastni alokator, nebo resetuje na vychozi pri NULL. */
void mzcmt_cpmtape_set_allocator ( const st_MZCMT_CPMTAPE_ALLOCATOR *allocator ) {
    g_allocator = allocator ? allocator : &g_mzcmt_cpmtape_default_allocator;
}


/**
 * @brief Vychozi error callback - vypisuje chyby na stderr.
 * @param func Nazev volajici funkce.
 * @param line Cislo radku.
 * @param fmt Formatovaci retezec (printf styl).
 */
static void mzcmt_cpmtape_default_error_cb ( const char *func, int line, const char *fmt, ... ) {
    va_list args;
    fprintf ( stderr, "%s():%d - ", func, line );
    va_start ( args, fmt );
    vfprintf ( stderr, fmt, args );
    va_end ( args );
}

/** @brief Aktualne aktivni error callback. */
static mzcmt_cpmtape_error_cb g_error_cb = mzcmt_cpmtape_default_error_cb;


/** @brief Nastavi vlastni error callback, nebo resetuje na vychozi pri NULL. */
void mzcmt_cpmtape_set_error_callback ( mzcmt_cpmtape_error_cb cb ) {
    g_error_cb = cb ? cb : mzcmt_cpmtape_default_error_cb;
}


/* =========================================================================
 *  Konstanty rychlosti
 * ========================================================================= */

/**
 * @brief Tabulka baud rate pro jednotlive rychlostni urovne.
 *
 * Index odpovida en_MZCMT_CPMTAPE_SPEED.
 */
static const double g_baud_rates[MZCMT_CPMTAPE_SPEED_COUNT] = {
    1200.0,  /* MZCMT_CPMTAPE_SPEED_1200 */
    2400.0,  /* MZCMT_CPMTAPE_SPEED_2400 */
    3200.0,  /* MZCMT_CPMTAPE_SPEED_3200 */
};


/* =========================================================================
 *  Interni pomocne funkce - kodovani bitu
 * ========================================================================= */

/**
 * @brief Zakoduje jeden bit manchesterskym kodovanim do vstreamu.
 *
 * Kazdy bit = dva pul-periody:
 * - Bit 1: HIGH(half_samples) + LOW(half_samples)
 * - Bit 0: LOW(half_samples) + HIGH(half_samples)
 *
 * Vstream automaticky spojuje po sobe jdouci vzorky se stejnou
 * hodnotou do jednoho eventu (RLE), takze sousedni bity stejne
 * hodnoty se spravne spoji na hranici.
 *
 * @param vstream Cilovy vstream.
 * @param bit Hodnota bitu (0 nebo 1).
 * @param half_samples Pocet vzorku jedne pulperiody.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int cpmtape_encode_bit ( st_CMT_VSTREAM *vstream, int bit, uint32_t half_samples ) {
    if ( bit ) {
        /* bit 1: HIGH pak LOW */
        if ( EXIT_SUCCESS != cmt_vstream_add_value ( vstream, 1, half_samples ) ) return EXIT_FAILURE;
        if ( EXIT_SUCCESS != cmt_vstream_add_value ( vstream, 0, half_samples ) ) return EXIT_FAILURE;
    } else {
        /* bit 0: LOW pak HIGH */
        if ( EXIT_SUCCESS != cmt_vstream_add_value ( vstream, 0, half_samples ) ) return EXIT_FAILURE;
        if ( EXIT_SUCCESS != cmt_vstream_add_value ( vstream, 1, half_samples ) ) return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


/**
 * @brief Zakoduje jeden bajt manchesterskym kodovanim (LSB first, 8 bitu).
 *
 * Bity se odesílají od nejnizsiho (LSB) k nejvyssimu (MSB),
 * coz odpovida instrukci RRC D na Z80 (rotace vpravo pres carry).
 * Stop bit se NEPRIDAVA.
 *
 * @param vstream Cilovy vstream.
 * @param byte Bajt k zakodovani.
 * @param half_samples Pocet vzorku jedne pulperiody.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int cpmtape_encode_byte ( st_CMT_VSTREAM *vstream, uint8_t byte, uint32_t half_samples ) {
    int i;
    for ( i = 0; i < 8; i++ ) {
        int bit = ( byte >> i ) & 1;
        if ( EXIT_SUCCESS != cpmtape_encode_bit ( vstream, bit, half_samples ) ) return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


/**
 * @brief Zakoduje jeden bajt, ale odesle jen 7 bitu (bez MSB).
 *
 * Pro posledni bajt body bloku - MSB (bit 7) se neprenasi.
 * Dekoder rekonstruuje chybejici bit z checksumu.
 *
 * @param vstream Cilovy vstream.
 * @param byte Bajt k zakodovani (odesle se jen bity 0-6).
 * @param half_samples Pocet vzorku jedne pulperiody.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int cpmtape_encode_byte_partial ( st_CMT_VSTREAM *vstream, uint8_t byte, uint32_t half_samples ) {
    int i;
    for ( i = 0; i < 7; i++ ) {
        int bit = ( byte >> i ) & 1;
        if ( EXIT_SUCCESS != cpmtape_encode_bit ( vstream, bit, half_samples ) ) return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


/* =========================================================================
 *  Interni pomocne funkce - struktura signalu
 * ========================================================================= */

/**
 * @brief Prida pilotni sekvenci do vstreamu.
 *
 * Pilot je opakujici se sekvence "011":
 * - Bit 0: manchestersky bit s prodlouzenym timingem (2x normalni)
 * - Bit 1: manchestersky bit s normalnim timingem
 * - Bit 1: manchestersky bit s normalnim timingem
 *
 * Prodlouzeny timing bitu 0 vytvari rozpoznatelny vzor pro detektor.
 *
 * @param vstream Cilovy vstream.
 * @param count Pocet opakovani "011" sekvence.
 * @param half_samples Normalni pulperioda v poctech vzorku.
 * @param ext_half_samples Prodlouzena pulperioda pro bit 0 v pilotu.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int cpmtape_add_pilot ( st_CMT_VSTREAM *vstream, uint32_t count,
                                uint32_t half_samples, uint32_t ext_half_samples ) {
    uint32_t i;
    for ( i = 0; i < count; i++ ) {
        /* bit 0 s prodlouzenym timingem */
        if ( EXIT_SUCCESS != cpmtape_encode_bit ( vstream, 0, ext_half_samples ) ) return EXIT_FAILURE;
        /* bit 1 s normalnim timingem */
        if ( EXIT_SUCCESS != cpmtape_encode_bit ( vstream, 1, half_samples ) ) return EXIT_FAILURE;
        /* bit 1 s normalnim timingem */
        if ( EXIT_SUCCESS != cpmtape_encode_bit ( vstream, 1, half_samples ) ) return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


/**
 * @brief Prida sync bit do vstreamu.
 *
 * Sync je jediny bit 1 s velmi prodlouzenym timingem (cca 5x normalni).
 * Oznacuje konec pilotu a zacatek datoveho bloku (flag byte nasleduje).
 *
 * @param vstream Cilovy vstream.
 * @param sync_half_samples Pulperioda sync bitu v poctech vzorku.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int cpmtape_add_sync ( st_CMT_VSTREAM *vstream, uint32_t sync_half_samples ) {
    return cpmtape_encode_bit ( vstream, 1, sync_half_samples );
}


/**
 * @brief Prida separator (konec bloku) do vstreamu.
 *
 * Separator je jediny bit 1 s prodlouzenym timingem. Uzavira blok
 * po checksumu a signalizuje dekoderu konec aktualne prijimaneho bloku.
 *
 * @param vstream Cilovy vstream.
 * @param sync_half_samples Pulperioda separatoru (shodna se sync bitem).
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int cpmtape_add_separator ( st_CMT_VSTREAM *vstream, uint32_t sync_half_samples ) {
    return cpmtape_encode_bit ( vstream, 1, sync_half_samples );
}


/**
 * @brief Prida ticho (gap) do vstreamu.
 *
 * Gap je obdobi ticha (LOW signal) mezi header a body bloky.
 * Umoznuje dekoderu zpracovat hlavicku pred ctenim body.
 *
 * @param vstream Cilovy vstream.
 * @param gap_samples Pocet vzorku ticha.
 * @return EXIT_SUCCESS pri uspechu, EXIT_FAILURE pri chybe.
 */
static int cpmtape_add_gap ( st_CMT_VSTREAM *vstream, uint32_t gap_samples ) {
    if ( gap_samples > 0 ) {
        if ( EXIT_SUCCESS != cmt_vstream_add_value ( vstream, 0, gap_samples ) ) return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


/**
 * @brief Serializuje MZF hlavicku do 128B pole v LE formatu.
 *
 * Prevede st_MZF_HEADER (s host byte order uint16 poli) zpet do
 * little-endian bajtu, jak je ocekava CPM-TAPE format na pasce.
 *
 * @param[out] out Vystupni buffer (musi byt >= 128 bajtu).
 * @param[in]  hdr Zdrojova MZF hlavicka v host byte order.
 */
static void cpmtape_serialize_header ( uint8_t out[MZF_HEADER_SIZE], const st_MZF_HEADER *hdr ) {
    /* kopirovani cele packed struktury (128 bajtu) */
    memcpy ( out, hdr, MZF_HEADER_SIZE );
    /* prevod uint16 poli z host byte order na LE */
    st_MZF_HEADER *le = ( st_MZF_HEADER* ) out;
    le->fsize = endianity_bswap16_LE ( le->fsize );
    le->fstrt = endianity_bswap16_LE ( le->fstrt );
    le->fexec = endianity_bswap16_LE ( le->fexec );
    le->fname.terminator = MZF_FNAME_TERMINATOR;
}


/* =========================================================================
 *  Verejne API
 * ========================================================================= */

/**
 * @brief Spocita 16-bitovy checksum (soucet bajtu).
 *
 * Jednoduchy 16-bitovy soucet vsech bajtu v bloku (modulo 65536).
 * Checksum se pocita nad kompletnimi daty (vsech 8 bitu kazdeho bajtu),
 * i kdyz se posledni bit body bloku na pasce neprenasi.
 *
 * @param data Ukazatel na datovy blok.
 * @param size Velikost bloku v bajtech.
 * @return 16-bitovy soucet. Vraci 0 pro NULL nebo size==0.
 */
uint16_t mzcmt_cpmtape_compute_checksum ( const uint8_t *data, uint32_t size ) {
    if ( !data || size == 0 ) return 0;
    uint16_t sum = 0;
    uint32_t i;
    for ( i = 0; i < size; i++ ) {
        sum += data[i];
    }
    return sum;
}


/**
 * @brief Vytvori CMT vstream s kompletnim CPM-TAPE signalem.
 *
 * Generuje kompletni signal pro TAPE.COM/ZTAPE detektor:
 *
 * 1. HEADER BLOK:
 *    - Pilot: 2000x "011" sekvence (bit 0 s 2x timingem)
 *    - Sync: bit 1 s 5x timingem
 *    - Flag $00 (8 bitu LSB first)
 *    - 128B MZF hlavicky (8 bitu LSB first na bajt)
 *    - 16-bit checksum (low byte first)
 *    - Separator: bit 1 s 5x timingem
 *
 * 2. GAP: 0.5 s ticha (LOW signal)
 *
 * 3. BODY BLOK (pokud body_size > 0):
 *    - Pilot: 800x "011" sekvence
 *    - Sync: bit 1 s 5x timingem
 *    - Flag $01 (8 bitu LSB first)
 *    - body_size bajtu dat (body_size*8-1 bitu, posledni bit vynechan)
 *    - 16-bit checksum plnych dat (low byte first)
 *    - Separator: bit 1 s 5x timingem
 *
 * Signal zacina v LOW stavu.
 *
 * @param header    MZF hlavicka (host byte order).
 * @param body      Ukazatel na datove telo.
 * @param body_size Velikost datoveho tela v bajtech.
 * @param config    Konfigurace koderu.
 * @param rate      Vzorkovaci frekvence (Hz).
 * @return Novy vstream, nebo NULL pri chybe.
 */
st_CMT_VSTREAM* mzcmt_cpmtape_create_vstream (
    const st_MZF_HEADER *header,
    const uint8_t *body,
    uint32_t body_size,
    const st_MZCMT_CPMTAPE_CONFIG *config,
    uint32_t rate
) {
    /* validace vstupu */
    if ( !header ) {
        g_error_cb ( __func__, __LINE__, "Header is NULL\n" );
        return NULL;
    }

    if ( body_size > 0 && !body ) {
        g_error_cb ( __func__, __LINE__, "Body is NULL but body_size=%u\n", body_size );
        return NULL;
    }

    if ( !config ) {
        g_error_cb ( __func__, __LINE__, "Config is NULL\n" );
        return NULL;
    }

    if ( config->speed >= MZCMT_CPMTAPE_SPEED_COUNT ) {
        g_error_cb ( __func__, __LINE__, "Invalid speed %d\n", config->speed );
        return NULL;
    }

    if ( rate == 0 ) {
        g_error_cb ( __func__, __LINE__, "Sample rate is 0\n" );
        return NULL;
    }

    /* ===== Vypocet pulznich delek ===== */

    double baud = g_baud_rates[config->speed];

    /* normalni pulperioda v sekundach a vzorcich */
    double half_period_s = 1.0 / ( 2.0 * baud );
    uint32_t half_samples = ( uint32_t ) round ( half_period_s * rate );
    if ( half_samples < 1 ) half_samples = 1;

    /* prodlouzena pulperioda pro pilotni bit 0 */
    uint32_t ext_half_samples = ( uint32_t ) round ( half_period_s * MZCMT_CPMTAPE_PILOT_EXTEND * rate );
    if ( ext_half_samples < 1 ) ext_half_samples = 1;

    /* pulperioda pro sync a separator */
    uint32_t sync_half_samples = ( uint32_t ) round ( half_period_s * MZCMT_CPMTAPE_SYNC_EXTEND * rate );
    if ( sync_half_samples < 1 ) sync_half_samples = 1;

    /* gap (ticho) mezi header a body bloky */
    uint32_t gap_samples = ( uint32_t ) round ( MZCMT_CPMTAPE_GAP_SECONDS * rate );

    /* ===== Serializace hlavicky do LE bajtu ===== */

    uint8_t hdr_bytes[MZF_HEADER_SIZE];
    cpmtape_serialize_header ( hdr_bytes, header );

    /* ===== Checksums ===== */

    /* header checksum: flag($00) + 128B hlavicky */
    uint16_t hdr_chk = 0x00; /* flag byte */
    hdr_chk += mzcmt_cpmtape_compute_checksum ( hdr_bytes, MZF_HEADER_SIZE );

    /* body checksum: flag($01) + body data */
    uint16_t body_chk = 0x01; /* flag byte */
    if ( body_size > 0 ) {
        body_chk += mzcmt_cpmtape_compute_checksum ( body, body_size );
    }

    /* ===== Vytvoreni vstreamu ===== */

    /* signal zacina v LOW stavu */
    st_CMT_VSTREAM *vstream = cmt_vstream_new ( rate, CMT_VSTREAM_BYTELENGTH8, 0, CMT_STREAM_POLARITY_NORMAL );
    if ( !vstream ) {
        g_error_cb ( __func__, __LINE__, "Can't create vstream\n" );
        return NULL;
    }

    /* ===== HEADER BLOK ===== */

    /* 1. pilot (2000x "011") */
    if ( EXIT_SUCCESS != cpmtape_add_pilot ( vstream, MZCMT_CPMTAPE_PILOT_HDR_COUNT,
                                              half_samples, ext_half_samples ) ) goto error;

    /* 2. sync */
    if ( EXIT_SUCCESS != cpmtape_add_sync ( vstream, sync_half_samples ) ) goto error;

    /* 3. flag byte $00 */
    if ( EXIT_SUCCESS != cpmtape_encode_byte ( vstream, 0x00, half_samples ) ) goto error;

    /* 4. 128B hlavicky */
    {
        int i;
        for ( i = 0; i < MZF_HEADER_SIZE; i++ ) {
            if ( EXIT_SUCCESS != cpmtape_encode_byte ( vstream, hdr_bytes[i], half_samples ) ) goto error;
        }
    }

    /* 5. checksum (low byte first) */
    if ( EXIT_SUCCESS != cpmtape_encode_byte ( vstream, ( uint8_t ) ( hdr_chk & 0xFF ), half_samples ) ) goto error;
    if ( EXIT_SUCCESS != cpmtape_encode_byte ( vstream, ( uint8_t ) ( hdr_chk >> 8 ), half_samples ) ) goto error;

    /* 6. separator */
    if ( EXIT_SUCCESS != cpmtape_add_separator ( vstream, sync_half_samples ) ) goto error;

    /* ===== GAP ===== */

    if ( EXIT_SUCCESS != cpmtape_add_gap ( vstream, gap_samples ) ) goto error;

    /* ===== BODY BLOK ===== */

    if ( body_size > 0 ) {
        /* 1. pilot (800x "011") */
        if ( EXIT_SUCCESS != cpmtape_add_pilot ( vstream, MZCMT_CPMTAPE_PILOT_BODY_COUNT,
                                                  half_samples, ext_half_samples ) ) goto error;

        /* 2. sync */
        if ( EXIT_SUCCESS != cpmtape_add_sync ( vstream, sync_half_samples ) ) goto error;

        /* 3. flag byte $01 */
        if ( EXIT_SUCCESS != cpmtape_encode_byte ( vstream, 0x01, half_samples ) ) goto error;

        /* 4. data bajty (vsechny krome posledniho plne, posledni jen 7 bitu) */
        if ( body_size == 1 ) {
            /* jediny bajt - jen 7 bitu */
            if ( EXIT_SUCCESS != cpmtape_encode_byte_partial ( vstream, body[0], half_samples ) ) goto error;
        } else {
            /* vsechny bajty krome posledniho: plnych 8 bitu */
            uint32_t i;
            for ( i = 0; i < body_size - 1; i++ ) {
                if ( EXIT_SUCCESS != cpmtape_encode_byte ( vstream, body[i], half_samples ) ) goto error;
            }
            /* posledni bajt: jen 7 bitu (MSB vynechan) */
            if ( EXIT_SUCCESS != cpmtape_encode_byte_partial ( vstream, body[body_size - 1], half_samples ) ) goto error;
        }

        /* 5. checksum (low byte first) */
        if ( EXIT_SUCCESS != cpmtape_encode_byte ( vstream, ( uint8_t ) ( body_chk & 0xFF ), half_samples ) ) goto error;
        if ( EXIT_SUCCESS != cpmtape_encode_byte ( vstream, ( uint8_t ) ( body_chk >> 8 ), half_samples ) ) goto error;

        /* 6. separator */
        if ( EXIT_SUCCESS != cpmtape_add_separator ( vstream, sync_half_samples ) ) goto error;
    }

    return vstream;

error:
    g_error_cb ( __func__, __LINE__, "Error during CPM-TAPE signal generation\n" );
    cmt_vstream_destroy ( vstream );
    return NULL;
}


/**
 * @brief Vytvori jednotny CMT stream s kompletnim CPM-TAPE signalem.
 *
 * Pro typ CMT_STREAM_TYPE_VSTREAM primo pouzije mzcmt_cpmtape_create_vstream().
 * Pro typ CMT_STREAM_TYPE_BITSTREAM vytvori vstream a konvertuje jej na
 * bitstream (presnejsi nez prima generace bitstreamu).
 *
 * @param header    MZF hlavicka.
 * @param body      Ukazatel na datove telo.
 * @param body_size Velikost datoveho tela v bajtech.
 * @param config    Konfigurace koderu.
 * @param type      Typ vystupniho streamu (bitstream/vstream).
 * @param rate      Vzorkovaci frekvence (Hz).
 * @return Novy stream, nebo NULL pri chybe.
 */
st_CMT_STREAM* mzcmt_cpmtape_create_stream (
    const st_MZF_HEADER *header,
    const uint8_t *body,
    uint32_t body_size,
    const st_MZCMT_CPMTAPE_CONFIG *config,
    en_CMT_STREAM_TYPE type,
    uint32_t rate
) {
    st_CMT_STREAM *stream = cmt_stream_new ( type );
    if ( !stream ) {
        g_error_cb ( __func__, __LINE__, "Can't create CMT stream\n" );
        return NULL;
    }

    switch ( stream->stream_type ) {
        case CMT_STREAM_TYPE_BITSTREAM:
        {
            /* vstream -> bitstream konverze (presnejsi) */
            st_CMT_VSTREAM *vstream = mzcmt_cpmtape_create_vstream ( header, body, body_size, config, rate );
            if ( !vstream ) {
                cmt_stream_destroy ( stream );
                return NULL;
            }

            st_CMT_BITSTREAM *bitstream = cmt_bitstream_new_from_vstream ( vstream, rate );
            cmt_vstream_destroy ( vstream );

            if ( !bitstream ) {
                g_error_cb ( __func__, __LINE__, "Can't convert vstream to bitstream\n" );
                cmt_stream_destroy ( stream );
                return NULL;
            }
            stream->str.bitstream = bitstream;
            break;
        }

        case CMT_STREAM_TYPE_VSTREAM:
        {
            st_CMT_VSTREAM *vstream = mzcmt_cpmtape_create_vstream ( header, body, body_size, config, rate );
            if ( !vstream ) {
                cmt_stream_destroy ( stream );
                return NULL;
            }
            stream->str.vstream = vstream;
            break;
        }

        default:
            g_error_cb ( __func__, __LINE__, "Unknown stream type '%d'\n", stream->stream_type );
            cmt_stream_destroy ( stream );
            return NULL;
    }

    return stream;
}
