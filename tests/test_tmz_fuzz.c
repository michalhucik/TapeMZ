/**
 * @file   test_tmz_fuzz.c
 * @brief  Náhodné mutace bloku 0x46 (č. 6):
 *         tmz_mzfile_parse(), tmz_mzfile_part_data(), tmz_part_fm_decode()
 *         a tmz_part_fm_check_reader() nad poškozenými daty nespadnou.
 *
 * Výchozí bloky: vektory tstdata/tmz-v2 a blok z každého profilu registru.
 * Mutace:
 * - náhodné: přepis 1 až 4 bajtů, useknutí, prodloužení náhodnými bajty;
 * - cílené: jeden bajt pole struktury bloku (L, verze, n, N, T,
 *   P, délka části, role/rodina/zdroj), hlavičky položky TLV, nebo 1 až 4
 *   bajty hodnoty TLV; bajt dostane náhodnou hodnotu, nebo +-1 (hraniční
 *   délky). Cíle se počítají z platného výchozího bloku.
 *
 * Spočítané offsety polí se ověří proti hodnotám z parseru (collect_targets()),
 * chyba výpočtu offsetů je FAIL. Cílené mutace polí struktury a hlaviček
 * TLV navíc musí aspoň jednou skončit odmítnutím parseru.
 *
 * Generátor je LCG s pevným semínkem, běh je opakovatelný. Kopie dat má
 * přesně délku bloku (malloc), takže čtení mimo buffer by odhalil ASan
 * nebo valgrind; samotný test zachytí jen pád.
 *
 * Spouštět z build/ (vstupy ../tstdata/tmz-v2/).
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
#include "libs/tmz/tmz_profile.h"
#include "libs/tmz/tmz_tlv.h"
#include "libs/mzf/mzf.h"
#include "libs/generic_driver/generic_driver.h"
#include "tstdata_skip.h"

/** @brief Počet mutací na jeden výchozí blok. */
#define MUTATIONS_PER_BLOCK 10000

/** @brief Nejvýš výchozích bloků. */
#define MAX_SEEDS 16

/** @brief Počet druhů mutací (3 náhodné + 3 cílené). */
#define OP_COUNT 6

/** @brief Nejvýš cílů jednoho druhu u jednoho výchozího bloku (další se ignorují). */
#define MAX_TARGETS 256

/** @brief Stav generátoru (LCG, pevné semínko). */
static uint32_t g_rng = 0x12345678u;


/**
 * @brief Další pseudonáhodné číslo (LCG Numerical Recipes, horních 16 bitů).
 * @return Číslo 0 až 65535.
 */
static uint32_t rnd ( void ) {
    g_rng = g_rng * 1664525u + 1013904223u;
    return g_rng >> 16;
}


/**
 * @brief Rozsah bajtů výchozího bloku, který cílená mutace mění.
 * @invariant off + len <= délka výchozího bloku, len >= 1.
 */
typedef struct st_RANGE {
    uint32_t off;   /**< offset v datech bloku (data začínají 4B délkou L) */
    uint32_t len;   /**< počet bajtů */
} st_RANGE;


/**
 * @brief Cíle cílených mutací jednoho výchozího bloku (spec 4.1, 4.2, 5).
 *
 * Offsety se počítají z platného výchozího bloku přes tmz_mzfile_parse(),
 * takže odpovídají jeho skutečnému rozvržení.
 */
typedef struct st_TARGETS {
    st_RANGE field[MAX_TARGETS];     /**< pole struktury: L, verze, n, N, T, P, délka a role/rodina/zdroj části */
    uint32_t field_count;            /**< počet platných field */
    st_RANGE tlv_head[MAX_TARGETS];  /**< hlavičky položek TLV (tag a délka, 4 B) */
    uint32_t tlv_head_count;         /**< počet platných tlv_head */
    st_RANGE tlv_value[MAX_TARGETS]; /**< hodnoty položek TLV (jen neprázdné) */
    uint32_t tlv_value_count;        /**< počet platných tlv_value */
} st_TARGETS;


/**
 * @brief Přidá rozsah do pole cílů (při plném poli nic neudělá).
 * @param arr Pole. @param count Počet prvků (zvýší se). @param off Offset. @param len Délka (0 = nic).
 */
