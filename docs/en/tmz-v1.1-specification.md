# TMZ Format Specification v1.1

This specification extends TMZ v1.0 with block 0x46 (MZ File). Anything
this specification does not change remains as defined in v1.0
(`tmz-specification.md`).

The file version is 1.1 because all v1.0 blocks remain valid
(chapter 2).

## 1. Introduction and Principles

1. **TZX is the core.** All TZX v1.20 blocks apply unchanged. A signal that
   no profile describes (unknown format, archival recording) is stored
   in TZX blocks (0x10-0x15, 0x19, 0x20). MZ blocks carry **meaning**: the MZF
   file, the writer profile and the recording structure.
2. **A block is self-contained.** Data block 0x46 carries the effective values of everything
   the player needs: the recording structure (list of parts), timing and
   encoding. The profile ID is only a label used for validation. A reader that
   does not know the profile still plays the block.
3. **Time is an integer in units of the part's time base.** The time base
   is a clock rate in Hz (CPU T-states, CTC ticks, µs, samples). The us*100 unit
   of v1.0 is not used in this version.
4. **Extensibility is handled by TLV** with a "mandatory to understand" flag (chapter 5).
5. **Modulation families are a fixed set** (FM, FSK, SLOW, DIRECT, MANCHESTER,
   PILOT). A new family means a new version of this specification.
6. **The TMZ format is not used by anything outside the TapeMZ project** (library, tools,
   integration into mz800emu). Compatibility with v1.0 serves existing
   files only. Compatibility with TZX tools still applies.

Terms:
- **pulse** = two half-waves, the first and the second (chapter 5.4). SHORT (S) and LONG (L)
  are the two pulse kinds of the FM family.
- **part** = one physical record on tape (tone, tapemark, data, checksum,
  tail, copy). A program writes one file as one or more parts.
- **profile** = a description of how a specific program or ROM writes (profile
  registry, chapter 8).

## 2. File Header

Unchanged from v1.0 except for the version:

```
Offset  Size      Value      Description
0x00    7         "TapeMZ!"  Signature (ASCII)
0x07    1         0x1A       EOF marker
0x08    1         1          Major version
0x09    1         1          Minor version (1.1 = this specification)
```

- A v1.1 reader accepts versions 1.0 and 1.1. A major version other than 1 is a load
  error.
- A v1.1 writer writes version 1.1. A file without block 0x46 may have 1.0.
- The "ZXTape!" header (native TZX) remains supported as in v1.0.

## 3. MZ Blocks in v1.1

| ID | name | status in v1.1 |
|---|---|---|
| 0x40 | MZ Standard Data | deprecated: read and played (chapter 9), new files do not write it |
| 0x41 | MZ Turbo Data | deprecated: ditto; the writer uses it for families other than FM until their family is implemented (chapter 7) |
| 0x42 | MZ Extra Body | deprecated, unsupported (chapter 9.3) |
| 0x43 | MZ Machine Info | valid: target machine, CPU clock; the clock is the default time base (chapter 5.3) |
| 0x44 | MZ Loader | deprecated, unsupported (chapter 9.3) |
| 0x45 | MZ BASIC Data | deprecated: read and played (chapter 9) |
| **0x46** | **MZ File** | **new: one MZF file and the list of its parts (chapter 4)** |
| 0x47-0x4F | - | reserved; the reader skips them with a warning |

All MZ blocks have a DWORD LE length after the ID (v1.0 chapter 3.1), so a reader
can skip any block.

## 4. Block 0x46 - MZ File

One MZF file as the program saved it, and the list of parts that make up
its recording on tape. The player plays the parts in the given order and literally.
It does not need the profile ID for that.

### 4.1 Layout

```
Offset  Type     Description
0x00    BYTE     block ID (0x46)
0x01    DWORD    L = length of the rest of the block (from offset 0x05 to the end of the block)
0x05    BYTE     block version (1)
0x06    BYTE     n = profile ID length (0..32; 0 = no ID)
0x07    n B      profile ID, ASCII (chapter 8.1)
        128 B    MZF header (bytes as the program saved them; LE as in MZF)
        DWORD    N = body length
        N B      body
        WORD     T = length of the block TLV area
        T B      block TLV (chapter 5.5)
        BYTE     P = number of parts (1..255)
        P x      part (chapter 4.2)
```

Invariants (violation = block structure error, chapter 4.4):
- All fields lie inside the block and the sum of their lengths is exactly L.
- `n <= 32`, `P >= 1`.
- The body length N does not have to match `fsize` in the MZF header. N is decisive.
  Example: BASIC files (BSD/BRD) have `fsize = 0` and the body carries chunks
  (chapter 6.7).

The MZF header and body are **the file as the program saved it**. If the
bytes on tape differ from the input file, the bytes from tape are stored. Bytes on tape that are not
part of the MZF (preloader header, preloader) are carried by parts as inline
data.

### 4.2 Part

```
Type     Description
DWORD    part length (bytes after this field up to the end of the part)
BYTE     role (table 4.3.1)
BYTE     family (table 4.3.2)
BYTE     data source (table 4.3.3)
[...]    data source fields according to table 4.3.3
TLV[]    part parameters up to the end of the part (chapter 5)
```

The part bytes (data from which the family generates the signal) are defined by the data source.
The checksum and other bytes added by the family are not among them (chapter 6.5).

### 4.3 Enumerations

#### 4.3.1 Role

The role describes the meaning of the part for humans and tools (tmzinfo, validation,
export). The player does not use it. An unknown value is read as 0.

| value | role | meaning |
|---|---|---|
| 0 | OTHER | other or unknown part |
| 1 | HEADER | program MZF header |
| 2 | BODY | program body (or a section of it) |
| 3 | LOADER_HEADER | preloader header (e.g. TurboCopy Loader Y) |
| 4 | LOADER | preloader or loader |
| 5 | BSD_CHUNK | chunk of a BASIC file (BSD/BRD) |

#### 4.3.2 Family

| value | family | chapter | status |
|---|---|---|---|
| 0 | - | - | invalid |
| 1 | FM | 6 | defined |
| 2 | FSK | 7.1 | reserved, parameters will be defined by a future version of the specification |
| 3 | SLOW | 7.1 | reserved |
| 4 | DIRECT | 7.1 | reserved |
| 5 | MANCHESTER | 7.2 | defined |
| 6 | PILOT | 7.3 | reserved |

The player does not play a part with an unknown or reserved family (chapter 4.4).

#### 4.3.3 Data Source

| value | source | fields | part bytes |
|---|---|---|---|
| 0 | MZF_HEADER | - | 128 B of the block's MZF header |
| 1 | MZF_BODY | - | the whole block body |
| 2 | BODY_SLICE | DWORD offset, DWORD length | body slice `[offset, offset+length)` |
| 3 | INLINE | DWORD length, bytes | bytes stored in the part |

