# tmz2wav - Konvertor TMZ/TZX do WAV

Konvertuje TMZ nebo TZX soubor na WAV audio soubor (mono, 8-bit PCM).
Používá TMZ player k přehrání všech audio bloků a generování
výsledného signálu.

Podporuje všechny bloky, které TMZ player umí přehrát:
- MZ bloky 0x40 (Standard Data) a 0x41 (Turbo Data) přes mztape kodéry
- TZX bloky 0x10-0x15 a 0x20 přes TZX knihovnu

Řídicí a informační bloky (0x21, 0x30, 0x32 atd.) jsou automaticky přeskočeny.

## Použití

```
tmz2wav <vstup.tmz|vstup.tzx> <vystup.wav> [volby]
```

## Volby

| Volba | Hodnota | Výchozí | Popis |
|-------|---------|---------|-------|
| `--rate` | 8000-192000 | 44100 | Vzorkovací frekvence v Hz |
| `--pulseset` | 700, 800, 80b | 800 | Výchozí pulzní sada |
| `--version` | - | - | Zobrazit verzi programu |
| `--lib-versions` | - | - | Zobrazit verze použitých knihoven |

### Podrobnosti k volbám

**--rate** - vzorkovací frekvence výstupního WAV souboru. Vyšší hodnoty
znamenají přesnější reprodukci signálu, ale větší soubory.
Běžné hodnoty: 22050, 44100, 48000.

**--pulseset** - výchozí pulzní sada pro bloky, které ji nemají
explicitně specifikovanou. Ovlivňuje časování pulzů:
- `700` - MZ-700/80K/80A časování
- `800` - MZ-800/1500 časování
- `80b` - MZ-80B časování

## Příklady

Základní konverze do WAV (44100 Hz):

```
tmz2wav game.tmz game.wav
```

Konverze s vyšší kvalitou (48000 Hz):

```
tmz2wav tape.tmz tape.wav --rate 48000
```

Konverze TZX souboru (ZX Spectrum):

```
tmz2wav spectrum.tzx spectrum.wav
```

Konverze s pulzní sadou MZ-700:

```
tmz2wav old_tape.tmz old_tape.wav --pulseset 700
```

Konverze s nižší vzorkovací frekvencí (menší soubor):

```
tmz2wav tape.tmz tape.wav --rate 22050
```

Příklad výstupu programu:

```
Input  : game.tmz (TMZ, v1.0, 2 blocks)
Output : game.wav
Rate   : 44100 Hz
Pulseset: MZ-800/1500

  [  0] 0x40 MZ Standard Data           -> 12.345 s
  [  1] 0x41 MZ Turbo Data              -> 6.789 s

Audio blocks: 2
Total  : 19.134 s (844210 samples, 1256 bytes vstream data)
Saved  : game.wav
```
