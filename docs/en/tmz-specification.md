# TMZ Format Specification v1.0

## 1. Introduction

TMZ (TapeMZ) is a file format for preserving the contents of cassette tapes from Sharp MZ computers (MZ-700, MZ-800, MZ-1500, MZ-80B) and ZX Spectrum. The format is based on TZX v1.20 (ZX Spectrum tape format) and extends it with blocks specific to the Sharp MZ platform.

TMZ is fully compatible with TZX at the block structure level. The "TapeMZ!" header (instead of "ZXTape!") distinguishes a file in which block IDs 0x40-0x4F carry Sharp MZ specific data (see chapter 4 - TZX does not reserve this range for extensions). A TZX format reader can read a TMZ file - it will skip unknown blocks thanks to the uniform block structure.

## 2. File Header (10 bytes)

```
Offset  Size      Value      Description
0x00    7         "TapeMZ!"  Signature (ASCII)
0x07    1         0x1A       EOF marker (end-of-file)
0x08    1         1          Major version
0x09    1         0          Minor version
```

The "TapeMZ!" signature distinguishes TMZ from TZX ("ZXTape!"). The EOF marker 0x1A stops the DOS TYPE command when displaying the file (a convention inherited from TZX). Version 1.0 is the first released version of the specification.

## 3. Block Structure

### 3.1 General Conventions
- Each block starts with a 1-byte ID
- Audio blocks (0x10-0x15, 0x18, 0x19) have block-specific headers with their own length definition (see TZX v1.20 specification for each type)
- Control and information blocks (0x20-0x5A) mostly start with a DWORD LE length (but some have special formats: 0x20=2B, 0x21=1B+text, 0x22/0x25/0x27=0B, etc.)
- TMZ extension blocks (0x40-0x4F) always have a DWORD LE length after the ID
- The reader skips unknown blocks using the length field
- All multi-byte fields are in little-endian (LE) order

### 3.2 Standard TZX Blocks

All TZX v1.20 blocks (0x10-0x5A) are valid in TMZ without modification. Their semantics and timing remain according to the TZX specification - T-states at 3.5 MHz (ZX Spectrum Z80).

See the TZX v1.20 specification for the complete list.

## 4. TMZ Extension Blocks (0x40-0x4F)

TMZ uses the ID range 0x40-0x4F for Sharp MZ specific data. TZX v1.20 does not reserve this range for extensions: ID 0x40 was the TZX Snapshot block (removed in v1.20, the ID is reserved) and IDs 0x41-0x4F are not defined by TZX. TZX only gives the general rule that blocks added after v1.10 carry a 4-byte length after the ID, so an unknown block can be skipped. A collision with a future TZX version is not excluded; the "TapeMZ!" header tells the files apart.

### 4.1 Block 0x40 - MZ Standard Data Block

A complete MZF recording in standard NORMAL 1200 Bd format (category 1). This block is reserved for the base 1200 Bd speed. For NORMAL FM at other speeds (2400, 2800, 3200 Bd etc. - category 2), block 0x41 with format=NORMAL and the appropriate speed ratio is used.

The player generates the complete tape signal from this block: LGAP + LTM + header + CHKH + SGAP + STM + body + CHKF.

```
Offset  Type   Description
0x00    BYTE   Block ID (0x40)
0x01    DWORD  Data length (= 134 + N)
0x05    BYTE   Target machine (0=generic, 1=MZ-700, 2=MZ-800, 3=MZ-1500, 4=MZ-80B)
0x06    BYTE   Pulse set (0=MZ-700, 1=MZ-800, 2=MZ-80B)
0x07    WORD   Pause after block (ms), 0 = no pause
0x09    128B   MZF header (raw 128 bytes in original format)
0x89    WORD   Body size N (redundant with MZF header.fsize)
0x8B    N B    Body data
```

The "Target machine" field specifies which model the recording is intended for. The "Pulse set" field determines which pulse lengths are used for signal generation. The redundant "Body size" field allows fast access without parsing the MZF header.

### 4.2 Block 0x41 - MZ Turbo Data Block

An MZF recording with configurable speed, format, and timing. Used for:
- Category 2: NORMAL FM at non-standard speed (2400, 2800, 3200 Bd etc.)
- Category 3: loader in header (TURBO, FASTIPL)
- Category 4: loader as program (FSK, SLOW, DIRECT)
- CP/M Tape: Pezik/MarVan Manchester encoding

