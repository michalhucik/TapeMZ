# mzf2tmz - Konvertor MZF/MZT do TMZ

Konvertuje soubory Sharp MZ (MZF) nebo kolekce více MZF souborů (MZT)
do formátu TMZ. Podporuje všechny kazetové formáty a rychlosti.

Při formátu NORMAL a rychlosti 1:1 se používá blok 0x40 (MZ Standard Data).
Při jiném formátu nebo rychlosti se používá blok 0x41 (MZ Turbo Data).

Pokud výstupní TMZ soubor již existuje, nové bloky se přidají na konec
existující pásky. Pokud neexistuje, vytvoří se nový TMZ soubor.

Vstupní soubor s příponou `.mzt` je zpracován jako sekvence více MZF
souborů poskládaných za sebou (každý 128B hlavička + tělo).

## Použití

```
mzf2tmz <vstup.mzf|vstup.mzt> <vystup.tmz> [volby]
```

## Volby

| Volba | Hodnoty | Výchozí | Popis |
|-------|---------|---------|-------|
| `--machine` | generic, mz700, mz800, mz1500, mz80b | mz800 | Cílový počítač |
| `--pulseset` | 700, 800, 80b, auto | auto | Pulzní sada (auto = dle machine) |
| `--format` | normal, turbo, fastipl, fsk, slow, direct, cpm-tape | normal | Formát záznamu na kazetě |
| `--speed` | 1:1, 2:1, 2:1cpm, 3:1, 3:2, 7:3, 8:3, 9:7, 25:14, nebo baudrate (napr. 2800) | 1:1 | Poměr rychlosti nebo baudrate v Bd (neplatný pro FSK/SLOW) |
| `--fsk-speed` | 0-6 | 0 | Rychlostní úroveň FSK (0=nejpomalejší, 6=nejrychlejší; pouze s `--format fsk`) |
| `--slow-speed` | 0-4 | 0 | Rychlostní úroveň SLOW (0=nejpomalejší, 4=nejrychlejší; pouze s `--format slow`) |
| `--pause` | 0-65535 | 1000 | Pauza po bloku v milisekundách (s `--profile` pauza za posledním dílem) |
| `--profile` | ID, list | - | Vytvořit blok 0x46 (TMZ v1.1) podle profilu z registru (viz níže) |
| `--loader` | y, n | y | TC-1.21: s předzavaděčem (y) nebo bez (n) |
| `--gap` | rom, short | podle profilu | `ROM-800-*`, `UNICMT` (i MZT bez `--profile` a bez voleb formátu včetně `--machine`): zaváděcí tóny ROM 22000/11000, nebo poloviční 11000/5500 |
| `--bd` | 600, 1200, 2400, 2800, 3200 | 1200 | Profily Intercopy `IC-10.2-*`: rychlost uložení (popisky Intercopy) |
| `--charset` | eu, jp, utf8-eu, utf8-jp | eu | Znaková sada Sharp MZ pro zobrazení názvu souboru (viz níže) |
| `--version` | - | - | Zobrazit verzi programu |
| `--lib-versions` | - | - | Zobrazit verze použitých knihoven |

### Podrobnosti k volbám

**--machine** - určuje cílový počítač. Ovlivňuje automatickou volbu pulzní sady:
- mz700 -> pulzní sada 700 (MZ-700/80K/80A)
- mz800 -> pulzní sada 800 (MZ-800/1500)
- mz80b -> pulzní sada 80B (MZ-80B)

**--format** - formát záznamu na kazetě:
- `normal` - standardní FM kódování (1200 Bd základ)
- `turbo` - FM s konfigurovatelným časováním a loaderem
- `fastipl` - $BB prefix, Intercopy loader
- `fsk` - Frequency Shift Keying (7 úrovní rychlosti)
- `slow` - 2 bity/pulz (5 úrovní rychlosti)
- `direct` - přímý bitový zápis
- `cpm-tape` - CP/M TAPE (Pezik/MarVan TAPE.COM, Manchester): totéž jako `--profile PEZIK-2.0A`,
  zapíše blok 0x46; rychlost `--bd 2400` (výchozí) nebo `3200`, `--speed` se nepoužívá

