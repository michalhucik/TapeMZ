/**
 * @file   test_tmz_blocks_parse.c
 * @brief  Regresní test: parse bloků 0x40/0x41/0x42/0x44 validuje velikost dat.
 *
 * Pro každý typ bloku: korektní blok (velikost dat přesně na hraně)
 * projde a blok s body_size/loader_size o 1 větším než data vrátí NULL
 * a TZX_ERROR_INVALID_BLOCK; 0x41 a 0x44 navíc s velkou hodnotou.
 * U 0x40 se ověřuje i prázdné tělo a odmítnutí v tmz_player_play_block,
 * u 0x40 a 0x41 odmítnutí v tmz_block_to_mzf (bez čtení mimo alokaci).
 *
 * Pozn.: parse dnes data bloku mutuje, proto se pro každý
 * případ "rejected" vytváří nový blok.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_player.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )


/**
 * @brief Přepíše 16bit LE pole uvnitř dat bloku.
 * @param block Blok (struktura začíná za 4B DWORD délkou).
 * @param field_offset Offset pole od začátku struktury bloku.
 * @param value Nová hodnota.
 */
static void poke16 ( st_TZX_BLOCK *block, size_t field_offset, uint16_t value ) {
    block->data[4 + field_offset] = ( uint8_t ) ( value & 0xFF );
    block->data[4 + field_offset + 1] = ( uint8_t ) ( value >> 8 );
}


