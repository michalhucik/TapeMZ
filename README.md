# TapeMZ - Tape Archive Format for Sharp MZ

## 1. Introduction

TapeMZ (TMZ) is a new file format for preserving the contents of cassette tapes from Sharp MZ computers (MZ-700, MZ-800, MZ-1500, MZ-80B) and ZX Spectrum. It was created in response to the long-standing absence of a unified format capable of faithfully capturing the complete contents of a tape - including non-standard speeds, turbo loaders, proprietary encodings, and multi-block programs.

The TMZ format is based on TZX v1.20 (ZX Spectrum tape image format) and extends it with blocks specific to the Sharp MZ platform. This makes it fully compatible with the existing TZX ecosystem while providing native support for all known Sharp MZ tape formats.

The project includes a set of C libraries for working with tape formats, a WAV analyzer capable of automatically decoding recordings from real cassette tapes, and eleven CLI tools for conversion, editing, and analysis.

## 2. Motivation

### 2.1 Why a New Format?

Existing formats for Sharp MZ have fundamental limitations:

- **MZF/MZT** - store only raw data (128B header + body). They carry no information about signal timing, non-standard formats, turbo loaders, or metadata. An MZF file with ftype=0x03 (BSD) and fsize=0 loses all BASIC data because the chunking protocol is not part of the format.

- **WAV** - preserves the exact analog signal but produces enormous files (tens of MB per program), does not allow structured editing, and depends on recording quality.

Neither of them can store an entire tape with programs in different formats and speeds, with metadata and the ability for lossless editing, in a single file.

### 2.2 Why TZX as the Foundation?

The TZX format (ZX Spectrum tape image) is the de facto standard for tape recording archival in the retro computing community. Its block structure is modular, extensible, and includes mechanisms for:
- Standard and non-standard data blocks
- Direct signal recording (Direct Recording)
- Metadata (Text Description, Archive Info)
- Logical grouping (Group Start/End)
- Pauses and control blocks

TZX specification v1.20 also explicitly reserves the ID range 0x40-0x4F for future extensions. TMZ uses precisely this range for Sharp MZ specific blocks.

Compatibility with TZX means that:
- A TMZ reader can play pure TZX files (ZX Spectrum)
- A TZX reader automatically skips unknown MZ blocks (thanks to the uniform length convention)
- A single file can contain both ZX Spectrum and Sharp MZ recordings
- Programs using CMT extensions in various formats (ZX, CP/M Tape, Sharp MZ) can coexist on a single virtual tape

### 2.3 Absence of Analytical Tools

Another key reason for this project was the complete absence of analytical programs for converting WAV recordings from Sharp MZ cassette tapes into structured formats. While the ZX Spectrum community has a range of tools (audio2tape, tzxwav, wav2pzx, maketzx), for Sharp MZ only isolated or proprietary tools existed, and there was no comparable program capable of:

- Automatically detecting the recording format (NORMAL, TURBO, FASTIPL, FSK, SLOW, DIRECT, BSD, CP/M Tape, ZX Spectrum)
- Decoding data from a WAV signal with checksum verification
- Exporting the result into a structured format

Existing tools such as mzftools (mz-fuzzy) or cmttool supported only the basic NORMAL format. Intercopy 10.2 (Sharp MZ native copier) did contain sophisticated auto-detection, but it ran only on real hardware.

## 3. Sharp MZ Tape Formats

Sharp MZ computers store data on magnetic tape using several different encoding methods. All of them use the built-in cassette interface controlled via PPI 8255 (SENSE input for reading, MOTOR for motor control).

### 3.1 Recording Categories

On the Sharp MZ platform, there are 4 basic categories of program recording:

#### Category 1: Sharp Standard (NORMAL 1200 Bd)

Standard MZF recording composed of two blocks (header and body), both at 1200 Bd speed. It is read by the standard ROM monitor. Encoding: FM modulation (bit "1" = long pulse, bit "0" = short pulse), 8 bits/byte MSB first, 1 stop bit.

