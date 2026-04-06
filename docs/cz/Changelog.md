# Changelog

## 2026-04-06

### wav2tmz v2.1.0
- Obnova castecnych BSD dat: nekompletni BSD soubory (chybejici ukoncovaci
  chunk ID=0xFFFF) lze nyni zachranit pomoci volby `--recover-bsd`.
- Diagnostika je vzdy zapnuta - pri selhani BSD dekodovani se vypise
  nazev souboru, typ chyby a napoveda na `--recover-bsd`.
- Obnovene soubory jsou ve vystupu oznaceny `[RECOVERED]`
  a v TMZ archivu jsou doplneny blokem Text Description (0x30) s varovanim.
- Nove volby: `--recover` (vsechny recovery mody),
  `--recover-bsd`, `--recover-body`, `--recover-header`
  (posledni dve pripraveny pro fazi 2, zatim bez efektu).

### wav_analyzer v1.1.0
- BSD dekoder: podpora obnovy castecnych dat (allow_partial).
- BSD dekoder: validace sekvencnich chunk ID - zamezuje cteni dat
  z nasledujicich souboru na pasce jako falesnych BSD chunku.
- BSD dekoder: oprava consumed_until - pri chybejicim terminatoru
  se pouziva pozice za poslednim uspesne prectenim chunkem.
- Nove datove typy: `en_WAV_RECOVERY_STATUS`, recovery pole v konfiguraci
  a ve vysledku analyzy.
- Nova funkce: `wav_recovery_status_string()`.

### mzf2tmz v1.1.0
- Pridana volba `--fsk-speed 0-6` pro vyber rychlostni urovne FSK formatu.
- Pridana volba `--slow-speed 0-4` pro vyber rychlostni urovne SLOW formatu.
- `--speed` (pomer) se nyni pro FSK a SLOW formaty odmitne s chybovym hlasenim
  a napovedi na spravnou volbu.
- `--fsk-speed` a `--slow-speed` se odmitnou pro ostatni formaty.
- Hodnoty rychlosti se do bloku 0x41 ukladaji jako nativni format-specificke
  urovne (drive se ukladal genericke CMTSPEED index, ktery neodpovidal
  skutecnym FSK/SLOW rychlostnim urovnim).

### tmzedit v1.1.0
- Prikaz `set`: pridany volby `--fsk-speed 0-6` a `--slow-speed 0-4`
  se stejnymi validacnimi pravidly jako v mzf2tmz.

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
