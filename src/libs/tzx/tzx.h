/**
 * @file   tzx.h
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Verejne API knihovny pro format TZX v1.20 - kazetovy archiv ZX Spectrum.
 *
 * Knihovna pro parsovani a generovani audio streamu ze vsech bloku TZX v1.20.
 * Implementuje kompletni sadu audio bloku (0x10-0x19) a ridicich bloku (0x20-0x2B).
 *
 * Audio bloky generuji CMT stream (bitstream/vstream) z pulsovych definici.
 * Casovani je v Z80 T-states pri 3.5 MHz, ale lze prepocitat na libovolny
 * CPU takt pomoci prepoctoveho koeficientu.
 *
 * Tato knihovna je pouzivana knihovnou tmz pro prehravani standardnich
 * TZX bloku uvnitr TMZ souboru.
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


#ifndef TZX_H
#define TZX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../cmt_stream/cmt_stream.h"
#include "../generic_driver/generic_driver.h"


/** @defgroup tzx_file_constants Konstanty souboroveho formatu TZX/TMZ
 *  @{ */

/** @brief Signatura TZX souboru ("ZXTape!") */
#define TZX_SIGNATURE           "ZXTape!"

/** @brief Delka signatury v bajtech (bez nuloveho terminatoru) */
#define TZX_SIGNATURE_LENGTH    7

/** @brief EOF marker nasledujici za signaturou (standardni hodnota 0x1A) */
#define TZX_EOF_MARKER          0x1A

/** @brief Hlavni cislo verze TZX formatu */
#define TZX_VERSION_MAJOR       1

/** @brief Vedlejsi cislo verze TZX formatu (TZX v1.20) */
#define TZX_VERSION_MINOR       20

/** @} */


/** @defgroup tzx_block_ids Identifikatory TZX bloku
 *  @{ */

/** @brief Standardni rychlostni datovy blok (1000 T-states pilot, 2168 pilot pulzu) */
#define TZX_BLOCK_ID_STANDARD_SPEED    0x10
/** @brief Turbo rychlostni datovy blok (uzivatelsky definovane casovani) */
#define TZX_BLOCK_ID_TURBO_SPEED       0x11
/** @brief Cisty ton - sekvence pulzu stejne delky */
#define TZX_BLOCK_ID_PURE_TONE         0x12
/** @brief Sekvence pulzu ruznych delek */
#define TZX_BLOCK_ID_PULSE_SEQUENCE    0x13
/** @brief Cista data bez pilot tonu a sync pulzu */
#define TZX_BLOCK_ID_PURE_DATA         0x14
/** @brief Primy zaznam - vzorky s definovanou vzorkovaci frekvenci */
#define TZX_BLOCK_ID_DIRECT_RECORDING  0x15
/** @brief CSW zaznam - komprimovany zvukovy zaznam */
#define TZX_BLOCK_ID_CSW_RECORDING     0x18
/** @brief Generalizovany datovy blok (TZX v1.20) */
#define TZX_BLOCK_ID_GENERALIZED_DATA  0x19
/** @brief Pauza (ticho) nebo zastaveni pasky */
#define TZX_BLOCK_ID_PAUSE             0x20
/** @brief Zacatek skupiny bloku */
#define TZX_BLOCK_ID_GROUP_START       0x21
/** @brief Konec skupiny bloku */
#define TZX_BLOCK_ID_GROUP_END         0x22
/** @brief Skok na jiny blok (relativni offset) */
#define TZX_BLOCK_ID_JUMP             0x23
/** @brief Zacatek smycky (opakujici se sekvence bloku) */
#define TZX_BLOCK_ID_LOOP_START       0x24
/** @brief Konec smycky */
#define TZX_BLOCK_ID_LOOP_END         0x25
/** @brief Sekvence volani bloku (CALL) */
#define TZX_BLOCK_ID_CALL_SEQUENCE    0x26
/** @brief Navrat ze sekvence volani (RETURN) */
#define TZX_BLOCK_ID_RETURN_FROM_SEQ  0x27
/** @brief Vyber bloku z nabidky (interaktivni) */
#define TZX_BLOCK_ID_SELECT_BLOCK     0x28
/** @brief Zastaveni pasky v 48K rezimu */
#define TZX_BLOCK_ID_STOP_48K         0x2A
/** @brief Nastaveni urovne signalu (vysoka/nizka) */
#define TZX_BLOCK_ID_SET_SIGNAL_LEVEL 0x2B
/** @brief Textovy popis (informativni, neni soucasti dat) */
#define TZX_BLOCK_ID_TEXT_DESCRIPTION 0x30
/** @brief Zprava se zobrazenim (casovany text) */
#define TZX_BLOCK_ID_MESSAGE          0x31
/** @brief Archivni informace (nazev, autor, rok, ...) */
#define TZX_BLOCK_ID_ARCHIVE_INFO     0x32
/** @brief Informace o hardwaru (seznam pouziteho HW) */
#define TZX_BLOCK_ID_HARDWARE_TYPE    0x33
/** @brief Uzivatelsky definovany informacni blok */
#define TZX_BLOCK_ID_CUSTOM_INFO      0x35
/** @brief Spojovaci blok (pro spojeni vice TZX souboru) */
#define TZX_BLOCK_ID_GLUE             0x5A

/** @} */


