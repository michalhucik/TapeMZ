/**
 * @file   test_wav_formats.c
 * @brief  Čtení WAV ve formátu EXTENSIBLE a IEEE float.
 *
 * Z MZF (1024 B) vyrobí blok 0x40, přehraje ho playerem a uloží jako PCM
 * WAV do temp/. Jeho vzorky přečte knihovnou wav a vlastním zapisovačem
 * hlaviček z nich vyrobí varianty: PCM 16, EXTENSIBLE PCM 16/24/24-v-32,
 * IEEE float 32/64, EXTENSIBLE float 32/64, PCM 64 a stereo EXTENSIBLE
 * float 32. Každou variantu dekóduje wav_analyzer a čeká 1 soubor
 * s CRC OK a tělem shodným s variantou PCM 16. Navíc kontroluje
 * rozpoznání formátu v hlavičce, odmítnutí neznámého SubFormat a
 * poškozené hlavičky, převod vzorků float a hlavičku zápisu WAV.
 *
 * GUID SubFormat (ksmedia.h, KSDATAFORMAT_SUBTYPE_PCM/IEEE_FLOAT) se ve
 * WAV ukládá little-endian: Data1 (1 = PCM, 3 = float) a ocas
 * 00 00 10 00 80 00 00 AA 00 38 9B 71. Data nepotřebuje.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "libs/mzf/mzf.h"
#include "libs/wav/wav.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/generic_driver/generic_driver.h"
#include "libs/generic_driver/memory_driver.h"
#include "libs/wav_analyzer/wav_analyzer.h"
#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_player.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše OK/FAIL. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )

/** @brief Ocas GUID SubFormat (bajty 4..15) na disku. */
static const uint8_t g_guid_tail[12] = { 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 };

/** @brief Druh vzorku zapisované varianty. */
typedef enum { K_PCM, K_FLOAT } en_KIND;


/** @brief Zapíše uint16 LE. @param f Soubor. @param v Hodnota. */
static void w16 ( FILE *f, uint16_t v ) {
    fputc ( v & 0xFF, f );
    fputc ( v >> 8, f );
}


/** @brief Zapíše uint32 LE. @param f Soubor. @param v Hodnota. */
static void w32 ( FILE *f, uint32_t v ) {
    w16 ( f, ( uint16_t ) v );
    w16 ( f, ( uint16_t ) ( v >> 16 ) );
}


/**
 * @brief Zapíše jeden vzorek v daném formátu.
 * @param f Soubor. @param kind PCM/float. @param bits Kontejner. @param valid Platné bity.
 * @param v Hodnota -1..1.
 */
static void write_sample ( FILE *f, en_KIND kind, uint16_t bits, uint16_t valid, double v ) {
    if ( kind == K_FLOAT ) {
        if ( bits == 32 ) {
            float x = ( float ) v;
            uint32_t u;
            memcpy ( &u, &x, 4 );
            w32 ( f, u );
        } else {
            uint64_t u;
            memcpy ( &u, &v, 8 );
            w32 ( f, ( uint32_t ) u );
            w32 ( f, ( uint32_t ) ( u >> 32 ) );
        }
        return;
    }
    /* PCM: hodnota ve valid bitech, zarovnaná vlevo v kontejneru */
    double scale = ldexp ( 1.0, valid - 1 ) - 1.0;
    int64_t s = ( int64_t ) llround ( v * scale );
    uint64_t u = ( uint64_t ) s << ( bits - valid );
    for ( int i = 0; i < bits / 8; i++ ) fputc ( ( int ) ( ( u >> ( 8 * i ) ) & 0xFF ), f );
}


/**
 * @brief Zapíše WAV variantu.
 * @param path Cesta. @param tag Formátový kód (1, 3, 0xFFFE). @param kind PCM/float (u 0xFFFE SubFormat).
 * @param bits Bity kontejneru. @param valid Platné bity (EXTENSIBLE). @param channels Kanály.
 * @param sub_data1 Data1 SubFormat (0 = podle kind). @param cb_size cbSize (EXTENSIBLE, normálně 22).
 * @param v Vzorky (mono). @param n Počet.
 * @return 1 = zapsáno.
 */