The slice must lie inside the body. An unknown source is a part error (the field length
cannot be determined, the part is skipped according to its length).

### 4.4 Errors

| situation | reaction |
|---|---|
| field lengths exceed the block, sum does not match L, `n > 32`, disallowed character in the profile ID, `P = 0`, part length exceeds the block | structure error: file load error |
| block version other than 1 | the block is not played, warning, processing continues with the next block |
| unknown family or source, slice outside the body, unknown mandatory tag, invalid value of a mandatory tag | part error: the player does not play the part and reports an error; it does not guess anything |
| unknown optional tag | skipped |

## 5. TLV

### 5.1 Encoding

```
WORD   tag     bit 15 = mandatory to understand, bits 0-14 = tag number
WORD   length  value length in bytes
B[]    value   numbers LE
```

- The tag is identified by bits 0-14. The writer sets bit 15 exactly according to
  the "mandatory" column in the tables of this specification. For a **known** tag
  the reader decides mandatoriness by this column, not by the bit in the file (an incorrectly
  written bit thus does not turn an error into a silent skip). Bit 15 decides
  only for unknown tags.
- **An unknown tag with bit 15 = 1** is a part error (chapter 4.4). **An unknown tag
  with bit 15 = 0** is skipped by the reader.
- A known tag with an unexpected length is an invalid value (for a mandatory tag a part
  error, an optional tag is skipped).
- Each tag may occur at most once in one TLV area unless stated
  otherwise for it. A repetition is an invalid value: for a mandatory tag
  a part error, for an optional tag the first occurrence applies and further ones are skipped.
- A missing tag means the default value from the table.

### 5.2 Tag Number Ranges

| range | use |
|---|---|
| 0001h-003Fh | common part tags (all families) |
| 0040h-007Fh | block tags |
| 0100h-01FFh | FM family |
| 0200h-02FFh | FSK |
| 0300h-03FFh | SLOW |
| 0400h-04FFh | DIRECT |
| 0500h-05FFh | MANCHESTER |
| 0600h-06FFh | PILOT |
| other | reserved |

### 5.3 Common Part Tags

| tag | name | mandatory | value | default |
|---|---|---|---|---|
| 0001h | TIMEBASE | yes | DWORD clock rate in Hz, > 0 | CPU clock from the last preceding block 0x43, otherwise 3 546 895 |
| 0002h | LEVEL | yes | BYTE 1 = first half-wave HIGH, 0 = first half-wave LOW | 1 |
| 0003h | PAUSE | yes | DWORD silence after the part in time base units; optionally BYTE level during the pause (0 LOW, 1 HIGH) | 0, LOW |
| 0004h | STOP | no | BYTE 1 = stop the tape after the part (and pause) | 0 |
| 0005h | READER | no | BYTE routine, WORD N (chapter 5.6) | no check |

**Time base.** All times in a part (half-waves, pause) are integers
in units of `1 / TIMEBASE` s. Examples: ROM in CPU T-states (3 546 895 Hz),
TurboCopy in CTC0 ticks (1 108 405 Hz), UniCMT in µs (1 000 000 Hz),
mzftools in samples (44 100 Hz).

### 5.4 Playback (normative)

- Pulse = first and second half-wave. With LEVEL = 1 the first is at level HIGH
  and the second LOW. With LEVEL = 0 the other way round.
- The pause starts after the second half-wave of the last pulse of the part. During the pause
  the level is according to PAUSE, default LOW. With level HIGH (Intercopy holds the output
  HIGH) the second half-wave of the last pulse is followed by a rising edge
  and the pause merges with the first HIGH half-wave of the next part. With LEVEL = 1 and pause
  LOW the second half-wave of the last pulse merges with the pause (silence = LOW). This
  matches the recordings of both ROM and TurboCopy.
- STOP: the emulator stops the tape after the pause. It is ignored on export
  to WAV.
- The level between blocks is held by the player. A TZX block after a pause (a TZX
  block with a pause, 0x20, silence of an MZ block at level LOW) and at the start
  of playback begins with an edge: the first pulse is HIGH, as on the ZX. TZX 1.20
  would write it LOW and it would merge with the pause (the Intercopy SINCLAIR
  recording shows the whole pilot after the gap).
- Conversion to the output sample rate: the player accumulates the exact time from
  the start of the part and places each edge on a sample according to the accumulated time.
  The rounding error thus does not add up (unlike rounding each
  half-wave separately). A section of x units then has
  floor or ceil(x·rate/timebase) samples. The player checks parts with READER
  with worst-case rounding before conversion (chapter 5.6).

### 5.5 Block Tags

| tag | name | mandatory | value |
|---|---|---|---|
| 0040h | PROFILE_ARGS | no | ASCII profile parameters, chapter 8.2 |

### 5.6 READER (read point check)

Describes who will read the part, so that tools can check the read point
margins (tape lint, `tmzinfo --lint`) and the player during conversion to
samples. Lint uses nominal lengths. The player converts the lengths to whole
samples with worst-case rounding (first half-wave SHORT ceil, first
half-wave LONG and SHORT period floor). Margin below 0 = the block is not played
(error), margin below 10 µs = warning.

| routine | meaning | model |
|---|---|---|
| 0 | ROM read routine with constant N = 82 (`LD A,82`, 0A4Ah) | sample at 86+14N+(0..52) T after the edge, conditions SHORT_H < 86+14N, LONG_H > 138+14N, SHORT period > 256+14N (mzcmt_turbo) |
| 1 | the same ROM routine with a changed constant N (the TurboCopy and Intercopy preloader overwrites 0A4Bh) | same with N from the value |
| 2 | routine 1 with the FAST IPL V07 loader (Intercopy), which adds 86 T after each rising edge (hook at 061Fh) | as 1, all limits shifted by 86 T |
| other | reserved | lint skips the part |

## 6. FM Family

FM encodes a bit by pulse length: SHORT = 0, LONG = 1. It covers ROM, TurboCopy,
Intercopy NORMAL/TURBO/FASTIPL, CP/M-CMT and BASIC (BSD/BRD).

### 6.1 Tags

| tag | name | mandatory | value | default |
|---|---|---|---|---|
| 0101h | PULSES | yes | 4x DWORD: SHORT first, SHORT second, LONG first, LONG second half-wave; each > 0 | **must be present** |
| 0102h | TONE | yes | DWORD number of SHORT in the tone (GAP) | 0 |
| 0103h | TAPEMARK | yes | WORD number of L, WORD number of S, BYTE number of L after the tapemark | 0, 0, 0 |
| 0104h | FRAMING | yes | BYTE number of start bits (LONG) before a byte, BYTE bit order (0 = MSB first, 1 = LSB first) | 1, 0 |
| 0105h | CHECKSUM | yes | BYTE algorithm (chapter 6.5), optionally followed by the checksum bytes as written to tape | 0 (no checksum) |
| 0106h | END | yes | BYTE number of L after the checksum, WORD number of S at the end | 0, 0 |
| 0107h | COPIES | yes | BYTE number of copies, WORD number of S in the separator | 0, 0 |
| 0108h | CONTEXT | yes | n x 11 B entries (chapter 6.3); may occur more than once, entries are added together | none |

