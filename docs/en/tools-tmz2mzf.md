# tmz2mzf - MZF File Extractor from TMZ/TZX

Extracts MZF files from blocks 0x40 (MZ Standard Data), 0x41 (MZ Turbo Data)
and 0x46 (MZ File, TMZ v1.1) in TMZ or TZX files. Supports extracting all blocks at once,
selecting a specific block by index, and listing extractable blocks.

## Usage

```
tmz2mzf <input.tmz|input.tzx> [options]
```

## Options

| Option | Value | Description |
|--------|-------|-------------|
| `--output` | `<file>` | Output file (default: derived from input) |
| `--index` | `<N>` | Extract only the block at index N (0-based) |
| `--list` | - | List extractable blocks without extraction |
| `--overwrite` | - | Overwrite existing output file(s) |
| `--append` | - | Append to existing output file (multi-MZF) |
| `--mzt` | - | All MZF files into one classic MZT (default for `--output *.mzt`) |
| `--mzt-unicmt` | - | MZT with UniCMT CMTSPEED headers |
| `--charset` | eu, jp, utf8-eu, utf8-jp | Sharp MZ character set for filename display (see below) |
| `--version` | - | Show program version |
| `--lib-versions` | - | Show library versions |

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

### Option Details

**--overwrite** - by default, the program refuses to overwrite an existing output file.
Use this option to allow overwriting.

**--append** - appends extracted MZF blocks to an existing file, creating a multi-MZF
(MZT-style) file with concatenated header+body records. If the file does not exist,
a new file is created. When extracting multiple blocks, all blocks go into a single
output file (instead of numbered separate files).

### Output Naming

- Single block with `--output`: exact name is used
- Single block without `--output`: derived from input (input.tmz -> input.mzf)
- Multiple blocks with `--output`: output_001.mzf, output_002.mzf, ...
- Multiple blocks without `--output`: input_001.mzf, input_002.mzf, ...
- `--append`: all blocks into a single file (`--output` or derived from input)

### MZT (`--mzt`, `--mzt-unicmt`)

Both options write all blocks carrying an MZF into one MZT file
(`--output`, otherwise the input name with the `.mzt` extension). They
cannot be combined with `--index`, `--list` or `--append`; an existing
file is overwritten only with `--overwrite`.

- `--mzt` writes a classic MZT: the MZF files one after another. Timing
  is lost; for a block that is not a ROM recording at 1x (e.g. TURBO,
  FASTIPL, speed 2:1) the program prints a warning.
- `--mzt-unicmt` inserts a UniCMT CMTSPEED header before a file wherever
  the speed differs from the previous file (default 1x). The block speed
  comes from its profile: ROM and 0x40 = 1x, 0x41 NORMAL by its speed
  (2:1 and 3:1 give exactly UniCMT 2x and 3x), `UNICMT` by the CMTSPEED
  header it came from. The first file must be 1x (UniCMT does not play a
  leading header) and all blocks must be ROM recordings (header + body);
  otherwise the program exits with an error and writes nothing.

An MZT converted by `mzf2tmz` and back by `tmz2mzf --mzt-unicmt` is
byte-identical to the original (verified on the UniCMT samples).

## Examples

Listing extractable blocks:

```
tmz2mzf tape.tmz --list
```

Example `--list` output:

```
=== tape.tmz ===

File type: TMZ, Version: 1.0, Blocks: 3

Extractable blocks (2):

  [  0] 0x40 MZ Standard Data          "LOADER"  type=0x01  size=256  load=0x1200  exec=0x1200
  [  1] 0x41 MZ Turbo Data             "GAME"    type=0x01  size=8192  load=0x4000  exec=0x4000
```

Extracting all MZF files:

```
tmz2mzf tape.tmz
```

Extracting a specific block by index:

```
tmz2mzf tape.tmz --index 1
```

Extracting with a custom output name:

```
tmz2mzf tape.tmz --output game.mzf --index 0
```

Extracting all blocks with a custom prefix:

```
tmz2mzf tape.tmz --output export.mzf
```

Extracting all blocks into a single multi-MZF file:

```
tmz2mzf tape.tmz --output collection.mzf --append
```

Appending a block to an existing MZF file:

```
tmz2mzf tape.tmz --index 0 --output existing.mzf --append
```

Overwriting an existing output file:

```
tmz2mzf tape.tmz --output game.mzf --index 0 --overwrite
```

Listing blocks with names in European UTF-8 character set:

```
tmz2mzf tape.tmz --list --charset utf8-eu
```
