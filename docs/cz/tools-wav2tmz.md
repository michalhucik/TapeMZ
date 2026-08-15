# wav2tmz - Analyzátor a dekodér WAV nahrávek

Analyzuje WAV soubor obsahující nahrávku magnetofonové kazety
počítačů Sharp MZ (nebo ZX Spectrum).

Implicitně provádí pouze analýzu a výpis nalezených bloků (bez ukládání).
S volbou `-o` nebo `--output-format` extrahuje MZF soubory nebo TMZ archiv.

Automaticky detekuje formát záznamu: NORMAL, TURBO, FASTIPL, BSD,
CPM-CMT, CPM-TAPE, MZ-80B, FSK, SLOW, DIRECT, ZX Spectrum.

## Použití

```
wav2tmz vstup.wav [-o vystup] [volby]
```

## Volby

| Volba | Hodnota | Výchozí | Popis |
|-------|---------|---------|-------|
| `-o` | `<soubor>` | odvozeno ze vstupu | Výstupní soubor (aktivuje ukládání) |
| `--output-format` | mzf, tmz | mzf | Formát výstupu (aktivuje ukládání) |
| `--append-tmz` | - | vypnuto | Připojit bloky do existujícího TMZ (bez toho je existující TMZ chyba) |
| `--overwrite-mzf` | - | vypnuto | Přepsat existující MZF soubory (bez toho je existující MZF chyba) |
| `--schmitt` | - | vypnuto | Použít Schmitt trigger místo zero-crossing |
| `--tolerance` | 0.02-0.35 | 0.10 | Tolerance detekce leader tónu |
| `--preprocess` | - | zapnuto | Zapnout preprocessing signálu |
| `--no-preprocess` | - | - | Vypnout preprocessing (DC offset, HP filtr, normalizace) |
| `--histogram` | - | vypnuto | Vypsat histogram délek pulzů |
| `--verbose`, `-v` | - | vypnuto | Podrobný výstup (reálná rychlost Bd, přibližná rychlost, pulzní sada) |
| `--channel` | L, R | L | Výběr kanálu ze sterea |
| `--invert` | - | vypnuto | Invertovat polaritu signálu |
| `--keep-unknown` | - | vypnuto | Uložit neidentifikované úseky jako Direct Recording |
| `--raw-format` | direct | direct | Formát pro neidentifikované bloky |
| `--pass` | `<N>` | 1 | Počet průchodů (zatím nepoužito) |
| `--pulse-mode` | approximate, exact | approximate | Režim ukládání délek pulzů pro TMZ výstup |
| `--charset` | eu, jp, utf8-eu, utf8-jp | eu | Znaková sada Sharp MZ pro zobrazení názvu souboru (viz níže) |
| `--recover` | - | vypnuto | Zapnout všechny recovery módy |
| `--recover-bsd` | - | vypnuto | Obnovit nekompletní BSD soubory (chybějící terminátor) |
| `--recover-body` | - | vypnuto | Obnovit částečné tělo (zatím neimplementováno) |
| `--recover-header` | - | vypnuto | Uložit osiřelé hlavičky (zatím neimplementováno) |
| `--version` | - | - | Zobrazit verzi programu |
| `--lib-versions` | - | - | Zobrazit verze použitých knihoven |
| `--help`, `-h` | - | - | Zobrazit nápovědu |

### Podrobnosti k volbám

**-o, --output-format** - aktivuje ukládání výstupu. Bez těchto voleb
se provádí pouze analýza a výpis nalezených bloků.
- `mzf` - každý dekódovaný soubor se uloží jako samostatný MZF soubor
  (pojmenování: vstup_1.mzf, vstup_2.mzf, ...)
- `tmz` - všechny bloky se uloží do jednoho TMZ archivu

**--append-tmz** - pokud výstupní TMZ soubor již existuje, přidá nové
bloky na konec existujícího archivu. Bez této volby je existence TMZ
souboru chyba. Pokud je volba zadána, ale soubor neexistuje, vytvoří
se nový soubor s varováním.

**--overwrite-mzf** - povolí přepsání existujících MZF souborů.
Bez této volby je existence výstupního MZF souboru chyba.

**--schmitt** - použije Schmitt trigger pro detekci pulzů místo
standardního zero-crossing. Vhodné pro zašuměné nahrávky.

**--tolerance** - tolerance pro rozpoznání leader tónu. Nižší hodnota
je přísnější, vyšší hodnota toleruje větší odchylky v časování.
Rozsah: 0.02 (velmi přísné) až 0.35 (velmi benevolentní).

