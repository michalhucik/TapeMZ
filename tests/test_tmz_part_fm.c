/**
 * @file   test_tmz_part_fm.c
 * @brief  Testy převodu TLV části na parametry FM a zpět (spec TMZ v1.1 kap. 5, 6.1).
 *         Spouštět z build/ (vektory ../tstdata/tmz-v2/).
 * Bez tstdata/ vypíše SKIP a skončí 0 (tests/tstdata_skip.h).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/tzx/tzx.h"
#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_mzfile.h"
#include "libs/tmz/tmz_part_fm.h"
#include "libs/generic_driver/generic_driver.h"
#include "tstdata_skip.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )


/**
 * @brief Načte první blok souboru (kopie; uvolnit tmz_block_free).
 * @param path Cesta. @return Blok, nebo NULL.
 */
static st_TZX_BLOCK* load_first_block ( const char *path ) {
    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *hp = generic_driver_open_file ( &h, &d, ( char* ) path, FILE_DRIVER_OPMODE_RO );
    if ( !hp ) return NULL;
    en_TZX_ERROR err = TZX_OK;
    st_TZX_FILE *f = tzx_load ( hp, &err );
    generic_driver_close ( hp );
    if ( !f || f->block_count < 1 ) { if ( f ) tzx_free ( f ); return NULL; }
    st_TZX_BLOCK *b = malloc ( sizeof ( st_TZX_BLOCK ) );
    b->id = f->blocks[0].id;
    b->length = f->blocks[0].length;
    b->data = malloc ( b->length );
    memcpy ( b->data, f->blocks[0].data, b->length );
    tzx_free ( f );
    return b;
}


/** @brief Všechny části všech vektorů: decode -> encode je bajtově shodné. */
static void test_vectors_roundtrip ( void ) {
    static const char *names[] = { "v1-rom-800-nominal", "v2-tc-2to1-loader-n", "v3-tc-3to1-loader-y",
                                   "v4-custom-x", "v6-rom-800-exact" };
    for ( size_t i = 0; i < sizeof ( names ) / sizeof ( names[0] ); i++ ) {
        char path[128], msg[160];
        snprintf ( path, sizeof ( path ), "../tstdata/tmz-v2/%s.tmz", names[i] );
        st_TZX_BLOCK *b = load_first_block ( path );
        st_TMZ_MZFILE mf;
        if ( !b || tmz_mzfile_parse ( b, &mf ) != TZX_OK ) { CHECK ( 0, path ); continue; }
        for ( uint8_t k = 0; k < mf.part_count; k++ ) {
            static st_MZCMT_FM_PARAMS p;
            st_TMZ_PART_COMMON c;
            en_TMZ_PART_ERROR e = tmz_part_fm_decode ( mf.parts[k].tlv, mf.parts[k].tlv_length, 3546895, &p, &c );
            st_TMZ_TLVBUF buf;
            tmz_tlvbuf_init ( &buf );
            int ok = ( e == TMZ_PART_OK ) && tmz_part_fm_encode ( &p, &c, 3546895, &buf ) == EXIT_SUCCESS
                     && buf.length == mf.parts[k].tlv_length && memcmp ( buf.data, mf.parts[k].tlv, buf.length ) == 0;
            snprintf ( msg, sizeof ( msg ), "%s part %u decode/encode identical", names[i], k );
            CHECK ( ok, msg );
            tmz_tlvbuf_free ( &buf );
        }
        tmz_mzfile_free ( &mf );
        tmz_block_free ( b );
    }
}