The "Format" and "Speed" fields determine what signal the player generates.

```
Offset  Type   Description
0x00    BYTE   Block ID (0x41)
0x01    DWORD  Data length
0x05    BYTE   Target machine (0=generic, 1=MZ-700, 2=MZ-800, 3=MZ-1500, 4=MZ-80B)
0x06    BYTE   Pulse set (0=MZ-700, 1=MZ-800, 2=MZ-80B)
0x07    BYTE   Format (see en_TMZ_FORMAT)
0x08    BYTE   Speed (interpretation depends on format - see table below)
0x09    WORD   LGAP length (number of short pulses, 0=default for format)
0x0B    WORD   SGAP length (number of short pulses, 0=default)
0x0D    WORD   Pause after block (ms), 0 = no pause
0x0F    WORD   Long pulse high (0.1 us, 0=default from pulse set)
0x11    WORD   Long pulse low (0.1 us, 0=default)
0x13    WORD   Short pulse high (0.1 us, 0=default)
0x15    WORD   Short pulse low (0.1 us, 0=default)
0x17    BYTE   Flags (bit0=with header copy, bit1=with body copy)
0x18    128B   MZF header (raw 128 bytes)
0x98    WORD   Body size N
0x9A    N B    Body data
```

Pulse lengths with value 0 mean "use default from pulse set". The unit is 0.1 us (e.g. 4980 = 498.0 us).

#### Interpretation of the "Speed" Field by Format

The "Speed" field (offset 0x08) is interpreted according to the "Format" field value:

| Format                            | Speed interpretation                                  | Range  |
|-----------------------------------|-------------------------------------------------------|--------|
| NORMAL, TURBO, FASTIPL            | en_CMTSPEED index (FM ratio)                          | 0-9    |
| FSK                               | en_MZCMT_FSK_SPEED (0=slowest, 6=fastest)             | 0-6    |
| SLOW                              | en_MZCMT_SLOW_SPEED (0=slowest, 4=fastest)            | 0-4    |
| DIRECT                            | ignored (0)                                            | 0      |
| CPM_TAPE                          | level: 2 = 3200 Bd, otherwise 2400 Bd (see below)     | 0-2    |
| SINCLAIR                          | not used (see below)                                  | -      |

#### FM Format Speed Ratios (NORMAL, TURBO, FASTIPL)

FM formats use en_CMTSPEED as a ratio relative to the base 1200 Bd speed. The speed determines pulse length scaling - a higher ratio = shorter pulses = higher baudrate.

| Value   | Ratio   | Baudrate | Note                              |
|---------|---------|----------|-----------------------------------|
| 0       | (none)  | -        | Invalid value                     |
| 1       | 1:1     | 1200 Bd  | Standard speed (ROM)              |
| 2       | 2:1     | 2400 Bd  | Double                            |
| 3       | 2:1 CPM | 2400 Bd  | CP/M variant (cmt.com)            |
| 4       | 3:1     | 3600 Bd  | Triple                            |
| 5       | 3:2     | 1800 Bd  |                                   |
| 6       | 7:3     | 2800 Bd  | close to Intercopy 10.2 "2800 Bd" |
| 7       | 8:3     | 3200 Bd  | close to Intercopy 10.2 "3200 Bd" |
| 8       | 9:7     | ~1543 Bd |                                   |
| 9       | 25:14   | ~2143 Bd |                                   |

The Baudrate column is a nominal label (a multiple of 1200 Bd); real writers differ from it.

#### SINCLAIR Format

The SINCLAIR format (3) is not used in block 0x41; a ZX Spectrum recording is stored as TZX blocks 0x10/0x11. The player does not play block 0x41 with the SINCLAIR format and the tools do not create it.

#### FSK Format Speeds

FSK (Frequency Shift Keying) has 7 speed levels. The speed is an index into the pulse length table in the Z80 loader. Lengths are defined in sample counts at the reference frequency of 44100 Hz (long pulse = bit "1", short pulse = bit "0").

| Value   | Long (low+high)    | Short (low+high)    | Ratio L:S | Description    |
|---------|--------------------|---------------------|-----------|----------------|
| 0       | 6+2 = 8 samples    | 2+2 = 4 samples     | 2.00      | Slowest        |
| 1       | 5+2 = 7 samples    | 2+2 = 4 samples     | 1.75      |                |
| 2       | 4+2 = 6 samples    | 2+2 = 4 samples     | 1.50      |                |
| 3       | 4+2 = 6 samples    | 1+2 = 3 samples     | 2.00      |                |
| 4       | 3+2 = 5 samples    | 1+1 = 2 samples     | 2.50      |                |
| 5       | 2+2 = 4 samples    | 1+1 = 2 samples     | 2.00      |                |
| 6       | 2+1 = 3 samples    | 1+1 = 2 samples     | 1.50      | Fastest        |

