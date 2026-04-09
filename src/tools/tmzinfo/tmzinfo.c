/**
 * @file   tmzinfo.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 1.3.0
 * @brief  Utility pro zobrazeni obsahu TMZ/TZX souboru.
 *
 * Nacte TMZ nebo TZX soubor a vypise informace o hlavicce,
 * vsech blocich, MZF hlavickach a dalsich metadatech.
 *
 * @par Pouziti:
 * @code
 *   tmzinfo [volby] <soubor.tmz|soubor.tzx>
 * @endcode
 *
 * @par Volby:
 * - --name-encoding <enc>  : kodovani nazvu: ascii, utf8-eu, utf8-jp (vychozi: ascii)
 * - --version              : zobrazit verzi programu
 * - --lib-versions         : zobrazit verze knihoven
 *
 * @par Licence:
 * GNU General Public License v3 (GPLv3)
 *
 * Copyright (C) 2017-2026 Michal Hucik <hucik@ordoz.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tzx/tzx.h"
#include "libs/mzf/mzf.h"
#include "libs/mzf/mzf_tools.h"
#include "libs/cmtspeed/cmtspeed.h"

/** @brief Verze programu tmzinfo (z @version v hlavicce souboru). */
#define TMZINFO_VERSION  "1.3.0"


/** @brief Kodovani nazvu souboru pro zobrazeni (file-level, nastaveno z --name-encoding). */
static en_MZF_NAME_ENCODING name_encoding = MZF_NAME_ASCII;


/**
 * @brief Vrati textovy nazev ciloveho stroje.
 * @param machine Hodnota en_TMZ_MACHINE.
 * @return Staticky retezec s nazvem.
 */
static const char* machine_name ( uint8_t machine ) {
    switch ( machine ) {
        case 0: return "Generic";
        case 1: return "MZ-700";
        case 2: return "MZ-800";
        case 3: return "MZ-1500";
        case 4: return "MZ-80B";
        default: return "Unknown";
    }
}


/**
 * @brief Vrati textovy nazev pulsni sady.
 * @param pulseset Hodnota en_TMZ_PULSESET.
 * @return Staticky retezec s nazvem.
 */
static const char* pulseset_name ( uint8_t pulseset ) {
    switch ( pulseset ) {
        case 0: return "MZ-700/80K/80A";
        case 1: return "MZ-800/1500";
        case 2: return "MZ-80B";
        default: return "Unknown";
    }
}


/**
 * @brief Vrati textovy nazev formatu zaznamu.
 * @param format Hodnota en_TMZ_FORMAT.
 * @return Staticky retezec s nazvem.
 */
static const char* format_name ( uint8_t format ) {
    switch ( format ) {
        case 0: return "NORMAL";
        case 1: return "TURBO";
        case 2: return "FASTIPL";
        case 3: return "SINCLAIR";
        case 4: return "FSK";
        case 5: return "SLOW";
        case 6: return "DIRECT";
        case 7: return "CPM-TAPE";
        default: return "Unknown";
    }
}


/**
 * @brief Vypise rychlost podle formatu bloku 0x41.
 *
 * Interpretace pole speed zavisi na formatu:
 * - FM formaty (NORMAL, TURBO, FASTIPL, SINCLAIR): en_CMTSPEED
 * - FSK: en_MZCMT_FSK_SPEED (0-6)
 * - SLOW: en_MZCMT_SLOW_SPEED (0-4)
 * - DIRECT: ignorovano
 * - CPM_TAPE: en_MZCMT_CPMTAPE_SPEED (0-2)
 *
 * @param format Hodnota en_TMZ_FORMAT.
 * @param speed Hodnota pole speed.
 */
static void print_speed ( uint8_t format, uint8_t speed ) {
    switch ( format ) {
        case 0: /* NORMAL */
        case 1: /* TURBO */
        case 3: /* SINCLAIR */
        {
            char speedtxt[64];
            cmtspeed_get_ratiospeedtxt ( speedtxt, sizeof ( speedtxt ),
                                         ( en_CMTSPEED ) speed, 1200 );
            printf ( "      Speed   : %s\n", speedtxt );
            break;
        }
        case 2: /* FASTIPL */
        {
            /* Intercopy uvadi rychlost v Bd, ne v pomerech */
            char speedtxt[64];
            cmtspeed_get_speedtxt ( speedtxt, sizeof ( speedtxt ),
                                    ( en_CMTSPEED ) speed, 1200 );
            printf ( "      Speed   : %s\n", speedtxt );
            break;
        }
        case 4: /* FSK */
            printf ( "      Speed   : FSK level %u (0=slowest, 6=fastest)\n", speed );
            break;
        case 5: /* SLOW */
            printf ( "      Speed   : SLOW level %u (0=slowest, 4=fastest)\n", speed );
            break;
        case 6: /* DIRECT */
            printf ( "      Speed   : (N/A - sample rate dependent)\n" );
            break;
        case 7: /* CPM_TAPE */
        {
            static const uint16_t cpmtape_bd[] = { 1200, 2400, 3200 };
            if ( speed < 3 ) {
                printf ( "      Speed   : %u Bd\n", cpmtape_bd[speed] );
            } else {
                printf ( "      Speed   : CPM-TAPE level %u\n", speed );
            }
            break;
        }
        default:
            printf ( "      Speed   : %u\n", speed );
            break;
    }
}