`sinclair` mzf2tmz odmítne: záznam ZX Spectrum patří do bloků TZX 0x10/0x11
(`tap2tmz`), blok MZ se SINCLAIR player nepřehraje. Formáty `fsk`, `slow`
a `direct` fungují na MZ-700, MZ-800 a MZ-1500; na MZ-80B nejsou ověřené
a mzf2tmz u nich vypíše varování.

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

**--speed** - poměr rychlosti nebo baudrate (pro FM formáty: normal, turbo, fastipl):
- Formát poměru: `1:1` = 1200 Bd, `2:1` = 2400 Bd, `7:3` = 2800 Bd, `8:3` = 3200 Bd, `3:1` = 3600 Bd atd.
- Formát baudrate: `1200`, `2400`, `2800`, `3200`, `3600` atd. (mapuje se na nejbližší poměr)
- `2:1cpm` = 2400 Bd varianta pro CP/M
- Neplatný pro FSK a SLOW formáty (použijte `--fsk-speed` / `--slow-speed`).
- Pro FASTIPL se doporučuje formát baudrate (odpovídá konvenci Intercopy).
- TURBO používá časování TurboCopy V1.2x a podporuje poměry s číslicemi
  1-9: 1:1, 2:1, 3:2, 7:3, 8:3, 9:7 a 3:1 (3:2 a 9:7 v emulátoru
  [neověřeno]). Poměry 25:14 a 2:1cpm odmítne.
  Při převodu do WAV (`tmz2wav`) se ověřuje, že ROM signál přečte.
  Při 22 050 Hz jsou 8:3 a 3:1 nečitelné a převod skončí chybou.
  Doporučená frekvence je 44 100 Hz nebo vyšší.

**--fsk-speed** - rychlostní úroveň FSK kodéru (pouze s `--format fsk`):
- Úroveň 0 (nejpomalejší): long=8, short=4 vzorků na cyklus
- Úroveň 6 (nejrychlejší): long=3, short=2 vzorků na cyklus
- Vyšší úroveň = rychlejší přenos, nižší spolehlivost

**--slow-speed** - rychlostní úroveň SLOW kodéru (pouze s `--format slow`):
- Úroveň 0 (nejpomalejší): nejdelší pulzy
- Úroveň 4 (nejrychlejší): nejkratší pulzy
- Vyšší úroveň = rychlejší přenos, nižší spolehlivost

**--profile** - vytvoří blok 0x46 MZ File (TMZ v1.1) se seznamem částí a jejich časováním podle profilu zapisovače. `--profile list` vypíše registr. Profily:
- `ROM-800-NOMINAL` - ROM SAVE MZ-800 s nominálními pulzy 240/278 a 470/494 µs, výchozí tóny poloviční (11000/5500).
- `ROM-800-EXACT` - časování ROM SAVE MZ-800 v T-stavech včetně délek podle místa v kódu; v emulátoru shodné se skutečným zápisem ROM. Výchozí tóny ROM (22000/11000).
- `TC-1.21` - zápis programu TurboCopy V1.21. `--speed` 1:1, 2:1, 3:1, 3:2, 7:3, 8:3, 9:7 (výchozí 2:1). S `--loader y` (výchozí) se zapíše předzavaděč, který nahraje ROM; s `--loader n` jen hlavička a tělo (načte je jen TurboCopy).

- `IC-10.2-NORMAL`, `IC-10.2-TURBO`, `IC-10.2-FASTIPL` - zápis programu Intercopy V10.2 (NIPSOFT) v režimu NORMAL, TURBO (předzavaděč TurboCopy, který nahraje ROM) a FAST IPL (zavaděč v hlavičce $BB). Rychlost `--bd` (600 až 3200). Časování je změřené v emulátoru; hlasová návěst se nezapisuje. `IC-10.2-NORMAL` jinou rychlostí než 1200 ROM nepřečte (načte ho jen Intercopy) a přehrávač u něj čitelnost nekontroluje.
- `BASIC-1Z016` - datový soubor BASICu 1Z-016 (`WOPEN#`, časování změřené v emulátoru). Tělo MZF musí být kusy po 258 B (ID + 256 B dat, poslední ID FFFF), 1 až 254 kusů, fsize 0. Běžně ho zapisuje dat2bsd a wav2tmz.
- `IC-10.2-CPM` - Intercopy V10.2 v režimu CPM (Manchester, `--bd` 1500, 2400 (výchozí), 2800, 3200 nebo 3600, časování změřené v emulátoru). Tělo jedním blokem.
- `PEZIK-2.0A` - CP/M program TAPE.COM V2.0a (Pezik/MarVan, Manchester), `--bd 2400` nebo `3200`. Časování spočítané z kódu programu (v emulátoru neověřeno). Tělo nejvýš 16 384 B (TAPE.COM delší soubor dělí na díly s vlastní hlavičkou CP/M); hlavička se zapíše jako MZF hlavička, zda ji TAPE.COM přijme, není ověřené.
- `UNICMT` - přehrávač UniCMT. Struktura jako `ROM-800-NOMINAL`, pulzy podle hlavičky CMTSPEED (výchozí 1x = 470/494/240/278 µs). Strukturu signálu UniCMT zatím nemáme ověřenou měřením.

