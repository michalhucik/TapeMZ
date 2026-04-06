# Changelog

## 2026-04-06

### bsd2dat v1.0.1
- Oprava: export dat nyni zahrnuje i data terminacniho chunku (ID=0xFFFF),
  coz odpovida chovani BSD dekoderu a realneho MZ-800 BASIC.

### dat2bsd v1.0.1
- Oprava: posledni datovy chunk se nyni oznaci jako terminacni (ID=0xFFFF)
  a nese posledni porci dat. Driv se pridaval extra terminacni chunk
  s nulovymi daty, coz zpusobovalo nekonzistenci pri WAV round-tripu.

### wav2tmz v2.0.1
- Oprava: pri vytvareni TMZ bloku 0x45 (MZ BASIC Data) z dekodovaneho
  BSD zaznamu se nyni posledni chunk oznaci jako terminacni (ID=0xFFFF)
  a nese data. Sjednoceni logiky s dat2bsd a BSD dekoderem.

### Vsechny knihovny
- Pridana funkce *_version() vracejici retezec s verzi knihovny.

### Vsechny nastroje
- Pridana volba --version pro zobrazeni verze programu.
- Pridana volba --lib-versions pro zobrazeni verzi pouzitych knihoven.

## 2026-04-05

### Verze 1.0.0 - Prvni vydani
- Knihovny: tmz, tmz_blocks, tmz_player, tzx, mzf, mzf_tools, wav,
  cmt_stream (bitstream, vstream), cmtspeed, endianity, sharpmz_ascii,
  sharpmz_utf8, mztape, zxtape, generic_driver, memory_driver
- Kodery: mzcmt_bsd, mzcmt_cpmtape, mzcmt_direct, mzcmt_fastipl,
  mzcmt_fsk, mzcmt_slow, mzcmt_turbo
- WAV analyzer: wav_preprocess, wav_pulse, wav_leader, wav_histogram,
  wav_classify, wav_decode_fm, wav_decode_bsd, wav_decode_cpmtape,
  wav_decode_direct, wav_decode_fastipl, wav_decode_fsk, wav_decode_slow,
  wav_decode_turbo, wav_decode_zx
- Nastroje: tmzinfo, mzf2tmz, tmz2mzf, tmz2wav, wav2tmz, tap2tmz,
  tmz2tap, tmzconv, tmzedit, dat2bsd, bsd2dat