/**
 * @brief Vrati textovy nazev typu MZF souboru.
 * @param ftype Hodnota MZF ftype.
 * @return Staticky retezec s nazvem.
 */
static const char* mzf_ftype_name ( uint8_t ftype ) {
    switch ( ftype ) {
        case 0x01: return "OBJ (machine code)";
        case 0x02: return "BTX (BASIC text)";
        case 0x03: return "BSD (BASIC data)";
        case 0x04: return "BRD (BASIC read-after-run)";
        case 0x05: return "RB (read and branch)";
        default: return "Unknown";
    }
}


/**
 * @brief Vypise MZF hlavicku.
 * @param hdr Ukazatel na MZF hlavicku.
 * @param indent Odsazeni (pocet mezer).
 */
static void print_mzf_header ( const st_MZF_HEADER *hdr, int indent ) {
    char fname[MZF_FNAME_UTF8_BUF_SIZE];
    mzf_tools_get_fname_ex ( hdr, fname, sizeof ( fname ), name_encoding );

    printf ( "%*sFilename : \"%s\"\n", indent, "", fname );
    printf ( "%*sType     : 0x%02X (%s)\n", indent, "", hdr->ftype, mzf_ftype_name ( hdr->ftype ) );
    printf ( "%*sSize     : %u bytes (0x%04X)\n", indent, "", hdr->fsize, hdr->fsize );
    printf ( "%*sLoad addr: 0x%04X\n", indent, "", hdr->fstrt );
    printf ( "%*sExec addr: 0x%04X\n", indent, "", hdr->fexec );
}


/**
 * @brief Vypise detail bloku 0x40 (MZ Standard Data).
 * @param block TMZ blok.
 */
static void print_block_mz_standard ( const st_TZX_BLOCK *block ) {
    /* kopie dat kvuli endianite */
    uint8_t *data_copy = malloc ( block->length );
    if ( !data_copy ) return;
    memcpy ( data_copy, block->data, block->length );

    st_TZX_BLOCK copy = *block;
    copy.data = data_copy;

    en_TZX_ERROR err;
    uint8_t *body;
    st_TMZ_MZ_STANDARD_DATA *d = tmz_block_parse_mz_standard ( &copy, &body, &err );
    if ( d ) {
        printf ( "      Machine : %s\n", machine_name ( d->machine ) );
        printf ( "      Pulseset: %s\n", pulseset_name ( d->pulseset ) );
        printf ( "      Pause   : %u ms\n", d->pause_ms );
        printf ( "      Body    : %u bytes\n", d->body_size );
        print_mzf_header ( &d->mzf_header, 6 );
    }
    free ( data_copy );
}


/**
 * @brief Vypise detail bloku 0x41 (MZ Turbo Data).
 * @param block TMZ blok.
 */
static void print_block_mz_turbo ( const st_TZX_BLOCK *block ) {
    uint8_t *data_copy = malloc ( block->length );
    if ( !data_copy ) return;
    memcpy ( data_copy, block->data, block->length );

    st_TZX_BLOCK copy = *block;
    copy.data = data_copy;

    en_TZX_ERROR err;
    uint8_t *body;
    st_TMZ_MZ_TURBO_DATA *d = tmz_block_parse_mz_turbo ( &copy, &body, &err );
    if ( d ) {
        printf ( "      Machine : %s\n", machine_name ( d->machine ) );
        int has_custom_pulses = ( d->long_high || d->long_low || d->short_high || d->short_low );
        if ( has_custom_pulses ) {
            /* custom pulse rezim - pulseset a speed se nepoužívají */
            printf ( "      Pulseset: custom\n" );
            printf ( "        Long  : %.1f/%.1f us\n",
                     d->long_high / 10.0, d->long_low / 10.0 );
            printf ( "        Short : %.1f/%.1f us\n",
                     d->short_high / 10.0, d->short_low / 10.0 );
            uint32_t sum = d->long_high + d->long_low + d->short_high + d->short_low;
            if ( sum > 0 ) {
                double avg_bit_us = ( sum / 10.0 ) / 2.0;
                printf ( "        ~%.0f Bd\n", 1000000.0 / avg_bit_us );
            }
        } else {
            printf ( "      Pulseset: %s\n", pulseset_name ( d->pulseset ) );
        }
        printf ( "      Format  : %s\n", format_name ( d->format ) );
        if ( !has_custom_pulses ) {
            print_speed ( d->format, d->speed );
        }
        printf ( "      LGAP    : %u\n", d->lgap_length );
        printf ( "      SGAP    : %u\n", d->sgap_length );
        printf ( "      Pause   : %u ms\n", d->pause_ms );
        printf ( "      Flags   : 0x%02X\n", d->flags );
        printf ( "      Body    : %u bytes\n", d->body_size );
        print_mzf_header ( &d->mzf_header, 6 );
    }
    free ( data_copy );
}


