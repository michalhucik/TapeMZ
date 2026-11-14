/**
 * @file   test_wav_analyzer_pulses.c
 * @brief  Testy wav_analyzer_analyze_pulses(): analýza nad posloupností pulzů.
 *
 * Tři skupiny kontrol:
 * 1. Shoda s analýzou WAV: pro nahrávky z tstdata/ se porovná výsledek
 *    wav_analyzer_analyze() s wav_analyzer_analyze_pulses() nad pulzy téhož
 *    souboru (stejné předzpracování a extrakce pulzů). Musí sedět počet
 *    souborů, formát, stavy CRC, bajty MZF, pozice v pulzech, surové bloky.
 *    Bez tstdata/ se skupina přeskočí (SKIP), ostatní běží.
 * 2. Jemný čas: signál formátu ROM (2x hlavička + 2x tělo 1 KB) z mztape
 *    při 17 721 600, 14 336 640 a 8 860 800 Hz převedený na pulzy se dekóduje
 *    jako jeden soubor NORMAL s platnými CRC a MZF shodným se vstupem.
 *    Záznamy hran z emulátoru (tstdata/ref, .hwh: ROM, TurboCopy, Intercopy)
 *    při pxCLK záznamu i při 17 721 600 Hz: formát, CRC, tělo, zapisovač.
 * 3. Meze: prázdná posloupnost, jeden pulz, NULL parametry, nulová
 *    frekvence, pulzy s nulovou délkou (přeskočí se, sousední pulzy téže
 *    úrovně se sloučí).
 *
 * Spouštět z build/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "libs/generic_driver/generic_driver.h"
#include "libs/wav/wav.h"
#include "libs/mzf/mzf.h"
#include "libs/mztape/mztape.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/wav_analyzer/wav_analyzer.h"
#include "tstdata_skip.h"


/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše výsledek; při selhání zvýší g_failures. */
#define CHECK( cond, ... ) do { \
    if ( cond ) { printf ( "OK: " ); printf ( __VA_ARGS__ ); printf ( "\n" ); } \
    else { printf ( "FAIL: " ); printf ( __VA_ARGS__ ); printf ( "\n" ); g_failures++; } \
} while ( 0 )

/** @brief Kontrola bez výpisu při úspěchu (pro porovnání mnoha položek). */
#define EXPECT( cond, ... ) do { \
    if ( !( cond ) ) { printf ( "FAIL: " ); printf ( __VA_ARGS__ ); printf ( "\n" ); g_failures++; } \
} while ( 0 )


/* =========================================================================
 *  1. Shoda s analýzou WAV
 * ========================================================================= */

/** @brief Nahrávky pro test shody (cesty relativně k build/). */
static const char *g_wav_files[] = {
    "../tstdata/tc-normal1.wav",            /* NORMAL 1:1 až 5:4, zapisovač TurboCopy */
    "../tstdata/ic-normal.wav",             /* NORMAL Intercopy 1200 až 3200 Bd */
    "../tstdata/tc-loader-all.wav",         /* TurboCopy TURBO (předzavaděč) */
    "../tstdata/basic-bsd-incomplete.wav",  /* poškozený záznam: BSD bez konce */
    "../tstdata/ic-cpmcmt.wav",             /* CPM-TAPE (Manchester) */
    "../tstdata/mzftools-fsk-s1.wav",       /* FSK (úroveň rychlosti podle frekvence) */
    "../tstdata/mzftools-direct.wav",       /* DIRECT (dekodér po vzorcích) */
};

/** @brief Počet nahrávek testu shody. */
#define WAV_FILE_COUNT ( sizeof ( g_wav_files ) / sizeof ( g_wav_files[0] ) )


/**
 * @brief Extrahuje pulzy z WAV stejně jako wav_analyzer_analyze() (hlavička, předzpracování, pulzy).
 *
 * @param path Cesta k WAV.
 * @param config Konfigurace analyzéru.
 * @param[out] seq Výstupní posloupnost (volající uvolní wav_pulse_sequence_destroy()).
 * @return 0 = OK, -1 = chyba (vypíše důvod).
 */
static int wav_to_pulses ( const char *path, const st_WAV_ANALYZER_CONFIG *config, st_WAV_PULSE_SEQUENCE *seq ) {
    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *hin = generic_driver_open_file ( &h, &d, ( char* ) path, FILE_DRIVER_OPMODE_RO );
    if ( !hin ) {
        printf ( "FAIL: cannot open '%s'\n", path );
        return -1;
    }
    en_WAV_ERROR werr;
    st_WAV_SIMPLE_HEADER *sh = wav_simple_header_new_from_handler ( hin, &werr );
    if ( !sh ) {
        printf ( "FAIL: WAV header of '%s': %s\n", path, wav_error_string ( werr ) );
        generic_driver_close ( hin );
        return -1;
    }
    double *samples = NULL;
    uint32_t n = 0;
    en_WAV_ANALYZER_ERROR err = wav_preprocess_run ( hin, sh, config, &samples, &n );
    uint32_t rate = sh->sample_rate;
    wav_simple_header_destroy ( sh );
    generic_driver_close ( hin );
    if ( err != WAV_ANALYZER_OK ) {
        printf ( "FAIL: preprocessing of '%s': %s\n", path, wav_analyzer_error_string ( err ) );
        return -1;
    }
    memset ( seq, 0, sizeof ( *seq ) );
    err = wav_pulse_extract ( samples, n, rate, config, seq );
    free ( samples );
    if ( err != WAV_ANALYZER_OK ) {
        printf ( "FAIL: pulse extraction of '%s': %s\n", path, wav_analyzer_error_string ( err ) );
        return -1;
    }
    return 0;
}


/**
 * @brief Porovná dva výsledky analýzy položku po položce.
 *
 * Délka WAV (wav_duration_sec) se neporovnává: u WAV pochází z hlavičky,
 * u posloupnosti pulzů ze součtu délek pulzů.
 *
 * @param name Jméno do výpisu.
 * @param a Výsledek wav_analyzer_analyze(). @param b Výsledek wav_analyzer_analyze_pulses().
 * @return Počet rozdílů.
 */
