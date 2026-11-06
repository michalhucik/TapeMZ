/**
 * @file   test_cpmtape_roundtrip.c
 * @brief  Round-trip CPM-TAPE: profil PEZIK-2.0A (2400, 3200)
 *         a IC-10.2-CPM -> player -> WAV -> wav_analyzer.
 *
 * Pro každý profil, délku těla (1024, 2, 1, 0 B) a vzorkovací frekvenci
 * (44 100, 48 000, 22 050 Hz) se blok 0x46 přehraje, uloží jako WAV
 * a dekóduje. Čeká se 1 soubor s CRC hlavičky i těla OK, shodnou hlavičkou
 * a tělem, správnou variantou (Pezik / Intercopy) a rychlostí. Totéž s WAV
 * s obrácenou polaritou (export emulátoru je proti PC1 invertovaný).
 * Navíc skutečná nahrávka tstdata/ic-cpmcmt.wav (Intercopy CPM 2400, Y2K).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/generic_driver/generic_driver.h"
#include "libs/generic_driver/memory_driver.h"
#include "libs/wav_analyzer/wav_analyzer.h"
#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_profile.h"
#include "libs/tmz/tmz_player.h"
#include "tstdata_skip.h"

/** @brief Počet selhaných kontrol. */
static int g_failures = 0;

/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_failures++; } \
} while ( 0 )


/**
 * @brief Obrátí polaritu 8bitového WAV (vzorek v -> 255 - v) za hlavičkou 44 B.
 * @param path Soubor (přepíše se). @return EXIT_SUCCESS, nebo EXIT_FAILURE.
 */
static int invert_wav ( const char *path ) {
    FILE *f = fopen ( path, "rb" );
    if ( !f ) return EXIT_FAILURE;
    fseek ( f, 0, SEEK_END );
    long n = ftell ( f );
    fseek ( f, 0, SEEK_SET );
    uint8_t *d = malloc ( ( size_t ) n );
    size_t r = d ? fread ( d, 1, ( size_t ) n, f ) : 0;
    fclose ( f );
    if ( r != ( size_t ) n || n < 44 || d[34] != 8 ) {
        free ( d );
        return EXIT_FAILURE;
    }
    for ( long i = 44; i < n; i++ ) d[i] = ( uint8_t ) ( 255 - d[i] );
    f = fopen ( path, "wb" );
    int ok = f && fwrite ( d, 1, ( size_t ) n, f ) == ( size_t ) n;
    if ( f ) fclose ( f );
    free ( d );
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}


/**
 * @brief Dekóduje WAV a vrátí výsledek (volající uvolní wav_analyzer_result_destroy).
 * @param path WAV. @param r Výsledek. @return Chyba analyzeru.
 */
static en_WAV_ANALYZER_ERROR analyze ( const char *path, st_WAV_ANALYZER_RESULT *r ) {
    st_HANDLER h;
    st_DRIVER d;
    memset ( r, 0, sizeof ( *r ) );
    generic_driver_file_init ( &d );
    st_HANDLER *hp = generic_driver_open_file ( &h, &d, ( char* ) path, FILE_DRIVER_OPMODE_RO );
    if ( !hp ) return WAV_ANALYZER_ERROR_IO;
    st_WAV_ANALYZER_CONFIG cfg;
    wav_analyzer_config_default ( &cfg );
    en_WAV_ANALYZER_ERROR e = wav_analyzer_analyze ( hp, &cfg, r );
    generic_driver_close ( hp );
    return e;
}


/**
 * @brief Jeden roundtrip.
 * @param id Profil. @param bd Rychlost (args.bd). @param size Délka těla. @param rate Frekvence.
 * @param invert 1 = obrátit polaritu WAV. @param variant Očekávaná varianta. @param exp_bd Očekávaná rychlost.
 */
