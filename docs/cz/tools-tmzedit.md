# tmzedit - Editor TMZ/TZX souborů

Univerzální nástroj pro manipulaci s bloky v TMZ/TZX souborech.
Podporuje výpis, hex dump, odebírání, přesun, spojování, rozdělování,
přidávání metadat, změnu parametrů MZ bloků a validaci integrity.

## Použití

```
tmzedit <prikaz> [volby] <soubor> [argumenty...]
```

## Příkazy

| Příkaz | Popis |
|--------|-------|
| `list` | Vypíše přehled všech bloků v souboru |
| `dump` | Zobrazí hex dump dat konkrétního bloku |
| `remove` | Odebere blok na daném indexu |
| `move` | Přesune blok z jedné pozice na druhou |
| `merge` | Spojí více souborů do jednoho |
| `split` | Rozdělí soubor na jednotlivé části |
| `add-text` | Přidá textový popis (blok 0x30) |
| `add-message` | Přidá zobrazovanou zprávu (blok 0x31) |
| `archive-info` | Přidá nebo nahradí metadata (blok 0x32) |
| `set` | Změní formát/rychlost/pulzy na MZ bloku (0x40/0x41) nebo časování na bloku 0x11 |
| `validate` | Zkontroluje integritu souboru |

## Společné volby

| Volba | Hodnoty | Výchozí | Popis |
|-------|---------|---------|-------|
| `-o` | `<soubor>` | přepsat vstup | Výstupní soubor |
| `--charset` | eu, jp, utf8-eu, utf8-jp | eu | Znaková sada Sharp MZ pro zobrazení názvu souboru (viz níže) |
| `--dump-charset` | raw, eu, jp, utf8-eu, utf8-jp | raw | Znaková sada textového sloupce hex dumpu (jen `dump`, viz níže) |
| `--version` | - | - | Zobrazit verzi programu |
| `--lib-versions` | - | - | Zobrazit verze použitých knihoven |

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

---

## list - Výpis bloků

Zobrazí stručný přehled všech bloků včetně indexu, ID, názvu, velikosti
a základních informací (název programu, text apod.).

```
tmzedit list <soubor>
```

### Příklad

```
tmzedit list game.tmz
```

Výstup:

```
File: game.tmz
Type: TMZ (TapeMZ!), version 1.0, 3 block(s)

  [  0] 0x32  Archive Info               42 B  (2 entries)
  [  1] 0x40  MZ Standard Data         4267 B  [MZ]  "LOADER"
  [  2] 0x41  MZ Turbo Data            8305 B  [MZ]  "GAME"

Total: 3 block(s)
```

---

## dump - Hex dump bloku

Zobrazí hexadecimální dump dat bloku na zadaném indexu.
Formát: 16 bajtů na řádek s offsetem, hex hodnotami a textovým sloupcem.

```
tmzedit dump [--dump-charset <režim>] <soubor> <index>
```

**--dump-charset** - určuje, jak se bajty interpretují v textovém sloupci:

- **raw** (výchozí) - standardní ASCII: bajty 0x20-0x7E se zobrazí jako znak,
  ostatní jako `.`. Vhodné pro strojový kód a data, která nejsou textem.
- **eu** / **jp** - bajty se berou jako Sharp MZ ASCII (evropská / japonská
  varianta) a konvertují do ASCII. Bajt, který nemá ASCII ekvivalent nebo není
  tisknutelný, se zobrazí jako `.`.
- **utf8-eu** / **utf8-jp** - stejné jako výše, ale výstup v UTF-8 (zobrazí se
  i přehlásky, šipky, karetní symboly, katakana apod.).

Data MZ bloků (0x40, 0x41) začínají 128bajtovou MZF hlavičkou, ve které je jméno
souboru v Sharp MZ ASCII - v režimu `raw` se malá písmena zobrazí jako `.`, v režimu
`eu` správně.

### Příklad

```
tmzedit dump game.tmz 1
tmzedit dump --dump-charset eu game.tmz 1
tmzedit dump --dump-charset utf8-eu game.tmz 1
```

---

## remove - Odebírání bloků

Odebere blok na zadaném indexu a uloží výsledek.
Pokud není specifikováno `-o`, přepíše vstupní soubor (in-place).

```
tmzedit remove <soubor> <index> [-o <vystup>]
```

