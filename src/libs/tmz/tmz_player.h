/**
 * @file   tmz_player.h
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.0.0
 * @brief  Verejne API TMZ playeru - generovani CMT audio streamu z TMZ/TZX bloku.
 *
 * Player iteruje bloky TMZ/TZX souboru a generuje CMT audio stream
 * pomoci knihoven mztape (Sharp MZ bloky) a tzx (standardni TZX bloky).
 * Vystupem je cmt_stream (bitstream nebo vstream), ktery lze exportovat
 * do WAV nebo pridat do emulatoru jako vstup kazetove jednotky.
 *
 * Player ma dve urovne API:
 * - Per-block: tmz_player_play_block() pro prehrani jednoho bloku
 * - Stavovy automat: tmz_player_state_init() + tmz_player_play_next()
 *   pro sekvencni prehravani celeho souboru vcetne ridicich bloku
 *   (Loop Start/End, Jump, Call Sequence/Return, Set Signal Level).
 *
 * Oba typy hlavicek (TapeMZ! i ZXTape!) jsou podporovany.
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


#ifndef TMZ_PLAYER_H
#define TMZ_PLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "tmz.h"
#include "tmz_blocks.h"
#include "../cmt_stream/cmt_stream.h"
#include "../mztape/mztape.h"
#include "../zxtape/zxtape.h"


    /** @brief Chybove kody TMZ playeru. */
    typedef enum en_TMZ_PLAYER_ERROR {
        TMZ_PLAYER_OK = 0,              /**< operace uspesna */
        TMZ_PLAYER_ERROR_NULL_INPUT,     /**< vstupni parametr je NULL */
        TMZ_PLAYER_ERROR_NO_BLOCKS,      /**< soubor neobsahuje zadne bloky */
        TMZ_PLAYER_ERROR_UNSUPPORTED,    /**< nepodporovany typ bloku */
        TMZ_PLAYER_ERROR_STREAM_CREATE,  /**< chyba pri vytvareni CMT streamu */
        TMZ_PLAYER_ERROR_ALLOC,          /**< selhani alokace pameti */
    } en_TMZ_PLAYER_ERROR;


    /** @brief Konfigurace playeru. */
    typedef struct st_TMZ_PLAYER_CONFIG {
        uint32_t sample_rate;            /**< vzorkovaci frekvence (Hz), 0 = CMTSTREAM_DEFAULT_RATE */
        en_CMT_STREAM_TYPE stream_type;  /**< typ vystupniho streamu (bitstream/vstream) */
        en_MZTAPE_PULSESET default_pulseset; /**< vychozi pulsni sada (pokud neni v bloku) */
        en_CMTSPEED default_speed;       /**< vychozi rychlost (pokud neni v bloku) */
        en_MZTAPE_FORMATSET default_formatset; /**< vychozi format zaznamu */
    } st_TMZ_PLAYER_CONFIG;


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
    extern void tmz_player_config_init ( st_TMZ_PLAYER_CONFIG *config );


    /**
     * @brief Prehaje jeden MZ Standard Data blok (0x40) na CMT stream.
     *
     * Vytvori st_MZTAPE_MZF z dat bloku a pouzije mztape knihovnu
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
    extern st_CMT_STREAM* tmz_player_play_mz_standard ( const st_TZX_BLOCK *block, const st_TMZ_PLAYER_CONFIG *config, en_TMZ_PLAYER_ERROR *err );


    /**
     * @brief Prehaje jeden MZ Turbo Data blok (0x41) na CMT stream.
     *
     * Dispatcher - podle pole format v bloku zvoli prislusny koder:
     * NORMAL (mztape), TURBO (mzcmt_turbo), FASTIPL (mzcmt_fastipl),
     * FSK (mzcmt_fsk), SLOW (mzcmt_slow), DIRECT (mzcmt_direct),
     * CPM-TAPE (mzcmt_cpmtape).
     *
     * @param block TMZ blok s ID 0x41.
     * @param config Konfigurace playeru.
     * @param err Vystupni chybovy kod (muze byt NULL).
     * @return Novy CMT stream, nebo NULL pri chybe.
     *
     * @pre block nesmi byt NULL a musi mit id == TMZ_BLOCK_ID_MZ_TURBO_DATA.
     * @post Volajici je vlastnikem vraceneho streamu a musi ho uvolnit pres cmt_stream_destroy().
     */
    extern st_CMT_STREAM* tmz_player_play_mz_turbo ( const st_TZX_BLOCK *block, const st_TMZ_PLAYER_CONFIG *config, en_TMZ_PLAYER_ERROR *err );


    /**
     * @brief Prehaje jeden MZ BASIC Data blok (0x45) na CMT stream.
     *
     * BSD/BRD chunkovany format - pouzije mzcmt_bsd koder.
     * Generuje kompletni signal vcetne hlavicky, tapemarkeru
     * a jednotlivych chunku jako body bloku.
     *
     * @param block TMZ blok s ID 0x45.
     * @param config Konfigurace playeru.
     * @param err Vystupni chybovy kod (muze byt NULL).
     * @return Novy CMT stream, nebo NULL pri chybe.
     *
     * @pre block nesmi byt NULL a musi mit id == TMZ_BLOCK_ID_MZ_BASIC_DATA.
     * @post Volajici je vlastnikem vraceneho streamu a musi ho uvolnit pres cmt_stream_destroy().
     */
    extern st_CMT_STREAM* tmz_player_play_mz_basic ( const st_TZX_BLOCK *block, const st_TMZ_PLAYER_CONFIG *config, en_TMZ_PLAYER_ERROR *err );


    /**
     * @brief Prehaje libovolny TZX audio blok na CMT stream.
     *
     * Deleguje parsovani a generovani na TZX knihovnu (tzx.h).
     * Podporuje bloky: 0x10 (Standard Speed), 0x11 (Turbo Speed),
     * 0x12 (Pure Tone), 0x13 (Pulse Sequence), 0x14 (Pure Data),
     * 0x15 (Direct Recording), 0x20 (Pause).
     *
     * @param block TMZ blok s TZX audio ID.
     * @param config Konfigurace playeru.
     * @param err Vystupni chybovy kod (muze byt NULL).
     * @return Novy CMT stream, nebo NULL pri chybe / ridici blok.
     *
     * @pre block nesmi byt NULL.
     * @post Volajici je vlastnikem vraceneho streamu a musi ho uvolnit pres cmt_stream_destroy().
     */
    extern st_CMT_STREAM* tmz_player_play_tzx_block ( const st_TZX_BLOCK *block, const st_TMZ_PLAYER_CONFIG *config, en_TMZ_PLAYER_ERROR *err );


    /**
     * @brief Prehaje jeden libovolny blok na CMT stream.
     *
     * Dispatcher - podle ID bloku zavola prislusnou specializovanou funkci.
     * Ridici bloky (pause, group, loop) jsou zpracovany, ale nevytvareji stream.
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
    extern st_CMT_STREAM* tmz_player_play_block ( const st_TZX_BLOCK *block, const st_TMZ_PLAYER_CONFIG *config, en_TMZ_PLAYER_ERROR *err );


    /**
     * @brief Vrati textovy popis chyboveho kodu TMZ playeru.
     *
     * Vzdy vraci platny retezec (nikdy NULL).
     *
     * @param err Chybovy kod.
     * @return Textovy popis chyby.
     */
    extern const char* tmz_player_error_string ( en_TMZ_PLAYER_ERROR err );


