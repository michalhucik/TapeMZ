# TMZ Format Specification v1.0

## 1. Introduction

TMZ (TapeMZ) is a file format for preserving the contents of cassette tapes from Sharp MZ computers (MZ-700, MZ-800, MZ-1500, MZ-80B) and ZX Spectrum. The format is based on TZX v1.20 (ZX Spectrum tape format) and extends it with blocks specific to the Sharp MZ platform.

TMZ is fully compatible with TZX at the block structure level. The "TapeMZ!" header (instead of "ZXTape!") ensures that extension block IDs (0x40-0x4F) are used for Sharp MZ specific data. A TZX format reader can read a TMZ file - it will skip unknown blocks thanks to the uniform block structure.

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

The ID range 0x40-0x4F is reserved in the TZX specification for extensions. TMZ uses these blocks for Sharp MZ specific data.

### 4.1 Block 0x40 - MZ Standard Data Block

A complete MZF recording in standard NORMAL 1200 Bd format (category 1). This block is reserved for the base 1200 Bd speed. For NORMAL FM at other speeds (2400, 2800, 3200 Bd etc. - category 2), block 0x41 with format=NORMAL and the appropriate speed ratio is used.

The player generates the complete tape signal from this block: LGAP + LTM + header + CHKH + SGAP + STM + body + CHKF.

```
Offset  Type   Description
0x00    BYTE   Block ID (0x40)
0x01    DWORD  Data length (= 131 + N)
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
0x0F    WORD   Long pulse high (us*100, 0=default from pulse set)
0x11    WORD   Long pulse low (us*100, 0=default)
0x13    WORD   Short pulse high (us*100, 0=default)
0x15    WORD   Short pulse low (us*100, 0=default)
0x17    BYTE   Flags (bit0=with header copy, bit1=with body copy)
0x18    128B   MZF header (raw 128 bytes)
0x98    WORD   Body size N
0x9A    N B    Body data
```

Pulse lengths with value 0 mean "use default from pulse set". The unit us*100 = 10 ns resolution, sufficiently precise for all known formats.

#### Interpretation of the "Speed" Field by Format

The "Speed" field (offset 0x08) is interpreted according to the "Format" field value:

| Format                            | Speed interpretation                                  | Range  |
|-----------------------------------|-------------------------------------------------------|--------|
| NORMAL, TURBO, FASTIPL, SINCLAIR  | en_CMTSPEED index (FM ratio)                          | 0-9    |
| FSK                               | en_MZCMT_FSK_SPEED (0=slowest, 6=fastest)             | 0-6    |
| SLOW                              | en_MZCMT_SLOW_SPEED (0=slowest, 4=fastest)            | 0-4    |
| DIRECT                            | ignored (0)                                            | 0      |
| CPM_TAPE                          | en_MZCMT_CPMTAPE_SPEED (0=1200, 1=2400, 2=3200 Bd)   | 0-2    |

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
| 6       | 7:3     | 2800 Bd  | Intercopy 10.2                    |
| 7       | 8:3     | 3200 Bd  | CP/M cmt.com                      |
| 8       | 9:7     | ~1543 Bd |                                   |
| 9       | 25:14   | ~2143 Bd |                                   |

#### SINCLAIR Format Speeds

The SINCLAIR format uses the same en_CMTSPEED ratios, but the base speed is 1400 Bd (not 1200 Bd as with Sharp MZ). Pulse lengths are derived from ZX Spectrum timing (pilot ~612 us, zero ~244 us, one ~488 us).

| Value   | Ratio   | Baudrate | Note                              |
|---------|---------|----------|-----------------------------------|
| 1       | 1:1     | 1400 Bd  | Standard ZX Spectrum speed        |
| 5       | 3:2     | 2100 Bd  |                                   |
| 8       | 9:7     | 1800 Bd  |                                   |
| 9       | 25:14   | 2500 Bd  |                                   |

Other ratios from en_CMTSPEED are also valid; the above are typical values.

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

CP/M Tape (Pezik/MarVan, Manchester encoding) has 3 fixed baud rates:

| Value   | Baudrate | Note                                  |
|---------|----------|---------------------------------------|
| 0       | 1200 Bd  | Compatible with ROM speed             |
| 1       | 2400 Bd  | Default for TAPE.COM                  |
| 2       | 3200 Bd  | Maximum speed                         |

#### Reference Load Time Comparison

Reference file: **Flappy ver 1.0A** (Flappy.mzf, 44161 bytes: 128B header + 44033B body).
Measured as WAV signal length generated from TMZ via tmz2wav (44100 Hz, pulseset MZ-800).
Time includes the complete tape signal including leader tone, GAP, tapemarks, and checksums.
For category 3 and 4 formats, the NORMAL FM preloader/loader (~16 s) is included.

**FM formats (NORMAL, TURBO, FASTIPL):**

| Speed             | NORMAL  | TURBO   | FASTIPL |
|-------------------|---------|---------|---------|
| 1:1 (1200 Bd)     | 291.5 s | 312.0 s | 312.3 s |
| 3:2 (1800 Bd)     | 193.7 s | 211.4 s | 213.9 s |
| 9:7 (~1543 Bd)    | 225.7 s | 244.4 s | 246.1 s |
| 2:1 (2400 Bd)     | 141.0 s | 157.3 s | 161.0 s |
| 25:14 (~2143 Bd)  | 163.6 s | 180.6 s | 183.7 s |
| 7:3 (~2800 Bd)    | 124.1 s | 140.0 s | 144.0 s |
| 8:3 (~3200 Bd)    | 111.0 s | 126.5 s | 130.8 s |
| 3:1 (3600 Bd)     |  97.8 s | 113.0 s | 117.6 s |

