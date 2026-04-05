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
| `--format` | normal, turbo, fastipl, sinclair, fsk, slow, direct, cpm-tape | normal | Formát záznamu na kazetě |
| `--speed` | 1:1, 2:1, 2:1cpm, 3:1, 3:2, 7:3, 8:3, 9:7, 25:14 | 1:1 | Poměr rychlosti |
| `--pause` | 0-65535 | 1000 | Pauza po bloku v milisekundách |
| `--name-encoding` | ascii, utf8-eu, utf8-jp | ascii | Kódování názvu souboru: ascii (výchozí), utf8-eu (evropská Sharp MZ), utf8-jp (japonská Sharp MZ) |

### Podrobnosti k volbám

**--machine** - určuje cílový počítač. Ovlivňuje automatickou volbu pulzní sady:
- mz700 -> pulzní sada 700 (MZ-700/80K/80A)
- mz800 -> pulzní sada 800 (MZ-800/1500)
- mz80b -> pulzní sada 80B (MZ-80B)

**--format** - formát záznamu na kazetě:
- `normal` - standardní FM kódování (1200 Bd základ)
- `turbo` - FM s konfigurovatelným časováním a loaderem
- `fastipl` - $BB prefix, Intercopy loader
- `sinclair` - FM varianta Sinclair
- `fsk` - Frequency Shift Keying (7 úrovní rychlosti)
- `slow` - 2 bity/pulz (5 úrovní rychlosti)
- `direct` - přímý bitový zápis
- `cpm-tape` - Manchester kódování (Pezik/MarVan)

**--name-encoding** - určuje, jak se zobrazují názvy souborů z MZF hlaviček:
- `ascii` - překlad Sharp MZ znakové sady do ASCII (výchozí, zpětně kompatibilní)
- `utf8-eu` - překlad do UTF-8, evropská varianta znakové sady (zobrazí skutečné Sharp MZ glyfy)
- `utf8-jp` - překlad do UTF-8, japonská varianta znakové sady (katakana místo malých písmen)

**--speed** - poměr rychlosti oproti základním 1200 Bd:
- `1:1` = 1200 Bd, `2:1` = 2400 Bd, `3:1` = 3600 Bd atd.
- `2:1cpm` = 2400 Bd varianta pro CP/M

## Příklady

Základní konverze MZF do TMZ (NORMAL 1:1, blok 0x40):

```
mzf2tmz game.mzf game.tmz
```

Konverze s TURBO formátem a dvojnásobnou rychlostí (blok 0x41):

```
mzf2tmz game.mzf game.tmz --format turbo --speed 2:1
```

Konverze pro MZ-700 s FSK formátem:

```
mzf2tmz program.mzf tape.tmz --machine mz700 --format fsk
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
mzf2tmz game.mzf game.tmz --name-encoding utf8-eu
```
