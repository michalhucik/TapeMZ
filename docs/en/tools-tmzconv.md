# tmzconv - TMZ/TZX Signature Converter

Converts a file between TMZ and TZX formats by rewriting the header signature.
Blocks and data remain unchanged - only the signature
("TapeMZ!" <-> "ZXTape!") and version number are modified.

Without an explicit switch, it automatically converts to the opposite format:
TMZ -> TZX and TZX -> TMZ.

## Usage

```
tmzconv [--to-tmz|--to-tzx] <input> <output>
```

## Options

| Option | Description |
|--------|-------------|
| `--to-tmz` | Force conversion to TMZ (signature "TapeMZ!", version 1.0) |
| `--to-tzx` | Force conversion to TZX (signature "ZXTape!", version 1.20) |

If neither `--to-tmz` nor `--to-tzx` is specified, the conversion direction
is determined automatically based on the input file type.

## Examples

Automatic TZX -> TMZ conversion:

```
tmzconv spectrum.tzx spectrum.tmz
```

Automatic TMZ -> TZX conversion:

```
tmzconv game.tmz game.tzx
```

Explicit conversion to TMZ:

```
tmzconv --to-tmz input.tzx output.tmz
```

Explicit conversion to TZX:

```
tmzconv --to-tzx input.tmz output.tzx
```

Example output:

```
Converted: game.tzx -> game.tmz
  TZX (ZXTape!) -> TMZ (TapeMZ!) v1.0
  Blocks: 3
```