The tone is always stored as a **number of pulses**. Rules such as "tone about 3.15 s"
(TurboCopy) belong to the profile registry and are applied when the file is created.

### 6.2 Pulse Sequence (normative)

A part of the FM family is played as a sequence of pulses. Each pulse has a kind
(S/L) and a context (chapter 6.3):

1. `TONE` x count from TONE. The last of them has context `TONE_LAST`.
2. `TM_L` x number of L from TAPEMARK (last `TM_L_LAST`), `TM_S` x number of S
   (last `TM_S_LAST`), `TM_END_L` x number of L after the tapemark.
3. **Data section:**
   1. for each byte of the part: `START` (L) x number of start bits, then 8 bits
      in the order given by FRAMING. Bits at positions 0-6 have context `BIT`,
      the bit at position 7 has context `BIT_LAST`. In the last byte of the part
      position 7 has context `BIT_LAST_DATA_END`.
   2. checksum bytes (chapter 6.5) the same way, with contexts `CK_START`, `CK_BIT`
      and `CK_BIT_LAST`. In the last checksum byte position 7 has context
      `CK_BIT_END`.
   3. `END_L` (L) x the first number from END, then `TAIL` (S) x the second number
      (last `TAIL_LAST`).
4. For each copy from COPIES: `COPY_SEP` (S) x separator count (last
   `COPY_SEP_LAST`) and the whole data section again (item 3). Neither the tone nor the tapemark
   is repeated.

Position means transmission order: position 0 is the first transmitted bit (with MSB
first it is bit 7 of the value).

### 6.3 Contexts and Half-Wave Lengths

CONTEXT entry (11 B):

```
BYTE   context (table below)
BYTE   pulse kind: 0 = S, 1 = L
BYTE   kind of the following pulse: 0 = S, 1 = L, FFh = any
DWORD  first half-wave
DWORD  second half-wave
```

| value | context | parent |
|---|---|---|
| 0 | TONE | - |
| 1 | TONE_LAST | TONE |
| 2 | TM_L | - |
| 3 | TM_L_LAST | TM_L |
| 4 | TM_S | - |
| 5 | TM_S_LAST | TM_S |
| 6 | TM_END_L | - |
| 7 | START | - |
| 8 | BIT | - |
| 9 | BIT_LAST | - |
| 10 | BIT_LAST_DATA_END | BIT_LAST |
| 11 | CK_START | START |
| 12 | CK_BIT | BIT |
| 13 | CK_BIT_LAST | BIT_LAST |
| 14 | CK_BIT_END | CK_BIT_LAST |
| 15 | END_L | - |
| 16 | TAIL | - |
| 17 | TAIL_LAST | TAIL |
| 18 | COPY_SEP | - |
| 19 | COPY_SEP_LAST | COPY_SEP |

**Resolving pulse lengths** with context K, kind D and kind of the following
pulse X (X does not exist for the last pulse of the part):
1. entry (K, D, X), if X exists,
2. entry (K, D, any),
3. the same for the parent of K, then for its parent,
4. the base from PULSES according to D.

An unknown context or kind value in CONTEXT is an invalid value (part
error). Two entries with the same triple (K, D, X) are an invalid value.

Why contexts: the ROM times by a processor loop and the length of the second half-wave depends
on which path the code takes. For example a data bit S has a second half-wave of 920 T,
but 910 T when followed by L, and the bit at position 7 has 1028 T. Profiles
timed by a counter (TurboCopy) do not need contexts.

### 6.4 Example: ROM-800-EXACT

Base PULSES = 844, 920, 1664, 1750 (T-states, default time base) and
22 CONTEXT entries:

| context | kind | next | half-waves |
|---|---|---|---|
| TONE | S | * | 844 / 918 |
| TONE_LAST | S | * | 844 / 913 |
| TM_L | L | * | 1664 / 1728 |
| TM_L_LAST | L | * | 1664 / 1723 |
| TM_S | S | * | 844 / 908 |
| TM_S_LAST | S | * | 844 / 903 |
| TM_END_L | L | * | 1664 / 1861 |
| START | L | S | 1664 / 1726 |
| START | L | L | 1664 / 1716 |
| BIT | S | L | 844 / 910 |
| BIT | L | L | 1664 / 1740 |
| BIT_LAST | S | * | 844 / 1028 |
| BIT_LAST | L | * | 1664 / 1858 |
| BIT_LAST_DATA_END | S | * | 844 / 1041 |
| BIT_LAST_DATA_END | L | * | 1664 / 1871 |
| CK_BIT_LAST | S | * | 844 / 965 |
| CK_BIT_LAST | L | * | 1664 / 1795 |
| CK_BIT_END | S | * | 844 / 926 |
| CK_BIT_END | L | * | 1664 / 1756 |
| END_L | L | * | 1664 / 1733 |
| COPY_SEP | S | * | 844 / 906 |
| COPY_SEP_LAST | S | * | 844 / 1000 |

Structure: header TONE 22000, TAPEMARK 40/40/1; body TONE 11000,
TAPEMARK 20/20/1; both CHECKSUM 1, END 1/0, COPIES 1/256.

Verification: the reference model plays this description and the result
matches the reference ROM SAVE recording from the emulator **in every
half-wave to the T-state** (108 889 half-waves; everything is compared except the second half-wave
of the last pulse, which merges with the pause). This applies to emulation without the
8255 WAIT model; not yet verified on real HW.

### 6.5 Checksum

| value | algorithm | bytes on tape |
|---|---|---|
| 0 | NONE | none |
| 1 | MZ_POPCOUNT16_BE | number of one bits in the part bytes, 16 bit, high byte first (ROM `@CHECK` 071Ah) |
| other | reserved for the families of chapter 7 | - |

If CHECKSUM carries bytes after the algorithm, the player writes these bytes
instead of the computed ones. Their count must match the algorithm (exactly 2 for 1),
otherwise it is an invalid value. This serves to store faithfully a recording with
a bad checksum (wav2tmz).

### 6.6 Example: TurboCopy V1.21 (TC-1.21)

Time base 1 108 405 Hz (CTC0 ticks), S = 71 + floor(480·b/a) (model derived from TurboCopy measurements in the emulator; a hypothesis for unmeasured speeds):

```
PULSES   = (S+1)/2, S/2, S, S           (integer division; 2/1: 156, 155, 311, 311; 7/3: 138, 138, 276, 276)
TONE     = count by S, table below (e.g. 1/1: 6344, 2/1: 11239, 7/3: 12664, 8/3: 13925, 3/1: 15130)
TAPEMARK = 40/40/1 (header, preloader header), 20/20/1 (other parts)
CHECKSUM = 1, END = 1/99, COPIES = 0, CONTEXT none, STOP = 1
```

