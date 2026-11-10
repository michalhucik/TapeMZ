/**
 * @file   test_mztape_pulses.c
 * @brief  Testy pulzních sad mztape a vlastních délek pulzů (hlavička
 *         CMTSPEED zařízení UniCMT). Spouštět z build/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "libs/mzf/mzf.h"
#include "libs/mztape/mztape.h"
#include "libs/cmt_stream/cmt_stream.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )

/** @brief Velikost testovacího těla MZF. */
#define BODY_SIZE 1024

/** @brief Tělo testovacího MZF (0x00..0xFF opakovaně). */
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
 * @brief Naplní MZF v paměti: hlavička typ 01h, tělo 1 KB.
 * @param[out] m Cílová struktura (tělo ukazuje do g_body, neuvolňovat).
 */
static void make_mzf ( st_MZTAPE_MZF *m ) {
    memset ( m, 0, sizeof ( *m ) );
    for ( int i = 0; i < BODY_SIZE; i++ ) g_body[i] = ( uint8_t ) i;
    m->header[0x00] = 0x01;
    memcpy ( &m->header[0x01], "PULSES TEST", 11 );
    m->header[0x0C] = 0x0D;
    m->header[0x12] = ( uint8_t ) ( BODY_SIZE & 0xFF );
    m->header[0x13] = ( uint8_t ) ( BODY_SIZE >> 8 );
    m->header[0x15] = 0x12;   /* start 1200h */
    m->header[0x17] = 0x12;   /* exec 1200h */
    m->body = g_body;
    m->size = BODY_SIZE;
    m->chkh = popcount ( m->header, sizeof ( m->header ) );
    m->chkb = popcount ( g_body, BODY_SIZE );
}


/** @brief Zapíše uint16 LE na pozici v bufferu. */
static void put_le16 ( uint8_t *p, uint16_t v ) {
    p[0] = ( uint8_t ) ( v & 0xFF );
    p[1] = ( uint8_t ) ( v >> 8 );
}


/** @brief Počet chyb nahlášených přes error callback mztape. */
static int g_err_count = 0;

/** @brief Error callback, který chyby jen počítá (bez výpisu). */
static void count_error_cb ( const char *func, int line, const char *fmt, ... ) {
    (void) func; (void) line; (void) fmt;
    g_err_count++;
}