static int compare_results ( const char *name, const st_WAV_ANALYZER_RESULT *a, const st_WAV_ANALYZER_RESULT *b ) {
    int before = g_failures;

    EXPECT ( a->sample_rate == b->sample_rate, "%s: sample_rate %u vs %u", name, a->sample_rate, b->sample_rate );
    EXPECT ( a->total_pulses == b->total_pulses, "%s: total_pulses %u vs %u", name, a->total_pulses, b->total_pulses );
    EXPECT ( a->leaders.count == b->leaders.count, "%s: leaders %u vs %u", name, a->leaders.count, b->leaders.count );
    EXPECT ( a->file_count == b->file_count, "%s: file_count %u vs %u", name, a->file_count, b->file_count );
    EXPECT ( a->raw_block_count == b->raw_block_count, "%s: raw_block_count %u vs %u", name, a->raw_block_count,
             b->raw_block_count );

    uint32_t n = a->file_count < b->file_count ? a->file_count : b->file_count;
    for ( uint32_t i = 0; i < n; i++ ) {
        const st_WAV_ANALYZER_FILE_RESULT *fa = &a->files[i];
        const st_WAV_ANALYZER_FILE_RESULT *fb = &b->files[i];
        EXPECT ( fa->format == fb->format, "%s #%u: format %s vs %s", name, i, wav_tape_format_name ( fa->format ),
                 wav_tape_format_name ( fb->format ) );
        EXPECT ( fa->header_crc == fb->header_crc, "%s #%u: header CRC %d vs %d", name, i, fa->header_crc, fb->header_crc );
        EXPECT ( fa->body_crc == fb->body_crc, "%s #%u: body CRC %d vs %d", name, i, fa->body_crc, fb->body_crc );
        EXPECT ( fa->recovery_status == fb->recovery_status, "%s #%u: recovery %u vs %u", name, i, fa->recovery_status,
                 fb->recovery_status );
        EXPECT ( fa->writer == fb->writer && fa->tc_a == fb->tc_a && fa->tc_b == fb->tc_b && fa->ic_bd == fb->ic_bd,
                 "%s #%u: writer differs", name, i );
        EXPECT ( fa->header_leader_pulse == fb->header_leader_pulse && fa->consumed_until_pulse == fb->consumed_until_pulse,
                 "%s #%u: pulse range %u-%u vs %u-%u", name, i, fa->header_leader_pulse, fa->consumed_until_pulse,
                 fb->header_leader_pulse, fb->consumed_until_pulse );
        EXPECT ( fabs ( fa->start_time_sec - fb->start_time_sec ) < 1e-9 && fabs ( fa->duration_sec - fb->duration_sec ) < 1e-9,
                 "%s #%u: time differs", name, i );
        EXPECT ( ( fa->mzf == NULL ) == ( fb->mzf == NULL ), "%s #%u: MZF presence differs", name, i );
        if ( fa->mzf && fb->mzf ) {
            EXPECT ( memcmp ( &fa->mzf->header, &fb->mzf->header, sizeof ( st_MZF_HEADER ) ) == 0,
                     "%s #%u: MZF header differs", name, i );
            EXPECT ( fa->mzf->body_size == fb->mzf->body_size, "%s #%u: body_size %u vs %u", name, i, fa->mzf->body_size,
                     fb->mzf->body_size );
            if ( fa->mzf->body_size == fb->mzf->body_size && fa->mzf->body_size > 0 ) {
                EXPECT ( memcmp ( fa->mzf->body, fb->mzf->body, fa->mzf->body_size ) == 0, "%s #%u: MZF body differs", name, i );
            }
        }
        EXPECT ( fa->tap_data_size == fb->tap_data_size, "%s #%u: TAP size %u vs %u", name, i, fa->tap_data_size,
                 fb->tap_data_size );
        if ( fa->tap_data && fb->tap_data && fa->tap_data_size == fb->tap_data_size ) {
            EXPECT ( memcmp ( fa->tap_data, fb->tap_data, fa->tap_data_size ) == 0, "%s #%u: TAP data differs", name, i );
        }
    }

    n = a->raw_block_count < b->raw_block_count ? a->raw_block_count : b->raw_block_count;
    for ( uint32_t i = 0; i < n; i++ ) {
        const st_WAV_ANALYZER_RAW_BLOCK *ra = &a->raw_blocks[i];
        const st_WAV_ANALYZER_RAW_BLOCK *rb = &b->raw_blocks[i];
        EXPECT ( ra->pulse_start == rb->pulse_start && ra->pulse_end == rb->pulse_end && ra->data_size == rb->data_size
                 && ra->used_bits_last == rb->used_bits_last
                 && ( ra->data_size == 0 || memcmp ( ra->data, rb->data, ra->data_size ) == 0 ),
                 "%s raw #%u differs", name, i );
    }

    return g_failures - before;
}


/**
 * @brief Test shody wav_analyzer_analyze() a wav_analyzer_analyze_pulses() pro jednu nahrávku.
 * @param path Cesta k WAV.
 */