Structure on tape:
```
[Leader tone]  22000 short pulses (synchronization tone)
[Tape mark]    40 long + 40 short pulses + 1 short
[Header]       128 bytes (MZF header)
[Checksum]     2 bytes (16-bit population count)
[Gap]          11000 short pulses
[Tape mark]    20 long + 20 short
[Body]         N bytes (data)
[Checksum]     2 bytes
```

This category also includes the **BSD/BRD** chunking format (BASIC data), where the header is followed by a sequence of 258B chunks (2B ID + 256B data), each with its own tapemark and checksum, terminated by a chunk with ID=0xFFFF.

#### Category 2: NORMAL FM at Non-Standard Speed

Same structure as category 1, but at a different speed (2400, 2800, 3200+ Bd). Both blocks (header and body) have identical speed. Only a modified ROM, copiers (Intercopy), or special programs can read them.

This category also includes **CP/M CMT** (cmt.com under CP/M, 2400 Bd) and **MZ-80B** (custom pulse set, 1800 Bd base).

#### Category 3: Loader in Header (Comment Field)

The recording appears as category 1, but a short program (loader) was inserted into the comment field of the MZF header. SIZE=0, EXEC points into the comment area. ROM loads only the header (no body) and launches the loader.

Formats in this category:
- **FASTIPL** - $BB prefix in header block, V02/V07 loader (Intercopy, Marek Šmihla - NIPSOFT)

#### Category 4: Loader as a Separate Program

The preloader consists of a header + body in NORMAL FM 1:1. ROM loads both header and body, then launches the loader. The loader patches ROM routines and reads user data from tape in the target format.

Formats in this category:
- **TURBO** (TurboCopy variant) - 90B loader at $D400, fsize=90, fstrt/fexec=$D400, identified by signature in cmnt[0..6] (TurboCopy, Michal Kreidl). The loader patches ROM speed and calls standard CMT read ($002A). TURBO data = body-only (STM tapemark + body + CRC, no header).
- **FSK** (Frequency Shift Keying) - bit determined by cycle frequency, 7 speed levels
- **SLOW** (quaternary) - 2 bits per pulse (4 symbols), 5 speed levels
- **DIRECT** (direct bit writing) - 1 sample = 1 bit, speed determined by sample rate

#### Outside Categories: CP/M Tape

CP/M Tape (Pezik/MarVan, ZTAPE/TAPE.COM) uses a completely different protocol - Manchester encoding, LSB first, no stop bits, pilot "011". It runs outside Sharp ROM under CP/M. Supports 1200/2400/3200 Bd.

### 3.2 Format Overview

| Format    | Category  | Modulation   | Speed       | Loader  | TMZ Block |
|-----------|-----------|--------------|-------------|---------|-----------|
| NORMAL    | 1         | FM           | 1200 Bd     | none    | 0x40      |
| NORMAL    | 2         | FM           | 2400+ Bd    | none    | 0x41      |
| CP/M CMT  | 2         | FM           | 2400 Bd     | none    | 0x41      |
| MZ-80B    | 2         | FM           | 1800 Bd     | none    | 0x41      |
| BSD/BRD   | 1/2       | FM (chunked) | 1200+ Bd    | none    | 0x45      |
| TURBO     | 4         | FM           | config.     | $D400   | 0x41      |
| FASTIPL   | 3         | FM + $BB     | config.     | comment | 0x41      |
| FSK       | 4         | FSK          | 7 levels    | program | 0x41      |
| SLOW      | 4         | quaternary   | 5 levels    | program | 0x41      |
| DIRECT    | 4         | bitwise      | sample rate | program | 0x41      |
| CP/M Tape | (outside) | Manchester   | 1200-3200   | CP/M    | 0x41      |
| SINCLAIR  | -         | custom       | 1400 Bd     | -       | TZX 0x10  |

### 3.3 Pulse Sets

Pulse lengths differ by computer model:

| Parameter    | MZ-700/80K/80A | MZ-800/1500 | MZ-80B   |
|--------------|----------------|-------------|----------|
| Long high    | ~464 us        | ~470 us     | ~333 us  |
| Long low     | ~494 us        | ~494 us     | ~334 us  |
| Short high   | ~240 us        | ~240 us     | ~167 us  |
| Short low    | ~264 us        | ~278 us     | ~166 us  |

### 3.4 Checksum

All Sharp MZ formats use a 16-bit checksum based on population count - the sum of one-bits across all bytes of the block. The mechanism is simple but sufficient for detecting most errors when reading from tape.

## 4. TZX Format - Foundation for TMZ

### 4.1 What is TZX

TZX (ZX Spectrum Tape Image) is the standard format for preserving ZX Spectrum cassette contents. It was designed by Tomasz Slanina in 1997 and has since become the de facto standard in the retro computing community. The current version is v1.20.

### 4.2 TZX Structure

A TZX file consists of:
- **Header** (10 bytes): signature "ZXTape!", EOF marker 0x1A, version (major.minor)
- **Block sequence**: each block starts with a 1-byte ID, followed by length and data

Block types:
- **Audio blocks** (0x10-0x15, 0x18, 0x19): carry audio data or parameters for signal generation
- **Control blocks** (0x20-0x27): pauses, groups, loops
- **Information blocks** (0x30-0x35, 0x5A): metadata, archive information
- **Extension blocks** (0x40-0x4F): reserved for extensions - this is where TMZ blocks are placed

### 4.3 TMZ Extension

TMZ uses the signature "TapeMZ!" instead of "ZXTape!" and defines 6 types of MZ-specific blocks in the range 0x40-0x45:

| ID   | Name               | Purpose                                         |
|------|--------------------|------------------------------------------------|
| 0x40 | MZ Standard Data   | NORMAL 1200 Bd recording (complete MZF)         |
| 0x41 | MZ Turbo Data      | Configurable format/speed/timing                |
| 0x42 | MZ Extra Body      | Additional data block for multi-block programs  |
| 0x43 | MZ Machine Info    | Target machine, CPU clock, ROM version          |
| 0x44 | MZ Loader          | Preloader program (TURBO/FASTIPL/FSK/SLOW/...)  |
| 0x45 | MZ BASIC Data      | BSD/BRD chunking format                         |

Blocks 0x46-0x4F are reserved for future extensions.

