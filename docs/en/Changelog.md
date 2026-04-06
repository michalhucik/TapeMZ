# Changelog

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