/* ========================================================================= */
/*  Stavovy automat pro sekvencni prehravani celeho souboru                  */
/* ========================================================================= */

/** @defgroup tmz_player_state Stavovy automat playeru
 *  Sekvencni prehravani s podporou ridicich bloku (loop, jump, call).
 *  @{ */

/** @brief Maximalni hloubka vnoreni smycek (Loop Start/End). */
#define TMZ_PLAYER_MAX_LOOP_DEPTH   8

/** @brief Maximalni hloubka vnoreni volani (Call Sequence/Return). */
#define TMZ_PLAYER_MAX_CALL_DEPTH   8


    /**
     * @brief Zaznam na zasobniku smycek.
     *
     * Pri Loop Start se ulozi index bloku za Loop Start a pocet opakovani.
     * Pri Loop End se dekrementuje remaining; pokud > 0, skok na start_block.
     */
    typedef struct st_TMZ_PLAYER_LOOP_ENTRY {
        uint32_t start_block; /**< index bloku hned za Loop Start */
        uint32_t remaining;   /**< zbyvajici opakovani (0 = hotovo) */
    } st_TMZ_PLAYER_LOOP_ENTRY;


    /**
     * @brief Zaznam na zasobniku volani.
     *
     * Pri Call Sequence se ulozi index Call bloku, celkovy pocet offsetu
     * a aktualni pozice v poli offsetu. Offsety se ctou primo z bloku.
     * Pri Return se posune na dalsi offset, nebo pop pokud vsechny zpracovany.
     */
    typedef struct st_TMZ_PLAYER_CALL_ENTRY {
        uint32_t call_block;         /**< index Call Sequence bloku */
        uint16_t current_offset_idx; /**< aktualni index v poli offsetu (0-based) */
        uint16_t total_offsets;      /**< celkovy pocet offsetu v Call Sequence */
    } st_TMZ_PLAYER_CALL_ENTRY;


    /**
     * @brief Stav playeru pro sekvencni prehravani.
     *
     * Obsahuje ukazatel na soubor, konfiguraci, aktualni pozici
     * a zasobniky pro smycky (loop) a volani (call).
     *
     * @par Invarianty:
     * - current_block < file->block_count dokud !finished
     * - loop_depth >= 0 && <= TMZ_PLAYER_MAX_LOOP_DEPTH
     * - call_depth >= 0 && <= TMZ_PLAYER_MAX_CALL_DEPTH
     *
     * @par Vlastnictvi:
     * State nevlastni soubor ani konfiguraci - volajici je zodpovedny
     * za jejich zivotnost po celou dobu prehravani.
     */
    typedef struct st_TMZ_PLAYER_STATE {
        const st_TZX_FILE *file;         /**< soubor k prehrani (nevlastni) */
        st_TMZ_PLAYER_CONFIG config;     /**< konfigurace playeru (kopie) */
        uint32_t current_block;          /**< aktualni pozice v blokove sekvenci */
        uint32_t last_played_block;      /**< index naposledy prehravaneho bloku */

        /** @brief Zasobnik smycek (Loop Start/End). */
        st_TMZ_PLAYER_LOOP_ENTRY loop_stack[TMZ_PLAYER_MAX_LOOP_DEPTH];
        int loop_depth;                  /**< aktualni hloubka vnoreni smycek */

        /** @brief Zasobnik volani (Call Sequence/Return). */
        st_TMZ_PLAYER_CALL_ENTRY call_stack[TMZ_PLAYER_MAX_CALL_DEPTH];
        int call_depth;                  /**< aktualni hloubka vnoreni volani */

        uint8_t signal_level;            /**< aktualni uroven signalu (0=LOW, 1=HIGH) */
        bool finished;                   /**< prehravani dokonceno */
    } st_TMZ_PLAYER_STATE;


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
    extern void tmz_player_state_init ( st_TMZ_PLAYER_STATE *state,
                                        const st_TZX_FILE *file,
                                        const st_TMZ_PLAYER_CONFIG *config );


    /**
     * @brief Prehaje dalsi audio blok v sekvenci.
     *
     * Interně zpracovava ridici bloky (Loop Start/End, Jump,
     * Call Sequence/Return, Set Signal Level, Group Start/End)
     * a vraci CMT stream az pri narazeni na audio blok.
     *
     * Pokud je prehravani dokonceno (vsechny bloky zpracovany),
     * vraci NULL s TMZ_PLAYER_OK a nastavi state->finished = true.
     *
     * Index prave prehravaneho bloku je dostupny pres
     * state->last_played_block po navratu.
     *
     * @param state Stav playeru.
     * @param err   Vystupni chybovy kod (muze byt NULL).
     * @return Novy CMT stream, nebo NULL pokud konec / chyba / neni audio.
     *
     * @pre state nesmi byt NULL a musi byt inicializovany.
     * @post Volajici je vlastnikem vraceneho streamu a musi ho uvolnit
     *       pres cmt_stream_destroy().
     */
    extern st_CMT_STREAM* tmz_player_play_next ( st_TMZ_PLAYER_STATE *state,
                                                  en_TMZ_PLAYER_ERROR *err );


    /**
     * @brief Zjisti, zda je prehravani dokonceno.
     *
     * @param state Stav playeru.
     * @return true pokud vsechny bloky byly zpracovany, false jinak.
     */
    extern bool tmz_player_state_finished ( const st_TMZ_PLAYER_STATE *state );

/** @} */


#ifdef __cplusplus
}
#endif

#endif /* TMZ_PLAYER_H */
