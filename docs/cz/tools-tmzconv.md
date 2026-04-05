# tmzconv - Konvertor signatury TMZ/TZX

Konvertuje soubor mezi formáty TMZ a TZX přepsáním signatury v hlavičce.
Bloky a data zůstávají beze změny - mění se pouze signatura
("TapeMZ!" <-> "ZXTape!") a číslo verze.

Bez explicitního přepínače automaticky konvertuje na opačný formát:
TMZ -> TZX a TZX -> TMZ.

## Použití

```
tmzconv [--to-tmz|--to-tzx] <vstup> <vystup>
```

## Volby

| Volba | Popis |
|-------|-------|
| `--to-tmz` | Vynutit konverzi na TMZ (signatura "TapeMZ!", verze 1.0) |
| `--to-tzx` | Vynutit konverzi na TZX (signatura "ZXTape!", verze 1.20) |

Pokud není specifikován ani `--to-tmz` ani `--to-tzx`, směr konverze
se určuje automaticky podle typu vstupního souboru.

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
  TZX (ZXTape!) -> TMZ (TapeMZ!) v1.0
  Blocks: 3
```