At a different sample rate, sample counts scale proportionally: result = round(ref * rate / 44100).

#### SLOW Format Speeds

SLOW (quaternary encoding, 2 bits per pulse) has 5 speed levels. Each byte is encoded as 4 symbols (0-3), each symbol having a different pulse length. Lengths are in sample counts at 44100 Hz.

| Value   | Symbol 0 (L+H) | Symbol 1 (L+H) | Symbol 2 (L+H) | Symbol 3 (L+H) | Description  |
|---------|-----------------|-----------------|-----------------|-----------------|--------------|
| 0       | 3+3 = 6         | 6+6 = 12        | 9+9 = 18        | 12+12 = 24      | Slowest      |
| 1       | 3+3 = 6         | 5+5 = 10        | 7+7 = 14        | 9+9 = 18        |              |
| 2       | 1+2 = 3         | 3+3 = 6         | 4+5 = 9         | 6+6 = 12        |              |
| 3       | 1+1 = 2         | 2+3 = 5         | 3+5 = 8         | 4+7 = 11        |              |
| 4       | 1+1 = 2         | 2+2 = 4         | 3+3 = 6         | 4+4 = 8         | Fastest      |

At a different sample rate, sample counts scale proportionally.

#### DIRECT Format Speed

The DIRECT format (direct bit writing) has no configurable speed. The "Speed" field is always 0 (ignored). The actual transfer speed is determined by the sample rate - each bit corresponds to one signal sample (HIGH=1, LOW=0), with synchronization overhead bits (12 samples per byte: 8 data + 4 synchronization).

#### CP/M Tape Format Speeds

CP/M Tape (Pezik/MarVan, Manchester encoding). The player always converts block 0x41 with the CPM_TAPE format to block 0x46 with profile `PEZIK-2.0A` (see `tmz-v1.1-specification.md` chapter 9.2). A body longer than 16384 B is not converted and the block is not played (block error). New CPM-TAPE recordings are written by `mzf2tmz` as block 0x46.

| Value   | Played as                             |
|---------|---------------------------------------|
| 2       | PEZIK-2.0A 3200 Bd                    |
| other   | PEZIK-2.0A 2400 Bd                    |

TAPE.COM writes only 2400 and 3200 Bd; a 1200 Bd level does not exist.

### 4.3 Block 0x42 - MZ Extra Body Block

An additional data block for multi-part programs. Used in conjunction with block 0x44 (MZ Loader Block) - the loader is loaded at standard speed and subsequently reads body blocks in a faster format.

```
Offset  Type   Description
0x00    BYTE   Block ID (0x42)
0x01    DWORD  Data length
0x05    BYTE   Format (see en_TMZ_FORMAT)
0x06    BYTE   Speed ratio (en_CMTSPEED index)
0x07    WORD   Pause after block (ms)
0x09    WORD   Data size N
0x0B    N B    Data
```

### 4.4 Block 0x43 - MZ Machine Info

Specifies the target machine and its parameters. The CPU clock is the default time base of the parts of the 0x46 blocks that follow it and have no TIMEBASE tag (see `tmz-v1.1-specification.md` chapter 5.3). It does not affect the timing of TZX blocks.

```
Offset  Type   Description
0x00    BYTE   Block ID (0x43)
0x01    DWORD  Data length (= 6)
0x05    BYTE   Machine type (0=generic, 1=MZ-700, 2=MZ-800, 3=MZ-1500, 4=MZ-80B)
0x06    DWORD  CPU clock (Hz), e.g. 3546895 for MZ-800
0x0A    BYTE   ROM version (0=unknown)
```

### 4.5 Block 0x44 - MZ Loader Block

A loader program (TURBO/FASTIPL/FSK/SLOW/DIRECT) that is loaded at standard speed and subsequently switches to faster reading for subsequent blocks (0x42).

```
Offset  Type   Description
0x00    BYTE   Block ID (0x44)
0x01    DWORD  Data length
0x05    BYTE   Loader type (see en_TMZ_LOADER_TYPE)
0x06    BYTE   Speed for subsequent body blocks (en_CMTSPEED index)
0x07    WORD   Loader data size M
0x09    128B   MZF header (with loader code in the comment section)
0x89    M B    Loader body
```

