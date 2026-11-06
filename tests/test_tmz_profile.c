/**
 * @file   test_tmz_profile.c
 * @brief  Registr profilů (spec kap. 8): bloky sestavené z emutest.mzf
 *         přehrané generátorem FM proti záznamům z emulátoru (tstdata/ref),
 *         parametry, PROFILE_ARGS, odmítnutí neplatných parametrů.
 *         Spouštět z build/.
 * Bez tstdata/ vypíše SKIP a skončí 0 (tests/tstdata_skip.h).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/endianity/endianity.h"
#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_mzfile.h"
#include "libs/tmz/tmz_tlv.h"
#include "libs/tmz/tmz_profile.h"
#include "tmz_ref_compare.h"
#include "tstdata_skip.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )

/** @brief Hlavička testovacího MZF v pořadí hostitele. */
static st_MZF_HEADER g_hdr;
/** @brief Tělo testovacího MZF. */
static uint8_t g_body[1024];


/**
 * @brief Načte tstdata/ref/emutest.mzf.
 * @return EXIT_SUCCESS, nebo EXIT_FAILURE.
 */
static int load_emutest ( void ) {
    uint8_t raw[1152];
    FILE *f = fopen ( "../tstdata/ref/emutest.mzf", "rb" );
    if ( !f ) return EXIT_FAILURE;
    size_t n = fread ( raw, 1, sizeof ( raw ), f );
    fclose ( f );
    if ( n != sizeof ( raw ) ) return EXIT_FAILURE;
    memcpy ( &g_hdr, raw, 128 );
    /* jen pořadí bajtů: mzf_header_items_correction() by přepsal 11h na 0Dh (ROM záznam má 00h) */
    g_hdr.fsize = endianity_bswap16_LE ( g_hdr.fsize );
    g_hdr.fstrt = endianity_bswap16_LE ( g_hdr.fstrt );
    g_hdr.fexec = endianity_bswap16_LE ( g_hdr.fexec );
    memcpy ( g_body, raw + 128, 1024 );
    return EXIT_SUCCESS;
}


/**
 * @brief Sestaví blok profilem a porovná ho se záznamem.
 * @param id Profil.
 * @param args Parametry.
 * @param hdr Hlavička (pořadí hostitele).
 * @param hwh Záznam .hwh.
 * @param exact 1 = shoda na T, 0 = meze TurboCopy.
 */
static void build_and_compare ( const char *id, const st_TMZ_PROFILE_ARGS *args, const st_MZF_HEADER *hdr,
                                const char *hwh, int exact ) {
    char label[200];
    en_TMZ_PROFILE_ERROR pe;
    st_TZX_BLOCK *b = tmz_profile_build_block ( id, args, hdr, g_body, 1024, &pe );
    snprintf ( label, sizeof ( label ), "%s vs %s", id, hwh );
    CHECK ( b != NULL, label );
    if ( !b ) return;
    ref_compare_block ( b, hwh, exact, 0, &g_failures, label );
    tmz_block_free ( b );
}


