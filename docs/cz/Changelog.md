# Changelog

## 2026-04-08

### tmzinfo v1.2.0
- Zobrazeni rychlosti v Bd pro TZX bloky 0x10 (Standard Speed Data)
  a 0x11 (Turbo Speed Data). Vzorec: Bd = 3500000 / (zero + one).

### tmzedit v1.3.0
- Nova volba `--sinclair-speed <Bd>` pro editaci casovani bloku 0x11
  (Turbo Speed Data). Akceptuje presne 4 hodnoty: 1381, 1772, 2074, 2487.
  Prepocitava vsechny casovaci parametry (pilot, sync, zero, one)
  se zachovanim ZX pomeru zero:one = 1:2.

### wav_analyzer v1.4.0
- Podpora SINCLAIR formatu (ZX Spectrum protokol na Sharp MZ).
  SINCLAIR pouziva ZX kodovani (pilot -> sync -> data s 1:2 pulsnim
  pomerem) pri ruznych rychlostech. Intercopy nahraba v tomto rezimu
  pri 1381, 1772, 2074 a 2487 Bd.
- ZX fallback po selhani FM tapemark detekce: kdyz FM dekoder nenajde
  tapemark, zkusi ZX Spectrum dekoder. Pri uspechu klasifikuje vysledek
  jako SINCLAIR. Bezpecne pro vsechny FM formaty (u NORMAL FM ZX sync
  detekce spolehive selze, protoze nema dostatecne kratke pulzy).
- Novy test `test_ic_sinclair_all` - overeni 4 kopii Turbo Copy V1.21
  nahrane Intercopy v SINCLAIR rezimu (1381-2487 Bd).

### wav2tmz v2.4.0
- SINCLAIR bloky se ukladaji jako TZX blok 0x11 (Turbo Speed Data)
  s casovanim skalovaym z prumerne pul-periody leader tonu vuci
  standardnimu ZX pilotu (2168 T-states = 619.4 us). Zachovava
  rychlost pres TMZ round-trip.
- Oprava segfault pri konverzi SINCLAIR nahravek: SINCLAIR vysledky
  maji tap_data (ne mzf), ale drive se smerovaly na create_block_turbo
  ktera pristupovala k mzf (NULL pointer dereference).
- Zobrazeni rychlosti SINCLAIR bloku v sumarnim vypisu
  (napr. "SINCLAIR flag=0xFF, speed=1.30x (476 us)").

### wav_analyzer v1.3.0
- Podpora dekodovani Intercopy FAST IPL nahravek ve vsech rychlostech
  (1200, 2400, 2800, 3200 Bd). FASTIPL je standardni dvou-blokovy MZF:
  header blok (LGAP 22000 p.p. + LTM + $BB hlavicka) pri NORMAL rychlosti,
  body blok (LGAP 11000 p.p. + STM + body data) pri TURBO rychlosti.
- FASTIPL dekoder zalozeny na analyze Intercopy 10.2 write handleru
  (sub_19EE): body LGAP ma 5500 pulzu (polovina header LGAP), body
  tapemark je STM (20+20, ne LTM 40+40). Snizeny FM threshold faktor
  1.4x (vs globalni 1.6x) pro spolehlivou detekci tapemarku pri 2800+ Bd.
- FASTIPL body leader vracen pres novy parametr `out_data_leader`
  pro spravnou rychlostni tridu a leader info ve vysledcich.
- Nove pole `header_leader_pulse` ve vysledcich pro spravny vypocet
  pokryti u dvoudilnych formatu (TURBO/FASTIPL).

### wav2tmz v2.3.0
- Odhad rychlosti FASTIPL z body LGAP leader avg pul-periody.
- Chronologicke prokladani dekodovanych souboru a raw bloku (Direct
  Recording) ve vystupu TMZ. Drive se vsechny soubory zapsaly prvni
  a raw bloky az na konec.
- Pokryti raw bloku pouziva `header_leader_pulse` misto
  `leader.start_index` aby nezahrnovalo header cast zaznamu.