TURBO and FASTIPL are slower than NORMAL at the same speed because they contain a loader in the header (header is always in NORMAL 1:1, only the body is at the target speed).

**FSK (Frequency Shift Keying):**

| Speed    | Time    | Effective Bd |
|----------|---------|--------------|
| 0        |  58.8 s | ~8 400 Bd    |
| 1        |  56.1 s | ~9 000 Bd    |
| 2        |  53.3 s | ~9 700 Bd    |
| 3        |  48.1 s | ~11 300 Bd   |
| 4        |  40.1 s | ~15 200 Bd   |
| 5        |  37.4 s | ~17 200 Bd   |
| 6        |  34.6 s | ~19 900 Bd   |

**SLOW (quaternary):**

| Speed    | Time    | Effective Bd |
|----------|---------|--------------|
| 0        |  64.3 s | ~7 500 Bd    |
| 1        |  56.3 s | ~9 000 Bd    |
| 2        |  40.3 s | ~15 300 Bd   |
| 3        |  36.3 s | ~18 500 Bd   |
| 4        |  32.3 s | ~23 500 Bd   |

**DIRECT (direct bit writing):**

| Speed    | Time    | Effective Bd |
|----------|---------|--------------|
| 0        |  28.9 s | ~32 000 Bd   |

**CP/M Tape (Manchester):**

| Speed    | Time    |
|----------|---------|
| 0 (1200) | 298.2 s |
| 1 (2400) | 149.3 s |
| 2 (3200) | 116.2 s |

Effective Bd for FSK, SLOW, and DIRECT is calculated from the data section transfer time (total time minus ~16 s NORMAL FM preloader). The actual bit rate varies depending on data content, because bits 0 and 1 (or symbols 0-3) have different pulse lengths.

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

Specifies the target machine and its parameters. Affects interpretation of subsequent blocks.

```
Offset  Type   Description
0x00    BYTE   Block ID (0x43)
0x01    DWORD  Data length (= 6)
0x05    BYTE   Machine type (0=generic, 1=MZ-700, 2=MZ-800, 3=MZ-1500, 4=MZ-80B)
0x06    DWORD  CPU clock (Hz), e.g. 3546900 for MZ-800
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

TMZ block 0x45 stores the complete chunked recording - header and all chunks including their IDs. The player generates the complete tape signal from this block: LGAP + LTM + header + CHKH + (STM + chunk + CHK) * N.

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

### 4.7 Blocks 0x46-0x4F - Reserved

Reserved for future extensions. The reader must skip these blocks using the length after the ID.

## 5. Timing

### 5.1 TMZ MZ-Specific Blocks (0x40-0x4F)

Pulse lengths in TMZ MZ-specific blocks use the unit us*100 (1 unit = 10 ns). This timing is absolute - it does not depend on the target machine's CPU clock.

A value of 0 means "use default from pulse set".

### 5.2 Default Pulse Sets

| Parameter      | MZ-700/80K/80A | MZ-800/1500 | MZ-80B |
|----------------|----------------|-------------|--------|
| Long high (us) | ~464           | ~470        | ~333   |
| Long low (us)  | ~494           | ~494        | ~334   |
| Short high (us)| ~240           | ~240        | ~167   |
| Short low (us) | ~264           | ~278        | ~166   |

Precise values for MZ-800 (Intercopy 10.2, GDG ticks):

| Parameter   | GDG ticks | us      |
|-------------|-----------|---------|
| Long high   | 8335      | 470.330 |
| Long low    | 8760      | 494.308 |
| Short high  | 4356      | 245.802 |
| Short low   | 4930      | 278.204 |

### 5.3 Standard TZX Blocks (0x10-0x19)

For standard TZX blocks, timing remains in Z80 T-states at 3.5 MHz (per TZX specification). The player converts T-states to the target CPU clock according to the MZ Machine Info block (0x43), if present.

## 6. Conversion Strategy

```
MZF -> TMZ:    Block 0x40 (standard NORMAL 1:1) or 0x41 (other format/speed)
TMZ -> MZF:    Data extraction from 0x40/0x41 (lossless for standard)
WAV -> TMZ:    Signal analysis -> 0x40/0x41/0x45, or Direct Recording 0x15
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
| 3       | SINCLAIR | ZX Spectrum compatible encoding                                |
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
| 6       | 7:3     | 2800 Bd (Intercopy 10.2)     |
| 7       | 8:3     | 3200 Bd (CP/M cmt.com)       |
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
- A single file can contain both ZX Spectrum (blocks 0x10-0x15) and Sharp MZ (0x40-0x45) data

## 9. Verification

The following tests are recommended for verifying implementation correctness:

- **Parser**: load an existing TZX file, verify correct block skipping
- **Round-trip MZF**: mzf2tmz -> tmz2mzf, bit-perfect match of input and output data
- **Round-trip WAV**: mzf2tmz -> tmz2wav, comparison with direct output from mztape encoder
- **Player**: stream generation from TMZ, comparison with direct mztape output
- **Validation**: tmzedit validate for file integrity checking
