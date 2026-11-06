# tmzconv - TMZ/TZX Signature Converter and TMZ v1.0 to v1.1 Upgrade

Converts a file between TMZ and TZX formats by rewriting the header signature.
Blocks and data remain unchanged - only the signature
("TapeMZ!" <-> "ZXTape!") and version number are modified.

Without an explicit switch, it automatically converts to the opposite format:
TMZ -> TZX and TZX -> TMZ.

With `--upgrade`, TMZ v1.0 blocks are converted to block 0x46 (TMZ v1.1)
and the file is written as TMZ version 1.1.

## Usage

```
tmzconv [--to-tmz|--to-tzx|--upgrade] <input> <output>
```

## Options

| Option | Description |
|--------|-------------|
| `--to-tmz` | Force conversion to TMZ (signature "TapeMZ!", current format version) |
| `--to-tzx` | Force conversion to TZX (signature "ZXTape!", version 1.20) |
| `--upgrade` | Convert v1.0 blocks to 0x46 and write TMZ v1.1 (cannot be combined with `--to-tzx`) |
| `--version` | Show program version |
| `--lib-versions` | Show library versions |

If neither `--to-tmz` nor `--to-tzx` is specified, the conversion direction
is determined automatically based on the input file type.

## TMZ v1.0 to v1.1 upgrade (`--upgrade`)

Blocks 0x40 (MZ Standard Data), 0x41 (MZ Turbo Data, formats NORMAL
at 1:1, TURBO, FASTIPL at 1:1, 2:1, 7:3 or 8:3 and CPM-TAPE) and 0x45 (MZ BASIC Data) are converted to block 0x46
with a writer profile (the mapping is described in `tmz-v1.1-specification.md` chapter 9.2):

| v1.0 block | profile |
|---|---|
| 0x40, MZ-800 pulse set | `ROM-800-NOMINAL` |
| 0x40, MZ-700 or MZ-80B pulse set | `TAPEMZ-V1-LEGACY` |
| 0x41 TURBO | `TC-1.21` (TurboCopy V1.21 with preloader) |
| 0x41 NORMAL 1:1 | `TAPEMZ-V1-LEGACY` |
| 0x41 FASTIPL 1:1, 2:1, 7:3, 8:3 | `IC-10.2-FASTIPL` (Intercopy V10.2, 1200 to 3200 Bd) |
| 0x41 CPM-TAPE | `PEZIK-2.0A`, level 2 = 3200 Bd, otherwise 2400 Bd |
| 0x45 (at most 254 chunks) | `BASIC-1Z016` (BASIC 1Z-016, timing measured in the emulator) |

Compared with playing the v1.0 file in earlier versions, the signal
changes:

- 0x40 with the MZ-800 pulse set uses the MZ-800 ROM timing;
- 0x40 with the MZ-700 or MZ-80B pulse set plays the pulses from the
  block field (previously always MZ-800);
- TURBO plays the tones, pulses and pauses measured on TurboCopy;
- FASTIPL plays the Intercopy V10.2 timing measured in the emulator;
- 0x45 plays the BASIC 1Z-016 timing with a tone and a copy for every chunk
  (BASIC did not read the earlier output past the first chunk);
- the pause after a block (block field) is now played.

Blocks that cannot be converted stay unchanged and play as before. This
covers FASTIPL at other speeds, the FSK, SLOW and DIRECT formats, TURBO with
custom pulses or a body copy and NORMAL at speeds other than 1:1. Block 0x45 with more
than 254 chunks is neither converted nor played (block error). Blocks 0x42 and 0x44 are
kept with a warning.

If a block cannot be converted because it is damaged (or it is 0x41 CPM-TAPE with a
body longer than 16384 B), no output file is
written and the program exits with an error.

```
tmzconv --upgrade game.tmz game11.tmz
Upgraded: 2 block(s) converted to 0x46, 0 kept as v1.0, 0 error(s)
Converted: game.tmz -> game11.tmz
  -> TMZ (TapeMZ!) v1.1
  Blocks: 2
```

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
  -> TMZ (TapeMZ!) v1.1
  Blocks: 3
```