int main ( void ) {
    if ( tstdata_skip ( "test_tmz_profile" ) ) return 0;
    if ( load_emutest () != EXIT_SUCCESS ) {
        printf ( "FAIL: emutest.mzf\n" );
        return 1;
    }

    CHECK ( tmz_profile_find ( "TC-1.21" ) && tmz_profile_find ( "ROM-800-NOMINAL" ) && tmz_profile_find ( "ROM-800-EXACT" )
            && !tmz_profile_find ( "X-NOPE" ) && tmz_profile_count () == 11 && tmz_profile_get ( 11 ) == NULL,
            "registry has 11 profiles" );

    /* CPM-TAPE (rodina MANCHESTER) */
    {
        st_TMZ_PROFILE_ARGS ca;
        en_TMZ_PROFILE_ERROR ce;
        CHECK ( tmz_profile_find ( "PEZIK-2.0A" ) && tmz_profile_find ( "IC-10.2-CPM" ), "CPM-TAPE profiles registered" );
        /* Intercopy zapsal hlavičku, jak ji načetl (11h = 0Dh, jako TC/IC výše) */
        st_MZF_HEADER ic_hdr = g_hdr;
        ( ( uint8_t* ) &ic_hdr )[0x11] = 0x0D;
        tmz_profile_args_init ( &ca );
        build_and_compare ( "IC-10.2-CPM", &ca, &ic_hdr, "../tstdata/ref/ic102-cpm-2400-emutest.hwh", 0 );
        /* další rychlosti: záznamy ze zdrojové pásky ROM (11h hlavičky = 00h jako emutest.mzf) */
        static const struct { uint16_t bd; const char *ref; } icr[] = {
            { 1500, "../tstdata/ref/ic102-cpm-1500-emutest.hwh" },
            { 2800, "../tstdata/ref/ic102-cpm-2800-emutest.hwh" },
            { 3200, "../tstdata/ref/ic102-cpm-3200-emutest.hwh" },
            { 3600, "../tstdata/ref/ic102-cpm-3600-emutest.hwh" },
        };
        for ( size_t i = 0; i < sizeof ( icr ) / sizeof ( icr[0] ); i++ ) {
            tmz_profile_args_init ( &ca );
            ca.bd = icr[i].bd;
            build_and_compare ( "IC-10.2-CPM", &ca, &g_hdr, icr[i].ref, 0 );
        }
        /*
         * Pauza za hlavičkou: porovnání po segmentech ji vynechá. V záznamu je jedna
         * dlouhá půlvlna LOW = 2. půlka koncového bitu (h) + pauza + 1. půlka pilotního
         * bitu 0 těla (h); musí se rovnat 2h + pauza části hlavičky (v T).
         */
        static const char *const icp[5] = { "../tstdata/ref/ic102-cpm-1500-emutest.hwh",
                                            "../tstdata/ref/ic102-cpm-2400-emutest.hwh",
                                            "../tstdata/ref/ic102-cpm-2800-emutest.hwh",
                                            "../tstdata/ref/ic102-cpm-3200-emutest.hwh",
                                            "../tstdata/ref/ic102-cpm-3600-emutest.hwh" };
        static const uint16_t icbd[5] = { 1500, 2400, 2800, 3200, 3600 };
        for ( size_t i = 0; i < 5; i++ ) {
            tmz_profile_args_init ( &ca );
            ca.bd = icbd[i];
            st_TZX_BLOCK *bp = tmz_profile_build_block ( "IC-10.2-CPM", &ca, i == 1 ? &ic_hdr : &g_hdr, g_body, 1024, &ce );
            st_TMZ_MZFILE mf;
            st_MZCMT_MANCH_PARAMS mp;
            st_TMZ_PART_COMMON pc;
            st_REF_ARR r = { 0 };
            int fl = 0;
            double longest = 0.0;
            int ok = bp && tmz_mzfile_parse ( bp, &mf ) == TZX_OK;
            if ( ok ) {
                ok = mf.part_count == 2 && tmz_part_manchester_decode ( mf.parts[0].tlv, mf.parts[0].tlv_length, 3546895,
                                                                         &mp, &pc ) == TMZ_PART_OK;
                tmz_mzfile_free ( &mf );
            }
            if ( ok && ref_load_hwh ( icp[i], &r, &fl ) == EXIT_SUCCESS ) {
                for ( uint32_t k = 0; k < r.n; k++ ) if ( r.v[k] > longest ) longest = r.v[k];
            } else {
                ok = 0;
            }
            free ( r.v );
            char label[96];
            snprintf ( label, sizeof ( label ), "IC-10.2-CPM %u: header pause matches the reference (%.0f T)",
                       icbd[i], longest );
            CHECK ( ok && mp.timebase == 3546895 && fabs ( longest - ( 2.0 * mp.half1[0] + mp.pause ) ) < 0.5, label );
            tmz_block_free ( bp );
        }
        {
            /* PROFILE_ARGS bloku se znovu načtou (tmzinfo, UniCMT export): bd 1500 a 3600 platí */
            static const char *const txt[2] = { "bd=1500,pause=1000", "bd=3600,pause=1000" };
            int okp = 1;
            for ( int k = 0; k < 2; k++ ) {
                st_TMZ_PROFILE_ARGS pa;
                okp &= tmz_profile_args_parse ( txt[k], ( uint16_t ) strlen ( txt[k] ), &pa ) == EXIT_SUCCESS
                       && pa.bd == ( k ? 3600 : 1500 );
            }
            st_TMZ_PROFILE_ARGS pb;
            okp &= tmz_profile_args_parse ( "bd=2000", 7, &pb ) != EXIT_SUCCESS;
            CHECK ( okp, "PROFILE_ARGS bd=1500 and bd=3600 parse, bd=2000 does not" );
        }
        {
            /* záporná kontrola: 3200 se od záznamu 3600 liší */
            tmz_profile_args_init ( &ca );
            ca.bd = 3200;
            st_TZX_BLOCK *bn = tmz_profile_build_block ( "IC-10.2-CPM", &ca, &g_hdr, g_body, 1024, &ce );
            int neg = 0;
            if ( bn ) ref_compare_block ( bn, icr[3].ref, 0, 0, &neg, "negative" );
            CHECK ( bn && neg > 0, "IC-10.2-CPM 3200 does not match the 3600 reference" );
            tmz_block_free ( bn );
        }
        static const struct { uint16_t bd; const char *ref; } pz[] = {
            { 2400, "../tstdata/ref/pezik20a-2400-emutest-model.hwh" },
            { 3200, "../tstdata/ref/pezik20a-3200-emutest-model.hwh" },
        };
        for ( size_t i = 0; i < 2; i++ ) {
            tmz_profile_args_init ( &ca );
            ca.bd = pz[i].bd;
            st_TZX_BLOCK *bp = tmz_profile_build_block ( "PEZIK-2.0A", &ca, &g_hdr, g_body, 1024, &ce );
            char label[96];
            snprintf ( label, sizeof ( label ), "PEZIK-2.0A bd=%u vs model (exact)", pz[i].bd );
            CHECK ( bp != NULL, label );
            if ( bp ) ref_compare_concat ( bp, pz[i].ref, 1, &g_failures, label );
            tmz_block_free ( bp );
        }
        /* test umí selhat: 2400 proti referenci 3200 */
        tmz_profile_args_init ( &ca );
        ca.bd = 2400;
        st_TZX_BLOCK *bn = tmz_profile_build_block ( "PEZIK-2.0A", &ca, &g_hdr, g_body, 1024, &ce );
        int neg = 0;
        if ( bn ) ref_compare_concat ( bn, pz[1].ref, 1, &neg, "negative" );
        CHECK ( bn && neg > 0, "Pezik 2400 does not match the 3200 reference" );
        tmz_block_free ( bn );
        /* ARGS */
        static const struct { const char *id; uint16_t bd; en_CMTSPEED speed; } bad[] = {
            { "PEZIK-2.0A", 2800, CMTSPEED_NONE }, { "IC-10.2-CPM", 2000, CMTSPEED_NONE },
            { "IC-10.2-CPM", 1200, CMTSPEED_NONE }, { "PEZIK-2.0A", 0, CMTSPEED_2_1 }, { "IC-10.2-CPM", 0, CMTSPEED_2_1 },
        };
        int okb = 1;
        for ( size_t i = 0; i < sizeof ( bad ) / sizeof ( bad[0] ); i++ ) {
            tmz_profile_args_init ( &ca );
            ca.bd = bad[i].bd;
            ca.speed = bad[i].speed;
            st_TZX_BLOCK *bx = tmz_profile_build_block ( bad[i].id, &ca, &g_hdr, g_body, 1024, &ce );
            if ( bx || ce != TMZ_PROFILE_ERROR_ARGS ) {
                printf ( "  %s bd=%u speed=%d accepted\n", bad[i].id, bad[i].bd, ( int ) bad[i].speed );
                okb = 0;
            }
            tmz_block_free ( bx );
        }
        CHECK ( okb, "CPM-TAPE profiles reject other speeds" );
        /* Pezik: tělo nad 16 KB odmítne (díly TAPE.COM), 16 KB projde */
        static uint8_t big[16385];
        st_MZF_HEADER bh = g_hdr;
        bh.fsize = 16384;
        tmz_profile_args_init ( &ca );
        st_TZX_BLOCK *b16 = tmz_profile_build_block ( "PEZIK-2.0A", &ca, &bh, big, 16384, &ce );
        CHECK ( b16 != NULL && ce == TMZ_PROFILE_OK, "Pezik body 16384 B accepted" );
        tmz_block_free ( b16 );
        bh.fsize = 16385;
        b16 = tmz_profile_build_block ( "PEZIK-2.0A", &ca, &bh, big, 16385, &ce );
        CHECK ( b16 == NULL && ce == TMZ_PROFILE_ERROR_BUILD, "Pezik body 16385 B rejected" );
        st_TZX_BLOCK *bi = tmz_profile_build_block ( "IC-10.2-CPM", &ca, &bh, big, 16385, &ce );
        CHECK ( bi != NULL, "Intercopy body 16385 B accepted (one block)" );
        tmz_block_free ( bi );
        /* fsize 0: jen část hlavičky */
        bh.fsize = 0;
        for ( int k = 0; k < 2; k++ ) {
            st_TZX_BLOCK *b0 = tmz_profile_build_block ( k ? "IC-10.2-CPM" : "PEZIK-2.0A", &ca, &bh, NULL, 0, &ce );
            st_TMZ_MZFILE m0;
            int one = b0 && tmz_mzfile_parse ( b0, &m0 ) == TZX_OK;
            if ( one ) {
                one = m0.part_count == 1 && m0.parts[0].family == TMZ_FAMILY_MANCHESTER;
                tmz_mzfile_free ( &m0 );
            }
            CHECK ( one, k ? "IC-10.2-CPM fsize 0: one MANCHESTER part" : "PEZIK-2.0A fsize 0: one MANCHESTER part" );
            tmz_block_free ( b0 );
        }
    }

    /* BASIC-1Z016 - registr, velikost těla, parametry */
    {
        static uint8_t chunks[255 * 258];
        st_MZF_HEADER bh;
        memset ( &bh, 0, sizeof ( bh ) );
        bh.ftype = 0x04;
        st_TMZ_PROFILE_ARGS ba;
        tmz_profile_args_init ( &ba );
        en_TMZ_PROFILE_ERROR be;
        const st_TMZ_PROFILE *bp = tmz_profile_find ( "BASIC-1Z016" );
        CHECK ( bp && strcmp ( bp->confidence, "emulator-verified" ) == 0, "BASIC-1Z016 registered, emulator-verified" );
        st_TZX_BLOCK *bb = tmz_profile_build_block ( "BASIC-1Z016", &ba, &bh, chunks, 258, &be );
        CHECK ( bb && be == TMZ_PROFILE_OK, "BASIC 1 chunk" );
        tmz_block_free ( bb );
        bb = tmz_profile_build_block ( "BASIC-1Z016", &ba, &bh, chunks, 254 * 258, &be );
        CHECK ( bb && be == TMZ_PROFILE_OK, "BASIC 254 chunks" );
        tmz_block_free ( bb );
        CHECK ( !tmz_profile_build_block ( "BASIC-1Z016", &ba, &bh, chunks, 255 * 258, &be ) && be == TMZ_PROFILE_ERROR_BUILD,
                "BASIC 255 chunks -> BUILD" );
        CHECK ( !tmz_profile_build_block ( "BASIC-1Z016", &ba, &bh, chunks, 257, &be ) && be == TMZ_PROFILE_ERROR_BUILD,
                "BASIC 257 B -> BUILD" );
        CHECK ( !tmz_profile_build_block ( "BASIC-1Z016", &ba, &bh, NULL, 0, &be ) && be == TMZ_PROFILE_ERROR_BUILD,
                "BASIC empty body -> BUILD" );
        ba.speed = CMTSPEED_2_1;
        CHECK ( !tmz_profile_build_block ( "BASIC-1Z016", &ba, &bh, chunks, 258, &be ) && be == TMZ_PROFILE_ERROR_ARGS,
                "BASIC speed -> ARGS" );
        tmz_profile_args_init ( &ba );
        ba.bd = 2400;
        CHECK ( !tmz_profile_build_block ( "BASIC-1Z016", &ba, &bh, chunks, 258, &be ) && be == TMZ_PROFILE_ERROR_ARGS,
                "BASIC bd -> ARGS" );

        /*
         * Pauzy: záznam z emulátoru obsahuje i dobu stojícího motoru, srovnání
         * s referencí pauzy nehlídá. Za hlavičkou a nekoncovými kusy 6 610 752 T
         * (ticho 6 612 558 T minus END_L 1806 T), za posledním
         * kusem ARGS pause: 1234 ms = round(1234 * 3546895 / 1000) = 4 376 868 T.
         */
        tmz_profile_args_init ( &ba );
        ba.pause_ms = 1234;
        st_TZX_BLOCK *bpz = tmz_profile_build_block ( "BASIC-1Z016", &ba, &bh, chunks, 3 * 258, &be );
        st_TMZ_MZFILE mz;
        int pok = bpz && tmz_mzfile_parse ( bpz, &mz ) == TZX_OK;
        if ( pok ) {
            pok = ( mz.part_count == 4 );
            for ( uint8_t k = 0; pok && k < mz.part_count; k++ ) {
                static st_MZCMT_FM_PARAMS pp;
                st_TMZ_PART_COMMON pc;
                pok = tmz_part_fm_decode ( mz.parts[k].tlv, mz.parts[k].tlv_length, 3546895, &pp, &pc ) == TMZ_PART_OK;
                uint32_t want = ( k + 1 == mz.part_count ) ? 4376868u : 6610752u;
                if ( pok && ( pp.pause != want || pp.pause_level != 0 ) ) {
                    printf ( "  part %u: pause %u level %u, expected %u level 0\n", k, pp.pause, pp.pause_level, want );
                    pok = 0;
                }
            }
            tmz_mzfile_free ( &mz );
        }
        CHECK ( pok, "BASIC pauses: 6610752 T between segments, ARGS pause after last chunk, LOW" );
        tmz_block_free ( bpz );
    }

    /* ROM-800-EXACT gap=rom: shoda na T s ROM SAVE */
    st_TMZ_PROFILE_ARGS a;
    tmz_profile_args_init ( &a );
    a.gap_rom = 1;
    build_and_compare ( "ROM-800-EXACT", &a, &g_hdr, "../tstdata/ref/rom800-save-emutest.hwh", 1 );

    /*
     * TC-1.21 Loader N. Pásku, kterou TurboCopy v emulátoru načetl, vyrobily
     * naše nástroje (mzf2tmz + tmz2wav); knihovna mzf při čtení MZF nastaví
     * fname.terminator (11h) na 0Dh, takže TurboCopy uložil hlavičku s 0Dh.
     */
    st_MZF_HEADER tc_hdr = g_hdr;
    ( ( uint8_t* ) &tc_hdr )[0x11] = 0x0D;
    static const struct { en_CMTSPEED s; const char *ref; } tc[] = {
        { CMTSPEED_2_1, "../tstdata/ref/tc121-2to1-loaderN-emutest.hwh" },
        { CMTSPEED_7_3, "../tstdata/ref/tc121-7to3-loaderN-emutest.hwh" },
        { CMTSPEED_8_3, "../tstdata/ref/tc121-8to3-loaderN-emutest.hwh" },
        { CMTSPEED_3_2, "../tstdata/ref/tc121-3to2-loaderN-emutest.hwh" },
        { CMTSPEED_9_7, "../tstdata/ref/tc121-9to7-loaderN-emutest.hwh" },
    };
    for ( size_t i = 0; i < sizeof ( tc ) / sizeof ( tc[0] ); i++ ) {
        tmz_profile_args_init ( &a );
        a.speed = tc[i].s;
        a.loader = 0;
        build_and_compare ( "TC-1.21", &a, &tc_hdr, tc[i].ref, 0 );
    }
    /* TC-1.21 Loader Y 3:1 (předzavaděč z mzcmt_turbo; tělo programu jako u Loader N) */
    tmz_profile_args_init ( &a );
    a.speed = CMTSPEED_3_1;
    build_and_compare ( "TC-1.21", &a, &tc_hdr, "../tstdata/ref/tc121-3to1-loaderY-emutest.hwh", 0 );

    /*
     * Intercopy V10.2: 3 režimy x 5 rychlostí proti záznamům z emulátoru.
     * Zdrojová páska prošla mzf2tmz, hlavička má na 11h 0Dh (jako u TC).
     */
    {
        static const char *const modes[3][2] = { { "IC-10.2-NORMAL", "normal" }, { "IC-10.2-TURBO", "turbo" },
                                                  { "IC-10.2-FASTIPL", "fastipl" } };
        static const uint16_t bds[5] = { 600, 1200, 2400, 2800, 3200 };
        for ( int mi = 0; mi < 3; mi++ ) {
            for ( int si = 0; si < 5; si++ ) {
                char ref[128];
                snprintf ( ref, sizeof ( ref ), "../tstdata/ref/ic102-%s-%u-emutest.hwh", modes[mi][1], bds[si] );
                tmz_profile_args_init ( &a );
                a.bd = bds[si];
                build_and_compare ( modes[mi][0], &a, &tc_hdr, ref, 0 );
            }
        }
        en_TMZ_PROFILE_ERROR pe2;
        /* srovnání umí selhat: 1200 proti záznamu 2400 */
        {
            int neg = 0;
            tmz_profile_args_init ( &a );
            a.bd = 1200;
            st_TZX_BLOCK *bn = tmz_profile_build_block ( "IC-10.2-NORMAL", &a, &tc_hdr, g_body, 1024, &pe2 );
            if ( bn ) {
                printf ( "(expected failures below)\n" );
                ref_compare_block ( bn, "../tstdata/ref/ic102-normal-2400-emutest.hwh", 0, 0, &neg, "negative" );
                tmz_block_free ( bn );
            }
            CHECK ( bn && neg > 0, "IC 1200 does not match the 2400 recording" );
        }
        tmz_profile_args_init ( &a );
        a.bd = 1000;
        CHECK ( tmz_profile_build_block ( "IC-10.2-NORMAL", &a, &g_hdr, g_body, 1024, &pe2 ) == NULL
                && pe2 == TMZ_PROFILE_ERROR_ARGS, "IC rejects bd 1000" );
        tmz_profile_args_init ( &a );
        a.speed = CMTSPEED_2_1;
        CHECK ( tmz_profile_build_block ( "IC-10.2-TURBO", &a, &g_hdr, g_body, 1024, &pe2 ) == NULL
                && pe2 == TMZ_PROFILE_ERROR_ARGS, "IC rejects speed=" );
        CHECK ( tmz_profile_args_parse ( "bd=2400", 7, &a ) == EXIT_SUCCESS && a.bd == 2400, "bd=2400 parsed" );
        CHECK ( tmz_profile_args_parse ( "bd=1000", 7, &a ) == EXIT_FAILURE, "bd=1000 rejected by parser" );
        tmz_profile_args_init ( &a );
        a.bd = 2400;
        st_TZX_BLOCK *bi = tmz_profile_build_block ( "IC-10.2-FASTIPL", &a, &g_hdr, g_body, 1024, &pe2 );
        st_TMZ_MZFILE mi;
        int oki = bi && tmz_mzfile_parse ( bi, &mi ) == TZX_OK;
        if ( oki ) {
            st_TMZ_TLV_ITER it;
            st_TMZ_TLV t;
            static st_MZCMT_FM_PARAMS pf;
            st_TMZ_PART_COMMON cf;
            static const char exp_ic[] = "bd=2400,pause=1000";
            tmz_tlv_iter_init ( &it, mi.block_tlv, mi.block_tlv_length );
            oki = tmz_tlv_iter_next ( &it, &t ) == 1 && t.length == strlen ( exp_ic ) && memcmp ( t.value, exp_ic, t.length ) == 0
                  && mi.part_count == 2 && mi.parts[0].source == TMZ_SOURCE_INLINE && mi.parts[0].inline_data[0] == 0xBB
                  && mi.parts[0].inline_data[0x19] == 32
                  && tmz_part_fm_decode ( mi.parts[1].tlv, mi.parts[1].tlv_length, 3546895, &pf, &cf ) == TMZ_PART_OK
                  && cf.has_reader && cf.reader_routine == 2 && cf.reader_n == 32 && pf.pause_level == 1;
            tmz_mzfile_free ( &mi );
        }
        CHECK ( oki, "IC-10.2-FASTIPL 2400: args, $BB header readpoint 32, READER routine 2, pause HIGH" );
        if ( bi ) tmz_block_free ( bi );
    }

    /* odmítnutí */
    en_TMZ_PROFILE_ERROR pe;
    tmz_profile_args_init ( &a );
    a.speed = CMTSPEED_25_14;
    CHECK ( tmz_profile_build_block ( "TC-1.21", &a, &g_hdr, g_body, 1024, &pe ) == NULL && pe == TMZ_PROFILE_ERROR_ARGS,
            "TC-1.21 rejects 25:14" );
    a.speed = CMTSPEED_2_1_CPM;
    CHECK ( tmz_profile_build_block ( "TC-1.21", &a, &g_hdr, g_body, 1024, &pe ) == NULL && pe == TMZ_PROFILE_ERROR_ARGS,
            "TC-1.21 rejects 2:1 CPM" );
    /*
     * TC-1.21 pro poměr TurboCopy mimo en_CMTSPEED: S = 71 + floor(480*b/a), tón podle
     * měření v emulátoru (počet pulzů tónu), čtecí bod předzavaděče floor(82*b/a).
     * 9:4: S 284, tón 12307, N 36; 1:2: S 1031, tón 3392, N 164 (bajt 4Bh záznamu).
     */
    {
        static const struct { const char *txt; unsigned s; uint32_t tone; int part; int reader_n; } tcr[] = {
            { "speed=9:4,loader=Y", 284, 12307, 2, 36 },
            { "speed=1:2,loader=N", 1031, 3392, 0, -1 },
            { "speed=5/6,loader=N", 647, 5403, 0, -1 },
        };
        for ( size_t i = 0; i < sizeof ( tcr ) / sizeof ( tcr[0] ); i++ ) {
            int oki = tmz_profile_args_parse ( tcr[i].txt, strlen ( tcr[i].txt ), &a ) == EXIT_SUCCESS;
            st_TZX_BLOCK *bt = oki ? tmz_profile_build_block ( "TC-1.21", &a, &g_hdr, g_body, 1024, &pe ) : NULL;
            st_TMZ_MZFILE mt;
            oki = bt && tmz_mzfile_parse ( bt, &mt ) == TZX_OK;
            if ( oki ) {
                static st_MZCMT_FM_PARAMS pt;
                st_TMZ_PART_COMMON ct;
                oki = tmz_part_fm_decode ( mt.parts[tcr[i].part].tlv, mt.parts[tcr[i].part].tlv_length, 3546895,
                                           &pt, &ct ) == TMZ_PART_OK
                      && pt.long_first == tcr[i].s && pt.short_first == ( tcr[i].s + 1 ) / 2
                      && pt.short_second == tcr[i].s / 2 && pt.tone == tcr[i].tone
                      && ( tcr[i].reader_n < 0 || ( ct.has_reader && ct.reader_n == tcr[i].reader_n ) );
                tmz_mzfile_free ( &mt );
            }
            CHECK ( oki, tcr[i].txt );
            if ( bt ) tmz_block_free ( bt );
        }
        /* mimo rozsah 1/2..3/1 a číslice mimo 1..9 */
        static const char *bad_tc[] = { "speed=4:9", "speed=7:2", "speed=10:3", "speed=0:1", "speed=2:0" };
        for ( size_t i = 0; i < sizeof ( bad_tc ) / sizeof ( bad_tc[0] ); i++ ) {
            int rej = tmz_profile_args_parse ( bad_tc[i], strlen ( bad_tc[i] ), &a ) == EXIT_FAILURE
                      || tmz_profile_build_block ( "TC-1.21", &a, &g_hdr, g_body, 1024, &pe ) == NULL;
            CHECK ( rej, bad_tc[i] );
        }
        /* poměr TurboCopy u jiného profilu = nepoužitý parametr */
        CHECK ( tmz_profile_args_parse ( "speed=9:4", 9, &a ) == EXIT_SUCCESS
                && tmz_profile_build_block ( "ROM-800-NOMINAL", &a, &g_hdr, g_body, 1024, &pe ) == NULL
                && pe == TMZ_PROFILE_ERROR_ARGS, "ROM profile rejects TurboCopy ratio 9:4" );
        /* en_CMTSPEED 2:1 zadaný textem i polem speed dá stejný blok */
        st_TMZ_PROFILE_ARGS a1, a2;
        tmz_profile_args_init ( &a1 );
        a1.speed = CMTSPEED_2_1;
        st_TZX_BLOCK *b1 = tmz_profile_build_block ( "TC-1.21", &a1, &g_hdr, g_body, 1024, &pe );
        st_TZX_BLOCK *b2 = tmz_profile_args_parse ( "speed=2/1", 9, &a2 ) == EXIT_SUCCESS
                           ? tmz_profile_build_block ( "TC-1.21", &a2, &g_hdr, g_body, 1024, &pe ) : NULL;
        CHECK ( b1 && b2 && b1->length == b2->length && memcmp ( b1->data, b2->data, b1->length ) == 0,
                "TC-1.21 2:1 block unchanged" );
        if ( b1 ) tmz_block_free ( b1 );
        if ( b2 ) tmz_block_free ( b2 );
    }

    tmz_profile_args_init ( &a );
    a.speed = CMTSPEED_2_1;
    CHECK ( tmz_profile_build_block ( "ROM-800-NOMINAL", &a, &g_hdr, g_body, 1024, &pe ) == NULL && pe == TMZ_PROFILE_ERROR_ARGS,
            "ROM profile rejects speed 2:1" );
    CHECK ( tmz_profile_build_block ( "NOPE", &a, &g_hdr, g_body, 1024, &pe ) == NULL && pe == TMZ_PROFILE_ERROR_UNKNOWN,
            "unknown profile" );

    /* ROM-800-NOMINAL výchozí: tóny 11000/5500, čas v µs */
    tmz_profile_args_init ( &a );
    st_TZX_BLOCK *b = tmz_profile_build_block ( "ROM-800-NOMINAL", &a, &g_hdr, g_body, 1024, &pe );
    st_TMZ_MZFILE mf;
    int ok = b && tmz_mzfile_parse ( b, &mf ) == TZX_OK;
    if ( ok ) {
        static st_MZCMT_FM_PARAMS p0, p1;
        st_TMZ_PART_COMMON c0, c1;
        ok = mf.part_count == 2
             && tmz_part_fm_decode ( mf.parts[0].tlv, mf.parts[0].tlv_length, 3546895, &p0, &c0 ) == TMZ_PART_OK
             && tmz_part_fm_decode ( mf.parts[1].tlv, mf.parts[1].tlv_length, 3546895, &p1, &c1 ) == TMZ_PART_OK
             && p0.timebase == 1000000 && p0.tone == 11000 && p1.tone == 5500 && p0.short_second == 278
             && p0.copies == 1 && p0.copy_sep == 256 && p0.pause == 1914441 && p1.pause == 1000000
             && c0.has_reader && c0.reader_routine == 0 && c0.reader_n == 82;
        tmz_mzfile_free ( &mf );
    }
    CHECK ( ok, "ROM-800-NOMINAL defaults (gap short, us, copies, reader)" );
    if ( b ) tmz_block_free ( b );

    /* PROFILE_ARGS: zapsané v bloku a čitelné zpět */
    tmz_profile_args_init ( &a );
    a.speed = CMTSPEED_8_3;
    a.loader = 0;
    a.pause_ms = 250;
    b = tmz_profile_build_block ( "TC-1.21", &a, &g_hdr, g_body, 1024, &pe );
    ok = b && tmz_mzfile_parse ( b, &mf ) == TZX_OK;
    if ( ok ) {
        st_TMZ_PROFILE_ARGS back;
        st_TMZ_TLV_ITER it;
        st_TMZ_TLV t;
        static const char exp[] = "speed=8:3,loader=N,pause=250";
        tmz_tlv_iter_init ( &it, mf.block_tlv, mf.block_tlv_length );
        ok = tmz_tlv_iter_next ( &it, &t ) == 1 && t.tag == TMZ_TAG_PROFILE_ARGS && !t.critical
             && t.length == strlen ( exp ) && memcmp ( t.value, exp, t.length ) == 0
             && tmz_profile_args_parse ( ( const char* ) t.value, t.length, &back ) == EXIT_SUCCESS
             && back.speed == CMTSPEED_8_3 && back.loader == 0 && back.pause_ms == 250
             && strcmp ( mf.profile_id, "TC-1.21" ) == 0 && mf.part_count == 2;
        tmz_mzfile_free ( &mf );
    }
    CHECK ( ok, "PROFILE_ARGS written and parsed back" );
    if ( b ) tmz_block_free ( b );
    CHECK ( tmz_profile_args_parse ( "speed=2:1,foo=1", 15, &a ) == EXIT_FAILURE, "unknown arg key rejected" );
    CHECK ( tmz_profile_args_parse ( "", 0, &a ) == EXIT_SUCCESS && a.pause_ms == 1000 && a.loader == 1, "empty args = defaults" );
    CHECK ( tmz_profile_args_parse ( "gap=rom", 7, &a ) == EXIT_SUCCESS && a.gap_rom == 1, "gap=rom parsed" );
    /* spec 8.2 uvádí zápis a/b (speed=2/1), registr píše a:b - oba se musí přečíst */
    CHECK ( tmz_profile_args_parse ( "speed=3/1,loader=Y", 18, &a ) == EXIT_SUCCESS && a.speed == CMTSPEED_3_1 && a.loader == 1,
            "speed=3/1 (a/b form) parsed" );

    /* rozšířené PROFILE_ARGS */
    static const char legacy_txt[] = "format=fastipl,machine=mz800,pulseset=80b,speed=2:1,lgap=3000,sgap=900,pulses=4980/4980/2490/2490,pause=0";
    CHECK ( tmz_profile_args_parse ( legacy_txt, strlen ( legacy_txt ), &a ) == EXIT_SUCCESS
            && a.legacy_format == TMZ_LEGACY_FASTIPL && a.machine == 2 && a.pulseset == 2 && a.speed == CMTSPEED_2_1
            && a.lgap == 3000 && a.sgap == 900 && a.pulses[0] == 4980 && a.pulses[3] == 2490 && a.pause_ms == 0,
            "legacy args parsed" );
    CHECK ( tmz_profile_args_parse ( "pulses=1/2/3", 12, &a ) == EXIT_FAILURE, "pulses needs 4 values" );
    CHECK ( tmz_profile_args_parse ( "pulses=0/0/1200/0", 17, &a ) == EXIT_SUCCESS && a.pulses[2] == 1200 && a.pulses[0] == 0,
            "pulses with default fields (0) parsed" );
    CHECK ( tmz_profile_args_parse ( "pulses=1/1/1/70000", 18, &a ) == EXIT_FAILURE, "pulses value over 65535 rejected" );
    CHECK ( tmz_profile_args_parse ( "machine=zx", 10, &a ) == EXIT_FAILURE, "unknown machine rejected" );
    CHECK ( tmz_profile_args_parse ( "", 0, &a ) == EXIT_SUCCESS && a.machine == -1 && a.legacy_format == TMZ_LEGACY_NONE
            && a.pulseset == -1, "new arg defaults" );
    CHECK ( tmz_profile_find ( "TAPEMZ-V1-LEGACY" ) != NULL && tmz_profile_count () == 11, "registry has LEGACY" );

    /* machine se zapíše i u ROM profilu a čte se zpět */
    tmz_profile_args_init ( &a );
    a.machine = 2;
    b = tmz_profile_build_block ( "ROM-800-NOMINAL", &a, &g_hdr, g_body, 1024, &pe );
    ok = b && tmz_mzfile_parse ( b, &mf ) == TZX_OK;
    if ( ok ) {
        st_TMZ_TLV_ITER it;
        st_TMZ_TLV t;
        static const char exp_m[] = "gap=short,pause=1000,machine=mz800";
        tmz_tlv_iter_init ( &it, mf.block_tlv, mf.block_tlv_length );
        ok = tmz_tlv_iter_next ( &it, &t ) == 1 && t.length == strlen ( exp_m ) && memcmp ( t.value, exp_m, t.length ) == 0;
        tmz_mzfile_free ( &mf );
    }
    CHECK ( ok, "machine written to PROFILE_ARGS" );
    if ( b ) tmz_block_free ( b );

    /* UNICMT: 1x = PULSES a struktura ROM-800-NOMINAL, 2x z CMTSPEED, READER jen 1x */
    CHECK ( tmz_profile_count () == 11 && tmz_profile_find ( "UNICMT" ), "registry has UNICMT" );
    tmz_profile_args_init ( &a );
    st_TZX_BLOCK *bn = tmz_profile_build_block ( "ROM-800-NOMINAL", &a, &g_hdr, g_body, 1024, &pe );
    st_TZX_BLOCK *bu = tmz_profile_build_block ( "UNICMT", &a, &g_hdr, g_body, 1024, &pe );
    st_TMZ_MZFILE mn, mu;
    ok = bn && bu && tmz_mzfile_parse ( bn, &mn ) == TZX_OK;
    if ( ok ) {
        ok = tmz_mzfile_parse ( bu, &mu ) == TZX_OK && mu.part_count == 2 && strcmp ( mu.profile_id, "UNICMT" ) == 0;
        for ( uint8_t i = 0; ok && i < 2; i++ ) {
            ok = mu.parts[i].tlv_length == mn.parts[i].tlv_length
                 && memcmp ( mu.parts[i].tlv, mn.parts[i].tlv, mn.parts[i].tlv_length ) == 0;
        }
        if ( ok ) {
            st_TMZ_TLV_ITER it;
            st_TMZ_TLV t;
            static const char exp_u[] = "gap=short,cmtspeed=470/494/240/278,pause=1000";
            tmz_tlv_iter_init ( &it, mu.block_tlv, mu.block_tlv_length );
            ok = tmz_tlv_iter_next ( &it, &t ) == 1 && t.length == strlen ( exp_u ) && memcmp ( t.value, exp_u, t.length ) == 0;
            tmz_mzfile_free ( &mu );
        }
        tmz_mzfile_free ( &mn );
    }
    CHECK ( ok, "UNICMT 1x parts identical to ROM-800-NOMINAL" );
    if ( bn ) tmz_block_free ( bn );
    if ( bu ) tmz_block_free ( bu );

    static const char u2[] = "cmtspeed=235/247/120/139";
    ok = tmz_profile_args_parse ( u2, strlen ( u2 ), &a ) == EXIT_SUCCESS && a.cmtspeed[0] == 235 && a.cmtspeed[3] == 139;
    bu = ok ? tmz_profile_build_block ( "UNICMT", &a, &g_hdr, g_body, 1024, &pe ) : NULL;
    ok = bu && tmz_mzfile_parse ( bu, &mu ) == TZX_OK;
    if ( ok ) {
        static st_MZCMT_FM_PARAMS pu;
        st_TMZ_PART_COMMON cu;
        ok = tmz_part_fm_decode ( mu.parts[1].tlv, mu.parts[1].tlv_length, 3546895, &pu, &cu ) == TMZ_PART_OK
             && pu.timebase == 1000000 && pu.long_first == 235 && pu.long_second == 247 && pu.short_first == 120
             && pu.short_second == 139 && !cu.has_reader;
        tmz_mzfile_free ( &mu );
    }
    CHECK ( ok, "UNICMT 2x: PULSES from cmtspeed, no READER" );
    if ( bu ) tmz_block_free ( bu );
    CHECK ( tmz_profile_args_parse ( "cmtspeed=0/1/1/1", 16, &a ) == EXIT_FAILURE, "cmtspeed value 0 rejected" );

    /* každá část každého profilu nese TIMEBASE, nezávisí na bloku 0x43 */
    ok = 1;
    for ( size_t i = 0; i < tmz_profile_count (); i++ ) {
        const char *id = tmz_profile_get ( i )->id;
        tmz_profile_args_init ( &a );
        if ( strcmp ( id, "TAPEMZ-V1-LEGACY" ) == 0 ) a.legacy_format = TMZ_LEGACY_NORMAL;
        /* BASIC-1Z016 čte tělo jako kusy po 258 B */
        uint32_t tsize = strcmp ( id, "BASIC-1Z016" ) == 0 ? 3 * 258 : 1024;
        st_TZX_BLOCK *bt = tmz_profile_build_block ( id, &a, &g_hdr, g_body, tsize, &pe );
        st_TMZ_MZFILE mt;
        if ( !bt || tmz_mzfile_parse ( bt, &mt ) != TZX_OK ) {
            printf ( "  %s: build/parse failed\n", id );
            ok = 0;
            if ( bt ) tmz_block_free ( bt );
            continue;
        }
        for ( uint8_t k = 0; k < mt.part_count; k++ ) {
            int found = 0;
            st_TMZ_TLV_ITER it;
            st_TMZ_TLV t;
            tmz_tlv_iter_init ( &it, mt.parts[k].tlv, mt.parts[k].tlv_length );
            while ( tmz_tlv_iter_next ( &it, &t ) == 1 ) {
                if ( t.tag == TMZ_TAG_TIMEBASE ) found = 1;
            }
            if ( !found ) {
                printf ( "  %s part %u: no TIMEBASE\n", id, k );
                ok = 0;
            }
        }
        tmz_mzfile_free ( &mt );
        tmz_block_free ( bt );
    }
    CHECK ( ok, "every part of every profile carries TIMEBASE" );

    /* profil odmítne parametr, který nepoužívá */
    {
        static const struct { const char *id; int field; } bad[] = {
            { "ROM-800-NOMINAL", 0 }, { "ROM-800-EXACT", 1 }, { "UNICMT", 0 }, { "TC-1.21", 2 },
            { "TC-1.21", 3 }, { "IC-10.2-TURBO", 3 }, { "IC-10.2-NORMAL", 1 }, { "TAPEMZ-V1-LEGACY", 3 },
            { "ROM-800-NOMINAL", 4 }, { "IC-10.2-FASTIPL", 5 },
        };
        ok = 1;
        for ( size_t i = 0; i < sizeof ( bad ) / sizeof ( bad[0] ); i++ ) {
            tmz_profile_args_init ( &a );
            if ( strcmp ( bad[i].id, "TAPEMZ-V1-LEGACY" ) == 0 ) a.legacy_format = TMZ_LEGACY_NORMAL;
            switch ( bad[i].field ) {
                case 0: a.bd = 2400; break;                 /* jen IC */
                case 1: a.loader = 0; break;                /* jen TC */
                case 2: a.cmtspeed[0] = a.cmtspeed[1] = a.cmtspeed[2] = a.cmtspeed[3] = 200; break;   /* jen UNICMT */
                case 3: a.gap_rom = 1; break;               /* jen ROM a UNICMT */
                case 4: a.pulseset = TMZ_PULSESET_700; break;   /* jen LEGACY */
                default: a.lgap = 100; break;               /* jen LEGACY */
            }
            st_TZX_BLOCK *bx = tmz_profile_build_block ( bad[i].id, &a, &g_hdr, g_body, 1024, &pe );
            if ( bx || pe != TMZ_PROFILE_ERROR_ARGS ) {
                printf ( "  %s field %d accepted\n", bad[i].id, bad[i].field );
                ok = 0;
            }
            if ( bx ) tmz_block_free ( bx );
        }
        CHECK ( ok, "profiles reject parameters they do not use" );
    }

    printf ( "\n=== TMZ Profile Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