S `--profile` nelze použít `--format`, `--pulseset`, `--fsk-speed` ani `--slow-speed`. Volby, které zvolený profil nepoužívá, skončí chybou: `--loader` platí jen pro `TC-1.21`, `--gap` pro `ROM-800-*` a `UNICMT`, `--bd` pro `IC-10.2-*`, `--speed` pro `ROM-800-*` (jen 1:1) a `TC-1.21`. `--machine` se uloží do bloku (PROFILE_ARGS, jen informativně). Profil `TAPEMZ-V1-LEGACY` vzniká jen převodem bloků v1.0 (`tmzconv --upgrade`), mzf2tmz ho odmítne. Bajty hlavičky MZF se zapíšou tak, jak jsou v souboru, včetně bajtu 11h (u profilů i bloků v1.0). Soubor s blokem 0x46 má hlavičku verze 1.1 (i když se blok připojí do souboru verze 1.0).

### MZT a hlavičky CMTSPEED (UniCMT)

MZT bez voleb `--format`, `--speed`, `--pulseset`, `--machine` a `--profile` (nebo
s `--profile UNICMT`) se převede na bloky 0x46 s profilem `UNICMT`. Každý
soubor MZF dá jeden blok. Hlavičky CMTSPEED z UniCMT (soubory
`1xspeed.mzf`, `2xspeed.mzf`, `3xspeed.mzf` vložené mezi soubory) určují
rychlost všech dalších souborů; samy blok netvoří.

- Hlavičku CMTSPEED na úplném začátku MZT UniCMT nepřehraje, mzf2tmz ji
  proto s varováním ignoruje.
- Hlavička CMTSPEED na konci MZT bez dalšího souboru vypíše varování.
- Zbytek kratší než hlavička na konci souboru (výplň, např. po přenosu
  přes CP/M) se s varováním ignoruje.
- S výslovnými volbami formátu se MZT bez CMTSPEED převede jako dřív
  (každý soubor podle voleb). MZT s CMTSPEED a jinou volbou než
  `--profile UNICMT` skončí chybou, protože rychlosti by se ztratily.

## Příklady

Základní konverze MZF do TMZ (NORMAL 1:1, blok 0x40):

```
mzf2tmz game.mzf game.tmz
```

Konverze s TURBO formátem a dvojnásobnou rychlostí (blok 0x41):

```
mzf2tmz game.mzf game.tmz --format turbo --speed 2:1
```

Konverze pro MZ-700 s FSK formátem na rychlostní úrovni 3:

```
mzf2tmz program.mzf tape.tmz --machine mz700 --format fsk --fsk-speed 3
```

Konverze s SLOW formátem na maximální rychlosti:

```
mzf2tmz program.mzf tape.tmz --format slow --slow-speed 4
```

Konverze MZT souboru (více MZF):

```
mzf2tmz collection.mzt tape.tmz
```

Přidání dalšího programu do existujícího TMZ:

```
mzf2tmz loader.mzf tape.tmz
mzf2tmz game.mzf tape.tmz --format turbo --speed 2:1
```

Konverze s vlastní pulzní sadou a pauzou:

```
mzf2tmz demo.mzf demo.tmz --pulseset 700 --pause 2000
```

Konverze pro CP/M formát:

```
mzf2tmz cpm.mzf tape.tmz --format cpm-tape --machine mz800
```

Konverze se zobrazením názvů v evropské UTF-8 znakové sadě:

```
mzf2tmz game.mzf game.tmz --charset utf8-eu
```
