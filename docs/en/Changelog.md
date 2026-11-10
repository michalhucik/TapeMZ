# Changelog

## 2026-11-10

### TZX block 0x18 CSW Recording

Versions: tzx v1.3.0, tmz v1.13.0, tmz_player v1.5.0.

- The tzx library can create block 0x18 (CSW Recording) with RLE compression
  (`tzx_block_create_csw_recording()`) and generate a signal from it
  (`tzx_generate_csw_recording()`, `tzx_generate_block()`). Pulse lengths are in samples
  at an exact sample rate in Hz, so a 44 100 Hz recording plays with the correct length
  at any output rate.
- The player (`tmz_player_play_next()`, `tmz_player_play_block()`, `tmz2wav`) plays
  block 0x18. The first pulse is low, levels alternate, and after the block the level is
  the opposite of the last pulse (as after block 0x15).
- Z-RLE compression is not played (error with a message).

### Linux build and automated tests

- The project builds on Linux (GCC) too: CMake links the math library to all targets there.
- The repository has automated tests (GitHub Actions): `make test` on Linux and on Windows
  (MSYS2 UCRT64) on every push and pull request.

### sharpmz_ascii v2.2.1

- Library upgrade to version 2.2.1 from the standalone sharpmz_ascii repository.
- Functions returning UTF-8 (`sharpmz_eu_convert_to_UTF8()`, `sharpmz_jp_convert_to_UTF8()`,
  `sharpmz_koi8cs_convert_to_UTF8()`, `sharpmz_display_to_utf8()`, `sharpmz_to_utf8()`)
  use a rotating buffer in thread-local storage (`SHARPMZ_UTF8_ROTATING_SLOTS` = 16 slots).
  They are thread-safe and can be called several times in one expression.

### Profile MZ800EMU-CMT

Versions: tmz_profile v1.9.0, tmz_mzt v1.2.0, mztape v2.4.0.

- New profile `MZ800EMU-CMT`: block 0x46 with the signal the mz800emu emulator plays
  from an MZF (mztape without copies, GDG clock 17 721 600 Hz, all speed ratios including
  2:1 CP/M and custom pulses in µs). Played at 17 721 600 Hz it gives the same pulses as mztape.
- MZT export for UniCMT (`tmz2mzf --mzt-unicmt`) treats blocks of this profile as a ROM recording.
- PROFILE_ARGS reads the speed `2:1 (cp/m)`.
- mztape: `mztape_get_format()` returns the definition of a format variant (tones, pulse set, blocks).

## 2026-11-09

### Libraries: custom pulses and pulse sets aligned with the emulator

Versions: cmtspeed v2.1.0, mztape v2.3.1, tmz_profile v1.8.0, mzf v2.1.1, tmzedit v1.7.2.

- mztape: API for custom pulse lengths and for the CMTSPEED header; new speed value
  `CMTSPEED_CUSTOM` (cmtspeed).
- The mztape pulse sets match what the Sharp MZ-800 emulator plays; the earlier symmetric
  sets 252/504 and 249/498 are gone.
- The TAPEMZ-V1-LEGACY profile keeps its own historical v1.0 pulse sets, so converting v1.0
  blocks gives the same output as before.
- Building a bitstream from a TMZ block rejects an invalid speed instead of computing from
  unfilled lengths.
- `tmzedit` no longer reads past the speed table when a block carries an unknown speed
  byte (shown as "?:?").
- Corrected file type descriptions 02h-05h in the mzf library.

## 2026-11-08

### Recovery of damaged recordings

Versions: tmz v1.12.0, wav_analyzer v1.14.0, wav2tmz v2.13.0.

- `wav2tmz --recover-body`: a cut-off or overwritten body of an FM record is kept,
  the rest is filled with 00h. The tone of the next record is not consumed.
- `--recover-body` also for TURBO, FASTIPL and CP/M TAPE; an FM body with errors in both
  copies is assembled from their valid parts when the checksum on the tape confirms it unambiguously.
- `--recover-bsd`: missing BSD chunks are filled with zeros in their place.
- A warning when the copies differ although both have a valid checksum.
- `wav2tmz --recover-header`: a header with a valid checksum and no body is kept.
- A header with a checksum error and a destroyed size: the size is repaired from
  the body length on the tape if the body checksum agrees with it.
- A record whose body could not be read is reported even without `--verbose`.
- A damaged NORMAL 1:1 record is written to TMZ as block 0x46 with the checksum bytes
  from the tape, so playback behaves like the damaged tape.

## 2026-11-06

### TMZ v1.1