/** @defgroup tzx_header Hlavicka TZX/TMZ souboru
 *  @{ */

    /**
     * @brief Hlavicka TZX/TMZ souboru - 10 bajtu, packed.
     *
     * Prvnich 7 bajtu obsahuje signaturu ("ZXTape!" nebo "TapeMZ!"),
     * nasleduje EOF marker (0x1A) a dvoubajtove cislo verze.
     *
     * Layout odpovida puvodnimu TZX formatu, takze soubory jsou
     * vzajemne rozpoznatelne podle signatury.
     *
     * @invariant sizeof(st_TZX_HEADER) == 10
     * @invariant eof_marker == 0x1A
     */
    typedef struct __attribute__((packed)) st_TZX_HEADER {
        uint8_t signature[TZX_SIGNATURE_LENGTH]; /**< "ZXTape!" nebo "TapeMZ!" */
        uint8_t eof_marker;                      /**< EOF marker (vzdy 0x1A) */
        uint8_t ver_major;                       /**< hlavni cislo verze */
        uint8_t ver_minor;                       /**< vedlejsi cislo verze */
    } st_TZX_HEADER;

    /* Compile-time overeni velikosti hlavicky */
#ifdef __cplusplus
    static_assert ( sizeof ( st_TZX_HEADER ) == 10,
                    "st_TZX_HEADER musi mit presne 10 bajtu" );
#else
    _Static_assert ( sizeof ( st_TZX_HEADER ) == 10,
                     "st_TZX_HEADER musi mit presne 10 bajtu" );
#endif

/** @} */


/** @defgroup tzx_structs Datove struktury souborove vrstvy
 *  @{ */

    /**
     * @brief Obecny blok TZX/TMZ souboru.
     *
     * Kazdy blok v souboru je identifikovan 1B ID a obsahuje
     * data, jejichz format zavisi na typu bloku.
     *
     * @par Ownership:
     * Pole data je vlastneno touto strukturou a je uvolneno
     * pri destrukci nadrazeneho st_TZX_FILE pres tzx_free().
     * Volajici nesmi uvolnovat data primo.
     */
    typedef struct st_TZX_BLOCK {
        uint8_t id;       /**< identifikator bloku (viz TZX_BLOCK_ID_*) */
        uint32_t length;  /**< delka dat za timto polem v bajtech */
        uint8_t *data;    /**< surova data bloku (vlastneno touto strukturou) */
    } st_TZX_BLOCK;


    /**
     * @brief Naparsovany TZX/TMZ soubor.
     *
     * Obsahuje hlavicku souboru a dynamicke pole vsech nactenych bloku.
     * Pole blocks je vlastneno touto strukturou a je uvolneno
     * pri volani tzx_free().
     *
     * @par Ownership:
     * Struktura a vsechna jeji data (vcetne bloku a jejich dat)
     * jsou vlastnena touto strukturou. Uvolneni probiha pres tzx_free().
     *
     * @invariant block_count odpovida poctu prvku v poli blocks
     * @invariant blocks != NULL kdyz block_count > 0
     */
    typedef struct st_TZX_FILE {
        st_TZX_HEADER header;     /**< hlavicka souboru (10 bajtu) */
        uint32_t block_count;     /**< pocet bloku v poli blocks */
        uint32_t block_capacity;  /**< alokovana kapacita pole blocks (pocet prvku) */
        st_TZX_BLOCK *blocks;     /**< pole bloku (vlastneno touto strukturou) */
        bool is_tmz;              /**< true = signatura "TapeMZ!", false = "ZXTape!" */
    } st_TZX_FILE;

/** @} */


/** @defgroup tzx_constants Konstanty formatu
 *  @{ */

/** @brief Vychozi CPU takt pro T-states prepocet (ZX Spectrum 3.5 MHz) */
#define TZX_DEFAULT_CPU_CLOCK   3500000

/** @brief Pocet T-states na vzorek pri 44100 Hz (79 T/vzorek) */
#define TZX_TSTATES_PER_SAMPLE_44100   79

/** @brief Pocet T-states na vzorek pri 22050 Hz (158 T/vzorek) */
#define TZX_TSTATES_PER_SAMPLE_22050   158

/** @} */


/** @defgroup tzx_default_timing Vychozi casovani (v T-states pri 3.5 MHz)
 *  Hodnoty ve slozenych zavorkach {} v TZX specifikaci.
 *  @{ */

/** @brief Delka pilot pulzu pro Standard Speed Data {2168} */
#define TZX_PILOT_PULSE_LENGTH      2168
/** @brief Delka 1. sync pulzu {667} */
#define TZX_SYNC1_PULSE_LENGTH      667
/** @brief Delka 2. sync pulzu {735} */
#define TZX_SYNC2_PULSE_LENGTH      735
/** @brief Delka pulzu bitu ZERO {855} */
#define TZX_ZERO_BIT_PULSE_LENGTH   855
/** @brief Delka pulzu bitu ONE {1710} */
#define TZX_ONE_BIT_PULSE_LENGTH    1710
/** @brief Pocet pilot pulzu pro header blok (flag < 128) {8063} */
#define TZX_PILOT_COUNT_HEADER      8063
/** @brief Pocet pilot pulzu pro data blok (flag >= 128) {3223} */
#define TZX_PILOT_COUNT_DATA        3223
/** @brief Vychozi pauza po bloku (ms) {1000} */
#define TZX_DEFAULT_PAUSE_MS        1000

/** @} */