static void roundtrip ( const char *id, uint16_t bd, uint16_t size, uint32_t rate, int invert, int variant, uint16_t exp_bd ) {
    static uint8_t body[1024];
    for ( int i = 0; i < 1024; i++ ) body[i] = ( uint8_t ) ( i * 13 + 7 );
    st_MZF_HEADER h;
    memset ( &h, 0, sizeof ( h ) );
    h.ftype = 1;
    memcpy ( h.fname.name, "CPMTEST\r", 8 );
    h.fsize = size;
    h.fstrt = 0x1200;
    h.fexec = 0x1200;
    st_TMZ_PROFILE_ARGS a;
    tmz_profile_args_init ( &a );
    a.bd = bd;
    en_TMZ_PROFILE_ERROR pe;
    st_TZX_BLOCK *b = tmz_profile_build_block ( id, &a, &h, size ? body : NULL, size, &pe );
    st_TMZ_PLAYER_CONFIG pc;
    tmz_player_config_init ( &pc );
    pc.sample_rate = rate;
    en_TMZ_PLAYER_ERROR ple;
    st_CMT_STREAM *s = b ? tmz_player_play_block ( b, &pc, &ple ) : NULL;
    char path[64];
    snprintf ( path, sizeof ( path ), "temp/cpmtape_rt.wav" );
    int saved = s && cmt_stream_save_wav ( s, rate, path ) == EXIT_SUCCESS;
    cmt_stream_destroy ( s );
    tmz_block_free ( b );
    if ( saved && invert ) saved = invert_wav ( path ) == EXIT_SUCCESS;
    st_WAV_ANALYZER_RESULT r;
    en_WAV_ANALYZER_ERROR e = saved ? analyze ( path, &r ) : WAV_ANALYZER_ERROR_IO;
    int ok = saved && e == WAV_ANALYZER_OK && r.file_count == 1 && r.files[0].mzf
             && r.files[0].format == WAV_TAPE_FORMAT_CPM_TAPE && r.files[0].header_crc == WAV_CRC_OK
             && ( size == 0 || r.files[0].body_crc == WAV_CRC_OK );
    if ( ok ) {
        const st_MZF *m = r.files[0].mzf;
        ok = m->header.fsize == size && memcmp ( m->header.fname.name, "CPMTEST", 7 ) == 0 && m->body_size == size
             && ( size == 0 || memcmp ( m->body, body, size ) == 0 ) && r.files[0].cpmtape_variant == variant
             && r.files[0].cpmtape_bd == exp_bd;
        if ( !ok ) printf ( "  fsize %u body %u variant %u bd %u\n", m->header.fsize, m->body_size,
                            r.files[0].cpmtape_variant, r.files[0].cpmtape_bd );
    } else if ( saved && e == WAV_ANALYZER_OK ) {
        printf ( "  files %u", r.file_count );
        if ( r.file_count ) printf ( ", format %d, CRC %d/%d", r.files[0].format, r.files[0].header_crc, r.files[0].body_crc );
        printf ( "\n" );
    }
    if ( saved && e == WAV_ANALYZER_OK ) wav_analyzer_result_destroy ( &r );
    char msg[128];
    snprintf ( msg, sizeof ( msg ), "%s bd=%u body %u B at %u Hz%s", id, bd ? bd : 2400, size, rate, invert ? " inverted" : "" );
    CHECK ( ok, msg );
}


/**
 * @brief Přehraje blok profilu a vrátí vzorky 8bitového WAV 44 100 Hz (bez hlavičky 44 B).
 * @param id Profil. @param bd Rychlost. @param name Jméno (do 16 znaků). @param size Délka těla.
 * @param[out] n Počet vzorků. @return Vzorky (uvolnit free), nebo NULL.
 */
static uint8_t* play_samples ( const char *id, uint16_t bd, const char *name, uint16_t size, uint32_t *n ) {
    static uint8_t body[2048];
    for ( int i = 0; i < 2048; i++ ) body[i] = ( uint8_t ) ( i * 31 + 5 );
    st_MZF_HEADER h;
    memset ( &h, 0, sizeof ( h ) );
    h.ftype = 1;
    memcpy ( h.fname.name, name, strlen ( name ) );
    h.fname.name[strlen ( name )] = 0x0D;
    h.fsize = size;
    h.fstrt = 0x1200;
    h.fexec = 0x1200;
    st_TMZ_PROFILE_ARGS a;
    tmz_profile_args_init ( &a );
    a.bd = bd;
    a.pause_ms = 500;
    en_TMZ_PROFILE_ERROR pe;
    st_TZX_BLOCK *b = tmz_profile_build_block ( id, &a, &h, body, size, &pe );
    st_TMZ_PLAYER_CONFIG pc;
    tmz_player_config_init ( &pc );
    pc.sample_rate = 44100;
    en_TMZ_PLAYER_ERROR ple;
    st_CMT_STREAM *s = b ? tmz_player_play_block ( b, &pc, &ple ) : NULL;
    int saved = s && cmt_stream_save_wav ( s, 44100, ( char* ) "temp/cpmtape_part.wav" ) == EXIT_SUCCESS;
    cmt_stream_destroy ( s );
    tmz_block_free ( b );
    if ( !saved ) return NULL;
    FILE *f = fopen ( "temp/cpmtape_part.wav", "rb" );
    if ( !f ) return NULL;
    fseek ( f, 0, SEEK_END );
    long len = ftell ( f );
    uint8_t *d = ( len > 44 ) ? malloc ( ( size_t ) len - 44 ) : NULL;
    fseek ( f, 44, SEEK_SET );
    int ok = d && fread ( d, 1, ( size_t ) len - 44, f ) == ( size_t ) len - 44;
    fclose ( f );
    if ( !ok ) {
        free ( d );
        return NULL;
    }
    *n = ( uint32_t ) len - 44;
    return d;
}