Versions: tmz v1.11.0, tzx v1.2.0, wav v2.1.0, mzf v2.1.0, cmt_stream v2.0.1, cmtspeed v2.0.1, generic_driver v2.0.1, zxtape v2.0.1, wav_analyzer v1.12.0, mzcmt_fm v1.1.1 (new), mzcmt_manchester v1.0.0 (new), mzcmt_turbo v2.3.0, mzcmt_direct v1.0.1, mzcmt_fsk v1.0.1, mzcmt_slow v1.0.1, mztape v2.2.0, mzcmt_fastipl v1.3.0, mzcmt_bsd v1.1.1, mzf2tmz v1.11.0, tmz2mzf v1.4.1, tmz2wav v1.3.1, tmzinfo v1.7.1, tmzedit v1.7.1, tmzconv v1.1.0, wav2tmz v2.11.1, dat2bsd v1.1.0, bsd2dat v1.2.1, extract_preloader v1.0.1.

- New block 0x46 (MZ File): an MZF file and a list of recording parts,
  each with its own timing (pulses, tones, tapemarks, context-dependent
  half-wave lengths, pauses). Files are written as format 1.1. Writer
  profiles in the registry: ROM-800-NOMINAL, ROM-800-EXACT, TC-1.21
  (TurboCopy V1.21), IC-10.2-NORMAL/TURBO/FASTIPL/CPM (Intercopy V10.2),
  UNICMT, PEZIK-2.0A (CP/M TAPE.COM), BASIC-1Z016 (BASIC 1Z-016 data files)
  and TAPEMZ-V1-LEGACY. Profile timing comes from the programs' code and
  from emulator recordings; the UNICMT signal structure is not verified yet.
- TMZ v1.0 blocks (0x40, 0x41, 0x45) are converted to block 0x46 when
  played; `tmzconv --upgrade` stores the conversion in a file. Output
  changes against v1.0: 0x40 with the MZ-800 set plays MZ-800 ROM timing,
  TURBO plays TurboCopy V1.21, FAST IPL 1:1, 2:1, 7:3 and 8:3 plays
  Intercopy V10.2, CP/M TAPE plays TAPE.COM, 0x45 plays BASIC 1Z-016 and
  the pause after a block is played.
- CP/M TAPE (Pezik/MarVan TAPE.COM and Intercopy CPM) in Manchester coding
  and BASIC data files with a tone, tapemark and copy for every chunk, as
  the original programs write them. Library mzcmt_cpmtape was replaced by mzcmt_manchester.
- TURBO (0x41): pulses and read point per TurboCopy V1.2x; 1:1, 2:1, 7:3,
  8:3 and 3:1 load in the emulator at 44.1 and 48 kHz. Timing the ROM cannot
  read with a 10 µs margin is an error.
- mzf2tmz: `--profile` (`--profile list`), `--bd`, `--loader`, `--gap`,
  MZT import including CMTSPEED headers (UniCMT). tmz2mzf: extraction from
  block 0x46, export `--mzt` and `--mzt-unicmt`. tmzinfo: block 0x46
  listing, `--lint`, block 0x35 listing. tmz2wav: plays block 0x46.
- SINCLAIR (ZX Spectrum protocol) is stored and played as TZX blocks
  0x10/0x11; `mzf2tmz --format sinclair` and `tmzedit set --format
  sinclair` for block 0x41 reject it and point to them. tmzedit rejects the cpm-tape
  format for block 0x41 (`mzf2tmz --format cpm-tape`).
- wav2tmz: decoding of TURBO at all speeds (also Intercopy 2800/3200 Bd
  and FAST IPL at 44100 Hz), second copies
  (Copy2), CP/M TAPE (both polarities, variant and speed), CRC of every BSD
  chunk using the copy, FSK/SLOW/DIRECT body checksum, WAV EXTENSIBLE,
  IEEE float and 64-bit PCM. The default `--tolerance` is 0.20.
- Player: tape stop (Pause 0, block 0x2A), signal level between blocks per
  TZX 1.20, reserved blocks 0x47-0x4F are skipped with a warning. tmz2wav
  rejects a signal the read routine could not read at the selected sample
  rate and returns code 1 for a block that is not played.
- tzx: the pilot of blocks 0x10/0x11 has `pilot_count` pulses, block 0x35
  is read with a 16-byte identifier, a truncated file and a major version
  other than 1 are errors, new function `tzx_generate_block()`.
- dat2bsd writes block 0x46 BASIC-1Z016 (type 04h, without `--speed` and
  `--pulseset`), bsd2dat reads blocks 0x45 and 0x46; tmzedit validate
  checks block 0x45. Type 04h is listed as "BRD (BASIC data, WOPEN#)".
- Tests without the `tstdata/` directory print SKIP and succeed.
- Fixes: out-of-bounds reads and crashes on damaged files, TLV and block
  0x32 buffer bounds, the custom pulse unit (0.1 µs) in help and
  documentation.
