# dat2bsd - Import binárních dat do TMZ jako datový soubor BASICu

Importuje libovolný binární soubor do TMZ jako datový soubor BASICu
(BSD/BRD) v bloku 0x46 s profilem `BASIC-1Z016`. Vstupní data se rozřežou
na kusy po 256 bajtech se sekvenčními ID (0x0000, 0x0001, ...). Poslední
kus má ID 0xFFFF a nese zbytek dat.

Profil `BASIC-1Z016` přehraje soubor tak, jak ho na kazetu zapisuje
BASIC 1Z-016 (`WOPEN#`, `PRINT#`, `CLOSE#`): před každým kusem tón
a tapemark, za každým kusem kopie, mezi kusy pauza. Časování je změřené
v emulátoru a BASIC 1Z-016 takový soubor přečte (`ROPEN#`, `INPUT#`).

## Použití

```
dat2bsd <vstup.dat> <vystup.tmz> [volby]
```

## Volby

| Volba | Hodnoty | Výchozí | Popis |
|-------|---------|---------|-------|
| `--name` | `<nazev>` | odvozeno ze vstupu | Název souboru v MZF hlavičce |
| `--ftype` | brd, bsd | brd | Typ souboru (0x04 nebo 0x03) |
| `--machine` | generic, mz700, mz800, mz1500, mz80b | mz800 | Cílový počítač (jen informace v bloku) |
| `--pause` | 0-65535 | 1000 | Pauza za posledním kusem v milisekundách |
| `--version` | - | - | Zobrazit verzi programu |
| `--lib-versions` | - | - | Zobrazit verze použitých knihoven |

Volby `--speed` a `--pulseset` z verze 1.0 už nejsou platné (chyba):
profil `BASIC-1Z016` má pevné časování.

### Podrobnosti

**--ftype** - typ MZF souboru:
- `brd` (0x04) - typ, který zapisuje `WOPEN#` BASICu 1Z-016 (datový soubor,
  ROM monitoru ho ve výpisu adresáře jmenuje BRD)
- `bsd` (0x03) - typ, který ROM monitoru jmenuje BSD

**--name** - název souboru uložený v MZF hlavičce (max 17 znaků Sharp MZ ASCII).
Pokud není zadáno, odvodí se z názvu vstupního souboru (bez cesty a přípony).

**Data pro BASIC** - `INPUT#` čte data tak, jak je zapisuje `PRINT#`: text
ukončený znakem 0Dh, konec souboru je znak 1Ah. dat2bsd data nemění.

### Struktura kusů

Každý kus má 258 bajtů:
- 2 bajty: ID kusu (little-endian, 0x0000, 0x0001, ..., poslední 0xFFFF)
- 256 bajtů: data (poslední kus je doplněný nulami)

Maximální velikost vstupních dat: 254 kusů x 256 bajtů = 65 024 bajtů
(blok 0x46 má nejvýš 255 částí). Prázdný vstup dá jeden kus s ID 0xFFFF.

## Příklady

Základní import:

```
dat2bsd data.dat tape.tmz
```

Import s vlastním názvem a typem 0x03:

```
dat2bsd savegame.dat tape.tmz --name "SAVEGAME" --ftype bsd
```

Import s delší pauzou za souborem:

```
dat2bsd large_data.dat tape.tmz --pause 2000
```

Příklad výstupu:

```
Imported: data.dat -> tape.tmz

  Filename   : "data"
  File type  : 0x04 (BRD)
  Input size : 1024 bytes
  Chunks     : 4 (last ID 0xFFFF)
  Machine    : MZ-800
  Pause      : 1000 ms
  Block      : 0x46 (MZ File, profile BASIC-1Z016)
  TMZ size   : 2880 bytes
```