/**
 * @brief Vypise detail bloku 0x43 (MZ Machine Info).
 * @param block TMZ blok.
 */
static void print_block_mz_machine_info ( const st_TZX_BLOCK *block ) {
    uint8_t *data_copy = malloc ( block->length );
    if ( !data_copy ) return;
    memcpy ( data_copy, block->data, block->length );

    st_TZX_BLOCK copy = *block;
    copy.data = data_copy;

    en_TZX_ERROR err;
    st_TMZ_MZ_MACHINE_INFO *d = tmz_block_parse_mz_machine_info ( &copy, &err );
    if ( d ) {
        printf ( "      Machine : %s\n", machine_name ( d->machine ) );
        printf ( "      CPU clk : %u Hz\n", d->cpu_clock );
        printf ( "      ROM ver : %u\n", d->rom_version );
    }
    free ( data_copy );
}


/**
 * @brief Vypise detail bloku 0x45 (MZ BASIC Data).
 * @param block TMZ blok.
 */
static void print_block_mz_basic_data ( const st_TZX_BLOCK *block ) {
    uint8_t *data_copy = malloc ( block->length );
    if ( !data_copy ) return;
    memcpy ( data_copy, block->data, block->length );

    st_TZX_BLOCK copy = *block;
    copy.data = data_copy;

    en_TZX_ERROR err;
    uint8_t *chunks;
    st_TMZ_MZ_BASIC_DATA *d = tmz_block_parse_mz_basic_data ( &copy, &chunks, &err );
    if ( d ) {
        printf ( "      Machine : %s\n", machine_name ( d->machine ) );
        printf ( "      Pulseset: %s\n", pulseset_name ( d->pulseset ) );
        printf ( "      Pause   : %u ms\n", d->pause_ms );
        printf ( "      Chunks  : %u (%u bytes total)\n", d->chunk_count,
                 d->chunk_count * TMZ_BASIC_CHUNK_DATA_SIZE );
        print_mzf_header ( &d->mzf_header, 6 );
    }
    free ( data_copy );
}


/**
 * @brief Vypise detail TZX bloku 0x10 (Standard Speed Data).
 * @param block TMZ blok.
 */
static void print_block_tzx_standard ( const st_TZX_BLOCK *block ) {
    if ( block->length < 4 ) return;
    uint16_t pause_ms = block->data[0] | ( block->data[1] << 8 );
    uint16_t data_len = block->data[2] | ( block->data[3] << 8 );
    /* ZX standard: zero=855 T, one=1710 T -> Bd = 3500000/(855+1710) */
    printf ( "      Speed   : %u Bd\n", 3500000 / ( 855 + 1710 ) );
    printf ( "      Pause   : %u ms\n", pause_ms );
    printf ( "      Data    : %u bytes\n", data_len );
    if ( data_len > 0 ) {
        uint8_t flag = block->data[4];
        printf ( "      Flag    : 0x%02X (%s)\n", flag,
                 flag < 128 ? "header" : "data" );

        /* ZX Spectrum TAP header: flag=0x00, type + 10B filename + params */
        if ( flag == 0x00 && data_len >= 18 ) {
            uint8_t type = block->data[5];
            const char *type_names[] = { "Program", "Number array", "Character array", "Bytes" };
            const char *type_name = ( type <= 3 ) ? type_names[type] : "Unknown";

            /* extrakce 10B jmena (ASCII, padding mezerami) */
            char fname[11];
            memcpy ( fname, &block->data[6], 10 );
            fname[10] = '\0';
            /* oriznuti mezer zprava */
            for ( int j = 9; j >= 0 && fname[j] == ' '; j-- ) fname[j] = '\0';

            uint16_t data_length = block->data[16] | ( block->data[17] << 8 );

            printf ( "      Type    : %u (%s)\n", type, type_name );
            printf ( "      Filename: \"%s\"\n", fname );
            printf ( "      Length  : %u bytes\n", data_length );

            if ( type == 0 ) {
                /* Program: param1 = autostart line, param2 = prog length */
                uint16_t autostart = block->data[18] | ( block->data[19] << 8 );
                if ( autostart < 10000 ) {
                    printf ( "      Autostart: LINE %u\n", autostart );
                }
            } else if ( type == 3 ) {
                /* Bytes: param1 = start address */
                uint16_t start_addr = block->data[18] | ( block->data[19] << 8 );
                printf ( "      Start   : 0x%04X\n", start_addr );
            }
        }
    }
}


/**
 * @brief Vypise detail TZX bloku 0x11 (Turbo Speed Data).
 * @param block TMZ blok.
 */
