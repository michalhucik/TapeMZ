/**
 * @file   tmz_ref_compare.h
 * @brief  Sdílené porovnání bloku 0x46 se záznamem z emulátoru (testy TMZ v1.1).
 *
 * Blok se přehraje generátorem své rodiny (FM nebo MANCHESTER, bez playeru,
 * v T-stavech CPU) a každá část se porovná se segmentem záznamu .hwh (tstdata/ref;
 * LE: "HWH1", počet půlvln, pxclk, dělič, první úroveň, pak délky v tikách
 * pxCLK). Segmenty oddělují pauzy (půlvlna delší
 * než REF_PAUSE_T); začíná-li záznam čekáním před uložením, první segment
 * je až za ním. Začíná-li segment jinou úrovní než část (pauza HIGH, první
 * půlvlna HIGH splynula s pauzou), první generovaná půlvlna se přeskočí.
 * Kritéria (spec kap. 12): exact = shoda každé půlvlny na T;
 * jinak struktura (počet půlvln +-1), průměrná odchylka do 0,05 jednotky
 * časové základny a odchylka každé půlvlny kromě první v části do 30 T.
 *
 * ref_compare_concat() porovná celý blok najednou (všechny části za sebou,
 * stejné úrovně na hranicích sloučené) - pro záznamy bez pauz mezi částmi
 * (Pezik TAPE.COM).
 *
 * Hlavička obsahuje statické funkce; includuje ji vždy jen jeden testovací
 * soubor programu.
 */

#ifndef TMZ_REF_COMPARE_H
#define TMZ_REF_COMPARE_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/tzx/tzx.h"
#include "libs/tmz/tmz_mzfile.h"
#include "libs/tmz/tmz_part_fm.h"
#include "libs/mzcmt_fm/mzcmt_fm.h"
#include "libs/tmz/tmz_part_manchester.h"
#include "libs/mzcmt_manchester/mzcmt_manchester.h"

/** @brief Takt CPU MZ-800 (T-stavy/s). */
#define REF_CPU_HZ 3546895.0
/** @brief Půlvlna delší než tato hodnota (T) je pauza. */
#define REF_PAUSE_T 20000.0
/** @brief Nejvíc segmentů záznamu. */
#define REF_MAX_SEG 64

/**
 * @brief Dynamické pole délek (T).
 * @invariant n <= cap
 */
typedef struct st_REF_ARR {
    double *v;     /**< hodnoty */
    uint32_t n;    /**< počet */
    uint32_t cap;  /**< kapacita */
} st_REF_ARR;

/**
 * @brief Přidá hodnotu.
 * @param a Pole.
 * @param x Hodnota.
 * @return EXIT_SUCCESS, nebo EXIT_FAILURE při selhání alokace.
 */
static int ref_arr_push ( st_REF_ARR *a, double x ) {
    if ( a->n == a->cap ) {
        uint32_t c = a->cap ? a->cap * 2 : 4096;
        double *n = realloc ( a->v, c * sizeof ( double ) );
        if ( !n ) return EXIT_FAILURE;
        a->v = n;
        a->cap = c;
    }
    a->v[a->n++] = x;
    return EXIT_SUCCESS;
}

/** @brief Kontext callbacku: převod jednotek části na T. */
typedef struct st_REF_COLLECT {
    st_REF_ARR *arr;  /**< cíl: délky v T */
    st_REF_ARR *lv;   /**< cíl: úrovně (0/1), stejný index jako arr */
    double k;         /**< T na jednotku */
    int invert;       /**< 1 = úrovně ukládat převrácené (mutace testu) */
} st_REF_COLLECT;

/**
 * @brief Callback generátoru: uloží půlvlnu v T a její úroveň.
 * @param user st_REF_COLLECT.
 * @param level Úroveň půlvlny.
 * @param duration Délka v jednotkách části.
 * @return Výsledek ref_arr_push().
 */
static int ref_collect ( void *user, int level, uint32_t duration ) {
    st_REF_COLLECT *c = user;
    if ( ref_arr_push ( c->lv, ( double ) ( ( level ? 1 : 0 ) ^ c->invert ) ) != EXIT_SUCCESS ) return EXIT_FAILURE;
    return ref_arr_push ( c->arr, duration * c->k );
}

/**
 * @brief Přečte DWORD LE.
 * @param b Bajty.
 * @return Hodnota.
 */
