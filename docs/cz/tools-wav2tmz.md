# wav2tmz - Analyzátor a dekodér WAV nahrávek

Analyzuje WAV soubor obsahující nahrávku magnetofonové kazety
počítačů Sharp MZ (nebo ZX Spectrum).

Implicitně provádí pouze analýzu a výpis nalezených bloků (bez ukládání).
S volbou `-o` nebo `--output-format` extrahuje MZF soubory nebo TMZ archiv.

Automaticky detekuje formát záznamu: NORMAL, TURBO, FASTIPL, BSD,
CPM-CMT, CPM-TAPE, FSK, SLOW, DIRECT, ZX Spectrum. MZ-80B se automaticky
nerozpoznává.

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
| `--tolerance` | 0.02-0.35 | 0.20 | Tolerance detekce leader tónu |
| `--preprocess` | - | zapnuto | Zapnout preprocessing signálu |
| `--no-preprocess` | - | - | Vypnout preprocessing (DC offset, HP filtr, normalizace) |
| `--histogram` | - | vypnuto | Jen vypíše radu použít `--verbose`; histogram délek pulzů vypisuje `--verbose` |
| `--verbose`, `-v` | - | vypnuto | Podrobný výstup (reálná rychlost Bd, přibližná rychlost, pulzní sada) |
| `--channel` | L, R | L | Výběr kanálu ze sterea |
| `--invert` | - | vypnuto | Invertovat polaritu signálu |
| `--keep-unknown` | - | vypnuto | Uložit neidentifikované úseky jako Direct Recording |
| `--raw-format` | direct | direct | Formát pro neidentifikované bloky |
| `--pass` | `<N>` | 1 | Počet průchodů (zatím nepoužito) |
| `--pulse-mode` | approximate, exact | approximate | Režim ukládání délek pulzů pro TMZ výstup |
| `--charset` | eu, jp, utf8-eu, utf8-jp | eu | Znaková sada Sharp MZ pro zobrazení názvu souboru (viz níže) |
| `--recover` | - | vypnuto | Zapnout všechny recovery módy |
| `--recover-bsd` | - | vypnuto | Obnovit nekompletní BSD soubory (chybějící terminátor nebo kusy) |
| `--recover-body` | - | vypnuto | Ponechat neúplné tělo (zbytek doplní 00h), složit tělo FM ze dvou poškozených kopií |
| `--recover-header` | - | vypnuto | Ponechat hlavičku záznamu FM, jejíž tělo chybí (tělo 00h) |
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

**--recover-body** - záznam FM (NORMAL, MZ-80B, CP/M CMT), jehož tělo je
useknuté nebo přemazané dalším záznamem, se neztratí. Přečtené bajty zůstanou,
zbytek do velikosti z hlavičky se doplní 00h, tělo má `Body CRC: ERROR`.
Tón dalšího záznamu se nespotřebuje, takže se další soubor dekóduje normálně.
Z těla se musí přečíst aspoň 1 bajt. Totéž platí pro TURBO, FASTIPL a CP/M TAPE.

Má-li tělo záznamu FM chybu součtu v obou kopiích, wav2tmz ho zkusí složit z platných
úseků obou kopií. Přijme jen výsledek, který jednoznačně souhlasí se součtem z pásky;
tělo pak má `Body CRC: OK` a výpis uvádí `copies merged`. Součet je jen počet jedniček,
takže jde o nejlepší odhad, ne o důkaz.

Chybějící kusy BSD (`--recover-bsd`) se doplní nulami na své místo, kus s chybou součtu
a poškozeným číslem se zařadí podle pořadí.

Liší-li se kopie, ačkoli obě mají platný součet, wav2tmz vždy vypíše varování
`copies differ although the checksum is valid`. Data nemění: součet ROM
převrácení 0 na 1 a 1 na 0 v jedné kopii nepozná.

**--recover-header** - hlavička záznamu FM s platným kontrolním součtem,
jejíž tělo na pásce není, se uloží s tělem z 00h.

Bez těchto voleb se takový soubor zahodí a wav2tmz vypíše (i bez `--verbose`),
co se našlo, kolik bajtů těla se přečetlo a kterou volbu použít. V TMZ výstupu
je před obnovený blok vložen Text Description (0x30), například
`WARNING: Recovered - partial body (700 of 1024 bytes read, rest filled with 00h)`.

**--recover** - zapne všechny recovery módy najednou (--recover-bsd,
--recover-body, --recover-header).

