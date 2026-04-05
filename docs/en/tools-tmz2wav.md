# tmz2wav - TMZ/TZX to WAV Converter

Converts a TMZ or TZX file to a WAV audio file (mono, 8-bit PCM).
Uses the TMZ player to play all audio blocks and generate
the resulting signal.

Supports all blocks that the TMZ player can play:
- MZ blocks 0x40 (Standard Data) and 0x41 (Turbo Data) via mztape encoders
- TZX blocks 0x10-0x15 and 0x20 via the TZX library

Control and information blocks (0x21, 0x30, 0x32 etc.) are automatically skipped.

## Usage

```
tmz2wav <input.tmz|input.tzx> <output.wav> [options]
```

## Options

| Option | Value | Default | Description |
|--------|-------|---------|-------------|
| `--rate` | 8000-192000 | 44100 | Sampling frequency in Hz |
| `--pulseset` | 700, 800, 80b | 800 | Default pulse set |

### Option Details

**--rate** - sampling frequency of the output WAV file. Higher values
mean more accurate signal reproduction but larger files.
Common values: 22050, 44100, 48000.

**--pulseset** - default pulse set for blocks that do not have one
explicitly specified. Affects pulse timing:
- `700` - MZ-700/80K/80A timing
- `800` - MZ-800/1500 timing
- `80b` - MZ-80B timing

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

Conversion with MZ-700 pulse set:

```
tmz2wav old_tape.tmz old_tape.wav --pulseset 700
```

Conversion with lower sampling frequency (smaller file):

```
tmz2wav tape.tmz tape.wav --rate 22050
```

Example program output:

```
Input  : game.tmz (TMZ, v1.0, 2 blocks)
Output : game.wav
Rate   : 44100 Hz
Pulseset: MZ-800/1500

  [  0] 0x40 MZ Standard Data           -> 12.345 s
  [  1] 0x41 MZ Turbo Data              -> 6.789 s

Audio blocks: 2
Total  : 19.134 s (844210 samples, 1256 bytes vstream data)
Saved  : game.wav
```
