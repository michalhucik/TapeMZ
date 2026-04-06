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
| `--format` | normal, turbo, fastipl, sinclair, fsk, slow, direct, cpm-tape | normal | Tape recording format |
| `--speed` | 1:1, 2:1, 2:1cpm, 3:1, 3:2, 7:3, 8:3, 9:7, 25:14 | 1:1 | Speed ratio (not valid for FSK/SLOW) |
| `--fsk-speed` | 0-6 | 0 | FSK speed level (0=slowest, 6=fastest; only with `--format fsk`) |
| `--slow-speed` | 0-4 | 0 | SLOW speed level (0=slowest, 4=fastest; only with `--format slow`) |
| `--pause` | 0-65535 | 1000 | Pause after block in milliseconds |
| `--name-encoding` | ascii, utf8-eu, utf8-jp | ascii | Filename encoding: ascii (default), utf8-eu (European Sharp MZ), utf8-jp (Japanese Sharp MZ) |
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
- `sinclair` - FM Sinclair variant
- `fsk` - Frequency Shift Keying (7 speed levels)
- `slow` - 2 bits/pulse (5 speed levels)
- `direct` - direct bit writing
- `cpm-tape` - Manchester encoding (Pezik/MarVan)

**--name-encoding** - determines how filenames from MZF headers are displayed:
- `ascii` - Sharp MZ character set translation to ASCII (default, backward compatible)
- `utf8-eu` - translation to UTF-8, European character set variant (displays actual Sharp MZ glyphs)
- `utf8-jp` - translation to UTF-8, Japanese character set variant (katakana instead of lowercase letters)

**--speed** - speed ratio relative to the base 1200 Bd (for FM formats: normal, turbo, fastipl, sinclair):
- `1:1` = 1200 Bd, `2:1` = 2400 Bd, `3:1` = 3600 Bd etc.
- `2:1cpm` = 2400 Bd variant for CP/M
- Not valid for FSK and SLOW formats (use `--fsk-speed` / `--slow-speed` instead).

**--fsk-speed** - FSK encoder speed level (only with `--format fsk`):
- Level 0 (slowest): long=8, short=4 samples per cycle
- Level 6 (fastest): long=3, short=2 samples per cycle
- Higher level = faster transfer, lower reliability

**--slow-speed** - SLOW encoder speed level (only with `--format slow`):
- Level 0 (slowest): longest pulses
- Level 4 (fastest): shortest pulses
- Higher level = faster transfer, lower reliability

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
mzf2tmz game.mzf game.tmz --name-encoding utf8-eu
```
