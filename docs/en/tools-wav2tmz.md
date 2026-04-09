# wav2tmz - WAV Recording Analyzer and Decoder

Analyzes a WAV file containing a cassette tape recording from
Sharp MZ (or ZX Spectrum) computers.

By default, performs analysis only and prints discovered blocks (no files saved).
With `-o` or `--output-format`, extracts MZF files or a TMZ archive.

Automatically detects the recording format: NORMAL, TURBO, FASTIPL, BSD,
CPM-CMT, CPM-TAPE, MZ-80B, FSK, SLOW, DIRECT, ZX Spectrum.

## Usage

```
wav2tmz input.wav [-o output] [options]
```

## Options

| Option | Value | Default | Description |
|--------|-------|---------|-------------|
| `-o` | `<file>` | derived from input | Output file (enables saving) |
| `--output-format` | mzf, tmz | mzf | Output format (enables saving) |
| `--append-tmz` | - | off | Append blocks to existing TMZ (without this, existing TMZ is an error) |
| `--overwrite-mzf` | - | off | Overwrite existing MZF files (without this, existing MZF is an error) |
| `--schmitt` | - | off | Use Schmitt trigger instead of zero-crossing |
| `--tolerance` | 0.02-0.35 | 0.10 | Leader tone detection tolerance |
| `--preprocess` | - | on | Enable signal preprocessing |
| `--no-preprocess` | - | - | Disable preprocessing (DC offset, HP filter, normalization) |
| `--histogram` | - | off | Print pulse length histogram |
| `--verbose`, `-v` | - | off | Verbose output (real speed Bd, approx speed, pulse set) |
| `--channel` | L, R | L | Channel selection from stereo |
| `--invert` | - | off | Invert signal polarity |
| `--keep-unknown` | - | off | Save unidentified segments as Direct Recording |
| `--raw-format` | direct | direct | Format for unidentified blocks |
| `--pass` | `<N>` | 1 | Number of passes (not yet used) |
| `--pulse-mode` | approximate, exact | approximate | Pulse width storage mode for TMZ output |
| `--name-encoding` | ascii, utf8-eu, utf8-jp | ascii | Filename encoding: ascii (default), utf8-eu (European Sharp MZ), utf8-jp (Japanese Sharp MZ) |
| `--recover` | - | off | Enable all recovery modes |
| `--recover-bsd` | - | off | Recover incomplete BSD files (missing terminator) |
| `--recover-body` | - | off | Recover partial body data (not yet implemented) |
| `--recover-header` | - | off | Save header-only files (not yet implemented) |
| `--version` | - | - | Show program version |
| `--lib-versions` | - | - | Show library versions |
| `--help`, `-h` | - | - | Show help |

### Option Details

**-o, --output-format** - enables saving output. Without these options,
only analysis and block listing is performed.
- `mzf` - each decoded file is saved as a separate MZF file
  (naming: input_1.mzf, input_2.mzf, ...)
- `tmz` - all blocks are saved into a single TMZ archive

**--append-tmz** - if the output TMZ file already exists, appends new
blocks to the end of the existing archive. Without this option, an existing
TMZ file is an error. If the option is specified but the file does not exist,
a new file is created with a warning.

**--overwrite-mzf** - allows overwriting existing MZF files.
Without this option, an existing output MZF file is an error.

**--schmitt** - uses Schmitt trigger for pulse detection instead of
standard zero-crossing. Suitable for noisy recordings.

**--tolerance** - tolerance for leader tone recognition. A lower value
is stricter, a higher value tolerates greater timing deviations.
Range: 0.02 (very strict) to 0.35 (very lenient).

**--channel** - for stereo recordings, selects the channel.
Tape is typically recorded on the left channel.

**--invert** - inverts signal polarity. Use when the
recording has wrong polarity (e.g. from a different recording device).

**--name-encoding** - determines how filenames from MZF headers are displayed:
- `ascii` - Sharp MZ character set translation to ASCII (default, backward compatible)
- `utf8-eu` - translation to UTF-8, European character set variant (displays actual Sharp MZ glyphs)
- `utf8-jp` - translation to UTF-8, Japanese character set variant (katakana instead of lowercase letters)

