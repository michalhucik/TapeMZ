/**
 * @file   test_bsd_wav.c
 * @brief  Komplexni integracni test BSD formatu: WAV dekodovani, re-enkodovani, round-trip.
 *
 * Testovaci program pro BSD/BRD chunkovany format:
 * 1. Dekóduje nahrávku z emulátoru tstdata/basic-bsd.wav pres wav_analyzer
 * 2. Overi format BSD, ftype (0x03/0x04), header CRC, body CRC
 * 3. Sestavi BSD chunky z dekodovanych dat
 * 4. Re-enkoduje profilem BASIC-1Z016 (blok 0x46, player) do WAV
 * 5. Dekoduje re-enkodovany WAV a porovna s originalem (i počet kusů a CRC)
 * 6. Vytvori TMZ blok 0x45, naparsuje zpet a overi shodu dat
 * 7. Poškozený kus: data z kopie, nebo CRC ERROR při poškození obou kopií
 * 8. ftype 03h s fsize 0 se dekóduje jako BSD
 *
 * @par Licence:
 * GNU General Public License v3 (GPLv3)
 *
 * Copyright (C) 2026 Michal Hucik <hucik@ordoz.com>
 * Bez tstdata/ vypíše SKIP a skončí 0 (tests/tstdata_skip.h).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/mzf/mzf.h"
#include "libs/cmt_stream/cmt_stream.h"
#include "libs/endianity/endianity.h"
#include "libs/generic_driver/generic_driver.h"
#include "libs/generic_driver/memory_driver.h"
#include "libs/wav_analyzer/wav_analyzer.h"
#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_profile.h"
#include "libs/tmz/tmz_player.h"
#include "libs/tzx/tzx.h"
#include "tstdata_skip.h"


/** @brief Cesta k testovaci nahravce (relativni z build/). */
static const char *g_wav_path = "../tstdata/basic-bsd.wav";

/** @brief Cesta k docasnemu WAV souboru pro round-trip. */
static const char *g_tmp_wav = "temp/bsd_roundtrip.wav";

/** @brief Vzorkovaci frekvence pro re-enkodovani. */
#define SAMPLE_RATE  44100

/** @brief Počet selhaných kontrol CHECK (testy 5 a 6). */
static int g_checks_failed = 0;

/** @brief Vyhodnotí podmínku a vypíše výsledek. */
#define CHECK( cond, msg ) do { \
    if ( cond ) { printf ( "OK: %s\n", msg ); } \
    else { printf ( "FAIL: %s\n", msg ); g_checks_failed++; } \
} while ( 0 )


/**
 * @brief Sestavi BSD chunky z raw body dat pro re-enkodovani.
 *
 * Rozreze data na 256B bloky s 2B chunk ID (LE). Posledni chunk
 * dostane ID=0xFFFF (terminacni marker). BSD dekoder zahrnuje
 * data terminacniho chunku do body, proto se nevytvari extra
 * prazdny terminacni chunk - posledni datovy chunk = terminacni.
 *
 * Pokud body_size == 0, vytvori se jediny chunk (terminacni,
 * nulova data).
 *
 * @param body_data Telo MZF souboru.
 * @param body_size Velikost tela v bajtech.
 * @param[out] out_chunks Alokovane pole chunku (chunk_count * 258B).
 * @param[out] out_chunk_count Pocet chunku (vcetne terminacniho).
 * @return 0 pri uspechu, -1 pri chybe.
 *
 * @post Pri uspechu volajici vlastni *out_chunks a musi ho uvolnit pres free().
 */
