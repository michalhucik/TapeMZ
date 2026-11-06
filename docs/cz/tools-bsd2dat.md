# bsd2dat - Export BSD/BRD dat z TMZ

Exportuje data datových souborů BASICu z TMZ souboru do binárního
souboru. Čte bloky 0x45 (MZ BASIC Data, TMZ v1.0) i bloky 0x46 s částmi
kusů BSD (profil `BASIC-1Z016`, který zapisuje dat2bsd a wav2tmz).
Podporuje dva režimy exportu: solid (výchozí) a chunks.

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

**Solid (výchozí)** - spojí data ze všech kusů včetně posledního (ID 0xFFFF
nese data jako ostatní) do jednoho binárního souboru. Každý kus přispívá
256 bajtů dat, poslední kus může obsahovat padding nulami.

Pojmenování výstupu (bez `--output` leží výstup vedle vstupního souboru,
ne v aktuálním adresáři):
- Jeden blok s `--output`: použije se přesný název
- Jeden blok bez `--output`: vstup.dat
- Více bloků s `--output`: output_001.dat, output_002.dat, ... (v adresáři `--output`)
- Více bloků bez `--output`: vstup_001.dat, vstup_002.dat, ...

**Chunks (--chunks)** - vytvoří adresář a do něj uloží každý chunk
jako samostatný soubor pojmenovaný `prefix_NNNN.dat`, kde NNNN
je hexadecimální ID kusu, včetně posledního kusu (FFFF).

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

File type: TMZ, Version: 1.1, Blocks: 2

BSD blocks (2):

  [  0] 0x45 MZ BASIC Data  "OLDDATA"  type=0x03 (BSD (BASIC data))  chunks=2  data=512 bytes
  [  1] 0x46 MZ File  "SAVEGAME"  type=0x04 (BRD (BASIC data, WOPEN#))  chunks=4  data=1024 bytes
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
- tape_FFFF.dat (256 B, poslední kus)

Export chunků s vlastním adresářem:

```
bsd2dat tape.tmz --chunks --output savegame_chunks
```

Výpis BSD bloků s názvy v evropské UTF-8 znakové sadě:

```
bsd2dat tape.tmz --list --charset utf8-eu
```
