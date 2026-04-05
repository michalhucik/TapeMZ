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
| `--name-encoding` | ascii, utf8-eu, utf8-jp | Kódování názvu souboru: ascii (výchozí), utf8-eu (evropská Sharp MZ), utf8-jp (japonská Sharp MZ) |

### Podrobnosti k volbám

**--name-encoding** - určuje, jak se zobrazují názvy souborů z MZF hlaviček:
- `ascii` - překlad Sharp MZ znakové sady do ASCII (výchozí, zpětně kompatibilní)
- `utf8-eu` - překlad do UTF-8, evropská varianta znakové sady (zobrazí skutečné Sharp MZ glyfy)
- `utf8-jp` - překlad do UTF-8, japonská varianta znakové sady (katakana místo malých písmen)

### Pojmenování výstupu

- Jeden blok s `--output`: použije se přesný název
- Jeden blok bez `--output`: odvozeno ze vstupu (vstup.tmz -> vstup.mzf)
- Více bloků s `--output`: output_001.mzf, output_002.mzf, ...
- Více bloků bez `--output`: vstup_001.mzf, vstup_002.mzf, ...

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

Výpis bloků s názvy v evropské UTF-8 znakové sadě:

```
tmz2mzf tape.tmz --list --name-encoding utf8-eu
```