### 4.6 Block 0x45 - MZ BASIC Data Block

A BASIC data recording (BSD/BRD). Unlike a standard MZF recording where data size is given by the fsize field, in a BSD/BRD recording fsize=0 and data is divided into 258B chunks. Each chunk contains a 2B ID (LE) and 256B of data. Chunks are stored on tape as separate body blocks with short tapemarks.

TMZ block 0x45 stores the complete chunked recording - header and all chunks including their IDs. The player (TMZ v1.1) always converts this block to block 0x46 with profile `BASIC-1Z016` (BASIC 1Z-016 timing measured in the emulator): the header and, for every chunk, its own 11000 S tone, short tapemark, chunk, checksum and copy; a pause between the segments as after the motor stops. The pulse set of the block is not used. A block with more than 254 chunks is not played.

```
Offset  Type   Description
0x00    BYTE   Block ID (0x45)
0x01    DWORD  Data length
0x05    BYTE   Target machine (0=generic, 1=MZ-700, 2=MZ-800, 3=MZ-1500, 4=MZ-80B)
0x06    BYTE   Pulse set (0=MZ-700, 1=MZ-800, 2=MZ-80B)
0x07    WORD   Pause after block (ms)
0x09    128B   MZF header (ftype=0x03 or 0x04, fsize/fstrt/fexec=0)
0x89    WORD   Number of chunks N
0x8B    N*258B Chunks (each: 2B ID LE + 256B data)
```

Chunk structure:
- First chunk: ID = 0x0000
- Subsequent chunks: ID increments (0x0001, 0x0002, ...)
- Last chunk: ID = 0xFFFF (termination marker)
- Total BASIC data size = N * 256 bytes

### 4.7 Block 0x46 - MZ File

One MZF file and the list of its tape parts with parameters in TLV (TMZ v1.1). The block is described in the document `tmz-v1.1-specification.md`.

### 4.8 Blocks 0x47-0x4F - Reserved

Reserved for future extensions. The reader must skip these blocks using the length after the ID.

## 5. Timing

### 5.1 TMZ MZ-Specific Blocks (0x40-0x4F)

Custom pulse lengths in block 0x41 use the unit 0.1 us (e.g. 4980 = 498.0 us). This timing is absolute - it does not depend on the target machine's CPU clock. A value of 0 means "use default from pulse set".

Times in blocks 0x46 are in units of the time base of the part (see `tmz-v1.1-specification.md` chapter 5.3).

### 5.2 Default Pulse Sets

| Parameter       | MZ-700/80K/80A | MZ-800/1500 | MZ-80B |
|-----------------|----------------|-------------|--------|
| Long high (us)  | 504            | 498         | 333    |
| Long low (us)   | 504            | 498         | 334    |
| Short high (us) | 252            | 249         | ~167   |
| Short low (us)  | 252            | 249         | 166    |

The table lists the default v1.0 pulse sets used by the encoders. The sets are symmetric (HIGH = LOW, except MZ-80B). The values for MZ-700/80K/80A and MZ-80B have no documented source [unverified]. The FSK, SLOW and DIRECT preloaders use a different MZ-700 set: LONG 464/494 µs, SHORT 240/264 µs [unverified].

The MZ-800 ROM writes asymmetric pulses. The nominal values per the hardware documentation are SHORT 240/278 us and LONG 470/494 us (HIGH/LOW). The player plays block 0x40 with the MZ-800 set with profile `ROM-800-NOMINAL` (see `tmz-v1.1-specification.md` chapter 9.2).

### 5.3 Standard TZX Blocks (0x10-0x19)

For standard TZX blocks, timing remains in Z80 T-states at 3.5 MHz (per TZX specification). The MZ Machine Info block (0x43) does not affect them.

## 6. Conversion Strategy

```
MZF -> TMZ:    Block 0x40 (standard NORMAL 1:1) or 0x41 (other format/speed)
TMZ -> MZF:    Data extraction from 0x40/0x41 (lossless for standard)
WAV -> TMZ:    Signal analysis -> 0x40/0x41/0x46, or Direct Recording 0x15
TMZ -> WAV:    Player generates audio -> WAV export (via cmt_stream)
TZX -> TMZ:    Header change + content validation
TMZ -> TZX:    Header change (MZ blocks remain, TZX reader skips them)
TAP -> TMZ:    TAP block conversion to TZX 0x10 (Standard Speed Data)
TMZ -> TAP:    Extraction of blocks 0x10
```

