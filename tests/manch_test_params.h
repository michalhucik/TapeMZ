/**
 * @file   manch_test_params.h
 * @brief  Sdílené parametry rodiny MANCHESTER pro testy: Pezik/MarVan
 *         TAPE.COM V2.0a a Intercopy V10.2 "CPM 2400" podle
 *         výpočtu z kódu TAPE.COM a měření Intercopy. Nezávislé na registru
 *         profilů (tmz_profile), aby testy profilů měly vlastní oráculum.
 *
 * Hlavička obsahuje statické funkce; includuje ji vždy jen jeden testovací
 * soubor programu.
 */

#ifndef MANCH_TEST_PARAMS_H
#define MANCH_TEST_PARAMS_H

#include <stdint.h>

#include "libs/mzcmt_manchester/mzcmt_manchester.h"


/**
 * @brief Přidá položku kontextu.
 * @param p Parametry. @param ctx Kontext. @param bit Bit (0, 1, MZCMT_MANCH_BIT_ANY). @param half2 Druhá půlka.
 */
static void ctx_add ( st_MZCMT_MANCH_PARAMS *p, uint8_t ctx, uint8_t bit, uint32_t half2 ) {
    p->context[p->context_count].ctx = ctx;
    p->context[p->context_count].bit = bit;
    p->context[p->context_count].half2 = half2;
    p->context_count++;
}


/**
 * @brief Kontext Pezik s druhou půlkou 13N + c pro bit 0 a 13N + c - 1 pro bit 1.
 * @param p Parametry. @param ctx Kontext. @param n N. @param c Konstanta pro bit 0.
 */
static void pz_ctx ( st_MZCMT_MANCH_PARAMS *p, uint8_t ctx, uint32_t n, uint32_t c ) {
    ctx_add ( p, ctx, 0, 13 * n + c );
    ctx_add ( p, ctx, 1, 13 * n + c - 1 );
}


/**
 * @brief Parametry Pezik TAPE.COM V2.0a (výpočet z T-stavů programu).
 * @param p Výstup. @param n N (43 = 2400, 28 = 3200). @param header 1 = hlavička, 0 = tělo.
 */
static void pezik ( st_MZCMT_MANCH_PARAMS *p, uint32_t n, int header ) {
    mzcmt_manch_params_init ( p );
    p->half1[0] = 13 * n + 195;
    p->half1[1] = 13 * n + 196;
    p->half2[0] = 13 * n + 192;
    p->half2[1] = 13 * n + 191;
    p->pilot_count = header ? 2134 : 1067;
    p->pilot_bits = 3;
    p->pilot_pattern = 0x06;
    p->sync_bits = 1;
    p->sync_pattern = 0x01;
    p->has_flag = 1;
    p->flag = header ? 0x00 : 0x01;
    p->checksum = MZCMT_MANCH_CHECKSUM_SUM16_LE;
    p->carry_adjust = -1;
    pz_ctx ( p, MZCMT_MANCH_CTX_PILOT_IN, n, 172 );
    pz_ctx ( p, MZCMT_MANCH_CTX_PILOT_NEXT, n, 189 );
    pz_ctx ( p, MZCMT_MANCH_CTX_PILOT_END, n, 180 );
    pz_ctx ( p, MZCMT_MANCH_CTX_SYNC_END, n, 206 );
    pz_ctx ( p, MZCMT_MANCH_CTX_FLAG_END, n, header ? 231 : 228 );
    pz_ctx ( p, MZCMT_MANCH_CTX_DATA_FIRST, n, 186 );
    pz_ctx ( p, MZCMT_MANCH_CTX_DATA_BYTE, n, 184 );
    pz_ctx ( p, MZCMT_MANCH_CTX_DATA_END, n, 201 );
    pz_ctx ( p, MZCMT_MANCH_CTX_DATA_SINGLE_END, n, 213 );
    pz_ctx ( p, MZCMT_MANCH_CTX_CK_MID, n, 202 );
    if ( header ) pz_ctx ( p, MZCMT_MANCH_CTX_CK_END, n, 314 );
}


/**
 * @brief Parametry Intercopy V10.2 "CPM 2400" (změřeno v emulátoru).
 * @param p Výstup. @param header 1 = hlavička (pauza 1 221 688 T), 0 = tělo.
 */
static void intercopy ( st_MZCMT_MANCH_PARAMS *p, int header ) {
    mzcmt_manch_params_init ( p );
    p->half1[0] = p->half1[1] = p->half2[0] = p->half2[1] = 741;
    p->pilot_count = header ? 2048 : 1024;
    p->pilot_bits = 3;
    p->pilot_pattern = 0x06;
    p->sync_bits = 1;
    p->sync_pattern = 0x01;
    p->has_flag = 1;
    p->flag = header ? 0x00 : 0x01;
    p->checksum = MZCMT_MANCH_CHECKSUM_SUM16_LE;
    p->end_bits = 1;
    p->end_pattern = 0x01;
    p->pause = header ? 1221688 : 0;
    ctx_add ( p, MZCMT_MANCH_CTX_PILOT_END, MZCMT_MANCH_BIT_ANY, 740 );
    if ( !header ) ctx_add ( p, MZCMT_MANCH_CTX_SYNC_END, MZCMT_MANCH_BIT_ANY, 740 );
    ctx_add ( p, MZCMT_MANCH_CTX_FLAG_END, MZCMT_MANCH_BIT_ANY, 740 );
    ctx_add ( p, MZCMT_MANCH_CTX_DATA_END, MZCMT_MANCH_BIT_ANY, 739 );
    ctx_add ( p, MZCMT_MANCH_CTX_DATA_SINGLE_END, MZCMT_MANCH_BIT_ANY, 739 );
    ctx_add ( p, MZCMT_MANCH_CTX_CK_MID, MZCMT_MANCH_BIT_ANY, 744 );
    ctx_add ( p, MZCMT_MANCH_CTX_CK_END, MZCMT_MANCH_BIT_ANY, 745 );
}


#endif /* MANCH_TEST_PARAMS_H */