static void test_match_wav ( const char *path ) {
    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );
    config.keep_unknown = 1;   /* porovnají se i surové bloky */

    /* analýza WAV */
    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );
    st_HANDLER *hin = generic_driver_open_file ( &h, &d, ( char* ) path, FILE_DRIVER_OPMODE_RO );
    if ( !hin ) {
        CHECK ( 0, "cannot open '%s'", path );
        return;
    }
    st_WAV_ANALYZER_RESULT ra;
    en_WAV_ANALYZER_ERROR ea = wav_analyzer_analyze ( hin, &config, &ra );
    generic_driver_close ( hin );
    if ( ea != WAV_ANALYZER_OK ) {
        CHECK ( 0, "%s: wav_analyzer_analyze: %s", path, wav_analyzer_error_string ( ea ) );
        return;
    }

    /* analýza pulzů téhož souboru */
    st_WAV_PULSE_SEQUENCE seq;
    if ( wav_to_pulses ( path, &config, &seq ) != 0 ) {
        g_failures++;
        wav_analyzer_result_destroy ( &ra );
        return;
    }
    st_WAV_ANALYZER_RESULT rb;
    en_WAV_ANALYZER_ERROR eb = wav_analyzer_analyze_pulses ( &seq, &config, &rb );
    CHECK ( eb == WAV_ANALYZER_OK, "%s: wav_analyzer_analyze_pulses returns OK (%s)", path, wav_analyzer_error_string ( eb ) );

    if ( eb == WAV_ANALYZER_OK ) {
        int diff = compare_results ( path, &ra, &rb );
        CHECK ( diff == 0, "%s: %u files, %u raw blocks, results identical", path, ra.file_count, ra.raw_block_count );
        wav_analyzer_result_destroy ( &rb );
    }

    wav_pulse_sequence_destroy ( &seq );
    wav_analyzer_result_destroy ( &ra );
}


/* =========================================================================
 *  2. Jemný čas (signál ROM z mztape)
 * ========================================================================= */

/** @brief Velikost testovacího těla MZF. */
#define BODY_SIZE 1024

/** @brief Tělo testovacího MZF. */
static uint8_t g_body[BODY_SIZE];


/**
 * @brief Počet jedničkových bitů v bloku (checksum mztape).
 * @param d Data. @param n Délka v bajtech.
 * @return Počet jedničkových bitů.
 */
static uint32_t popcount ( const uint8_t *d, uint32_t n ) {
    uint32_t c = 0;
    for ( uint32_t i = 0; i < n; i++ ) for ( uint8_t b = d[i]; b; b >>= 1 ) c += b & 1;
    return c;
}


/**
 * @brief Naplní MZF v paměti: hlavička typ 01h, tělo 1 KB (pseudonáhodné bajty).
 * @param[out] m Cílová struktura (tělo ukazuje do g_body, neuvolňovat).
 */
static void make_mzf ( st_MZTAPE_MZF *m ) {
    memset ( m, 0, sizeof ( *m ) );
    uint32_t x = 12345;
    for ( int i = 0; i < BODY_SIZE; i++ ) {
        x = x * 1103515245u + 12345u;
        g_body[i] = ( uint8_t ) ( x >> 16 );
    }
    m->header[0x00] = 0x01;
    memcpy ( &m->header[0x01], "PULSE ANALYZE", 13 );
    m->header[0x0E] = 0x0D;
    m->header[0x12] = ( uint8_t ) ( BODY_SIZE & 0xFF );
    m->header[0x13] = ( uint8_t ) ( BODY_SIZE >> 8 );
    m->header[0x15] = 0x12;   /* start 1200h */
    m->header[0x17] = 0x12;   /* exec 1200h */
    m->body = g_body;
    m->size = BODY_SIZE;
    m->chkh = popcount ( m->header, sizeof ( m->header ) );
    m->chkb = popcount ( g_body, BODY_SIZE );
}


/**
 * @brief Převede vstream na posloupnost pulzů (délka v us přesně podle počtu vzorků).
 *
 * Eventy s nulovou délkou (první event vstreamu, dělení dlouhých běhů) se vynechají.
 *
 * @param vs Vstream (čtecí pozice se resetuje a posune na konec).
 * @param[out] seq Výstupní posloupnost (volající uvolní wav_pulse_sequence_destroy()).
 * @return 0 = OK, -1 = chyba alokace.
 */
static int vstream_to_pulses ( st_CMT_VSTREAM *vs, st_WAV_PULSE_SEQUENCE *seq ) {
    memset ( seq, 0, sizeof ( *seq ) );
    seq->sample_rate = vs->rate;
    cmt_vstream_read_reset ( vs );
    uint64_t samples;
    int value;
    while ( cmt_vstream_read_pulse ( vs, &samples, &value ) == EXIT_SUCCESS ) {
        if ( samples == 0 ) continue;
        if ( seq->count >= seq->capacity ) {
            uint32_t cap = seq->capacity ? seq->capacity * 2 : 4096;
            st_WAV_PULSE *p = ( st_WAV_PULSE* ) realloc ( seq->pulses, cap * sizeof ( st_WAV_PULSE ) );
            if ( !p ) return -1;
            seq->pulses = p;
            seq->capacity = cap;
        }
        st_WAV_PULSE *p = &seq->pulses[seq->count++];
        p->duration_samples = ( uint32_t ) samples;
        p->duration_us = ( double ) samples * 1000000.0 / ( double ) vs->rate;
        p->level = value;
    }
    return 0;
}


/**
 * @brief Ověří, že výsledek obsahuje právě jeden soubor NORMAL shodný se vstupním MZF.
 * @param label Popis do výpisu. @param r Výsledek analýzy. @param m Vstupní MZF.
 */
static void check_single_normal ( const char *label, const st_WAV_ANALYZER_RESULT *r, const st_MZTAPE_MZF *m ) {
    CHECK ( r->file_count == 1, "%s: one file decoded (got %u)", label, r->file_count );
    if ( r->file_count < 1 ) return;
    const st_WAV_ANALYZER_FILE_RESULT *f = &r->files[0];
    CHECK ( f->format == WAV_TAPE_FORMAT_NORMAL, "%s: format NORMAL (got %s)", label, wav_tape_format_name ( f->format ) );
    CHECK ( f->header_crc == WAV_CRC_OK && f->body_crc == WAV_CRC_OK, "%s: header and body CRC OK (%d, %d)", label,
            f->header_crc, f->body_crc );
    int same = 0;
    if ( f->mzf ) {
        st_MZF_HEADER hdr;
        memcpy ( &hdr, &f->mzf->header, sizeof ( hdr ) );
        mzf_header_swap_endianity ( &hdr );   /* host -> LE (na LE beze změny) */
        same = memcmp ( &hdr, m->header, sizeof ( hdr ) ) == 0 && f->mzf->body_size == m->size
               && memcmp ( f->mzf->body, m->body, m->size ) == 0;
    }
    CHECK ( same, "%s: MZF byte-identical with the input", label );
}