- wav2tmz recognizes the writer: a TurboCopy V1.2x recording (by the tone length) is
  stored as block 0x46 `TC-1.21` with the ratio a/b and the preloader choice, an
  Intercopy V10.2 NORMAL, TURBO and FAST IPL recording as `IC-10.2-NORMAL` / `-TURBO` /
  `-FASTIPL` with speed `bd` (600 to 3200 Bd).
- Profile `TC-1.21` supports all 34 TurboCopy ratios (a, b from 1..9, 1/2 to 3/1);
  `mzf2tmz --profile TC-1.21 --speed 9:4`; `tmz2mzf --mzt-unicmt` writes TC-1.21 without
  the preloader with a CMTSPEED header from the ratio (1:1 as 1x).
- wav2tmz: FSK and SLOW speed level from the delay byte in the loader, a ZX recording
  with a slow pilot (Intercopy SINCLAIR 900 Bd) as block 0x11, the actual Intercopy CPM
  speed in the summary, the original MZF comment behind the TurboCopy preloader.
- wav2tmz warns when TMZ cannot express the speed (NORMAL far from the block 0x41
  speeds, unrecognized FSK/SLOW level).
- Profile `IC-10.2-CPM` for all Intercopy CPM speeds (1500, 2400, 2800, 3200, 3600 Bd,
  timing measured in the emulator); wav2tmz recognizes and writes them.
- Byte 11h of the MZF header is kept unchanged all the way (mzf2tmz, v1.0 and 0x46 blocks,
  playback, wav2tmz, tmz2mzf, the TurboCopy preloader); before, v1.0 blocks and WAV decoding
  rewrote it to 0Dh. ROM SAVE writes the byte from memory (e.g. 00h) and copy programs pass
  the header on as they read it. mzf library: new function `mzf_header_swap_endianity()`.
  Consequence: mzf2tmz without `--profile` makes a different TMZ from an MZF with 00h at 11h
  than before (00h instead of 0Dh); a v1.0 block with a byte other than 0Dh plays with it.
  Files with 0Dh play unchanged.
- TZX blocks play as on the ZX: the first pulse after a pause (and at the start of
  playback) makes an edge, so the whole pilot is present; TZX 1.20 would write it at
  the pause level. Played TZX tapes therefore have one pilot pulse more than before
  (no effect on loading).
- wav2tmz decodes Intercopy NORMAL 2800 and 3200 Bd also from a WAV made from TMZ
  (asymmetric half-waves of the tone and the tapemark are also searched by periods).
- TMZ -> WAV -> TMZ check on 121 recordings: all tapes byte-identical.

## 2026-08-15

### tmzedit v1.6.0, extract_preloader
- New option `--dump-charset <mode>` for hex dumps (tmzedit dump,
  extract_preloader). Modes: raw (default, standard ASCII), eu, jp,
  utf8-eu, utf8-jp (bytes interpreted as Sharp MZ ASCII EU/JP with ASCII
  or UTF-8 output). Non-convertible or non-printable bytes are shown as '.'.

### mzf_tools v2.2.0
- New hex dump API with character set: en_MZF_DUMP_CHARSET,
  mzf_tools_parse_dump_charset(), mzf_tools_dump_char(), mzf_tools_hex_dump().

### sharpmz_ascii v2.2.0
- Library upgrade to version 2.2.0 from the standalone sharpmz_ascii
  repository. Adds KOI8-CS charset (sharpmz_koi8cs_*) and display codes
  (sharpmz_display_*); enum sharpmz_charset_t extended with KOI8CS,
  DISPLAY_EU and DISPLAY_JP. EU/JP behaviour unchanged.

### wav v2.0.0, mzf v2.0.0
- Error messages of the wav and mzf libraries and the MZF header dump are
  now in English (unified with the mz800new emulator, project convention).

## 2026-04-18

### sharpmz_ascii v2.1.0
- Library upgrade to version 2.1.0 from standalone sharpmz_ascii repository.
  Merged sharpmz_ascii and sharpmz_utf8 into a single module.
- New functions: sharpmz_convert_to_ASCII(), sharpmz_eu_convert_to_UTF8(),
  sharpmz_eu_convert_UTF8_to(), sharpmz_jp_cnv_from(), sharpmz_jp_cnv_to(),
  sharpmz_jp_convert_to_ASCII(), sharpmz_jp_convert_to_UTF8(),
  sharpmz_jp_convert_UTF8_to().
- Dispatch wrappers: sharpmz_to_utf8(), sharpmz_from_utf8(),
  sharpmz_str_to_utf8(), sharpmz_str_from_utf8() with sharpmz_charset_t parameter.
- Added mapping for '^' (0x8B) and '`' (0x93) in EU variant.