Loader Y: three parts (LOADER_HEADER inline 128 B and LOADER inline 90 B
at 1/1, BODY at a/b). Loader N: HEADER and BODY at a/b.

The speed is a fraction a/b, a and b are digits 1-9, 1/2 <= a/b <= 3 (34 distinct ratios).
The preloader read point (byte 4Bh) is floor(82·b/a). Tone pulse count by S
(measured in the emulator; the product count × S is 3 495 030 to 3 497 152):

| a/b | S | TONE |
|---|---|---|
| 1/2 | 1031 | 3392 |
| 5/9 | 935 | 3740 |
| 4/7 | 911 | 3838 |
| 3/5 | 871 | 4014 |
| 5/8 | 839 | 4167 |
| 2/3 | 791 | 4420 |
| 5/7 | 743 | 4705 |
| 3/4 | 711 | 4917 |
| 7/9 | 688 | 5081 |
| 4/5 | 671 | 5210 |
| 5/6 | 647 | 5403 |
| 6/7 | 631 | 5540 |
| 7/8 | 619 | 5648 |
| 8/9 | 611 | 5722 |
| 1/1 | 551 | 6344 |
| 9/8 | 497 | 7033 |
| 8/7 | 491 | 7119 |
| 7/6 | 482 | 7252 |
| 6/5 | 471 | 7422 |
| 5/4 | 455 | 7683 |
| 9/7 | 444 | 7873 |
| 4/3 | 431 | 8110 |
| 7/5 | 413 | 8464 |
| 3/2 | 391 | 8940 |
| 8/5 | 371 | 9421 |
| 5/3 | 359 | 9736 |
| 7/4 | 345 | 10131 |
| 9/5 | 337 | 10372 |
| 2/1 | 311 | 11239 |
| 9/4 | 284 | 12307 |
| 7/3 | 276 | 12664 |
| 5/2 | 263 | 13290 |
| 8/3 | 251 | 13925 |
| 3/1 | 231 | 15130 |

Verification against reference recordings from the emulator: structure
exact, mean half-wave deviation 0.000 T, largest deviation 24.8 T. The first
HIGH of each part is 152 to 163 T shorter in the recording (counter start-up). This
TurboCopy jitter is not repeatable even between two emulator runs, so the profile
carries the nominal value.

### 6.7 BASIC (BSD/BRD), profile BASIC-1Z016

The block body carries the chunks one after another (each 258 B: ID WORD LE and 256 B of data,
the last ID FFFFh carries the rest of the data). The MZF header is stored as the
program wrote it (`fsize = 0`). Parts: HEADER (MZF_HEADER) and for each chunk
BSD_CHUNK (BODY_SLICE `i·258`, 258), at most 254 chunks.

Profile `BASIC-1Z016` (BASIC 1Z-016 V1.0A, `WOPEN#`/`PRINT#`/`CLOSE#`,
measured in the emulator):

- HEADER: tone 22000, tapemark 40/40, 1 L, data, checksum, 1 L, separator 256 S, copy, 1 L;
- BSD_CHUNK: tone 11000, tapemark 20/20, otherwise the same;
- time base 3 546 895 Hz, contexts according to the table, no READER and STOP
  (the BASIC read routine has not been analysed);
- pause LOW after the header and after each chunk except the last 6 610 752 T
  (with END_L 1806 T the silence is 6 612 558 T: end of signal -> motor stop
  1905 T, motor start -> next section 6 610 653 T); after the last chunk
  PROFILE_ARGS `pause` (ms, default 1000);
- PROFILE_ARGS: `pause`, `machine`;
- the profile does not check chunk IDs, the bytes go into the signal unchanged.

| context | bit | next | half-waves (T, HIGH/LOW) |
|---|---|---|---|
| base SHORT / LONG | 0 / 1 | - | 868/944, 1739/1825 |
| TONE | 0 | * | 868/942 |
| TONE_LAST | 0 | * | 868/937 |
| TM_L | 1 | * | 1739/1803 |
| TM_L_LAST | 1 | * | 1739/1798 |
| TM_S | 0 | * | 868/932 |
| TM_S_LAST | 0 | * | 868/927 |
| TM_END_L | 1 | * | 1739/1946 |
| START | 1 | 0 / 1 | 1739/1801, 1739/1791 |
| BIT | 0 | 1 | 868/934 |
| BIT | 1 | 1 | 1739/1815 |
| BIT_LAST | 0 / 1 | * | 868/1053, 1739/1934 |
| BIT_LAST_DATA_END | 0 / 1 | * | 868/1061, 1739/1942 |
| CK_BIT_LAST | 0 / 1 | * | 868/989, 1739/1870 |
| CK_BIT_END | 0 / 1 | * | 868/950, 1739/1831 |
| END_L | 1 | * | 1739/1806 |
| COPY_SEP | 0 | * | 868/929 |
| COPY_SEP_LAST | 0 | * | 868/1020 |

Verification: a block built from the data of the reference recording matches that
emulator recording exactly to the T-state (chapter 12).

## 7. Other Families (reserved)

The families are reserved in v1.1. Their tags will be defined by a future version
of the specification together with their implementation. Until then the writer stores these formats
as 0x41 (v1.0). Below is the range of parameters that the future version must cover
(source: code `src/libs/mzcmt_*`).

### 7.1 FSK, SLOW, DIRECT (mzftools)

- Common: leader (number of cycles, lengths), polarity pulse, sync, fade-out
  (initial and final width, step), XOR checksum.
- FSK: table of symbols for bit 0/1 by speed level (7 levels).
- SLOW: 4 symbols of 2 bits each (5 levels).
- DIRECT: bit length, sync bits after every 2 data bits.
- Time base 44 100 Hz: these formats are created by a PC program, its
  original is a 44.1 kHz signal. Exact values will be taken from the code of
  `mzcmt_fsk/slow/direct` and from the mzftools sources.

### 7.2 MANCHESTER (CPM-TAPE)

Sources of values: Intercopy V10.2 "CPM" 1500 to 3600 Bd measured in the emulator, Pezik/MarVan
TAPE.COM V2.0a calculated from T-states.

**Bit.** Each bit has two halves. With LEVEL = 1 bit 1 has the first half HIGH,
the second LOW, bit 0 the other way round (Manchester, transition in the middle of the bit; PC1 level
of the write routines). LEVEL = 0 swaps the levels. Bytes are transmitted LSB first,
without start and stop bits.

**Tags:**