static uint32_t ref_rd32 ( const uint8_t *b ) {
    return b[0] | ( b[1] << 8 ) | ( b[2] << 16 ) | ( ( uint32_t ) b[3] << 24 );
}

/**
 * @brief Načte .hwh a vrátí půlvlny v T.
 * @param path Soubor.
 * @param out Výstup (volající uvolní out->v).
 * @param[out] first_level Úroveň první půlvlny záznamu (úrovně se střídají).
 * @return EXIT_SUCCESS, nebo EXIT_FAILURE.
 */
static int ref_load_hwh ( const char *path, st_REF_ARR *out, int *first_level ) {
    FILE *f = fopen ( path, "rb" );
    if ( !f ) return EXIT_FAILURE;
    uint8_t h[20];
    if ( fread ( h, 1, 20, f ) != 20 || memcmp ( h, "HWH1", 4 ) != 0 ) {
        fclose ( f );
        return EXIT_FAILURE;
    }
    uint32_t n = ref_rd32 ( h + 4 );
    uint32_t div = ref_rd32 ( h + 12 );
    *first_level = h[16] ? 1 : 0;
    for ( uint32_t i = 0; i < n; i++ ) {
        uint8_t b[4];
        if ( fread ( b, 1, 4, f ) != 4 || ref_arr_push ( out, ( double ) ref_rd32 ( b ) / div ) != EXIT_SUCCESS ) {
            fclose ( f );
            return EXIT_FAILURE;
        }
    }
    fclose ( f );
    return EXIT_SUCCESS;
}

/**
 * @brief Přehraje blok 0x46 a porovná ho se záznamem po částech.
 *
 * @param b Blok 0x46.
 * @param hwh_path Záznam .hwh.
 * @param exact 1 = shoda na T, 0 = meze TurboCopy.
 * @param mutate 1 = před generováním změnit SHORT druhou půlvlnu o 1, 2 = převrátit úrovně generovaných
 *        půlvln (ověření, že test umí selhat).
 * @param failures Čítač selhání (zvýší se o každou neshodu).
 * @param label Popis do výpisu.
 */