static void add_range ( st_RANGE *arr, uint32_t *count, uint32_t off, uint32_t len ) {
    if ( len == 0 || *count >= MAX_TARGETS ) return;
    arr[*count].off = off;
    arr[*count].len = len;
    ( *count )++;
}


/**
 * @brief Přidá položky jedné oblasti TLV (hlavičky a neprázdné hodnoty).
 * @param t Cíle. @param data Data bloku. @param tlv Oblast (ukazuje do data). @param length Délka oblasti.
 */
static void add_tlv_area ( st_TARGETS *t, const uint8_t *data, const uint8_t *tlv, uint32_t length ) {
    st_TMZ_TLV_ITER it;
    st_TMZ_TLV v;
    if ( !tlv || !length ) return;
    tmz_tlv_iter_init ( &it, tlv, length );
    while ( tmz_tlv_iter_next ( &it, &v ) == 1 ) {
        uint32_t voff = ( uint32_t ) ( v.value - data );
        add_range ( t->tlv_head, &t->tlv_head_count, voff - 4, 4 );
        add_range ( t->tlv_value, &t->tlv_value_count, voff, v.length );
    }
}


/**
 * @brief Spočítá cíle cílených mutací z platného výchozího bloku.
 *
 * Spočítané offsety ověří proti výsledku parseru: na offsetu N, T a P
 * a na začátku každé části musí ležet hodnota, kterou parser přečetl
 * (délka těla, délka blokových TLV, počet částí, délka části). Chyba
 * výpočtu offsetů se tak projeví hned, ne až slabší statistikou mutací.
 *
 * @param b Výchozí blok (musí projít tmz_mzfile_parse()).
 * @param[out] t Cíle (při neúspěchu nebo neshodě offsetů prázdné).
 * @return 1 = hotovo, 0 = blok nejde naparsovat, -1 = spočítané offsety
 *         neodpovídají bloku (chyba v collect_targets).
 */
static int collect_targets ( const st_TZX_BLOCK *b, st_TARGETS *t ) {
    st_TMZ_MZFILE mf;
    memset ( t, 0, sizeof ( *t ) );
    if ( tmz_mzfile_parse ( b, &mf ) != TZX_OK ) return 0;
    const uint8_t *d = b->data;
    uint32_t hoff = ( uint32_t ) ( mf.header - d );
    uint32_t toff = hoff + 128 + 4 + mf.body_size;          /* WORD T */
    uint32_t poff = toff + 2 + mf.block_tlv_length;         /* BYTE P */
    if ( poff >= b->length || tmz_rd32 ( d + hoff + 128 ) != mf.body_size || tmz_rd16 ( d + toff ) != mf.block_tlv_length
         || d[poff] != mf.part_count ) {
        tmz_mzfile_free ( &mf );
        memset ( t, 0, sizeof ( *t ) );
        return -1;
    }
    add_range ( t->field, &t->field_count, 0, 4 );          /* L */
    add_range ( t->field, &t->field_count, 4, 1 );          /* verze */
    add_range ( t->field, &t->field_count, 5, 1 );          /* n */
    add_range ( t->field, &t->field_count, hoff + 128, 4 ); /* N */
    add_range ( t->field, &t->field_count, toff, 2 );       /* T */
    add_range ( t->field, &t->field_count, poff, 1 );       /* P */
    add_tlv_area ( t, d, mf.block_tlv, mf.block_tlv_length );
    uint32_t start = poff + 1;
    for ( uint8_t k = 0; k < mf.part_count; k++ ) {
        add_range ( t->field, &t->field_count, start, 4 );     /* délka části */
        add_range ( t->field, &t->field_count, start + 4, 3 ); /* role, rodina, zdroj */
        if ( !mf.parts[k].tlv ) break;   /* neznámý zdroj: konec části nejde určit */
        uint32_t end = ( uint32_t ) ( mf.parts[k].tlv - d ) + mf.parts[k].tlv_length;
        if ( start + 4 > end || start + 4 + tmz_rd32 ( d + start ) != end ) {   /* délka části končí s její TLV */
            tmz_mzfile_free ( &mf );
            memset ( t, 0, sizeof ( *t ) );
            return -1;
        }
        add_tlv_area ( t, d, mf.parts[k].tlv, mf.parts[k].tlv_length );
        start = end;
    }
    tmz_mzfile_free ( &mf );
    return 1;
}


