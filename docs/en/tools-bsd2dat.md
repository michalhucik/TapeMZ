# bsd2dat - BSD/BRD Data Export from TMZ

Exports BASIC data files from a TMZ file to a binary file. Reads
blocks 0x45 (MZ BASIC Data, TMZ v1.0) and blocks 0x46 with BSD chunk
parts (profile `BASIC-1Z016`, written by dat2bsd and wav2tmz).
Supports two export modes: solid (default) and chunks.

## Usage

```
bsd2dat <input.tmz> [options]
```

## Options

| Option | Value | Description |
|--------|-------|-------------|
| `--output` | `<path>` | Output file (solid) or directory (chunks). Default: derived from input |
| `--index` | `<N>` | Extract only the block at index N (0-based) |
| `--list` | - | List BSD blocks without extraction |
| `--chunks` | - | Chunks mode - each chunk as a separate file |
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

### Export Modes

**Solid (default)** - joins data from all chunks including the last one
(ID 0xFFFF carries data like the others) into a single binary file. Each
chunk contributes 256 bytes of data; the last chunk may contain zero padding.

Output naming (without `--output` the output is placed next to the input
file, not in the current directory):
- Single block with `--output`: exact name is used
- Single block without `--output`: input.dat
- Multiple blocks with `--output`: output_001.dat, output_002.dat, ... (in the `--output` directory)
- Multiple blocks without `--output`: input_001.dat, input_002.dat, ...

**Chunks (--chunks)** - creates a directory and saves each chunk
as a separate file named `prefix_NNNN.dat`, where NNNN
is the hexadecimal chunk ID, including the last chunk (FFFF).

Directory naming:
- Single block with `--output`: exact name is used
- Single block without `--output`: input_chunks/
- Multiple blocks: input_001_chunks/, input_002_chunks/, ...

## Examples

Listing BSD blocks in a file:

```
bsd2dat tape.tmz --list
```

Example `--list` output:

```
=== tape.tmz ===

File type: TMZ, Version: 1.1, Blocks: 2

BSD blocks (2):

  [  0] 0x45 MZ BASIC Data  "OLDDATA"  type=0x03 (BSD (BASIC data))  chunks=2  data=512 bytes
  [  1] 0x46 MZ File  "SAVEGAME"  type=0x04 (BRD (BASIC data, WOPEN#))  chunks=4  data=1024 bytes
```

Exporting BSD data to a single file:

```
bsd2dat tape.tmz
```

Exporting a specific block with a custom name:

```
bsd2dat tape.tmz --index 1 --output savegame.dat
```

Exporting individual chunks to a directory:

```
bsd2dat tape.tmz --chunks
```

Result: directory `tape_chunks/` containing:
- tape_0000.dat (256 B)
- tape_0001.dat (256 B)
- tape_0002.dat (256 B)
- tape_FFFF.dat (256 B, last chunk)

Exporting chunks with a custom directory:

```
bsd2dat tape.tmz --chunks --output savegame_chunks
```

Listing BSD blocks with names in European UTF-8 character set:

```
bsd2dat tape.tmz --list --charset utf8-eu
```
