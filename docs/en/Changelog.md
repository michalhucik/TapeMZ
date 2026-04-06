# Changelog

## 2026-04-06

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