static void print_block_tzx_turbo ( const st_TZX_BLOCK *block ) {
    if ( block->length < 0x12 ) return;
    uint16_t pilot = block->data[0] | ( block->data[1] << 8 );
    uint16_t sync1 = block->data[2] | ( block->data[3] << 8 );
    uint16_t sync2 = block->data[4] | ( block->data[5] << 8 );
    uint16_t zero  = block->data[6] | ( block->data[7] << 8 );
    uint16_t one   = block->data[8] | ( block->data[9] << 8 );
    uint16_t pilot_count = block->data[10] | ( block->data[11] << 8 );
    uint8_t  used_bits = block->data[12];
    uint16_t pause_ms = block->data[13] | ( block->data[14] << 8 );
    uint32_t data_len = block->data[15] | ( block->data[16] << 8 ) | ( block->data[17] << 16 );

    /* Bd = 3500000 / (zero + one) - prumerna rychlost pri 50% 0/1 */
    uint32_t bd = ( zero + one > 0 ) ? 3500000 / ( zero + one ) : 0;
    printf ( "      Speed   : %u Bd\n", bd );
    printf ( "      Pilot   : %u T x %u pulses\n", pilot, pilot_count );
    printf ( "      Sync    : %u / %u T\n", sync1, sync2 );
    printf ( "      Bits    : zero=%u T, one=%u T\n", zero, one );
    printf ( "      UsedBits: %u\n", used_bits );
    printf ( "      Pause   : %u ms\n", pause_ms );
    printf ( "      Data    : %u bytes\n", data_len );
}


/**
 * @brief Vypise detail TZX bloku 0x12 (Pure Tone).
 * @param block TMZ blok.
 */
static void print_block_tzx_pure_tone ( const st_TZX_BLOCK *block ) {
    if ( block->length < 4 ) return;
    uint16_t pulse_len = block->data[0] | ( block->data[1] << 8 );
    uint16_t count = block->data[2] | ( block->data[3] << 8 );
    printf ( "      Pulse   : %u T-states\n", pulse_len );
    printf ( "      Count   : %u pulses\n", count );
}


/**
 * @brief Vypise detail TZX bloku 0x13 (Pulse Sequence).
 * @param block TMZ blok.
 */
static void print_block_tzx_pulse_sequence ( const st_TZX_BLOCK *block ) {
    if ( block->length < 1 ) return;
    uint8_t count = block->data[0];
    printf ( "      Pulses  : %u\n", count );
    for ( uint8_t i = 0; i < count && ( 1 + (uint32_t) i * 2 + 2 ) <= block->length; i++ ) {
        uint16_t len = block->data[1 + i * 2] | ( block->data[2 + i * 2] << 8 );
        printf ( "        [%u]: %u T-states\n", i, len );
    }
}


/**
 * @brief Vypise detail TZX bloku 0x14 (Pure Data).
 *
 * Data bloku zacinaji 4B DWORD delkou (default parser),
 * vlastni obsah je az od offsetu 4.
 *
 * @param block TMZ blok.
 */
static void print_block_tzx_pure_data ( const st_TZX_BLOCK *block ) {
    if ( block->length < 0x0A ) return;
    const uint8_t *d = block->data;
    uint16_t zero = d[0] | ( d[1] << 8 );
    uint16_t one  = d[2] | ( d[3] << 8 );
    uint8_t  used_bits = d[4];
    uint16_t pause_ms = d[5] | ( d[6] << 8 );
    uint32_t data_len = d[7] | ( d[8] << 8 ) | ( d[9] << 16 );
    printf ( "      Bits    : zero=%u T, one=%u T\n", zero, one );
    printf ( "      UsedBits: %u\n", used_bits );
    printf ( "      Pause   : %u ms\n", pause_ms );
    printf ( "      Data    : %u bytes\n", data_len );
}


/**
 * @brief Vypise detail TZX bloku 0x15 (Direct Recording).
 *
 * Data bloku zacinaji primo hlavickovymi poli (2B tstates + 2B pause +
 * 1B used_bits + 3B data_len), bez DWORD delkoveho prefixu.
 *
 * @param block TMZ blok.
 */
static void print_block_tzx_direct_recording ( const st_TZX_BLOCK *block ) {
    if ( block->length < 0x08 ) return;
    const uint8_t *d = block->data;
    uint16_t tstates = d[0] | ( d[1] << 8 );
    uint16_t pause_ms = d[2] | ( d[3] << 8 );
    uint8_t  used_bits = d[4];
    uint32_t data_len = d[5] | ( d[6] << 8 ) | ( d[7] << 16 );
    printf ( "      T/sample: %u T-states", tstates );
    if ( tstates > 0 ) {
        printf ( " (~%u Hz)", 3500000 / tstates );
    }
    printf ( "\n" );
    printf ( "      UsedBits: %u\n", used_bits );
    printf ( "      Pause   : %u ms\n", pause_ms );
    printf ( "      Data    : %u bytes\n", data_len );
}


/**
 * @brief Vypise detail TZX bloku 0x18 (CSW Recording).
 *
 * Data bloku zacinaji 4B DWORD delkou (default parser),
 * vlastni obsah je az od offsetu 4.
 *
 * @param block TMZ blok.
 */
static void print_block_tzx_csw ( const st_TZX_BLOCK *block ) {
    if ( block->length < 4 + 0x0A ) return;
    const uint8_t *d = block->data + 4;
    uint16_t pause_ms = d[0] | ( d[1] << 8 );
    uint32_t sample_rate = d[2] | ( d[3] << 8 ) | ( d[4] << 16 );
    uint8_t  compression = d[5];
    uint32_t stored_pulses = d[6] | ( d[7] << 8 ) | ( d[8] << 16 ) | ( (uint32_t) d[9] << 24 );
    printf ( "      Pause   : %u ms\n", pause_ms );
    printf ( "      SampRate: %u Hz\n", sample_rate );
    printf ( "      Compress: %s\n", compression == 1 ? "RLE" : compression == 2 ? "Z-RLE" : "Unknown" );
    printf ( "      Pulses  : %u\n", stored_pulses );
}


