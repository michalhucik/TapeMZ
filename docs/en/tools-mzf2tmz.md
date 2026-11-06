# mzf2tmz - MZF/MZT to TMZ Converter

Converts Sharp MZ files (MZF) or collections of multiple MZF files (MZT)
to the TMZ format. Supports all tape formats and speeds.

When the format is NORMAL and speed is 1:1, block 0x40 (MZ Standard Data) is used.
When a different format or speed is used, block 0x41 (MZ Turbo Data) is used.

If the output TMZ file already exists, new blocks are appended to the end
of the existing tape. If it does not exist, a new TMZ file is created.

An input file with the `.mzt` extension is processed as a sequence of multiple MZF
files concatenated together (each 128B header + body).

## Usage

```
mzf2tmz <input.mzf|input.mzt> <output.tmz> [options]
```

## Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `--machine` | generic, mz700, mz800, mz1500, mz80b | mz800 | Target computer |
| `--pulseset` | 700, 800, 80b, auto | auto | Pulse set (auto = based on machine) |
| `--format` | normal, turbo, fastipl, fsk, slow, direct, cpm-tape | normal | Tape recording format |
| `--speed` | 1:1, 2:1, 2:1cpm, 3:1, 3:2, 7:3, 8:3, 9:7, 25:14, or baudrate (e.g. 2800) | 1:1 | Speed ratio or baudrate in Bd (not valid for FSK/SLOW) |
| `--fsk-speed` | 0-6 | 0 | FSK speed level (0=slowest, 6=fastest; only with `--format fsk`) |
| `--slow-speed` | 0-4 | 0 | SLOW speed level (0=slowest, 4=fastest; only with `--format slow`) |
| `--pause` | 0-65535 | 1000 | Pause after block in milliseconds (with `--profile` after the last part) |
| `--profile` | ID, list | - | Create a 0x46 block (TMZ v1.1) by a registry profile (see below) |
| `--loader` | y, n | y | TC-1.21: with preloader (y) or without (n) |
| `--gap` | rom, short | profile default | `ROM-800-*`, `UNICMT` (also MZT without `--profile` and without format options, including `--machine`): ROM lead-in tones 22000/11000 or half 11000/5500 |
| `--bd` | 600, 1200, 2400, 2800, 3200 | 1200 | Intercopy profiles `IC-10.2-*`: save speed (Intercopy labels) |
| `--charset` | eu, jp, utf8-eu, utf8-jp | eu | Sharp MZ character set for filename display (see below) |
| `--version` | - | - | Show program version |
| `--lib-versions` | - | - | Show library versions |

### Option Details

**--machine** - specifies the target computer. Affects automatic pulse set selection:
- mz700 -> pulse set 700 (MZ-700/80K/80A)
- mz800 -> pulse set 800 (MZ-800/1500)
- mz80b -> pulse set 80B (MZ-80B)

**--format** - tape recording format:
- `normal` - standard FM encoding (1200 Bd base)
- `turbo` - FM with configurable timing and loader
- `fastipl` - $BB prefix, Intercopy loader
- `fsk` - Frequency Shift Keying (7 speed levels)
- `slow` - 2 bits/pulse (5 speed levels)
- `direct` - direct bit writing
- `cpm-tape` - CP/M TAPE (Pezik/MarVan TAPE.COM, Manchester): same as `--profile PEZIK-2.0A`,
  writes block 0x46; speed `--bd 2400` (default) or `3200`, `--speed` is not used

mzf2tmz rejects `sinclair`: a ZX Spectrum recording belongs into TZX blocks
0x10/0x11 (`tap2tmz`), the player does not play an MZ block with SINCLAIR.
The `fsk`, `slow` and `direct` formats work on the MZ-700, MZ-800 and
MZ-1500; they are not verified on the MZ-80B and mzf2tmz prints a warning.

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

**--speed** - speed ratio or baudrate (for FM formats: normal, turbo, fastipl):
- Ratio format: `1:1` = 1200 Bd, `2:1` = 2400 Bd, `7:3` = 2800 Bd, `8:3` = 3200 Bd, `3:1` = 3600 Bd etc.
- Baudrate format: `1200`, `2400`, `2800`, `3200`, `3600` etc. (mapped to nearest ratio)
- `2:1cpm` = 2400 Bd variant for CP/M
- Not valid for FSK and SLOW formats (use `--fsk-speed` / `--slow-speed` instead).
- For FASTIPL, baudrate format is recommended (matches Intercopy convention).
- TURBO uses TurboCopy V1.2x timing and supports ratios with digits 1-9:
  1:1, 2:1, 3:2, 7:3, 8:3, 9:7 and 3:1 (3:2 and 9:7 not verified in the
  emulator); 25:14 and 2:1cpm are rejected.
  On conversion to WAV (`tmz2wav`) the signal is checked to be readable by
  the ROM; at 22050 Hz 8:3 and 3:1 are not readable and the conversion
  fails. Use 44100 Hz or more.

**--fsk-speed** - FSK encoder speed level (only with `--format fsk`):
- Level 0 (slowest): long=8, short=4 samples per cycle
- Level 6 (fastest): long=3, short=2 samples per cycle
- Higher level = faster transfer, lower reliability

**--slow-speed** - SLOW encoder speed level (only with `--format slow`):
- Level 0 (slowest): longest pulses
- Level 4 (fastest): shortest pulses
- Higher level = faster transfer, lower reliability

