/**
 * @file   tstdata_skip.h
 * @brief  Přeskočení testu bez testovacích dat.
 *
 * Veřejná kopie repozitáře (GitHub) nemá adresář tstdata/. Test, který
 * z něj čte, tam vypíše SKIP a skončí 0; plné pokrytí se ověřuje jen
 * v soukromém repozitáři. Přítomnost dat se pozná podle souboru
 * TSTDATA_MARKER (testy běží z build/).
 */

#ifndef TSTDATA_SKIP_H
#define TSTDATA_SKIP_H

#include <stdio.h>

/** @brief Soubor, podle kterého test pozná přítomnost tstdata/ (relativně k build/). */
#define TSTDATA_MARKER "../tstdata/popis.txt"


/**
 * @brief Zjistí, zda chybí testovací data; když ano, vypíše SKIP.
 * @param test_name Jméno testu do výpisu.
 * @return 1 = data chybí (volající skončí s návratem 0), 0 = data jsou.
 */
static inline int tstdata_skip ( const char *test_name ) {
    FILE *f = fopen ( TSTDATA_MARKER, "rb" );
    if ( f ) {
        fclose ( f );
        return 0;
    }
    printf ( "SKIP: %s needs %s (test data are not in the public repository)\n", test_name, TSTDATA_MARKER );
    return 1;
}

#endif /* TSTDATA_SKIP_H */