/** @defgroup tzx_enums Vycty
 *  @{ */

    /**
     * @brief Chybove kody TZX operaci.
     *
     * Vsechny funkce vracejici en_TZX_ERROR vraci TZX_OK (0) pri uspechu
     * a konkretni chybovy kod pri selhani.
     */
    typedef enum en_TZX_ERROR {
        TZX_OK = 0,                 /**< operace uspesna */
        TZX_ERROR_NULL_INPUT,        /**< vstupni parametr je NULL */
        TZX_ERROR_INVALID_BLOCK,     /**< neplatna data bloku */
        TZX_ERROR_UNSUPPORTED,       /**< nepodporovany typ bloku */
        TZX_ERROR_STREAM_CREATE,     /**< chyba pri vytvareni CMT streamu */
        TZX_ERROR_ALLOC,             /**< selhani alokace pameti */
        TZX_ERROR_IO,                /**< chyba cteni/zapisu pres generic_driver */
        TZX_ERROR_INVALID_SIGNATURE, /**< signatura neni "ZXTape!" ani "TapeMZ!" */
        TZX_ERROR_INVALID_VERSION,   /**< nepodporovana verze formatu */
        TZX_ERROR_UNEXPECTED_EOF,    /**< neocekavany konec souboru pri cteni bloku */
        TZX_ERROR_UNKNOWN_BLOCK,     /**< neznamy identifikator bloku */
    } en_TZX_ERROR;

/** @} */


/** @defgroup tzx_config Konfigurace
 *  @{ */

    /**
     * @brief Konfigurace TZX generatoru.
     *
     * Urcuje parametry pro generovani audio streamu z TZX bloku.
     * cpu_clock umoznuje prepocet T-states na jiny CPU nez ZX Spectrum 3.5 MHz.
     */
    typedef struct st_TZX_CONFIG {
        uint32_t sample_rate;            /**< vzorkovaci frekvence (Hz), 0 = CMTSTREAM_DEFAULT_RATE */
        en_CMT_STREAM_TYPE stream_type;  /**< typ vystupniho streamu (bitstream/vstream) */
        uint32_t cpu_clock;              /**< CPU takt pro T-states prepocet (Hz), 0 = TZX_DEFAULT_CPU_CLOCK */
    } st_TZX_CONFIG;

    /**
     * @brief Inicializuje konfiguraci TZX generatoru na vychozi hodnoty.
     *
     * Nastavi: sample_rate = 44100, stream_type = vstream,
     * cpu_clock = 3500000 (ZX Spectrum).
     *
     * @param config Ukazatel na konfiguraci k inicializaci.
     *
     * @pre config nesmi byt NULL.
     * @post Vsechna pole konfigurace jsou nastavena na vychozi hodnoty.
     */
    extern void tzx_config_init ( st_TZX_CONFIG *config );

/** @} */


/** @defgroup tzx_conversion Konverzni funkce
 *  Prevod T-states na sekundy a vzorky.
 *  @{ */

    /**
     * @brief Prevede T-states na sekundy pri danem CPU taktu.
     *
     * @param tstates Pocet T-states.
     * @param cpu_clock CPU takt v Hz (0 = TZX_DEFAULT_CPU_CLOCK).
     * @return Doba v sekundach.
     */
    static inline double tzx_tstates_to_seconds ( uint32_t tstates, uint32_t cpu_clock ) {
        if ( cpu_clock == 0 ) cpu_clock = TZX_DEFAULT_CPU_CLOCK;
        return (double) tstates / (double) cpu_clock;
    }

    /**
     * @brief Prevede T-states na pocet vzorku pri danem sample rate a CPU taktu.
     *
     * @param tstates Pocet T-states.
     * @param sample_rate Vzorkovaci frekvence v Hz.
     * @param cpu_clock CPU takt v Hz (0 = TZX_DEFAULT_CPU_CLOCK).
     * @return Pocet vzorku (zaokrouhleno).
     */
    static inline uint32_t tzx_tstates_to_samples ( uint32_t tstates, uint32_t sample_rate, uint32_t cpu_clock ) {
        if ( cpu_clock == 0 ) cpu_clock = TZX_DEFAULT_CPU_CLOCK;
        return (uint32_t) ( ( (double) tstates * (double) sample_rate ) / (double) cpu_clock + 0.5 );
    }

/** @} */


