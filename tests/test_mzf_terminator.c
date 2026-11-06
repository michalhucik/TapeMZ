/**
 * @file test_mzf_terminator.c
 * @brief Test: bajt 11h hlavičky MZF (fname.terminator) zůstává beze změny.
 *
 * Ověřuje mzf_header_swap_endianity() (jen pořadí fsize/fstrt/fexec,
 * symetrie, 11h beze změny) a cestu hlavičky blokem v1.0: zápis a parser
 * bloků 0x40, 0x41, 0x44, 0x45 a převod tmz_v1conv na 0x46 s bajtem 11h = 00h
 * (ROM SAVE zapíše 11h tak, jak je v paměti). mzf_header_items_correction()
 * dál nastavuje 0Dh (zpětná kompatibilita).
 *
 * Nečte tstdata, běží i ve veřejné kopii.
 *
 * @par Licence:
 * GNU General Public License v3 (GPLv3)
 *
 * Copyright (C) 2017-2026 Michal Hucik <hucik@ordoz.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/endianity/endianity.h"
#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_mzfile.h"
#include "libs/tmz/tmz_v1conv.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vypíše OK/FAIL a započítá selhání. */
#define CHECK( cond, msg ) do { \
        if ( cond ) { printf ( "OK: %s\n", msg ); } \
        else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
    } while ( 0 )


/**
 * @brief Hlavička v pořadí hostitele: jméno "TERM", 11h = term, fsize 16, 1200h.
 * @param h Výstup. @param term Bajt 11h.
 */
static void make_header ( st_MZF_HEADER *h, uint8_t term ) {
    memset ( h, 0, sizeof ( *h ) );
    h->ftype = 0x01;
    memset ( &h->fname, 0x0D, sizeof ( h->fname ) );
    memcpy ( h->fname.name, "TERM", 4 );
    h->fname.terminator = term;
    h->fsize = 16;
    h->fstrt = 0x1200;
    h->fexec = 0x1234;
}


/** @brief mzf_header_swap_endianity(): pořadí bajtů a bajt 11h. */
static void test_swap ( void ) {
    st_MZF_HEADER h;
    make_header ( &h, 0x00 );
    st_MZF_HEADER s = h;
    mzf_header_swap_endianity ( &s );
    const uint8_t *b = ( const uint8_t* ) &s;
    CHECK ( b[0x11] == 0x00, "swap keeps 11h = 00h" );
    CHECK ( b[0x12] == 0x10 && b[0x13] == 0x00 && b[0x14] == 0x00 && b[0x15] == 0x12 && b[0x16] == 0x34 && b[0x17] == 0x12,
            "swap writes fsize/fstrt/fexec little-endian" );
    mzf_header_swap_endianity ( &s );
    CHECK ( memcmp ( &s, &h, sizeof ( h ) ) == 0, "swap twice returns the original" );
    st_MZF_HEADER c = h;
    mzf_header_items_correction ( &c );
    CHECK ( ( ( const uint8_t* ) &c )[0x11] == 0x0D, "mzf_header_items_correction still sets 11h = 0Dh" );
}


/**
 * @brief Hlavička bloku 0x46 z převodu v1.0 (bajty bloku, LE).
 * @param b Blok v1.0. @param[out] term Bajt 11h. @return 1 = převod OK.
 */
static int v1conv_term ( const st_TZX_BLOCK *b, uint8_t *term ) {
    st_TZX_BLOCK *out = NULL;
    if ( tmz_v1conv_block ( b, NULL, &out ) != TMZ_V1CONV_OK ) return 0;
    st_TMZ_MZFILE mf;
    int ok = tmz_mzfile_parse ( out, &mf ) == TZX_OK;
    if ( ok ) {
        *term = mf.header[0x11];
        tmz_mzfile_free ( &mf );
    }
    tmz_block_free ( out );
    return ok;
}