/**
 * @brief Vypise detail TZX bloku 0x21 (Group Start).
 * @param block TMZ blok.
 */
static void print_block_tzx_group_start ( const st_TZX_BLOCK *block ) {
    if ( block->length < 1 ) return;
    uint8_t len = block->data[0];
    if ( len > 0 && 1 + (uint32_t) len <= block->length ) {
        printf ( "      Name    : \"%.*s\"\n", len, (const char*) ( block->data + 1 ) );
    }
}


/**
 * @brief Vypise detail TZX bloku 0x23 (Jump to Block).
 * @param block TMZ blok.
 */
static void print_block_tzx_jump ( const st_TZX_BLOCK *block ) {
    if ( block->length < 2 ) return;
    int16_t offset = (int16_t) ( block->data[0] | ( block->data[1] << 8 ) );
    printf ( "      Offset  : %+d\n", offset );
}


/**
 * @brief Vypise detail TZX bloku 0x24 (Loop Start).
 * @param block TMZ blok.
 */
static void print_block_tzx_loop_start ( const st_TZX_BLOCK *block ) {
    if ( block->length < 2 ) return;
    uint16_t count = block->data[0] | ( block->data[1] << 8 );
    printf ( "      Repeat  : %u\n", count );
}


/**
 * @brief Vypise detail TZX bloku 0x26 (Call Sequence).
 * @param block TMZ blok.
 */
static void print_block_tzx_call_sequence ( const st_TZX_BLOCK *block ) {
    if ( block->length < 2 ) return;
    uint16_t count = block->data[0] | ( block->data[1] << 8 );
    printf ( "      Calls   : %u\n", count );
    for ( uint16_t i = 0; i < count && ( 2 + (uint32_t) i * 2 + 2 ) <= block->length; i++ ) {
        int16_t offset = (int16_t) ( block->data[2 + i * 2] | ( block->data[3 + i * 2] << 8 ) );
        printf ( "        [%u]: %+d\n", i, offset );
    }
}


/**
 * @brief Vypise detail TZX bloku 0x28 (Select Block).
 *
 * Data bloku zacinaji 2B WORD delkou, za ni nasleduje
 * 1B pocet polozek a pole SELECT zaznamu.
 *
 * @param block TMZ blok.
 */
static void print_block_tzx_select ( const st_TZX_BLOCK *block ) {
    if ( block->length < 3 ) return;
    uint8_t count = block->data[2];
    printf ( "      Items   : %u\n", count );
    uint32_t pos = 3;
    for ( uint8_t i = 0; i < count && pos + 3 <= block->length; i++ ) {
        int16_t offset = (int16_t) ( block->data[pos] | ( block->data[pos + 1] << 8 ) );
        uint8_t text_len = block->data[pos + 2];
        if ( pos + 3 + text_len <= block->length ) {
            printf ( "        [%u]: offset=%+d \"%.*s\"\n", i, offset,
                     text_len, (const char*) ( block->data + pos + 3 ) );
        }
        pos += 3 + text_len;
    }
}


/**
 * @brief Vypise detail TZX bloku 0x2B (Set Signal Level).
 *
 * Data bloku zacinaji 4B DWORD delkou (default parser),
 * uroven signalu je na offsetu 4.
 *
 * @param block TMZ blok.
 */
static void print_block_tzx_set_signal_level ( const st_TZX_BLOCK *block ) {
    if ( block->length < 5 ) return;
    uint8_t level = block->data[4];
    printf ( "      Level   : %s (%u)\n", level ? "HIGH" : "LOW", level );
}


/**
 * @brief Vypise detail TZX bloku 0x30 (Text Description).
 * @param block TMZ blok.
 */
static void print_block_tzx_text_description ( const st_TZX_BLOCK *block ) {
    if ( block->length < 1 ) return;
    uint8_t len = block->data[0];
    if ( 1 + (uint32_t) len <= block->length ) {
        printf ( "      Text    : \"%.*s\"\n", len, (const char*) ( block->data + 1 ) );
    }
}


/**
 * @brief Vypise detail TZX bloku 0x31 (Message Block).
 * @param block TMZ blok.
 */
static void print_block_tzx_message ( const st_TZX_BLOCK *block ) {
    if ( block->length < 2 ) return;
    uint8_t time_s = block->data[0];
    uint8_t len = block->data[1];
    printf ( "      Time    : %u s\n", time_s );
    if ( 2 + (uint32_t) len <= block->length ) {
        printf ( "      Message : \"%.*s\"\n", len, (const char*) ( block->data + 2 ) );
    }
}


/**
 * @brief Vrati textovy nazev typu Archive Info zaznamu.
 * @param id ID textu dle TZX specifikace.
 * @return Staticky retezec s nazvem.
 */
