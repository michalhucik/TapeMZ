# tmz2wav - Konvertor TMZ/TZX do WAV

Konvertuje TMZ nebo TZX soubor na WAV audio soubor (mono, 8-bit PCM).
Používá TMZ player k přehrání vybraných (nebo všech) audio bloků
a generování výsledného signálu.

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
| `--blocks` | specifikace | všechny | Výběr bloků k exportu |
| `--append` | - | - | Připojit k existujícímu WAV souboru |
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

**--blocks** - výběr konkrétních bloků k exportu (indexy podle tmzinfo).
Formát specifikace:
- `0` - jen blok 0
- `0,2` - bloky 0 a 2
- `0-2,5` - bloky 0, 1, 2 a 5

Řídicí bloky (smyčky, skoky, volání) se zpracovávají vždy - výběr
ovlivňuje pouze audio bloky.

**--append** - připojí nový audio signál na konec existujícího WAV souboru.
Existující WAV musí být mono PCM a jeho vzorkovací frekvence musí
odpovídat `--rate` (nebo výchozí 44100 Hz). Pokud soubor neexistuje,
program ohlásí chybu.

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

Export pouze prvního bloku:

```
tmz2wav game.tmz game_header.wav --blocks 0
```

Export vybraných bloků (0, 1, 2 a 5):

```
tmz2wav tape.tmz partial.wav --blocks 0-2,5
```

Postupné skládání WAV po blocích:

```
tmz2wav tape.tmz output.wav --blocks 0
tmz2wav tape.tmz output.wav --append --blocks 1
```

Příklad výstupu programu:

```
Input  : game.tmz (TMZ, v1.0, 4 blocks)
Output : game.wav
Rate   : 44100 Hz
Pulseset: MZ-800/1500
Blocks : 0,2

  [  0] 0x40 MZ Standard Data           -> 12.345 s
  [  1] 0x41 MZ Turbo Data                 SKIPPED
  [  2] 0x40 MZ Standard Data           -> 8.123 s
  [  3] 0x41 MZ Turbo Data                 SKIPPED

Audio blocks: 2, Skipped: 2
Total  : 20.468 s (902636 samples, 1024 bytes vstream data)
Saved  : game.wav
```

Příklad výstupu při append:

```
Input  : game.tmz (TMZ, v1.0, 2 blocks)
Output : game.wav (append)
Rate   : 44100 Hz
Pulseset: MZ-800/1500
Blocks : 1

Existing WAV: 12.345 s, 44100 Hz, 8-bit

  [  0] 0x40 MZ Standard Data              SKIPPED
  [  1] 0x41 MZ Turbo Data              -> 6.789 s

Audio blocks: 1, Skipped: 1
Total  : 19.134 s (844210 samples, 1256 bytes vstream data)
Saved  : game.wav
```