/** @brief Hodnoty z V3 část 2 (TC 3/1 tělo) a V6 (kontexty ROM). */
static void test_values ( void ) {
    st_TZX_BLOCK *b = load_first_block ( "../tstdata/tmz-v2/v3-tc-3to1-loader-y.tmz" );
    st_TMZ_MZFILE mf;
    if ( !b || tmz_mzfile_parse ( b, &mf ) != TZX_OK ) { CHECK ( 0, "V3 load" ); return; }
    static st_MZCMT_FM_PARAMS p;
    st_TMZ_PART_COMMON c;
    CHECK ( tmz_part_fm_decode ( mf.parts[2].tlv, mf.parts[2].tlv_length, 3546895, &p, &c ) == TMZ_PART_OK, "V3 part 2 decode" );
    CHECK ( p.timebase == 1108405 && p.short_first == 116 && p.short_second == 115 && p.long_first == 231
            && p.long_second == 231, "V3 part 2 timebase + pulses" );
    CHECK ( p.tone == 15130 && p.tm_long == 20 && p.tm_short == 20 && p.tm_end_long == 1, "V3 part 2 tone + tapemark" );
    CHECK ( p.checksum == 1 && p.end_long == 1 && p.tail_short == 99 && p.copies == 0 && p.pause == 0, "V3 part 2 rest" );
    CHECK ( c.stop == 1 && c.has_reader && c.reader_routine == 1 && c.reader_n == 27, "V3 part 2 STOP + READER" );
    tmz_mzfile_free ( &mf );
    tmz_block_free ( b );

    b = load_first_block ( "../tstdata/tmz-v2/v6-rom-800-exact.tmz" );
    if ( !b || tmz_mzfile_parse ( b, &mf ) != TZX_OK ) { CHECK ( 0, "V6 load" ); return; }
    CHECK ( tmz_part_fm_decode ( mf.parts[0].tlv, mf.parts[0].tlv_length, 3546895, &p, &c ) == TMZ_PART_OK
            && p.context_count == 22 && p.copies == 1 && p.copy_sep == 256 && p.timebase == 3546895,
            "V6 part 0: 22 contexts, copy 256, default timebase" );
    CHECK ( p.context[7].ctx == MZCMT_FM_CTX_START && p.context[7].kind == 1 && p.context[7].next == 0
            && p.context[7].second == 1726, "V6 context START L->S 1726" );
    tmz_mzfile_free ( &mf );
    tmz_block_free ( b );
}