/**
 * @brief Signál ROM (2x hlavička + 2x tělo) při dané frekvenci převedený na pulzy se dekóduje beze ztráty.
 * @param rate Vzorkovací frekvence (Hz).
 */
static void test_fine_time ( uint32_t rate ) {
    char label[64];
    snprintf ( label, sizeof ( label ), "ROM signal at %u Hz", rate );

    st_MZTAPE_MZF m;
    make_mzf ( &m );
    st_CMT_VSTREAM *vs = mztape_create_cmt_vstream_from_mztmzf ( &m, MZTAPE_FORMATSET_MZ800, CMTSPEED_1_1, rate );
    if ( !vs ) {
        CHECK ( 0, "%s: mztape vstream created", label );
        return;
    }
    st_WAV_PULSE_SEQUENCE seq;
    int rc = vstream_to_pulses ( vs, &seq );
    cmt_vstream_destroy ( vs );
    if ( rc != 0 ) {
        CHECK ( 0, "%s: pulses allocated", label );
        wav_pulse_sequence_destroy ( &seq );
        return;
    }

    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );
    st_WAV_ANALYZER_RESULT r;
    en_WAV_ANALYZER_ERROR err = wav_analyzer_analyze_pulses ( &seq, &config, &r );
    CHECK ( err == WAV_ANALYZER_OK, "%s: %u pulses analysed (%s)", label, seq.count, wav_analyzer_error_string ( err ) );
    if ( err == WAV_ANALYZER_OK ) {
        CHECK ( r.sample_rate == rate, "%s: result sample_rate %u", label, r.sample_rate );
        check_single_normal ( label, &r, &m );
        wav_analyzer_result_destroy ( &r );
    }
    wav_pulse_sequence_destroy ( &seq );
}


/* =========================================================================
 *  2b. Jemný čas: záznamy hran z emulátoru (soubory .hwh v tstdata/ref)
 * ========================================================================= */

/**
 * @brief Očekávání pro jeden záznam hran z emulátoru.
 */
typedef struct st_HWH_CASE {
    const char *path;          /**< záznam .hwh (relativně k build/) */
    en_WAV_TAPE_FORMAT format; /**< očekávaný formát */
    en_WAV_WRITER writer;      /**< očekávaný zapisovač */
    uint8_t tc_a;              /**< TurboCopy: čitatel poměru (jinak 0) */
    uint8_t tc_b;              /**< TurboCopy: jmenovatel poměru (jinak 0) */
    uint16_t ic_bd;            /**< Intercopy: rychlost v Bd (jinak 0) */
} st_HWH_CASE;

/** @brief Záznamy uložení testovacího MZF (ref/emutest.mzf) různými programy. */
static const st_HWH_CASE g_hwh_cases[] = {
    { "../tstdata/ref/rom800-save-emutest.hwh", WAV_TAPE_FORMAT_NORMAL, WAV_WRITER_UNKNOWN, 0, 0, 0 },
    { "../tstdata/ref/tc121-2to1-loaderN-emutest.hwh", WAV_TAPE_FORMAT_NORMAL, WAV_WRITER_TURBOCOPY, 2, 1, 0 },
    { "../tstdata/ref/tc121-3to1-loaderY-emutest.hwh", WAV_TAPE_FORMAT_TURBO, WAV_WRITER_TURBOCOPY, 3, 1, 0 },
    { "../tstdata/ref/tc121-3to2-loaderN-emutest.hwh", WAV_TAPE_FORMAT_NORMAL, WAV_WRITER_TURBOCOPY, 3, 2, 0 },
    { "../tstdata/ref/tc121-7to3-loaderN-emutest.hwh", WAV_TAPE_FORMAT_NORMAL, WAV_WRITER_TURBOCOPY, 7, 3, 0 },
    { "../tstdata/ref/tc121-8to3-loaderN-emutest.hwh", WAV_TAPE_FORMAT_NORMAL, WAV_WRITER_TURBOCOPY, 8, 3, 0 },
    { "../tstdata/ref/tc121-9to7-loaderN-emutest.hwh", WAV_TAPE_FORMAT_NORMAL, WAV_WRITER_TURBOCOPY, 9, 7, 0 },
    { "../tstdata/ref/ic102-normal-600-emutest.hwh", WAV_TAPE_FORMAT_NORMAL, WAV_WRITER_INTERCOPY, 0, 0, 600 },
    { "../tstdata/ref/ic102-normal-1200-emutest.hwh", WAV_TAPE_FORMAT_NORMAL, WAV_WRITER_UNKNOWN, 0, 0, 0 },
    { "../tstdata/ref/ic102-normal-2400-emutest.hwh", WAV_TAPE_FORMAT_NORMAL, WAV_WRITER_INTERCOPY, 0, 0, 2400 },
    { "../tstdata/ref/ic102-normal-2800-emutest.hwh", WAV_TAPE_FORMAT_NORMAL, WAV_WRITER_INTERCOPY, 0, 0, 2800 },
    { "../tstdata/ref/ic102-normal-3200-emutest.hwh", WAV_TAPE_FORMAT_NORMAL, WAV_WRITER_INTERCOPY, 0, 0, 3200 },
    { "../tstdata/ref/ic102-turbo-600-emutest.hwh", WAV_TAPE_FORMAT_TURBO, WAV_WRITER_INTERCOPY, 0, 0, 600 },
    { "../tstdata/ref/ic102-turbo-1200-emutest.hwh", WAV_TAPE_FORMAT_TURBO, WAV_WRITER_INTERCOPY, 0, 0, 1200 },
    { "../tstdata/ref/ic102-turbo-2400-emutest.hwh", WAV_TAPE_FORMAT_TURBO, WAV_WRITER_INTERCOPY, 0, 0, 2400 },
    { "../tstdata/ref/ic102-turbo-2800-emutest.hwh", WAV_TAPE_FORMAT_TURBO, WAV_WRITER_INTERCOPY, 0, 0, 2800 },
    { "../tstdata/ref/ic102-turbo-3200-emutest.hwh", WAV_TAPE_FORMAT_TURBO, WAV_WRITER_INTERCOPY, 0, 0, 3200 },
    { "../tstdata/ref/ic102-fastipl-600-emutest.hwh", WAV_TAPE_FORMAT_FASTIPL, WAV_WRITER_INTERCOPY, 0, 0, 600 },
    { "../tstdata/ref/ic102-fastipl-1200-emutest.hwh", WAV_TAPE_FORMAT_FASTIPL, WAV_WRITER_INTERCOPY, 0, 0, 1200 },
    { "../tstdata/ref/ic102-fastipl-2400-emutest.hwh", WAV_TAPE_FORMAT_FASTIPL, WAV_WRITER_INTERCOPY, 0, 0, 2400 },
    { "../tstdata/ref/ic102-fastipl-2800-emutest.hwh", WAV_TAPE_FORMAT_FASTIPL, WAV_WRITER_INTERCOPY, 0, 0, 2800 },
    { "../tstdata/ref/ic102-fastipl-3200-emutest.hwh", WAV_TAPE_FORMAT_FASTIPL, WAV_WRITER_INTERCOPY, 0, 0, 3200 },
    { "../tstdata/ref/ic102-cpm-1500-emutest.hwh", WAV_TAPE_FORMAT_CPM_TAPE, WAV_WRITER_UNKNOWN, 0, 0, 0 },
    { "../tstdata/ref/ic102-cpm-2400-emutest.hwh", WAV_TAPE_FORMAT_CPM_TAPE, WAV_WRITER_UNKNOWN, 0, 0, 0 },
    { "../tstdata/ref/ic102-cpm-2800-emutest.hwh", WAV_TAPE_FORMAT_CPM_TAPE, WAV_WRITER_UNKNOWN, 0, 0, 0 },
    { "../tstdata/ref/ic102-cpm-3200-emutest.hwh", WAV_TAPE_FORMAT_CPM_TAPE, WAV_WRITER_UNKNOWN, 0, 0, 0 },
    { "../tstdata/ref/ic102-cpm-3600-emutest.hwh", WAV_TAPE_FORMAT_CPM_TAPE, WAV_WRITER_UNKNOWN, 0, 0, 0 },
};