/** @defgroup tzx_block_structs Parsovane blokove struktury
 *  Struktury pro naparsovana data z TZX bloku.
 *  Nejsou packed - pouzivaji se az po parsovani z raw dat.
 *  @{ */

    /**
     * @brief Naparsovany blok 0x10 - Standard Speed Data.
     *
     * Data jako v .TAP souborech se standardnim casovanim ROM.
     * Pilot ton: 8063 pulzu pokud flag < 128, jinak 3223.
     */
    typedef struct st_TZX_STANDARD_SPEED {
        uint16_t pause_ms;   /**< pauza po bloku (ms) */
        uint16_t data_length; /**< delka dat */
        uint8_t *data;       /**< ukazatel na data (ukazuje do raw bloku) */
    } st_TZX_STANDARD_SPEED;

    /**
     * @brief Naparsovany blok 0x11 - Turbo Speed Data.
     *
     * Data s uzivatelsky definovanym casovanim (pilot, sync, pulzy).
     * Umoznuje zachytit libovolny loader s FM-like kodovanim.
     */
    typedef struct st_TZX_TURBO_SPEED {
        uint16_t pilot_pulse;  /**< delka PILOT pulzu (T-states) */
        uint16_t sync1_pulse;  /**< delka 1. SYNC pulzu */
        uint16_t sync2_pulse;  /**< delka 2. SYNC pulzu */
        uint16_t zero_pulse;   /**< delka pulzu bitu ZERO */
        uint16_t one_pulse;    /**< delka pulzu bitu ONE */
        uint16_t pilot_count;  /**< pocet PILOT pulzu */
        uint8_t  used_bits;    /**< pouzite bity v poslednim bajtu (1-8) */
        uint16_t pause_ms;     /**< pauza po bloku (ms) */
        uint32_t data_length;  /**< delka dat (3 bajty v bloku) */
        uint8_t *data;         /**< ukazatel na data */
    } st_TZX_TURBO_SPEED;

    /**
     * @brief Naparsovany blok 0x12 - Pure Tone.
     *
     * Sekvence pulzu stejne delky (jako pilot ton).
     */
    typedef struct st_TZX_PURE_TONE {
        uint16_t pulse_length; /**< delka jednoho pulzu (T-states) */
        uint16_t pulse_count;  /**< pocet pulzu */
    } st_TZX_PURE_TONE;

    /**
     * @brief Naparsovany blok 0x13 - Pulse Sequence.
     *
     * Sekvence pulzu ruznych delek (max 255 pulzu).
     */
    typedef struct st_TZX_PULSE_SEQUENCE {
        uint8_t  pulse_count;  /**< pocet pulzu */
        uint16_t *pulses;      /**< pole delek pulzu (T-states, ukazuje do raw bloku) */
    } st_TZX_PULSE_SEQUENCE;

    /**
     * @brief Naparsovany blok 0x14 - Pure Data.
     *
     * Data bez pilot a sync pulzu, s vlastnim casovanim bitu.
     */
    typedef struct st_TZX_PURE_DATA {
        uint16_t zero_pulse;   /**< delka pulzu bitu ZERO (T-states) */
        uint16_t one_pulse;    /**< delka pulzu bitu ONE */
        uint8_t  used_bits;    /**< pouzite bity v poslednim bajtu (1-8) */
        uint16_t pause_ms;     /**< pauza po bloku (ms) */
        uint32_t data_length;  /**< delka dat (3 bajty v bloku) */
        uint8_t *data;         /**< ukazatel na data */
    } st_TZX_PURE_DATA;

    /**
     * @brief Naparsovany blok 0x15 - Direct Recording.
     *
     * Primy zaznam vzorku - kazdy bit je jedna uroven signalu (0=low, 1=high).
     * MSb se prehraje prvni.
     */
    typedef struct st_TZX_DIRECT_RECORDING {
        uint16_t tstates_per_sample; /**< pocet T-states na vzorek */
        uint16_t pause_ms;           /**< pauza po bloku (ms) */
        uint8_t  used_bits;          /**< pouzite bity v poslednim bajtu (1-8) */
        uint32_t data_length;        /**< delka dat vzorku (3 bajty v bloku) */
        uint8_t *data;               /**< ukazatel na data vzorku */
    } st_TZX_DIRECT_RECORDING;

    /**
     * @brief Naparsovany blok 0x18 - CSW Recording.
     *
     * Komprimovany zvukovy zaznam ve formatu CSW v2.
     */
    typedef struct st_TZX_CSW_RECORDING {
        uint16_t pause_ms;           /**< pauza po bloku (ms) */
        uint32_t sample_rate;        /**< vzorkovaci frekvence (3 bajty v bloku) */
        uint8_t  compression_type;   /**< typ komprese: 0x01=RLE, 0x02=Z-RLE */
        uint32_t pulse_count;        /**< pocet ulozenych pulzu (po dekompresi) */
        uint32_t data_length;        /**< delka CSW dat */
        uint8_t *data;               /**< ukazatel na CSW data */
    } st_TZX_CSW_RECORDING;

    /**
     * @brief Naparsovany blok 0x20 - Pause / Stop Tape.
     *
     * Ticho po zadany pocet milisekund. Hodnota 0 = zastaveni pasky.
     */
    typedef struct st_TZX_PAUSE {
        uint16_t pause_ms;   /**< doba pauzy (ms), 0 = stop */
    } st_TZX_PAUSE;

/** @} */