/**
 * @brief Zapíše 8bitový mono WAV 44 100 Hz.
 * @param path Soubor. @param d Vzorky. @param n Počet. @return EXIT_SUCCESS, nebo EXIT_FAILURE.
 */
static int write_wav8 ( const char *path, const uint8_t *d, uint32_t n ) {
    uint8_t h[44] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0,
                      0x44, 0xAC, 0, 0, 0x44, 0xAC, 0, 0, 1, 0, 8, 0, 'd', 'a', 't', 'a', 0, 0, 0, 0 };
    uint32_t riff = 36 + n;
    for ( int i = 0; i < 4; i++ ) {
        h[4 + i] = ( uint8_t ) ( riff >> ( 8 * i ) );
        h[40 + i] = ( uint8_t ) ( n >> ( 8 * i ) );
    }
    FILE *f = fopen ( path, "wb" );
    int ok = f && fwrite ( h, 1, 44, f ) == 44 && fwrite ( d, 1, n, f ) == n;
    if ( f ) fclose ( f );
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}


/**
 * @brief Páska z několika záznamů za sebou; vrátí počet souborů s CRC OK a jejich pořadí jmen.
 *
 * Mezi záznamy 0,5 s konstantní úrovně (poslední vzorek předchozího záznamu).
 * damage > 0: v prvním záznamu se úsek damage vzorků od poloviny nahradí
 * konstantou (poškozené tělo).
 *
 * @param ids Profily. @param bds Rychlosti. @param names Jména. @param count Počet záznamů.
 * @param damage Délka poškození prvního záznamu (vzorky), 0 = bez.
 * @param[out] order Jména dekódovaných souborů za sebou (první písmeno každého).
 * @return Počet souborů s CRC hlavičky i těla OK.
 */
static uint32_t tape ( const char *const *ids, const uint16_t *bds, const char *const *names, int count, uint32_t damage,
                       char *order ) {
    uint8_t *all = NULL;
    uint32_t total = 0;
    for ( int k = 0; k < count; k++ ) {
        uint32_t n = 0;
        uint8_t *d = play_samples ( ids[k], bds[k], names[k], 1024, &n );
        if ( !d ) {
            free ( all );
            return 0;
        }
        if ( k == 0 && damage && damage < n / 4 ) memset ( d + n * 3 / 4, d[n * 3 / 4], damage );   /* ve třech čtvrtinách = tělo */
        uint8_t *na = realloc ( all, total + n + 22050 );
        if ( !na ) {
            free ( d );
            free ( all );
            return 0;
        }
        all = na;
        memcpy ( all + total, d, n );
        memset ( all + total + n, d[n - 1], 22050 );
        total += n + 22050;
        free ( d );
    }
    uint32_t ok = 0;
    order[0] = 0;
    if ( write_wav8 ( "temp/cpmtape_tape.wav", all, total ) == EXIT_SUCCESS ) {
        st_WAV_ANALYZER_RESULT r;
        if ( analyze ( "temp/cpmtape_tape.wav", &r ) == WAV_ANALYZER_OK ) {
            for ( uint32_t i = 0; i < r.file_count; i++ ) {
                if ( r.files[i].mzf && r.files[i].header_crc == WAV_CRC_OK && r.files[i].body_crc == WAV_CRC_OK ) {
                    size_t l = strlen ( order );
                    order[l] = ( char ) r.files[i].mzf->header.fname.name[0];
                    order[l + 1] = 0;
                    ok++;
                }
            }
            wav_analyzer_result_destroy ( &r );
        }
    }
    free ( all );
    return ok;
}


/**
 * @brief Zjistí, zda jméno obsahuje "Y2K".
 * @param n Jméno. @param len Délka pole. @return 1 = ano.
 */
static int has_y2k ( const char *n, size_t len ) {
    for ( size_t i = 0; i + 3 <= len; i++ ) {
        if ( memcmp ( n + i, "Y2K", 3 ) == 0 ) return 1;
    }
    return 0;
}


