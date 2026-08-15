# tmzedit - TMZ/TZX File Editor

A universal tool for manipulating blocks in TMZ/TZX files.
Supports listing, hex dump, removal, moving, merging, splitting,
adding metadata, changing MZ block parameters, and integrity validation.

## Usage

```
tmzedit <command> [options] <file> [arguments...]
```

## Commands

| Command | Description |
|---------|-------------|
| `list` | List all blocks in the file |
| `dump` | Display hex dump of a specific block's data |
| `remove` | Remove a block at the given index |
| `move` | Move a block from one position to another |
| `merge` | Merge multiple files into one |
| `split` | Split a file into individual parts |
| `add-text` | Add a text description (block 0x30) |
| `add-message` | Add a display message (block 0x31) |
| `archive-info` | Add or replace metadata (block 0x32) |
| `set` | Change format/speed/pulse on an MZ block (0x40/0x41) or timing on block 0x11 |
| `validate` | Check file integrity |

## Common Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `-o` | `<output>` | overwrite input | Output file |
| `--charset` | eu, jp, utf8-eu, utf8-jp | eu | Sharp MZ character set for filename display (see below) |
| `--dump-charset` | raw, eu, jp, utf8-eu, utf8-jp | raw | Character set of the hex dump text column (`dump` only, see below) |
| `--version` | - | - | Show program version |
| `--lib-versions` | - | - | Show library versions |

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

---

## list - Block Listing

Displays a brief overview of all blocks including index, ID, name, size,
and basic information (program name, text, etc.).

```
tmzedit list <file>
```

### Example

```
tmzedit list game.tmz
```

Output:

```
File: game.tmz
Type: TMZ (TapeMZ!), version 1.0, 3 block(s)

  [  0] 0x32  Archive Info               42 B  (2 entries)
  [  1] 0x40  MZ Standard Data         4267 B  [MZ]  "LOADER"
  [  2] 0x41  MZ Turbo Data            8305 B  [MZ]  "GAME"

Total: 3 block(s)
```

---

## dump - Block Hex Dump

Displays a hexadecimal dump of the block data at the given index.
Format: 16 bytes per line with offset, hex values, and a text column.

```
tmzedit dump [--dump-charset <mode>] <file> <index>
```

**--dump-charset** - selects how bytes are interpreted in the text column:

- **raw** (default) - standard ASCII: bytes 0x20-0x7E are shown as characters,
  everything else as `.`. Suitable for machine code and non-text data.
- **eu** / **jp** - bytes are treated as Sharp MZ ASCII (European / Japanese
  variant) and converted to ASCII. A byte without an ASCII equivalent or a
  non-printable one is shown as `.`.
- **utf8-eu** / **utf8-jp** - same as above, but the output is UTF-8 (umlauts,
  arrows, card suit symbols, katakana etc. are displayed).

MZ block data (0x40, 0x41) start with the 128-byte MZF header containing the file
name in Sharp MZ ASCII - in `raw` mode lowercase letters appear as `.`, in `eu`
mode they are shown correctly.

### Example

```
tmzedit dump game.tmz 1
tmzedit dump --dump-charset eu game.tmz 1
tmzedit dump --dump-charset utf8-eu game.tmz 1
```

---

## remove - Block Removal

Removes the block at the given index and saves the result.
If `-o` is not specified, the input file is overwritten (in-place).

```
tmzedit remove <file> <index> [-o <output>]
```

### Example

```
tmzedit remove tape.tmz 0 -o tape_cleaned.tmz
```

---

## move - Block Move

Moves a block from position `<from>` to position `<to>`.
Indices are 0-based.

```
tmzedit move <file> <from> <to> [-o <output>]
```

### Example

Moving a block from position 2 to position 0:

```
tmzedit move tape.tmz 2 0 -o tape_reordered.tmz
```

---

## merge - File Merging

Merges blocks from multiple TMZ/TZX files into a single output file.
The output header matches the first file.

```
tmzedit merge <file1> <file2> [<file3> ...] -o <output>
```

### Example

```
tmzedit merge side_a.tmz side_b.tmz -o full_tape.tmz
```

---

## split - File Splitting

Splits a file into individual parts. If the file contains
Group Start (0x21) / Group End (0x22) blocks, it splits by groups.
Otherwise each data block forms a separate file.

Output files: `<prefix>_000.tmz`, `<prefix>_001.tmz`, ...

```
tmzedit split <file> [-o <prefix>]
```

### Example

```
tmzedit split collection.tmz -o game
```

Result: game_000.tmz, game_001.tmz, ...

Without `-o`, the prefix is derived from the input:

```
tmzedit split collection.tmz
```

Result: collection_000.tmz, collection_001.tmz, ...

---

## add-text - Adding Text Description

Adds block 0x30 (Text Description) to the end of the file.

```
tmzedit add-text <file> --text "<text>" [-o <output>]
```

### Example

```
tmzedit add-text game.tmz --text "Sharp MZ-800 game collection" -o game.tmz
```

---

## add-message - Adding Message

Adds block 0x31 (Message) to the end of the file. The message is displayed
during tape playback (e.g. in an emulator).

```
tmzedit add-message <file> --text "<text>" [--time <seconds>] [-o <output>]
```

| Option | Default | Description |
|--------|---------|-------------|
| `--text` | (required) | Message text |
| `--time` | 5 | Display duration in seconds (0-255) |

### Example