/** @brief Chybové stavy TLV (spec 4.4, 5.1). */
static void test_errors ( void ) {
    static st_MZCMT_FM_PARAMS p;
    st_TMZ_PART_COMMON c;
    uint8_t pulses[20] = { 0x01, 0x81, 0x10, 0x00, 10, 0, 0, 0, 11, 0, 0, 0, 20, 0, 0, 0, 21, 0, 0, 0 };
    CHECK ( tmz_part_fm_decode ( pulses, 20, 3546895, &p, &c ) == TMZ_PART_OK && p.timebase == 3546895,
            "minimal part (PULSES only), default timebase" );
    CHECK ( tmz_part_fm_decode ( NULL, 0, 3546895, &p, &c ) == TMZ_PART_ERROR_MISSING_PULSES, "no PULSES" );

    uint8_t buf[64];
    memcpy ( buf, pulses, 20 );
    /* PAUSE: délka 4 = LOW, délka 5 s úrovní, úroveň 2 je chyba (Review Focus 1) */
    static const uint8_t pause4[] = { 0x03, 0x80, 0x04, 0x00, 0x64, 0, 0, 0 };
    static const uint8_t pause5[] = { 0x03, 0x80, 0x05, 0x00, 0x64, 0, 0, 0, 0x01 };
    static const uint8_t pause5bad[] = { 0x03, 0x80, 0x05, 0x00, 0x64, 0, 0, 0, 0x02 };
    memcpy ( buf + 20, pause4, sizeof ( pause4 ) );
    CHECK ( tmz_part_fm_decode ( buf, 28, 3546895, &p, &c ) == TMZ_PART_OK && p.pause == 100 && p.pause_level == 0,
            "PAUSE length 4 -> level LOW" );
    memcpy ( buf + 20, pause5, sizeof ( pause5 ) );
    CHECK ( tmz_part_fm_decode ( buf, 29, 3546895, &p, &c ) == TMZ_PART_OK && p.pause == 100 && p.pause_level == 1,
            "PAUSE length 5 -> level HIGH" );
    {
        st_TMZ_TLVBUF tb;
        tmz_tlvbuf_init ( &tb );
        int ok = tmz_part_fm_encode ( &p, &c, 3546895, &tb ) == EXIT_SUCCESS;
        static st_MZCMT_FM_PARAMS q;
        st_TMZ_PART_COMMON qc;
        ok = ok && tmz_part_fm_decode ( tb.data, tb.length, 3546895, &q, &qc ) == TMZ_PART_OK && q.pause_level == 1
             && q.pause == 100;
        CHECK ( ok, "PAUSE level HIGH round trip" );
        tmz_tlvbuf_free ( &tb );
    }
    memcpy ( buf + 20, pause5bad, sizeof ( pause5bad ) );
    CHECK ( tmz_part_fm_decode ( buf, 29, 3546895, &p, &c ) == TMZ_PART_ERROR_BAD_VALUE, "PAUSE level 2 rejected" );
    memcpy ( buf, pulses, 20 );
    /* neznámý povinný tag 0x0199 */
    static const uint8_t unk_crit[] = { 0x99, 0x81, 0x01, 0x00, 0x00 };
    memcpy ( buf + 20, unk_crit, 5 );
    CHECK ( tmz_part_fm_decode ( buf, 25, 3546895, &p, &c ) == TMZ_PART_ERROR_UNKNOWN_CRITICAL, "unknown critical tag" );
    /* neznámý nepovinný tag 0x0199 */
    static const uint8_t unk_opt[] = { 0x99, 0x01, 0x01, 0x00, 0x00 };
    memcpy ( buf + 20, unk_opt, 5 );
    CHECK ( tmz_part_fm_decode ( buf, 25, 3546895, &p, &c ) == TMZ_PART_OK, "unknown optional tag skipped" );
    /* opakovaný TONE */
    static const uint8_t tone2[] = { 0x02, 0x81, 0x04, 0x00, 1, 0, 0, 0, 0x02, 0x81, 0x04, 0x00, 2, 0, 0, 0 };
    memcpy ( buf + 20, tone2, 16 );
    CHECK ( tmz_part_fm_decode ( buf, 36, 3546895, &p, &c ) == TMZ_PART_ERROR_BAD_VALUE, "duplicate TONE" );
    /* TIMEBASE se špatnou délkou */
    static const uint8_t tb3[] = { 0x01, 0x80, 0x03, 0x00, 1, 2, 3 };
    memcpy ( buf + 20, tb3, 7 );
    CHECK ( tmz_part_fm_decode ( buf, 27, 3546895, &p, &c ) == TMZ_PART_ERROR_BAD_VALUE, "TIMEBASE length 3" );
    /* TIMEBASE 0 */
    static const uint8_t tb0[] = { 0x01, 0x80, 0x04, 0x00, 0, 0, 0, 0 };
    memcpy ( buf + 20, tb0, 8 );
    CHECK ( tmz_part_fm_decode ( buf, 28, 3546895, &p, &c ) == TMZ_PART_ERROR_BAD_VALUE, "TIMEBASE 0" );
    /* READER (nepovinný) se špatnou délkou se přeskočí */
    static const uint8_t rd2[] = { 0x05, 0x00, 0x02, 0x00, 1, 2 };
    memcpy ( buf + 20, rd2, 6 );
    CHECK ( tmz_part_fm_decode ( buf, 26, 3546895, &p, &c ) == TMZ_PART_OK && !c.has_reader, "bad READER skipped" );
    /* CONTEXT s délkou 12 (není násobek 11) (Review Focus 2) */
    uint8_t ctx12[16] = { 0x08, 0x81, 0x0C, 0x00 };
    memcpy ( buf + 20, ctx12, 16 );
    CHECK ( tmz_part_fm_decode ( buf, 36, 3546895, &p, &c ) == TMZ_PART_ERROR_BAD_VALUE, "CONTEXT length 12" );
    /* CONTEXT s duplicitní trojicí ve dvou TLV */
    uint8_t ctxdup[30] = { 0x08, 0x81, 0x0B, 0x00, 0, 0, 0xFF, 1, 0, 0, 0, 1, 0, 0, 0,
                           0x08, 0x81, 0x0B, 0x00, 0, 0, 0xFF, 2, 0, 0, 0, 2, 0, 0, 0 };
    memcpy ( buf + 20, ctxdup, 30 );
    CHECK ( tmz_part_fm_decode ( buf, 50, 3546895, &p, &c ) == TMZ_PART_ERROR_BAD_VALUE, "CONTEXT duplicate triple" );
    /* dvě CONTEXT TLV s různými trojicemi se sčítají */
    buf[20 + 15 + 5] = 1;   /* druhá položka (už v buf): kind L */
    CHECK ( tmz_part_fm_decode ( buf, 50, 3546895, &p, &c ) == TMZ_PART_OK && p.context_count == 2, "two CONTEXT TLVs accumulate" );
    /* CHECKSUM s explicitními bajty délky 1 */
    static const uint8_t ck1[] = { 0x05, 0x81, 0x02, 0x00, 0x01, 0xAA };
    memcpy ( buf + 20, ck1, 6 );
    CHECK ( tmz_part_fm_decode ( buf, 26, 3546895, &p, &c ) == TMZ_PART_ERROR_BAD_VALUE, "explicit checksum of 1 byte" );
    /* CHECKSUM s neznámým algoritmem */
    static const uint8_t ck7[] = { 0x05, 0x81, 0x01, 0x00, 0x07 };
    memcpy ( buf + 20, ck7, 5 );
    CHECK ( tmz_part_fm_decode ( buf, 25, 3546895, &p, &c ) == TMZ_PART_ERROR_BAD_VALUE, "unknown checksum algorithm" );
    /* opakovaný nepovinný tag (STOP) se přeskočí, platí první výskyt (spec 5.1) */
    static const uint8_t stop2[] = { 0x04, 0x00, 0x01, 0x00, 0x01, 0x04, 0x00, 0x01, 0x00, 0x00 };
    memcpy ( buf + 20, stop2, 10 );
    CHECK ( tmz_part_fm_decode ( buf, 30, 3546895, &p, &c ) == TMZ_PART_OK && c.stop == 1,
            "duplicate optional STOP skipped, first wins" );
    /* známý povinný tag zapsaný bez bitu 15 se špatnou délkou je chyba (povinnost podle tabulky spec) */
    static const uint8_t tb3nc[] = { 0x01, 0x00, 0x03, 0x00, 1, 2, 3 };
    memcpy ( buf + 20, tb3nc, 7 );
    CHECK ( tmz_part_fm_decode ( buf, 27, 3546895, &p, &c ) == TMZ_PART_ERROR_BAD_VALUE,
            "TIMEBASE without bit 15, length 3 -> error (not silent default)" );
    /* přesah TLV */
    CHECK ( tmz_part_fm_decode ( pulses, 19, 3546895, &p, &c ) == TMZ_PART_ERROR_TLV_STRUCTURE, "truncated TLV" );
}