/** @brief Počet záznamů hran. */
#define HWH_CASE_COUNT ( sizeof ( g_hwh_cases ) / sizeof ( g_hwh_cases[0] ) )


/**
 * @brief Načte záznam hran .hwh jako posloupnost pulzů s frekvencí pxCLK záznamu.
 *
 * Formát (LE): "HWH1", počet půlvln, pxclk (Hz), dělič CPU, úroveň první
 * půlvlny (bajt, doplněno na 20 B), pak délky půlvln v tikách pxCLK (DWORD).
 *
 * @param path Soubor.
 * @param[out] seq Výstupní posloupnost (volající uvolní wav_pulse_sequence_destroy()).
 * @return 0 = OK, -1 = chyba čtení nebo alokace.
 */
static int hwh_to_pulses ( const char *path, st_WAV_PULSE_SEQUENCE *seq ) {
    memset ( seq, 0, sizeof ( *seq ) );
    FILE *f = fopen ( path, "rb" );
    if ( !f ) return -1;
    uint8_t h[20];
    if ( fread ( h, 1, 20, f ) != 20 || memcmp ( h, "HWH1", 4 ) != 0 ) {
        fclose ( f );
        return -1;
    }
    uint32_t n = h[4] | ( h[5] << 8 ) | ( h[6] << 16 ) | ( ( uint32_t ) h[7] << 24 );
    uint32_t pxclk = h[8] | ( h[9] << 8 ) | ( h[10] << 16 ) | ( ( uint32_t ) h[11] << 24 );
    int level = h[16] ? 1 : 0;
    if ( pxclk == 0 || n == 0 ) {
        fclose ( f );
        return -1;
    }
    seq->pulses = ( st_WAV_PULSE* ) malloc ( ( size_t ) n * sizeof ( st_WAV_PULSE ) );
    if ( !seq->pulses ) {
        fclose ( f );
        return -1;
    }
    seq->capacity = n;
    seq->sample_rate = pxclk;
    for ( uint32_t i = 0; i < n; i++ ) {
        uint8_t b[4];
        if ( fread ( b, 1, 4, f ) != 4 ) {
            fclose ( f );
            return -1;
        }
        uint32_t ticks = b[0] | ( b[1] << 8 ) | ( b[2] << 16 ) | ( ( uint32_t ) b[3] << 24 );
        st_WAV_PULSE *p = &seq->pulses[seq->count++];
        p->duration_samples = ticks;
        p->duration_us = ( double ) ticks * 1000000.0 / ( double ) pxclk;
        p->level = level;
        level = !level;
    }
    fclose ( f );
    return 0;
}


/**
 * @brief Záznam hran z emulátoru dekóduje na jeden soubor s tělem ref/emutest.mzf a správným zapisovačem.
 *
 * Při @p rate 0 se použije pxCLK záznamu (17 734 475 Hz, emulátor, který
 * záznamy pořídil). Jiná frekvence znamená tytéž počty tiků v emulátoru
 * s jinou časovou základnou (např. 17 721 600 Hz): emulovaný program
 * odpočítá stejné takty, skutečné délky se úměrně změní (o 0,07 %).
 *
 * @param c Očekávání.
 * @param rate Frekvence tiků (Hz), 0 = pxCLK ze záznamu.
 */