| tag | name | mandatory | value | default |
|---|---|---|---|---|
| 0501h | HALVES | yes | 4x DWORD: first half of bit 0, first half of bit 1, second half of bit 0, second half of bit 1 (context BIT); each > 0 | **must be present** |
| 0502h | PILOT | yes | DWORD number of patterns, BYTE pattern length in bits (1-8), BYTE pattern (bit 0 = first transmitted bit) | 0 patterns |
| 0503h | SYNC | yes | BYTE number of bits (0-8), BYTE bits (bit 0 first) | 0 bits |
| 0504h | FLAG | yes | BYTE flag value; presence of the tag = the flag is transmitted | no flag |
| 0505h | CHECKSUM | yes | BYTE algorithm (0 = none, 1 = SUM16_LE), optionally followed by 2 B of checksum as written | 0 |
| 0506h | END | yes | BYTE number of end bits (0-8), BYTE bits (bit 0 first) | 0 bits |
| 0507h | CARRY | yes | signed BYTE: adjustment of the second half in contexts DATA_FIRST and DATA_BYTE on a sum carry (below) | 0 |
| 0508h | CONTEXT | yes | n x 6 B entries: BYTE context, BYTE bit (0, 1, FFh = both), DWORD second half; may occur more than once, entries are added together | none |

The common tags (chapter 5.3) apply; READER is not checked for the MANCHESTER family
(lint skips the part). A repeated pair (context, bit) in CONTEXT is an
invalid value.

**Sequence (normative):**
1. pilot: PILOT pattern x count;
2. sync: SYNC bits;
3. flag: 8 bits of the FLAG value (if the tag is present);
4. part data (data source, chapter 4.3.3), each byte 8 bits LSB first;
5. checksum: SUM16_LE = sum of the data bytes (without the flag), 16 bits,
   modulo 65536, written low byte, then high byte; with explicit bytes
   those are written;
6. END end bits;
7. pause according to PAUSE (chapter 5.4).

**The first half** of a bit is HALVES according to the bit value. **The second half** of a bit
depends on what follows (context) and on the bit value. The lookup is
CONTEXT entry (context, bit), then (context, FFh), otherwise HALVES (context
BIT) according to the bit value.

| value | context | second half of the bit ... |
|---|---|---|
| 0 | BIT | of bits 0-6 of each byte (and default) |
| 1 | PILOT_IN | inside a pilot pattern |
| 2 | PILOT_NEXT | of the last bit of a pattern followed by another pattern |
| 3 | PILOT_END | of the last pilot bit |
| 4 | SYNC_IN | of a sync bit followed by another sync bit |
| 5 | SYNC_END | of the last sync bit |
| 6 | FLAG_END | of bit 7 of the flag |
| 7 | DATA_FIRST | of bit 7 of byte data[0], followed by data[1] |
| 8 | DATA_BYTE | of bit 7 of byte data[k], k >= 1, followed by data[k+1] |
| 9 | DATA_END | of bit 7 of the last data byte (length >= 2) |
| 10 | DATA_SINGLE_END | of bit 7 of the only data byte (length 1) |
| 11 | CK_MID | of bit 7 of the low checksum byte |
| 12 | CK_END | of bit 7 of the high checksum byte |
| 13 | END_IN | of an end bit followed by another end bit |
| 14 | END_LAST | of the last end bit |

If a section is missing (no sync, flag, data or checksum), the last
bit of the preceding section gets its own end context (e.g. without a checksum
bit 7 of the last data byte has DATA_END). Bit 7 of flag, data
and checksum bytes always has the context according to the table, the other bits of the bytes BIT.

**Carry.** For DATA_FIRST and DATA_BYTE, CARRY is added to the second half when
(low byte of the sum data[0..k]) + data[k+1] > FFh (Pezik: the overflow of
`ADD A,C` shortens the loop by 1 T). The resulting second half must be > 0.

**Output.** Adjacent halves of the same level form one half-wave (from edge
to edge). The pause is the last half-wave with the level from PAUSE; the same level
continues (chapter 5.4).

**Profiles:** `PEZIK-2.0A` and `IC-10.2-CPM` (chapter 8).
Example: TLV of the header part of `IC-10.2-CPM` (role 1, family 5, source 0;
pause 1 221 688 T, halves 741 T, pilot 2048 x "011", sync 1, flag 00h,
SUM16_LE, end bit 1, contexts PILOT_END/FLAG_END 740, DATA_END/
DATA_SINGLE_END 739, CK_MID 744, CK_END 745):

```
TIMEBASE  01 80 04 00 0F 1F 36 00
PAUSE     03 80 04 00 38 A4 12 00
HALVES    01 85 10 00 E5 02 00 00 E5 02 00 00 E5 02 00 00 E5 02 00 00
PILOT     02 85 06 00 00 08 00 00 03 06
SYNC      03 85 02 00 01 01
FLAG      04 85 01 00 00
CHECKSUM  05 85 01 00 01
END       06 85 02 00 01 01
CONTEXT   08 85 24 00 03 FF E4 02 00 00 06 FF E4 02 00 00 09 FF E3 02 00 00
                      0A FF E3 02 00 00 0B FF E8 02 00 00 0C FF E9 02 00 00
```

### 7.3 PILOT (Intercopy Sinclair)

Pilot (count, length), sync 1 and 2, pulses of bit 0 and 1, flag byte, XOR
checksum including the flag, end pulse (IC-SINCLAIR). It stays in 0x46
because it is an MZ file with meaning; pure ZX recordings belong to TZX 0x10/0x11.

## 8. Profiles

### 8.1 Profile ID

- ASCII, length 0 to 32 characters, allowed characters `A-Z`, `0-9`, `.` and `-`.
  Any other character is a block structure error.
- Built-in profiles are defined by the registry (in the library the
  `tmz_profile` module): for example `ROM-800-NOMINAL`, `ROM-800-EXACT`,
  `TC-1.21`, `IC-10.2-NORMAL`, `IC-10.2-TURBO`, `IC-10.2-FASTIPL`, `UNICMT`,
  `TAPEMZ-V1-LEGACY`, `PEZIK-2.0A`, `IC-10.2-CPM`, `BASIC-1Z016`.
- A custom profile has the prefix `X-` (e.g. `X-DEMO`). Built-in IDs never have the prefix `X-`.
- The ID is a label. The player does not read it. A correction of a profile in the registry does not change
  existing files, because blocks carry effective values.

### 8.2 PROFILE_ARGS

The parameters with which the registry created the block, as an ASCII string
of `key=value` pairs separated by commas (key `[a-z0-9_]+`, value printable
ASCII without a comma). Example: `speed=2/1,loader=Y`. Used for display and
validation, the player does not read it. `speed` is a ratio a:b (the registry writes a:b and also reads a/b):
an en_CMTSPEED speed, for TC-1.21 also any other TurboCopy ratio (digits 1-9, 1/2 to 3/1).

### 8.3 Registry and Validation

A profile in the registry has: ID, family, description, source and confidence level (verified
in emulation / calculation / hypothesis / unverified), allowed parameters and a function
that creates the list of parts from an MZF and parameters. `tmzinfo --lint` can:
- compare a block with the registry output for its ID and PROFILE_ARGS and list
  the differences (warning, not error),
