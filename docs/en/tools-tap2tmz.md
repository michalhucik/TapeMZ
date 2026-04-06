# tap2tmz - ZX Spectrum TAP to TZX/TMZ Converter

Converts a ZX Spectrum TAP file to TZX or TMZ format.
Each TAP block is converted to TZX block 0x10 (Standard Speed Data).

If the output file already exists, new blocks are appended
to the end of the existing file. If it does not exist, a new file is created.

The output file is TZX by default (signature "ZXTape!").
The `--tmz` switch changes to TMZ signature ("TapeMZ!").

## Usage

```
tap2tmz <input.tap> <output.tzx|output.tmz> [options]
```

## Options

| Option | Value | Default | Description |
|--------|-------|---------|-------------|
| `--pause` | 0-65535 | 1000 | Pause after each block in milliseconds |
| `--tmz` | - | off | Use TMZ signature ("TapeMZ!") instead of TZX |
| `--version` | - | - | Show program version |
| `--lib-versions` | - | - | Show library versions |

## TAP Format

A TAP file is a sequence of blocks. Each block has the structure:
- 2 bytes: data length (little-endian)
- N bytes: data (starting with flag byte: 0x00 = header, 0xFF = data block)

## Examples

Basic TAP to TZX conversion:

```
tap2tmz game.tap game.tzx
```

Conversion with TMZ signature:

```
tap2tmz game.tap game.tmz --tmz
```

Conversion with shorter pause between blocks:

```
tap2tmz game.tap game.tzx --pause 500
```

Appending another TAP file to an existing TZX:

```
tap2tmz loader.tap tape.tzx
tap2tmz game.tap tape.tzx
```

Example program output:

```
Converted: game.tap -> game.tzx

  Format: TZX (ZXTape!) v1.20
  Blocks: 4 (new: 4)
  Pause : 1000 ms

  [  0] 0x10 Standard Speed Data     19 bytes  flag=0x00 (header)
  [  1] 0x10 Standard Speed Data   6914 bytes  flag=0xFF (data)
  [  2] 0x10 Standard Speed Data     19 bytes  flag=0x00 (header)
  [  3] 0x10 Standard Speed Data    256 bytes  flag=0xFF (data)
```
