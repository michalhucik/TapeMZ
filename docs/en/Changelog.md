# Changelog

## 2026-04-08

### tmzinfo v1.2.0
- Speed display in Bd for TZX blocks 0x10 (Standard Speed Data)
  and 0x11 (Turbo Speed Data). Formula: Bd = 3500000 / (zero + one).

### tmzedit v1.3.0
- New option `--sinclair-speed <Bd>` for editing timing of block 0x11
  (Turbo Speed Data). Accepts exactly 4 values: 1381, 1772, 2074, 2487.
  Recalculates all timing parameters (pilot, sync, zero, one)
  while preserving the ZX ratio zero:one = 1:2.

### wav_analyzer v1.4.0
- SINCLAIR format support (ZX Spectrum protocol on Sharp MZ).
  SINCLAIR uses ZX encoding (pilot -> sync -> data with 1:2 pulse
  ratio) at various speeds. Intercopy records in this mode at
  1381, 1772, 2074, and 2487 Bd.
- ZX fallback after FM tapemark detection failure: when the FM decoder
  fails to find a tapemark, it tries the ZX Spectrum decoder. On success,
  classifies the result as SINCLAIR. Safe for all FM formats (ZX sync
  detection reliably fails for genuine NORMAL FM due to absence of
  sufficiently short pulses).
- New test `test_ic_sinclair_all` - verifies 4 copies of Turbo Copy V1.21
  recorded by Intercopy in SINCLAIR mode (1381-2487 Bd).

### wav2tmz v2.4.0
- SINCLAIR blocks are stored as TZX block 0x11 (Turbo Speed Data)
  with timing scaled from leader tone average half-period relative to
  standard ZX pilot (2168 T-states = 619.4 us). Preserves speed
  across TMZ round-trip.
- Fix segfault when converting SINCLAIR recordings: SINCLAIR results
  have tap_data (not mzf), but were previously routed to
  create_block_turbo which accessed mzf (NULL pointer dereference).
- SINCLAIR block speed display in summary output
  (e.g. "SINCLAIR flag=0xFF, speed=1.30x (476 us)").

### wav_analyzer v1.3.0
- Support for decoding Intercopy FAST IPL recordings at all speeds
  (1200, 2400, 2800, 3200 Bd). FASTIPL is a standard two-block MZF:
  header block (LGAP 22000 h.p. + LTM + $BB header) at NORMAL speed,
  body block (LGAP 11000 h.p. + STM + body data) at TURBO speed.
- FASTIPL decoder based on analysis of Intercopy 10.2 write handler
  (sub_19EE): body LGAP is 5500 pulses (half of header LGAP), body
  tapemark is STM (20+20, not LTM 40+40). Reduced FM threshold factor
  1.4x (vs global 1.6x) for reliable tapemark detection at 2800+ Bd.
- FASTIPL body leader returned via new `out_data_leader` parameter
  for correct speed class and leader info in file results.
- New field `header_leader_pulse` in file results for correct coverage
  calculation of two-part formats (TURBO/FASTIPL).

### wav2tmz v2.3.0
- FASTIPL speed estimation from body LGAP leader average half-period.
- Chronological interleaving of decoded files and raw blocks (Direct
  Recording) in TMZ output. Previously all decoded files were written
  first, followed by all raw blocks at the end.
- Raw block coverage uses `header_leader_pulse` instead of
  `leader.start_index` to exclude header portions from raw blocks.

### mzf2tmz v1.2.0
- `--speed` now accepts baudrate values (e.g. `--speed 2800`) in addition
  to ratio format (`--speed 7:3`). Baudrate is mapped to nearest CMTSPEED.
- FASTIPL speed displayed as baudrate (e.g. "2800 Bd") instead of ratio.

### tmzedit v1.2.0
- `--speed` now accepts baudrate values (e.g. `--speed 2800`) in addition
  to ratio format (`--speed 7:3`). Baudrate is mapped to nearest CMTSPEED.
- FASTIPL speed displayed as baudrate in `convert` and `set` commands.

### tmzinfo v1.1.0
- FASTIPL speed displayed as baudrate (e.g. "2800 Bd") instead of
  ratio format ("7:3 - 2800 Bd"), matching Intercopy convention.

### cmtspeed v2.0.0
- New function: `cmtspeed_from_bdspeed()` - maps baudrate to nearest
  `en_CMTSPEED` value.