/** @defgroup tzx_parse Parsovani bloku
 *  Funkce pro parsovani raw TZX bloku na typovane struktury.
 *  Vsechny parse funkce pracuji zero-copy - data ukazuji do raw bloku.
 *  @{ */

    /**
     * @brief Rozparsuje blok 0x10 (Standard Speed Data) z raw dat.
     *
     * @param raw_data Surova data bloku (za ID bajtem).
     * @param raw_length Delka surovych dat.
     * @param[out] result Vystupni struktura.
     * @return TZX_OK pri uspechu, jinak chybovy kod.
     */
    extern en_TZX_ERROR tzx_parse_standard_speed ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_STANDARD_SPEED *result );

    /**
     * @brief Rozparsuje blok 0x11 (Turbo Speed Data) z raw dat.
     *
     * @param raw_data Surova data bloku (za ID bajtem).
     * @param raw_length Delka surovych dat.
     * @param[out] result Vystupni struktura.
     * @return TZX_OK pri uspechu, jinak chybovy kod.
     */
    extern en_TZX_ERROR tzx_parse_turbo_speed ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_TURBO_SPEED *result );

    /**
     * @brief Rozparsuje blok 0x12 (Pure Tone) z raw dat.
     *
     * @param raw_data Surova data bloku (za ID bajtem).
     * @param raw_length Delka surovych dat.
     * @param[out] result Vystupni struktura.
     * @return TZX_OK pri uspechu, jinak chybovy kod.
     */
    extern en_TZX_ERROR tzx_parse_pure_tone ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_PURE_TONE *result );

    /**
     * @brief Rozparsuje blok 0x13 (Pulse Sequence) z raw dat.
     *
     * @param raw_data Surova data bloku (za ID bajtem).
     * @param raw_length Delka surovych dat.
     * @param[out] result Vystupni struktura.
     * @return TZX_OK pri uspechu, jinak chybovy kod.
     */
    extern en_TZX_ERROR tzx_parse_pulse_sequence ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_PULSE_SEQUENCE *result );

    /**
     * @brief Rozparsuje blok 0x14 (Pure Data) z raw dat.
     *
     * @param raw_data Surova data bloku (za ID bajtem).
     * @param raw_length Delka surovych dat.
     * @param[out] result Vystupni struktura.
     * @return TZX_OK pri uspechu, jinak chybovy kod.
     */
    extern en_TZX_ERROR tzx_parse_pure_data ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_PURE_DATA *result );

    /**
     * @brief Rozparsuje blok 0x15 (Direct Recording) z raw dat.
     *
     * @param raw_data Surova data bloku (za ID bajtem).
     * @param raw_length Delka surovych dat.
     * @param[out] result Vystupni struktura.
     * @return TZX_OK pri uspechu, jinak chybovy kod.
     */
    extern en_TZX_ERROR tzx_parse_direct_recording ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_DIRECT_RECORDING *result );

    /**
     * @brief Rozparsuje blok 0x18 (CSW Recording) z raw dat.
     *
     * @param raw_data Surova data bloku (za ID bajtem).
     * @param raw_length Delka surovych dat.
     * @param[out] result Vystupni struktura.
     * @return TZX_OK pri uspechu, jinak chybovy kod.
     */
    extern en_TZX_ERROR tzx_parse_csw_recording ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_CSW_RECORDING *result );

    /**
     * @brief Rozparsuje blok 0x20 (Pause) z raw dat.
     *
     * @param raw_data Surova data bloku (za ID bajtem).
     * @param raw_length Delka surovych dat.
     * @param[out] result Vystupni struktura.
     * @return TZX_OK pri uspechu, jinak chybovy kod.
     */
    extern en_TZX_ERROR tzx_parse_pause ( const uint8_t *raw_data, uint32_t raw_length, st_TZX_PAUSE *result );

/** @} */


/** @defgroup tzx_generate Generovani audio streamu
 *  Funkce pro generovani CMT vstreamu z naparsovanych TZX bloku.
 *  Kazda funkce vytvari novy vstream, ktery volajici musi uvolnit.
 *  @{ */

    /**
     * @brief Generuje vstream z bloku 0x10 (Standard Speed Data).
     *
     * Generuje pilot ton (8063/3223 pulzu), sync pulzy a data
     * se standardnim ROM casovanim.
     *
     * @param block Naparsovany blok.
     * @param config Konfigurace generatoru.
     * @param err Vystupni chybovy kod (muze byt NULL).
     * @return Novy vstream, nebo NULL pri chybe.
     *
     * @post Volajici musi uvolnit vraceny vstream pres cmt_vstream_destroy().
     */
    extern st_CMT_VSTREAM* tzx_generate_standard_speed ( const st_TZX_STANDARD_SPEED *block, const st_TZX_CONFIG *config, en_TZX_ERROR *err );

    /**
     * @brief Generuje vstream z bloku 0x11 (Turbo Speed Data).
     *
     * Generuje pilot ton, sync pulzy a data s uzivatelsky
     * definovanym casovanim.
     *
     * @param block Naparsovany blok.
     * @param config Konfigurace generatoru.
     * @param err Vystupni chybovy kod (muze byt NULL).
     * @return Novy vstream, nebo NULL pri chybe.
     *
     * @post Volajici musi uvolnit vraceny vstream pres cmt_vstream_destroy().
     */
    extern st_CMT_VSTREAM* tzx_generate_turbo_speed ( const st_TZX_TURBO_SPEED *block, const st_TZX_CONFIG *config, en_TZX_ERROR *err );

    /**
     * @brief Generuje vstream z bloku 0x12 (Pure Tone).
     *
     * @param block Naparsovany blok.
     * @param config Konfigurace generatoru.
     * @param err Vystupni chybovy kod (muze byt NULL).
     * @return Novy vstream, nebo NULL pri chybe.
     */
    extern st_CMT_VSTREAM* tzx_generate_pure_tone ( const st_TZX_PURE_TONE *block, const st_TZX_CONFIG *config, en_TZX_ERROR *err );

    /**
     * @brief Generuje vstream z bloku 0x13 (Pulse Sequence).
     *
     * @param block Naparsovany blok.
     * @param config Konfigurace generatoru.
     * @param err Vystupni chybovy kod (muze byt NULL).
     * @return Novy vstream, nebo NULL pri chybe.
     */
    extern st_CMT_VSTREAM* tzx_generate_pulse_sequence ( const st_TZX_PULSE_SEQUENCE *block, const st_TZX_CONFIG *config, en_TZX_ERROR *err );

    /**
     * @brief Generuje vstream z bloku 0x14 (Pure Data).
     *
     * @param block Naparsovany blok.
     * @param config Konfigurace generatoru.
     * @param err Vystupni chybovy kod (muze byt NULL).
     * @return Novy vstream, nebo NULL pri chybe.
     */
    extern st_CMT_VSTREAM* tzx_generate_pure_data ( const st_TZX_PURE_DATA *block, const st_TZX_CONFIG *config, en_TZX_ERROR *err );

    /**
     * @brief Generuje vstream z bloku 0x15 (Direct Recording).
     *
     * Kazdy bit vstupnich dat je jeden vzorek signalu (0=low, 1=high).
     * MSb se prehraje prvni.
     *
     * @param block Naparsovany blok.
     * @param config Konfigurace generatoru.
     * @param err Vystupni chybovy kod (muze byt NULL).
     * @return Novy vstream, nebo NULL pri chybe.
     */
    extern st_CMT_VSTREAM* tzx_generate_direct_recording ( const st_TZX_DIRECT_RECORDING *block, const st_TZX_CONFIG *config, en_TZX_ERROR *err );

    /**
     * @brief Generuje vstream pauzy (ticha).
     *
     * @param pause_ms Doba pauzy v milisekundach.
     * @param config Konfigurace generatoru.
     * @param err Vystupni chybovy kod (muze byt NULL).
     * @return Novy vstream, nebo NULL pri chybe.
     */
    extern st_CMT_VSTREAM* tzx_generate_pause ( uint16_t pause_ms, const st_TZX_CONFIG *config, en_TZX_ERROR *err );