static void test_hwh ( const st_HWH_CASE *c, uint32_t rate ) {
    /* referenční MZF: hlavička 128 B + tělo */
    static uint8_t ref[128 + 65536];
    FILE *f = fopen ( "../tstdata/ref/emutest.mzf", "rb" );
    size_t ref_size = f ? fread ( ref, 1, sizeof ( ref ), f ) : 0;
    if ( f ) fclose ( f );
    if ( ref_size <= 128 ) {
        CHECK ( 0, "%s: reference ../tstdata/ref/emutest.mzf loaded", c->path );
        return;
    }

    st_WAV_PULSE_SEQUENCE seq;
    if ( hwh_to_pulses ( c->path, &seq ) != 0 ) {
        CHECK ( 0, "%s: loaded", c->path );
        wav_pulse_sequence_destroy ( &seq );
        return;
    }
    if ( rate ) {
        seq.sample_rate = rate;
        for ( uint32_t i = 0; i < seq.count; i++ ) {
            seq.pulses[i].duration_us = ( double ) seq.pulses[i].duration_samples * 1000000.0 / ( double ) rate;
        }
    }
    char label[160];
    snprintf ( label, sizeof ( label ), "%s at %u Hz", c->path, seq.sample_rate );

    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );
    st_WAV_ANALYZER_RESULT r;
    en_WAV_ANALYZER_ERROR err = wav_analyzer_analyze_pulses ( &seq, &config, &r );
    CHECK ( err == WAV_ANALYZER_OK, "%s: %u pulses analysed", label, seq.count );
    if ( err == WAV_ANALYZER_OK ) {
        CHECK ( r.file_count == 1, "%s: one file (got %u)", label, r.file_count );
        if ( r.file_count >= 1 ) {
            const st_WAV_ANALYZER_FILE_RESULT *fr = &r.files[0];
            CHECK ( fr->format == c->format, "%s: format %s (got %s)", label, wav_tape_format_name ( c->format ),
                    wav_tape_format_name ( fr->format ) );
            CHECK ( fr->header_crc == WAV_CRC_OK && fr->body_crc == WAV_CRC_OK, "%s: CRC OK (%d, %d)", label,
                    fr->header_crc, fr->body_crc );
            CHECK ( fr->mzf && fr->mzf->body_size == ref_size - 128 && memcmp ( fr->mzf->body, ref + 128, ref_size - 128 ) == 0,
                    "%s: body identical with emutest.mzf", label );
            CHECK ( fr->writer == c->writer && fr->tc_a == c->tc_a && fr->tc_b == c->tc_b && fr->ic_bd == c->ic_bd,
                    "%s: writer %d a/b %u/%u bd %u (got %d %u/%u %u)", label, c->writer, c->tc_a, c->tc_b, c->ic_bd,
                    fr->writer, fr->tc_a, fr->tc_b, fr->ic_bd );
        }
        wav_analyzer_result_destroy ( &r );
    }
    wav_pulse_sequence_destroy ( &seq );
}


/* =========================================================================
 *  3. Meze
 * ========================================================================= */

/**
 * @brief Zjistí, zda je výsledek celý vynulovaný (všechny bajty 0).
 * @param r Výsledek. @return 1 = samé nuly.
 */
static int result_is_zero ( const st_WAV_ANALYZER_RESULT *r ) {
    const uint8_t *b = ( const uint8_t* ) r;
    for ( size_t i = 0; i < sizeof ( *r ); i++ ) {
        if ( b[i] ) return 0;
    }
    return 1;
}


/**
 * @brief Prázdná posloupnost, jeden pulz, NULL parametry, nulová frekvence,
 *        vynulování výsledku při chybě, horní mez délky pulzu, surový blok nad 2^35 vzorků
 *        a posloupnost jen z nulových pulzů.
 */