int main ( void ) {
    memory_driver_init ();
    static const struct { const char *id; uint16_t bd; int variant; } prof[] = {
        { "PEZIK-2.0A", 2400, WAV_CPMTAPE_PEZIK },
        { "PEZIK-2.0A", 3200, WAV_CPMTAPE_PEZIK },
        { "IC-10.2-CPM", 0, WAV_CPMTAPE_INTERCOPY },
    };
    static const uint16_t sizes[] = { 1024, 2, 1, 0 };
    static const uint32_t rates[] = { 44100, 48000, 22050 };
    for ( size_t p = 0; p < 3; p++ ) {
        for ( size_t k = 0; k < 4; k++ ) {
            for ( size_t r = 0; r < 3; r++ ) {
                roundtrip ( prof[p].id, prof[p].bd, sizes[k], rates[r], 0, prof[p].variant, prof[p].bd ? prof[p].bd : 2400 );
            }
        }
        roundtrip ( prof[p].id, prof[p].bd, 1024, 44100, 1, prof[p].variant, prof[p].bd ? prof[p].bd : 2400 );
    }

    /*
     * Smíšená páska: CPM a za ním NORMAL; dřív se záznam CPM
     * ztratil (falešný leader NORMAL v hlavičce CPM si přivlastnil úsek)
     */
    {
        char order[16];
        static const char *const ids1[2] = { "IC-10.2-CPM", "ROM-800-NOMINAL" };
        static const char *const ids2[2] = { "PEZIK-2.0A", "ROM-800-NOMINAL" };
        static const uint16_t bd2[2] = { 2400, 0 };
        static const uint16_t bd0[2] = { 0, 0 };
        static const char *const nm[2] = { "CPMFIRST", "NORMAL2" };
        uint32_t n = tape ( ids1, bd0, nm, 2, 0, order );
        char msg[96];
        snprintf ( msg, sizeof ( msg ), "IC-10.2-CPM + NORMAL: 2 files in tape order (got %u, %s)", n, order );
        CHECK ( n == 2 && strcmp ( order, "CN" ) == 0, msg );
        n = tape ( ids2, bd2, nm, 2, 0, order );
        snprintf ( msg, sizeof ( msg ), "PEZIK-2.0A + NORMAL: 2 files in tape order (got %u, %s)", n, order );
        CHECK ( n == 2 && strcmp ( order, "CN" ) == 0, msg );
        /* tři záznamy CPM, první s poškozeným tělem: ostatní dva se přečtou */
        static const char *const ids3[3] = { "PEZIK-2.0A", "PEZIK-2.0A", "IC-10.2-CPM" };
        static const uint16_t bd3[3] = { 2400, 3200, 0 };
        static const char *const nm3[3] = { "AFIRST", "BSECOND", "CTHIRD" };
        n = tape ( ids3, bd3, nm3, 3, 4410, order );
        snprintf ( msg, sizeof ( msg ), "3 CPM records, first damaged: 2 good files decoded (got %u, %s)", n, order );
        CHECK ( n == 2 && strcmp ( order, "BC" ) == 0, msg );
    }

    /* skutečná nahrávka: Intercopy CPM 2400, Y2K (32 747 B) */
    if ( !tstdata_skip ( "test_cpmtape_roundtrip (ic-cpmcmt.wav)" ) ) {
        st_WAV_ANALYZER_RESULT r;
        en_WAV_ANALYZER_ERROR e = analyze ( "../tstdata/ic-cpmcmt.wav", &r );
        int ok = e == WAV_ANALYZER_OK && r.file_count == 1 && r.files[0].mzf && r.files[0].format == WAV_TAPE_FORMAT_CPM_TAPE
                 && r.files[0].header_crc == WAV_CRC_OK && r.files[0].body_crc == WAV_CRC_OK
                 && r.files[0].mzf->body_size == 32747 && r.files[0].cpmtape_variant == WAV_CPMTAPE_INTERCOPY
                 && has_y2k ( ( const char* ) r.files[0].mzf->header.fname.name, sizeof ( r.files[0].mzf->header.fname.name ) );
        if ( e == WAV_ANALYZER_OK ) {
            if ( !ok ) printf ( "  files %u\n", r.file_count );
            wav_analyzer_result_destroy ( &r );
        }
        CHECK ( ok, "ic-cpmcmt.wav: 1 file Y2K, 32747 B, CRC OK, Intercopy" );
    }
    printf ( "\n=== CPM-TAPE Round-trip Test: %s ===\n", g_failures ? "FAIL" : "PASS" );
    return g_failures ? 1 : 0;
}
