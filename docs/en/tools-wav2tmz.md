# wav2tmz - WAV Recording Analyzer and Decoder

Analyzes a WAV file containing a cassette tape recording from
Sharp MZ (or ZX Spectrum) computers and extracts MZF files
or a TMZ archive from it.

Automatically detects the recording format: NORMAL, TURBO, FASTIPL, BSD,
CPM-CMT, CPM-TAPE, MZ-80B, FSK, SLOW, DIRECT, ZX Spectrum.

If the output format is TMZ and the output file already exists,
new blocks are appended to the end of the existing archive.

## Usage

```
wav2tmz input.wav [-o output] [options]
```

## Options

| Option | Value | Default | Description |
|--------|-------|---------|-------------|
| `-o` | `<file>` | derived from input | Output file |
| `--output-format` | mzf, tmz | mzf | Output format |
| `--schmitt` | - | off | Use Schmitt trigger instead of zero-crossing |
| `--tolerance` | 0.02-0.35 | 0.10 | Leader tone detection tolerance |
| `--preprocess` | - | on | Enable signal preprocessing |
| `--no-preprocess` | - | - | Disable preprocessing (DC offset, HP filter, normalization) |
| `--histogram` | - | off | Print pulse length histogram |
| `--verbose`, `-v` | - | off | Verbose analysis output |
| `--channel` | L, R | L | Channel selection from stereo |
| `--invert` | - | off | Invert signal polarity |
| `--keep-unknown` | - | off | Save unidentified segments as Direct Recording |
| `--raw-format` | direct | direct | Format for unidentified blocks |
| `--pass` | `<N>` | 1 | Number of passes (not yet used) |
| `--name-encoding` | ascii, utf8-eu, utf8-jp | ascii | Filename encoding: ascii (default), utf8-eu (European Sharp MZ), utf8-jp (Japanese Sharp MZ) |
| `--version` | - | - | Show program version |
| `--lib-versions` | - | - | Show library versions |
| `--help`, `-h` | - | - | Show help |

### Option Details

**--output-format** - determines how decoded data is saved:
- `mzf` - each decoded file is saved as a separate MZF file
  (naming: input_1.mzf, input_2.mzf, ...)
- `tmz` - all blocks are saved into a single TMZ archive

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

Basic decoding to MZF files:

```
wav2tmz recording.wav
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
wav2tmz noisy_tape.wav --schmitt --tolerance 0.15
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

Appending additional recordings to an existing TMZ:

```
wav2tmz side_a.wav -o tape.tmz --output-format tmz
wav2tmz side_b.wav -o tape.tmz --output-format tmz
```