static void test_limits ( void ) {
    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );
    st_WAV_ANALYZER_RESULT r;
    en_WAV_ANALYZER_ERROR err;

    /* prázdná posloupnost */
    st_WAV_PULSE_SEQUENCE empty = { NULL, 0, 0, 44100 };
    err = wav_analyzer_analyze_pulses ( &empty, &config, &r );
    CHECK ( err == WAV_ANALYZER_OK && r.file_count == 0 && r.raw_block_count == 0 && r.total_pulses == 0,
            "empty sequence: OK without files (%s)", wav_analyzer_error_string ( err ) );
    if ( err == WAV_ANALYZER_OK ) wav_analyzer_result_destroy ( &r );

    /* jeden pulz (i se surovými bloky) */
    st_WAV_PULSE one_pulse = { 250.0, 4430, 1 };
    st_WAV_PULSE_SEQUENCE one = { &one_pulse, 1, 1, 17721600 };
    config.keep_unknown = 1;
    err = wav_analyzer_analyze_pulses ( &one, &config, &r );
    CHECK ( err == WAV_ANALYZER_OK && r.file_count == 0 && r.raw_block_count == 0 && r.total_pulses == 1,
            "one pulse: OK without files (%s)", wav_analyzer_error_string ( err ) );
    if ( err == WAV_ANALYZER_OK ) wav_analyzer_result_destroy ( &r );
    config.keep_unknown = 0;

    /* NULL parametry */
    err = wav_analyzer_analyze_pulses ( NULL, &config, &r );
    CHECK ( err == WAV_ANALYZER_ERROR_INVALID_PARAM, "seq == NULL: INVALID_PARAM" );
    err = wav_analyzer_analyze_pulses ( &one, NULL, &r );
    CHECK ( err == WAV_ANALYZER_ERROR_INVALID_PARAM, "config == NULL: INVALID_PARAM" );
    err = wav_analyzer_analyze_pulses ( &one, &config, NULL );
    CHECK ( err == WAV_ANALYZER_ERROR_INVALID_PARAM, "result == NULL: INVALID_PARAM" );

    /* pulzy bez pole, nulová frekvence; výsledek se při chybě celý vynuluje */
    st_WAV_PULSE_SEQUENCE no_array = { NULL, 5, 0, 44100 };
    memset ( &r, 0xA5, sizeof ( r ) );
    err = wav_analyzer_analyze_pulses ( &no_array, &config, &r );
    CHECK ( err == WAV_ANALYZER_ERROR_INVALID_PARAM && result_is_zero ( &r ),
            "count > 0 with pulses == NULL: INVALID_PARAM, result zeroed" );
    st_WAV_PULSE_SEQUENCE no_rate = { &one_pulse, 1, 1, 0 };
    memset ( &r, 0xA5, sizeof ( r ) );
    err = wav_analyzer_analyze_pulses ( &no_rate, &config, &r );
    CHECK ( err == WAV_ANALYZER_ERROR_INVALID_PARAM && result_is_zero ( &r ), "sample_rate 0: INVALID_PARAM, result zeroed" );
    memset ( &r, 0xA5, sizeof ( r ) );
    err = wav_analyzer_analyze_pulses ( NULL, &config, &r );
    CHECK ( err == WAV_ANALYZER_ERROR_INVALID_PARAM && result_is_zero ( &r ), "seq == NULL: result zeroed" );
    memset ( &r, 0xA5, sizeof ( r ) );
    err = wav_analyzer_analyze_pulses ( &one, NULL, &r );
    CHECK ( err == WAV_ANALYZER_ERROR_INVALID_PARAM && result_is_zero ( &r ), "config == NULL: result zeroed" );

    /*
     * horní mez délky pulzu: DBL_MAX a dva DBL_MAX sloučené přes nulový pulz
     * (součet by byl nekonečno), pulz 0xFFFFFFFF vzorků při 1 Hz; za tónem
     * 2000 pulzů (leader) a daty, aby se nad dlouhými pulzy sestavil histogram
     * (i s šířkou binu 0,001 us)
     */
    {
        enum { LONG_N = 2000 + 200 + 3 };
        static st_WAV_PULSE lp[LONG_N];
        uint32_t k = 0;
        lp[k++] = ( st_WAV_PULSE ) { DBL_MAX, 0xFFFFFFFFu, 1 };
        lp[k++] = ( st_WAV_PULSE ) { 0.0, 0, 0 };
        lp[k++] = ( st_WAV_PULSE ) { DBL_MAX, 0xFFFFFFFFu, 1 };   /* sloučí se s prvním */
        for ( int i = 0; i < 2000; i++, k++ ) lp[k] = ( st_WAV_PULSE ) { 250.0, 250, ( int ) ( k & 1 ) };
        for ( int i = 0; i < 200; i++, k++ ) {
            double d = ( i % 50 == 25 ) ? 4.294967295e15 : ( ( i & 1 ) ? 250.0 : 500.0 );   /* 0xFFFFFFFF vzorků při 1 Hz */
            lp[k] = ( st_WAV_PULSE ) { d, ( d > 1e9 ) ? 0xFFFFFFFFu : ( uint32_t ) d, ( int ) ( k & 1 ) };
        }
        st_WAV_PULSE_SEQUENCE long_seq = { lp, k, k, 1000000 };
        for ( int pass = 0; pass < 2; pass++ ) {
            st_WAV_ANALYZER_CONFIG c2 = config;
            if ( pass ) c2.histogram_bin_width = 0.001;
            err = wav_analyzer_analyze_pulses ( &long_seq, &c2, &r );
            int ok = err == WAV_ANALYZER_OK && r.total_pulses == k - 2 && isfinite ( r.wav_duration_sec )
                     && r.pulse_stats.max_us == WAV_ANALYZER_PULSE_MAX_US && isfinite ( r.pulse_stats.avg_us )
                     && isfinite ( r.pulse_stats.stddev_us );
            CHECK ( ok, "pulses over %.0f us clamped (bin width %.3f us): %s, %u pulses, max %.0f us, %.1f s",
                    WAV_ANALYZER_PULSE_MAX_US, c2.histogram_bin_width, wav_analyzer_error_string ( err ), r.total_pulses,
                    r.pulse_stats.max_us, r.wav_duration_sec );
            if ( err == WAV_ANALYZER_OK ) wav_analyzer_result_destroy ( &r );
        }
    }

    /*
     * surový blok nad 2^35 vzorků (1 bit na vzorek se do uint32 bajtů nevejde):
     * 64 pulzů po 0xFFFFFFFF vzorcích při 0xFFFFFFFF Hz (1 s) se odmítne jako
     * surový blok, analýza skončí bez chyby a hned
     */
    {
        enum { BIG_N = 64 };
        static st_WAV_PULSE big[BIG_N];
        for ( int i = 0; i < BIG_N; i++ ) big[i] = ( st_WAV_PULSE ) { 1000000.0, 0xFFFFFFFFu, i & 1 };
        st_WAV_PULSE_SEQUENCE big_seq = { big, BIG_N, BIG_N, 0xFFFFFFFFu };
        config.keep_unknown = 1;
        err = wav_analyzer_analyze_pulses ( &big_seq, &config, &r );
        CHECK ( err == WAV_ANALYZER_OK && r.file_count == 0 && r.raw_block_count == 0,
                "gap of 2.7e11 samples at 4294967295 Hz: no raw block, OK (%s, %u raw)", wav_analyzer_error_string ( err ),
                err == WAV_ANALYZER_OK ? r.raw_block_count : 0 );
        if ( err == WAV_ANALYZER_OK ) wav_analyzer_result_destroy ( &r );
        config.keep_unknown = 0;
    }

    /* jen pulzy s nulovou délkou */
    st_WAV_PULSE zeros[3] = { { 0.0, 0, 1 }, { 0.0, 0, 0 }, { -1.0, 0, 1 } };
    st_WAV_PULSE_SEQUENCE zero_seq = { zeros, 3, 3, 44100 };
    err = wav_analyzer_analyze_pulses ( &zero_seq, &config, &r );
    CHECK ( err == WAV_ANALYZER_OK && r.file_count == 0 && r.total_pulses == 0,
            "only zero-length pulses: skipped, OK without files (%s)", wav_analyzer_error_string ( err ) );
    if ( err == WAV_ANALYZER_OK ) wav_analyzer_result_destroy ( &r );
}