The formal TMZ format specification is in a separate document: **[tmz-specification.md](https://github.com/michalhucik/TapeMZ/blob/main/docs/en/tmz-specification.md)**

## 5. WAV Analyzer

### 5.1 Overview

The WAV analyzer is a key component of the project - it enables automatic analysis and decoding of recordings from real Sharp MZ cassette tapes. It is a five-layer pipeline processing raw PCM audio signal into structured data.

### 5.2 Pipeline Architecture

```
WAV file (PCM 8/16/24/32/64-bit)
  |
  v
[Layer 0] Signal Preprocessing
  |-- DC offset removal
  |-- High-pass filter
  |-- Amplitude normalization
  |
  v
[Layer 1] Pulse Extraction
  |-- Zero-crossing detector (default)
  |-- Schmitt trigger (optional, for noisy recordings)
  |
  v
[Layer 2] Leader Tone Detection
  |-- Adaptive calibration (measuring average half-period)
  |-- Tolerance window for pulse length spread
  |
  v
[Layer 2b] Histogram Analysis
  |-- Mode (peak) detection in pulse length distribution
  |-- Automatic threshold calculation between SHORT/LONG
  |-- Signal quality diagnostics
  |
  v
[Layer 3] Format Classification
  |-- Two-stage detection:
  |     1. Speed class from leader tone average
  |     2. Signature in header (ftype, $BB prefix, NIPSOFT)
  |-- Recognized formats: NORMAL, TURBO, FASTIPL, BSD,
  |   CP/M CMT, CP/M Tape, MZ-80B, FSK, SLOW, DIRECT, ZX Spectrum
  |
  v
[Layer 4] Specialized Decoders
  |-- wav_decode_fm    : NORMAL, SINCLAIR, CP/M CMT, MZ-80B
  |-- wav_decode_turbo : TURBO (FM configurable timing)
  |-- wav_decode_fastipl : FASTIPL ($BB prefix)
  |-- wav_decode_bsd   : BSD/BRD (chunking protocol)
  |-- wav_decode_fsk   : FSK (Frequency Shift Keying)
  |-- wav_decode_slow  : SLOW (quaternary encoding)
  |-- wav_decode_direct: DIRECT (direct bit writing)
  |-- wav_decode_cpmtape: CP/M Tape (Manchester)
  |-- wav_decode_zx    : ZX Spectrum (pilot ~612 us)
  |
  v
Output: MZF files or TMZ archive
```

### 5.3 Analytical Methods

The analyzer combines several independent analytical methods:

1. **Adaptive leader tone calibration** - measuring 100+ consecutive pulses, computing average, checking consistency (inspired by the Intercopy 10.2 algorithm)

2. **Histogram analysis** - statistical distribution of pulse lengths reveals bimodal (FM: SHORT/LONG) or multimodal (SLOW: 4 symbols) distributions

3. **Zero-crossing detector** - standard method for pulse extraction from analog signal, accurate for high-quality recordings

4. **Schmitt trigger** - optional alternative with hysteresis for noisy recordings with amplitude fluctuation

5. **Two-stage classification** - first determines the speed class from the leader tone, then identifies the specific format from header signatures

6. **CRC verification** - checksum verification of both header and body to confirm decoded data integrity

7. **Copy2 search** - searching for backup data in case of errors in the primary block (double recording on tape)

### 5.4 Output Formats

The analyzer produces:
- Individual **MZF files** (default mode) - one file per decoded program
- **TMZ archive** (option --output-format tmz) - a single file with all blocks, including source metadata

For unidentified signal segments, TZX Direct Recording blocks (0x15) can optionally be generated (--keep-unknown).

## 6. Sources and References

The project draws from the following sources:

### 6.1 Hardware Documentation
- **Sharp MZ-800 ROM** - routines RHEAD ($04D8), RDATA ($04F8) for tape reading, timing constants
- **Sharp MZ-700 documentation** - pulse sets, ROM monitor
- **Sharp MZ-80B documentation** - different timing (1800 Bd base), custom pulse set

### 6.2 Software
- **Intercopy 10.2** (MZ-800, Marek Šmihla - NIPSOFT) - reference auto-detection implementation, Z80 disassembly
- **TurboCopy V1.21** (MZ-800, Michal Kreidl) - TURBO loader implementation
- **cmt.com / ztape.com** (CP/M) - CP/M cassette utilities, Manchester encoding
- **mzftools** (mz-fuzzy, GPLv3) - reference C implementation of MZF conversions, FSK/SLOW/DIRECT Z80 loaders

### 6.3 Format Specifications
- **TZX v1.20** - base format, block structure
- **TAP** (ZX Spectrum) - simple tape format
- **CSW v2** - compression format for signal recording

### 6.4 Analytical References
- **audio2tape** (fuse-utils) - Schmitt trigger reference
- **wavdec.c** - partial C port of the Intercopy algorithm
- **cmttool** - NORMAL/CPM format decoder

## 7. Libraries

The project contains a set of C libraries (C99/C11) with modular architecture:

### 7.1 Core I/O and Utilities
- **generic_driver** - abstract I/O handler for files and memory buffers
- **endianity** - little-endian (Z80) to host byte order conversion
- **sharpmz_ascii** - Sharp MZ character set to standard ASCII conversion
- **sharpmz_utf8** - Sharp MZ character set (EU/JP variants) to UTF-8/Unicode conversion

### 7.2 Format Libraries
- **wav** - reading/writing RIFF WAVE (PCM 8/16/24/32/64-bit), normalization to float
- **mzf** - MZF format (128B header + body), validation, endianness
- **mzf_tools** - helper functions (filename conversion, factory, dump)
- **cmtspeed** - tape speed ratios (1:1, 2:1, 7:3, 3:1, ...)

### 7.3 CMT Audio Layer
- **cmt_bitstream** - 1 bit = 1 sample, O(1) random access
- **cmt_vstream** - RLE encoded pulses (1/2/4 bytes), space efficient
- **cmt_stream** - polymorphic wrapper unifying bitstream/vstream

### 7.4 TZX/TMZ Format Layer
- **tzx** - parsing and generating audio streams from all TZX v1.20 blocks
- **tmz** - reading/writing TMZ/TZX files, block structure, parsing MZ blocks
- **tmz_blocks** - definition and parsing of individual block types (0x40-0x45)
- **tmz_player** - generating CMT audio streams from TMZ blocks

### 7.5 CMT Encoders
Standalone libraries for each tape format, including embedded Z80 loaders:
- **mztape** - NORMAL FM (standard and increased speeds)
- **zxtape** - ZX Spectrum TAP (1400 Bd base)
- **mzcmt_turbo** - TURBO (configurable FM timing, TurboCopy loader)
- **mzcmt_fastipl** - FASTIPL ($BB prefix, Intercopy V02/V07 loader)
- **mzcmt_fsk** - FSK (Frequency Shift Keying, 7 speeds, two-phase loader)
- **mzcmt_slow** - SLOW (quaternary encoding, 5 speeds, two-phase loader)
- **mzcmt_direct** - DIRECT (direct bit writing, two-phase loader)
- **mzcmt_bsd** - BSD/BRD (chunked NORMAL FM, no loader)
- **mzcmt_cpmtape** - CP/M Tape (Pezik/MarVan, Manchester, LSB first, 1200/2400/3200 Bd)

### 7.6 WAV Analyzer
- **wav_analyzer** - complete analytical pipeline (preprocessing, pulse extraction, leader detection, format classification, decoding)

Detailed documentation for individual libraries will be published in separate documents.

## 8. CLI Tools

The project includes 11 command-line tools for conversion, analysis, and editing:

| Tool         | Purpose                                           |
|--------------|---------------------------------------------------|
| **tmzinfo**  | Display TMZ/TZX file contents                     |
| **mzf2tmz**  | Convert MZF/MZT to TMZ                            |
| **tmz2mzf**  | Extract MZF files from TMZ/TZX                    |
| **tmz2wav**  | Generate WAV audio from TMZ/TZX (player)          |
| **wav2tmz**  | Analyze WAV recordings and decode to MZF/TMZ      |
| **tap2tmz**  | Convert ZX Spectrum TAP to TZX/TMZ                |
| **tmz2tap**  | Extract TAP blocks from TMZ/TZX                   |
| **tmzconv**  | Convert signature between TMZ and TZX             |
| **tmzedit**  | Tape file editor (list, dump, edit, merge)         |
| **dat2bsd**  | Import binary data into TMZ as BSD block 0x45      |
| **bsd2dat**  | Export BSD data from TMZ block 0x45                |

Detailed documentation for each tool with examples is in separate **tools-*.md** files.

## 9. Future Plans

### 9.1 GUI Application

A graphical application is in preparation, enabling:
- Drag & drop file transfers between virtual tapes
- Visual tape content editor
- Clear block display with metadata
- Interactive format conversion

### 9.2 Integration into mz800emu

The TMZ format and all associated libraries will be integrated into the **mz800emu** emulator, where they will function as:
- **VirtualCMT** - a virtual cassette player/recorder capable of playing TMZ/TZX files in real time and recording output from the emulated computer
- **Online WAV analyzer** - analysis and decoding of WAV recordings directly in the emulator

Existing functionality for loading plain MZF/MZT and WAV files will be preserved.

## 10. License

The project is licensed under the GNU General Public License v3 (GPLv3).

Some libraries (mzcmt_fsk, mzcmt_slow) contain embedded Z80 loader binaries derived from the mzftools project (mz-fuzzy, GPLv3). The mzcmt_turbo library contains a loader derived from reverse engineering of TurboCopy V1.21 (Michal Kreidl).

## 11. Authors

- Michal Hucik (hucik@ordoz.com) - author and lead developer

Copyright (C) 2017-2026 Michal Hucik