static int build_chunks ( const uint8_t *body_data, uint32_t body_size,
                           uint8_t **out_chunks, uint16_t *out_chunk_count ) {
    uint32_t total_chunks;
    if ( body_size == 0 ) {
        total_chunks = 1; /* pouze terminacni chunk */
    } else {
        total_chunks = ( body_size + TMZ_BASIC_CHUNK_DATA_SIZE - 1 ) / TMZ_BASIC_CHUNK_DATA_SIZE;
    }

    size_t chunks_bytes = (size_t) total_chunks * TMZ_BASIC_CHUNK_SIZE;

    uint8_t *chunks = calloc ( 1, chunks_bytes );
    if ( !chunks ) return -1;

    for ( uint32_t i = 0; i < total_chunks; i++ ) {
        uint8_t *chunk = chunks + (size_t) i * TMZ_BASIC_CHUNK_SIZE;

        /* chunk ID: posledni chunk dostane 0xFFFF, ostatni sekvencni */
        uint16_t chunk_id = ( i == total_chunks - 1 )
                            ? TMZ_BASIC_LAST_CHUNK_ID
                            : (uint16_t) i;
        uint16_t id_le = endianity_bswap16_LE ( chunk_id );
        memcpy ( chunk, &id_le, 2 );

        /* chunk data (256B, zbytek nulovy z calloc) */
        if ( body_data && body_size > 0 ) {
            size_t offset = (size_t) i * TMZ_BASIC_CHUNK_DATA_SIZE;
            if ( offset < body_size ) {
                size_t remaining = body_size - offset;
                size_t copy_size = ( remaining < TMZ_BASIC_CHUNK_DATA_SIZE )
                                   ? remaining : TMZ_BASIC_CHUNK_DATA_SIZE;
                memcpy ( chunk + 2, body_data + offset, copy_size );
            }
        }
    }

    *out_chunks = chunks;
    *out_chunk_count = (uint16_t) total_chunks;
    return 0;
}


/**
 * @brief Dekoduje WAV soubor pres wav_analyzer a vrati vysledek.
 *
 * @param wav_path Cesta k WAV souboru.
 * @param[out] result Vysledek analyzy.
 * @return 0 pri uspechu, 1 pri chybe.
 */
static int decode_wav ( const char *wav_path, st_WAV_ANALYZER_RESULT *result ) {
    st_HANDLER h;
    st_DRIVER d;
    generic_driver_file_init ( &d );

    st_HANDLER *h_in = generic_driver_open_file ( &h, &d, (char*) wav_path, FILE_DRIVER_OPMODE_RO );
    if ( !h_in ) {
        fprintf ( stderr, "ERROR: cannot open '%s'\n", wav_path );
        return 1;
    }

    st_WAV_ANALYZER_CONFIG config;
    wav_analyzer_config_default ( &config );

    en_WAV_ANALYZER_ERROR err = wav_analyzer_analyze ( h_in, &config, result );
    generic_driver_close ( h_in );

    if ( err != WAV_ANALYZER_OK ) {
        fprintf ( stderr, "ERROR: analysis failed: %s\n", wav_analyzer_error_string ( err ) );
        return 1;
    }

    return 0;
}


/**
 * @brief Test 1: dekódování BSD nahrávky z emulátoru.
 *
 * @param[out] out_result Vysledek analyzy (pro pouziti v dalsich testech).
 * @return Pocet selhani.
 */
static int test_decode_real_wav ( st_WAV_ANALYZER_RESULT *out_result ) {
    int failures = 0;

    printf ( "=== Test 1: Decoding BSD recording (emulator) ===\n\n" );

    if ( decode_wav ( g_wav_path, out_result ) != 0 ) {
        printf ( "FAIL: cannot decode WAV\n" );
        return 1;
    }

    printf ( "WAV: %u Hz, %.1f sec, %u pulses, %u leaders, %u files\n",
             out_result->sample_rate, out_result->wav_duration_sec,
             out_result->total_pulses, out_result->leaders.count,
             out_result->file_count );

    /* minimalne 1 soubor */
    if ( out_result->file_count < 1 ) {
        printf ( "FAIL: no files decoded\n" );
        return 1;
    }
    printf ( "OK: %u files decoded\n", out_result->file_count );

    const st_WAV_ANALYZER_FILE_RESULT *f = &out_result->files[0];

    /* format musi byt BSD */
    if ( f->format != WAV_TAPE_FORMAT_BSD ) {
        printf ( "FAIL: expected format BSD, got %s\n", wav_tape_format_name ( f->format ) );
        failures++;
    } else {
        printf ( "OK: format = BSD\n" );
    }

    /* musi byt dekodovany MZF */
    if ( !f->mzf ) {
        printf ( "FAIL: no MZF decoded\n" );
        return failures + 1;
    }

    /* ftype musi byt BSD (0x03) nebo BRD (0x04) */
    if ( f->mzf->header.ftype != MZF_FTYPE_BSD && f->mzf->header.ftype != MZF_FTYPE_BRD ) {
        printf ( "FAIL: ftype = 0x%02X, expected 0x03 (BSD) or 0x04 (BRD)\n",
                 f->mzf->header.ftype );
        failures++;
    } else {
        printf ( "OK: ftype = 0x%02X (%s)\n", f->mzf->header.ftype,
                 f->mzf->header.ftype == MZF_FTYPE_BSD ? "BSD" : "BRD" );
    }

    /* body musi existovat a mit rozumnou velikost */
    if ( !f->mzf->body || f->mzf->body_size == 0 ) {
        printf ( "FAIL: no body data decoded\n" );
        failures++;
    } else {
        printf ( "OK: body_size = %u bytes (%u chunks)\n",
                 f->mzf->body_size,
                 ( f->mzf->body_size + 255 ) / 256 );
    }

    /* header CRC */
    if ( f->header_crc == WAV_CRC_OK ) {
        printf ( "OK: header CRC\n" );
    } else {
        printf ( "FAIL: header CRC = %s\n",
                 f->header_crc == WAV_CRC_ERROR ? "ERROR" : "N/A" );
        failures++;
    }

    /* body CRC (posledni chunk) */
    if ( f->body_crc == WAV_CRC_OK ) {
        printf ( "OK: body CRC\n" );
    } else if ( f->body_crc == WAV_CRC_NOT_AVAILABLE ) {
        printf ( "INFO: body CRC not available (expected for BSD - per-chunk CRC)\n" );
    } else {
        printf ( "FAIL: body CRC = ERROR\n" );
        failures++;
    }

    printf ( "\n" );
    return failures;
}