- check read point margins for parts with READER (margin at least 10 µs;
  `mzcmt_turbo_check_readpoint`).

## 9. Compatibility with v1.0

### 9.1 Reading

A v1.1 reader converts v1.0 blocks in memory to 0x46 and then processes them the same way
as 0x46 (a single player path). Exceptions are formats whose family
is not implemented (chapter 7) and cases the FM model cannot express
(table 9.2). These are played by the original v1.0 path. Blocks 0x41 CPM_TAPE
and 0x45 are always converted, they have no v1.0 path; whatever cannot be
converted from them is not played (block error).

### 9.2 Mapping

| v1.0 block | condition | result |
|---|---|---|
| 0x40 | MZ-800 pulse set | profile `ROM-800-NOMINAL` with the registry default parameters, tones according to Intercopy or TurboCopy (switchable). |
| 0x40 | MZ-700 or MZ-80B pulse set | `TAPEMZ-V1-LEGACY` with the pulse set from the block, until ROM-700 and MZ-80B are verified. |
| 0x41 | format TURBO | `TC-1.21` with the speed from the block and the preloader; tones, tail, pause and pulses according to TurboCopy measurements in the emulator. Custom pulses, body copies, the MZ-80B set and speeds TurboCopy does not support are not converted. |
| 0x41 | format NORMAL, speed 1:1 | `TAPEMZ-V1-LEGACY` with the parameters used by the v1.0 player |
| 0x41 | format FASTIPL, speed 1:1, 2:1, 7:3, 8:3 | `IC-10.2-FASTIPL` with `bd` 1200, 2400, 2800, 3200; Intercopy V10.2 timing measured in the emulator. Other speeds, custom pulses, a body tone other than 5500 and the MZ-80B set are not converted (v1.0 path). |
| 0x45 | at most 254 chunks | `BASIC-1Z016` always (even without v1.0 conversion in the player); machine into PROFILE_ARGS, the pulse set does not apply. |
| 0x41 | format NORMAL when the player has the MZ800 recording format (copies), or a speed other than 1:1 | not converted, played by the v1.0 path (the FM model cannot express the first byte of a copy without a start bit; 2:1 CPM has cmt.com pulses; other speeds: loading via ROM cannot be verified and the conversion changes the recording length systematically) |
| 0x45 | more than 254 chunks | not converted and not played (block error; block 0x46 has at most 255 parts) |
| 0x41 | format CPM_TAPE | `PEZIK-2.0A`: speed level 2 -> `bd=3200`, otherwise `bd=2400` (level 0 "1200" does not exist, invalid values are played as 2400); custom pulses, lgap and flags of the block are ignored; a body over 16 384 B is not converted (block error, profile PEZIK-2.0A rejects it). |
| 0x41 | format FSK, SLOW, DIRECT | not converted, played by the v1.0 path |
| 0x41 | format SINCLAIR | not converted and not played (a ZX Spectrum recording belongs in TZX blocks 0x10/0x11) |
| 0x42, 0x44 | - | unsupported (chapter 9.3) |

The "Pause after block" field (ms) is converted to PAUSE of the last part (recalculated
to the time base of the part). "Target
machine" and "Pulse set" are stored in PROFILE_ARGS.

The `TAPEMZ-V1-LEGACY` conversion is verified by a test against the v1.0 generator:
the same number and the same levels of half-waves, the length of each half-wave in samples
may differ by at most 1. The v1.0 generators round
each half-wave separately, the FM model accumulates the part time (chapter 5.4), so byte-level
WAV identity is not a goal. Readability is verified by loading in the emulator.

### 9.3 Blocks 0x42 and 0x44

Nothing in the project writes them and the v1.0 player did not play them. In v1.1
they are not converted: the reader skips them with a warning and `tmzconv --upgrade`
leaves them unchanged with a warning.

### 9.4 File Conversion

`tmzconv --upgrade` converts a v1.0 file to v1.1 according to chapter 9.2 and raises
the version in the header. Writing v1.0 is not supported (the format has no readers other than
this project).

## 10. MZT (input and output)

MZT is not part of TMZ, it is an input and output format like MZF and WAV.

### 10.1 Format

A sequence of MZF files (128 B header + body of length `fsize`). The UniCMT
extension inserts 128 B **CMTSPEED** headers between the files:

```
Offset  Content
00h     00h (file type)
01h     "CMTSPEED" + 0Dh
12h     0000h, 0000h, 0000h (fsize, fstrt, fexec; there is no body)
20h     01h (meaning unknown, hypothesis: version)
30h     4x WORD LE: LONG first, LONG second, SHORT first, SHORT second half-wave in µs
other   00h
```

UniCMT rules: the speed applies to all following files until the next
header. A header in the first position is not played (observed, cause
unknown).

### 10.2 Import (mzf2tmz)

- The reader recognises CMTSPEED by itself (type 00h, name `CMTSPEED` + 0Dh, fsize 0).
  A classic MZT is the case without headers.
- Each file becomes a block 0x46 with profile `UNICMT`, time
  base 1 MHz and pulses from the last preceding CMTSPEED (without one 1x: 470,
  494, 240, 278 µs).
- The UniCMT signal structure (tones, tapemarks) is not yet known. The
  `UNICMT` profile lists it in the registry as `[unverified]`: it took over the structure of
  ROM-800-NOMINAL (tones according to `gap`, copies, pauses), PULSES in µs
  from PROFILE_ARGS `cmtspeed=LH/LL/SH/SL`, READER (ROM, N = 82) only at 1x.
- A CMTSPEED in the first position of an MZT is not used (warning), a CMTSPEED without
  a following file is not used either [unverified how UniCMT treats them].
- MZT input with explicit tool format options and without CMTSPEED is converted
  according to the options; with CMTSPEED and an option other than the UNICMT profile it is an error
  (the speeds would be lost).

### 10.3 Export (tmz2mzf)

| switch | output |
|---|---|
| `--mzt` (default for MZT) | classic MZT, only MZF files one after another. Timing is lost; the tool prints a warning for blocks that did not have the standard speed or ROM structure. |
| `--mzt-unicmt` | MZT with CMTSPEED wherever the pulses change. Byte 20h = 01h, other bytes according to 10.1. The first file must be 1x; otherwise the tool refuses (a header must not be first). Blocks without ROM structure (header + FM body, e.g. the TurboCopy preloader) are refused by the tool or produce a warning. |

A CMTSPEED header generated according to 10.1 is byte-identical to the file
`2xspeed.mzf` from UniCMT (vector V5).

Block speed for export: 0x40 and ROM profiles = 1x; 0x41 NORMAL
and TAPEMZ-V1-LEGACY normal = floor(1x / speed divisor), i.e. 2:1 and 3:1
exactly 2x and 3x UniCMT; UNICMT = `cmtspeed`. Other blocks do not have
ROM structure: `--mzt` writes them with a warning, `--mzt-unicmt` refuses them.
MZF headers go into the MZT byte for byte from the block. An MZT converted there and back
(`mzf2tmz`, `tmz2mzf --mzt-unicmt`) is byte-identical to the UniCMT samples
`interkar_2x.mzt` and `interkar_3x.mzt` (test_tmz_mzt).

