# tmz2tap - Extraktor TAP z TMZ/TZX

Extrahuje bloky Standard Speed Data (0x10) z TMZ nebo TZX souboru
a uloží je ve formátu TAP (ZX Spectrum).

Extrahovány jsou pouze bloky 0x10. Ostatní typy bloků (MZ-specifické,
řídicí, informační) jsou přeskočeny.

## Použití

```
tmz2tap <vstup.tmz|vstup.tzx> <vystup.tap>
```

## Argumenty

| Argument | Popis |
|----------|-------|
| `<vstup>` | Vstupní TMZ nebo TZX soubor |
| `<vystup>` | Výstupní TAP soubor |

Program nemá žádné volitelné přepínače.

## TAP formát

Výstupní TAP soubor obsahuje sekvenci bloků:
- 2 bajty: délka dat (little-endian)
- N bajtů: data (flag + payload, jak je uloženo v TZX bloku 0x10)

## Příklady

Extrakce TAP ze ZX Spectrum TZX:

```
tmz2tap spectrum.tzx spectrum.tap
```

Extrakce z TMZ souboru (pokud obsahuje ZX bloky):

```
tmz2tap mixed_tape.tmz zx_part.tap
```

Příklad výstupu programu:

```
Extracting from: spectrum.tzx (TZX, v1.20, 6 blocks)

  [  0] 0x10 Standard Speed Data     19 bytes  flag=0x00 (header)
  [  1] 0x10 Standard Speed Data   6914 bytes  flag=0xFF (data)
  [  2] 0x10 Standard Speed Data     19 bytes  flag=0x00 (header)
  [  3] 0x10 Standard Speed Data    256 bytes  flag=0xFF (data)

Extracted 4 TAP block(s), 7216 bytes total.
Output: spectrum.tap
```