/**
 * @brief Test 2: WAV round-trip (dekodovany MZF -> BSD WAV -> dekodovani zpet).
 *
 * @param orig_mzf Puvodni dekodovany MZF (z testu 1).
 * @return Pocet selhani.
 */
static int test_wav_roundtrip ( const st_MZF *orig_mzf ) {
    int failures = 0;

    printf ( "=== Test 2: WAV round-trip (re-enkodovani + dekodovani) ===\n\n" );

    /* sestaveni chunku z body dat */
    uint8_t *chunks = NULL;
    uint16_t chunk_count = 0;

    if ( build_chunks ( orig_mzf->body, orig_mzf->body_size, &chunks, &chunk_count ) != 0 ) {
        printf ( "FAIL: build_chunks failed\n" );
        return 1;
    }

    printf ( "Built %u chunks (%u data + 1 termination)\n", chunk_count, chunk_count - 1 );

    /*
     * Hlavička pro profil BASIC-1Z016 v pořadí hostitele. BSD má fsize,
     * fstrt a fexec 0 - na rozdíl od dekódovaného MZF, kde fsize obsahuje
     * celkovou velikost dat ze všech kusů (doplněnou dekodérem).
     */
    st_MZF_HEADER hdr = orig_mzf->header;
    hdr.fsize = 0;
    hdr.fstrt = 0;
    hdr.fexec = 0;

    /* re-enkódování profilem BASIC-1Z016 přes player */
    st_TMZ_PROFILE_ARGS pa;
    tmz_profile_args_init ( &pa );
    en_TMZ_PROFILE_ERROR pe;
    st_TZX_BLOCK *block = tmz_profile_build_block ( "BASIC-1Z016", &pa, &hdr, chunks,
                                                    ( uint32_t ) chunk_count * 258, &pe );
    free ( chunks );
    st_TMZ_PLAYER_CONFIG pc;
    tmz_player_config_init ( &pc );
    pc.sample_rate = SAMPLE_RATE;
    en_TMZ_PLAYER_ERROR ple;
    st_CMT_STREAM *stream = block ? tmz_player_play_block ( block, &pc, &ple ) : NULL;
    tmz_block_free ( block );

    if ( !stream ) {
        printf ( "FAIL: BASIC-1Z016 stream failed\n" );
        return 1;
    }

    printf ( "BSD stream created: %.3f sec\n", cmt_stream_get_length ( stream ) );

    /* ulozeni do WAV */
    int result = cmt_stream_save_wav ( stream, SAMPLE_RATE, (char*) g_tmp_wav );
    cmt_stream_destroy ( stream );

    if ( result != EXIT_SUCCESS ) {
        printf ( "FAIL: cmt_stream_save_wav failed\n" );
        return 1;
    }

    printf ( "WAV written: %s\n", g_tmp_wav );

    /* dekodovani re-enkodovaneho WAV */
    st_WAV_ANALYZER_RESULT rt_result;
    if ( decode_wav ( g_tmp_wav, &rt_result ) != 0 ) {
        printf ( "FAIL: cannot decode re-encoded WAV\n" );
        return 1;
    }

    if ( rt_result.file_count < 1 ) {
        printf ( "FAIL: no files in re-encoded WAV\n" );
        wav_analyzer_result_destroy ( &rt_result );
        return 1;
    }

    const st_WAV_ANALYZER_FILE_RESULT *f = &rt_result.files[0];

    /* format */
    if ( f->format != WAV_TAPE_FORMAT_BSD ) {
        printf ( "FAIL: round-trip format = %s, expected BSD\n", wav_tape_format_name ( f->format ) );
        failures++;
    } else {
        printf ( "OK: round-trip format = BSD\n" );
    }

    if ( !f->mzf ) {
        printf ( "FAIL: no MZF in round-trip\n" );
        wav_analyzer_result_destroy ( &rt_result );
        return failures + 1;
    }

    /* ftype */
    if ( f->mzf->header.ftype != orig_mzf->header.ftype ) {
        printf ( "FAIL: ftype mismatch: orig=0x%02X, rt=0x%02X\n",
                 orig_mzf->header.ftype, f->mzf->header.ftype );
        failures++;
    } else {
        printf ( "OK: ftype = 0x%02X\n", f->mzf->header.ftype );
    }

    /* body size */
    if ( f->mzf->body_size != orig_mzf->body_size ) {
        printf ( "FAIL: body_size mismatch: orig=%u, rt=%u\n",
                 orig_mzf->body_size, f->mzf->body_size );
        failures++;
    } else {
        printf ( "OK: body_size = %u\n", f->mzf->body_size );
    }

    /* body data */
    if ( f->mzf->body && f->mzf->body_size == orig_mzf->body_size ) {
        if ( memcmp ( f->mzf->body, orig_mzf->body, orig_mzf->body_size ) != 0 ) {
            for ( uint32_t j = 0; j < orig_mzf->body_size; j++ ) {
                if ( f->mzf->body[j] != orig_mzf->body[j] ) {
                    printf ( "FAIL: body differs at offset %u (orig=0x%02X, rt=0x%02X)\n",
                             j, orig_mzf->body[j], f->mzf->body[j] );
                    break;
                }
            }
            failures++;
        } else {
            printf ( "OK: body data matches (%u bytes)\n", orig_mzf->body_size );
        }
    }

    /* header CRC */
    if ( f->header_crc == WAV_CRC_OK ) {
        printf ( "OK: round-trip header CRC\n" );
    } else {
        printf ( "FAIL: round-trip header CRC = %s\n",
                 f->header_crc == WAV_CRC_ERROR ? "ERROR" : "N/A" );
        failures++;
    }

    /* počet kusů, žádná chyba CRC, kopie nepoužitá */
    if ( f->bsd_chunks == chunk_count && f->bsd_crc_errors == 0 && f->bsd_from_copy == 0
         && f->body_crc == WAV_CRC_OK ) {
        printf ( "OK: round-trip %u chunks, no CRC error\n", f->bsd_chunks );
    } else {
        printf ( "FAIL: round-trip chunks %u (expected %u), CRC errors %u, from copy %u\n",
                 f->bsd_chunks, chunk_count, f->bsd_crc_errors, f->bsd_from_copy );
        failures++;
    }

    wav_analyzer_result_destroy ( &rt_result );
    printf ( "\n" );
    return failures;
}