/**
 * @brief Změní jeden bajt v rozsahu: náhodná hodnota, nebo +-1 (hraniční délky).
 * @param d Kopie bloku. @param r Rozsah (uvnitř kopie).
 */
static void mutate_range ( uint8_t *d, const st_RANGE *r ) {
    uint32_t i = r->off + rnd () % r->len;
    uint32_t how = rnd () % 3;
    if ( how == 0 ) d[i] = ( uint8_t ) rnd ();
    else if ( how == 1 ) d[i]++;
    else d[i]--;
}


/**
 * @brief Načte soubor .tmz a vrátí kopii prvního bloku.
 * @param path Cesta k souboru.
 * @return Blok (uvolnit tmz_block_free), nebo NULL.
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
    if ( !f || f->block_count < 1 ) {
        if ( f ) tzx_free ( f );
        return NULL;
    }
    st_TZX_BLOCK *b = malloc ( sizeof ( st_TZX_BLOCK ) );
    uint8_t *data = malloc ( f->blocks[0].length );
    if ( !b || !data ) {
        free ( b );
        free ( data );
        tzx_free ( f );
        return NULL;
    }
    b->id = f->blocks[0].id;
    b->length = f->blocks[0].length;
    b->data = data;
    memcpy ( b->data, f->blocks[0].data, b->length );
    tzx_free ( f );
    return b;
}


/**
 * @brief Projde blok všemi čtecími funkcemi (výsledek se nehodnotí, jen nesmí spadnout).
 * @param b Blok.
 * @return 1 = parse přijal blok, 0 = odmítl.
 */
static int exercise ( const st_TZX_BLOCK *b ) {
    st_TMZ_MZFILE mf;
    if ( tmz_mzfile_parse ( b, &mf ) != TZX_OK ) return 0;
    static st_MZCMT_FM_PARAMS p;
    for ( uint8_t k = 0; k < mf.part_count; k++ ) {
        uint32_t size = 0;
        en_TMZ_PART_ERROR pe;
        const uint8_t *data = tmz_mzfile_part_data ( &mf, &mf.parts[k], &size, &pe );
        volatile uint8_t sink = 0;
        if ( data && size ) sink = data[size - 1];   /* poslední bajt části musí být čitelný */
        ( void ) sink;
        st_TMZ_PART_COMMON c;
        if ( mf.parts[k].family == TMZ_FAMILY_FM
             && tmz_part_fm_decode ( mf.parts[k].tlv, mf.parts[k].tlv_length, 3546895, &p, &c ) == TMZ_PART_OK ) {
            st_TMZ_READER_MARGINS m;
            ( void ) tmz_part_fm_check_reader ( &p, &c, 44100, &m );
        }
    }
    tmz_mzfile_free ( &mf );
    return 1;
}


