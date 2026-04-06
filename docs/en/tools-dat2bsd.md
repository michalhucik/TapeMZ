# dat2bsd - Binary Data Import into TMZ as BSD/BRD Block

Imports an arbitrary binary file into TMZ format as block 0x45
(MZ BASIC Data). Input data is sliced into 256-byte chunks
with sequential IDs (0x0000, 0x0001, ...) and a termination chunk
with ID 0xFFFF.

Used for storing BSD (BASIC data) or BRD (BASIC read-after-run)
files in a TMZ archive.

## Usage

```
dat2bsd <input.dat> <output.tmz> [options]
```

## Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `--name` | `<name>` | derived from input | Filename in MZF header |
| `--ftype` | bsd, brd | bsd | File type (0x03 BSD or 0x04 BRD) |
| `--machine` | generic, mz700, mz800, mz1500, mz80b | mz800 | Target computer |
| `--pulseset` | 700, 800, 80b, auto | auto | Pulse set (auto = based on machine) |
| `--speed` | 1:1, 2:1, 2:1cpm, 3:1, 3:2, 7:3, 8:3, 9:7, 25:14 | 1:1 | Speed ratio |
| `--pause` | 0-65535 | 1000 | Pause after block in milliseconds |
| `--version` | - | - | Show program version |
| `--lib-versions` | - | - | Show library versions |

### Details

**--ftype** - MZF file type:
- `bsd` (0x03) - BASIC Data - standard data file
- `brd` (0x04) - BASIC Read-after-run - data loaded after program start

**--name** - filename stored in the MZF header (max 17 characters Sharp MZ ASCII).
If not specified, it is derived from the input filename (without path and extension).

### Chunk Structure

Each chunk is 258 bytes:
- 2 bytes: chunk ID (little-endian, 0x0000-0xFFFE for data, 0xFFFF for terminator)
- 256 bytes: data (the last data chunk may be padded with zeros)

Maximum input data size: 65534 chunks x 256 bytes = 16,776,704 bytes.

## Examples

Basic import:

```
dat2bsd data.dat tape.tmz
```

Import with custom name and BRD type:

```
dat2bsd savegame.dat tape.tmz --name "SAVEGAME" --ftype brd
```

Import for MZ-700 with speed 2:1:

```
dat2bsd data.bin tape.tmz --machine mz700 --speed 2:1
```

Import with longer pause:

```
dat2bsd large_data.dat tape.tmz --pause 2000
```

Example output:

```
Imported: data.dat -> tape.tmz

  Filename   : "data"
  File type  : 0x03 (BSD)
  Input size : 1024 bytes
  Chunks     : 4 data + 1 termination = 5 total
  Machine    : MZ-800
  Pulseset   : MZ-800/1500
  Speed      : 1:1
  Pause      : 1000 ms
  Block      : 0x45 (MZ BASIC Data)
  TMZ size   : 1310 bytes
```