/**
 * @brief Rezervy čtecího bodu (spec 5.6).
 *
 * TC 2:1: SH 156 tiků = 499,2 T < 86+14·41 = 660; LH 311 tiků = 995 T > 712.
 * N=60: LH 995 > 978 jen o 17 T (4,8 µs < 10), perioda 995 T < 1096 (nečitelné). ROM N=82: LH 995 < 1286.
 * ROM nominál: SH 851 T < 1234, LH 1667 T > 1286, perioda 1837 T > 1404.
 */
static void test_reader_check ( void ) {
    static st_MZCMT_FM_PARAMS p;
    st_TMZ_PART_COMMON c = { 0, 1, 1, 41 };
    st_TMZ_READER_MARGINS m;
    mzcmt_fm_params_init ( &p );
    p.timebase = 1108405;
    p.short_first = 156;
    p.short_second = 155;
    p.long_first = 311;
    p.long_second = 311;   /* TC 2:1 */
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 0, &m ) == TMZ_READER_OK && m.short_high_us > 10 && m.long_high_us > 10,
            "TC 2:1 body with N=41 readable" );
    c.reader_n = 60;   /* LONG_H rezerva 4,8 µs, perioda SHORT 995 T < 256+14·60 = 1096 T */
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 0, &m ) == TMZ_READER_UNREADABLE && m.long_high_us > 0
            && m.long_high_us < TMZ_READER_MIN_MARGIN_US && m.short_period_us < 0, "N=60 not readable (period)" );
    c.reader_routine = 0;   /* ROM N=82 na pulzech 2:1 - LONG je příliš krátký */
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 0, &m ) == TMZ_READER_UNREADABLE && m.long_high_us < 0,
            "ROM reading TC 2:1 not readable" );
    p.timebase = 1000000;
    p.short_first = 240;
    p.short_second = 278;
    p.long_first = 470;
    p.long_second = 494;   /* ROM nominál */
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 0, &m ) == TMZ_READER_OK, "ROM nominal readable by ROM" );
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 22050, &m ) == TMZ_READER_OK, "ROM nominal readable at 22050 Hz" );
    p.context_count = 1;
    p.context[0] = ( st_MZCMT_FM_CONTEXT ) { MZCMT_FM_CTX_BIT, 1, MZCMT_FM_NEXT_ANY, 300, 494 };   /* krátký LONG v kontextu */
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 0, &m ) == TMZ_READER_UNREADABLE, "short LONG in a context detected" );
    c.has_reader = 0;
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 0, &m ) == TMZ_READER_SKIPPED, "no READER -> skipped" );
    c.has_reader = 1;
    c.reader_routine = 7;
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 0, &m ) == TMZ_READER_SKIPPED, "unknown routine -> skipped" );
}


/**
 * @brief Rezervy po převodu na vzorky: nejhorší zaokrouhlení hran.
 *
 * TC-1.21 (časová základna 1 108 405 Hz). Při 22 050 Hz: 3:1 SHORT_H
 * 116 tiků -> 3 vzorky = 136,1 µs > 464 T = 130,8 µs; 8:3 perioda 251 tiků
 * -> 4 vzorky = 181,4 µs < 676 T = 190,6 µs; 2:1 SHORT_H 156 tiků -> 4 vzorky
 * = 181,4 µs, rezerva 4,7 µs. Při 44 100 Hz 8:3 SHORT_H 126 tiků -> 6 vzorků,
 * rezerva 6,6 µs.
 */