### Příklad

```
tmzedit remove tape.tmz 0 -o tape_cleaned.tmz
```

---

## move - Přesun bloku

Přesune blok z pozice `<odkud>` na pozici `<kam>`.
Indexy jsou 0-based.

```
tmzedit move <soubor> <odkud> <kam> [-o <vystup>]
```

### Příklad

Přesun bloku z pozice 2 na pozici 0:

```
tmzedit move tape.tmz 2 0 -o tape_reordered.tmz
```

---

## merge - Spojení souborů

Spojí bloky z více TMZ/TZX souborů do jednoho výstupního souboru.
Hlavička výstupu odpovídá prvnímu souboru.

```
tmzedit merge <soubor1> <soubor2> [<soubor3> ...] -o <vystup>
```

### Příklad

```
tmzedit merge side_a.tmz side_b.tmz -o full_tape.tmz
```

---

## split - Rozdělení souboru

Rozdělí soubor na jednotlivé části. Pokud soubor obsahuje
Group Start (0x21) / Group End (0x22) bloky, rozděluje podle skupin.
Jinak každý datový blok tvoří samostatný soubor.

Výstupní soubory: `<prefix>_000.tmz`, `<prefix>_001.tmz`, ...

```
tmzedit split <soubor> [-o <prefix>]
```

### Příklad

```
tmzedit split collection.tmz -o game
```

Výsledek: game_000.tmz, game_001.tmz, ...

Bez `-o` se prefix odvodí ze vstupu:

```
tmzedit split collection.tmz
```

Výsledek: collection_000.tmz, collection_001.tmz, ...

---

## add-text - Přidání textového popisu

Přidá blok 0x30 (Text Description) na konec souboru.

```
tmzedit add-text <soubor> --text "<text>" [-o <vystup>]
```

### Příklad

```
tmzedit add-text game.tmz --text "Sharp MZ-800 game collection" -o game.tmz
```

---

## add-message - Přidání zprávy

Přidá blok 0x31 (Message) na konec souboru. Zpráva se zobrazuje
při přehrávání pásky (např. v emulátoru).

```
tmzedit add-message <soubor> --text "<text>" [--time <sekundy>] [-o <vystup>]
```

| Volba | Výchozí | Popis |
|-------|---------|-------|
| `--text` | (povinné) | Text zprávy |
| `--time` | 5 | Doba zobrazení v sekundách (0-255) |

### Příklad

```
tmzedit add-message tape.tmz --text "Press PLAY on tape" --time 10 -o tape.tmz
```

---

## archive-info - Metadata archivu

Přidá nebo nahradí blok 0x32 (Archive Info) s metadaty.
Pokud soubor již obsahuje blok 0x32, nový ho nahradí.
Nový blok se vloží na začátek souboru (index 0).

```
tmzedit archive-info <soubor> [--title <X>] [--publisher <X>] [--author <X>]
    [--year <X>] [--language <X>] [--type <X>] [--price <X>]
    [--protection <X>] [--origin <X>] [--comment <X>] [-o <vystup>]
```

| Volba | TZX ID | Popis |
|-------|--------|-------|
| `--title` | 0x00 | Název programu |
| `--publisher` | 0x01 | Vydavatel |
| `--author` | 0x02 | Autor(i) |
| `--year` | 0x03 | Rok vydání |
| `--language` | 0x04 | Jazyk |
| `--type` | 0x05 | Typ (hra, utilita, ...) |
| `--price` | 0x06 | Cena |
| `--protection` | 0x07 | Ochrana proti kopírování |
| `--origin` | 0x08 | Původ |
| `--comment` | 0xFF | Komentář |

### Příklad

```
tmzedit archive-info game.tmz --title "Space Invaders" --author "Taito" --year "1985" -o game.tmz
```

Doplnění komentáře:

```
tmzedit archive-info tape.tmz --comment "Dumped from original tape" -o tape.tmz
```

---

## set - Změna formátu/rychlosti/pulzů MZ bloků

Změní formát, rychlost a/nebo délky pulzů záznamu na MZ bloku (0x40 nebo 0x41),
nebo časování na bloku 0x11 (Turbo Speed Data / SINCLAIR).