/** @brief Bloky 0x40, 0x41, 0x44, 0x45: zápis, parser a převod zachovají 11h. */
static void test_blocks ( void ) {
    st_MZF_HEADER h;
    uint8_t body[16];
    for ( int i = 0; i < 16; i++ ) body[i] = ( uint8_t ) i;
    for ( int t = 0; t < 2; t++ ) {
        uint8_t term = t ? 0x0D : 0x00;
        char msg[96];
        make_header ( &h, term );

        /* 0x40 */
        st_TZX_BLOCK *b = tmz_block_create_mz_standard ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 1000, &h, body, 16 );
        snprintf ( msg, sizeof ( msg ), "0x40 stores 11h = %02Xh", term );
        CHECK ( b && b->data[4 + 4 + 0x11] == term, msg );
        uint8_t ct = 0xFF;
        snprintf ( msg, sizeof ( msg ), "0x40 -> 0x46 keeps 11h = %02Xh", term );
        CHECK ( b && v1conv_term ( b, &ct ) && ct == term, msg );
        en_TZX_ERROR e;
        st_TMZ_MZ_STANDARD_DATA *sd = b ? tmz_block_parse_mz_standard ( b, NULL, &e ) : NULL;
        snprintf ( msg, sizeof ( msg ), "0x40 parser returns 11h = %02Xh and host fsize", term );
        CHECK ( sd && ( ( const uint8_t* ) &sd->mzf_header )[0x11] == term && sd->mzf_header.fsize == 16, msg );
        tmz_block_free ( b );

        /* 0x41 */
        st_TMZ_MZ_TURBO_DATA tp;
        memset ( &tp, 0, sizeof ( tp ) );
        tp.machine = TMZ_MACHINE_MZ800;
        tp.pulseset = TMZ_PULSESET_800;
        tp.format = TMZ_FORMAT_TURBO;   /* převoditelné na 0x46 (TC-1.21) */
        tp.speed = CMTSPEED_2_1;
        tp.pause_ms = 1000;
        tp.mzf_header = h;
        b = tmz_block_create_mz_turbo ( &tp, body, 16 );
        snprintf ( msg, sizeof ( msg ), "0x41 -> 0x46 keeps 11h = %02Xh", term );
        ct = 0xFF;
        CHECK ( b && v1conv_term ( b, &ct ) && ct == term, msg );
        st_TMZ_MZ_TURBO_DATA *td = b ? tmz_block_parse_mz_turbo ( b, NULL, &e ) : NULL;
        snprintf ( msg, sizeof ( msg ), "0x41 parser returns 11h = %02Xh", term );
        CHECK ( td && ( ( const uint8_t* ) &td->mzf_header )[0x11] == term && td->mzf_header.fstrt == 0x1200, msg );
        tmz_block_free ( b );

        /* 0x44 */
        b = tmz_block_create_mz_loader ( TMZ_LOADER_TURBO_1_0, CMTSPEED_1_1, &h, body, 16 );
        st_TMZ_MZ_LOADER *ld = b ? tmz_block_parse_mz_loader ( b, NULL, &e ) : NULL;
        snprintf ( msg, sizeof ( msg ), "0x44 parser returns 11h = %02Xh", term );
        CHECK ( ld && ( ( const uint8_t* ) &ld->mzf_header )[0x11] == term && ld->mzf_header.fexec == 0x1234, msg );
        tmz_block_free ( b );

        /* 0x45: hlavička BSD (typ 03h, fsize/fstrt/fexec 0), jeden kus 258 B s ID FFFFh */
        st_MZF_HEADER bh = h;
        bh.ftype = 0x03;
        bh.fsize = bh.fstrt = bh.fexec = 0;
        uint8_t chunk[TMZ_BASIC_CHUNK_SIZE];
        memset ( chunk, 0, sizeof ( chunk ) );
        chunk[0] = chunk[1] = 0xFF;
        b = tmz_block_create_mz_basic_data ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 1000, &bh, chunk, 1 );
        snprintf ( msg, sizeof ( msg ), "0x45 -> 0x46 keeps 11h = %02Xh", term );
        ct = 0xFF;
        CHECK ( b && v1conv_term ( b, &ct ) && ct == term, msg );
        st_TMZ_MZ_BASIC_DATA *bd = b ? tmz_block_parse_mz_basic_data ( b, NULL, &e ) : NULL;
        snprintf ( msg, sizeof ( msg ), "0x45 parser returns 11h = %02Xh", term );
        CHECK ( bd && ( ( const uint8_t* ) &bd->mzf_header )[0x11] == term && bd->mzf_header.ftype == 0x03, msg );
        tmz_block_free ( b );
    }
}


int main ( void ) {
    test_swap ();
    test_blocks ();
    if ( g_failures ) {
        printf ( "=== test_mzf_terminator: %d FAILED ===\n", g_failures );
        return 1;
    }
    printf ( "=== test_mzf_terminator: all passed ===\n" );
    return 0;
}