## 11. Test Vectors

The dumps below were produced by the reference model, offsets are from the start of the block
(after the file header `TapeMZ!` 1Ah 01h 01h).

| vector | content | verification |
|---|---|---|
| V1 `v1-rom-800-nominal.tmz` | ROM-800-NOMINAL in µs, body 4 B, tones as ROM (22000/11000, not the registry default tones) | structure (dump below) |
| V2 `v2-tc-2to1-loader-n.tmz` | TC-1.21 2/1 Loader N, emutest.mzf | model = reference recording from the emulator |
| V3 `v3-tc-3to1-loader-y.tmz` | TC-1.21 3/1 Loader Y, inline preloader | model = reference recording from the emulator |
| V4 `v4-custom-x.tmz` | custom profile `X-DEMO`, body slice, 2 contexts | structure |
| V5 `v5-mzt-cmtspeed.mzt` | MZT: file + CMTSPEED 2x + file | CMTSPEED = UniCMT `2xspeed.mzf` byte for byte |
| V6 `v6-rom-800-exact.tmz` | ROM-800-EXACT, 22 contexts, emutest.mzf | model = reference recording from the emulator in every half-wave |

A FASTIPL vector is not yet part of the set (the header template
`$BB` will be taken from `mzcmt_fastipl`).

### 11.1 V1 - ROM-800-NOMINAL

```
0000  46                                               block ID 0x46
0001  32 01 00 00                                      length of the rest of the block = 306
0005  01                                               block version
0006  0f                                               profile ID length = 15
0007  52 4f 4d 2d 38 30 30 2d 4e 4f 4d 49 4e 41 4c     profile ID "ROM-800-NOMINAL"
0016  01 56 32 54 45 53 54 0d ... (128 B)              MZF header
0096  04 00 00 00                                      body length = 4
009A  3e 01 c9 00                                      body
009E  00 00                                            block TLV length = 0
00A0  02                                               number of parts = 2
00A1  4b 00 00 00                                      part 0: length 75
00A5  01 01 00                                         role 1, family 1, source 0
00A8  01 80 04 00                                      TLV TIMEBASE (tag 0001h, mandatory), length 4
00AC  40 42 0f 00                                        = 1000000
00B0  03 80 04 00                                      TLV PAUSE (tag 0003h, mandatory), length 4
00B4  40 42 0f 00                                        = 1000000
00B8  01 81 10 00                                      TLV PULSES (tag 0101h, mandatory), length 16
00BC  f0 00 00 00 16 01 00 00 d6 01 00 00 ee 01 00 00    = SH 240, SL 278, LH 470, LL 494
00CC  02 81 04 00                                      TLV TONE (tag 0102h, mandatory), length 4
00D0  f0 55 00 00                                        = 22000
00D4  03 81 05 00                                      TLV TAPEMARK (tag 0103h, mandatory), length 5
00D8  28 00 28 00 01                                     = 40 L, 40 S, 1 L after the tapemark
00DD  05 81 01 00                                      TLV CHECKSUM (tag 0105h, mandatory), length 1
00E1  01                                                 = algorithm 1
00E2  06 81 03 00                                      TLV END (tag 0106h, mandatory), length 3
00E6  01 00 00                                           = 1 L, 0 S tail
00E9  07 81 03 00                                      TLV COPIES (tag 0107h, mandatory), length 3
00ED  01 00 01                                           = 1 copy, separator 256 S
00F0  43 00 00 00                                      part 1: length 67
00F4  02 01 01                                         role 2, family 1, source 1
00F7  01 80 04 00                                      TLV TIMEBASE (tag 0001h, mandatory), length 4
00FB  40 42 0f 00                                        = 1000000
00FF  01 81 10 00                                      TLV PULSES (tag 0101h, mandatory), length 16
0103  f0 00 00 00 16 01 00 00 d6 01 00 00 ee 01 00 00    = SH 240, SL 278, LH 470, LL 494
0113  02 81 04 00                                      TLV TONE (tag 0102h, mandatory), length 4
0117  f8 2a 00 00                                        = 11000
011B  03 81 05 00                                      TLV TAPEMARK (tag 0103h, mandatory), length 5
011F  14 00 14 00 01                                     = 20 L, 20 S, 1 L after the tapemark
0124  05 81 01 00                                      TLV CHECKSUM (tag 0105h, mandatory), length 1
0128  01                                                 = algorithm 1
0129  06 81 03 00                                      TLV END (tag 0106h, mandatory), length 3
012D  01 00 00                                           = 1 L, 0 S tail
0130  07 81 03 00                                      TLV COPIES (tag 0107h, mandatory), length 3
0134  01 00 01                                           = 1 copy, separator 256 S
```

### 11.2 V4 - custom profile

```
0000  46                                               block ID 0x46
0001  ed 00 00 00                                      length of the rest of the block = 237
0005  01                                               block version
0006  06                                               profile ID length = 6
0007  58 2d 44 45 4d 4f                                profile ID "X-DEMO"
000D  01 56 32 54 45 53 54 0d ... (128 B)              MZF header
008D  04 00 00 00                                      body length = 4
0091  3e 01 c9 00                                      body
0095  00 00                                            block TLV length = 0
0097  01                                               number of parts = 1
0098  56 00 00 00                                      part 0: length 86
009C  02 01 02                                         role 2, family 1, source 2
009F  00 00 00 00 04 00 00 00                          slice: offset 0, length 4
00A7  01 81 10 00                                      TLV PULSES (tag 0101h, mandatory), length 16
00AB  20 03 00 00 84 03 00 00 40 06 00 00 a4 06 00 00    = SH 800, SL 900, LH 1600, LL 1700
00BB  02 81 04 00                                      TLV TONE (tag 0102h, mandatory), length 4
00BF  a0 0f 00 00                                        = 4000
00C3  03 81 05 00                                      TLV TAPEMARK (tag 0103h, mandatory), length 5
00C7  14 00 14 00 01                                     = 20 L, 20 S, 1 L after the tapemark
00CC  05 81 01 00                                      TLV CHECKSUM (tag 0105h, mandatory), length 1
00D0  01                                                 = algorithm 1
00D1  06 81 03 00                                      TLV END (tag 0106h, mandatory), length 3
00D5  01 00 00                                           = 1 L, 0 S tail
00D8  08 81 16 00                                      TLV CONTEXT (tag 0108h, mandatory), length 22
00DC  09 00 ff 20 03 00 00 e8 ... (22 B)                 = BIT_LAST S->* 800/1000; BIT_LAST L->* 1600/1800
```

### 11.3 V3 - TurboCopy 3/1 Loader Y