**--profile** - creates a 0x46 MZ File block (TMZ v1.1) with the list of tape parts and their timing following a writer profile. `--profile list` prints the registry. Profiles:
- `ROM-800-NOMINAL` - MZ-800 ROM SAVE with nominal pulses 240/278 and 470/494 us, half tones by default (11000/5500).
- `ROM-800-EXACT` - MZ-800 ROM SAVE timing in T-states including code-path dependent lengths; identical to the real ROM output in the emulator. ROM tones by default (22000/11000).
- `TC-1.21` - TurboCopy V1.21 output. `--speed` 1:1, 2:1, 3:1, 3:2, 7:3, 8:3, 9:7 (default 2:1). `--loader y` (default) writes the preloader loadable by the ROM; `--loader n` writes only header and body (loadable by TurboCopy only).
- `IC-10.2-NORMAL`, `IC-10.2-TURBO`, `IC-10.2-FASTIPL` - Intercopy V10.2 (NIPSOFT) output in NORMAL, TURBO (TurboCopy preloader loadable by the ROM) and FAST IPL (loader in the $BB header) mode. Speed `--bd` (600 to 3200). Timing measured in the emulator; the voice label is not written. `IC-10.2-NORMAL` at a speed other than 1200 cannot be read by the ROM (only by Intercopy) and the player does not check its readability.
- `BASIC-1Z016` - BASIC 1Z-016 data file (`WOPEN#`, timing measured in the emulator). The MZF body must be 258-byte chunks (ID + 256 data bytes, last ID FFFF), 1 to 254 chunks, fsize 0. Usually written by dat2bsd and wav2tmz.
- `IC-10.2-CPM` - Intercopy V10.2 in CPM mode (Manchester, `--bd` 1500, 2400 (default), 2800, 3200 or 3600, timing measured in the emulator). The body is one block.
- `PEZIK-2.0A` - CP/M program TAPE.COM V2.0a (Pezik/MarVan, Manchester), `--bd 2400` or `3200`. Timing calculated from the program code (not verified in the emulator). Body at most 16 384 bytes (TAPE.COM splits a longer file into parts with its own CP/M header); the header is written as the MZF header, whether TAPE.COM accepts it is not verified.
- `UNICMT` - UniCMT player: `ROM-800-NOMINAL` structure, pulses of the CMTSPEED header (default 1x).

`--format`, `--pulseset`, `--fsk-speed` and `--slow-speed` are not used with `--profile`. Options the chosen profile does not use are an error: `--loader` is valid only for `TC-1.21`, `--gap` for `ROM-800-*` and `UNICMT`, `--bd` for `IC-10.2-*`, `--speed` for `ROM-800-*` (1:1 only) and `TC-1.21`. `--machine` is stored in the block (PROFILE_ARGS, informative only). The `TAPEMZ-V1-LEGACY` profile is produced only by v1.0 block conversion (`tmzconv --upgrade`); mzf2tmz rejects it. The MZF header bytes are written as they are in the file, including byte 11h (with profiles and v1.0 blocks). A file with a 0x46 block gets header version 1.1 (also when the block is appended to a version 1.0 file).

### MZT and CMTSPEED headers (UniCMT)

An MZT without `--format`, `--speed`, `--pulseset`, `--machine` and `--profile` (or
with `--profile UNICMT`) is converted to 0x46 blocks with the `UNICMT`
profile: one block per MZF file. UniCMT CMTSPEED headers (files
`1xspeed.mzf`, `2xspeed.mzf`, `3xspeed.mzf` inserted between the files)
set the speed of all following files and do not create a block. The
`UNICMT` profile uses the `ROM-800-NOMINAL` structure with the pulses of
the CMTSPEED header (default 1x = 470/494/240/278 us); the UniCMT signal
structure has not been verified by measurement yet.

- A CMTSPEED header at the very start of the MZT is not played by
  UniCMT, so mzf2tmz ignores it with a warning.
- A CMTSPEED header at the end of the MZT with no file after it prints
  a warning.
- A remainder shorter than a header at the end of the file (padding,
  e.g. after a transfer through CP/M) is ignored with a warning.
- With explicit format options an MZT without CMTSPEED is converted as
  before (each file by the options). An MZT with CMTSPEED and any option
  other than `--profile UNICMT` is an error, because the speeds would be
  lost.

## Examples

Basic MZF to TMZ conversion (NORMAL 1:1, block 0x40):

```
mzf2tmz game.mzf game.tmz
```

Conversion with TURBO format and double speed (block 0x41):

```
mzf2tmz game.mzf game.tmz --format turbo --speed 2:1
```

Conversion for MZ-700 with FSK format at speed level 3:

```
mzf2tmz program.mzf tape.tmz --machine mz700 --format fsk --fsk-speed 3
```

Conversion with SLOW format at maximum speed:

```
mzf2tmz program.mzf tape.tmz --format slow --slow-speed 4
```

Conversion of an MZT file (multiple MZFs):

```
mzf2tmz collection.mzt tape.tmz
```

Appending another program to an existing TMZ:

```
mzf2tmz loader.mzf tape.tmz
mzf2tmz game.mzf tape.tmz --format turbo --speed 2:1
```

Conversion with custom pulse set and pause:

```
mzf2tmz demo.mzf demo.tmz --pulseset 700 --pause 2000
```

Conversion for CP/M format:

```
mzf2tmz cpm.mzf tape.tmz --format cpm-tape --machine mz800
```

Conversion with filenames displayed in European UTF-8 character set:

```
mzf2tmz game.mzf game.tmz --charset utf8-eu
```