**--recover-bsd** - if a BSD file on tape is missing the termination chunk
(ID=0xFFFF), e.g. due to a missing CLOSE in BASIC, all successfully decoded
chunks are salvaged. Without this option, incomplete BSD data is discarded
(with a diagnostic message and hint). In TMZ output, a Text Description
block (0x30) with a warning is inserted before the recovered block.

**--recover** - enables all recovery modes at once (--recover-bsd
and future --recover-body, --recover-header).

**--pulse-mode** - controls how pulse widths are stored in TMZ blocks:
- `approximate` (default) - quantizes speed to the nearest CMTSPEED ratio
  (1:1, 2:1, 7:3, ...). Standard 1200 Bd uses block 0x40, other speeds use 0x41.
- `exact` - stores measured pulse widths from histogram analysis directly
  in block 0x41 fields (long_high/low, short_high/low). Speed is set to 0
  (custom mode). Preserves the exact timing of the original recording,
  including sub-standard speeds (< 1200 Bd) that would otherwise be
  rounded to 1:1. Only affects NORMAL and MZ-80B formats.

**--keep-unknown** - recording segments that were not identified
as any known format are saved as TZX block 0x15 (Direct Recording).
Useful for preserving the entire tape contents.

## Output Formats

### MZF Mode (default)

A separate MZF file is created for each decoded file.
ZX Spectrum blocks are skipped in this mode (use TMZ format).

### TMZ Mode

All decoded blocks are saved into a single TMZ file:
- NORMAL 1:1 -> block 0x40 (MZ Standard Data)
- NORMAL other speed -> block 0x41 (MZ Turbo Data, format=NORMAL)
- TURBO/FASTIPL/FSK/SLOW/DIRECT/CPM-CMT/CPM-TAPE -> block 0x41
- BSD -> block 0x45 (MZ BASIC Data)
- ZX Spectrum -> block 0x10 (Standard Speed Data)
- Unidentified (with --keep-unknown) -> block 0x15 (Direct Recording)

The file begins with block 0x30 (Text Description) containing source WAV metadata.

## Examples

Analyze WAV file (no saving):

```
wav2tmz recording.wav
```

Verbose analysis with speeds and pulse sets:

```
wav2tmz recording.wav --verbose
```

Decoding to MZF files:

```
wav2tmz recording.wav -o recording.mzf
```

Decoding to a TMZ archive:

```
wav2tmz recording.wav --output-format tmz
```

Decoding with a custom output file:

```
wav2tmz recording.wav -o game.mzf
```

Decoding with Schmitt trigger (noisy recording):

```
wav2tmz noisy_tape.wav --schmitt --tolerance 0.15 -o output.mzf
```

Detailed analysis with histogram:

```
wav2tmz recording.wav --verbose --histogram
```

Decoding the right channel with inverted polarity:

```
wav2tmz stereo_tape.wav --channel R --invert
```

Decoding with unidentified segments saved:

```
wav2tmz full_tape.wav --output-format tmz --keep-unknown
```

Disabling preprocessing (raw data):

```
wav2tmz clean_recording.wav --no-preprocess
```

Decoding with filenames displayed in European UTF-8 character set:

```
wav2tmz recording.wav --name-encoding utf8-eu
```

Recovering incomplete BSD files:

```
wav2tmz tape_with_broken_bsd.wav --recover-bsd
```

Recovering all partial data into a TMZ archive:

```
wav2tmz damaged_tape.wav --recover --output-format tmz -o rescued.tmz
```

Decoding with exact pulse widths into TMZ:

```
wav2tmz recording.wav --output-format tmz --pulse-mode exact -o precise.tmz
```

Appending additional recordings to an existing TMZ:

```
wav2tmz side_a.wav -o tape.tmz --output-format tmz
wav2tmz side_b.wav -o tape.tmz --output-format tmz --append-tmz
```

Overwriting existing MZF files:

```
wav2tmz recording.wav -o game.mzf --overwrite-mzf
```