/** @} */


/** @defgroup tzx_error Chybove hlaseni
 *  @{ */

    /**
     * @brief Vrati textovy popis chyboveho kodu en_TZX_ERROR.
     *
     * @param err Chybovy kod.
     * @return Ukazatel na staticky retezec s popisem chyby.
     */
    extern const char* tzx_error_string ( en_TZX_ERROR err );

/** @} */


/** @defgroup tzx_error_cb Error callback
 *  Umoznuje presmerovat chybova hlaseni knihovny do vlastniho handleru.
 *  @{ */

    /**
     * @brief Typ callback funkce pro hlaseni chyb.
     *
     * @param func Jmeno funkce, ve ktere doslo k chybe.
     * @param line Cislo radku ve zdrojovem kodu.
     * @param fmt  Formatovaci retezec (printf-style).
     */
    typedef void (*tzx_error_cb)(const char *func, int line, const char *fmt, ...);

    /**
     * @brief Nastavi vlastni error callback pro knihovnu TZX.
     *
     * Callback je volan pri chybach uvnitr knihovny misto vychoziho
     * vypisu na stderr. Predani NULL resetuje na vychozi.
     *
     * @param cb Ukazatel na callback funkci, nebo NULL pro reset na vychozi.
     */
    extern void tzx_set_error_callback ( tzx_error_cb cb );

/** @} */


/** @defgroup tzx_allocator Uzivatelsky alokator
 *  Umoznuje nahradit vychozi malloc/calloc/free vlastnimi funkcemi.
 *  @{ */

    /**
     * @brief Uzivatelsky alokator pameti pro knihovnu TZX.
     *
     * Pokud neni nastaven vlastni alokator, knihovna pouziva
     * standardni systemove funkce malloc, calloc a free.
     */
    typedef struct st_TZX_ALLOCATOR {
        void* (*alloc)(size_t size);   /**< alokace pameti (jako malloc) */
        void* (*alloc0)(size_t size);  /**< alokace s nulovanim (jako calloc) */
        void  (*free)(void *ptr);      /**< uvolneni pameti (jako free) */
    } st_TZX_ALLOCATOR;

    /**
     * @brief Nastavi vlastni alokator pro knihovnu TZX.
     *
     * Predany alokator se pouzije pro vsechny nasledujici alokace
     * v ramci knihovny. Predani NULL resetuje na vychozi systemove funkce.
     *
     * @param allocator Ukazatel na strukturu alokatoru, nebo NULL pro reset na vychozi.
     */
    extern void tzx_set_allocator ( const st_TZX_ALLOCATOR *allocator );

/** @} */


/** @defgroup tzx_file_io Souborove I/O API
 *  Nacitani, ukladani a uvolnovani TZX/TMZ souboru.
 *  @{ */

    /**
     * @brief Nacte cely TZX/TMZ soubor (hlavicku + vsechny bloky) z handleru.
     *
     * Alokuje a vraci novou st_TZX_FILE strukturu na heapu.
     * Rozpozna signaturu "ZXTape!" i "TapeMZ!" a nastavi priznak is_tmz.
     * Volajici musi uvolnit pres tzx_free().
     *
     * @param h   Otevreny handler s TZX/TMZ daty
     * @param err Vystupni chybovy kod (muze byt NULL)
     * @return Ukazatel na st_TZX_FILE, nebo NULL pri chybe
     *
     * @pre h != NULL a handler musi byt otevreny (HANDLER_STATUS_READY)
     * @post Pri uspechu vraci platnou strukturu, *err == TZX_OK
     * @post Pri chybe vraci NULL, *err obsahuje konkretni chybovy kod
     */
    extern st_TZX_FILE* tzx_load ( st_HANDLER *h, en_TZX_ERROR *err );

    /**
     * @brief Zapise cely TZX/TMZ soubor (hlavicku + vsechny bloky) do handleru.
     *
     * Zapisuje bloky sekvencne v poradi, v jakem jsou ulozeny v poli blocks.
     * Signatura v hlavicce se zapisuje tak, jak je nastavena ve strukture.
     *
     * @param h    Otevreny handler pro zapis
     * @param file TZX data k ulozeni (nesmi byt NULL)
     * @return TZX_OK pri uspechu, jinak chybovy kod
     *
     * @pre h != NULL, file != NULL
     * @pre Handler musi byt otevreny pro zapis (ne READ_ONLY)
     */
    extern en_TZX_ERROR tzx_save ( st_HANDLER *h, const st_TZX_FILE *file );

    /**
     * @brief Uvolni st_TZX_FILE strukturu vcetne vsech bloku a jejich dat.
     *
     * Bezpecne volat s NULL (no-op). Uvolni vsechna data vlastnena
     * jednotlivymi bloky a nasledne pole bloku a samotnou strukturu.
     *
     * @param file Ukazatel na strukturu k uvolneni (muze byt NULL)
     */
    extern void tzx_free ( st_TZX_FILE *file );

