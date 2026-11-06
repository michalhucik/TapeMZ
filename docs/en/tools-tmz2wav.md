# tmz2wav - TMZ/TZX to WAV Converter

Converts a TMZ or TZX file to a WAV audio file (mono, 8-bit PCM).
Uses the TMZ player to play selected (or all) audio blocks and generate
the resulting signal.

Supports all blocks that the TMZ player can play:
- MZ blocks 0x40 (Standard Data), 0x41 (Turbo Data) and 0x45 (BASIC Data): converted
  in memory to block 0x46 (TMZ v1.1); formats that cannot be converted (FSK, SLOW,
  DIRECT and others, see tmzconv `--upgrade`) via the mzcmt_* encoders. 0x41
  CPM-TAPE and 0x45 are always converted (0x45 as `BASIC-1Z016`, more than 254 chunks are not played)
- TZX blocks 0x10-0x15 and 0x20 via the TZX library; a ZX Spectrum (SINCLAIR) recording
  is played as TZX block 0x10/0x11. The player does not play block 0x41 with the
  SINCLAIR format and the tools do not create it
- block 0x46 (MZ File, TMZ v1.1) via the FM and MANCHESTER generators

Control and information blocks (0x21, 0x30, 0x32 etc.) are automatically skipped.

## Usage

```
tmz2wav <input.tmz|input.tzx> <output.wav> [options]
```

## Options

| Option | Value | Default | Description |
|--------|-------|---------|-------------|
| `--rate` | 8000-192000 | 44100 | Sampling frequency in Hz |
| `--pulseset` | 700, 800, 80b | 800 | Default pulse set [ignored by the player] |
| `--blocks` | specification | all | Select blocks to export |
| `--append` | - | - | Append to existing WAV file |
| `--version` | - | - | Show program version |
| `--lib-versions` | - | - | Show library versions |

### Option Details

**--rate** - sampling frequency of the output WAV file. Higher values
mean more accurate signal reproduction but larger files.
Common values: 22050, 44100, 48000.

For block 0x46 the player checks that the read routine can still read
the signal after conversion to samples. A pulse edge moves by up to one
sample, which at a low rate is enough to break fast transfers. If a
block is not readable (e.g. TurboCopy 8:3 or 3:1 at 22050 Hz), no WAV is
saved and the program exits with an error; use a higher `--rate`. For a
margin below 10 us the program only prints a warning and saves the WAV.

**--pulseset** - the program accepts and prints the value (`700`, `800`, `80b`),
but the player does not use it [ignored by the player]. It takes the pulse set from the block.

**--blocks** - select specific blocks to export (indices as shown by tmzinfo).
Specification format:
- `0` - block 0 only
- `0,2` - blocks 0 and 2
- `0-2,5` - blocks 0, 1, 2 and 5

Control blocks (loops, jumps, calls) are always processed - the selection
affects only audio blocks.

**--append** - appends new audio signal to the end of an existing WAV file.
The existing WAV must be mono PCM and its sample rate must match
`--rate` (or the default 44100 Hz). If the file does not exist,
the program reports an error.

### Exit code

The program exits with code 1 on an option or input error, on a block that
cannot be read at the chosen `--rate` (the WAV is then not saved), and also
on a block that failed to play for another reason (e.g. an unsupported TZX
block). In that case the WAV is saved without that block.

## Examples

Basic conversion to WAV (44100 Hz):

```
tmz2wav game.tmz game.wav
```

Conversion with higher quality (48000 Hz):

```
tmz2wav tape.tmz tape.wav --rate 48000
```

Conversion of a TZX file (ZX Spectrum):

```
tmz2wav spectrum.tzx spectrum.wav
```

Conversion with lower sampling frequency (smaller file):

```
tmz2wav tape.tmz tape.wav --rate 22050
```

Export only the first block:

```
tmz2wav game.tmz game_header.wav --blocks 0
```

Export selected blocks (0, 1, 2 and 5):

```
tmz2wav tape.tmz partial.wav --blocks 0-2,5
```

Incremental WAV building block by block:

```
tmz2wav tape.tmz output.wav --blocks 0
tmz2wav tape.tmz output.wav --append --blocks 1
```

Example program output:

```
Input  : game.tmz (TMZ, v1.0, 4 blocks)
Output : game.wav
Rate   : 44100 Hz
Pulseset: MZ-800/1500
Blocks : 0,2

  [  0] 0x40 MZ Standard Data           -> 12.345 s
  [  1] 0x41 MZ Turbo Data                 SKIPPED
  [  2] 0x40 MZ Standard Data           -> 8.123 s
  [  3] 0x41 MZ Turbo Data                 SKIPPED

Audio blocks: 2, Skipped: 2
Total  : 20.468 s (902636 samples, 1024 bytes vstream data)
Saved  : game.wav
```

Example output with append:

```
Input  : game.tmz (TMZ, v1.0, 2 blocks)
Output : game.wav (append)
Rate   : 44100 Hz
Pulseset: MZ-800/1500
Blocks : 1

Existing WAV: 12.345 s, 44100 Hz, 8-bit

  [  0] 0x40 MZ Standard Data              SKIPPED
  [  1] 0x41 MZ Turbo Data              -> 6.789 s

Audio blocks: 1, Skipped: 1
Total  : 19.134 s (844210 samples, 1256 bytes vstream data)
Saved  : game.wav
```
