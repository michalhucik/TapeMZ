# tmz2tap - TAP Extractor from TMZ/TZX

Extracts Standard Speed Data blocks (0x10) from a TMZ or TZX file
and saves them in TAP format (ZX Spectrum).

Only blocks 0x10 are extracted. Other block types (MZ-specific,
control, information) are skipped.

## Usage

```
tmz2tap <input.tmz|input.tzx> <output.tap>
```

## Arguments

| Argument | Description |
|----------|-------------|
| `<input>` | Input TMZ or TZX file |
| `<output>` | Output TAP file |

The program has no optional switches.

## TAP Format

The output TAP file contains a sequence of blocks:
- 2 bytes: data length (little-endian)
- N bytes: data (flag + payload, as stored in TZX block 0x10)

## Examples

Extracting TAP from a ZX Spectrum TZX:

```
tmz2tap spectrum.tzx spectrum.tap
```

Extracting from a TMZ file (if it contains ZX blocks):

```
tmz2tap mixed_tape.tmz zx_part.tap
```

Example program output:

```
Extracting from: spectrum.tzx (TZX, v1.20, 6 blocks)

  [  0] 0x10 Standard Speed Data     19 bytes  flag=0x00 (header)
  [  1] 0x10 Standard Speed Data   6914 bytes  flag=0xFF (data)
  [  2] 0x10 Standard Speed Data     19 bytes  flag=0x00 (header)
  [  3] 0x10 Standard Speed Data    256 bytes  flag=0xFF (data)

Extracted 4 TAP block(s), 7216 bytes total.
Output: spectrum.tap
```
