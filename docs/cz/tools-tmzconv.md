# tmzconv - Konvertor signatury TMZ/TZX a převod TMZ v1.0 na v1.1

Konvertuje soubor mezi formáty TMZ a TZX přepsáním signatury v hlavičce.
Bloky a data zůstávají beze změny - mění se pouze signatura
("TapeMZ!" <-> "ZXTape!") a číslo verze.

Bez explicitního přepínače automaticky konvertuje na opačný formát:
TMZ -> TZX a TZX -> TMZ.

S volbou `--upgrade` převede bloky TMZ v1.0 na blok 0x46 (TMZ v1.1)
a zapíše soubor TMZ verze 1.1.

## Použití

```
tmzconv [--to-tmz|--to-tzx|--upgrade] <vstup> <vystup>
```

## Volby

| Volba | Popis |
|-------|-------|
| `--to-tmz` | Vynutit konverzi na TMZ (signatura "TapeMZ!", aktuální verze formátu) |
| `--to-tzx` | Vynutit konverzi na TZX (signatura "ZXTape!", verze 1.20) |
| `--upgrade` | Převést bloky v1.0 na 0x46 a zapsat TMZ v1.1 (nelze kombinovat s `--to-tzx`) |
| `--version` | Zobrazit verzi programu |
| `--lib-versions` | Zobrazit verze použitých knihoven |

Pokud není specifikován ani `--to-tmz` ani `--to-tzx`, směr konverze
se určuje automaticky podle typu vstupního souboru.

## Převod TMZ v1.0 na v1.1 (`--upgrade`)

Převádějí se bloky 0x40 (MZ Standard Data), 0x41 (MZ Turbo Data, formáty
NORMAL rychlostí 1:1, TURBO, FASTIPL rychlostí 1:1, 2:1, 7:3 nebo 8:3
a CPM-TAPE) a 0x45 (MZ BASIC Data). Výsledkem je blok 0x46
s profilem zapisovače (mapování popisuje `tmz-v1.1-specification.md` kap. 9.2):

| blok v1.0 | profil |
|---|---|
| 0x40, pulzní sada MZ-800 | `ROM-800-NOMINAL` |
| 0x40, pulzní sada MZ-700 nebo MZ-80B | `TAPEMZ-V1-LEGACY` |
| 0x41 TURBO | `TC-1.21` (TurboCopy V1.21 s předzavaděčem) |
| 0x41 NORMAL 1:1 | `TAPEMZ-V1-LEGACY` |
| 0x41 FASTIPL 1:1, 2:1, 7:3, 8:3 | `IC-10.2-FASTIPL` (Intercopy V10.2, 1200 až 3200 Bd) |
| 0x41 CPM-TAPE | `PEZIK-2.0A`, úroveň 2 = 3200 Bd, jinak 2400 Bd |
| 0x45 (nejvýš 254 kusů) | `BASIC-1Z016` (BASIC 1Z-016, časování změřené v emulátoru) |

Signál se proti přehrání souboru v1.0 v předchozích verzích mění:

- 0x40 se sadou MZ-800 se zapisuje časováním ROM MZ-800;
- 0x40 se sadou MZ-700 nebo MZ-80B hraje pulzy podle pole bloku (dřív
  vždy MZ-800);
- TURBO hraje tóny, pulzy a pauzy změřené na TurboCopy;
- FASTIPL hraje časování Intercopy V10.2 změřené v emulátoru;
- 0x45 hraje časování BASICu 1Z-016 s tónem a kopií u každého kusu
  (dřívější výstup BASIC za prvním kusem nepřečetl);
- pauza po bloku (pole bloku) se nově přehraje.

Bloky, které převést nejde, zůstanou beze změny a přehrávají se jako
dřív. Patří sem FASTIPL jinou rychlostí, formáty FSK, SLOW a DIRECT, TURBO
s vlastními pulzy nebo kopií těla a NORMAL jinou rychlostí než 1:1. Blok 0x45 s víc
než 254 kusy se nepřevede a nepřehraje (chyba bloku). Bloky 0x42 a 0x44 zůstanou
s varováním.

Když převod některého bloku selže (poškozený blok, 0x41 CPM-TAPE s tělem
delším než 16384 B), výstupní soubor
se nezapíše a program skončí chybou.

```
tmzconv --upgrade game.tmz game11.tmz
Upgraded: 2 block(s) converted to 0x46, 0 kept as v1.0, 0 error(s)
Converted: game.tmz -> game11.tmz
  -> TMZ (TapeMZ!) v1.1
  Blocks: 2
```

## Příklady

Automatická konverze TZX -> TMZ:

```
tmzconv spectrum.tzx spectrum.tmz
```

Automatická konverze TMZ -> TZX:

```
tmzconv game.tmz game.tzx
```

Explicitní konverze na TMZ:

```
tmzconv --to-tmz input.tzx output.tmz
```

Explicitní konverze na TZX:

```
tmzconv --to-tzx input.tmz output.tzx
```

Příklad výstupu:

```
Converted: game.tzx -> game.tmz
  -> TMZ (TapeMZ!) v1.1
  Blocks: 3
```