### tmzinfo v1.4.0, tmzedit v1.5.0, wav2tmz v2.7.0, tmz2mzf v1.2.0, mzf2tmz v1.3.0, bsd2dat v1.1.0
- New option `--charset <mode>` replaces the former `--name-encoding`.
  Modes: eu (default), jp, utf8-eu, utf8-jp.
- New mode `jp` for Japanese Sharp MZ character set (Sharp MZ-JP -> ASCII).
  Converts only characters 0x20-0x5D, katakana and kanji are replaced with spaces.
- Mode `eu` (formerly `ascii`) converts the European character set including
  lowercase letters. Modes `utf8-eu` and `utf8-jp` display the maximum number
  of characters including umlauts, arrows, katakana, and kanji.

### mzf_tools v2.1.0
- Function mzf_tools_get_fname_ex() now supports MZF_NAME_ASCII_JP
  (Japanese character set -> ASCII).
- Function mzf_tools_get_fname() is now a wrapper over mzf_tools_get_fname_ex().

## 2026-04-09

### tmz2mzf v1.1.0
- New option `--overwrite` to allow overwriting existing output files.
  By default, the program now refuses to overwrite an existing file
  and suggests `--overwrite` or `--append`.
- New option `--append` to append extracted MZF blocks to an existing
  file, creating a multi-MZF (MZT-style) file with concatenated
  header+body records. If the file does not exist, a new file is created.
  When extracting multiple blocks, all blocks go into a single output file.

### tmz2wav v1.1.0
- New option `--blocks <spec>` for selecting specific blocks to export.
  Format: numbers and ranges separated by commas (e.g. "0", "0,2", "0-2,5").
  Control blocks (loops, jumps, calls) are always processed.
- New option `--append` for appending new audio signal to the end of an
  existing WAV file. Validation: sample rate must match, file must be
  mono PCM. Reports an error if the file does not exist.

### wav_analyzer v1.6.0
- New fields `start_time_sec` and `duration_sec` in file results - absolute
  block start time and duration in the WAV file. Computed from pulse indices
  before the pulse sequence is released.
- Function `wav_analyzer_print_summary()` has a new `verbose` parameter.
  In verbose mode, prints real speed (Bd), approximate speed (nearest
  CMTSPEED), and pulse set (MZ-800/MZ-80B) for each block.
- New dependency on cmtspeed library (for approximate speed calculation).

### wav2tmz v2.6.0
- Default mode is analysis-only (no saving). Saving is activated by
  `-o` or `--output-format` options.
- Summary output always includes absolute block start times and durations.
- New option `--append-tmz` - append blocks to an existing TMZ file.
  Without this option, an existing TMZ file causes an error.
- New option `--overwrite-mzf` - overwrite existing MZF files.
  Without this option, an existing MZF file causes an error.
- In verbose mode (--verbose), displays: real speed (Bd), approximate
  speed, pulse set, and measured pulse widths.

### wav_analyzer v1.5.0
- New fields `short_pulse_us` and `long_pulse_us` in file results - measured
  pulse widths from histogram peaks (FM signals with 2+ peaks).
  Enables exact pulse width preservation in TMZ blocks.

### mztape v2.1.0
- New function `mztape_create_stream_from_mztapemzf_ex()` - extended version
  with optional custom pulse widths (us*100 fields). Non-zero values override
  the default pulseset+speed calculation. Same conversion convention as
  mzcmt_turbo (seconds = value / 1e7).

### tmz_player v1.1.0
- NORMAL format playback (block 0x41) now uses `_ex` variant to pass
  custom pulse fields (`long_high/low`, `short_high/low`) from the block.
  Previously these fields were ignored for NORMAL format.

### wav2tmz v2.5.0
- New option `--pulse-mode <approximate|exact>` (default: approximate).
  In `exact` mode, NORMAL/MZ-80B files always use block 0x41 with custom
  pulse fields from histogram analysis (speed=0, pulse fields != 0).
  In `approximate` mode, behavior is unchanged (CMTSPEED quantization).

### tmzinfo v1.3.0
- Custom pulseset display for block 0x41: when pulse fields are non-zero,
  shows "Pulseset: custom" with Long/Short values in microseconds and
  estimated baudrate instead of the standard pulseset name and speed ratio.

### tmzedit v1.4.0
- New option `--pulse <long_h/long_l,short_h/short_l>` for the `set` command.
  Sets custom pulse mode (speed=0, pulse fields filled) on block 0x41.
  Values are in us*100 units (e.g. `--pulse 4980/4980,2490/2490` for ~1339 Bd).
- `--speed` on a block with custom pulses now clears pulse fields
  (switches back to table mode).
- Conversion 0x41 -> 0x40 is blocked when custom pulse fields are present.

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
- FASTIPL speed displayed as baudrate in `convert` (command does not exist;
  use tmzconv) and `set` commands.

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

## 2026-04-07

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