int main ( void ) {

    /* pulzní sada MZ-800: údaj servisního manuálu */
    double out[4] = { 0, 0, 0, 0 };
    CHECK ( mztape_get_pulse_lengths ( MZTAPE_PULSESET_800, out ) == EXIT_SUCCESS
            && fabs ( out[0] - 0.000470 ) < 1e-9 && fabs ( out[1] - 0.000494 ) < 1e-9
            && fabs ( out[2] - 0.000240 ) < 1e-9 && fabs ( out[3] - 0.000278 ) < 1e-9,
            "pulse set 800 = 470/494, 240/278 us" );

    /* vlastní délky pulzů */
    st_MZTAPE_PULSES_LENGTH p;
    memset ( &p, 0, sizeof ( p ) );
    CHECK ( mztape_pulses_set_us ( &p, 470, 494, 240, 278 ) == EXIT_SUCCESS, "pulses_set_us 470/494/240/278 accepted" );
    CHECK ( mztape_pulses_set_us ( &p, 0, 494, 240, 278 ) != EXIT_SUCCESS, "pulses_set_us with zero length rejected" );
    CHECK ( fabs ( p.long_pulse.high - 0.000470 ) < 1e-12 && fabs ( p.short_pulse.total - 0.000518 ) < 1e-12,
            "rejected call keeps previous pulses" );

    /* ekvivalentní poměr rychlosti */
    double r1 = mztape_pulses_get_ratio ( MZTAPE_FORMATSET_MZ800_SANE, &p );
    CHECK ( fabs ( r1 - 1.0 ) < 0.02, "ratio of UniCMT 1x is 1.0" );
    st_MZTAPE_PULSES_LENGTH p2;
    CHECK ( mztape_pulses_set_us ( &p2, 235, 247, 120, 139 ) == EXIT_SUCCESS, "pulses_set_us UniCMT 2x accepted" );
    double r2 = mztape_pulses_get_ratio ( MZTAPE_FORMATSET_MZ800_SANE, &p2 );
    CHECK ( fabs ( r2 - 2.0 ) < 0.05, "ratio of UniCMT 2x is 2.0" );

    /* hlavička CMTSPEED */
    uint8_t hdr[128];
    memset ( hdr, 0, sizeof ( hdr ) );
    hdr[0x00] = 0x00;
    memcpy ( &hdr[0x01], "CMTSPEED\r", 9 );
    put_le16 ( &hdr[MZTAPE_UNICMT_PULSES_OFFSET + 0], 470 );
    put_le16 ( &hdr[MZTAPE_UNICMT_PULSES_OFFSET + 2], 494 );
    put_le16 ( &hdr[MZTAPE_UNICMT_PULSES_OFFSET + 4], 240 );
    put_le16 ( &hdr[MZTAPE_UNICMT_PULSES_OFFSET + 6], 278 );
    st_MZTAPE_PULSES_LENGTH pm;
    memset ( &pm, 0, sizeof ( pm ) );
    CHECK ( mztape_unicmt_speed_marker ( hdr, &pm ) == MZTAPE_UNICMT_MARKER_VALID
            && fabs ( pm.long_pulse.high - 0.000470 ) < 1e-12, "CMTSPEED header recognized, LONG high 470 us" );

    /* stream s vlastními pulzy */
    st_MZTAPE_MZF m;
    make_mzf ( &m );
    st_CMT_STREAM *s44 = mztape_create_stream_from_mztapemzf_pulses ( &m, &p, CMT_STREAM_TYPE_VSTREAM, MZTAPE_FORMATSET_MZ800_SANE, 44100 );
    double len44 = s44 ? cmt_stream_get_length ( s44 ) : 0;
    CHECK ( s44 != NULL && len44 >= 10.0 && len44 <= 40.0, "stream with custom pulses at 44100 Hz, length 10-40 s" );
    st_CMT_STREAM *sgdg = mztape_create_stream_from_mztapemzf_pulses ( &m, &p, CMT_STREAM_TYPE_VSTREAM, MZTAPE_FORMATSET_MZ800_SANE, 17721600 );
    double lengdg = sgdg ? cmt_stream_get_length ( sgdg ) : 0;
    CHECK ( sgdg != NULL && len44 > 0 && fabs ( lengdg - len44 ) / len44 <= 0.01,
            "stream with custom pulses at 17721600 Hz, length within 1 % of 44100 Hz" );
    printf ( "lengths: %.6f s at 44100 Hz, %.6f s at 17721600 Hz\n", len44, lengdg );
    cmt_stream_destroy ( s44 );
    cmt_stream_destroy ( sgdg );

    /* bitstream cesta musí odmítnout neplatnou rychlost stejně jako vstream */
    g_err_count = 0;
    mztape_set_error_callback ( count_error_cb );
    st_CMT_BITSTREAM *bs_custom = mztape_create_cmt_bitstream_from_mztmzf ( &m, MZTAPE_FORMATSET_MZ800_SANE, CMTSPEED_CUSTOM, 44100 );
    st_CMT_BITSTREAM *bs_none = mztape_create_cmt_bitstream_from_mztmzf ( &m, MZTAPE_FORMATSET_MZ800_SANE, CMTSPEED_NONE, 44100 );
    CHECK ( bs_custom == NULL && bs_none == NULL && g_err_count == 2, "bitstream path rejects CMTSPEED_CUSTOM and CMTSPEED_NONE with an error" );
    if ( bs_custom ) cmt_bitstream_destroy ( bs_custom );
    if ( bs_none ) cmt_bitstream_destroy ( bs_none );
    mztape_set_error_callback ( NULL );

    printf ( "\n=== MZTAPE Pulses Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