int main ( void ) {
    if ( tstdata_skip ( "test_tmz_fuzz" ) ) return 0;
    st_TZX_BLOCK *seeds[MAX_SEEDS];
    int n = 0;
    static const char *const vectors[] = { "v1-rom-800-nominal", "v2-tc-2to1-loader-n", "v3-tc-3to1-loader-y",
                                           "v4-custom-x", "v6-rom-800-exact" };
    for ( size_t i = 0; i < sizeof ( vectors ) / sizeof ( vectors[0] ); i++ ) {
        char path[128];
        snprintf ( path, sizeof ( path ), "../tstdata/tmz-v2/%s.tmz", vectors[i] );
        st_TZX_BLOCK *b = load_first_block ( path );
        if ( b ) seeds[n++] = b;
        else printf ( "FAIL: load %s\n", path );
    }
    static uint8_t body[258];
    st_MZF_HEADER h;
    memset ( &h, 0, sizeof ( h ) );
    h.ftype = 1;
    h.fsize = sizeof ( body );
    h.fstrt = 0x1200;
    h.fexec = 0x1200;
    for ( size_t i = 0; i < tmz_profile_count () && n < MAX_SEEDS; i++ ) {
        st_TMZ_PROFILE_ARGS a;
        tmz_profile_args_init ( &a );
        const char *id = tmz_profile_get ( i )->id;
        if ( strcmp ( id, "TAPEMZ-V1-LEGACY" ) == 0 ) a.legacy_format = TMZ_LEGACY_NORMAL;
        st_TZX_BLOCK *b = tmz_profile_build_block ( id, &a, &h, body, sizeof ( body ), NULL );
        if ( b ) seeds[n++] = b;
        else printf ( "FAIL: build %s\n", id );
    }
    int failures = ( n != ( int ) ( sizeof ( vectors ) / sizeof ( vectors[0] ) + tmz_profile_count () ) );

    static st_TARGETS tg;   /* static: ~6 kB */
    uint32_t accepted = 0, total = 0;
    uint32_t total_op[OP_COUNT] = { 0 }, rejected_op[OP_COUNT] = { 0 };
    for ( int s = 0; s < n; s++ ) {
        if ( !exercise ( seeds[s] ) ) {
            printf ( "FAIL: seed %d not accepted unmodified\n", s );
            failures++;
        }
        if ( collect_targets ( seeds[s], &tg ) != 1 ) {
            printf ( "FAIL: seed %d: computed field offsets do not match the block\n", s );
            failures++;
        }
        for ( int it = 0; it < MUTATIONS_PER_BLOCK; it++ ) {
            uint32_t len = seeds[s]->length;
            uint32_t op = rnd () % OP_COUNT;
            if ( ( op == 3 && !tg.field_count ) || ( op == 4 && !tg.tlv_head_count ) || ( op == 5 && !tg.tlv_value_count ) ) {
                op = 0;   /* blok nemá cíle tohoto druhu */
            }
            if ( op == 1 ) len = len ? rnd () % len : 0;   /* useknutí */
            else if ( op == 2 ) len += 1 + rnd () % 16;      /* prodloužení */
            uint8_t *d = malloc ( len ? len : 1 );
            if ( !d ) return 1;
            uint32_t keep = len < seeds[s]->length ? len : seeds[s]->length;
            memcpy ( d, seeds[s]->data, keep );
            for ( uint32_t j = keep; j < len; j++ ) d[j] = ( uint8_t ) rnd ();
            if ( op == 0 && len ) {
                uint32_t cnt = 1 + rnd () % 4;
                for ( uint32_t j = 0; j < cnt; j++ ) d[rnd () % len] = ( uint8_t ) rnd ();
            } else if ( op == 3 ) {
                mutate_range ( d, &tg.field[rnd () % tg.field_count] );
            } else if ( op == 4 ) {
                mutate_range ( d, &tg.tlv_head[rnd () % tg.tlv_head_count] );
            } else if ( op == 5 ) {
                const st_RANGE *r = &tg.tlv_value[rnd () % tg.tlv_value_count];
                uint32_t cnt = 1 + rnd () % 4;
                for ( uint32_t j = 0; j < cnt; j++ ) mutate_range ( d, r );
            }
            st_TZX_BLOCK mb = { TMZ_BLOCK_ID_MZ_FILE, len, d };
            int ok = exercise ( &mb );
            accepted += ( uint32_t ) ok;
            total++;
            total_op[op]++;
            if ( !ok ) rejected_op[op]++;
            free ( d );
        }
    }
    printf ( "OK: %u mutations, %u accepted by the parser\n", total, accepted );
    static const char *const op_name[OP_COUNT] = { "random bytes", "truncate", "extend", "structure field",
                                                   "TLV item header", "TLV value" };
    for ( int o = 0; o < OP_COUNT; o++ ) {
        printf ( "  %-16s %6u mutations, %6u rejected\n", op_name[o], total_op[o], rejected_op[o] );
    }
    for ( int o = 3; o <= 4; o++ ) {
        if ( total_op[o] == 0 || rejected_op[o] == 0 ) {
            printf ( "FAIL: targeted mutation '%s' never hit a validated field\n", op_name[o] );
            failures++;
        }
    }
    for ( int s = 0; s < n; s++ ) tmz_block_free ( seeds[s] );

    printf ( "\n=== TMZ Fuzz Test: %s ===\n", failures ? "FAIL" : "PASS" );
    return failures ? 1 : 0;
}