/**
 * @brief Zapíše 8bitový mono WAV 44 100 Hz (jako test_cpmtape_roundtrip.c).
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
 * @brief Přehraje BASIC-1Z016 do 8bitových vzorků 44,1 kHz (přes temp/bsd_part.wav).
 * @param ftype Typ souboru v hlavičce. @param nchunks Počet kusů 1 až 3 (data = vzorek bajtů).
 * @param[out] n Počet vzorků. @return Vzorky (uvolnit free), nebo NULL.
 */
static uint8_t* basic_samples ( uint8_t ftype, uint16_t nchunks, uint32_t *n ) {
    static uint8_t chunks[3 * 258];
    for ( uint32_t i = 0; i < sizeof ( chunks ); i++ ) chunks[i] = ( uint8_t ) ( i * 13 + 7 );
    for ( uint16_t k = 0; k < nchunks; k++ ) {
        uint16_t id = ( k + 1 == nchunks ) ? 0xFFFF : k;
        chunks[k * 258] = ( uint8_t ) id;
        chunks[k * 258 + 1] = ( uint8_t ) ( id >> 8 );
    }
    st_MZF_HEADER h;
    memset ( &h, 0, sizeof ( h ) );
    h.ftype = ftype;
    memcpy ( h.fname.name, "DATA", 4 );
    h.fname.name[4] = 0x0D;
    st_TMZ_PROFILE_ARGS a;
    tmz_profile_args_init ( &a );
    en_TMZ_PROFILE_ERROR pe;
    st_TZX_BLOCK *b = tmz_profile_build_block ( "BASIC-1Z016", &a, &h, chunks, nchunks * 258u, &pe );
    st_TMZ_PLAYER_CONFIG pc;
    tmz_player_config_init ( &pc );
    pc.sample_rate = 44100;
    en_TMZ_PLAYER_ERROR ple;
    st_CMT_STREAM *s = b ? tmz_player_play_block ( b, &pc, &ple ) : NULL;
    int saved = s && cmt_stream_save_wav ( s, 44100, ( char* ) "temp/bsd_part.wav" ) == EXIT_SUCCESS;
    cmt_stream_destroy ( s );
    tmz_block_free ( b );
    if ( !saved ) return NULL;
    FILE *f = fopen ( "temp/bsd_part.wav", "rb" );
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
 * @brief Najde úseky signálu oddělené během konstantních vzorků >= 22050 (0,5 s).
 * @param d Vzorky. @param n Počet. @param[out] st Začátky. @param[out] en Konce. @param max Kapacita.
 * @return Počet úseků.
 */
static int find_segments ( const uint8_t *d, uint32_t n, uint32_t *st, uint32_t *en, int max ) {
    int k = 0;
    uint32_t i = 0;
    while ( i < n && k < max ) {
        uint32_t s = i;
        uint32_t run = 0;
        while ( i < n ) {
            run = ( i > 0 && d[i] == d[i - 1] ) ? run + 1 : 0;
            if ( run >= 22050 ) break;
            i++;
        }
        st[k] = s;
        en[k] = i - run;
        k++;
        while ( i < n && d[i] == d[i - 1] ) i++;
    }
    return k;
}


/**
 * @brief Poškodí 40 vzorků úseku na poměrné pozici frac (konstanta = vzorek před).
 * @param d Vzorky. @param st Začátek úseku. @param en Konec úseku. @param frac Poměrná pozice 0..1.
 */
static void damage ( uint8_t *d, uint32_t st, uint32_t en, double frac ) {
    uint32_t p = st + ( uint32_t ) ( ( en - st ) * frac );
    memset ( d + p, d[p - 1], 40 );
}


/**
 * @brief Kus 1 (úsek 2): poškození 1. kopie, pak obou kopií.
 *
 * Úsek kusu BASIC-1Z016 trvá asi 9,3 s: tón 0 až 61 %, 1. kopie 61 až 80 %,
 * oddělovač, kopie 81 až 100 % (z půlvln profilu BASIC-1Z016). Poškození 40 vzorků změní bajt.
 */
static void test_corrupt_chunk ( void ) {
    printf ( "=== Test 5: poškozený kus (kopie, CRC každého kusu) ===\n\n" );
    uint32_t n = 0;
    uint32_t st[8], en[8];
    for ( int both = 0; both < 2; both++ ) {
        uint8_t *d = basic_samples ( 0x04, 3, &n );
        int segs = d ? find_segments ( d, n, st, en, 8 ) : 0;
        CHECK ( segs == 4, "BASIC-1Z016 3 chunks = 4 segments" );
        if ( segs != 4 ) {
            free ( d );
            return;
        }
        damage ( d, st[2], en[2], 0.70 );
        if ( both ) damage ( d, st[2], en[2], 0.90 );
        st_WAV_ANALYZER_RESULT r;
        int ok = write_wav8 ( "temp/bsd_damaged.wav", d, n ) == EXIT_SUCCESS
                 && decode_wav ( "temp/bsd_damaged.wav", &r ) == 0;
        free ( d );
        CHECK ( ok && r.file_count == 1 && r.files[0].format == WAV_TAPE_FORMAT_BSD, "damaged BSD decodes as one file" );
        if ( !ok ) continue;
        if ( r.file_count == 1 ) {
            const st_WAV_ANALYZER_FILE_RESULT *fr = &r.files[0];
            printf ( "   body CRC %d, chunks %u, CRC errors %u, from copy %u\n", fr->body_crc, fr->bsd_chunks,
                     fr->bsd_crc_errors, fr->bsd_from_copy );
            if ( !both ) {
                CHECK ( fr->body_crc == WAV_CRC_OK && fr->bsd_from_copy == 1 && fr->bsd_crc_errors == 0,
                        "chunk 1 first copy damaged -> data from copy, CRC OK" );
            } else {
                CHECK ( fr->body_crc == WAV_CRC_ERROR && fr->bsd_crc_errors == 1,
                        "chunk 1 both copies damaged -> CRC ERROR although the last chunk is OK" );
            }
        }
        wav_analyzer_result_destroy ( &r );
    }
    printf ( "\n" );
}


/**
 * @brief Hlavička ftype 03h a fsize 0 se dekóduje jako BSD.
 */
static void test_ftype03 ( void ) {
    printf ( "=== Test 6: ftype 03h ===\n\n" );
    uint32_t n = 0;
    uint8_t *d = basic_samples ( 0x03, 1, &n );
    st_WAV_ANALYZER_RESULT r;
    int ok = d && write_wav8 ( "temp/bsd_ftype03.wav", d, n ) == EXIT_SUCCESS
             && decode_wav ( "temp/bsd_ftype03.wav", &r ) == 0;
    free ( d );
    CHECK ( ok && r.file_count == 1 && r.files[0].format == WAV_TAPE_FORMAT_BSD, "ftype 03h with fsize 0 -> BSD" );
    if ( ok ) wav_analyzer_result_destroy ( &r );
    printf ( "\n" );
}


/**
 * @brief Test 3: TMZ blok 0x45 round-trip (vytvoreni + parsovani).
 *
 * @param orig_mzf Puvodni dekodovany MZF (z testu 1).
 * @return Pocet selhani.
 */
static int test_tmz_block_roundtrip ( const st_MZF *orig_mzf ) {
    int failures = 0;

    printf ( "=== Test 3: TMZ blok 0x45 round-trip ===\n\n" );

    /* sestaveni chunku z body dat */
    uint8_t *chunks = NULL;
    uint16_t chunk_count = 0;

    if ( build_chunks ( orig_mzf->body, orig_mzf->body_size, &chunks, &chunk_count ) != 0 ) {
        printf ( "FAIL: build_chunks failed\n" );
        return 1;
    }

    /* vytvoreni TMZ bloku 0x45 */
    st_TZX_BLOCK *block = tmz_block_create_mz_basic_data (
        TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 1000,
        &orig_mzf->header, chunks, chunk_count
    );

    free ( chunks );

    if ( !block ) {
        printf ( "FAIL: tmz_block_create_mz_basic_data failed\n" );
        return 1;
    }

    printf ( "TMZ block 0x45 created: %u bytes\n", block->length );

    /* parsovani bloku zpet */
    /* potrebujeme kopii, protoze parse modifikuje data */
    uint8_t *data_copy = malloc ( block->length );
    if ( !data_copy ) {
        printf ( "FAIL: alloc failed\n" );
        tmz_block_free ( block );
        return 1;
    }
    memcpy ( data_copy, block->data, block->length );

    st_TZX_BLOCK block_copy = *block;
    block_copy.data = data_copy;

    en_TZX_ERROR tzx_err;
    uint8_t *parsed_chunks = NULL;
    st_TMZ_MZ_BASIC_DATA *bsd = tmz_block_parse_mz_basic_data ( &block_copy, &parsed_chunks, &tzx_err );

    if ( !bsd ) {
        printf ( "FAIL: tmz_block_parse_mz_basic_data failed: %s\n", tzx_error_string ( tzx_err ) );
        free ( data_copy );
        tmz_block_free ( block );
        return 1;
    }

    printf ( "Parsed: chunk_count=%u\n", bsd->chunk_count );

    /* overeni hlavicky */
    if ( bsd->mzf_header.ftype != orig_mzf->header.ftype ) {
        printf ( "FAIL: TMZ ftype mismatch: orig=0x%02X, parsed=0x%02X\n",
                 orig_mzf->header.ftype, bsd->mzf_header.ftype );
        failures++;
    } else {
        printf ( "OK: TMZ ftype = 0x%02X\n", bsd->mzf_header.ftype );
    }

    /*
     * Overeni dat chunku - sestavime body z naparsovanych chunku.
     * BSD dekoder zahrnuje data vsech chunku vcetne terminacniho
     * (ID=0xFFFF), proto je musime take zahrnout.
     */
    uint32_t reassembled_size = (uint32_t) bsd->chunk_count * TMZ_BASIC_CHUNK_DATA_SIZE;
    printf ( "Chunks: %u, reassembled_size: %u\n", bsd->chunk_count, reassembled_size );

    /* porovnani body dat */
    if ( reassembled_size >= orig_mzf->body_size ) {
        uint8_t *reassembled = malloc ( reassembled_size );
        if ( reassembled ) {
            for ( uint16_t i = 0; i < bsd->chunk_count; i++ ) {
                const uint8_t *chunk = parsed_chunks + (size_t) i * TMZ_BASIC_CHUNK_SIZE;
                memcpy ( reassembled + (size_t) i * TMZ_BASIC_CHUNK_DATA_SIZE,
                         chunk + 2, TMZ_BASIC_CHUNK_DATA_SIZE );
            }

            if ( memcmp ( reassembled, orig_mzf->body, orig_mzf->body_size ) != 0 ) {
                for ( uint32_t j = 0; j < orig_mzf->body_size; j++ ) {
                    if ( reassembled[j] != orig_mzf->body[j] ) {
                        printf ( "FAIL: TMZ body differs at offset %u (orig=0x%02X, parsed=0x%02X)\n",
                                 j, orig_mzf->body[j], reassembled[j] );
                        break;
                    }
                }
                failures++;
            } else {
                printf ( "OK: TMZ body data matches (%u bytes)\n", orig_mzf->body_size );
            }

            free ( reassembled );
        }
    } else {
        printf ( "FAIL: reassembled_size %u < orig body_size %u\n",
                 reassembled_size, orig_mzf->body_size );
        failures++;
    }

    free ( data_copy );
    tmz_block_free ( block );
    printf ( "\n" );
    return failures;
}


/** @brief Cesta k docasnemu TMZ souboru pro round-trip. */
static const char *g_tmp_tmz = "temp/bsd_roundtrip.tmz";


/**
 * @brief Test 4: TMZ soubor round-trip (zapis do souboru + cteni zpet).
 *
 * @param orig_mzf Puvodni dekodovany MZF (z testu 1).
 * @return Pocet selhani.
 */
static int test_tmz_file_roundtrip ( const st_MZF *orig_mzf ) {
    int failures = 0;

    printf ( "=== Test 4: TMZ soubor round-trip (zapis + cteni) ===\n\n" );

    /* sestaveni chunku */
    uint8_t *chunks = NULL;
    uint16_t chunk_count = 0;

    if ( build_chunks ( orig_mzf->body, orig_mzf->body_size, &chunks, &chunk_count ) != 0 ) {
        printf ( "FAIL: build_chunks failed\n" );
        return 1;
    }

    /* vytvoreni TMZ bloku 0x45 */
    st_TZX_BLOCK *block = tmz_block_create_mz_basic_data (
        TMZ_MACHINE_MZ800, TMZ_PULSESET_800, 1000,
        &orig_mzf->header, chunks, chunk_count
    );

    free ( chunks );

    if ( !block ) {
        printf ( "FAIL: tmz_block_create_mz_basic_data failed\n" );
        return 1;
    }

    /* sestaveni TMZ souboru */
    st_TZX_FILE tmz_file;
    tmz_header_init ( &tmz_file.header );
    tmz_file.is_tmz = true;
    tmz_file.block_count = 1;
    tmz_file.blocks = block;

    /* zapis do souboru */
    st_HANDLER out_handler;
    st_DRIVER out_driver;
    generic_driver_file_init ( &out_driver );

    st_HANDLER *h_out = generic_driver_open_file ( &out_handler, &out_driver, (char*) g_tmp_tmz, FILE_DRIVER_OPMODE_W );
    if ( !h_out ) {
        printf ( "FAIL: cannot create TMZ file '%s'\n", g_tmp_tmz );
        tmz_block_free ( block );
        return 1;
    }

    en_TZX_ERROR tmz_err = tzx_save ( h_out, &tmz_file );
    generic_driver_close ( h_out );

    tmz_block_free ( block );

    if ( tmz_err != TZX_OK ) {
        printf ( "FAIL: tzx_save failed: %s\n", tzx_error_string ( tmz_err ) );
        return 1;
    }

    printf ( "TMZ written: %s\n", g_tmp_tmz );

    /* cteni zpet */
    st_HANDLER in_handler;
    st_DRIVER in_driver;
    generic_driver_file_init ( &in_driver );

    st_HANDLER *h_in = generic_driver_open_file ( &in_handler, &in_driver, (char*) g_tmp_tmz, FILE_DRIVER_OPMODE_RO );
    if ( !h_in ) {
        printf ( "FAIL: cannot open TMZ file '%s'\n", g_tmp_tmz );
        return 1;
    }

    en_TZX_ERROR load_err;
    st_TZX_FILE *loaded = tzx_load ( h_in, &load_err );
    generic_driver_close ( h_in );

    if ( !loaded ) {
        printf ( "FAIL: tzx_load failed: %s\n", tzx_error_string ( load_err ) );
        return 1;
    }

    /* overeni */
    if ( !loaded->is_tmz ) {
        printf ( "FAIL: loaded file is not TMZ\n" );
        failures++;
    } else {
        printf ( "OK: loaded as TMZ\n" );
    }

    if ( loaded->block_count != 1 ) {
        printf ( "FAIL: block_count = %u, expected 1\n", loaded->block_count );
        failures++;
    } else {
        printf ( "OK: block_count = 1\n" );
    }

    if ( loaded->block_count >= 1 && loaded->blocks[0].id == TMZ_BLOCK_ID_MZ_BASIC_DATA ) {
        printf ( "OK: block ID = 0x45 (MZ BASIC Data)\n" );
    } else if ( loaded->block_count >= 1 ) {
        printf ( "FAIL: block ID = 0x%02X, expected 0x45\n", loaded->blocks[0].id );
        failures++;
    }

    tzx_free ( loaded );
    printf ( "\n" );
    return failures;
}


/**
 * @brief Hlavni funkce - spusti vsechny BSD testy.
 *
 * @param argc Pocet argumentu.
 * @param argv Pole argumentu. argv[1] = cesta k WAV (volitelne).
 * @return 0 pri uspechu (vsechny testy PASS), 1 pri selhani.
 */
int main ( int argc, char *argv[] ) {
    if ( argc < 2 && tstdata_skip ( "test_bsd_wav" ) ) return 0;   /* argv[1] dává vstup místo tstdata/ */
    memory_driver_init ();

    if ( argc > 1 ) g_wav_path = argv[1];

    int total_failures = 0;

    /* Test 1: dekódování nahrávky z emulátoru */
    st_WAV_ANALYZER_RESULT orig_result;
    memset ( &orig_result, 0, sizeof ( orig_result ) ); /* při selhání dekódování se čte file_count */
    int t1_failures = test_decode_real_wav ( &orig_result );
    total_failures += t1_failures;

    if ( t1_failures > 0 || orig_result.file_count < 1 || !orig_result.files[0].mzf ) {
        printf ( "\n=== BSD Test: FAIL (cannot proceed without decoded MZF) ===\n" );
        if ( orig_result.file_count > 0 ) wav_analyzer_result_destroy ( &orig_result );
        return 1;
    }

    const st_MZF *orig_mzf = orig_result.files[0].mzf;

    /* Test 2: WAV round-trip */
    total_failures += test_wav_roundtrip ( orig_mzf );

    /* Test 3: TMZ blok 0x45 round-trip */
    total_failures += test_tmz_block_roundtrip ( orig_mzf );

    /* Test 4: TMZ soubor round-trip */
    total_failures += test_tmz_file_roundtrip ( orig_mzf );

    /* Test 5 a 6: poškozený kus, ftype 03h */
    test_corrupt_chunk ();
    test_ftype03 ();
    total_failures += g_checks_failed;

    /* celkovy vysledek */
    printf ( "=== BSD Test: %s (%d failures) ===\n",
             total_failures == 0 ? "PASS" : "FAIL", total_failures );

    wav_analyzer_result_destroy ( &orig_result );
    return total_failures > 0 ? 1 : 0;
}