static const char* archive_text_id_name ( uint8_t id ) {
    switch ( id ) {
        case 0x00: return "Title";
        case 0x01: return "Publisher";
        case 0x02: return "Author(s)";
        case 0x03: return "Year";
        case 0x04: return "Language";
        case 0x05: return "Type";
        case 0x06: return "Price";
        case 0x07: return "Protection";
        case 0x08: return "Origin";
        case 0xFF: return "Comment";
        default:   return "Unknown";
    }
}


/**
 * @brief Vypise detail TZX bloku 0x32 (Archive Info).
 *
 * Data bloku zacinaji 2B WORD delkou, za ni nasleduje
 * 1B pocet textovych zaznamu a pole TEXT zaznamu
 * (kazdy: 1B typ, 1B delka, text).
 *
 * @param block TMZ blok.
 */
static void print_block_tzx_archive_info ( const st_TZX_BLOCK *block ) {
    if ( block->length < 3 ) return;
    uint8_t count = block->data[2];
    printf ( "      Entries : %u\n", count );
    uint32_t pos = 3;
    for ( uint8_t i = 0; i < count && pos + 2 <= block->length; i++ ) {
        uint8_t type_id = block->data[pos];
        uint8_t text_len = block->data[pos + 1];
        if ( pos + 2 + text_len <= block->length ) {
            printf ( "        %-12s: \"%.*s\"\n", archive_text_id_name ( type_id ),
                     text_len, (const char*) ( block->data + pos + 2 ) );
        }
        pos += 2 + text_len;
    }
}


/**
 * @brief Vrati textovy nazev kategorie hardware.
 * @param type Typ hardware dle TZX specifikace.
 * @return Staticky retezec s nazvem.
 */
static const char* hw_type_name ( uint8_t type ) {
    switch ( type ) {
        case 0x00: return "Computer";
        case 0x01: return "Ext. Storage";
        case 0x02: return "ROM/RAM";
        case 0x03: return "Sound";
        case 0x04: return "Joystick";
        case 0x05: return "Mouse";
        case 0x06: return "Controller";
        case 0x07: return "Serial Port";
        case 0x08: return "Parallel Port";
        case 0x09: return "Printer";
        case 0x0A: return "Modem";
        case 0x0B: return "Digitizer";
        case 0x0C: return "Network";
        case 0x0D: return "Keyboard";
        case 0x0E: return "AD/DA";
        case 0x0F: return "EPROM Prog.";
        case 0x10: return "Graphics";
        default:   return "Unknown";
    }
}


/**
 * @brief Vrati textovy popis HW info bajtu.
 * @param info HW info hodnota dle TZX specifikace.
 * @return Staticky retezec s nazvem.
 */
static const char* hw_info_name ( uint8_t info ) {
    switch ( info ) {
        case 0x00: return "runs on";
        case 0x01: return "uses";
        case 0x02: return "runs w/o";
        case 0x03: return "doesn't run";
        default:   return "unknown";
    }
}


/**
 * @brief Vypise detail TZX bloku 0x33 (Hardware Type).
 * @param block TMZ blok.
 */
static void print_block_tzx_hardware_type ( const st_TZX_BLOCK *block ) {
    if ( block->length < 1 ) return;
    uint8_t count = block->data[0];
    printf ( "      Entries : %u\n", count );
    for ( uint8_t i = 0; i < count && ( 1 + (uint32_t) i * 3 + 3 ) <= block->length; i++ ) {
        uint8_t type = block->data[1 + i * 3];
        uint8_t id   = block->data[2 + i * 3];
        uint8_t info = block->data[3 + i * 3];
        printf ( "        %s 0x%02X (%s)\n", hw_type_name ( type ), id, hw_info_name ( info ) );
    }
}


/**
 * @brief Vypise detail TZX bloku 0x35 (Custom Info).
 *
 * Data bloku zacinaji 10B ASCII identifikatorem,
 * nasleduje 4B DWORD delka vlastnich dat.
 *
 * @param block TMZ blok.
 */
static void print_block_tzx_custom_info ( const st_TZX_BLOCK *block ) {
    if ( block->length < 14 ) return;
    printf ( "      ID      : \"%.*s\"\n", 10, (const char*) block->data );
    uint32_t len = block->data[10] | ( block->data[11] << 8 ) |
                   ( block->data[12] << 16 ) | ( (uint32_t) block->data[13] << 24 );
    printf ( "      Data    : %u bytes\n", len );
}


/**
 * @brief Vypise detail bloku 0x42 (MZ Extra Body).
 * @param block TMZ blok.
 */
static void print_block_mz_extra_body ( const st_TZX_BLOCK *block ) {
    uint8_t *data_copy = malloc ( block->length );
    if ( !data_copy ) return;
    memcpy ( data_copy, block->data, block->length );

    st_TZX_BLOCK copy = *block;
    copy.data = data_copy;

    en_TZX_ERROR err;
    uint8_t *body;
    st_TMZ_MZ_EXTRA_BODY *d = tmz_block_parse_mz_extra_body ( &copy, &body, &err );
    if ( d ) {
        printf ( "      Format  : %s\n", format_name ( d->format ) );
        print_speed ( d->format, d->speed );
        printf ( "      Pause   : %u ms\n", d->pause_ms );
        printf ( "      Body    : %u bytes\n", d->body_size );
    }
    free ( data_copy );
}


