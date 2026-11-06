# wav2tmz - WAV Recording Analyzer and Decoder

Analyzes a WAV file containing a cassette tape recording from
Sharp MZ (or ZX Spectrum) computers.

By default, performs analysis only and prints discovered blocks (no files saved).
With `-o` or `--output-format`, extracts MZF files or a TMZ archive.

Automatically detects the recording format: NORMAL, TURBO, FASTIPL, BSD,
CPM-CMT, CPM-TAPE, FSK, SLOW, DIRECT, ZX Spectrum. MZ-80B is not detected
automatically.

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
| `--tolerance` | 0.02-0.35 | 0.20 | Leader tone detection tolerance |
| `--preprocess` | - | on | Enable signal preprocessing |
| `--no-preprocess` | - | - | Disable preprocessing (DC offset, HP filter, normalization) |
| `--histogram` | - | off | Only prints a hint to use `--verbose`; the pulse length histogram is printed by `--verbose` |
| `--verbose`, `-v` | - | off | Verbose output (real speed Bd, approx speed, pulse set) |
| `--channel` | L, R | L | Channel selection from stereo |
| `--invert` | - | off | Invert signal polarity |
| `--keep-unknown` | - | off | Save unidentified segments as Direct Recording |
| `--raw-format` | direct | direct | Format for unidentified blocks |
| `--pass` | `<N>` | 1 | Number of passes (not yet used) |
| `--pulse-mode` | approximate, exact | approximate | Pulse width storage mode for TMZ output |
| `--charset` | eu, jp, utf8-eu, utf8-jp | eu | Sharp MZ character set for filename display (see below) |
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

**--charset** - selects the Sharp MZ character set for filename conversion:

Sharp MZ computers used two character set variants - European (EU) and Japanese (JP).
Both share the range 0x20-0x5D (uppercase letters, digits, basic punctuation), which
is identical to standard ASCII. Above this range the sets differ:

- **EU** (European) - contains lowercase letters a-z and several special characters
  (umlauts, Eszett, arrows, card suit symbols). When converting to ASCII, lowercase
  letters are translated correctly, but special characters are replaced with spaces.
- **JP** (Japanese) - contains katakana, kanji, and special characters (¥, £).
  When converting to ASCII, everything above 0x5D is replaced with a space -
  the result is therefore more limited compared to the EU variant.

Conversion modes:
- `eu` (default) - Sharp MZ-EU -> ASCII. Lowercase letters and basic characters
  are translated, special European characters (umlauts, arrows) are replaced with spaces.
- `jp` - Sharp MZ-JP -> ASCII. Only characters 0x20-0x5D are translated, everything
  else (katakana, kanji) is replaced with spaces.
- `utf8-eu` - Sharp MZ-EU -> UTF-8. Displays the maximum number of characters
  including umlauts (Ö, ü, ß, Ä, ö, ä), arrows (↑↓←→), card suit symbols (♤♡♧♢),
  pound sign (£), and pi (π).
- `utf8-jp` - Sharp MZ-JP -> UTF-8. Displays the maximum number of characters
  including katakana (ア-ン), day-of-week kanji (日月火水木金土), ¥, £,
  and other Japanese characters.

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

## Input WAV

wav2tmz reads WAV in PCM (8, 16, 24, 32 and 64 bit), IEEE float (32 and
64 bit) and WAVE_FORMAT_EXTENSIBLE with SubFormat PCM or IEEE float, mono
or stereo (`--channel`). Other encodings (A-law, mu-law, compressed) fail
with the reason.

## Output Formats

### MZF Mode (default)

A separate MZF file is created for each decoded file.
ZX Spectrum blocks and blocks without MZF data (e.g. SINCLAIR) are skipped
with a warning in this mode (use TMZ format). If no file is saved, the
program exits with code 1.

### TMZ Mode

All decoded blocks are saved into a single TMZ file:
- A TurboCopy V1.2x recording (recognized by the tone length: pulse count x S is constant)
  -> block 0x46 with profile `TC-1.21`, the TurboCopy ratio a/b (all 34 ratios 1/2 to 3/1),
  `loader=Y` with the preloader, `loader=N` without it
- An Intercopy V10.2 recording -> block 0x46: NORMAL 600/2400/2800/3200 (header tone 11000
  pulses) -> `IC-10.2-NORMAL`, TURBO (body tone 5500, preloader read point) ->
  `IC-10.2-TURBO`, FAST IPL with the V07 loader (Intercopy v7 to V10.2; the timing of older
  versions is not verified) -> `IC-10.2-FASTIPL`, always with speed `bd`
- NORMAL 1:1 -> block 0x40 (MZ Standard Data)
- NORMAL other speed -> block 0x41 (MZ Turbo Data, format=NORMAL); a warning is printed when
  the speed estimate is more than 5 % away from the nearest block 0x41 speed and also further
  than the sampling resolution (1 sample per half-wave)
- TURBO/FASTIPL without a recognized writer, FSK/SLOW/DIRECT/CPM-CMT -> block 0x41; for FSK and SLOW the speed level is
  taken from the delay byte in the loader (if not recognized, 0 is written with a warning)
- CPM-TAPE -> block 0x46: the Intercopy variant (end bit after the checksum) -> profile `IC-10.2-CPM`,
  otherwise `PEZIK-2.0A` at 2400 or 3200 by the bit length. The Intercopy speed
  (1500, 2400, 2800, 3200, 3600 Bd) is determined by the bit length and written as `bd`
- BSD -> block 0x46 with profile `BASIC-1Z016` (BASIC 1Z-016 timing; the tones, copies and
  pauses of the source are not kept). A file with more than 254 chunks cannot be stored (error).
  The CRC of every chunk is checked; on an error the chunk copy is used, the summary prints
  `BSD chunks: N, CRC errors: E, from copy: C`.
- ZX Spectrum -> block 0x10 (Standard Speed Data); a pilot outside the standard speed
  (619 µs +- 5 %, e.g. Intercopy SINCLAIR 900 Bd with a 930 µs pilot) -> block 0x11
  (Turbo Speed Data) with the measured timing
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

Detailed analysis with the pulse length histogram (printed by `--verbose`):

```
wav2tmz recording.wav --verbose
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
wav2tmz recording.wav --charset utf8-eu
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
