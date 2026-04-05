# TMZ knihovna (src/libs/tmz/)

Knihovna pro cteni, zapis a prehravani souboru ve formatu TMZ (TapeMZ) -
kazetovy archiv pro pocitace Sharp MZ. TMZ je rozsireni formatu TZX v1.20
o bloky specificke pro Sharp MZ (0x40-0x4F).

## Soubory

| Soubor | Ucel |
|--------|------|
| tmz.h | Hlavni API - hlavicka, blokove ID, enumy, alokator, error callback |
| tmz.c | Parser TMZ/TZX souboru (load/save/free), blokove cteni, nazvy bloku |
| tmz_blocks.h | Packed struktury MZ-specifickych bloku, parse/create API |
| tmz_blocks.c | Parsovani (zero-copy), vytvareni bloku, MZF konverze |
| tmz_player.h | Player API - konfigurace, prehravani bloku |
| tmz_player.c | Player dispatcher, MZ a TZX prehravani |

## Architektura

```
TMZ/TZX soubor (generic_driver)
  |
  +-- tmz_load() --> st_TMZ_FILE (hlavicka + pole bloku)
  |
  +-- tmz_player_play_block() --> CMT stream
        |
        +-- MZ bloky (0x40-0x45) --> mztape --> cmt_stream
        |
        +-- TZX audio bloky (0x10-0x15, 0x20) --> tzx --> cmt_vstream
        |
        +-- Ridici/info bloky --> NULL (zadne audio)
```

### Tri vrstvy

1. **Souborova vrstva** (tmz.c) - cteni/zapis TMZ/TZX souboru, blokova struktura
2. **Blokova vrstva** (tmz_blocks.c) - parsovani a vytvareni MZ-specifickych bloku
3. **Player vrstva** (tmz_player.c) - generovani audio streamu, dispatcher

## Implementovano

### Souborove operace

| Funkce | Stav | Poznamka |
|--------|------|----------|
| tmz_load() | ano | Nacte TMZ i TZX soubory (oba typy hlavicek) |
| tmz_save() | ano | Zapise vsechny bloky sekvencne |
| tmz_free() | ano | NULL-safe uvolneni |
| tmz_read_header() | ano | Validace signatury TapeMZ! / ZXTape! |
| tmz_write_header() | ano | - |
| tmz_header_init() | ano | Inicializace na TapeMZ! v1.0 |

### MZ-specificke bloky

| ID | Blok | Parse | Create | Player | Poznamka |
|----|------|-------|--------|--------|----------|
| 0x40 | MZ Standard Data | ano | ano | ano | NORMAL 1200 Bd pres mztape |
| 0x41 | MZ Turbo Data | ano | ano | castecne | Player pouziva mztape, ktera generuje jen NORMAL FM kodovani. Pole format (TURBO/FASTIPL/FSK/SLOW/DIRECT) se ignoruje - data se prehraji v FM s danou rychlosti. Pro korektni prehrani nestandardnich formatu je treba pouzit TZX bloky 0x11-0x15. |
| 0x42 | MZ Extra Body | ano | ne | ne | Parsovani hotovo, create a player cekaji na implementaci mztape rozsireni pro chunkovane bloky |
| 0x43 | MZ Machine Info | ano | ano | ne (info) | Informacni blok - player ho precte ale negeneruje audio |
| 0x44 | MZ Loader | ano | ne | ne | Parsovani hotovo. Player by musel generovat standardni zaznam (hlavicka s loaderem) + turbo body - vyzaduje komplexni sekvencovani bloku. |
| 0x45 | MZ BASIC Data | ano | ano | ne | Parsovani a vytvareni hotovo. Player vyzaduje rozsireni mztape o generovani BSD/BRD chunkovanych bloku (kazdy chunk = samostatny body blok s tapemarkem). |

### TZX bloky (delegovano na tzx knihovnu)

| ID | Blok | Player | Poznamka |
|----|------|--------|----------|
| 0x10 | Standard Speed Data | ano | Pres tzx knihovnu |
| 0x11 | Turbo Speed Data | ano | Pres tzx knihovnu - klicovy pro nestandardni MZ formaty |
| 0x12 | Pure Tone | ano | Pres tzx knihovnu |
| 0x13 | Pulse Sequence | ano | Pres tzx knihovnu |
| 0x14 | Pure Data | ano | Pres tzx knihovnu |
| 0x15 | Direct Recording | ano | Pres tzx knihovnu |
| 0x18 | CSW Recording | ne | TZX knihovna ma parser ale ne generator (CSW dekomprese) |
| 0x19 | Generalized Data | ne | Slozita struktura, zridka pouzivany |
| 0x20 | Pause | ano | Pres tzx knihovnu |

### Ridici a informacni bloky

| ID | Blok | Player |
|----|------|--------|
| 0x21 | Group Start | preskocen (OK) |
| 0x22 | Group End | preskocen (OK) |
| 0x23 | Jump to Block | preskocen (OK) |
| 0x24 | Loop Start | preskocen (OK) |
| 0x25 | Loop End | preskocen (OK) |
| 0x28 | Select Block | preskocen (OK) |
| 0x2A | Stop Tape if 48K | preskocen (OK) |
| 0x2B | Set Signal Level | preskocen (OK) |
| 0x30 | Text Description | preskocen (OK) |
| 0x31 | Message Block | preskocen (OK) |
| 0x32 | Archive Info | preskocen (OK) |
| 0x33 | Hardware Type | preskocen (OK) |
| 0x35 | Custom Info | preskocen (OK) |
| 0x5A | Glue Block | preskocen (OK) |

