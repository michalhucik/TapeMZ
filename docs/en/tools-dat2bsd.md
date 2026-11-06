# dat2bsd - Import binary data into TMZ as a BASIC data file

Imports any binary file into TMZ as a BASIC data file (BSD/BRD) in block
0x46 with profile `BASIC-1Z016`. The input data is split into 256-byte
chunks with sequential IDs (0x0000, 0x0001, ...). The last chunk has
ID 0xFFFF and carries the rest of the data.

Profile `BASIC-1Z016` plays the file the way BASIC 1Z-016 writes it to
tape (`WOPEN#`, `PRINT#`, `CLOSE#`): a tone and a tapemark before every
chunk, a copy after every chunk and a pause between chunks. The timing was
measured in the emulator and BASIC 1Z-016 reads such a file (`ROPEN#`,
`INPUT#`).

## Usage

```
dat2bsd <input.dat> <output.tmz> [options]
```

## Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `--name` | `<name>` | derived from input | Filename in the MZF header |
| `--ftype` | brd, bsd | brd | File type (0x04 or 0x03) |
| `--machine` | generic, mz700, mz800, mz1500, mz80b | mz800 | Target machine (information in the block only) |
| `--pause` | 0-65535 | 1000 | Pause after the last chunk in milliseconds |
| `--version` | - | - | Show program version |
| `--lib-versions` | - | - | Show library versions |

The version 1.0 options `--speed` and `--pulseset` are no longer valid
(error): profile `BASIC-1Z016` has a fixed timing.

### Details

**--ftype** - MZF file type:
- `brd` (0x04) - the type written by `WOPEN#` of BASIC 1Z-016 (a data file;
  the ROM monitor calls it BRD in the directory listing)
- `bsd` (0x03) - the type the ROM monitor calls BSD

**--name** - filename stored in the MZF header (max 17 Sharp MZ ASCII characters).
If not specified, it is derived from the input filename (without path and extension).

**Data for BASIC** - `INPUT#` reads data as `PRINT#` writes it: text
terminated by 0Dh, the end of the file is the character 1Ah. dat2bsd does
not change the data.

### Chunk structure

Each chunk is 258 bytes:
- 2 bytes: chunk ID (little-endian, 0x0000, 0x0001, ..., the last 0xFFFF)
- 256 bytes: data (the last chunk is padded with zeros)

Maximum input data size: 254 chunks x 256 bytes = 65,024 bytes (block 0x46
has at most 255 parts). An empty input gives one chunk with ID 0xFFFF.

## Examples

Basic import:

```
dat2bsd data.dat tape.tmz
```

Import with a custom name and type 0x03:

```
dat2bsd savegame.dat tape.tmz --name "SAVEGAME" --ftype bsd
```

Import with a longer pause after the file:

```
dat2bsd large_data.dat tape.tmz --pause 2000
```

Example output:

```
Imported: data.dat -> tape.tmz

  Filename   : "data"
  File type  : 0x04 (BRD)
  Input size : 1024 bytes
  Chunks     : 4 (last ID 0xFFFF)
  Machine    : MZ-800
  Pause      : 1000 ms
  Block      : 0x46 (MZ File, profile BASIC-1Z016)
  TMZ size   : 2880 bytes
```