static void test_reader_check_rate ( void );


/**
 * @brief READER rutina 2: zavaděč FAST IPL V07 = ROM + 86 T po náběžné hraně (disassembly Intercopy 10.2).
 *
 * Intercopy 3200, N = 17, T-stavy: SHORT 273/416, LONG 559/637.
 * Nominálně: SHORT_H 410 - 273 = 137 T (38,6 µs), LONG_H 559 - 462 = 97 T
 * (27,3 µs), perioda 689 - 580 = 109 T (30,7 µs) -> OK. Rutina 1 (bez +86):
 * LONG_H 559 - 376 = 183 T (51,6 µs), rozdíl 86 T = 24,2 µs. Při 22 050 Hz
 * (1 vzorek = 160,85 T): LONG_H floor(3,475) = 3 vzorky = 482,6 T, rezerva
 * 20,6 T = 5,8 µs -> LOW_MARGIN; SHORT_H ceil(1,697) = 2 vzorky = 321,7 T,
 * rezerva 88,3 T; perioda floor(4,283) = 4 vzorky = 643,4 T, rezerva 63,4 T.
 */
static void test_reader_routine2 ( void ) {
    static st_MZCMT_FM_PARAMS p;
    st_TMZ_PART_COMMON c = { 0, 1, 2, 17 };
    st_TMZ_READER_MARGINS m, m1;
    mzcmt_fm_params_init ( &p );
    p.timebase = 3546895;
    p.short_first = 273;
    p.short_second = 416;
    p.long_first = 559;
    p.long_second = 637;
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 0, &m ) == TMZ_READER_OK && m.long_high_us > 27.0 && m.long_high_us < 27.6,
            "routine 2 (FAST IPL V07) 3200: LONG_H margin 27.3 us" );
    c.reader_routine = 1;
    tmz_part_fm_check_reader ( &p, &c, 0, &m1 );
    CHECK ( m1.long_high_us - m.long_high_us > 24.1 && m1.long_high_us - m.long_high_us < 24.4,
            "routine 2 differs from routine 1 by 86 T" );
    c.reader_routine = 2;
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 22050, &m ) == TMZ_READER_LOW_MARGIN && m.long_high_us > 5.5
            && m.long_high_us < 6.1, "routine 2 3200 at 22050 Hz: low margin 5.8 us" );
}


static void test_reader_check_rate ( void ) {
    static st_MZCMT_FM_PARAMS p;
    st_TMZ_PART_COMMON c = { 0, 1, 1, 27 };
    st_TMZ_READER_MARGINS m;
    mzcmt_fm_params_init ( &p );
    p.timebase = 1108405;
    p.short_first = 116;
    p.short_second = 115;
    p.long_first = 231;
    p.long_second = 231;   /* TC 3:1, N = 27 */
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 0, &m ) == TMZ_READER_OK, "TC 3:1 nominal OK" );
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 22050, &m ) == TMZ_READER_UNREADABLE && m.short_high_us < 0,
            "TC 3:1 at 22050 Hz unreadable (SHORT_H 3 samples)" );
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 44100, &m ) == TMZ_READER_OK, "TC 3:1 at 44100 Hz OK" );

    c.reader_n = 30;
    p.short_first = 126;
    p.short_second = 125;
    p.long_first = 251;
    p.long_second = 251;   /* TC 8:3 */
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 22050, &m ) == TMZ_READER_UNREADABLE && m.short_period_us < 0
            && m.short_high_us > 0, "TC 8:3 at 22050 Hz unreadable (SHORT period 4 samples)" );
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 44100, &m ) == TMZ_READER_LOW_MARGIN && m.short_high_us > 6.0
            && m.short_high_us < 7.0, "TC 8:3 at 44100 Hz low margin 6.6 us" );
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 48000, &m ) == TMZ_READER_OK, "TC 8:3 at 48000 Hz OK" );

    c.reader_n = 41;
    p.short_first = 156;
    p.short_second = 155;
    p.long_first = 311;
    p.long_second = 311;   /* TC 2:1 */
    CHECK ( tmz_part_fm_check_reader ( &p, &c, 22050, &m ) == TMZ_READER_LOW_MARGIN && m.short_high_us > 4.0
            && m.short_high_us < 5.0, "TC 2:1 at 22050 Hz low margin 4.7 us" );
}


int main ( void ) {
    if ( tstdata_skip ( "test_tmz_part_fm" ) ) return 0;
    test_vectors_roundtrip ();
    test_values ();
    test_reader_check_rate ();
    test_reader_routine2 ();
    test_errors ();
    test_reader_check ();
    printf ( "\n=== TMZ Part FM Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