static void ref_compare_block ( const st_TZX_BLOCK *b, const char *hwh_path, int exact, int mutate,
                                int *failures, const char *label ) {
    st_REF_ARR r = { 0 };
    st_TMZ_MZFILE mf;
    if ( tmz_mzfile_parse ( b, &mf ) != TZX_OK ) {
        printf ( "FAIL: %s: parse block\n", label );
        ( *failures )++;
        return;
    }
    int first_level = 1;
    if ( ref_load_hwh ( hwh_path, &r, &first_level ) != EXIT_SUCCESS || r.n == 0 ) {
        printf ( "FAIL: %s: load reference %s\n", label, hwh_path );
        ( *failures )++;
        free ( r.v );
        tmz_mzfile_free ( &mf );
        return;
    }
    /* segmenty záznamu: za každou pauzou; začíná-li záznam signálem, první segment od 0 */
    uint32_t starts[REF_MAX_SEG], nseg = 0;
    if ( r.v[0] <= REF_PAUSE_T ) starts[nseg++] = 0;
    for ( uint32_t i = 0; i < r.n && nseg < REF_MAX_SEG; i++ ) {
        if ( r.v[i] > REF_PAUSE_T ) starts[nseg++] = i + 1;
    }
    if ( nseg < mf.part_count ) {
        printf ( "FAIL: %s: %u parts vs %u segments\n", label, mf.part_count, nseg );
        ( *failures )++;
    } else {
        printf ( "OK: %s: %u parts vs %u segments\n", label, mf.part_count, nseg );
    }

    static st_MZCMT_FM_PARAMS p;
    static st_MZCMT_MANCH_PARAMS mp;
    for ( uint8_t k = 0; k < mf.part_count && k < nseg; k++ ) {
        st_TMZ_PART_COMMON c;
        uint32_t size;
        const uint8_t *data = tmz_mzfile_part_data ( &mf, &mf.parts[k], &size, NULL );
        int is_m = mf.parts[k].family == TMZ_FAMILY_MANCHESTER;
        en_TMZ_PART_ERROR pe;
        uint32_t tbase;
        int plevel;
        if ( is_m ) {
            pe = tmz_part_manchester_decode ( mf.parts[k].tlv, mf.parts[k].tlv_length, 3546895, &mp, &c );
            mp.pause = 0;
            if ( mutate == 1 ) mp.half2[0]++;
            tbase = mp.timebase;
            plevel = mzcmt_manch_first_level ( &mp, data, size );
        } else {
            pe = tmz_part_fm_decode ( mf.parts[k].tlv, mf.parts[k].tlv_length, 3546895, &p, &c );
            p.pause = 0;   /* pauza splývá s druhou půlvlnou posledního pulzu, srovnává se bez ní */
            if ( mutate == 1 ) p.short_second++;
            tbase = p.timebase;
            plevel = p.level;
        }
        st_REF_ARR g = { 0 };
        st_REF_ARR gl = { 0 };
        st_REF_COLLECT col = { &g, &gl, pe == TMZ_PART_OK ? REF_CPU_HZ / tbase : 1.0, mutate == 2 };
        int ok = data && pe == TMZ_PART_OK
                 && ( is_m ? mzcmt_manch_generate ( &mp, data, size, ref_collect, &col )
                           : mzcmt_fm_generate ( &p, data, size, ref_collect, &col ) ) == EXIT_SUCCESS
                 && g.n > 1;
        uint32_t seg_end = ( ( uint32_t ) k + 1 < nseg ) ? starts[k + 1] : r.n;   /* pauza patří do segmentu */
        uint32_t seg_n = seg_end - starts[k];
        /*
         * Začíná-li segment záznamu jinou úrovní než část (pauza HIGH u Intercopy:
         * první půlvlna HIGH splyne s pauzou), první generovanou půlvlnu přeskočit.
         */
        int seg_level = first_level ^ ( int ) ( starts[k] & 1 );
        uint32_t go = ( ok && seg_level != plevel ) ? 1 : 0;
        uint32_t gn = ok ? g.n - go : 0;
        int same_len = ok && ( gn == seg_n || gn + 1 == seg_n || gn == seg_n + 1 );
        uint32_t m = ( ok && seg_n > 0 && gn > 0 ) ? ( ( gn < seg_n ? gn : seg_n ) - 1 ) : 0;   /* poslední druhou půlvlnu nesrovnávat */
        double sum = 0, worst = 0;
        int mism = 0;
        uint32_t lvl_mism = 0;   /* úroveň generované půlvlny proti záznamu (úrovně záznamu se střídají) */
        for ( uint32_t j = 0; j < m; j++ ) {
            int rec_level = first_level ^ ( int ) ( ( starts[k] + j ) & 1 );
            if ( ( int ) gl.v[j + go] != rec_level ) lvl_mism++;
            double d = r.v[starts[k] + j] - g.v[j + go];
            if ( exact ) {
                if ( lround ( r.v[starts[k] + j] ) != lround ( g.v[j + go] ) ) mism++;
            } else if ( j > 0 ) {
                sum += d;
                if ( fabs ( d ) > worst ) worst = fabs ( d );
            }
        }
        int pass;
        if ( exact ) {
            pass = ok && same_len && mism == 0 && lvl_mism == 0;
            printf ( "%s: %s part %u: %u halves, exact on T (%d mismatches, %u level mismatches)\n", pass ? "OK" : "FAIL",
                     label, k, g.n, mism, lvl_mism );
        } else {
            double mean_units = m > 1 ? ( sum / ( m - 1 ) ) / ( REF_CPU_HZ / tbase ) : 0;
            pass = ok && same_len && fabs ( mean_units ) <= 0.05 && worst <= 30.0 && lvl_mism == 0;
            printf ( "%s: %s part %u: %u halves, mean %.4f units, max |dev| %.1f T, %u level mismatches\n",
                     pass ? "OK" : "FAIL", label, k, g.n, mean_units, worst, lvl_mism );
        }
        if ( !pass ) ( *failures )++;
        free ( g.v );
        free ( gl.v );
    }
    free ( r.v );
    tmz_mzfile_free ( &mf );
}