/**
 * @brief Pulzy s nulovou délkou uvnitř platného signálu se přeskočí a vstup se nezmění.
 *
 * Do signálu ROM (8 860 800 Hz) se vloží nulové pulzy dvojím způsobem:
 * - rozdělení jednoho pulzu na dvě poloviny stejné úrovně s nulovým pulzem
 *   mezi nimi (poloviny se musí sloučit zpět v jeden pulz),
 * - nulový pulz mezi dva sousední pulzy různé úrovně (po vynechání se nic
 *   slučovat nesmí),
 * a na začátek a konec posloupnosti. Počet pulzů po přípravě i výsledek musí
 * být stejné jako bez nulových pulzů a vstupní pole se nesmí změnit.
 */
static void test_zero_pulses ( void ) {
    const uint32_t rate = 8860800;
    st_MZTAPE_MZF m;
    make_mzf ( &m );
    st_CMT_VSTREAM *vs = mztape_create_cmt_vstream_from_mztmzf ( &m, MZTAPE_FORMATSET_MZ800, CMTSPEED_1_1, rate );
    if ( !vs ) {
        CHECK ( 0, "zero pulses: mztape vstream created" );
        return;
    }
    st_WAV_PULSE_SEQUENCE seq;
    int rc = vstream_to_pulses ( vs, &seq );
    cmt_vstream_destroy ( vs );
    if ( rc != 0 ) {
        CHECK ( 0, "zero pulses: pulses allocated" );
        wav_pulse_sequence_destroy ( &seq );
        return;
    }

    /*
     * každý 997. pulz rozdělit na dvě poloviny s nulovým pulzem opačné úrovně mezi nimi,
     * před každý 991. pulz vložit nulový pulz (mezi dva pulzy různé úrovně)
     */
    uint32_t extra = 2 * ( seq.count / 997 + 1 ) + ( seq.count / 991 + 1 ) + 2;
    st_WAV_PULSE *buf = ( st_WAV_PULSE* ) malloc ( ( seq.count + extra ) * sizeof ( st_WAV_PULSE ) );
    if ( !buf ) {
        CHECK ( 0, "zero pulses: buffer allocated" );
        wav_pulse_sequence_destroy ( &seq );
        return;
    }
    uint32_t n = 0;
    buf[n++] = ( st_WAV_PULSE ) { 0.0, 0, !seq.pulses[0].level };   /* nulový pulz na začátku */
    uint32_t split = 0, between = 0;
    for ( uint32_t i = 0; i < seq.count; i++ ) {
        st_WAV_PULSE p = seq.pulses[i];
        if ( i % 991 == 200 && i > 0 && seq.pulses[i - 1].level != p.level ) {
            /* nulový pulz mezi dva pulzy různé úrovně (úroveň nuly stejná jako předchozí pulz) */
            buf[n++] = ( st_WAV_PULSE ) { 0.0, 0, seq.pulses[i - 1].level };
            between++;
        }
        if ( i % 997 == 500 && p.duration_samples >= 2 ) {
            st_WAV_PULSE a = p, b = p;
            a.duration_samples = p.duration_samples / 2;
            b.duration_samples = p.duration_samples - a.duration_samples;
            a.duration_us = ( double ) a.duration_samples * 1000000.0 / rate;
            b.duration_us = ( double ) b.duration_samples * 1000000.0 / rate;
            buf[n++] = a;
            buf[n++] = ( st_WAV_PULSE ) { 0.0, 0, !p.level };
            buf[n++] = b;
            split++;
        } else {
            buf[n++] = p;
        }
    }
    buf[n++] = ( st_WAV_PULSE ) { 0.0, 0, !seq.pulses[seq.count - 1].level };   /* nulový pulz na konci */
    st_WAV_PULSE_SEQUENCE zseq = { buf, n, n, rate };

    /* kopie vstupu pro kontrolu, že funkce posloupnost nemění */
    st_WAV_PULSE *copy = ( st_WAV_PULSE* ) malloc ( n * sizeof ( st_WAV_PULSE ) );
    if ( copy ) memcpy ( copy, buf, n * sizeof ( st_WAV_PULSE ) );

    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );
    st_WAV_ANALYZER_RESULT r;
    en_WAV_ANALYZER_ERROR err = wav_analyzer_analyze_pulses ( &zseq, &config, &r );
    CHECK ( split > 0 && between > 0, "zero pulses: %u pulses split, %u zero-length pulses between pulses", split, between );
    CHECK ( err == WAV_ANALYZER_OK, "zero pulses: sequence with zero-length pulses analysed (%s)",
            wav_analyzer_error_string ( err ) );
    if ( err == WAV_ANALYZER_OK ) {
        CHECK ( r.total_pulses == seq.count, "zero pulses: merged back to %u pulses (got %u)", seq.count, r.total_pulses );
        check_single_normal ( "zero pulses", &r, &m );
        wav_analyzer_result_destroy ( &r );
    }
    CHECK ( copy && zseq.pulses == buf && zseq.count == n && memcmp ( copy, buf, n * sizeof ( st_WAV_PULSE ) ) == 0,
            "zero pulses: input sequence unchanged" );

    free ( copy );
    free ( buf );
    wav_pulse_sequence_destroy ( &seq );
}


int main ( void ) {
    printf ( "=== wav_analyzer_analyze_pulses ===\n" );

    test_limits ();

    test_fine_time ( 17721600 );   /* GDGCLK MZ-800 */
    test_fine_time ( 14336640 );   /* GDGCLK MZ-700 a MZ-1500 */
    test_fine_time ( 8860800 );
    test_zero_pulses ();

    if ( !tstdata_skip ( "test_wav_analyzer_pulses (emulator edges, WAV match)" ) ) {
        for ( uint32_t i = 0; i < HWH_CASE_COUNT; i++ ) {
            test_hwh ( &g_hwh_cases[i], 0 );          /* pxCLK záznamu */
            test_hwh ( &g_hwh_cases[i], 17721600 );   /* GDGCLK MZ-800 */
        }
        for ( uint32_t i = 0; i < WAV_FILE_COUNT; i++ ) {
            test_match_wav ( g_wav_files[i] );
        }
    }

    printf ( "\n=== wav_analyzer_analyze_pulses: %s (%d failures) ===\n", g_failures == 0 ? "PASS" : "FAIL", g_failures );
    return g_failures == 0 ? 0 : 1;
}
