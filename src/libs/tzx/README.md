# TZX knihovna (src/libs/tzx/)

Knihovna pro parsovani a generovani audio streamu ze standardnich
TZX v1.20 bloku. Pouzivana knihovnou TMZ pro prehravani TZX bloku
uvnitr TMZ souboru.

## Soubory

| Soubor | Ucel |
|--------|------|
| tzx.h  | Verejne API - konfigurace, parsovani, generovani, konverzni funkce |
| tzx.c  | Implementace vsech parseru a generatoru |

## Architektura

```
Raw TZX blok (uint8_t *)
  |
  +-- tzx_parse_*()  --> naparsovana struktura (st_TZX_*)
  |
  +-- tzx_generate_*() --> CMT vstream (st_CMT_VSTREAM *)
```

Knihovna pracuje ve dvou krocich:
1. **Parsovani** - raw bajty bloku -> typovana struktura (zero-copy, ukazatele do raw dat)
2. **Generovani** - typovana struktura -> CMT vstream (RLE kodovane pulzy)

Casovani je v Z80 T-states. Prepocet na vzorky: `samples = tstates * sample_rate / cpu_clock`.
Vychozi CPU takt je 3 500 000 Hz (ZX Spectrum), ale lze zmenit pres st_TZX_CONFIG.cpu_clock
pro Sharp MZ (3 546 900 Hz) nebo jiny procesor.

## Implementovano

### Audio bloky (generuji CMT vstream)

| ID   | Blok | Parsovani | Generovani | Poznamka |
|------|------|-----------|------------|----------|
| 0x10 | Standard Speed Data | ano | ano | Standardni ROM casovani, pilot 8063/3223 pulzu |
| 0x11 | Turbo Speed Data | ano | ano | Vlastni pilot/sync/bit timing - klicovy pro nestandardni formaty |
| 0x12 | Pure Tone | ano | ano | Sekvence pulzu stejne delky |
| 0x13 | Pulse Sequence | ano | ano | Az 255 pulzu ruznych delek |
| 0x14 | Pure Data | ano | ano | Data bez pilot/sync, vlastni bit timing |
| 0x15 | Direct Recording | ano | ano | Primy 1-bit zaznam, libovolny signal |
| 0x20 | Pause | ano | ano | Ticho / zastaveni pasky |

### Bloky pouze s parsovanim (bez generovani)

| ID   | Blok | Parsovani | Generovani | Duvod |
|------|------|-----------|------------|-------|
| 0x18 | CSW Recording | ano | ne | Vyzaduje CSW v2 dekompresi (RLE/Z-RLE) - externi zavislost na zlib pro Z-RLE |

## Neimplementovano

| ID   | Blok | Duvod |
|------|------|-------|
| 0x19 | Generalized Data | Slozita struktura SYMDEF/PRLE s variabilnim poctem pulzu na symbol. Implementace je netrivialni a tento blok se v praxi pouziva zridka. |
| 0x21 | Group Start | Ridici blok - neprodukuje audio, zpracovava ho dispatcher v tmz_player |
| 0x22 | Group End | Ridici blok |
| 0x23 | Jump to Block | Ridici blok |
| 0x24 | Loop Start | Ridici blok |
| 0x25 | Loop End | Ridici blok |
| 0x26 | Call Sequence | Ridici blok |
| 0x27 | Return from Sequence | Ridici blok |
| 0x28 | Select Block | Ridici blok |
| 0x2A | Stop Tape if 48K | Ridici blok |
| 0x2B | Set Signal Level | Ridici blok |
| 0x30 | Text Description | Informacni blok |
| 0x31 | Message Block | Informacni blok |
| 0x32 | Archive Info | Informacni blok |
| 0x33 | Hardware Type | Informacni blok |
| 0x35 | Custom Info | Informacni blok |
| 0x5A | Glue Block | Spojovaci blok |

Ridici a informacni bloky nepotrebuji parsovani ani generovani v TZX knihovne -
zpracovava je dispatcher v tmz_player na vyssi urovni.

## Error handling

### Chybove kody (en_TZX_ERROR)

| Kod | Vyznam | Kdy nastane |
|-----|--------|-------------|
| TZX_OK | Uspech | - |
| TZX_ERROR_NULL_INPUT | NULL parametr | Vstupni ukazatel je NULL |
| TZX_ERROR_INVALID_BLOCK | Neplatna data | Blok je kratsi nez minimalni velikost, nebo data_length presahuje raw_length |
| TZX_ERROR_UNSUPPORTED | Nepodporovany blok | Blok nema implementovany generator (CSW, Generalized) |
| TZX_ERROR_STREAM_CREATE | Selhani generovani | Chyba pri pridavani pulzu do vstreamu (typicky nedostatek pameti) |
| TZX_ERROR_ALLOC | Selhani alokace | malloc/calloc vraci NULL |

### Chovani pri chybach

- Parse funkce: vraci chybovy kod, vystupni strukturu **nemodifikuji** pri chybe
- Generate funkce: vraci NULL a nastavi chybovy kod pres vystupni parametr *err
- Zadna funkce netise selhava - kazda chyba se propaguje nahoru
- Knihovna ma konfigurovatelny error callback (tzx_set_error_callback) -
  vychozi = fprintf(stderr). Volany pri kazde chybe s nazvem funkce,
  cislem radku a popisem problemu (vcetne ID bloku a velikosti dat).

### Omezeni

- Knihovna neprovadi validaci semantiky dat (napr. neoveruje, ze pilot_count > 0)
- Pulse Sequence (0x13): pole pulzu se cte primo z raw dat bez konverze endianity
  za runtime - funguje spravne jen na little-endian platformach
- Direct Recording: sousedni bity se stejnou urovni se slucuji do jednoho pulzu
  pro efektivnejsi vstream, coz je semanticky ekvivalentni ale fyzicky odlisna
  reprezentace

## Zavislosti

- `cmt_stream` (cmt_vstream) - vystupni format
- `endianity` - konverze byte order (vyuzito jen v CSW parseru)

## Pouziti

```c
#include "libs/tzx/tzx.h"

st_TZX_CONFIG cfg;
tzx_config_init(&cfg);
cfg.cpu_clock = 3546900; /* Sharp MZ-800 */

/* parsovani bloku 0x11 */
st_TZX_TURBO_SPEED parsed;
en_TZX_ERROR err = tzx_parse_turbo_speed(raw_data, raw_length, &parsed);

/* generovani vstreamu */
en_TZX_ERROR gen_err;
st_CMT_VSTREAM *vs = tzx_generate_turbo_speed(&parsed, &cfg, &gen_err);

/* export do WAV... */
cmt_vstream_destroy(vs);
```