```
0000  46                                               block ID 0x46
0001  75 06 00 00                                      length of the rest of the block = 1653
0005  01                                               block version
0006  07                                               profile ID length = 7
0007  54 43 2d 31 2e 32 31                             profile ID "TC-1.21"
000E  01 45 4d 55 54 45 53 54 ... (128 B)              MZF header
008E  00 04 00 00                                      body length = 1024
0092  18 fe 00 01 02 03 04 05 ... (1024 B)             body
0492  16 00                                            block TLV length = 22
0494  40 00 12 00                                      TLV PROFILE_ARGS (tag 0040h), length 18
0498  73 70 65 65 64 3d 33 2f ... (18 B)                 = "speed=3/1,loader=Y"
04AA  03                                               number of parts = 3
04AB  cd 00 00 00                                      part 0: length 205
04AF  03 01 03                                         role 3, family 1, source 3
04B2  80 00 00 00                                      inline length = 128
04B6  01 45 4d 55 54 45 53 54 ... (128 B)              inline bytes
0536  01 80 04 00                                      TLV TIMEBASE (tag 0001h, mandatory), length 4
053A  b5 e9 10 00                                        = 1108405
053E  03 80 04 00                                      TLV PAUSE (tag 0003h, mandatory), length 4
0542  9a d0 01 00                                        = 118938
0546  04 00 01 00                                      TLV STOP (tag 0004h), length 1
054A  01                                                 = 01
054B  01 81 10 00                                      TLV PULSES (tag 0101h, mandatory), length 16
054F  14 01 00 00 13 01 00 00 27 02 00 00 27 02 00 00    = SH 276, SL 275, LH 551, LL 551
055F  02 81 04 00                                      TLV TONE (tag 0102h, mandatory), length 4
0563  c8 18 00 00                                        = 6344
0567  03 81 05 00                                      TLV TAPEMARK (tag 0103h, mandatory), length 5
056B  28 00 28 00 01                                     = 40 L, 40 S, 1 L after the tapemark
0570  05 81 01 00                                      TLV CHECKSUM (tag 0105h, mandatory), length 1
0574  01                                                 = algorithm 1
0575  06 81 03 00                                      TLV END (tag 0106h, mandatory), length 3
0579  01 63 00                                           = 1 L, 99 S tail
057C  ae 00 00 00                                      part 1: length 174
0580  04 01 03                                         role 4, family 1, source 3
0583  5a 00 00 00                                      inline length = 90
0587  3e 08 d3 ce e5 21 00 00 ... (90 B)               inline bytes
05E1  01 80 04 00                                      TLV TIMEBASE (tag 0001h, mandatory), length 4
05E5  b5 e9 10 00                                        = 1108405
05E9  03 80 04 00                                      TLV PAUSE (tag 0003h, mandatory), length 4
05ED  6f 01 02 00                                        = 131439
05F1  04 00 01 00                                      TLV STOP (tag 0004h), length 1
05F5  01                                                 = 01
05F6  05 00 03 00                                      TLV READER (tag 0005h), length 3
05FA  00 52 00                                           = routine 0, N = 82
05FD  01 81 10 00                                      TLV PULSES (tag 0101h, mandatory), length 16
0601  14 01 00 00 13 01 00 00 27 02 00 00 27 02 00 00    = SH 276, SL 275, LH 551, LL 551
0611  02 81 04 00                                      TLV TONE (tag 0102h, mandatory), length 4
0615  c8 18 00 00                                        = 6344
0619  03 81 05 00                                      TLV TAPEMARK (tag 0103h, mandatory), length 5
061D  14 00 14 00 01                                     = 20 L, 20 S, 1 L after the tapemark
0622  05 81 01 00                                      TLV CHECKSUM (tag 0105h, mandatory), length 1
0626  01                                                 = algorithm 1
0627  06 81 03 00                                      TLV END (tag 0106h, mandatory), length 3
062B  01 63 00                                           = 1 L, 99 S tail
062E  48 00 00 00                                      part 2: length 72
0632  02 01 01                                         role 2, family 1, source 1
0635  01 80 04 00                                      TLV TIMEBASE (tag 0001h, mandatory), length 4
0639  b5 e9 10 00                                        = 1108405
063D  04 00 01 00                                      TLV STOP (tag 0004h), length 1
0641  01                                                 = 01
0642  05 00 03 00                                      TLV READER (tag 0005h), length 3
0646  01 1b 00                                           = routine 1, N = 27
0649  01 81 10 00                                      TLV PULSES (tag 0101h, mandatory), length 16
064D  74 00 00 00 73 00 00 00 e7 00 00 00 e7 00 00 00    = SH 116, SL 115, LH 231, LL 231
065D  02 81 04 00                                      TLV TONE (tag 0102h, mandatory), length 4
0661  1a 3b 00 00                                        = 15130
0665  03 81 05 00                                      TLV TAPEMARK (tag 0103h, mandatory), length 5
0669  14 00 14 00 01                                     = 20 L, 20 S, 1 L after the tapemark
066E  05 81 01 00                                      TLV CHECKSUM (tag 0105h, mandatory), length 1
0672  01                                                 = algorithm 1
0673  06 81 03 00                                      TLV END (tag 0106h, mandatory), length 3
0677  01 63 00                                           = 1 L, 99 S tail
```

### 11.4 V5 - CMTSPEED header (2x)

```
0000  00 43 4d 54 53 50 45 45 44 0d 00 00 00 00 00 00
0010  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0020  01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030  eb 00 f7 00 78 00 8b 00 00 00 00 00 00 00 00 00
0040  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0050  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0060  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0070  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

## 12. Verification

- Format: writing and reading 0x46 there and back for all data sources; unknown
  mandatory and optional tag; corrupted inputs (lengths beyond the block, slice outside the
  body, P = 0, ID longer than 32) end with an error without reading outside the buffer.
- FM generator against reference recordings from the emulator: ROM-800-EXACT match to the T-state;
  TC-1.21 structure exact, mean deviation within 0.05 tick, every half-wave
  except the first in a part within ±30 T; BASIC-1Z016 match to the T-state (the comparison does not
  check pauses, because the emulator recording also contains the time the motor is stopped: they are checked by the
  parameter test in test_tmz_profile).
- v1.0 compatibility: half-waves of the TAPEMZ-V1-LEGACY conversion against the v1.0 generator (same count and levels, length ±1 sample, chapter 9.2), loading in the emulator.
- MZT: classic export and import there and back; `--mzt-unicmt` (placement of
  CMTSPEED, first file 1x, byte identity with UniCMT); import of the UniCMT
  samples `interkar_*.mzt`.
- Emulator: loading v1.1 files in the emulator.
- Lint: read point margins (TurboCopy 2:1 OK). Player: TurboCopy 8:3
  and 3:1 at 22 050 Hz rejected, 2:1 at 22 050 Hz warning.
- HW: logic analyzer on the CMT output (not yet done).