/**
 * @brief Vrati textovy nazev typu loaderu.
 * @param type Hodnota en_TMZ_LOADER_TYPE.
 * @return Staticky retezec s nazvem.
 */
static const char* loader_type_name ( uint8_t type ) {
    switch ( type ) {
        case 0: return "TURBO 1.0";
        case 1: return "TURBO 1.2x";
        case 2: return "FASTIPL v2";
        case 3: return "FASTIPL v7";
        case 4: return "FSK";
        case 5: return "SLOW";
        case 6: return "DIRECT";
        default: return "Unknown";
    }
}


/**
 * @brief Vypise detail bloku 0x44 (MZ Loader).
 * @param block TMZ blok.
 */
static void print_block_mz_loader ( const st_TZX_BLOCK *block ) {
    uint8_t *data_copy = malloc ( block->length );
    if ( !data_copy ) return;
    memcpy ( data_copy, block->data, block->length );

    st_TZX_BLOCK copy = *block;
    copy.data = data_copy;

    en_TZX_ERROR err;
    uint8_t *loader_data;
    st_TMZ_MZ_LOADER *d = tmz_block_parse_mz_loader ( &copy, &loader_data, &err );
    if ( d ) {
        printf ( "      Type    : %s\n", loader_type_name ( d->loader_type ) );
        /* loader_type mapuje na format: TURBO_1_0/1_2x->TURBO, FASTIPL->FASTIPL, FSK/SLOW/DIRECT */
        {
            static const uint8_t loader_to_format[] = {
                1, 1, 2, 2, 4, 5, 6  /* TURBO, TURBO, FASTIPL, FASTIPL, FSK, SLOW, DIRECT */
            };
            uint8_t fmt = d->loader_type < 7 ? loader_to_format[d->loader_type] : 0;
            print_speed ( fmt, d->speed );
        }
        printf ( "      Loader  : %u bytes\n", d->loader_size );
        print_mzf_header ( &d->mzf_header, 6 );
    }
    free ( data_copy );
}


/**
 * @brief Vypise verze vsech pouzitych knihoven na stdout.
 */
static void print_lib_versions ( void ) {
    printf ( "Library versions:\n" );
    printf ( "  tmz            %s (TMZ format v%s)\n", tmz_version (), tmz_format_version () );
    printf ( "  tzx            %s (TZX format v%s)\n", tzx_version (), tzx_format_version () );
    printf ( "  mzf            %s\n", mzf_version () );
    printf ( "  cmtspeed       %s\n", cmtspeed_version () );
}


/**
 * @brief Hlavni funkce - nacte soubor a vypise jeho obsah.
 */
