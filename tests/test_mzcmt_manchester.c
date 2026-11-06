/**
 * @file   test_mzcmt_manchester.c
 * @brief  Testy generátoru rodiny MANCHESTER (spec TMZ v1.1 kap. 7.2):
 *         posloupnost bitů, kontexty a hodnota bitu, přenos součtu, délky dat
 *         0/1/2, validace, reference Pezik (model, přesně na T) a Intercopy
 *         (záznam z emulátoru, s tolerancí) a převod na vzorky.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "libs/mzcmt_manchester/mzcmt_manchester.h"
#include "libs/cmt_stream/cmt_vstream.h"
#include "tstdata_skip.h"
#include "manch_test_params.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )

/** @brief Nejvíc nasbíraných půlvln (Intercopy záznam má 26 646). */
#define MAX_HALVES 65536

/** @brief Nasbírané půlvlny; sousední stejné úrovně se sloučí (pro spojení částí). */
typedef struct st_HALVES {
    uint32_t n;                 /**< počet */
    int level[MAX_HALVES];      /**< úrovně */
    uint32_t dur[MAX_HALVES];   /**< délky */
} st_HALVES;

/** @brief Pracovní pole (velká, proto statická). */
static st_HALVES g_a, g_b;


/**
 * @brief Callback generátoru: uloží půlvlnu, stejnou úroveň jako poslední přičte.
 * @param user st_HALVES. @param level Úroveň. @param duration Délka.
 * @return EXIT_SUCCESS, nebo EXIT_FAILURE při plném poli.
 */
static int collect ( void *user, int level, uint32_t duration ) {
    st_HALVES *h = user;
    if ( h->n && h->level[h->n - 1] == level ) {
        h->dur[h->n - 1] += duration;
        return EXIT_SUCCESS;
    }
    if ( h->n >= MAX_HALVES ) return EXIT_FAILURE;
    h->level[h->n] = level;
    h->dur[h->n] = duration;
    h->n++;
    return EXIT_SUCCESS;
}


/**
 * @brief Nezávislý výpočet: bity rozepíše na půlky (všechny délky d), bit 1 = H L, bit 0 = L H.
 * @param bits Bity v pořadí vysílání. @param n Počet. @param d Délka půlky. @param out Výstup.
 */
static void expand ( const uint8_t *bits, uint32_t n, uint32_t d, st_HALVES *out ) {
    out->n = 0;
    for ( uint32_t i = 0; i < n; i++ ) {
        collect ( out, bits[i] ? 1 : 0, d );
        collect ( out, bits[i] ? 0 : 1, d );
    }
}


/**
 * @brief Parametry se všemi půlkami 100 a strukturou testu (a).
 * @param p Výstup.
 */
static void simple ( st_MZCMT_MANCH_PARAMS *p ) {
    mzcmt_manch_params_init ( p );
    p->half1[0] = p->half1[1] = p->half2[0] = p->half2[1] = 100;
    p->pilot_count = 2;
    p->pilot_bits = 3;
    p->pilot_pattern = 0x06;
    p->sync_bits = 1;
    p->sync_pattern = 0x01;
    p->has_flag = 1;
    p->flag = 0x00;
    p->checksum = MZCMT_MANCH_CHECKSUM_SUM16_LE;
    p->end_bits = 1;
    p->end_pattern = 0x01;
}


