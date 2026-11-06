/**
 * @file   test_tmz_mzt.c
 * @brief  MZT a hlavička CMTSPEED (spec 10): ukázky UniCMT, V5, poškozené vstupy,
 *         import na bloky UNICMT a export tam a zpět.
 *         Spouštět z build/.
 * Bez tstdata/ vypíše SKIP a skončí 0 (tests/tstdata_skip.h).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/tmz/tmz_mzt.h"
#include "libs/tmz/tmz_mzfile.h"
#include "libs/tmz/tmz_tlv.h"
#include "libs/tmz/tmz_part_fm.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_profile.h"
#include "libs/mzcmt_fm/mzcmt_fm.h"
#include "tstdata_skip.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;
/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )

/**
 * @brief Načte celý soubor.
 * @param path Cesta. @param[out] size Velikost.
 * @return Data (uvolnit free), nebo NULL.
 */
static uint8_t* load ( const char *path, uint32_t *size ) {
    FILE *f = fopen ( path, "rb" );
    if ( !f ) return NULL;
    fseek ( f, 0, SEEK_END );
    long n = ftell ( f );
    fseek ( f, 0, SEEK_SET );
    uint8_t *d = malloc ( n > 0 ? ( size_t ) n : 1 );
    if ( d && fread ( d, 1, ( size_t ) n, f ) != ( size_t ) n ) {
        free ( d );
        d = NULL;
    }
    fclose ( f );
    *size = ( uint32_t ) n;
    return d;
}

/** @brief Varování exportu do počítadla. */
static void count_warn ( void *user, uint32_t idx, const char *text ) {
    ( void ) idx;
    ( void ) text;
    ( *( int* ) user )++;
}

/**
 * @brief Import MZT na bloky UNICMT do souboru v paměti a export.
 * @return 1 = export bajtově shodný s ref.
 */
static int roundtrip ( const char *src, const char *ref, en_TMZ_MZT_MODE mode, int *warnings ) {
    uint32_t n, rn;
    uint8_t *d = load ( src, &n ), *r = load ( ref, &rn );
    st_TMZ_MZT m;
    st_TZX_BLOCK **blk = NULL;
    uint32_t nb = 0;
    int lead, trail, same = 0;
    if ( d && r && tmz_mzt_parse ( d, n, &m ) == EXIT_SUCCESS ) {
        if ( tmz_mzt_build_unicmt_blocks ( &m, NULL, &blk, &nb, &lead, &trail ) == EXIT_SUCCESS ) {
            st_TZX_FILE f;
            memset ( &f, 0, sizeof ( f ) );
            f.blocks = malloc ( nb * sizeof ( st_TZX_BLOCK ) );
            int all = ( f.blocks != NULL );
            for ( uint32_t i = 0; all && i < nb; i++ ) {
                if ( blk[i] ) f.blocks[i] = *blk[i];
                else all = 0;
            }
            f.block_count = nb;
            uint8_t *out = NULL;
            uint32_t on = 0;
            *warnings = 0;
            if ( all && tmz_mzt_export ( &f, mode, &out, &on, count_warn, warnings ) == EXIT_SUCCESS ) {
                same = ( on == rn && memcmp ( out, r, rn ) == 0 );
                if ( !same ) {
                    uint32_t k = 0;
                    while ( k < on && k < rn && out[k] == r[k] ) k++;
                    printf ( "  %s: size %u vs %u, first difference at 0x%X\n", src, on, rn, k );
                }
            }
            free ( out );
            free ( f.blocks );
            for ( uint32_t i = 0; i < nb; i++ ) tmz_block_free ( blk[i] );
            free ( blk );
        }
        tmz_mzt_free ( &m );
    }
    free ( d );
    free ( r );
    return same;
}