### mzf2tmz v1.2.0
- `--speed` nyne prijima i hodnoty baudrate (napr. `--speed 2800`)
  krome pomeru (`--speed 7:3`). Baudrate se mapuje na nejblizsi CMTSPEED.
- FASTIPL rychlost zobrazena jako baudrate (napr. "2800 Bd") misto pomeru.

### tmzedit v1.2.0
- `--speed` nyne prijima i hodnoty baudrate (napr. `--speed 2800`)
  krome pomeru (`--speed 7:3`). Baudrate se mapuje na nejblizsi CMTSPEED.
- FASTIPL rychlost zobrazena jako baudrate v prikazech `convert` a `set`.

### tmzinfo v1.1.0
- FASTIPL rychlost zobrazena jako baudrate (napr. "2800 Bd") misto
  formatu pomeru ("7:3 - 2800 Bd"), shodne s konvenci Intercopy.

### cmtspeed v2.0.0
- Nova funkce: `cmtspeed_from_bdspeed()` - mapuje baudrate na nejblizsi
  hodnotu `en_CMTSPEED`.

### mzcmt_fastipl v1.2.0
- Oprava signalove struktury podle analyzy Intercopy 10.2 (sub_19EE):
  - Header LGAP: 11000 pulzu (drive 22000 - dvojnasobek oproti Intercopy).
  - Body LGAP: 5500 pulzu (drive 22000, Intercopy: HL=$157C=5500).
  - Body tapemark: STM 20+20 (drive LTM 40+40, Intercopy pise STM pro body).
  - Odstranena SGAP+STM+CRC(=0) sekce z header bloku (Intercopy ji nepise).
  - Odstranena pauza 1000 ms mezi bloky (Intercopy pise bez pauzy).
  - Loader binary rozsiren z 96B na 110B (pokryva offsety $12-$7F,
    shodne s Intercopy sub_2035 layoutem).
- Readpoint: lookup tabulka z referencni nahravky ic-loader-all.wav
  (1:1=77, 2:1=32, 7:3=22, 8:3=17). Predchozi vzorec floor(82/divisor)
  daval spatne hodnoty (82, 41, 35, 30) - Intercopy pocita readpoint
  pres sub_201C/sub_1E09, ne prostym delenim.
- Pulseset MZ-800: asymetricky (227/272 us SHORT, 476/499 us LONG)
  z referencni nahravky. Pri 44100 Hz: SHORT=10+12=22 smp,
  LONG=21+22=43 smp. Asymetricky pulseset je nutny pro spravne
  skalovani turbo rychlosti (2:1 dava 5+6=11 smp, ne sym. 5+5=10).
  Readpoint a pulseset musi byt konzistentni (oba ze stejne reference).
- Overeno v emulatoru: ROM nacte vsechny 4 rychlosti (1200-3200 Bd),
  Intercopy statistiky odpovidaji referencni nahravce.

### mzcmt_bsd v1.1.0
- Pulseset MZ-800: symetricke 498/498 us LONG, 249/249 us SHORT
  (drive asymetricke 470/494, 246/278 z Intercopy mereni).
- Pulseset MZ-700: symetricke 504/504, 252/252 (drive 464/494, 240/264).
- Konzistentni s mzcmt_turbo a mztape vstream pulsesety.

### mzcmt_turbo v2.0.1
- Oprava ROM delay: vzorec zmenen z `round(82/speed_ratio)` na
  `floor(82/speed_ratio)` (truncation). TurboCopy na Z80 pouziva
  celociselne deleni, ktere orezava desetinnou cast. Rounding
  zpusoboval chybu u rychlosti 8:3 (delay 31 misto 30), coz vedlo
  k ~2.5% odchylce v namerenene rychlosti (2625 Bd misto 2692 Bd).

### mztape v2.0.1
- Oprava MZ-800 vstream pulsesetu: prechod z asymetrickych pulzu
  (Intercopy 10.2 mereni: 245.8/278.2 us SHORT) na symetricke
  (249/249 us SHORT, 498/498 us LONG). Asymetricke hodnoty se
  pri 44100 Hz zaokrouhlovaly na 11+12=23 vzorku misto 11+11=22,
  coz zpusobovalo ~4.5% odchylku v rychlosti (1099 Bd misto 1150 Bd).
  Symetricke hodnoty odpovidaji ROM chovani a jsou shodne
  s mzcmt_turbo g_pulses_800.

