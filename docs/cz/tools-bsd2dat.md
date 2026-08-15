# bsd2dat - Export BSD/BRD dat z TMZ

Exportuje data z bloků 0x45 (MZ BASIC Data) v TMZ souboru
do binárního souboru. Podporuje dva režimy exportu:
solid (výchozí) a chunks.

## Použití

```
bsd2dat <vstup.tmz> [volby]
```

## Volby

| Volba | Hodnota | Popis |
|-------|---------|-------|
| `--output` | `<cesta>` | Výstupní soubor (solid) nebo adresář (chunks). Výchozí: odvozeno ze vstupu |
| `--index` | `<N>` | Extrahovat jen blok na indexu N (0-based) |
| `--list` | - | Vypsat BSD bloky bez extrakce |
| `--chunks` | - | Režim chunků - každý chunk jako samostatný soubor |
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

### Režimy exportu

**Solid (výchozí)** - spojí data ze všech datových chunků (kromě terminačního)
do jednoho binárního souboru. Každý chunk přispívá 256 bajtů dat,
poslední chunk může obsahovat padding nulami.

Pojmenování výstupu:
- Jeden blok s `--output`: použije se přesný název
- Jeden blok bez `--output`: vstup.dat
- Více bloků s `--output`: output_001.dat, output_002.dat, ...
- Více bloků bez `--output`: vstup_001.dat, vstup_002.dat, ...

**Chunks (--chunks)** - vytvoří adresář a do něj uloží každý chunk
jako samostatný soubor pojmenovaný `prefix_NNNN.dat`, kde NNNN
je hexadecimální ID chunku. Včetně terminačního chunku (FFFF).

Pojmenování adresáře:
- Jeden blok s `--output`: použije se přesný název
- Jeden blok bez `--output`: vstup_chunks/
- Více bloků: vstup_001_chunks/, vstup_002_chunks/, ...

## Příklady

Výpis BSD bloků v souboru:

```
bsd2dat tape.tmz --list
```

Příklad výstupu `--list`:

```
=== tape.tmz ===

File type: TMZ, Version: 1.0, Blocks: 2

BSD blocks (1):

  [  1] 0x45 MZ BASIC Data  "SAVEGAME"  type=0x03 (BSD (BASIC data))  chunks=4  data=1024 bytes
```

Export BSD dat do jednoho souboru:

```
bsd2dat tape.tmz
```

Export konkrétního bloku s vlastním názvem:

```
bsd2dat tape.tmz --index 1 --output savegame.dat
```

Export jednotlivých chunků do adresáře:

```
bsd2dat tape.tmz --chunks
```

Výsledek: adresář `tape_chunks/` obsahující:
- tape_0000.dat (256 B)
- tape_0001.dat (256 B)
- tape_0002.dat (256 B)
- tape_0003.dat (256 B)
- tape_FFFF.dat (256 B, terminator)

Export chunků s vlastním adresářem:

```
bsd2dat tape.tmz --chunks --output savegame_chunks
```

Výpis BSD bloků s názvy v evropské UTF-8 znakové sadě:

```
bsd2dat tape.tmz --list --charset utf8-eu
```
