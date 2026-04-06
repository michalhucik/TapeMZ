# dat2bsd - Import binárních dat do TMZ jako BSD/BRD blok

Importuje libovolný binární soubor do TMZ formátu jako blok 0x45
(MZ BASIC Data). Vstupní data se rozřežou na 256-bajtové chunky
se sekvenčními ID (0x0000, 0x0001, ...) a terminačním chunkem
s ID 0xFFFF.

Používá se pro uložení BSD (BASIC data) nebo BRD (BASIC read-after-run)
souborů do TMZ archivu.

## Použití

```
dat2bsd <vstup.dat> <vystup.tmz> [volby]
```

## Volby

| Volba | Hodnoty | Výchozí | Popis |
|-------|---------|---------|-------|
| `--name` | `<nazev>` | odvozeno ze vstupu | Název souboru v MZF hlavičce |
| `--ftype` | bsd, brd | bsd | Typ souboru (0x03 BSD nebo 0x04 BRD) |
| `--machine` | generic, mz700, mz800, mz1500, mz80b | mz800 | Cílový počítač |
| `--pulseset` | 700, 800, 80b, auto | auto | Pulzní sada (auto = dle machine) |
| `--speed` | 1:1, 2:1, 2:1cpm, 3:1, 3:2, 7:3, 8:3, 9:7, 25:14 | 1:1 | Poměr rychlosti |
| `--pause` | 0-65535 | 1000 | Pauza po bloku v milisekundách |
| `--version` | - | - | Zobrazit verzi programu |
| `--lib-versions` | - | - | Zobrazit verze použitých knihoven |

### Podrobnosti

**--ftype** - typ MZF souboru:
- `bsd` (0x03) - BASIC Data - standardní datový soubor
- `brd` (0x04) - BASIC Read-after-run - data se načítají po spuštění programu

**--name** - název souboru uložený v MZF hlavičce (max 17 znaků Sharp MZ ASCII).
Pokud není zadáno, odvodí se z názvu vstupního souboru (bez cesty a přípony).

### Struktura chunků

Každý chunk má 258 bajtů:
- 2 bajty: ID chunku (little-endian, 0x0000-0xFFFE pro data, 0xFFFF pro terminator)
- 256 bajtů: data (poslední datový chunk může být doplněn nulami)

Maximální velikost vstupních dat: 65534 chunků x 256 bajtů = 16 776 704 bajtů.

## Příklady

Základní import:

```
dat2bsd data.dat tape.tmz
```

Import s vlastním názvem a typem BRD:

```
dat2bsd savegame.dat tape.tmz --name "SAVEGAME" --ftype brd
```

Import pro MZ-700 s rychlostí 2:1:

```
dat2bsd data.bin tape.tmz --machine mz700 --speed 2:1
```

Import s delší pauzou:

```
dat2bsd large_data.dat tape.tmz --pause 2000
```

Příklad výstupu:

```
Imported: data.dat -> tape.tmz

  Filename   : "data"
  File type  : 0x03 (BSD)
  Input size : 1024 bytes
  Chunks     : 4 data + 1 termination = 5 total
  Machine    : MZ-800
  Pulseset   : MZ-800/1500
  Speed      : 1:1
  Pause      : 1000 ms
  Block      : 0x45 (MZ BASIC Data)
  TMZ size   : 1310 bytes
```
