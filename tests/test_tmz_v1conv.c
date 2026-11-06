/**
 * @file   test_tmz_v1conv.c
 * @brief  Převod bloků v1.0 na 0x46 (spec 9.2): mapování na profily,
 *         nepřevoditelné případy, pauza, převod souboru.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_mzfile.h"
#include "libs/tmz/tmz_part_fm.h"
#include "libs/tmz/tmz_tlv.h"
#include "libs/tmz/tmz_v1conv.h"
#include "libs/tmz/tmz_player.h"
#include "libs/cmt_stream/cmt_stream.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;
/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )

/** @brief Hlavička (pořadí hostitele, 11h = 00h: převod ji musí opravit na 0Dh). */
static st_MZF_HEADER g_hdr;
/** @brief Tělo. */
static uint8_t g_body[64];

/** @brief Naplní testovací data. */
static void make_data ( void ) {
    memset ( &g_hdr, 0, sizeof ( g_hdr ) );
    uint8_t *h = ( uint8_t* ) &g_hdr;
    h[0] = 0x01;
    memcpy ( h + 1, "CONV", 4 );
    h[5] = 0x0D;
    g_hdr.fsize = sizeof ( g_body );
    g_hdr.fstrt = 0x1200;
    g_hdr.fexec = 0x1200;
    for ( size_t i = 0; i < sizeof ( g_body ); i++ ) g_body[i] = ( uint8_t ) i;
}

/**
 * @brief Převede blok a vrátí ID profilu a PROFILE_ARGS výsledku.
 * @return Výsledek převodu; při OK naplní id a args.
 */
static en_TMZ_V1CONV_RESULT conv ( const st_TZX_BLOCK *b, const st_TMZ_V1CONV_CONFIG *cfg, char *id, char *args,
                                   st_TZX_BLOCK **keep ) {
    st_TZX_BLOCK *out = NULL;
    en_TMZ_V1CONV_RESULT r = tmz_v1conv_block ( b, cfg, &out );
    id[0] = args[0] = 0;
    if ( r == TMZ_V1CONV_OK ) {
        st_TMZ_MZFILE mf;
        if ( tmz_mzfile_parse ( out, &mf ) == TZX_OK ) {
            strcpy ( id, mf.profile_id );
            st_TMZ_TLV_ITER it;
            st_TMZ_TLV t;
            tmz_tlv_iter_init ( &it, mf.block_tlv, mf.block_tlv_length );
            if ( tmz_tlv_iter_next ( &it, &t ) == 1 && t.tag == TMZ_TAG_PROFILE_ARGS ) {
                memcpy ( args, t.value, t.length );
                args[t.length] = 0;
            }
            tmz_mzfile_free ( &mf );
        }
        if ( keep ) *keep = out;
        else tmz_block_free ( out );
    }
    return r;
}

/** @brief Blok 0x41 se zadanými poli (ostatní 0). */
static st_TZX_BLOCK* turbo_block ( uint8_t format, uint8_t speed, uint8_t pulseset, uint16_t pause_ms ) {
    st_TMZ_MZ_TURBO_DATA td;
    memset ( &td, 0, sizeof ( td ) );
    td.machine = TMZ_MACHINE_MZ800;
    td.pulseset = pulseset;
    td.format = format;
    td.speed = speed;
    td.pause_ms = pause_ms;
    td.mzf_header = g_hdr;
    return tmz_block_create_mz_turbo ( &td, g_body, sizeof ( g_body ) );
}

