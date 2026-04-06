# tap2tmz - Konvertor ZX Spectrum TAP do TZX/TMZ

Konvertuje ZX Spectrum TAP soubor do formátu TZX nebo TMZ.
Každý TAP blok se převede na TZX blok 0x10 (Standard Speed Data).

Pokud výstupní soubor již existuje, nové bloky se přidají
na konec existujícího souboru. Pokud neexistuje, vytvoří se nový.

Výstupní soubor je ve výchozím stavu TZX (signatura "ZXTape!").
Přepínač `--tmz` přepne na TMZ signaturu ("TapeMZ!").

## Použití

```
tap2tmz <vstup.tap> <vystup.tzx|vystup.tmz> [volby]
```

## Volby

| Volba | Hodnota | Výchozí | Popis |
|-------|---------|---------|-------|
| `--pause` | 0-65535 | 1000 | Pauza po každém bloku v milisekundách |
| `--tmz` | - | vypnuto | Použít TMZ signaturu ("TapeMZ!") místo TZX |
| `--version` | - | - | Zobrazit verzi programu |
| `--lib-versions` | - | - | Zobrazit verze použitých knihoven |

## TAP formát

TAP soubor je sekvence bloků. Každý blok má strukturu:
- 2 bajty: délka dat (little-endian)
- N bajtů: data (začínají flag bajtem: 0x00 = header, 0xFF = data blok)

## Příklady

Základní konverze TAP do TZX:

```
tap2tmz game.tap game.tzx
```

Konverze s TMZ signaturou:

```
tap2tmz game.tap game.tmz --tmz
```

Konverze s kratší pauzou mezi bloky:

```
tap2tmz game.tap game.tzx --pause 500
```

Přidání dalšího TAP souboru do existujícího TZX:

```
tap2tmz loader.tap tape.tzx
tap2tmz game.tap tape.tzx
```

Příklad výstupu programu:

```
Converted: game.tap -> game.tzx

  Format: TZX (ZXTape!) v1.20
  Blocks: 4 (new: 4)
  Pause : 1000 ms

  [  0] 0x10 Standard Speed Data     19 bytes  flag=0x00 (header)
  [  1] 0x10 Standard Speed Data   6914 bytes  flag=0xFF (data)
  [  2] 0x10 Standard Speed Data     19 bytes  flag=0x00 (header)
  [  3] 0x10 Standard Speed Data    256 bytes  flag=0xFF (data)
```