int main ( void ) {
    if ( tstdata_skip ( "test_tmz_mzt" ) ) return 0;
    static const char *const sp[3] = { "../tstdata/unicmt/1xspeed.mzf", "../tstdata/unicmt/2xspeed.mzf",
                                       "../tstdata/unicmt/3xspeed.mzf" };
    static const uint16_t pv[3][4] = { { 470, 494, 240, 278 }, { 235, 247, 120, 139 }, { 156, 164, 80, 92 } };
    for ( int i = 0; i < 3; i++ ) {
        uint32_t n;
        uint8_t *d = load ( sp[i], &n );
        uint8_t built[128];
        uint16_t got[4] = { 0 };
        tmz_mzt_build_cmtspeed ( pv[i], built );
        char label[80];
        snprintf ( label, sizeof ( label ), "CMTSPEED %dx: recognised and byte-identical", i + 1 );
        CHECK ( d && n == 128 && tmz_mzt_is_cmtspeed ( d, got ) && memcmp ( got, pv[i], sizeof ( got ) ) == 0
                && memcmp ( built, d, 128 ) == 0, label );
        free ( d );
    }

    /* interkar_plain: 3 MZF bez CMTSPEED, všechny 1x */
    uint32_t n;
    uint8_t *d = load ( "../tstdata/unicmt/interkar_plain.mzt", &n );
    st_TMZ_MZT m;
    int ok = d && tmz_mzt_parse ( d, n, &m ) == EXIT_SUCCESS && m.count == 3 && m.cmtspeed_count == 0
             && m.items[1].offset == 0x402 && m.items[2].offset == 0x23C2 && m.items[2].body_size == 0xB43E
             && m.items[2].pulses_us[0] == 470;
    CHECK ( ok, "interkar_plain: 3 files, 1x" );
    if ( ok ) tmz_mzt_free ( &m );
    free ( d );

    /* interkar_2x: soubor, CMTSPEED 2x, 2 soubory s rychlostí 2x */
    d = load ( "../tstdata/unicmt/interkar_2x.mzt", &n );
    ok = d && tmz_mzt_parse ( d, n, &m ) == EXIT_SUCCESS && m.count == 4 && m.cmtspeed_count == 1
         && !m.items[0].is_cmtspeed && m.items[0].pulses_us[0] == 470
         && m.items[1].is_cmtspeed && m.items[1].offset == 0x402
         && m.items[2].pulses_us[0] == 235 && m.items[3].pulses_us[3] == 139;
    CHECK ( ok, "interkar_2x: CMTSPEED after the first file, 2x for the rest" );
    if ( ok ) tmz_mzt_free ( &m );

    /* useknuté tělo */
    CHECK ( d && tmz_mzt_parse ( d, n - 1, &m ) == EXIT_FAILURE && m.count == 0 && m.items == NULL,
            "truncated body -> error" );
    CHECK ( d && tmz_mzt_parse ( d, 100, &m ) == EXIT_FAILURE, "less than one header -> error" );
    CHECK ( tmz_mzt_parse ( d, 0, &m ) == EXIT_FAILURE, "empty -> error" );
    free ( d );

    /* zbytek kratší než hlavička (CP/M výplň) tolerantní čtení přijme a ohlásí */
    d = load ( "../tstdata/unicmt/interkar_plain.mzt", &n );
    {
        uint8_t *pad = d ? realloc ( d, n + 50 ) : NULL;
        if ( pad ) {
            d = pad;
            memset ( d + n, 0, 50 );
        }
        uint32_t tail = 0;
        CHECK ( pad && tmz_mzt_parse ( d, n + 50, &m ) == EXIT_FAILURE, "strict parse rejects 50 B padding" );
        ok = pad && tmz_mzt_parse_tail ( d, n + 50, &m, &tail ) == EXIT_SUCCESS && m.count == 3 && tail == 50;
        CHECK ( ok, "tolerant parse: 3 files, 50 B tail" );
        if ( ok ) tmz_mzt_free ( &m );
        CHECK ( pad && tmz_mzt_parse_tail ( d, n - 1, &m, &tail ) == EXIT_FAILURE, "tolerant parse: truncated body still an error" );
    }
    free ( d );

    /* jméno CMTSPEED s fsize != 0 je obyčejný soubor */
    uint8_t fake[128 + 4];
    tmz_mzt_build_cmtspeed ( pv[1], fake );
    fake[0x12] = 4;
    memset ( fake + 128, 0xAA, 4 );
    ok = !tmz_mzt_is_cmtspeed ( fake, NULL ) && tmz_mzt_parse ( fake, sizeof ( fake ), &m ) == EXIT_SUCCESS
         && m.count == 1 && !m.items[0].is_cmtspeed && m.items[0].body_size == 4;
    CHECK ( ok, "CMTSPEED name with fsize 4 is a plain file" );
    if ( ok ) tmz_mzt_free ( &m );
    fake[0x12] = 0;
    fake[0] = 0x01;
    CHECK ( !tmz_mzt_is_cmtspeed ( fake, NULL ), "CMTSPEED name with type 01h is not CMTSPEED" );

    /* V5 */
    d = load ( "../tstdata/tmz-v2/v5-mzt-cmtspeed.mzt", &n );
    ok = d && tmz_mzt_parse ( d, n, &m ) == EXIT_SUCCESS && m.count == 3 && m.items[1].is_cmtspeed
         && m.items[2].pulses_us[1] == 247;
    CHECK ( ok, "V5: file, CMTSPEED 2x, file" );
    if ( ok ) tmz_mzt_free ( &m );
    free ( d );

    /* import (Task 3): interkar_2x -> 3 bloky UNICMT, 1x, 2x, 2x */
    d = load ( "../tstdata/unicmt/interkar_2x.mzt", &n );
    st_TZX_BLOCK **blk = NULL;
    uint32_t nb = 0;
    int lead = -1, trail = -1;
    ok = d && tmz_mzt_parse ( d, n, &m ) == EXIT_SUCCESS
         && tmz_mzt_build_unicmt_blocks ( &m, NULL, &blk, &nb, &lead, &trail ) == EXIT_SUCCESS
         && nb == 3 && lead == 0 && trail == 0;
    if ( ok ) {
        static const char *const exp_args[3] = { "gap=short,cmtspeed=470/494/240/278,pause=1000",
                                                 "gap=short,cmtspeed=235/247/120/139,pause=1000",
                                                 "gap=short,cmtspeed=235/247/120/139,pause=1000" };
        for ( uint32_t i = 0; ok && i < 3; i++ ) {
            st_TMZ_MZFILE mf;
            ok = tmz_mzfile_parse ( blk[i], &mf ) == TZX_OK && strcmp ( mf.profile_id, "UNICMT" ) == 0
                 && memcmp ( mf.header, m.items[i == 0 ? 0 : i + 1].header, 128 ) == 0;
            if ( ok ) {
                st_TMZ_TLV_ITER it;
                st_TMZ_TLV t;
                tmz_tlv_iter_init ( &it, mf.block_tlv, mf.block_tlv_length );
                ok = tmz_tlv_iter_next ( &it, &t ) == 1 && t.length == strlen ( exp_args[i] )
                     && memcmp ( t.value, exp_args[i], t.length ) == 0;
                tmz_mzfile_free ( &mf );
            }
        }
        for ( uint32_t i = 0; i < nb; i++ ) tmz_block_free ( blk[i] );
        free ( blk );
        tmz_mzt_free ( &m );
    }
    CHECK ( ok, "import interkar_2x: UNICMT 1x, 2x, 2x, header bytes kept" );
    free ( d );

    /* CMTSPEED na začátku a na konci */
    uint8_t edge[128 + 128 + 4 + 128];
    tmz_mzt_build_cmtspeed ( pv[1], edge );
    memset ( edge + 128, 0, 128 + 4 );
    edge[128] = 0x01;
    edge[128 + 0x12] = 4;
    tmz_mzt_build_cmtspeed ( pv[2], edge + 128 + 128 + 4 );
    ok = tmz_mzt_parse ( edge, sizeof ( edge ), &m ) == EXIT_SUCCESS
         && tmz_mzt_build_unicmt_blocks ( &m, NULL, &blk, &nb, &lead, &trail ) == EXIT_SUCCESS
         && nb == 1 && lead == 1 && trail == 1;
    if ( ok ) {
        st_TMZ_MZFILE mf;
        ok = tmz_mzfile_parse ( blk[0], &mf ) == TZX_OK;
        if ( ok ) {
            static st_MZCMT_FM_PARAMS pe1;
            st_TMZ_PART_COMMON ce1;
            ok = tmz_part_fm_decode ( mf.parts[0].tlv, mf.parts[0].tlv_length, 3546895, &pe1, &ce1 ) == TMZ_PART_OK
                 && pe1.long_first == 470;   /* úvodní CMTSPEED se nepoužije */
            tmz_mzfile_free ( &mf );
        }
        tmz_block_free ( blk[0] );
        free ( blk );
        tmz_mzt_free ( &m );
    }
    CHECK ( ok, "leading CMTSPEED ignored, trailing CMTSPEED reported" );

    /* export (Task 4): tam a zpět bajtově shodné s ukázkami UniCMT */
    int w = 0;
    CHECK ( roundtrip ( "../tstdata/unicmt/interkar_2x.mzt", "../tstdata/unicmt/interkar_2x.mzt", TMZ_MZT_UNICMT, &w ) && w == 0,
            "interkar_2x -> TMZ -> --mzt-unicmt byte-identical" );
    CHECK ( roundtrip ( "../tstdata/unicmt/interkar_3x.mzt", "../tstdata/unicmt/interkar_3x.mzt", TMZ_MZT_UNICMT, &w ) && w == 0,
            "interkar_3x -> TMZ -> --mzt-unicmt byte-identical" );
    CHECK ( roundtrip ( "../tstdata/unicmt/interkar_2x.mzt", "../tstdata/unicmt/interkar_plain.mzt", TMZ_MZT_CLASSIC, &w ) && w == 2,
            "interkar_2x -> TMZ -> --mzt = interkar_plain, 2 warnings" );
    CHECK ( roundtrip ( "../tstdata/unicmt/interkar_plain.mzt", "../tstdata/unicmt/interkar_plain.mzt", TMZ_MZT_UNICMT, &w ) && w == 0,
            "plain -> TMZ -> --mzt-unicmt without CMTSPEED" );
    CHECK ( roundtrip ( "../tstdata/tmz-v2/v5-mzt-cmtspeed.mzt", "../tstdata/tmz-v2/v5-mzt-cmtspeed.mzt", TMZ_MZT_UNICMT, &w ),
            "V5 round trip" );

    /* návrat na 1x vloží 1xspeed; stejná rychlost za sebou jen jednou */
    {
        uint8_t one[128 + 4];
        memset ( one, 0, sizeof ( one ) );
        one[0] = 0x01;
        one[0x12] = 4;
        st_TMZ_PROFILE_ARGS a;
        tmz_profile_args_init ( &a );
        st_TZX_BLOCK *bb[4];
        static const int spd[4] = { 1, 2, 2, 1 };
        for ( int i = 0; i < 4; i++ ) {
            st_MZF_HEADER h;
            memcpy ( &h, one, 128 );
            h.fsize = 4;
            memcpy ( a.cmtspeed, pv[spd[i] - 1], sizeof ( a.cmtspeed ) );
            bb[i] = tmz_profile_build_block ( "UNICMT", &a, &h, one + 128, 4, NULL );
        }
        if ( !bb[0] || !bb[1] || !bb[2] || !bb[3] ) {
            CHECK ( 0, "export: UNICMT blocks built" );
            for ( int i = 0; i < 4; i++ ) tmz_block_free ( bb[i] );
            goto export_done;
        }
        st_TZX_FILE f;
        memset ( &f, 0, sizeof ( f ) );
        st_TZX_BLOCK arr[4] = { *bb[0], *bb[1], *bb[2], *bb[3] };
        f.blocks = arr;
        f.block_count = 4;
        uint8_t *out = NULL;
        uint32_t on = 0;
        ok = tmz_mzt_export ( &f, TMZ_MZT_UNICMT, &out, &on, NULL, NULL ) == EXIT_SUCCESS
             && on == 4 * 132 + 2 * 128 && tmz_mzt_is_cmtspeed ( out + 132, NULL )
             && !tmz_mzt_is_cmtspeed ( out + 132 + 128 + 132, NULL )
             && tmz_mzt_is_cmtspeed ( out + 132 + 128 + 2 * 132, NULL ) && out[132 + 128 + 2 * 132 + 0x30] == ( 470 & 0xFF );
        CHECK ( ok, "export: 1x, CMTSPEED 2x, 2x, 2x, CMTSPEED 1x, 1x" );
        free ( out );

        /* první soubor 2x -> chyba */
        f.blocks = arr + 1;
        f.block_count = 3;
        out = NULL;
        CHECK ( tmz_mzt_export ( &f, TMZ_MZT_UNICMT, &out, &on, NULL, NULL ) == EXIT_FAILURE && out == NULL,
                "unicmt export: first file not 1x -> error" );
        for ( int i = 0; i < 4; i++ ) tmz_block_free ( bb[i] );
    }
export_done:

    /* IC-10.2-NORMAL je záznam ROM, rychlost z bd */
    {
        uint8_t one[128 + 4];
        memset ( one, 0, sizeof ( one ) );
        one[0] = 0x01;
        one[0x12] = 4;
        st_MZF_HEADER h;
        memcpy ( &h, one, 128 );
        h.fsize = 4;
        st_TMZ_PROFILE_ARGS a;
        tmz_profile_args_init ( &a );
        a.bd = 1200;
        st_TZX_BLOCK *i1 = tmz_profile_build_block ( "IC-10.2-NORMAL", &a, &h, one + 128, 4, NULL );
        a.bd = 2400;
        st_TZX_BLOCK *i2 = tmz_profile_build_block ( "IC-10.2-NORMAL", &a, &h, one + 128, 4, NULL );
        st_TMZ_MZT_BLOCK_INFO info;
        int ir = i1 ? tmz_mzt_block_info ( i1, &info ) : -1;
        ok = i2 && ir == 1 && info.rom_structure && memcmp ( info.pulses_us, pv[0], sizeof ( info.pulses_us ) ) == 0;
        if ( ir == 1 ) tmz_mzt_block_info_free ( &info );
        CHECK ( ok, "IC-10.2-NORMAL 1200 = ROM structure 1x" );
        if ( i1 && i2 ) {
            st_TZX_FILE f;
            memset ( &f, 0, sizeof ( f ) );
            st_TZX_BLOCK arr[2] = { *i1, *i2 };
            f.blocks = arr;
            f.block_count = 2;
            uint8_t *out = NULL;
            uint32_t on = 0;
            ok = tmz_mzt_export ( &f, TMZ_MZT_UNICMT, &out, &on, NULL, NULL ) == EXIT_SUCCESS && on == 2 * 132 + 128
                 && tmz_mzt_is_cmtspeed ( out + 132, NULL ) && out[132 + 0x30] == 235;
            free ( out );
            CHECK ( ok, "IC-10.2-NORMAL 1200 + 2400 -> UniCMT MZT with CMTSPEED 2x" );
        }
        if ( i1 ) tmz_block_free ( i1 );
        if ( i2 ) tmz_block_free ( i2 );
    }

    /*
     * Rychlost bloků v1.0 a 0x46 ROM/LEGACY pro export:
     * 0x40 = 1x, 0x41 NORMAL 2:1 = 2x, LEGACY normal 3:1 = 3x, ROM-800-NOMINAL = 1x
     */
    {
        static uint8_t body4[4] = { 9, 8, 7, 6 };
        st_MZF_HEADER h;
        memset ( &h, 0, sizeof ( h ) );
        h.ftype = 0x01;
        h.fname.name[0] = 'A';
        h.fname.terminator = 0x0D;
        h.fsize = 4;
        h.fstrt = h.fexec = 0x1200;
        st_TMZ_MZ_TURBO_DATA td;
        memset ( &td, 0, sizeof ( td ) );
        td.format = TMZ_FORMAT_NORMAL;
        td.speed = CMTSPEED_2_1;
        td.pulseset = TMZ_PULSESET_800;
        td.mzf_header = h;
        st_TMZ_PROFILE_ARGS a;
        tmz_profile_args_init ( &a );
        a.legacy_format = TMZ_LEGACY_NORMAL;
        a.speed = CMTSPEED_3_1;
        st_TZX_BLOCK *v[4] = {
            tmz_block_create_mz_standard ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 0, &h, body4, 4 ),
            tmz_block_create_mz_turbo ( &td, body4, 4 ),
            tmz_profile_build_block ( "TAPEMZ-V1-LEGACY", &a, &h, body4, 4, NULL ),
            tmz_profile_build_block ( "ROM-800-NOMINAL", NULL, &h, body4, 4, NULL ),
        };
        ok = v[0] && v[1] && v[2] && v[3];
        if ( ok ) {
            st_TZX_BLOCK arr[4] = { *v[0], *v[1], *v[2], *v[3] };
            st_TZX_FILE f;
            memset ( &f, 0, sizeof ( f ) );
            f.blocks = arr;
            f.block_count = 4;
            uint8_t *out = NULL;
            uint32_t on = 0;
            ok = tmz_mzt_export ( &f, TMZ_MZT_UNICMT, &out, &on, NULL, NULL ) == EXIT_SUCCESS
                 && tmz_mzt_parse ( out, on, &m ) == EXIT_SUCCESS && m.count == 7 && m.cmtspeed_count == 3
                 && m.items[1].is_cmtspeed && memcmp ( m.items[1].pulses_us, pv[1], 8 ) == 0
                 && m.items[3].is_cmtspeed && memcmp ( m.items[3].pulses_us, pv[2], 8 ) == 0
                 && m.items[5].is_cmtspeed && memcmp ( m.items[5].pulses_us, pv[0], 8 ) == 0
                 && m.items[6].body_size == 4 && memcmp ( m.items[6].body, body4, 4 ) == 0;
            if ( ok ) tmz_mzt_free ( &m );
            free ( out );
            CHECK ( ok, "export v1.0 and ROM/LEGACY blocks: 1x, CMTSPEED 2x, CMTSPEED 3x, CMTSPEED 1x" );
            w = 0;
            out = NULL;
            CHECK ( tmz_mzt_export ( &f, TMZ_MZT_CLASSIC, &out, &on, count_warn, &w ) == EXIT_SUCCESS && on == 4 * 132
                    && w == 2, "classic export of the same blocks: 2 timing warnings" );
            free ( out );
        } else {
            CHECK ( 0, "export v1.0 and ROM/LEGACY blocks: blocks built" );
        }
        for ( int i = 0; i < 4; i++ ) tmz_block_free ( v[i] );
    }

    /* blok bez struktury ROM: unicmt chyba, classic varování */
    {
        st_TMZ_PROFILE_ARGS a;
        tmz_profile_args_init ( &a );
        a.speed = CMTSPEED_2_1;
        uint8_t body4[4] = { 1, 2, 3, 4 };
        st_MZF_HEADER h;
        memset ( &h, 0, sizeof ( h ) );
        h.ftype = 0x01;
        h.fsize = 4;
        st_TZX_BLOCK *tc = tmz_profile_build_block ( "TC-1.21", &a, &h, body4, 4, NULL );
        st_TZX_FILE f;
        memset ( &f, 0, sizeof ( f ) );
        f.blocks = tc;
        f.block_count = 1;
        uint8_t *out = NULL;
        uint32_t on = 0;
        CHECK ( tc && tmz_mzt_export ( &f, TMZ_MZT_UNICMT, &out, &on, NULL, NULL ) == EXIT_FAILURE,
                "unicmt export rejects TC-1.21" );
        w = 0;
        CHECK ( tc && tmz_mzt_export ( &f, TMZ_MZT_CLASSIC, &out, &on, count_warn, &w ) == EXIT_SUCCESS && on == 132 && w == 1,
                "classic export of TC-1.21 with a warning" );
        free ( out );
        tmz_block_free ( tc );
    }

    printf ( "\n=== TMZ MZT Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