static int write_wav ( const char *path, uint16_t tag, en_KIND kind, uint16_t bits, uint16_t valid, uint16_t channels,
                       uint32_t sub_data1, uint16_t cb_size, const double *v, uint32_t n ) {
    FILE *f = fopen ( path, "wb" );
    if ( !f ) return 0;
    uint32_t rate = 44100;
    uint16_t block = ( uint16_t ) ( channels * bits / 8 );
    uint32_t fmt_size = ( tag == 0xFFFE ) ? 40 : ( tag == 3 ) ? 18 : 16;
    uint32_t data_size = n * block;
    uint32_t fact = ( tag != 1 ) ? 12 : 0;
    fwrite ( "RIFF", 1, 4, f );
    w32 ( f, 4 + 8 + fmt_size + fact + 8 + data_size );
    fwrite ( "WAVEfmt ", 1, 8, f );
    w32 ( f, fmt_size );
    w16 ( f, tag );
    w16 ( f, channels );
    w32 ( f, rate );
    w32 ( f, rate * block );
    w16 ( f, block );
    w16 ( f, bits );
    if ( tag == 3 ) w16 ( f, 0 );
    if ( tag == 0xFFFE ) {
        w16 ( f, cb_size );
        w16 ( f, valid );
        w32 ( f, channels == 1 ? 0x4 : 0x3 );
        w32 ( f, sub_data1 ? sub_data1 : ( kind == K_FLOAT ? 3 : 1 ) );
        fwrite ( g_guid_tail, 1, sizeof ( g_guid_tail ), f );
    }
    if ( fact ) {
        fwrite ( "fact", 1, 4, f );
        w32 ( f, 4 );
        w32 ( f, n );
    }
    fwrite ( "data", 1, 4, f );
    w32 ( f, data_size );
    for ( uint32_t i = 0; i < n; i++ ) {
        for ( uint16_t c = 0; c < channels; c++ ) write_sample ( f, kind, bits, valid, v[i] );
    }
    fclose ( f );
    return 1;
}


/**
 * @brief Otevře WAV a přečte hlavičku knihovnou wav.
 * @param path Cesta. @param[out] h Handler (zavřít generic_driver_close). @param[out] d Driver.
 * @param[out] err Chyba knihovny.
 * @return Hlavička (uvolnit wav_simple_header_destroy), nebo NULL.
 */
static st_WAV_SIMPLE_HEADER* open_header ( const char *path, st_HANDLER *h, st_DRIVER *d, st_HANDLER **hp,
                                           en_WAV_ERROR *err ) {
    generic_driver_file_init ( d );
    *hp = generic_driver_open_file ( h, d, ( char* ) path, FILE_DRIVER_OPMODE_RO );
    if ( !*hp ) {
        *err = WAV_ERROR_IO;
        return NULL;
    }
    return wav_simple_header_new_from_handler ( *hp, err );
}


/**
 * @brief Dekóduje WAV analyzerem a vrátí kopii těla prvního souboru.
 * @param path Cesta. @param channel Kanál. @param[out] body Tělo (uvolnit free). @param[out] size Velikost.
 * @return Počet souborů s CRC hlavičky i těla OK.
 */
static uint32_t decode_body ( const char *path, en_WAV_CHANNEL_SELECT channel, uint8_t **body, uint32_t *size ) {
    st_HANDLER h;
    st_DRIVER d;
    *body = NULL;
    *size = 0;
    generic_driver_file_init ( &d );
    st_HANDLER *hp = generic_driver_open_file ( &h, &d, ( char* ) path, FILE_DRIVER_OPMODE_RO );
    if ( !hp ) return 0;
    st_WAV_ANALYZER_CONFIG cfg;
    wav_analyzer_config_default ( &cfg );
    cfg.channel = channel;
    st_WAV_ANALYZER_RESULT r;
    memset ( &r, 0, sizeof ( r ) );
    en_WAV_ANALYZER_ERROR e = wav_analyzer_analyze ( hp, &cfg, &r );
    generic_driver_close ( hp );
    uint32_t ok = 0;
    if ( e == WAV_ANALYZER_OK && r.file_count == 1 && r.files[0].mzf && r.files[0].header_crc == WAV_CRC_OK
         && r.files[0].body_crc == WAV_CRC_OK ) {
        ok = 1;
        *size = r.files[0].mzf->body_size;
        *body = malloc ( *size ? *size : 1 );
        if ( *body ) memcpy ( *body, r.files[0].mzf->body, *size );
    }
    wav_analyzer_result_destroy ( &r );
    return ok;
}