/** @brief (a) Posloupnost bitů a slučování půlek. */
static void test_structure ( void ) {
    st_MZCMT_MANCH_PARAMS p;
    simple ( &p );
    static const uint8_t bits[] = {
        0, 1, 1, 0, 1, 1,                 /* pilot 2x 011 */
        1,                                /* sync */
        0, 0, 0, 0, 0, 0, 0, 0,           /* flag 00h */
        1, 0, 1, 0, 0, 1, 0, 1,           /* A5h LSB first */
        1, 0, 1, 0, 0, 1, 0, 1,           /* checksum L = A5h */
        0, 0, 0, 0, 0, 0, 0, 0,           /* checksum H = 00h */
        1,                                /* koncový bit */
    };
    uint8_t d = 0xA5;
    memset ( &g_a, 0, sizeof ( g_a ) );
    int r = mzcmt_manch_generate ( &p, &d, 1, collect, &g_a );
    expand ( bits, sizeof ( bits ), 100, &g_b );
    int same = r == EXIT_SUCCESS && g_a.n == g_b.n;
    for ( uint32_t i = 0; same && i < g_a.n; i++ ) {
        if ( g_a.level[i] != g_b.level[i] || g_a.dur[i] != g_b.dur[i] ) same = 0;
    }
    CHECK ( same, "pilot, sync, flag, data, checksum, end bit as independent expansion" );
    static const int lv[8] = { 0, 1, 0, 1, 0, 1, 0, 1 };
    static const uint32_t du[8] = { 100, 200, 100, 100, 200, 200, 100, 100 };
    int first = g_a.n >= 8;
    for ( int i = 0; first && i < 8; i++ ) {
        if ( g_a.level[i] != lv[i] || g_a.dur[i] != du[i] ) first = 0;
    }
    CHECK ( first, "first 8 halves L100 H200 L100 H100 L200 H200 L100 H100" );
    CHECK ( mzcmt_manch_checksum ( &d, 1 ) == 0x00A5, "checksum A5h" );

    /* LEVEL 0 úrovně prohodí */
    p.level = 0;
    memset ( &g_b, 0, sizeof ( g_b ) );
    r = mzcmt_manch_generate ( &p, &d, 1, collect, &g_b );
    int inv = r == EXIT_SUCCESS && g_b.n == g_a.n;
    for ( uint32_t i = 0; inv && i < g_a.n; i++ ) {
        if ( g_b.level[i] != !g_a.level[i] || g_b.dur[i] != g_a.dur[i] ) inv = 0;
    }
    CHECK ( inv, "LEVEL 0 inverts levels" );

    /* pauza: samostatná poslední půlvlna s úrovní z PAUSE */
    p.level = 1;
    p.pause = 5000;
    p.pause_level = 1;
    memset ( &g_b, 0, sizeof ( g_b ) );
    r = mzcmt_manch_generate ( &p, &d, 1, collect, &g_b );
    CHECK ( r == EXIT_SUCCESS && g_b.n == g_a.n + 1 && g_b.level[g_b.n - 1] == 1 && g_b.dur[g_b.n - 1] == 5000,
            "pause HIGH is the last half-wave" );
}


/**
 * @brief Najde v posloupnosti půlek druhou půlku bitu č. k (počítáno od 0).
 *
 * Vygeneruje část s jedinečnými délkami první půlky (1000) tak, aby šlo
 * půlky spočítat: posloupnost se rozepíše zpět na půlky podle známých
 * prvních půlek. Jednodušší: generuje se s pilot_count 0, sync 0 a první
 * půlka 1000, druhé půlky < 1000; bit k má pak druhou půlku na pevném
 * místě v nesloučeném výstupu.
 */
typedef struct st_RAW {
    uint32_t n;           /**< počet */
    uint32_t dur[256];    /**< délky bez slučování */
} st_RAW;

/**
 * @brief Callback bez slučování (jen pro testy s jedinečnými délkami).
 * @param user st_RAW. @param level Úroveň (nepoužitá). @param duration Délka.
 * @return EXIT_SUCCESS, nebo EXIT_FAILURE při plném poli.
 */
static int collect_raw ( void *user, int level, uint32_t duration ) {
    ( void ) level;
    st_RAW *r = user;
    if ( r->n >= 256 ) return EXIT_FAILURE;
    r->dur[r->n++] = duration;
    return EXIT_SUCCESS;
}


/**
 * @brief Druhé půlky bitů 7 všech bajtů (flag, data, checksum) z části bez pilotu a sync.
 *
 * Generátor slučuje stejné úrovně, proto se délky rekonstruují ze sloučených
 * půlvln: první půlka je vždy 1000 (u Pezik parametrů přepsaná), takže
 * každá sloučená půlvlna je buď druhá půlka (< 1000), první půlka (1000),
 * nebo jejich součet (> 1000). Z toho se druhé půlky dopočítají.
 *
 * @param p Parametry (half1 se přepíše na 1000). @param data Data. @param size Délka.
 * @param out Druhé půlky všech bitů v pořadí. @return Počet bitů.
 */