/**
 * @brief Porovná celý blok (všechny části za sebou) s celým záznamem .hwh.
 *
 * Části se vygenerují svou rodinou včetně pauz; sousední půlvlny stejné
 * úrovně (i přes hranici částí) se sloučí. Porovná se úroveň a délka každé
 * půlvlny kromě poslední (záznam končí posledním zápisem, generátor má
 * navíc otevřenou poslední půlvlnu). Počet půlvln se smí lišit o 1.
 *
 * @param b Blok 0x46.
 * @param hwh_path Záznam .hwh.
 * @param exact 1 = shoda na T, 0 = meze jako ref_compare_block (průměr do
 *        0,05 T, každá půlvlna kromě první do 30 T).
 * @param failures Čítač selhání.
 * @param label Popis do výpisu.
 */
__attribute__ ( ( unused ) ) static void ref_compare_concat ( const st_TZX_BLOCK *b, const char *hwh_path, int exact, int *failures, const char *label ) {
    st_REF_ARR r = { 0 }, g = { 0 }, gl = { 0 };
    st_TMZ_MZFILE mf;
    int first_level = 1;
    if ( tmz_mzfile_parse ( b, &mf ) != TZX_OK ) {
        printf ( "FAIL: %s: parse block\n", label );
        ( *failures )++;
        return;
    }
    int ok = ref_load_hwh ( hwh_path, &r, &first_level ) == EXIT_SUCCESS && r.n > 0;
    static st_MZCMT_FM_PARAMS p;
    static st_MZCMT_MANCH_PARAMS mp;
    for ( uint8_t k = 0; ok && k < mf.part_count; k++ ) {
        st_TMZ_PART_COMMON c;
        uint32_t size;
        const uint8_t *data = tmz_mzfile_part_data ( &mf, &mf.parts[k], &size, NULL );
        st_REF_ARR pg = { 0 }, pl = { 0 };
        int is_m = mf.parts[k].family == TMZ_FAMILY_MANCHESTER;
        en_TMZ_PART_ERROR pe = is_m ? tmz_part_manchester_decode ( mf.parts[k].tlv, mf.parts[k].tlv_length, 3546895, &mp, &c )
                                    : tmz_part_fm_decode ( mf.parts[k].tlv, mf.parts[k].tlv_length, 3546895, &p, &c );
        st_REF_COLLECT col = { &pg, &pl, pe == TMZ_PART_OK ? REF_CPU_HZ / ( is_m ? mp.timebase : p.timebase ) : 1.0, 0 };
        ok = data && pe == TMZ_PART_OK
             && ( is_m ? mzcmt_manch_generate ( &mp, data, size, ref_collect, &col )
                       : mzcmt_fm_generate ( &p, data, size, ref_collect, &col ) ) == EXIT_SUCCESS;
        /* sloučit stejné úrovně, i na hranici s předchozí částí */
        for ( uint32_t j = 0; ok && j < pg.n; j++ ) {
            if ( g.n && gl.v[gl.n - 1] == pl.v[j] ) g.v[g.n - 1] += pg.v[j];
            else ok = ref_arr_push ( &g, pg.v[j] ) == EXIT_SUCCESS && ref_arr_push ( &gl, pl.v[j] ) == EXIT_SUCCESS;
        }
        free ( pg.v );
        free ( pl.v );
    }
    uint32_t m = ( g.n < r.n ? g.n : r.n );
    if ( m ) m--;
    uint32_t mism = 0, lvl = 0;
    double sum = 0, worst = 0;
    for ( uint32_t j = 0; ok && j < m; j++ ) {
        if ( ( int ) gl.v[j] != ( first_level ^ ( int ) ( j & 1 ) ) ) lvl++;
        double d = r.v[j] - g.v[j];
        if ( lround ( r.v[j] ) != lround ( g.v[j] ) ) mism++;
        if ( j > 0 ) {
            sum += fabs ( d );
            if ( fabs ( d ) > worst ) worst = fabs ( d );
        }
    }
    int same_len = g.n == r.n || g.n == r.n + 1 || g.n + 1 == r.n;
    int pass = ok && same_len && lvl == 0 && ( exact ? mism == 0 : ( m > 1 && sum / ( m - 1 ) <= 0.05 && worst <= 30.0 ) );
    printf ( "%s: %s: %u halves vs %u, %u mismatches on T, max |dev| %.1f T, %u level mismatches\n", pass ? "OK" : "FAIL",
             label, g.n, r.n, mism, worst, lvl );
    if ( !pass ) ( *failures )++;
    free ( r.v );
    free ( g.v );
    free ( gl.v );
    tmz_mzfile_free ( &mf );
}

#endif /* TMZ_REF_COMPARE_H */