**Poškozený soubor v TMZ:** záznam NORMAL 1:1 s chybou kontrolního součtu se ve výstupu
TMZ uloží blokem 0x46 (profil ROM-800-NOMINAL) i s bajty součtu z pásky.
Obnovené tělo součet nemá, zapíše se záměrně neplatný. Přehrání tak dopadne
jako na poškozené pásce: ROM ohlásí chybu součtu. Bloky 0x41 součet nenesou.

**Oprava velikosti z pásky:** má-li hlavička chybu součtu bez platné kopie
a velikost z hlavičky nesouhlasí se součtem těla, použije se velikost zjištěná
z délky signálu, pokud s ní součet těla souhlasí. Běží vždy, bez volby;
výpis uvádí `fsize repaired from body length`.

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

## Vstupní WAV

wav2tmz čte WAV v PCM (8, 16, 24, 32 a 64 bit), v IEEE float (32 a 64 bit)
i ve formátu WAVE_FORMAT_EXTENSIBLE se SubFormat PCM nebo IEEE float, mono
i stereo (`--channel`). Jiné kódování (A-law, mu-law, komprimované) skončí
chybou s důvodem.

## Výstupní formáty

### Režim MZF (výchozí)

Pro každý dekódovaný soubor se vytvoří samostatný MZF soubor.
ZX Spectrum bloky a bloky bez dat MZF (např. SINCLAIR) se v tomto režimu
přeskakují s varováním (použijte TMZ formát). Pokud se neuloží žádný soubor,
program skončí kódem 1.

### Režim TMZ

Všechny dekódované bloky se uloží do jednoho TMZ souboru:
- Záznam TurboCopy V1.2x (pozná se podle délky tónu: počet pulzů x S je stálý)
  -> blok 0x46 s profilem `TC-1.21`, poměr a/b TurboCopy (všech 34 poměrů 1/2 až 3/1),
  `loader=Y` s předzavaděčem, `loader=N` bez něj
- Záznam Intercopy V10.2 -> blok 0x46: NORMAL 600/2400/2800/3200 (tón hlavičky 11000
  pulzů) -> `IC-10.2-NORMAL`, TURBO (tón těla 5500, čtecí bod předzavaděče) ->
  `IC-10.2-TURBO`, FAST IPL se zavaděčem V07 (Intercopy v7 až V10.2; časování starších
  verzí neověřeno) -> `IC-10.2-FASTIPL`, vždy s rychlostí `bd`
- NORMAL 1:1 -> blok 0x40 (MZ Standard Data)
- NORMAL jiná rychlost -> blok 0x41 (MZ Turbo Data, format=NORMAL); když je odhad
  rychlosti od nejbližší rychlosti bloku 0x41 dál než 5 % a zároveň dál, než je
  rozlišení vzorkování (1 vzorek na půlvlnu), vypíše se varování
- TURBO/FASTIPL bez rozpoznaného zapisovače, FSK/SLOW/DIRECT/CPM-CMT -> blok 0x41; u FSK a SLOW se úroveň
  rychlosti určí z bajtu zpoždění v zavaděči (nepozná-li se, zapíše se 0 s varováním)
- CPM-TAPE -> blok 0x46: varianta Intercopy (koncový bit za checksumem) -> profil `IC-10.2-CPM`,
  jinak `PEZIK-2.0A` s rychlostí 2400 nebo 3200 podle délky bitu. Rychlost Intercopy
  (1500, 2400, 2800, 3200, 3600 Bd) se určí podle délky bitu a zapíše jako `bd`
- BSD -> blok 0x46 s profilem `BASIC-1Z016` (časování BASICu 1Z-016; tón, kopie a pauzy
  zdroje se nezachovají). Soubor s víc než 254 kusy nejde uložit (chyba).
  CRC se ověřuje u každého kusu; při chybě se použije kopie kusu, souhrn vypíše
  `BSD chunks: N, CRC errors: E, from copy: C`.
- ZX Spectrum -> blok 0x10 (Standard Speed Data); pilot mimo standardní rychlost
  (619 µs +- 5 %, např. Intercopy SINCLAIR 900 Bd s pilotem 930 µs) -> blok 0x11
  (Turbo Speed Data) s naměřeným časováním
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

Podrobná analýza s histogramem délek pulzů (vypisuje ho `--verbose`):

```
wav2tmz recording.wav --verbose
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

Ponechání useknutého těla jako MZF:

```
wav2tmz cut_tape.wav --recover-body -o rescued.mzf
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