## 2026-04-07

### wav_analyzer v1.2.0
- Podpora dekodovani TurboCopy TURBO nahravek (rychlosti 2:1, 7:3, 8:3, 3:1 atd.).
  TurboCopy TURBO preloader (fsize=90, fstrt=$D400) patchne ROM a pouzije
  standardni CMT read rutinu pro cteni tela v TURBO rychlosti. Dekoder
  extrahuje metadata (fsize/fstrt/fexec) z preloader body a dekoduje
  TURBO data jako standardni FM (body-only, STM tapemark).
- Podpora mzftools TURBO formatu (fsize=0, loader v comment). Metadata
  se extrahuje z comment oblasti hlavicky (cmnt[1..6]).
- Nova funkce: `wav_decode_turbo_turbocopy_mzf()` - TurboCopy body-only dekodovani.
- Nova funkce: `wav_decode_turbo_mzftools_mzf()` - mzftools body-only dekodovani.
- Oprava: TURBO dispatch v `process_leader()` - chybejici `else` mezi TurboCopy
  a mzftools TURBO cestami zpusoboval zahozeni vysledku dekodovani.
- Oprava: leader skip podminka `leader_end < skip_until_pulse` misto
  `start_index < skip_until_pulse` - leader na hranici consumed oblasti
  se chybne preskakoval (round-trip 3/5 -> 5/5).
- Vylepseni: TURBO dekoder pouziva `wav_leader_detect()` pro nalezeni
  skutecneho TURBO leaderu misto syntetickeho leaderu s pulse_count=0.

### mzcmt_turbo v2.0.0
- Kompletni prepis tape encoderu na TurboCopy kompatibilni format.
  Preloader nyni generuje fsize=90, fstrt=$D400 (misto mzftools fsize=0, $1110).
  TurboCopy format funguje na realnem HW, v emulatoru i v TurboCopy/Intercopy.
- Embedded TurboCopy loader (75B genericky kod z reverse eng. TurboCopy V1.21)
  s patchovatelnou datovou casti (speed_val, fsize/fstrt/fexec, ROM params).
- Preloader hlavicka: TurboCopy identifikacni signatura v cmnt[0..6],
  originalni comment data v cmnt[7..103].
- TURBO datova sekce: body-only format (STM tapemark + body + CRC).
  Odpovidajici strukture realne TurboCopy nahravky.
- ROM delay: vzorec `round(82/speed_ratio)` odvozeny z mereni realnych
  TurboCopy nahravek. Nahrazuje chybnou lookup tabulku z mzftools.
- Pulseset MZ-800: symetricke pulzy (249/249 us SHORT, 498/498 us LONG).
  ROM generuje symetricke pulzy; puvodni asym. hodnoty (246/278, 470/494)
  zpusobovaly spatne zaokrouhlovani na 44100 Hz.
- Pulseset MZ-700: analogicky symetricke (252/252, 504/504).

### wav2tmz v2.2.0
- Odhad rychlosti TURBO souboru z leader avg pul-periody
  (drive jen pro NORMAL/MZ-80B, nyni i pro TURBO format).
- Presnejsi rychlost pro TurboCopy z speed_val bajtu preloaderu ($4B).

### Nova utilita: extract_preloader
- Extrakce TurboCopy TURBO preloader binarky (90B) z WAV nahravky.

### Nova testovaci data
- `tstdata/tc-loader-all.wav` - 5 kopii Turbo Copy V1.21 s TURBO loaderem
  (rychlosti 1:1, 2:1, 7:3, 8:3, 3:1).
- `tstdata/mzf/Turbo_Copy_V1.21.mzf` - referencni MZF pro verifikaci.
- `tests/test_tc_loader_all.c` - integracni test (5 kopii, body match, CRC OK).

### Atribuce
- Opravena atribuce: TURBO = TurboCopy (Michal Kreidl),
  FAST IPL = Intercopy (Marek Smihla - NIPSOFT).

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