int main ( void ) {
    make_data ();
    st_TMZ_V1CONV_CONFIG cfg;
    tmz_v1conv_config_init ( &cfg );
    char id[40], args[200];

    /* 0x40 sada 800 -> ROM-800-NOMINAL, pauza z bloku */
    st_TZX_BLOCK *b = tmz_block_create_mz_standard ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 500, &g_hdr, g_body, sizeof ( g_body ) );
    CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_OK && strcmp ( id, "ROM-800-NOMINAL" ) == 0
            && strcmp ( args, "gap=short,pause=500,machine=mz800" ) == 0, "0x40 800 -> ROM-800-NOMINAL" );
    tmz_block_free ( b );

    /* 0x40 sada 700 -> LEGACY normal se sadou z bloku; 11h (00h) zůstává podle bloku */
    b = tmz_block_create_mz_standard ( TMZ_MACHINE_MZ700, TMZ_PULSESET_700, 0, &g_hdr, g_body, sizeof ( g_body ) );
    st_TZX_BLOCK *o = NULL;
    int ok = conv ( b, &cfg, id, args, &o ) == TMZ_V1CONV_OK && strcmp ( id, "TAPEMZ-V1-LEGACY" ) == 0
             && strcmp ( args, "format=normal,pulseset=700,speed=1:1,lgap=6400,sgap=11000,pause=0,machine=mz700" ) == 0;
    if ( ok ) {
        st_TMZ_MZFILE mf;
        ok = tmz_mzfile_parse ( o, &mf ) == TZX_OK && mf.header[0x11] == 0x00;
        if ( ok ) {
            static st_MZCMT_FM_PARAMS p;
            st_TMZ_PART_COMMON c;
            /* pauza 0 = žádná PAUSE na poslední části */
            ok = tmz_part_fm_decode ( mf.parts[1].tlv, mf.parts[1].tlv_length, 3546895, &p, &c ) == TMZ_PART_OK
                 && p.pause == 0;
            tmz_mzfile_free ( &mf );
        }
    }
    CHECK ( ok, "0x40 700 -> LEGACY normal 700, header 11h = 00h kept, no pause" );
    tmz_block_free ( o );
    tmz_block_free ( b );

    /* 0x41 NORMAL: formát z konfigurace, neplatná rychlost -> default_speed */
    b = turbo_block ( TMZ_FORMAT_NORMAL, 99, TMZ_PULSESET_800, 0 );
    CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_OK
            && strcmp ( args, "format=normal,pulseset=800,speed=1:1,lgap=6400,sgap=11000,pause=0,machine=mz800" ) == 0,
            "0x41 NORMAL invalid speed -> default_speed" );
    /* NORMAL nad 1:1 zůstává na cestě v1.0 (zavedení neověřitelné, systematický posun délky) */
    cfg.default_speed = CMTSPEED_2_1;
    CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_NOT_CONVERTIBLE, "0x41 NORMAL default_speed 2:1 kept" );
    cfg.default_speed = CMTSPEED_1_1;
    {
        st_TZX_BLOCK *b3 = turbo_block ( TMZ_FORMAT_NORMAL, CMTSPEED_3_1, TMZ_PULSESET_800, 0 );
        CHECK ( conv ( b3, &cfg, id, args, NULL ) == TMZ_V1CONV_NOT_CONVERTIBLE, "0x41 NORMAL 3:1 kept" );
        tmz_block_free ( b3 );
    }
    cfg.formatset = MZTAPE_FORMATSET_MZ80B;
    CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_OK
            && strcmp ( args, "format=normal,pulseset=80b,speed=1:1,lgap=10000,sgap=11000,pause=0,machine=mz800" ) == 0,
            "0x41 NORMAL with player format MZ80B" );
    cfg.formatset = MZTAPE_FORMATSET_MZ800;
    CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_NOT_CONVERTIBLE, "0x41 NORMAL with format MZ800 (copies) kept" );
    cfg.formatset = MZTAPE_FORMATSET_MZ800_SANE;
    tmz_block_free ( b );
    b = turbo_block ( TMZ_FORMAT_NORMAL, CMTSPEED_2_1_CPM, TMZ_PULSESET_800, 0 );
    CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_NOT_CONVERTIBLE, "0x41 NORMAL 2:1 CPM kept" );
    tmz_block_free ( b );

    /* 0x41 TURBO -> TC-1.21 s předzavaděčem; vlastní pulzy, 80B, 25:14 nepřevádí */
    b = turbo_block ( TMZ_FORMAT_TURBO, CMTSPEED_3_1, TMZ_PULSESET_800, 300 );
    CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_OK && strcmp ( id, "TC-1.21" ) == 0
            && strcmp ( args, "speed=3:1,loader=Y,pause=300,machine=mz800" ) == 0, "0x41 TURBO -> TC-1.21" );
    tmz_block_free ( b );
    b = turbo_block ( TMZ_FORMAT_TURBO, CMTSPEED_25_14, TMZ_PULSESET_800, 0 );
    CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_NOT_CONVERTIBLE, "0x41 TURBO 25:14 kept" );
    tmz_block_free ( b );
    b = turbo_block ( TMZ_FORMAT_TURBO, CMTSPEED_2_1, TMZ_PULSESET_80B, 0 );
    CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_NOT_CONVERTIBLE, "0x41 TURBO 80B kept" );
    tmz_block_free ( b );
    {
        st_TMZ_MZ_TURBO_DATA td;
        memset ( &td, 0, sizeof ( td ) );
        td.format = TMZ_FORMAT_TURBO;
        td.speed = CMTSPEED_2_1;
        td.pulseset = TMZ_PULSESET_800;
        td.short_high = 1200;
        td.mzf_header = g_hdr;
        b = tmz_block_create_mz_turbo ( &td, g_body, sizeof ( g_body ) );
        CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_NOT_CONVERTIBLE, "0x41 TURBO custom pulses kept" );
        tmz_block_free ( b );
        td.short_high = 0;
        td.flags = TMZ_TURBO_FLAG_BODY_COPY;
        b = tmz_block_create_mz_turbo ( &td, g_body, sizeof ( g_body ) );
        CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_NOT_CONVERTIBLE, "0x41 TURBO body copy kept" );
        tmz_block_free ( b );
        /*
         * FASTIPL: převod na profil Intercopy V10.2 FAST IPL;
         * rychlosti Intercopy 1:1, 2:1, 7:3, 8:3, jinak cesta v1.0
         */
        td.flags = 0;
        td.format = TMZ_FORMAT_FASTIPL;
        b = tmz_block_create_mz_turbo ( &td, g_body, sizeof ( g_body ) );
        CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_OK && strcmp ( id, "IC-10.2-FASTIPL" ) == 0
                && strcmp ( args, "bd=2400,pause=0,machine=generic" ) == 0, "0x41 FASTIPL 2:1 -> IC-10.2-FASTIPL 2400" );
        tmz_block_free ( b );
        static const uint8_t kept_speeds[3] = { CMTSPEED_3_1, CMTSPEED_3_2, CMTSPEED_9_7 };
        for ( int k = 0; k < 3; k++ ) {
            td.speed = kept_speeds[k];
            b = tmz_block_create_mz_turbo ( &td, g_body, sizeof ( g_body ) );
            CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_NOT_CONVERTIBLE, "0x41 FASTIPL speed without Intercopy row kept" );
            tmz_block_free ( b );
        }
        td.speed = CMTSPEED_2_1;
        td.short_high = 1200;
        b = tmz_block_create_mz_turbo ( &td, g_body, sizeof ( g_body ) );
        CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_NOT_CONVERTIBLE, "0x41 FASTIPL custom pulses kept" );
        tmz_block_free ( b );
        td.short_high = 0;
        td.lgap_length = 3000;
        b = tmz_block_create_mz_turbo ( &td, g_body, sizeof ( g_body ) );
        CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_NOT_CONVERTIBLE, "0x41 FASTIPL lgap 3000 kept" );
        tmz_block_free ( b );
    }
    b = turbo_block ( TMZ_FORMAT_FSK, 0, TMZ_PULSESET_800, 0 );
    CHECK ( conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_NOT_CONVERTIBLE, "0x41 FSK kept" );
    tmz_block_free ( b );

    /* 0x45 BSD: 1 a 254 kusů -> BASIC-1Z016, 255 = chyba bloku */
    uint8_t *chunks = calloc ( 255, 258 );
    st_MZF_HEADER bh = g_hdr;
    bh.ftype = 0x03;
    bh.fsize = bh.fstrt = bh.fexec = 0;
    chunks[253 * 258] = chunks[253 * 258 + 1] = 0xFF;
    b = tmz_block_create_mz_basic_data ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 0, &bh, chunks, 254 );
    o = NULL;
    ok = conv ( b, &cfg, id, args, &o ) == TMZ_V1CONV_OK && strcmp ( id, "BASIC-1Z016" ) == 0
         && strcmp ( args, "pause=0,machine=mz800" ) == 0;
    if ( ok ) {
        st_TMZ_MZFILE mf;
        ok = tmz_mzfile_parse ( o, &mf ) == TZX_OK && mf.part_count == 255 && mf.body_size == 254u * 258
             && mf.parts[254].source == TMZ_SOURCE_BODY_SLICE && mf.parts[254].slice_offset == 253u * 258;
        if ( ok ) tmz_mzfile_free ( &mf );
    }
    CHECK ( ok, "0x45 254 chunks -> BASIC-1Z016, 255 parts" );
    tmz_block_free ( o );
    tmz_block_free ( b );
    chunks[0] = chunks[1] = 0xFF;
    b = tmz_block_create_mz_basic_data ( TMZ_MACHINE_MZ700, TMZ_PULSESET_700, 500, &bh, chunks, 1 );
    ok = b && conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_OK && strcmp ( id, "BASIC-1Z016" ) == 0
         && strcmp ( args, "pause=500,machine=mz700" ) == 0;
    CHECK ( ok, "0x45 1 chunk MZ-700 -> BASIC-1Z016, pulseset ignored" );
    tmz_block_free ( b );
    chunks[0] = chunks[1] = 0;
    chunks[253 * 258] = chunks[253 * 258 + 1] = 0;
    chunks[254 * 258] = chunks[254 * 258 + 1] = 0xFF;
    b = tmz_block_create_mz_basic_data ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 0, &bh, chunks, 255 );
    CHECK ( b && conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_ERROR, "0x45 255 chunks -> error" );
    tmz_block_free ( b );

    /* 0x45 se převede i při v1_convert = 0 a hraje stejně jako BASIC-1Z016; 255 kusů se nepřehraje */
    {
        st_TMZ_PLAYER_CONFIG pc;
        tmz_player_config_init ( &pc );
        pc.v1_convert = 0;
        en_TMZ_PLAYER_ERROR pe;
        chunks[254 * 258] = chunks[254 * 258 + 1] = 0;
        chunks[258] = chunks[259] = 0xFF;
        b = tmz_block_create_mz_basic_data ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 1000, &bh, chunks, 2 );
        st_CMT_STREAM *s = tmz_player_play_block ( b, &pc, &pe );
        st_TZX_BLOCK *cb = NULL;
        st_CMT_STREAM *s2 = NULL;
        if ( tmz_v1conv_block ( b, &cfg, &cb ) == TMZ_V1CONV_OK ) s2 = tmz_player_play_block ( cb, &pc, &pe );
        CHECK ( s && s2 && cmt_stream_get_count_scans ( s ) == cmt_stream_get_count_scans ( s2 ),
                "0x45 with v1_convert = 0 plays the converted signal" );
        cmt_stream_destroy ( s );
        cmt_stream_destroy ( s2 );
        tmz_block_free ( cb );
        tmz_block_free ( b );
        pc.v1_convert = 1;
        chunks[258] = chunks[259] = 0;
        chunks[254 * 258] = chunks[254 * 258 + 1] = 0xFF;
        b = tmz_block_create_mz_basic_data ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 0, &bh, chunks, 255 );
        s = tmz_player_play_block ( b, &pc, &pe );
        CHECK ( !s && pe != TMZ_PLAYER_OK, "0x45 with 255 chunks is not played" );
        cmt_stream_destroy ( s );
        tmz_block_free ( b );
    }
    free ( chunks );

    /* poškozený blok 0x40 */
    st_TZX_BLOCK bad = { TMZ_BLOCK_ID_MZ_STANDARD_DATA, 10, g_body };
    st_TZX_BLOCK *ob = NULL;
    CHECK ( tmz_v1conv_block ( &bad, &cfg, &ob ) == TMZ_V1CONV_ERROR && ob == NULL, "truncated 0x40 -> error" );

    /*
     * 0x41 CPM_TAPE: vždy PEZIK-2.0A; úroveň 2 = 3200, jinak 2400
     * (úroveň 0 "1200" neexistuje, neplatnou hrál player v1.0 jako 2400)
     */
    {
        static const struct { uint8_t speed; const char *args; } cpm[] = {
            { 0, "bd=2400,pause=300,machine=mz800" }, { 1, "bd=2400,pause=300,machine=mz800" },
            { 2, "bd=3200,pause=300,machine=mz800" }, { 7, "bd=2400,pause=300,machine=mz800" },
        };
        for ( size_t i = 0; i < sizeof ( cpm ) / sizeof ( cpm[0] ); i++ ) {
            b = turbo_block ( TMZ_FORMAT_CPM_TAPE, cpm[i].speed, TMZ_PULSESET_800, 300 );
            char msg[96];
            int okc = conv ( b, &cfg, id, args, NULL ) == TMZ_V1CONV_OK && strcmp ( id, "PEZIK-2.0A" ) == 0
                      && strcmp ( args, cpm[i].args ) == 0;
            if ( !okc ) printf ( "  got %s (%s)\n", id, args );
            snprintf ( msg, sizeof ( msg ), "0x41 CPM_TAPE level %u -> PEZIK-2.0A %s", cpm[i].speed, cpm[i].args );
            CHECK ( okc, msg );
            tmz_block_free ( b );
        }
        /* player přehraje blok i bez převodu v paměti (v1_convert = 0): starý kodér už není */
        st_TMZ_PLAYER_CONFIG pc;
        tmz_player_config_init ( &pc );
        pc.v1_convert = 0;
        b = turbo_block ( TMZ_FORMAT_CPM_TAPE, 1, TMZ_PULSESET_800, 0 );
        en_TMZ_PLAYER_ERROR pe;
        st_CMT_STREAM *s = tmz_player_play_block ( b, &pc, &pe );
        st_TZX_BLOCK *cb = NULL;
        st_CMT_STREAM *s2 = NULL;
        if ( tmz_v1conv_block ( b, &cfg, &cb ) == TMZ_V1CONV_OK ) s2 = tmz_player_play_block ( cb, &pc, &pe );
        CHECK ( s && s2 && cmt_stream_get_count_scans ( s ) == cmt_stream_get_count_scans ( s2 ),
                "0x41 CPM_TAPE with v1_convert = 0 plays the converted signal" );
        cmt_stream_destroy ( s );
        cmt_stream_destroy ( s2 );
        tmz_block_free ( cb );
        tmz_block_free ( b );
    }

    /* převod souboru: 0x43 zůstane, 0x40 -> 0x46, FSK zůstane */
    st_TZX_FILE file;
    memset ( &file, 0, sizeof ( file ) );
    st_TZX_BLOCK *fb[3] = {
        tmz_block_create_mz_machine_info ( TMZ_MACHINE_MZ800, 3546895, 0 ),
        tmz_block_create_mz_standard ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 0, &g_hdr, g_body, sizeof ( g_body ) ),
        turbo_block ( TMZ_FORMAT_FSK, 0, TMZ_PULSESET_800, 0 ),
    };
    file.blocks = malloc ( 3 * sizeof ( st_TZX_BLOCK ) );
    for ( int i = 0; i < 3; i++ ) {
        file.blocks[i] = *fb[i];
        free ( fb[i] );   /* data přebírá soubor */
    }
    file.block_count = file.block_capacity = 3;
    st_TMZ_V1CONV_REPORT rep;
    CHECK ( tmz_v1conv_file ( &file, &cfg, &rep ) == EXIT_SUCCESS && rep.converted == 1 && rep.kept == 1 && rep.errors == 0
            && file.blocks[0].id == TMZ_BLOCK_ID_MZ_MACHINE_INFO && file.blocks[1].id == TMZ_BLOCK_ID_MZ_FILE
            && file.blocks[2].id == TMZ_BLOCK_ID_MZ_TURBO_DATA, "file conversion" );
    for ( int i = 0; i < 3; i++ ) free ( file.blocks[i].data );
    free ( file.blocks );

    printf ( "\n=== TMZ v1 Conversion Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
