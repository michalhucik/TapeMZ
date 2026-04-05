/**
 * @file   mzf_tools.c
 * @author Michal Hucik <hucik@ordoz.com>
 * @version 2.0.0
 * @brief  Implementace pomocných funkcí pro MZF hlavičku.
 *
 * Implementace konverze jmen souborů (Sharp MZ ASCII <-> ASCII),
 * tovární funkce pro vytvoření hlavičky a debug výpisu.
 * Viz mzf_tools.h pro popis API.
 *
 * @par Changelog:
 * - 2026-03-14: Proběhla kompletní revize a refaktorizace. Vytvořeny unit testy.
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "mzf.h"
#include "mzf_tools.h"
#include "libs/sharpmz_ascii/sharpmz_ascii.h"
#include "libs/sharpmz_ascii/sharpmz_utf8.h"


/** @copydoc mzf_tools_set_fname */
void mzf_tools_set_fname ( st_MZF_HEADER *mzfhdr, const char *ascii_filename ) {
    unsigned ascii_fname_length = strlen ( ascii_filename );
    unsigned num_chars = ( ascii_fname_length >= MZF_FILE_NAME_LENGTH ) ? MZF_FILE_NAME_LENGTH : ascii_fname_length;

    /* Konverze ASCII → Sharp MZ ASCII */
    unsigned i;
    for ( i = 0; i < num_chars; i++ ) {
        mzfhdr->fname.name[i] = sharpmz_cnv_to ( ascii_filename[i] );
    }

    /* Vyplnění zbytku jména terminátory */
    for ( ; i < MZF_FILE_NAME_LENGTH; i++ ) {
        mzfhdr->fname.name[i] = MZF_FNAME_TERMINATOR;
    }

    /* Explicitní nastavení terminátoru struktury */
    mzfhdr->fname.terminator = MZF_FNAME_TERMINATOR;
}


/** @copydoc mzf_tools_get_fname_length */
uint8_t mzf_tools_get_fname_length ( const st_MZF_HEADER *mzfhdr ) {
    for ( unsigned i = 0; i < MZF_FILE_NAME_LENGTH; i++ ) {
        if ( mzfhdr->fname.name[i] == MZF_FNAME_TERMINATOR ) {
            return i;
        }
    }
    return MZF_FILE_NAME_LENGTH;
}


/** @copydoc mzf_tools_get_fname */
void mzf_tools_get_fname ( const st_MZF_HEADER *mzfhdr, char *ascii_filename ) {
    const uint8_t *fname = mzfhdr->fname.name;

    for ( unsigned i = 0; i < MZF_FNAME_FULL_LENGTH; i++ ) {
        if ( *fname == MZF_FNAME_TERMINATOR ) break;

        /* Konverze Sharp MZ ASCII → ASCII, přeskočení netisknutelných znaků */
        uint8_t c = sharpmz_cnv_from ( *fname );
        if ( c >= 0x20 ) {
            *ascii_filename++ = c;
        }
        fname++;
    }
    *ascii_filename = 0x00;
}


/** @copydoc mzf_tools_get_fname_ex */
void mzf_tools_get_fname_ex ( const st_MZF_HEADER *mzfhdr, char *filename,
                               size_t buf_size, en_MZF_NAME_ENCODING encoding ) {
    if ( !mzfhdr || !filename || buf_size == 0 ) return;

    if ( encoding == MZF_NAME_ASCII ) {
        /* ASCII režim - delegujeme na původní funkci */
        if ( buf_size >= MZF_FILE_NAME_LENGTH + 1 ) {
            mzf_tools_get_fname ( mzfhdr, filename );
        } else {
            /* menší buffer - konverze s kontrolou velikosti */
            char tmp[MZF_FILE_NAME_LENGTH + 1];
            mzf_tools_get_fname ( mzfhdr, tmp );
            strncpy ( filename, tmp, buf_size - 1 );
            filename[buf_size - 1] = '\0';
        }
        return;
    }

    /* UTF-8 režim (EU nebo JP) */
    sharpmz_charset_t charset = ( encoding == MZF_NAME_UTF8_JP )
                                  ? SHARPMZ_CHARSET_JP : SHARPMZ_CHARSET_EU;

    /* zjistíme délku jména (po terminátor 0x0D) */
    uint8_t src_len = mzf_tools_get_fname_length ( mzfhdr );

    sharpmz_str_to_utf8 ( mzfhdr->fname.name, src_len,
                           filename, buf_size, charset );
}


/** @copydoc mzf_tools_create_mzfhdr */
st_MZF_HEADER* mzf_tools_create_mzfhdr ( uint8_t ftype, uint16_t fsize, uint16_t fstrt, uint16_t fexec, const uint8_t *fname, unsigned fname_length, const uint8_t *cmnt ) {
    st_MZF_HEADER *mzfhdr = (st_MZF_HEADER*) malloc ( sizeof ( st_MZF_HEADER ) );
    if ( !mzfhdr ) return NULL;

    mzfhdr->ftype = ftype;
    mzfhdr->fsize = fsize;
    mzfhdr->fstrt = fstrt;
    mzfhdr->fexec = fexec;

    /* Komentář — vynulovat pokud není zadán */
    if ( !cmnt ) {
        memset ( mzfhdr->cmnt, 0x00, sizeof ( mzfhdr->cmnt ) );
    } else {
        memcpy ( mzfhdr->cmnt, cmnt, sizeof ( mzfhdr->cmnt ) );
    }

    /* Jméno — vyplnit terminátory, pak překopírovat zadané znaky */
    memset ( &mzfhdr->fname, MZF_FNAME_TERMINATOR, sizeof ( mzfhdr->fname ) );
    unsigned length = ( fname_length < MZF_FILE_NAME_LENGTH ) ? fname_length : MZF_FILE_NAME_LENGTH;
    memcpy ( mzfhdr->fname.name, fname, length );

    return mzfhdr;
}


/** @copydoc mzf_tools_dump_header */
void mzf_tools_dump_header ( const st_MZF_HEADER *mzfhdr, FILE *fp ) {
    if ( !mzfhdr || !fp ) return;

    /* Jméno souboru */
    char ascii_name[MZF_FILE_NAME_LENGTH + 1];
    mzf_tools_get_fname ( mzfhdr, ascii_name );

    fprintf ( fp, "MZF hlavička:\n" );
    fprintf ( fp, "  ftype : 0x%02X\n", mzfhdr->ftype );
    fprintf ( fp, "  fname : \"%s\" (délka: %u)\n", ascii_name, mzf_tools_get_fname_length ( mzfhdr ) );
    fprintf ( fp, "  fsize : 0x%04X (%u)\n", mzfhdr->fsize, mzfhdr->fsize );
    fprintf ( fp, "  fstrt : 0x%04X\n", mzfhdr->fstrt );
    fprintf ( fp, "  fexec : 0x%04X\n", mzfhdr->fexec );

    /* Komentář — prvních 16 bajtů hex dump */
    fprintf ( fp, "  cmnt  : " );
    unsigned dump_len = ( MZF_CMNT_LENGTH < 16 ) ? MZF_CMNT_LENGTH : 16;
    for ( unsigned i = 0; i < dump_len; i++ ) {
        fprintf ( fp, "%02X ", mzfhdr->cmnt[i] );
    }
    if ( MZF_CMNT_LENGTH > 16 ) fprintf ( fp, "..." );
    fprintf ( fp, "\n" );
}
