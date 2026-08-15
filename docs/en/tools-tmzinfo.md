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
| `--charset` | eu, jp, utf8-eu, utf8-jp | eu | Sharp MZ character set for filename display (see below) |
| `--version` | - | - | Show program version |
| `--lib-versions` | - | - | Show library versions |

### Option Details

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

All information is printed to standard output.

## Displayed Information

### File Header

- File type: TMZ (TapeMZ!) or TZX (ZXTape!)
- Format version (e.g. 1.0 for TMZ, 1.20 for TZX)
- Total block count

### MZ Blocks (0x40-0x45)

- **0x40 MZ Standard Data** - target machine, pulse set, pause, MZF header (name, type, size, addresses)
- **0x41 MZ Turbo Data** - format (NORMAL/TURBO/FASTIPL/FSK/SLOW/DIRECT/CPM-TAPE), speed or custom pulseset (with pulse widths in us and estimated Bd), MZF header
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
tmzinfo game.tmz --charset utf8-eu
```

Displaying a TMZ file with Archive Info:

```
tmzinfo tape.tmz
```