int main ( void ) {
    memory_driver_init ();

    /* 1. zdrojový PCM WAV: blok 0x40 přes player */
    static uint8_t mzbody[1024];
    for ( int i = 0; i < 1024; i++ ) mzbody[i] = ( uint8_t ) ( i * 7 + 3 );
    st_MZF_HEADER hdr;
    memset ( &hdr, 0, sizeof ( hdr ) );
    hdr.ftype = 1;
    memcpy ( hdr.fname.name, "WAVFMT\r", 7 );
    hdr.fsize = 1024;
    hdr.fstrt = 0x1200;
    hdr.fexec = 0x1200;
    st_TZX_BLOCK *b = tmz_block_create_mz_standard ( TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 0, &hdr, mzbody, 1024 );
    st_TMZ_PLAYER_CONFIG pc;
    tmz_player_config_init ( &pc );
    en_TMZ_PLAYER_ERROR pe;
    st_CMT_STREAM *s = b ? tmz_player_play_block ( b, &pc, &pe ) : NULL;
    int saved = s && cmt_stream_save_wav ( s, 44100, ( char* ) "temp/wavfmt_src.wav" ) == EXIT_SUCCESS;
    cmt_stream_destroy ( s );
    tmz_block_free ( b );
    if ( !saved ) {
        printf ( "FAIL: source WAV not created\n" );
        return 1;
    }

    /* 2. vzorky zdroje */
    st_HANDLER h;
    st_DRIVER d;
    st_HANDLER *hp = NULL;
    en_WAV_ERROR we;
    st_WAV_SIMPLE_HEADER *sh = open_header ( "temp/wavfmt_src.wav", &h, &d, &hp, &we );
    if ( !sh ) {
        printf ( "FAIL: source WAV not readable\n" );
        return 1;
    }
    uint32_t n = sh->blocks;
    double *v = malloc ( n * sizeof ( double ) );
    for ( uint32_t i = 0; v && i < n; i++ ) {
        if ( wav_get_sample_float ( hp, sh, i, 0, &v[i] ) != WAV_OK ) v[i] = 0.0;
        v[i] *= 0.9;   /* rezerva proti přetečení při zaokrouhlení */
    }
    wav_simple_header_destroy ( sh );
    generic_driver_close ( hp );
    if ( !v ) return 1;

    /* 3. reference PCM 16 */
    uint8_t *ref = NULL;
    uint32_t ref_size = 0;
    write_wav ( "temp/wavfmt_pcm16.wav", 1, K_PCM, 16, 16, 1, 0, 22, v, n );
    CHECK ( decode_body ( "temp/wavfmt_pcm16.wav", WAV_CHANNEL_LEFT, &ref, &ref_size ) == 1 && ref_size == 1024
            && memcmp ( ref, mzbody, 1024 ) == 0, "PCM 16 reference decodes" );

    /* 4. varianty */
    static const struct {
        const char *name;
        uint16_t tag;
        en_KIND kind;
        uint16_t bits, valid, channels;
        en_WAV_CHANNEL_SELECT ch;
    } var[] = {
        { "ext_pcm16", 0xFFFE, K_PCM, 16, 16, 1, WAV_CHANNEL_LEFT },
        { "ext_pcm24", 0xFFFE, K_PCM, 24, 24, 1, WAV_CHANNEL_LEFT },
        { "ext_pcm24in32", 0xFFFE, K_PCM, 32, 24, 1, WAV_CHANNEL_LEFT },
        { "float32", 3, K_FLOAT, 32, 32, 1, WAV_CHANNEL_LEFT },
        { "float64", 3, K_FLOAT, 64, 64, 1, WAV_CHANNEL_LEFT },
        { "ext_float32", 0xFFFE, K_FLOAT, 32, 32, 1, WAV_CHANNEL_LEFT },
        { "ext_float64", 0xFFFE, K_FLOAT, 64, 64, 1, WAV_CHANNEL_LEFT },
        { "pcm64", 1, K_PCM, 64, 64, 1, WAV_CHANNEL_LEFT },
        { "stereo_ext_float32_left", 0xFFFE, K_FLOAT, 32, 32, 2, WAV_CHANNEL_LEFT },
        { "stereo_ext_float32_right", 0xFFFE, K_FLOAT, 32, 32, 2, WAV_CHANNEL_RIGHT },
    };
    for ( size_t i = 0; i < sizeof ( var ) / sizeof ( var[0] ); i++ ) {
        char path[96], msg[128];
        snprintf ( path, sizeof ( path ), "temp/wavfmt_%s.wav", var[i].name );
        write_wav ( path, var[i].tag, var[i].kind, var[i].bits, var[i].valid, var[i].channels, 0, 22, v, n );
        uint8_t *body = NULL;
        uint32_t size = 0;
        uint32_t ok = decode_body ( path, var[i].ch, &body, &size );
        snprintf ( msg, sizeof ( msg ), "%s decodes like PCM 16", var[i].name );
        CHECK ( ok == 1 && ref && size == ref_size && memcmp ( body, ref, size ) == 0, msg );
        free ( body );
    }

    /* 5. hlavička: rozpoznání formátu a chyby */
    write_wav ( "temp/wavfmt_u1.wav", 0xFFFE, K_PCM, 16, 16, 1, 0, 22, v, 1000 );
    sh = open_header ( "temp/wavfmt_u1.wav", &h, &d, &hp, &we );
    CHECK ( sh && sh->format_code == WAVE_FORMAT_CODE_EXTENSIBLE && sh->sample_format == WAVE_FORMAT_CODE_PCM
            && sh->valid_bits_per_sample == 16, "EXTENSIBLE PCM header: raw tag 0xFFFE, sample format PCM" );
    wav_simple_header_destroy ( sh );
    if ( hp ) generic_driver_close ( hp );
    write_wav ( "temp/wavfmt_u2.wav", 0xFFFE, K_FLOAT, 32, 32, 1, 0, 22, v, 1000 );
    sh = open_header ( "temp/wavfmt_u2.wav", &h, &d, &hp, &we );
    CHECK ( sh && sh->sample_format == WAVE_FORMAT_CODE_IEEE_FLOAT, "EXTENSIBLE float header: sample format IEEE float" );
    wav_simple_header_destroy ( sh );
    if ( hp ) generic_driver_close ( hp );
    write_wav ( "temp/wavfmt_u3.wav", 0xFFFE, K_PCM, 16, 16, 1, 6, 22, v, 1000 );   /* A-law */
    sh = open_header ( "temp/wavfmt_u3.wav", &h, &d, &hp, &we );
    CHECK ( !sh && we == WAV_ERROR_UNSUPPORTED_CODEC, "EXTENSIBLE with an unknown SubFormat (A-law) rejected" );
    wav_simple_header_destroy ( sh );
    if ( hp ) generic_driver_close ( hp );
    write_wav ( "temp/wavfmt_u4.wav", 0xFFFE, K_PCM, 16, 16, 1, 0, 20, v, 1000 );   /* cbSize 20 */
    sh = open_header ( "temp/wavfmt_u4.wav", &h, &d, &hp, &we );
    CHECK ( !sh && we == WAV_ERROR_CORRUPT_HEADER, "EXTENSIBLE with cbSize < 22 rejected as corrupt" );
    wav_simple_header_destroy ( sh );
    if ( hp ) generic_driver_close ( hp );
    write_wav ( "temp/wavfmt_u5.wav", 3, K_PCM, 16, 16, 1, 0, 22, v, 1000 );   /* float s 16 bity */
    sh = open_header ( "temp/wavfmt_u5.wav", &h, &d, &hp, &we );
    CHECK ( !sh && we == WAV_ERROR_UNSUPPORTED_BPS, "IEEE float with 16 bits rejected" );
    wav_simple_header_destroy ( sh );
    if ( hp ) generic_driver_close ( hp );

    /* 6. převod vzorků float */
    {
        st_WAV_SIMPLE_HEADER fh;
        memset ( &fh, 0, sizeof ( fh ) );
        fh.sample_format = WAVE_FORMAT_CODE_IEEE_FLOAT;
        fh.bits_per_sample = 32;
        float x = 0.5f;
        uint8_t raw[8];
        double out = 0.0;
        memcpy ( raw, &x, 4 );
        CHECK ( wav_decode_sample ( &fh, raw, &out ) == WAV_OK && out == 0.5, "float32 0.5 decoded exactly" );
        x = -0.25f;
        memcpy ( raw, &x, 4 );
        CHECK ( wav_decode_sample ( &fh, raw, &out ) == WAV_OK && out == -0.25, "float32 -0.25 decoded exactly" );
        x = NAN;
        memcpy ( raw, &x, 4 );
        CHECK ( wav_decode_sample ( &fh, raw, &out ) == WAV_OK && out == 0.0, "float32 NaN decoded as 0.0" );
    }

    /* 7. zápis WAV beze změny: 44 B hlavičky, tag 1, fmt 16 B */
    {
        FILE *f = fopen ( "temp/wavfmt_src.wav", "rb" );
        uint8_t hb[44];
        int ok = f && fread ( hb, 1, 44, f ) == 44 && memcmp ( hb + 12, "fmt ", 4 ) == 0 && hb[16] == 16
                 && hb[20] == 1 && hb[21] == 0 && memcmp ( hb + 36, "data", 4 ) == 0;
        if ( f ) fclose ( f );
        CHECK ( ok, "written WAV keeps the plain PCM header (44 B, tag 1, fmt 16 B)" );
    }

    free ( ref );
    free ( v );
    printf ( "\n=== WAV formats Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