/** @} */


/** @defgroup tzx_header_ops Operace s hlavickou
 *  Cteni, zapis a validace hlavicky TZX/TMZ souboru.
 *  @{ */

    /**
     * @brief Nacte TZX/TMZ hlavicku z handleru.
     *
     * Cte 10 bajtu od aktualni pozice (offset 0) a naplni strukturu hlavicky.
     * Validuje signaturu ("ZXTape!" nebo "TapeMZ!") a EOF marker.
     *
     * @param h      Otevreny handler s TZX/TMZ daty
     * @param header Vystupni buffer pro hlavicku
     * @return TZX_OK pri uspechu, TZX_ERROR_IO pri chybe cteni
     *
     * @pre h != NULL, header != NULL
     */
    extern en_TZX_ERROR tzx_read_header ( st_HANDLER *h, st_TZX_HEADER *header );

    /**
     * @brief Zapise TZX/TMZ hlavicku do handleru.
     *
     * Zapisuje 10 bajtu od aktualni pozice (offset 0).
     *
     * @param h      Otevreny handler pro zapis
     * @param header Hlavicka k zapisu
     * @return TZX_OK pri uspechu, TZX_ERROR_IO pri chybe zapisu
     *
     * @pre h != NULL, header != NULL
     * @pre Handler musi byt otevreny pro zapis
     */
    extern en_TZX_ERROR tzx_write_header ( st_HANDLER *h, const st_TZX_HEADER *header );

    /**
     * @brief Overi, zda hlavicka obsahuje TZX signaturu ("ZXTape!").
     *
     * @param header Ukazatel na hlavicku k overeni
     * @return true pokud signatura odpovida "ZXTape!", false jinak
     *
     * @pre header != NULL
     */
    extern bool tzx_header_is_tzx ( const st_TZX_HEADER *header );

    /**
     * @brief Inicializuje hlavicku vychozimi hodnotami TZX formatu.
     *
     * Nastavi signaturu na "ZXTape!", EOF marker na 0x1A
     * a verzi na TZX_VERSION_MAJOR.TZX_VERSION_MINOR (1.20).
     *
     * @param header Ukazatel na hlavicku k inicializaci
     *
     * @pre header != NULL
     * @post header obsahuje platnou TZX hlavicku
     */
    extern void tzx_header_init ( st_TZX_HEADER *header );

/** @} */


/** @defgroup tzx_block_names Nazvy bloku
 *  @{ */

    /**
     * @brief Vrati textovy nazev TZX bloku pro dany identifikator.
     *
     * Vraci lidsky citelny nazev bloku (napr. "Standard Speed Data").
     * Pro nezname ID vraci "Unknown". Vzdy vraci platny retezec (nikdy NULL).
     *
     * @param id Identifikator bloku (viz TZX_BLOCK_ID_*)
     * @return Ukazatel na staticky retezec s nazvem bloku
     */
    extern const char* tzx_block_id_name ( uint8_t id );

/** @} */


/** @defgroup tzx_file_manipulation Manipulace s bloky v souboru
 *  Funkce pro pridavani, odebirani a presun bloku v st_TZX_FILE.
 *  @{ */

    /**
     * @brief Odebere blok na danem indexu z pole bloku.
     *
     * Uvolni data bloku a posune zbyle bloky doleva.
     * Dekrementuje block_count.
     *
     * @param file TZX soubor.
     * @param index Index bloku k odebrani (0-based).
     * @return TZX_OK pri uspechu, TZX_ERROR_NULL_INPUT nebo TZX_ERROR_INVALID_BLOCK.
     *
     * @pre file != NULL, index < file->block_count
     * @post block_count je o 1 mensi, data odebranehp bloku jsou uvolnena
     */
    extern en_TZX_ERROR tzx_file_remove_block ( st_TZX_FILE *file, uint32_t index );

    /**
     * @brief Vlozi blok na dany index (posune ostatni doprava).
     *
     * Prebira ownership dat bloku - volajici nesmi dale uvolnovat block->data.
     * Pri nedostatku kapacity pole reallokuje na dvojnasobek.
     *
     * @param file TZX soubor.
     * @param index Pozice pro vlozeni (0 = na zacatek, block_count = na konec).
     * @param block Blok k vlozeni (obsah se zkopiruje, ownership dat predan).
     * @return TZX_OK pri uspechu, chybovy kod jinak.
     *
     * @pre file != NULL, block != NULL, index <= file->block_count
     * @post block_count je o 1 vetsi
     */
    extern en_TZX_ERROR tzx_file_insert_block ( st_TZX_FILE *file, uint32_t index, const st_TZX_BLOCK *block );

    /**
     * @brief Prida blok na konec pole bloku.
     *
     * Ekvivalent tzx_file_insert_block(file, file->block_count, block).
     *
     * @param file TZX soubor.
     * @param block Blok k pridani (ownership dat predan).
     * @return TZX_OK pri uspechu, chybovy kod jinak.
     *
     * @pre file != NULL, block != NULL
     */
    extern en_TZX_ERROR tzx_file_append_block ( st_TZX_FILE *file, const st_TZX_BLOCK *block );

    /**
     * @brief Presune blok z pozice from na pozici to.
     *
     * Blok se odebere z from a vlozi na to. Indexy se chovaji
     * jako: nejdrive remove(from), pak insert(to).
     *
     * @param file TZX soubor.
     * @param from Zdrojova pozice (0-based).
     * @param to Cilova pozice (0-based, po odebrani).
     * @return TZX_OK pri uspechu, chybovy kod jinak.
     *
     * @pre file != NULL, from < block_count, to <= block_count-1
     */
    extern en_TZX_ERROR tzx_file_move_block ( st_TZX_FILE *file, uint32_t from, uint32_t to );

    /**
     * @brief Spoji bloky z src za bloky v dst.
     *
     * Vsechny bloky ze src se pripoji na konec dst.
     * Src je po operaci prazdny (block_count == 0, blocks == NULL).
     * Ownership dat je preveden na dst.
     *
     * @param dst Cilovy soubor (bloky se pridaji na konec).
     * @param src Zdrojovy soubor (bude vyprazdnen).
     * @return TZX_OK pri uspechu, chybovy kod jinak.
     *
     * @pre dst != NULL, src != NULL
     * @post src->block_count == 0, src->blocks == NULL
     */
    extern en_TZX_ERROR tzx_file_merge ( st_TZX_FILE *dst, st_TZX_FILE *src );

