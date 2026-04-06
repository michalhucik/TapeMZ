# tmzinfo - Prohlížeč obsahu TMZ/TZX souborů

Zobrazuje podrobné informace o struktuře TMZ nebo TZX souboru.
Vypíše typ souboru, verzi, počet bloků a detailní informace
o každém bloku včetně MZF hlaviček, časování, formátu a metadat.

## Použití

```
tmzinfo <soubor.tmz|soubor.tzx>
```

## Argumenty

| Argument | Popis |
|----------|-------|
| `<soubor>` | Vstupní TMZ nebo TZX soubor |

## Volby

| Volba | Hodnoty | Výchozí | Popis |
|-------|---------|---------|-------|
| `--name-encoding` | ascii, utf8-eu, utf8-jp | ascii | Kódování názvu souboru: ascii (výchozí), utf8-eu (evropská Sharp MZ), utf8-jp (japonská Sharp MZ) |
| `--version` | - | - | Zobrazit verzi programu |
| `--lib-versions` | - | - | Zobrazit verze použitých knihoven |

### Podrobnosti k volbám

**--name-encoding** - určuje, jak se zobrazují názvy souborů z MZF hlaviček:
- `ascii` - překlad Sharp MZ znakové sady do ASCII (výchozí, zpětně kompatibilní)
- `utf8-eu` - překlad do UTF-8, evropská varianta znakové sady (zobrazí skutečné Sharp MZ glyfy)
- `utf8-jp` - překlad do UTF-8, japonská varianta znakové sady (katakana místo malých písmen)

Všechny informace se vypisují na standardní výstup.

## Zobrazované informace

### Hlavička souboru

- Typ souboru: TMZ (TapeMZ!) nebo TZX (ZXTape!)
- Verze formátu (např. 1.0 pro TMZ, 1.20 pro TZX)
- Celkový počet bloků

### MZ bloky (0x40-0x45)

- **0x40 MZ Standard Data** - cílový stroj, pulzní sada, pauza, MZF hlavička (název, typ, velikost, adresy)
- **0x41 MZ Turbo Data** - formát (NORMAL/TURBO/FASTIPL/FSK/SLOW/DIRECT/CPM-TAPE), rychlost, časování pulzů, MZF hlavička
- **0x42 MZ Extra Body** - formát, rychlost, pauza, velikost těla
- **0x43 MZ Machine Info** - stroj, CPU takt, verze ROM
- **0x44 MZ Loader** - typ loaderu (TURBO/FASTIPL/FSK/SLOW/DIRECT), rychlost, velikost, MZF hlavička
- **0x45 MZ BASIC Data** - stroj, pulzní sada, počet chunků, MZF hlavička

### TZX bloky

- **0x10 Standard Speed Data** - pauza, délka dat, flag (header/data)
- **0x11 Turbo Speed Data** - pilot, sync, časování bitů, počet bajtů
- **0x12 Pure Tone** - délka pulzů v T-states, počet pulzů
- **0x13 Pulse Sequence** - jednotlivé pulzy v T-states
- **0x14 Pure Data** - časování bitů, pauza, data
- **0x15 Direct Recording** - T-states/vzorek (~frekvence), pauza, data
- **0x18 CSW Recording** - vzorkovací frekvence, komprese, počet pulzů
- **0x20 Pause** - délka pauzy (0 = STOP)
- **0x21 Group Start** - název skupiny
- **0x30 Text Description** - textový popis
- **0x31 Message** - zobrazovaná zpráva s časem
- **0x32 Archive Info** - metadata (Title, Author, Year, Publisher, ...)
- **0x33 Hardware Type** - požadavky na hardware

## Příklady

Zobrazení obsahu TMZ souboru:

```
tmzinfo game.tmz
```

Příklad výstupu:

```
=== game.tmz ===

File type  : TMZ (TapeMZ!)
Version    : 1.0
Blocks     : 2

  [  0] ID 0x40  MZ Standard Data           (4267 bytes)  [MZ]
      Machine : MZ-800
      Pulseset: MZ-800/1500
      Pause   : 1000 ms
      Body    : 4139 bytes
      Filename : "GAME"
      Type     : 0x01 (OBJ (machine code))
      Size     : 4139 bytes (0x102B)
      Load addr: 0x1200
      Exec addr: 0x1200

  [  1] ID 0x41  MZ Turbo Data              (4280 bytes)  [MZ]
      Machine : MZ-800
      Pulseset: MZ-800/1500
      Format  : TURBO
      Speed   : 2:1 (2400 Bd)
      ...
```

Zobrazení obsahu TZX souboru se ZX Spectrum daty:

```
tmzinfo spectrum.tzx
```

Zobrazení názvů souborů v evropské UTF-8 znakové sadě:

```
tmzinfo game.tmz --name-encoding utf8-eu
```

Zobrazení TMZ souboru s Archive Info:

```
tmzinfo tape.tmz
```