### mzcmt_fastipl v1.2.0
- Fix signal structure based on Intercopy 10.2 write handler (sub_19EE):
  - Header LGAP: 11000 pulses (was 22000 - double of Intercopy).
  - Body LGAP: 5500 pulses (was 22000, Intercopy: HL=$157C=5500).
  - Body tapemark: STM 20+20 (was LTM 40+40, Intercopy writes STM for body).
  - Removed SGAP+STM+CRC(=0) section from header block (Intercopy doesn't write it).
  - Removed 1000 ms pause between blocks (Intercopy writes without pause).
  - Loader binary extended from 96B to 110B (covers offsets $12-$7F,
    matching Intercopy sub_2035 layout).
- Readpoint: lookup table from reference recording ic-loader-all.wav
  (1:1=77, 2:1=32, 7:3=22, 8:3=17). Previous formula floor(82/divisor)
  gave wrong values (82, 41, 35, 30) - Intercopy computes readpoint
  via sub_201C/sub_1E09, not a simple division.
- Pulseset MZ-800: asymmetric (227/272 us SHORT, 476/499 us LONG)
  derived from reference recording. At 44100 Hz: SHORT=10+12=22 smp,
  LONG=21+22=43 smp. Asymmetric pulseset is required for correct
  scaling at turbo speeds (2:1 gives 5+6=11 smp, not symmetric 5+5=10).
  Readpoint and pulseset must be consistent (both from same reference).
- Verified in emulator: ROM loads all 4 speeds (1200-3200 Bd),
  Intercopy statistics match reference recording.

### mzcmt_bsd v1.1.0
- Pulseset MZ-800: symmetric 498/498 us LONG, 249/249 us SHORT
  (previously asymmetric 470/494, 246/278 from Intercopy measurements).
- Pulseset MZ-700: symmetric 504/504, 252/252 (previously 464/494, 240/264).
- Consistent with mzcmt_turbo and mztape vstream pulsesets.

## 2026-04-07#

### mzcmt_turbo v2.0.1
- Fix ROM delay: formula changed from `round(82/speed_ratio)` to
  `floor(82/speed_ratio)` (truncation). TurboCopy on Z80 uses
  integer division which truncates the fractional part. Rounding
  caused an error at 8:3 speed (delay 31 instead of 30), leading
  to ~2.5% speed deviation (2625 Bd instead of 2692 Bd).

### mztape v2.0.1
- Fix MZ-800 vstream pulseset: changed from asymmetric pulses
  (Intercopy 10.2 measurements: 245.8/278.2 us SHORT) to symmetric
  (249/249 us SHORT, 498/498 us LONG). Asymmetric values rounded
  to 11+12=23 samples instead of 11+11=22 at 44100 Hz, causing
  ~4.5% speed deviation (1099 Bd instead of 1150 Bd). Symmetric
  values match ROM behavior and are consistent with mzcmt_turbo
  g_pulses_800.

# 2026-04-07

### wav_analyzer v1.2.0
- Support for decoding TurboCopy TURBO recordings (speeds 2:1, 7:3, 8:3, 3:1 etc.).
  TurboCopy TURBO preloader (fsize=90, fstrt=$D400) patches the ROM and uses
  the standard CMT read routine to read the body at TURBO speed. The decoder
  extracts metadata (fsize/fstrt/fexec) from the preloader body and decodes
  TURBO data as standard FM (body-only, STM tapemark).
- Support for mzftools TURBO format (fsize=0, loader in comment). Metadata
  extracted from header comment area (cmnt[1..6]).
- New function: `wav_decode_turbo_turbocopy_mzf()` - TurboCopy body-only decoding.
- New function: `wav_decode_turbo_mzftools_mzf()` - mzftools body-only decoding.
- Fix: TURBO dispatch in `process_leader()` - missing `else` between TurboCopy
  and mzftools TURBO paths caused the decode result to be discarded.
- Fix: leader skip condition `leader_end < skip_until_pulse` instead of
  `start_index < skip_until_pulse` - leader at consumed boundary was
  incorrectly skipped (round-trip 3/5 -> 5/5).
- Improvement: TURBO decoder uses `wav_leader_detect()` to find the actual
  TURBO leader instead of a synthetic leader with pulse_count=0.

### mzcmt_turbo v2.0.0
- Complete rewrite of tape encoder to TurboCopy compatible format.
  Preloader now generates fsize=90, fstrt=$D400 (instead of mzftools fsize=0, $1110).
  TurboCopy format works on real hardware, in emulators, and with TurboCopy/Intercopy.
- Embedded TurboCopy loader (75B generic code from reverse eng. TurboCopy V1.21)
  with patchable data section (speed_val, fsize/fstrt/fexec, ROM params).
- Preloader header: TurboCopy identification signature in cmnt[0..6],
  original comment data in cmnt[7..103].
- TURBO data section: body-only format (STM tapemark + body + CRC).
  Matches the structure of real TurboCopy recordings.
- ROM delay: formula `round(82/speed_ratio)` derived from measurements of real
  TurboCopy recordings. Replaces incorrect mzftools lookup table.
- Pulseset MZ-800: symmetric pulses (249/249 us SHORT, 498/498 us LONG).
  ROM generates symmetric pulses; original asymmetric values (246/278, 470/494)
  caused incorrect rounding at 44100 Hz.
- Pulseset MZ-700: similarly symmetric (252/252, 504/504).

### wav2tmz v2.2.0
- TURBO file speed estimation from leader average half-period
  (previously only for NORMAL/MZ-80B, now also for TURBO format).
- More accurate speed for TurboCopy from speed_val byte in preloader ($4B).

### New utility: extract_preloader
- Extracts TurboCopy TURBO preloader binary (90B) from WAV recordings.

### New test data
- `tstdata/tc-loader-all.wav` - 5 copies of Turbo Copy V1.21 with TURBO loader
  (speeds 1:1, 2:1, 7:3, 8:3, 3:1).
- `tstdata/mzf/Turbo_Copy_V1.21.mzf` - reference MZF for verification.
- `tests/test_tc_loader_all.c` - integration test (5 copies, body match, CRC OK).

### Attribution
- Fixed attribution: TURBO = TurboCopy (Michal Kreidl),
  FAST IPL = Intercopy (Marek Smihla - NIPSOFT).

## 2026-04-06

### wav2tmz v2.1.0
- Partial BSD data recovery: incomplete BSD files (missing terminator
  chunk ID=0xFFFF) can now be salvaged using the `--recover-bsd` option.
- Diagnostics are always enabled - when BSD decoding fails, the file name,
  error type and a hint to use `--recover-bsd` are printed.
- Recovered files are marked `[RECOVERED]` in the output summary
  and in TMZ archives a Text Description block (0x30) with a warning is added.
- New options: `--recover` (all recovery modes),
  `--recover-bsd`, `--recover-body`, `--recover-header`
  (the latter two prepared for phase 2, currently no effect).

### wav_analyzer v1.1.0
- BSD decoder: partial data recovery support (allow_partial).
- BSD decoder: sequential chunk ID validation - prevents reading data
  from subsequent files on tape as false BSD chunks.
- BSD decoder: consumed_until fix - when the terminator is missing,
  the position after the last successfully read chunk is used.
- New data types: `en_WAV_RECOVERY_STATUS`, recovery fields in config
  and analysis result.
- New function: `wav_recovery_status_string()`.

### mzf2tmz v1.1.0
- Added `--fsk-speed 0-6` option for FSK format speed level selection.
- Added `--slow-speed 0-4` option for SLOW format speed level selection.
- `--speed` (ratio) is now rejected for FSK and SLOW formats with a clear error
  message pointing to the correct option.
- `--fsk-speed` and `--slow-speed` are rejected for non-FSK/non-SLOW formats.
- Speed values are stored as native format-specific levels in block 0x41
  (previously the generic CMTSPEED ratio index was stored, which did not
  correspond to actual FSK/SLOW speed levels).

### tmzedit v1.1.0
- `set` command: added `--fsk-speed 0-6` and `--slow-speed 0-4` options,
  with the same validation rules as mzf2tmz.

### bsd2dat v1.0.1
- Fix: data export now includes the termination chunk data (ID=0xFFFF),
  matching the BSD decoder and real MZ-800 BASIC behavior.

### dat2bsd v1.0.1
- Fix: the last data chunk is now marked as termination (ID=0xFFFF)
  and carries the final portion of data. Previously an extra termination
  chunk with zero data was appended, causing inconsistency in WAV round-trips.

### wav2tmz v2.0.1
- Fix: when creating TMZ block 0x45 (MZ BASIC Data) from a decoded BSD
  recording, the last chunk is now marked as termination (ID=0xFFFF) and
  carries data. Aligns with dat2bsd and BSD decoder logic.

### All libraries
- Added *_version() function returning the library version string.

### All tools
- Added --version option to show program version.
- Added --lib-versions option to show versions of used libraries.

## 2026-04-05

### Version 1.0.0 - Initial release
- Libraries: tmz, tmz_blocks, tmz_player, tzx, mzf, mzf_tools, wav,
  cmt_stream (bitstream, vstream), cmtspeed, endianity, sharpmz_ascii,
  sharpmz_utf8, mztape, zxtape, generic_driver, memory_driver
- Encoders: mzcmt_bsd, mzcmt_cpmtape, mzcmt_direct, mzcmt_fastipl,
  mzcmt_fsk, mzcmt_slow, mzcmt_turbo
- WAV analyzer: wav_preprocess, wav_pulse, wav_leader, wav_histogram,
  wav_classify, wav_decode_fm, wav_decode_bsd, wav_decode_cpmtape,
  wav_decode_direct, wav_decode_fastipl, wav_decode_fsk, wav_decode_slow,
  wav_decode_turbo, wav_decode_zx
- Tools: tmzinfo, mzf2tmz, tmz2mzf, tmz2wav, wav2tmz, tap2tmz,
  tmz2tap, tmzconv, tmzedit, dat2bsd, bsd2dat