**--channel** - při stereo nahrávkách umožňuje zvolit kanál.
Kazeta je typicky nahraná na levém kanálu.

**--invert** - invertuje polaritu signálu. Použijte, pokud je
nahrávka ve špatné polaritě (např. z jiného nahrávacího zařízení).

**--charset** - určuje znakovou sadu Sharp MZ pro konverzi názvu souboru:

Sharp MZ počítače používaly dvě varianty znakové sady - evropskou (EU) a japonskou (JP).
Obě sdílejí rozsah 0x20-0x5D (velká písmena, číslice, základní interpunkce), který je
shodný se standardním ASCII. Nad tímto rozsahem se sady liší:

- **EU** (evropská) - obsahuje malá písmena a-z a několik speciálních znaků
  (přehlásky, Eszett, šipky, karetní symboly). Při konverzi do ASCII se malá
  písmena převedou správně, ale speciální znaky se nahradí mezerou.
- **JP** (japonská) - obsahuje katakanu, kanji a speciální znaky (¥, £).
  Při konverzi do ASCII se vše nad 0x5D nahradí mezerou - výsledek je tedy
  ochuzený oproti EU variantě.

Režimy konverze:
- `eu` (výchozí) - Sharp MZ-EU -> ASCII. Malá písmena a základní znaky se převedou,
  speciální evropské znaky (přehlásky, šipky) se nahradí mezerou.
- `jp` - Sharp MZ-JP -> ASCII. Pouze znaky 0x20-0x5D se převedou, vše ostatní
  (katakana, kanji) se nahradí mezerou.
- `utf8-eu` - Sharp MZ-EU -> UTF-8. Zobrazí maximum znaků včetně přehlásek
  (Ö, ü, ß, Ä, ö, ä), šipek (↑↓←→), karetních symbolů (♤♡♧♢), libry (£) a pí (π).
- `utf8-jp` - Sharp MZ-JP -> UTF-8. Zobrazí maximum znaků včetně katakany (ア-ン),
  kanji dnů v týdnu (日月火水木金土), ¥, £ a dalších japonských znaků.

**--recover-bsd** - pokud BSD soubor na pásce nemá ukončovací chunk
(ID=0xFFFF), např. kvůli chybějícímu CLOSE v BASICu, jsou všechny úspěšně
dekódované chunky zachráněny. Bez této volby se nekompletní BSD data zahodí
(s diagnostickou zprávou a nápovědou). V TMZ výstupu je před obnovený
blok vložen Text Description (0x30) s varováním.

**--recover** - zapne všechny recovery módy najednou (--recover-bsd
a budoucí --recover-body, --recover-header).

**--pulse-mode** - řídí způsob ukládání délek pulzů do TMZ bloků:
- `approximate` (výchozí) - kvantizuje rychlost na nejbližší CMTSPEED poměr
  (1:1, 2:1, 7:3, ...). Standardní 1200 Bd používá blok 0x40, jiné rychlosti 0x41.
- `exact` - ukládá naměřené délky pulzů z histogramové analýzy přímo
  do polí bloku 0x41 (long_high/low, short_high/low). Rychlost se nastaví na 0
  (custom režim). Zachovává přesné časování originální nahrávky,
  včetně sub-standardních rychlostí (< 1200 Bd), které by se jinak
  zaokrouhlily na 1:1. Ovlivňuje pouze formáty NORMAL a MZ-80B.

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

Analýza WAV souboru (bez ukládání):

```
wav2tmz recording.wav
```

Podrobná analýza s rychlostmi a pulzními sadami:

```
wav2tmz recording.wav --verbose
```

Dekódování do MZF souborů:

```
wav2tmz recording.wav -o recording.mzf
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
wav2tmz noisy_tape.wav --schmitt --tolerance 0.15 -o output.mzf
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
wav2tmz recording.wav --charset utf8-eu
```

Obnova nekompletních BSD souborů:

```
wav2tmz tape_with_broken_bsd.wav --recover-bsd
```

Obnova všech částečných dat do TMZ archivu:

```
wav2tmz damaged_tape.wav --recover --output-format tmz -o rescued.tmz
```

Dekódování s přesnými délkami pulzů do TMZ:

```
wav2tmz recording.wav --output-format tmz --pulse-mode exact -o precise.tmz
```

Přidání dalších nahrávek do existujícího TMZ:

```
wav2tmz side_a.wav -o tape.tmz --output-format tmz
wav2tmz side_b.wav -o tape.tmz --output-format tmz --append-tmz
```

Přepsání existujících MZF souborů:

```
wav2tmz recording.wav -o game.mzf --overwrite-mzf
```