static uint32_t second_halves ( st_MZCMT_MANCH_PARAMS *p, const uint8_t *data, uint32_t size, uint32_t *out ) {
    p->half1[0] = p->half1[1] = 1000;
    p->pilot_count = 0;
    p->sync_bits = 0;
    st_RAW r;
    memset ( &r, 0, sizeof ( r ) );
    if ( mzcmt_manch_generate ( p, data, size, collect_raw, &r ) != EXIT_SUCCESS ) return 0;
    /* rozklad: každá půlvlna = [druhá půlka] + [první půlka 1000] */
    uint32_t k = 0;
    for ( uint32_t i = 0; i < r.n; i++ ) {
        uint32_t d = r.dur[i];
        if ( i == 0 ) continue;                 /* první půlka prvního bitu */
        if ( d > 1000 ) out[k++] = d - 1000;    /* druhá + první další bitu */
        else if ( d < 1000 ) out[k++] = d;      /* druhá půlka, další bit má jinou úroveň */
    }
    return k;
}


/** @brief (b) Kontexty a hodnota bitu (Pezik 2400). */
static void test_contexts ( void ) {
    st_MZCMT_MANCH_PARAMS p;
    pezik ( &p, 43, 1 );
    p.carry_adjust = 0;
    uint8_t data[2] = { 0x00, 0xFF };
    uint32_t h2[64];
    uint32_t n = second_halves ( &p, data, 2, h2 );
    /* bity: flag 8, data 16, ck 16 = 40; poslední druhá půlka (CK_END) bez další půlky */
    CHECK ( n >= 39, "second halves recovered" );
    if ( n < 39 ) return;
    char msg[96];
    snprintf ( msg, sizeof ( msg ), "FLAG_END bit 0 = 790 (got %u)", h2[7] );
    CHECK ( h2[7] == 790, msg );
    snprintf ( msg, sizeof ( msg ), "BIT bit 0 inside byte = 751 (got %u)", h2[8] );
    CHECK ( h2[8] == 751, msg );
    snprintf ( msg, sizeof ( msg ), "DATA_FIRST bit 0 = 745 (got %u)", h2[15] );
    CHECK ( h2[15] == 745, msg );
    snprintf ( msg, sizeof ( msg ), "BIT bit 1 inside byte = 750 (got %u)", h2[16] );
    CHECK ( h2[16] == 750, msg );
    snprintf ( msg, sizeof ( msg ), "DATA_END bit 1 = 759 (got %u)", h2[23] );
    CHECK ( h2[23] == 759, msg );
    /* checksum = 00FFh: L = FFh (bit 7 = 1), CK_MID bit 1 = 760 */
    snprintf ( msg, sizeof ( msg ), "CK_MID bit 1 = 760 (got %u)", h2[31] );
    CHECK ( h2[31] == 760, msg );
}


/** @brief (c) Přenos součtu: úprava jen u přechodu do bajtu, který přeteče. */
static void test_carry ( void ) {
    st_MZCMT_MANCH_PARAMS p;
    pezik ( &p, 43, 1 );
    uint8_t data[3] = { 0x80, 0x80, 0x01 };
    uint32_t h2[64];
    uint32_t n = second_halves ( &p, data, 3, h2 );
    CHECK ( n >= 40, "second halves recovered (carry)" );
    if ( n < 40 ) return;
    char msg[96];
    /* bit 7 data[0] = 1: DATA_FIRST bit 1 = 744, přenos 80h + 80h -> 743 */
    snprintf ( msg, sizeof ( msg ), "DATA_FIRST with carry = 743 (got %u)", h2[15] );
    CHECK ( h2[15] == 743, msg );
    /* bit 7 data[1] = 1: DATA_BYTE bit 1 = 742, součet 00h + 01h bez přenosu */
    snprintf ( msg, sizeof ( msg ), "DATA_BYTE without carry = 742 (got %u)", h2[23] );
    CHECK ( h2[23] == 742, msg );
}