/** @} */


/** @defgroup tzx_block_create Vytvareni TZX info bloku
 *  Funkce pro vytvareni standardnich TZX informacnich bloku.
 *  @{ */

    /**
     * @brief Zaznam Archive Info bloku (0x32).
     *
     * Pouziva se jako vstup pro tzx_block_create_archive_info().
     * Typ zaznamu urcuje vyznam textu (nazev, autor, rok, ...).
     */
    typedef struct st_TZX_ARCHIVE_ENTRY {
        uint8_t type_id;     /**< typ zaznamu (0x00=Title, 0x01=Publisher, 0x02=Author, 0x03=Year, 0xFF=Comment) */
        const char *text;    /**< ASCII text zaznamu (nesmi byt NULL) */
    } st_TZX_ARCHIVE_ENTRY;

    /**
     * @brief Vytvori blok 0x30 (Text Description).
     *
     * Alokuje data bloku: [1B delka textu][text].
     *
     * @param text Textovy retezec (max 255 znaku).
     * @param[out] block Vystupni blok (id a data budou naplneny).
     * @return TZX_OK pri uspechu, chybovy kod jinak.
     *
     * @pre text != NULL, block != NULL
     * @post block->data je vlastneno volajicim (uvolnit pres free)
     */
    extern en_TZX_ERROR tzx_block_create_text_description ( const char *text, st_TZX_BLOCK *block );

    /**
     * @brief Vytvori blok 0x31 (Message Block).
     *
     * Alokuje data bloku: [1B cas][1B delka textu][text].
     *
     * @param text Textovy retezec (max 255 znaku).
     * @param time_seconds Cas zobrazeni zpravy v sekundach.
     * @param[out] block Vystupni blok (id a data budou naplneny).
     * @return TZX_OK pri uspechu, chybovy kod jinak.
     *
     * @pre text != NULL, block != NULL
     * @post block->data je vlastneno volajicim (uvolnit pres free)
     */
    extern en_TZX_ERROR tzx_block_create_message ( const char *text, uint8_t time_seconds, st_TZX_BLOCK *block );

    /**
     * @brief Vytvori blok 0x32 (Archive Info).
     *
     * Alokuje data bloku: [2B celkova delka][1B pocet][N * (1B typ, 1B delka, text)].
     *
     * @param entries Pole zaznamu.
     * @param count Pocet zaznamu.
     * @param[out] block Vystupni blok (id a data budou naplneny).
     * @return TZX_OK pri uspechu, chybovy kod jinak.
     *
     * @pre entries != NULL, count > 0, block != NULL
     * @post block->data je vlastneno volajicim (uvolnit pres free)
     */
    extern en_TZX_ERROR tzx_block_create_archive_info ( const st_TZX_ARCHIVE_ENTRY *entries, uint8_t count, st_TZX_BLOCK *block );

    /**
     * @brief Vytvori blok 0x15 (Direct Recording).
     *
     * Serializuje parametry Direct Recording do binarniho formatu TZX bloku:
     * [2B tstates_per_sample LE][2B pause_ms LE][1B used_bits][3B data_length LE][data].
     * Data jsou zkopirovana - volajici muze uvolnit sve.
     *
     * @param tstates_per_sample Pocet T-states na jeden vzorek.
     * @param pause_ms Pauza po bloku v milisekundach.
     * @param used_bits Pouzite bity v poslednim bajtu (1-8).
     * @param data Bitova data zaznamu. Nesmi byt NULL pokud data_length > 0.
     * @param data_length Delka dat v bajtech.
     * @param[out] block Vystupni blok (id a data budou naplneny).
     * @return TZX_OK pri uspechu, chybovy kod jinak.
     *
     * @pre block != NULL
     * @post block->data je vlastneno volajicim (uvolnit pres free)
     */
    extern en_TZX_ERROR tzx_block_create_direct_recording (
        uint16_t tstates_per_sample,
        uint16_t pause_ms,
        uint8_t used_bits,
        const uint8_t *data,
        uint32_t data_length,
        st_TZX_BLOCK *block
    );

/** @} */


#ifdef __cplusplus
}
#endif

#endif /* TZX_H */