int main ( int argc, char *argv[] ) {

    if ( argc < 2 ) {
        fprintf ( stderr, "Usage: tmzinfo [options] <file.tmz|file.tzx>\n\n" );
        fprintf ( stderr, "Options:\n" );
        fprintf ( stderr, "  --name-encoding <enc> Filename encoding: ascii, utf8-eu, utf8-jp (default: ascii)\n" );
        fprintf ( stderr, "  --version             Show program version\n" );
        fprintf ( stderr, "  --lib-versions        Show library versions\n" );
        return EXIT_FAILURE;
    }

    /* parsovani argumentu */
    const char *filename = NULL;

    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp ( argv[i], "--version" ) == 0 ) {
            printf ( "tmzinfo %s\n", TMZINFO_VERSION );
            return EXIT_SUCCESS;
        } else if ( strcmp ( argv[i], "--lib-versions" ) == 0 ) {
            print_lib_versions ();
            return EXIT_SUCCESS;
        } else if ( strcmp ( argv[i], "--name-encoding" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf ( stderr, "Error: --name-encoding requires a value\n" );
                return EXIT_FAILURE;
            }
            if ( strcmp ( argv[i], "ascii" ) == 0 ) {
                name_encoding = MZF_NAME_ASCII;
            } else if ( strcmp ( argv[i], "utf8-eu" ) == 0 ) {
                name_encoding = MZF_NAME_UTF8_EU;
            } else if ( strcmp ( argv[i], "utf8-jp" ) == 0 ) {
                name_encoding = MZF_NAME_UTF8_JP;
            } else {
                fprintf ( stderr, "Error: unknown name encoding '%s' (use: ascii, utf8-eu, utf8-jp)\n", argv[i] );
                return EXIT_FAILURE;
            }
        } else if ( argv[i][0] == '-' ) {
            fprintf ( stderr, "Error: unknown option '%s'\n", argv[i] );
            return EXIT_FAILURE;
        } else {
            if ( filename ) {
                fprintf ( stderr, "Error: too many arguments\n" );
                return EXIT_FAILURE;
            }
            filename = argv[i];
        }
    }

    if ( !filename ) {
        fprintf ( stderr, "Error: no input file specified\n" );
        return EXIT_FAILURE;
    }

    /* otevreni souboru pres generic_driver */
    st_HANDLER handler;
    st_DRIVER driver;

    generic_driver_file_init ( &driver );

    st_HANDLER *h = generic_driver_open_file ( &handler, &driver, (char*) filename, FILE_DRIVER_OPMODE_RO );
    if ( !h ) {
        fprintf ( stderr, "Error: cannot open file '%s'\n", filename );
        return EXIT_FAILURE;
    }

    /* nacteni TMZ/TZX souboru */
    en_TZX_ERROR err;
    st_TZX_FILE *file = tzx_load ( h, &err );

    generic_driver_close ( h );

    if ( !file ) {
        fprintf ( stderr, "Error: %s\n", tzx_error_string ( err ) );
        return EXIT_FAILURE;
    }

    /* vypis hlavicky */
    printf ( "=== %s ===\n\n", filename );
    printf ( "File type  : %s\n", file->is_tmz ? "TMZ (TapeMZ!)" : "TZX (ZXTape!)" );
    printf ( "Version    : %u.%u\n", file->header.ver_major, file->header.ver_minor );
    printf ( "Blocks     : %u\n\n", file->block_count );

    /* vypis bloku */
    for ( uint32_t i = 0; i < file->block_count; i++ ) {
        const st_TZX_BLOCK *block = &file->blocks[i];

        printf ( "  [%3u] ID 0x%02X  %-25s  (%u bytes)%s\n",
                 i, block->id, tmz_block_id_name ( block->id ),
                 block->length,
                 tmz_block_is_mz_extension ( block->id ) ? "  [MZ]" : "" );

        /* detaily specifickych bloku */
        switch ( block->id ) {

            /* TMZ MZ-specificke bloky */
            case TMZ_BLOCK_ID_MZ_STANDARD_DATA:
                print_block_mz_standard ( block );
                break;
            case TMZ_BLOCK_ID_MZ_TURBO_DATA:
                print_block_mz_turbo ( block );
                break;
            case TMZ_BLOCK_ID_MZ_EXTRA_BODY:
                print_block_mz_extra_body ( block );
                break;
            case TMZ_BLOCK_ID_MZ_MACHINE_INFO:
                print_block_mz_machine_info ( block );
                break;
            case TMZ_BLOCK_ID_MZ_LOADER:
                print_block_mz_loader ( block );
                break;
            case TMZ_BLOCK_ID_MZ_BASIC_DATA:
                print_block_mz_basic_data ( block );
                break;

            /* TZX audio bloky */
            case TZX_BLOCK_ID_STANDARD_SPEED:
                print_block_tzx_standard ( block );
                break;
            case TZX_BLOCK_ID_TURBO_SPEED:
                print_block_tzx_turbo ( block );
                break;
            case TZX_BLOCK_ID_PURE_TONE:
                print_block_tzx_pure_tone ( block );
                break;
            case TZX_BLOCK_ID_PULSE_SEQUENCE:
                print_block_tzx_pulse_sequence ( block );
                break;
            case TZX_BLOCK_ID_PURE_DATA:
                print_block_tzx_pure_data ( block );
                break;
            case TZX_BLOCK_ID_DIRECT_RECORDING:
                print_block_tzx_direct_recording ( block );
                break;
            case TZX_BLOCK_ID_CSW_RECORDING:
                print_block_tzx_csw ( block );
                break;
            case TZX_BLOCK_ID_PAUSE:
                if ( block->length >= 2 ) {
                    uint16_t p = block->data[0] | ( block->data[1] << 8 );
                    printf ( "      Pause   : %u ms%s\n", p, p == 0 ? " (STOP)" : "" );
                }
                break;

            /* TZX ridici bloky */
            case TZX_BLOCK_ID_GROUP_START:
                print_block_tzx_group_start ( block );
                break;
            case TZX_BLOCK_ID_JUMP:
                print_block_tzx_jump ( block );
                break;
            case TZX_BLOCK_ID_LOOP_START:
                print_block_tzx_loop_start ( block );
                break;
            case TZX_BLOCK_ID_CALL_SEQUENCE:
                print_block_tzx_call_sequence ( block );
                break;
            case TZX_BLOCK_ID_SELECT_BLOCK:
                print_block_tzx_select ( block );
                break;
            case TZX_BLOCK_ID_SET_SIGNAL_LEVEL:
                print_block_tzx_set_signal_level ( block );
                break;

            /* TZX informacni bloky */
            case TZX_BLOCK_ID_TEXT_DESCRIPTION:
                print_block_tzx_text_description ( block );
                break;
            case TZX_BLOCK_ID_MESSAGE:
                print_block_tzx_message ( block );
                break;
            case TZX_BLOCK_ID_ARCHIVE_INFO:
                print_block_tzx_archive_info ( block );
                break;
            case TZX_BLOCK_ID_HARDWARE_TYPE:
                print_block_tzx_hardware_type ( block );
                break;
            case TZX_BLOCK_ID_CUSTOM_INFO:
                print_block_tzx_custom_info ( block );
                break;

            default:
                break;
        }
    }

    printf ( "\n" );
    tzx_free ( file );

    return EXIT_SUCCESS;
}
