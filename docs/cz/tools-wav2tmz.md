# wav2tmz - Analyzátor a dekodér WAV nahrávek

Analyzuje WAV soubor obsahující nahrávku magnetofonové kazety
počítačů Sharp MZ (nebo ZX Spectrum) a extrahuje z něj MZF soubory
nebo TMZ archiv.

Automaticky detekuje formát záznamu: NORMAL, TURBO, FASTIPL, BSD,
CPM-CMT, CPM-TAPE, MZ-80B, FSK, SLOW, DIRECT, ZX Spectrum.

Pokud je výstupní formát TMZ a výstupní soubor již existuje,
nové bloky se přidají na konec existujícího archivu.

## Použití

```
wav2tmz vstup.wav [-o vystup] [volby]
```

## Volby

| Volba | Hodnota | Výchozí | Popis |
|-------|---------|---------|-------|
| `-o` | `<soubor>` | odvozeno ze vstupu | Výstupní soubor |
| `--output-format` | mzf, tmz | mzf | Formát výstupu |
| `--schmitt` | - | vypnuto | Použít Schmitt trigger místo zero-crossing |
| `--tolerance` | 0.02-0.35 | 0.10 | Tolerance detekce leader tónu |
| `--preprocess` | - | zapnuto | Zapnout preprocessing signálu |
| `--no-preprocess` | - | - | Vypnout preprocessing (DC offset, HP filtr, normalizace) |
| `--histogram` | - | vypnuto | Vypsat histogram délek pulzů |
| `--verbose`, `-v` | - | vypnuto | Podrobný výstup analýzy |
| `--channel` | L, R | L | Výběr kanálu ze sterea |
| `--invert` | - | vypnuto | Invertovat polaritu signálu |
| `--keep-unknown` | - | vypnuto | Uložit neidentifikované úseky jako Direct Recording |
| `--raw-format` | direct | direct | Formát pro neidentifikované bloky |
| `--pass` | `<N>` | 1 | Počet průchodů (zatím nepoužito) |
| `--name-encoding` | ascii, utf8-eu, utf8-jp | ascii | Kódování názvu souboru: ascii (výchozí), utf8-eu (evropská Sharp MZ), utf8-jp (japonská Sharp MZ) |
| `--help`, `-h` | - | - | Zobrazit nápovědu |

### Podrobnosti k volbám

**--output-format** - určuje, jak budou dekódovaná data uložena:
- `mzf` - každý dekódovaný soubor se uloží jako samostatný MZF soubor
  (pojmenování: vstup_1.mzf, vstup_2.mzf, ...)
- `tmz` - všechny bloky se uloží do jednoho TMZ archivu

**--schmitt** - použije Schmitt trigger pro detekci pulzů místo
standardního zero-crossing. Vhodné pro zašuměné nahrávky.

**--tolerance** - tolerance pro rozpoznání leader tónu. Nižší hodnota
je přísnější, vyšší hodnota toleruje větší odchylky v časování.
Rozsah: 0.02 (velmi přísné) až 0.35 (velmi benevolentní).

**--channel** - při stereo nahrávkách umožňuje zvolit kanál.
Kazeta je typicky nahraná na levém kanálu.

**--invert** - invertuje polaritu signálu. Použijte, pokud je
nahrávka ve špatné polaritě (např. z jiného nahrávacího zařízení).

**--name-encoding** - určuje, jak se zobrazují názvy souborů z MZF hlaviček:
- `ascii` - překlad Sharp MZ znakové sady do ASCII (výchozí, zpětně kompatibilní)
- `utf8-eu` - překlad do UTF-8, evropská varianta znakové sady (zobrazí skutečné Sharp MZ glyfy)
- `utf8-jp` - překlad do UTF-8, japonská varianta znakové sady (katakana místo malých písmen)

**--keep-unknown** - úseky nahrávky, které nebyly identifikovány
jako žádný známý formát, se uloží jako TZX blok 0x15 (Direct Recording).
Použitelné pro zachování celého obsahu kazety.

## Výstupní formáty

### Režim MZF (výchozí)

Pro každý dekódovaný soubor se vytvoří samostatný MZF soubor.
ZX Spectrum bloky se v tomto režimu přeskakují (použijte TMZ formát).

### Režim TMZ

Všechny dekódované bloky se uloží do jednoho TMZ souboru:
- NORMAL 1:1 -> blok 0x40 (MZ Standard Data)
- NORMAL jiná rychlost -> blok 0x41 (MZ Turbo Data, format=NORMAL)
- TURBO/FASTIPL/FSK/SLOW/DIRECT/CPM-CMT/CPM-TAPE -> blok 0x41
- BSD -> blok 0x45 (MZ BASIC Data)
- ZX Spectrum -> blok 0x10 (Standard Speed Data)
- Neidentifikované (s --keep-unknown) -> blok 0x15 (Direct Recording)

Soubor začíná blokem 0x30 (Text Description) s metadaty o zdrojovém WAV.

## Příklady

Základní dekódování do MZF souborů:

```
wav2tmz recording.wav
```

Dekódování do TMZ archivu:

```
wav2tmz recording.wav --output-format tmz
```

Dekódování s vlastním výstupním souborem:

```
wav2tmz recording.wav -o game.mzf
```

Dekódování se Schmitt triggerem (zašuměná nahrávka):

```
wav2tmz noisy_tape.wav --schmitt --tolerance 0.15
```

Podrobná analýza s histogramem:

```
wav2tmz recording.wav --verbose --histogram
```

Dekódování pravého kanálu s invertovanou polaritou:

```
wav2tmz stereo_tape.wav --channel R --invert
```

Dekódování s uložením neidentifikovaných úseků:

```
wav2tmz full_tape.wav --output-format tmz --keep-unknown
```

Vypnutí preprocessingu (surová data):

```
wav2tmz clean_recording.wav --no-preprocess
```

Dekódování se zobrazením názvů v evropské UTF-8 znakové sadě:

```
wav2tmz recording.wav --name-encoding utf8-eu
```

Přidání dalších nahrávek do existujícího TMZ:

```
wav2tmz side_a.wav -o tape.tmz --output-format tmz
wav2tmz side_b.wav -o tape.tmz --output-format tmz
```
