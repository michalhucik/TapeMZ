# tmz2mzf - MZF File Extractor from TMZ/TZX

Extracts MZF files from blocks 0x40 (MZ Standard Data) and 0x41 (MZ Turbo Data)
in TMZ or TZX files. Supports extracting all blocks at once,
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