MZF -> TMZ -> MZF conversion is lossless for standard data (round-trip). WAV -> TMZ conversion may be lossy depending on the method.

## 7. Enums and Constants

### 7.1 Target Machine (en_TMZ_MACHINE)

| Value   | Meaning                     |
|---------|-----------------------------|
| 0       | Generic (unspecified)       |
| 1       | MZ-700                      |
| 2       | MZ-800                      |
| 3       | MZ-1500                     |
| 4       | MZ-80B                      |

### 7.2 Pulse Set (en_TMZ_PULSESET)

| Value   | Meaning                      |
|---------|------------------------------|
| 0       | MZ-700 (MZ-700, MZ-80K, MZ-80A) |
| 1       | MZ-800 (MZ-800, MZ-1500)    |
| 2       | MZ-80B                       |

### 7.3 Recording Format (en_TMZ_FORMAT)

| Value   | Name     | Description                                                   |
|---------|----------|---------------------------------------------------------------|
| 0       | NORMAL   | Standard 1200 Bd FM modulation (1 bit = 1 pulse)              |
| 1       | TURBO    | Proprietary fast encoding (TurboCopy)                          |
| 2       | FASTIPL  | Fast IPL encoding with $BB prefix (Intercopy)                  |
| 3       | SINCLAIR | ZX Spectrum encoding; not used in block 0x41 (chapter 4.2)     |
| 4       | FSK      | Frequency Shift Keying (carrier frequency change)              |
| 5       | SLOW     | 2 bits per pulse, more compact than FM                         |
| 6       | DIRECT   | Direct bit writing without modulation, fastest transfer        |
| 7       | CPM_TAPE | Pezik/MarVan Manchester encoding (ZTAPE/TAPE.COM under CP/M)  |

### 7.4 FM Format Speed Ratios (en_CMTSPEED)

| Value   | Ratio   | Baudrate at 1200 Bd base     |
|---------|---------|------------------------------|
| 0       | (none)  | invalid value                |
| 1       | 1:1     | 1200 Bd (standard)           |
| 2       | 2:1     | 2400 Bd                      |
| 3       | 2:1 CPM | 2400 Bd (CP/M variant)       |
| 4       | 3:1     | 3600 Bd                      |
| 5       | 3:2     | 1800 Bd                      |
| 6       | 7:3     | 2800 Bd                      |
| 7       | 8:3     | 3200 Bd                      |
| 8       | 9:7     | ~1543 Bd                     |
| 9       | 25:14   | ~2143 Bd                     |

### 7.5 Loader Type (en_TMZ_LOADER_TYPE)

| Value   | Name         | Description                      |
|---------|--------------|----------------------------------|
| 0       | TURBO_1_0    | Turbo loader version 1.0         |
| 1       | TURBO_1_2x   | Turbo loader version 1.2x        |
| 2       | FASTIPL_V2   | Fast-IPL loader version 2        |
| 3       | FASTIPL_V7   | Fast-IPL loader version 7        |
| 4       | FSK          | FSK loader                       |
| 5       | SLOW         | SLOW loader                      |
| 6       | DIRECT       | DIRECT loader                    |

## 8. Compatibility

### 8.1 TZX Compatibility
- A TMZ reader must be able to read files with the "ZXTape!" header (native TZX)
- TMZ blocks (0x40-0x4F) are automatically skipped by a TZX reader (thanks to the uniform block structure with DWORD length)
- For TZX -> TMZ conversion, it suffices to change the signature in the header

### 8.2 Backward Compatibility
- A TMZ file without MZ-specific blocks is functionally equivalent to a TZX file
- The player must support both header types (TapeMZ! and ZXTape!)
- A single file can contain both ZX Spectrum (blocks 0x10-0x15) and Sharp MZ (0x40-0x46) data

## 9. Verification

The following tests are recommended for verifying implementation correctness:

- **Parser**: load an existing TZX file, verify correct block skipping
- **Round-trip MZF**: mzf2tmz -> tmz2mzf, bit-perfect match of input and output data
- **Round-trip WAV**: mzf2tmz -> tmz2wav, comparison with direct output from mztape encoder
- **Player**: stream generation from TMZ, comparison with direct mztape output
- **Validation**: tmzedit validate for file integrity checking