Ridici bloky (loop, jump, group) player preskakuje s TMZ_PLAYER_OK.
Jejich semantika (opakovani smycek, skoky) **neni implementovana** -
player prehraje bloky sekvencne bez opakovani. Pro plnohodnotne prehravani
je treba implementovat stavovy automat se zasobnikem.

## Error handling

### Chybove kody TMZ (en_TMZ_ERROR)

| Kod | Vyznam | Kdy nastane |
|-----|--------|-------------|
| TMZ_OK | Uspech | - |
| TMZ_ERROR_IO | I/O chyba | Selhani generic_driver read/write |
| TMZ_ERROR_INVALID_SIGNATURE | Neplatna signatura | Hlavicka neni TapeMZ! ani ZXTape! |
| TMZ_ERROR_INVALID_VERSION | Neplatna verze | Rezervovano pro budouci pouziti |
| TMZ_ERROR_INVALID_BLOCK | Neplatny blok | Data bloku jsou kratsi nez ocekavana struktura |
| TMZ_ERROR_UNEXPECTED_EOF | Neocekavany konec | Soubor konci uprostred bloku |
| TMZ_ERROR_ALLOC | Selhani alokace | malloc/calloc vraci NULL |
| TMZ_ERROR_UNKNOWN_BLOCK | Neznamy blok | Rezervovano - parser nezname bloky preskoci |

### Chybove kody playeru (en_TMZ_PLAYER_ERROR)

| Kod | Vyznam | Kdy nastane |
|-----|--------|-------------|
| TMZ_PLAYER_OK | Uspech | Vcetne ridicich bloku (vraci NULL stream, ale OK) |
| TMZ_PLAYER_ERROR_NULL_INPUT | NULL parametr | block nebo config je NULL |
| TMZ_PLAYER_ERROR_NO_BLOCKS | Zadne bloky | Soubor neobsahuje bloky |
| TMZ_PLAYER_ERROR_UNSUPPORTED | Nepodporovany blok | CSW (0x18), Generalized (0x19), MZ BASIC Data (0x45), neznama ID |
| TMZ_PLAYER_ERROR_STREAM_CREATE | Selhani generovani | TZX/mztape knihovna vrati NULL |
| TMZ_PLAYER_ERROR_ALLOC | Selhani alokace | malloc/calloc vraci NULL |

### Chovani pri chybach

- **Error callback**: tmz.c a tmz_blocks.c pouzivaji konfigurovatelny error callback
  (tmz_set_error_callback). Vychozi = fprintf(stderr). Volany pri kazde chybe s nazvem
  funkce a cislem radku.
- **Player**: vypisuje chyby pres interni callback, navic vraci chybovy kod.
  Pri nepodporovanem bloku vraci TMZ_PLAYER_ERROR_UNSUPPORTED - **nikdy tise neselhava**.
- **Parser (tmz_load)**: pri chybe cteni bloku ukonci parsovani a vrati co se podarilo
  nacist. Chyba se loguje pres error callback.
- **Parse funkce bloku**: modifikuji data in-place (konverze endianity) - volat jen jednou!

### Zname nedostatky v error handling

1. **Pulse Sequence endianita** - tzx_parse_pulse_sequence() vraci ukazatel primo
   na raw data bez konverze LE->host. Na big-endian platformach by pulzy mely spatne
   hodnoty. Konverze probiha az v tzx_generate_pulse_sequence() pres read_le16().
2. **Ridici bloky bez semantiky** - player preskakuje Loop/Jump/Call bloky bez
   vykonavani jejich logiky. U souboru s opakovanim smycek se prehrajou jen jednou.

## Konverzni cesty

```
MZF --> tmz_block_from_mzf() --> st_TMZ_BLOCK (0x40)
st_TMZ_BLOCK (0x40/0x41) --> tmz_block_to_mzf() --> st_MZF

MZF -> TMZ -> MZF: bezztratova round-trip pro standardni data
```

## Zavislosti

- `generic_driver` - I/O abstrakce
- `mzf` - MZF hlavicky, validace, endianita
- `mztape` - generovani CMT streamu z MZF dat
- `cmt_stream` - vystupni audio format
- `cmtspeed` - rychlostni pomery
- `endianity` - byte order konverze
- `tzx` - parsovani a generovani TZX audio bloku

## Pouziti

```c
#include "libs/tmz/tmz.h"
#include "libs/tmz/tmz_blocks.h"
#include "libs/tmz/tmz_player.h"

/* nacteni souboru */
en_TMZ_ERROR err;
st_TMZ_FILE *file = tmz_load(handler, &err);
if (!file) { printf("Error: %s\n", tmz_error_string(err)); }

/* prehrani vsech bloku */
st_TMZ_PLAYER_CONFIG cfg;
tmz_player_config_init(&cfg);

for (uint32_t i = 0; i < file->block_count; i++) {
    en_TMZ_PLAYER_ERROR perr;
    st_CMT_STREAM *stream = tmz_player_play_block(&file->blocks[i], &cfg, &perr);

    if (perr == TMZ_PLAYER_ERROR_UNSUPPORTED) {
        printf("Block 0x%02X: %s - skipping\n",
               file->blocks[i].id, tmz_block_id_name(file->blocks[i].id));
        continue;
    }

    if (stream) {
        /* export do WAV, prehrani v emulatoru, ... */
        cmt_stream_destroy(stream);
    }
}

tmz_free(file);
```
