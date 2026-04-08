# tmzinfo - TMZ/TZX File Viewer

Displays detailed information about the structure of a TMZ or TZX file.
Shows the file type, version, block count, and detailed information
about each block including MZF headers, timing, format, and metadata.

## Usage

```
tmzinfo <file.tmz|file.tzx>
```

## Arguments

| Argument | Description |
|----------|-------------|
| `<file>` | Input TMZ or TZX file |

## Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `--name-encoding` | ascii, utf8-eu, utf8-jp | ascii | Filename encoding: ascii (default), utf8-eu (European Sharp MZ), utf8-jp (Japanese Sharp MZ) |
| `--version` | - | - | Show program version |
| `--lib-versions` | - | - | Show library versions |

### Option Details

**--name-encoding** - determines how filenames from MZF headers are displayed:
- `ascii` - Sharp MZ character set translation to ASCII (default, backward compatible)
- `utf8-eu` - translation to UTF-8, European character set variant (displays actual Sharp MZ glyphs)
- `utf8-jp` - translation to UTF-8, Japanese character set variant (katakana instead of lowercase letters)

All information is printed to standard output.

## Displayed Information

### File Header

- File type: TMZ (TapeMZ!) or TZX (ZXTape!)
- Format version (e.g. 1.0 for TMZ, 1.20 for TZX)
- Total block count

### MZ Blocks (0x40-0x45)

- **0x40 MZ Standard Data** - target machine, pulse set, pause, MZF header (name, type, size, addresses)
- **0x41 MZ Turbo Data** - format (NORMAL/TURBO/FASTIPL/FSK/SLOW/DIRECT/CPM-TAPE), speed, pulse timing, MZF header
- **0x42 MZ Extra Body** - format, speed, pause, body size
- **0x43 MZ Machine Info** - machine, CPU clock, ROM version
- **0x44 MZ Loader** - loader type (TURBO/FASTIPL/FSK/SLOW/DIRECT), speed, size, MZF header
- **0x45 MZ BASIC Data** - machine, pulse set, chunk count, MZF header

### TZX Blocks

- **0x10 Standard Speed Data** - speed in Bd, pause, data length, flag (header/data)
- **0x11 Turbo Speed Data** - speed in Bd, pilot, sync, bit timing, byte count
- **0x12 Pure Tone** - pulse length in T-states, pulse count
- **0x13 Pulse Sequence** - individual pulses in T-states
- **0x14 Pure Data** - bit timing, pause, data
- **0x15 Direct Recording** - T-states/sample (~frequency), pause, data
- **0x18 CSW Recording** - sampling frequency, compression, pulse count
- **0x20 Pause** - pause length (0 = STOP)
- **0x21 Group Start** - group name
- **0x30 Text Description** - text description
- **0x31 Message** - display message with time
- **0x32 Archive Info** - metadata (Title, Author, Year, Publisher, ...)
- **0x33 Hardware Type** - hardware requirements

## Examples

Displaying TMZ file contents:

```
tmzinfo game.tmz
```

Example output:

```
=== game.tmz ===

File type  : TMZ (TapeMZ!)
Version    : 1.0
Blocks     : 2

  [  0] ID 0x40  MZ Standard Data           (4267 bytes)  [MZ]
      Machine : MZ-800
      Pulseset: MZ-800/1500
      Pause   : 1000 ms
      Body    : 4139 bytes
      Filename : "GAME"
      Type     : 0x01 (OBJ (machine code))
      Size     : 4139 bytes (0x102B)
      Load addr: 0x1200
      Exec addr: 0x1200

  [  1] ID 0x41  MZ Turbo Data              (4280 bytes)  [MZ]
      Machine : MZ-800
      Pulseset: MZ-800/1500
      Format  : TURBO
      Speed   : 2:1 (2400 Bd)
      ...
```

Displaying TZX file contents with ZX Spectrum data:

```
tmzinfo spectrum.tzx
```

Displaying filenames in European UTF-8 character set:

```
tmzinfo game.tmz --name-encoding utf8-eu
```

Displaying a TMZ file with Archive Info:

```
tmzinfo tape.tmz
```