```
tmzedit add-message tape.tmz --text "Press PLAY on tape" --time 10 -o tape.tmz
```

---

## archive-info - Archive Metadata

Adds or replaces block 0x32 (Archive Info) with metadata.
If the file already contains block 0x32, the new one replaces it.
The new block is inserted at the beginning of the file (index 0).

```
tmzedit archive-info <file> [--title <X>] [--publisher <X>] [--author <X>]
    [--year <X>] [--language <X>] [--type <X>] [--price <X>]
    [--protection <X>] [--origin <X>] [--comment <X>] [-o <output>]
```

| Option | TZX ID | Description |
|--------|--------|-------------|
| `--title` | 0x00 | Program title |
| `--publisher` | 0x01 | Publisher |
| `--author` | 0x02 | Author(s) |
| `--year` | 0x03 | Year of release |
| `--language` | 0x04 | Language |
| `--type` | 0x05 | Type (game, utility, ...) |
| `--price` | 0x06 | Price |
| `--protection` | 0x07 | Copy protection |
| `--origin` | 0x08 | Origin |
| `--comment` | 0xFF | Comment |

### Example

```
tmzedit archive-info game.tmz --title "Space Invaders" --author "Taito" --year "1985" -o game.tmz
```

Adding a comment:

```
tmzedit archive-info tape.tmz --comment "Dumped from original tape" -o tape.tmz
```

---

## set - Changing MZ Block Format/Speed/Pulse

Changes the format, speed and/or pulse widths of a recording on an MZ block
(0x40 or 0x41), or the timing on block 0x11 (Turbo Speed Data / SINCLAIR).

Conversion rules:
- Block 0x40 with non-standard format/speed/pulse is converted to block 0x41
- Block 0x41 with format NORMAL and speed 1:1 (without custom pulses) is converted back to block 0x40
- Block 0x40 with NORMAL 1:1 remains unchanged
- Block 0x11 only supports `--sinclair-speed`
- `--pulse` sets custom mode (speed=0, pulse fields filled)
- `--speed` clears pulse fields (back to table mode)

```
tmzedit set <file> <index> [--format <fmt>] [--speed <spd>] [--pulse <values>] [-o <output>]
```

| Option | Values | Description |
|--------|--------|-------------|
| `--format` | normal, turbo, fastipl, sinclair, fsk, slow, direct, cpm-tape | Recording format |
| `--speed` | 1:1, 2:1, 2:1cpm, 3:1, 3:2, 7:3, 8:3, 9:7, 25:14, or baudrate (e.g. 2800) | Speed ratio or baudrate in Bd (not valid for FSK/SLOW/SINCLAIR) |
| `--fsk-speed` | 0-6 | FSK speed level (only with FSK format) |
| `--slow-speed` | 0-4 | SLOW speed level (only with SLOW format) |
| `--sinclair-speed` | 1381, 1772, 2074, 2487 | SINCLAIR speed in Bd (only for block 0x11) |
| `--pulse` | long_h/long_l,short_h/short_l | Custom pulse widths in us*100 units |

### Examples

Changing block 0x40 to TURBO 2:1 (converts to 0x41):

```
tmzedit set tape.tmz 0 --format turbo --speed 2:1 -o tape_turbo.tmz
```

Changing speed of an existing block 0x41:

```
tmzedit set tape.tmz 1 --speed 3:1 -o tape_fast.tmz
```

Changing an FSK block to speed level 5:

```
tmzedit set tape.tmz 0 --fsk-speed 5 -o tape_fast_fsk.tmz
```

Converting a block to SLOW format at speed level 2:

```
tmzedit set tape.tmz 0 --format slow --slow-speed 2 -o tape_slow.tmz
```

Changing SINCLAIR block (0x11) speed to 2074 Bd:

```
tmzedit set tape.tmz 3 --sinclair-speed 2074 -o tape_sinclair.tmz
```

Setting custom pulse widths on a block (converts 0x40 to 0x41):

```
tmzedit set tape.tmz 0 --pulse 4980/4980,2490/2490 -o tape_custom.tmz
```

Switching a custom pulse block back to table mode:

```
tmzedit set tape.tmz 0 --speed 2:1 -o tape_table.tmz
```

Converting block 0x41 back to 0x40 (by setting NORMAL 1:1):

```
tmzedit set tape.tmz 0 --format normal --speed 1:1 -o tape_normal.tmz
```

---

## validate - Integrity Validation

Checks the integrity of a TMZ/TZX file:
- Signature and version validity
- Known block IDs
- Group Start / Group End pairing
- Loop Start / Loop End pairing
- MZF headers in MZ blocks (ftype, fsize vs body_size)

Return code: 0 = valid, 1 = invalid (errors).

```
tmzedit validate <file>
```

### Example

```
tmzedit validate game.tmz
```

Output:

```
Validating: game.tmz
  Type: TMZ, version 1.0

  Blocks: 3, Errors: 0, Warnings: 0
  Result: VALID
```

Example with errors:

```
Validating: broken.tmz
  Type: TMZ, version 1.0
  ERROR: block [2] nested Group Start (depth 2)
  WARNING: block [5] MZF fsize=4096 but body_size=4000

  Blocks: 6, Errors: 1, Warnings: 1
  Result: INVALID
```

---

## Example Usage of --charset

Listing blocks with names in European UTF-8 character set:

```
tmzedit list game.tmz --charset utf8-eu
```
