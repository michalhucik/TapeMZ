# tmz2mzf - Extraktor MZF souborů z TMZ/TZX

Extrahuje MZF soubory z bloků 0x40 (MZ Standard Data) a 0x41 (MZ Turbo Data)
v TMZ nebo TZX souborech. Podporuje extrakci všech bloků najednou,
výběr konkrétního bloku podle indexu a výpis seznamu extrahovatelných bloků.

## Použití

```
tmz2mzf <vstup.tmz|vstup.tzx> [volby]
```

## Volby

| Volba | Hodnota | Popis |
|-------|---------|-------|
| `--output` | `<soubor>` | Výstupní soubor (výchozí: odvozeno ze vstupu) |
| `--index` | `<N>` | Extrahovat jen blok na indexu N (0-based) |
| `--list` | - | Vypsat extrahovatelné bloky bez extrakce |
| `--overwrite` | - | Povolit přepis existujícího výstupního souboru |
| `--append` | - | Připojit na konec existujícího souboru (multi-MZF) |
| `--charset` | eu, jp, utf8-eu, utf8-jp | Znaková sada Sharp MZ pro zobrazení názvu souboru (viz níže) |
| `--version` | - | Zobrazit verzi programu |
| `--lib-versions` | - | Zobrazit verze použitých knihoven |

### Podrobnosti k volbám

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

### Podrobnosti k dalším volbám

**--overwrite** - ve výchozím režimu program odmítne přepsat existující výstupní soubor.
Tato volba přepis povolí.

**--append** - připojí extrahované MZF bloky na konec existujícího souboru a vytvoří
multi-MZF (MZT-style) soubor se zřetězenými header+body záznamy. Pokud soubor neexistuje,
vytvoří nový. Při extrakci více bloků jdou všechny bloky do jednoho souboru
(místo číslovaných samostatných souborů).

### Pojmenování výstupu

- Jeden blok s `--output`: použije se přesný název
- Jeden blok bez `--output`: odvozeno ze vstupu (vstup.tmz -> vstup.mzf)
- Více bloků s `--output`: output_001.mzf, output_002.mzf, ...
- Více bloků bez `--output`: vstup_001.mzf, vstup_002.mzf, ...
- `--append`: všechny bloky do jednoho souboru (`--output` nebo odvozeno ze vstupu)

## Příklady

Výpis seznamu extrahovatelných bloků:

```
tmz2mzf tape.tmz --list
```

Příklad výstupu `--list`:

```
=== tape.tmz ===

File type: TMZ, Version: 1.0, Blocks: 3

Extractable blocks (2):

  [  0] 0x40 MZ Standard Data          "LOADER"  type=0x01  size=256  load=0x1200  exec=0x1200
  [  1] 0x41 MZ Turbo Data             "GAME"    type=0x01  size=8192  load=0x4000  exec=0x4000
```

Extrakce všech MZF souborů:

```
tmz2mzf tape.tmz
```

Extrakce konkrétního bloku podle indexu:

```
tmz2mzf tape.tmz --index 1
```

Extrakce s vlastním názvem výstupu:

```
tmz2mzf tape.tmz --output game.mzf --index 0
```

Extrakce všech bloků s vlastním prefixem:

```
tmz2mzf tape.tmz --output export.mzf
```

Extrakce všech bloků do jednoho multi-MZF souboru:

```
tmz2mzf tape.tmz --output kolekce.mzf --append
```

Připojení bloku k existujícímu MZF souboru:

```
tmz2mzf tape.tmz --index 0 --output existujici.mzf --append
```

Přepis existujícího výstupního souboru:

```
tmz2mzf tape.tmz --output hra.mzf --index 0 --overwrite
```

Výpis bloků s názvy v evropské UTF-8 znakové sadě:

```
tmz2mzf tape.tmz --list --charset utf8-eu
```