/** @brief (d) Délka dat 1 a 0. */
static void test_lengths ( void ) {
    st_MZCMT_MANCH_PARAMS p;
    pezik ( &p, 43, 1 );
    uint8_t one = 0x80;
    uint32_t h2[64];
    uint32_t n = second_halves ( &p, &one, 1, h2 );
    char msg[96];
    snprintf ( msg, sizeof ( msg ), "DATA_SINGLE_END bit 1 = 771 (got %u)", n > 15 ? h2[15] : 0 );
    CHECK ( n > 15 && h2[15] == 771, msg );
    /* délka 0: flag, checksum 0000h; bit 7 flagu FLAG_END, checksum L bit 7 CK_MID */
    pezik ( &p, 43, 1 );
    n = second_halves ( &p, NULL, 0, h2 );
    snprintf ( msg, sizeof ( msg ), "empty data: FLAG_END 790, CK_MID bit 0 = 761 (got %u, %u)",
               n > 15 ? h2[7] : 0, n > 15 ? h2[15] : 0 );
    CHECK ( n > 15 && h2[7] == 790 && h2[15] == 761, msg );
}


/** @brief (e) Validace. */
static void test_validate ( void ) {
    st_MZCMT_MANCH_PARAMS p;
    pezik ( &p, 43, 1 );
    CHECK ( mzcmt_manch_params_validate ( &p ) == EXIT_SUCCESS, "Pezik params valid" );
    intercopy ( &p, 1 );
    CHECK ( mzcmt_manch_params_validate ( &p ) == EXIT_SUCCESS, "Intercopy params valid" );
    mzcmt_manch_params_init ( &p );
    CHECK ( mzcmt_manch_params_validate ( &p ) == EXIT_FAILURE, "HALVES 0 invalid" );
    simple ( &p );
    p.pilot_bits = 9;
    CHECK ( mzcmt_manch_params_validate ( &p ) == EXIT_FAILURE, "pilot_bits 9 invalid" );
    simple ( &p );
    ctx_add ( &p, MZCMT_MANCH_CTX_CK_MID, 0, 50 );
    ctx_add ( &p, MZCMT_MANCH_CTX_CK_MID, 0, 60 );
    CHECK ( mzcmt_manch_params_validate ( &p ) == EXIT_FAILURE, "duplicate context invalid" );
    simple ( &p );
    p.carry_adjust = -100;
    CHECK ( mzcmt_manch_params_validate ( &p ) == EXIT_FAILURE, "half2 + carry <= 0 invalid" );
    simple ( &p );
    p.checksum_explicit = 1;
    p.checksum = MZCMT_MANCH_CHECKSUM_NONE;
    CHECK ( mzcmt_manch_params_validate ( &p ) == EXIT_FAILURE, "explicit checksum without algorithm invalid" );
}


/**
 * @brief Načte .hwh ("HWH1", počet, pxclk, dělič, první úroveň, délky v tikách pxCLK) a vrátí půlvlny v T.
 * @param path Soubor. @param out Výstup. @param first_level Úroveň první půlvlny.
 * @return EXIT_SUCCESS, nebo EXIT_FAILURE.
 */
static int load_hwh ( const char *path, st_HALVES *out, int *first_level ) {
    FILE *f = fopen ( path, "rb" );
    if ( !f ) return EXIT_FAILURE;
    uint8_t h[20];
    int ok = fread ( h, 1, 20, f ) == 20 && memcmp ( h, "HWH1", 4 ) == 0;
    uint32_t n = ok ? ( uint32_t ) h[4] | ( uint32_t ) h[5] << 8 | ( uint32_t ) h[6] << 16 | ( uint32_t ) h[7] << 24 : 0;
    *first_level = h[16];
    out->n = 0;
    for ( uint32_t i = 0; ok && i < n && i < MAX_HALVES; i++ ) {
        uint8_t b[4];
        if ( fread ( b, 1, 4, f ) != 4 ) { ok = 0; break; }
        uint32_t px = ( uint32_t ) b[0] | ( uint32_t ) b[1] << 8 | ( uint32_t ) b[2] << 16 | ( uint32_t ) b[3] << 24;
        out->level[i] = *first_level ^ ( int ) ( i & 1 );
        out->dur[i] = ( px + 2 ) / 5;
        out->n++;
    }
    fclose ( f );
    return ok && out->n == n ? EXIT_SUCCESS : EXIT_FAILURE;
}