Pravidla konverze:
- Blok 0x40 s nestandardním formátem/rychlostí/pulzy se překonvertuje na blok 0x41
- Blok 0x41 s formátem NORMAL a rychlostí 1:1 (bez custom pulzů) se překonvertuje zpět na blok 0x40
- Blok 0x40 s NORMAL 1:1 zůstává beze změny
- Blok 0x11 podporuje pouze `--sinclair-speed`
- `--pulse` nastaví custom režim (speed=0, pulse pole vyplněna)
- `--speed` vynuluje pulse pole (zpět do tabulkového režimu)

```
tmzedit set <soubor> <index> [--format <fmt>] [--speed <spd>] [--pulse <hodnoty>] [-o <vystup>]
```

| Volba | Hodnoty | Popis |
|-------|---------|-------|
| `--format` | normal, turbo, fastipl, sinclair, fsk, slow, direct, cpm-tape | Formát záznamu |
| `--speed` | 1:1, 2:1, 2:1cpm, 3:1, 3:2, 7:3, 8:3, 9:7, 25:14, nebo baudrate (napr. 2800) | Poměr rychlosti nebo baudrate v Bd (neplatný pro FSK/SLOW/SINCLAIR) |
| `--fsk-speed` | 0-6 | Rychlostní úroveň FSK (pouze pro FSK formát) |
| `--slow-speed` | 0-4 | Rychlostní úroveň SLOW (pouze pro SLOW formát) |
| `--sinclair-speed` | 1381, 1772, 2074, 2487 | SINCLAIR rychlost v Bd (pouze pro blok 0x11) |
| `--pulse` | long_h/long_l,short_h/short_l | Vlastní délky pulzů v us*100 jednotkách |

### Příklady

Změna bloku 0x40 na TURBO 2:1 (konvertuje na 0x41):

```
tmzedit set tape.tmz 0 --format turbo --speed 2:1 -o tape_turbo.tmz
```

Změna rychlosti existujícího bloku 0x41:

```
tmzedit set tape.tmz 1 --speed 3:1 -o tape_fast.tmz
```

Změna FSK bloku na rychlostní úroveň 5:

```
tmzedit set tape.tmz 0 --fsk-speed 5 -o tape_fast_fsk.tmz
```

Konverze bloku na SLOW formát s rychlostní úrovní 2:

```
tmzedit set tape.tmz 0 --format slow --slow-speed 2 -o tape_slow.tmz
```

Změna rychlosti SINCLAIR bloku (0x11) na 2074 Bd:

```
tmzedit set tape.tmz 3 --sinclair-speed 2074 -o tape_sinclair.tmz
```

Nastavení vlastních délek pulzů na bloku (konvertuje 0x40 na 0x41):

```
tmzedit set tape.tmz 0 --pulse 4980/4980,2490/2490 -o tape_custom.tmz
```

Přepnutí bloku s custom pulzy zpět do tabulkového režimu:

```
tmzedit set tape.tmz 0 --speed 2:1 -o tape_table.tmz
```

Konverze bloku 0x41 zpět na 0x40 (nastavením NORMAL 1:1):

```
tmzedit set tape.tmz 0 --format normal --speed 1:1 -o tape_normal.tmz
```

---

## validate - Validace integrity

Zkontroluje integritu TMZ/TZX souboru:
- Platnost signatury a verze
- Známé blokové ID
- Párovanost Group Start / Group End
- Párovanost Loop Start / Loop End
- MZF hlavičky v MZ blocích (ftype, fsize vs body_size)

Návratový kód: 0 = validní, 1 = nevalidní (chyby).

```
tmzedit validate <soubor>
```

### Příklad

```
tmzedit validate game.tmz
```

Výstup:

```
Validating: game.tmz
  Type: TMZ, version 1.0

  Blocks: 3, Errors: 0, Warnings: 0
  Result: VALID
```

Příklad s chybami:

```
Validating: broken.tmz
  Type: TMZ, version 1.0
  ERROR: block [2] nested Group Start (depth 2)
  WARNING: block [5] MZF fsize=4096 but body_size=4000

  Blocks: 6, Errors: 1, Warnings: 1
  Result: INVALID
```

---

## Příklad použití --charset

Výpis bloků s názvy v evropské UTF-8 znakové sadě:

```
tmzedit list game.tmz --charset utf8-eu
```