int main ( void ) {
    st_MZF_HEADER hdr;
    memset ( &hdr, 0, sizeof ( hdr ) );
    hdr.ftype = 0x01;
    memcpy ( hdr.fname.name, "BOUNDS", 6 );
    hdr.fname.name[6] = 0x0D;
    hdr.fsize = 16;
    uint8_t body[16];
    for ( int i = 0; i < 16; i++ ) body[i] = ( uint8_t ) i;
    en_TZX_ERROR err;
    st_TZX_BLOCK *b;

    /* --- 0x40 --- */
    b = tmz_block_create_mz_standard ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 1000, &hdr, body, 16 );
    CHECK ( b && tmz_block_parse_mz_standard ( b, NULL, &err ) && err == TZX_OK, "0x40 valid block parses" );
    tmz_block_free ( b );

    b = tmz_block_create_mz_standard ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 1000, &hdr, body, 16 );
    poke16 ( b, offsetof ( st_TMZ_MZ_STANDARD_DATA, body_size ), 17 );
    CHECK ( !tmz_block_parse_mz_standard ( b, NULL, &err ) && err == TZX_ERROR_INVALID_BLOCK, "0x40 body_size+1 rejected" );
    tmz_block_free ( b );

    b = tmz_block_create_mz_standard ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 1000, &hdr, body, 16 );
    poke16 ( b, offsetof ( st_TMZ_MZ_STANDARD_DATA, body_size ), 0xFFFF );
    CHECK ( tmz_block_to_mzf ( b, &err ) == NULL, "0x40 to_mzf rejects oversized body" );
    tmz_block_free ( b );

    b = tmz_block_create_mz_standard ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 1000, &hdr, body, 16 );
    poke16 ( b, offsetof ( st_TMZ_MZ_STANDARD_DATA, body_size ), 0xFFFF );
    {
        st_TMZ_PLAYER_CONFIG cfg;
        tmz_player_config_init ( &cfg );
        en_TMZ_PLAYER_ERROR perr = TMZ_PLAYER_OK;
        st_CMT_STREAM *s = tmz_player_play_block ( b, &cfg, &perr );
        CHECK ( s == NULL && perr != TMZ_PLAYER_OK, "0x40 player rejects oversized body" );
    }
    tmz_block_free ( b );

    /* prázdné tělo musí projít */
    b = tmz_block_create_mz_standard ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 1000, &hdr, NULL, 0 );
    CHECK ( b && tmz_block_parse_mz_standard ( b, NULL, &err ) && err == TZX_OK, "0x40 empty body parses" );
    tmz_block_free ( b );

    /* --- 0x41 --- */
    st_TMZ_MZ_TURBO_DATA tp;
    memset ( &tp, 0, sizeof ( tp ) );
    tp.machine = TMZ_MACHINE_MZ800;
    tp.pulseset = TMZ_PULSESET_800;
    tp.format = TMZ_FORMAT_NORMAL;
    tp.speed = CMTSPEED_1_1;
    memcpy ( &tp.mzf_header, &hdr, sizeof ( hdr ) );
    b = tmz_block_create_mz_turbo ( &tp, body, 16 );
    CHECK ( b && tmz_block_parse_mz_turbo ( b, NULL, &err ) && err == TZX_OK, "0x41 valid block parses" );
    tmz_block_free ( b );

    b = tmz_block_create_mz_turbo ( &tp, body, 16 );
    poke16 ( b, offsetof ( st_TMZ_MZ_TURBO_DATA, body_size ), 17 );
    CHECK ( !tmz_block_parse_mz_turbo ( b, NULL, &err ) && err == TZX_ERROR_INVALID_BLOCK, "0x41 body_size+1 rejected" );
    tmz_block_free ( b );

    b = tmz_block_create_mz_turbo ( &tp, body, 16 );
    poke16 ( b, offsetof ( st_TMZ_MZ_TURBO_DATA, body_size ), 0xFFFF );
    CHECK ( !tmz_block_parse_mz_turbo ( b, NULL, &err ) && err == TZX_ERROR_INVALID_BLOCK, "0x41 body_size 0xFFFF rejected" );
    tmz_block_free ( b );

    b = tmz_block_create_mz_turbo ( &tp, body, 16 );
    poke16 ( b, offsetof ( st_TMZ_MZ_TURBO_DATA, body_size ), 0xFFFF );
    CHECK ( tmz_block_to_mzf ( b, &err ) == NULL, "0x41 to_mzf rejects oversized body" );
    tmz_block_free ( b );

    /* --- 0x42 --- */
    b = tmz_block_create_mz_extra_body ( TMZ_FORMAT_NORMAL, CMTSPEED_1_1, 0, body, 16 );
    CHECK ( b && tmz_block_parse_mz_extra_body ( b, NULL, &err ) && err == TZX_OK, "0x42 valid block parses" );
    tmz_block_free ( b );

    b = tmz_block_create_mz_extra_body ( TMZ_FORMAT_NORMAL, CMTSPEED_1_1, 0, body, 16 );
    poke16 ( b, offsetof ( st_TMZ_MZ_EXTRA_BODY, body_size ), 17 );
    CHECK ( !tmz_block_parse_mz_extra_body ( b, NULL, &err ) && err == TZX_ERROR_INVALID_BLOCK, "0x42 body_size+1 rejected" );
    tmz_block_free ( b );

    /* --- 0x44 --- */
    b = tmz_block_create_mz_loader ( TMZ_LOADER_TURBO_1_2X, CMTSPEED_2_1, &hdr, body, 16 );
    CHECK ( b && tmz_block_parse_mz_loader ( b, NULL, &err ) && err == TZX_OK, "0x44 valid block parses" );
    tmz_block_free ( b );

    b = tmz_block_create_mz_loader ( TMZ_LOADER_TURBO_1_2X, CMTSPEED_2_1, &hdr, body, 16 );
    poke16 ( b, offsetof ( st_TMZ_MZ_LOADER, loader_size ), 17 );
    CHECK ( !tmz_block_parse_mz_loader ( b, NULL, &err ) && err == TZX_ERROR_INVALID_BLOCK, "0x44 loader_size+1 rejected" );
    tmz_block_free ( b );

    b = tmz_block_create_mz_loader ( TMZ_LOADER_TURBO_1_2X, CMTSPEED_2_1, &hdr, body, 16 );
    poke16 ( b, offsetof ( st_TMZ_MZ_LOADER, loader_size ), 1000 );
    CHECK ( !tmz_block_parse_mz_loader ( b, NULL, &err ) && err == TZX_ERROR_INVALID_BLOCK, "0x44 loader_size 1000 rejected" );
    tmz_block_free ( b );

    printf ( "\n=== TMZ blocks parse Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