/**
 * @brief Načte emutest.mzf.
 * @param hdr Hlavička 128 B. @param body Tělo (1024 B). @return Délka těla, 0 při chybě.
 */
static uint32_t load_emutest ( uint8_t *hdr, uint8_t *body ) {
    FILE *f = fopen ( "../tstdata/ref/emutest.mzf", "rb" );
    if ( !f ) return 0;
    size_t a = fread ( hdr, 1, 128, f );
    uint32_t fsize = ( uint32_t ) hdr[0x12] | ( uint32_t ) hdr[0x13] << 8;
    size_t b = ( a == 128 && fsize <= 1024 ) ? fread ( body, 1, fsize, f ) : 0;
    fclose ( f );
    return b == fsize ? fsize : 0;
}


/** @brief (f) Pezik: hlavička + tělo za sebou proti referenci z modelu, přesně na T. */
static void test_ref_pezik ( void ) {
    uint8_t hdr[128], body[1024];
    uint32_t size = load_emutest ( hdr, body );
    CHECK ( size == 1024, "load emutest.mzf" );
    if ( size != 1024 ) return;
    static const struct { uint32_t n; const char *path; } refs[] = {
        { 43, "../tstdata/ref/pezik20a-2400-emutest-model.hwh" },
        { 28, "../tstdata/ref/pezik20a-3200-emutest-model.hwh" },
    };
    for ( size_t r = 0; r < 2; r++ ) {
        st_MZCMT_MANCH_PARAMS ph, pb;
        pezik ( &ph, refs[r].n, 1 );
        pezik ( &pb, refs[r].n, 0 );
        memset ( &g_a, 0, sizeof ( g_a ) );
        int ok = mzcmt_manch_generate ( &ph, hdr, 128, collect, &g_a ) == EXIT_SUCCESS
                 && mzcmt_manch_generate ( &pb, body, size, collect, &g_a ) == EXIT_SUCCESS;
        int first = 0;
        ok = ok && load_hwh ( refs[r].path, &g_b, &first ) == EXIT_SUCCESS;
        /* reference končí posledním OUT: generátor má navíc poslední (otevřenou) půlvlnu */
        uint32_t mism = 0, j;
        for ( j = 0; ok && j < g_b.n && j < g_a.n; j++ ) {
            if ( g_a.level[j] != g_b.level[j] || g_a.dur[j] != g_b.dur[j] ) {
                if ( !mism ) printf ( "  first mismatch at %u: gen %d/%u ref %d/%u\n", j, g_a.level[j], g_a.dur[j],
                                       g_b.level[j], g_b.dur[j] );
                mism++;
            }
        }
        char msg[128];
        snprintf ( msg, sizeof ( msg ), "Pezik N=%u: %u halves vs reference %u, %u mismatches", refs[r].n, g_a.n, g_b.n, mism );
        CHECK ( ok && g_a.n == g_b.n + 1 && mism == 0, msg );
    }
}


/** @brief (g) Intercopy: hlavička a tělo proti záznamu z emulátoru po segmentech, s tolerancí. */
static void test_ref_intercopy ( void ) {
    uint8_t hdr[128], body[1024];
    uint32_t size = load_emutest ( hdr, body );
    if ( size != 1024 ) return;
    hdr[0x11] = 0x0D;   /* Intercopy zapsal hlavičku, jak ji načetl (jako test_tmz_profile.c) */
    int first = 0;
    if ( load_hwh ( "../tstdata/ref/ic102-cpm-2400-emutest.hwh", &g_b, &first ) != EXIT_SUCCESS ) {
        CHECK ( 0, "load Intercopy reference" );
        return;
    }
    /* segmenty: pauza > 100 000 T */
    uint32_t pause_at = 0;
    for ( uint32_t i = 0; i < g_b.n; i++ ) {
        if ( g_b.dur[i] > 100000 ) { pause_at = i; break; }
    }
    CHECK ( pause_at > 0, "reference has one pause" );
    for ( int part = 0; part < 2; part++ ) {
        st_MZCMT_MANCH_PARAMS p;
        intercopy ( &p, !part );
        p.pause = 0;
        memset ( &g_a, 0, sizeof ( g_a ) );
        int ok = mzcmt_manch_generate ( &p, part ? body : hdr, part ? size : 128, collect, &g_a ) == EXIT_SUCCESS;
        /* hlavička: záznam 0..pause_at-1, poslední generovaná LOW splývá s pauzou; tělo: za pauzou, první LOW splývá */
        uint32_t rs = part ? pause_at + 1 : 0;
        uint32_t rn = part ? g_b.n - rs : pause_at;
        uint32_t gs = part ? 1 : 0;
        uint32_t gn = g_a.n - gs - ( part ? 0 : 1 );
        uint32_t m = ( gn < rn ? gn : rn );
        if ( m ) m--;   /* poslední půlvlnu segmentu nesrovnávat (konec záznamu, splynutí) */
        double sum = 0, worst = 0;
        int lvl = 0;
        for ( uint32_t j = 0; ok && j < m; j++ ) {
            if ( g_a.level[gs + j] != g_b.level[rs + j] ) lvl++;
            double d = ( double ) g_b.dur[rs + j] - ( double ) g_a.dur[gs + j];
            sum += fabs ( d );
            if ( j > 0 && fabs ( d ) > worst ) worst = fabs ( d );
        }
        char msg[160];
        snprintf ( msg, sizeof ( msg ), "Intercopy %s: %u vs %u halves, mean |d| %.4f T, worst %.0f T, level mismatches %d",
                   part ? "body" : "header", gn, rn, m ? sum / m : 0.0, worst, lvl );
        int len_ok = gn == rn || gn + 1 == rn || gn == rn + 1;
        CHECK ( ok && len_ok && m && sum / m <= 0.05 && worst <= 30 && lvl == 0, msg );
    }
}


/** @brief (h) Převod na vzorky: dvě části za sebou, součet vzorků podle akumulovaného času. */
static void test_vstream ( void ) {
    st_MZCMT_MANCH_PARAMS ph, pb;
    intercopy ( &ph, 1 );
    intercopy ( &pb, 0 );
    uint8_t hdr[128], body[16];
    for ( int i = 0; i < 128; i++ ) hdr[i] = ( uint8_t ) ( i * 3 );
    for ( int i = 0; i < 16; i++ ) body[i] = ( uint8_t ) ( 0xF0 + i );
    uint64_t th = 0, tb = 0;
    memset ( &g_a, 0, sizeof ( g_a ) );
    mzcmt_manch_generate ( &ph, hdr, 128, collect, &g_a );
    for ( uint32_t i = 0; i < g_a.n; i++ ) th += g_a.dur[i];
    memset ( &g_a, 0, sizeof ( g_a ) );
    mzcmt_manch_generate ( &pb, body, 16, collect, &g_a );
    for ( uint32_t i = 0; i < g_a.n; i++ ) tb += g_a.dur[i];
    st_CMT_VSTREAM *vs = mzcmt_manch_create_vstream ( &ph, hdr, 128, 44100 );
    int ok = vs && mzcmt_manch_append_vstream ( vs, &pb, body, 16 ) == EXIT_SUCCESS;
    uint64_t expect = ( ( th + tb ) * 44100 + 3546895 / 2 ) / 3546895;
    uint64_t got = ok ? cmt_vstream_get_count_scans ( vs ) : 0;
    char msg[128];
    snprintf ( msg, sizeof ( msg ), "vstream samples %llu vs %llu (+-1)", ( unsigned long long ) got, ( unsigned long long ) expect );
    CHECK ( ok && ( got == expect || got + 1 == expect || got == expect + 1 ), msg );
    cmt_vstream_destroy ( vs );
}


int main ( void ) {
    test_structure ();
    test_contexts ();
    test_carry ();
    test_lengths ();
    test_validate ();
    test_vstream ();
    if ( !tstdata_skip ( "test_mzcmt_manchester (references)" ) ) {
        test_ref_pezik ();
        test_ref_intercopy ();
    }
    printf ( "\n=== MZCMT Manchester Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
