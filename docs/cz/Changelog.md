# Changelog

## 2026-08-15

### tmzedit v1.6.0, extract_preloader
- Nová volba `--dump-charset <mode>` pro hex dumpy (tmzedit dump,
  extract_preloader). Režimy: raw (výchozí, standardní ASCII), eu, jp,
  utf8-eu, utf8-jp (interpretace bajtů jako Sharp MZ ASCII EU/JP s výstupem
  v ASCII nebo UTF-8). Nekonvertovatelné nebo netisknutelné bajty se
  zobrazí jako '.'.

### mzf_tools v2.2.0
- Nové API pro hex dump se znakovou sadou: en_MZF_DUMP_CHARSET,
  mzf_tools_parse_dump_charset(), mzf_tools_dump_char(), mzf_tools_hex_dump().

### sharpmz_ascii v2.2.0
- Upgrade knihovny na verzi 2.2.0 ze samostatného repozitáře sharpmz_ascii.
  Přidává znakové sady KOI8-CS (sharpmz_koi8cs_*) a display kódy
  (sharpmz_display_*), enum sharpmz_charset_t rozšířen o KOI8CS,
  DISPLAY_EU a DISPLAY_JP. Chování EU/JP se nemění.

### wav v2.0.0, mzf v2.0.0
- Chybové hlášky knihoven wav a mzf a dump MZF hlavičky převedeny do
  angličtiny (sjednoceno s emulátorem mz800new, projektová konvence).

## 2026-04-18

### sharpmz_ascii v2.1.0
- Upgrade knihovny na verzi 2.1.0 ze samostatného repozitáře sharpmz_ascii.
  Sloučení sharpmz_ascii a sharpmz_utf8 do jednoho modulu.
- Nové funkce: sharpmz_convert_to_ASCII(), sharpmz_eu_convert_to_UTF8(),
  sharpmz_eu_convert_UTF8_to(), sharpmz_jp_cnv_from(), sharpmz_jp_cnv_to(),
  sharpmz_jp_convert_to_ASCII(), sharpmz_jp_convert_to_UTF8(),
  sharpmz_jp_convert_UTF8_to().
- Dispatch wrappery: sharpmz_to_utf8(), sharpmz_from_utf8(),
  sharpmz_str_to_utf8(), sharpmz_str_from_utf8() s parametrem sharpmz_charset_t.
- Přidáno mapování znaků '^' (0x8B) a '`' (0x93) v EU variantě.

### tmzinfo v1.4.0, tmzedit v1.5.0, wav2tmz v2.7.0, tmz2mzf v1.2.0, mzf2tmz v1.3.0, bsd2dat v1.1.0
- Nová volba `--charset <mode>` nahradí původní `--name-encoding`.
  Režimy: eu (výchozí), jp, utf8-eu, utf8-jp.
- Nový režim `jp` pro japonskou znakovou sadu Sharp MZ (Sharp MZ-JP -> ASCII).
  Konvertuje pouze znaky 0x20-0x5D, katakana a kanji se nahradí mezerou.
- Režim `eu` (dříve `ascii`) konvertuje evropskou znakovou sadu včetně
  malých písmen. Režimy `utf8-eu` a `utf8-jp` zobrazí maximum znaků
  včetně přehlásek, šipek, katakany a kanji.

### mzf_tools v2.1.0
- Funkce mzf_tools_get_fname_ex() nyní podporuje MZF_NAME_ASCII_JP
  (japonská znaková sada -> ASCII).
- Funkce mzf_tools_get_fname() je nyní wrapper nad mzf_tools_get_fname_ex().

## 2026-04-09

### tmz2mzf v1.1.0
- Nová volba `--overwrite` pro povolení přepisu existujících výstupních souborů.
  Ve výchozím režimu program nyní odmítne přepsat existující soubor
  a navrhne použití `--overwrite` nebo `--append`.
- Nová volba `--append` pro připojení extrahovaných MZF bloků na konec
  existujícího souboru. Vytvoří multi-MZF (MZT-style) soubor se zřetězenými
  header+body záznamy. Pokud soubor neexistuje, vytvoří nový.
  Při extrakci více bloků jdou všechny bloky do jednoho výstupního souboru.

### tmz2wav v1.1.0
- Nová volba `--blocks <spec>` pro výběr konkrétních bloků k exportu.
  Formát: čísla a rozsahy oddělené čárkou (např. "0", "0,2", "0-2,5").
  Řídicí bloky (smyčky, skoky, volání) se zpracovávají vždy.
- Nová volba `--append` pro připojení nového audio signálu na konec
  existujícího WAV souboru. Validace: sample rate musí odpovídat,
  soubor musí být mono PCM. Pokud soubor neexistuje, ohlásí chybu.

### wav_analyzer v1.6.0
- Nová pole `start_time_sec` a `duration_sec` ve výsledcích - absolutní
  časy začátku a délky bloku ve WAV souboru. Vypočteny z indexů pulzů
  před uvolněním pulzní sekvence.
- Funkce `wav_analyzer_print_summary()` má nový parametr `verbose`.
  Ve verbose režimu vypisuje reálnou rychlost (Bd), přibližnou rychlost
  (nejbližší CMTSPEED) a pulzní sadu (MZ-800/MZ-80B) pro každý blok.
- Nová závislost na knihovně cmtspeed (pro výpočet přibližné rychlosti).

### wav2tmz v2.6.0
- Implicitně se provádí pouze analýza (bez ukládání). Ukládání se
  aktivuje volbou `-o` nebo `--output-format`.
- Souhrnný výpis vždy obsahuje absolutní časy začátku a délky bloku.
- Nová volba `--append-tmz` - připojit bloky do existujícího TMZ souboru.
  Bez této volby je existence TMZ souboru chyba.
- Nová volba `--overwrite-mzf` - přepsat existující MZF soubory.
  Bez této volby je existence MZF souboru chyba.
- Ve verbose režimu (--verbose) se vypisují: reálná rychlost (Bd),
  přibližná rychlost, pulzní sada a naměřené délky pulzů.

### wav_analyzer v1.5.0
- Nová pole `short_pulse_us` a `long_pulse_us` ve výsledcích - naměřené
  délky pulzů z histogramových peaků (FM signály se 2+ peaky).
  Umožňuje přesné zachování délek pulzů v TMZ blocích.

### mztape v2.1.0
- Nová funkce `mztape_create_stream_from_mztapemzf_ex()` - rozšířená verze
  s volitelnými custom délkami pulzů (us*100 pole). Nenulové hodnoty přepíší
  výchozí výpočet z pulseset+speed. Shodná konverze s mzcmt_turbo
  (seconds = value / 1e7).

### tmz_player v1.1.0
- Přehrávání NORMAL formátu (blok 0x41) nyní používá `_ex` variantu
  pro předání custom pulse polí (`long_high/low`, `short_high/low`) z bloku.
  Dříve se tato pole pro NORMAL formát ignorovala.

### wav2tmz v2.5.0
- Nová volba `--pulse-mode <approximate|exact>` (výchozí: approximate).
  V `exact` režimu NORMAL/MZ-80B soubory vždy použijí blok 0x41 s custom
  pulse fields z histogramové analýzy (speed=0, pulse fields != 0).
  V `approximate` režimu je chování nezměněno (kvantizace na CMTSPEED).

### tmzinfo v1.3.0
- Zobrazení custom pulsesetu pro blok 0x41: když jsou pulse pole nenulová,
  zobrazuje "Pulseset: custom" s Long/Short hodnotami v mikrosekundách
  a odhadovanou baudovou rychlostí místo standardního názvu pulsesetu a poměru.

### tmzedit v1.4.0
- Nová volba `--pulse <long_h/long_l,short_h/short_l>` pro příkaz `set`.
  Nastaví custom pulse režim (speed=0, pulse pole vyplněna) na bloku 0x41.
  Hodnoty jsou v us*100 jednotkách (např. `--pulse 4980/4980,2490/2490` pro ~1339 Bd).
- `--speed` na bloku s custom pulzy nyní vynuluje pulse pole
  (přepnutí zpět na tabulkový režim).
- Konverze 0x41 -> 0x40 se zablokuje při přítomnosti custom pulse polí.

## 2026-04-08

### tmzinfo v1.2.0
- Zobrazení rychlosti v Bd pro TZX bloky 0x10 (Standard Speed Data)
  a 0x11 (Turbo Speed Data). Vzorec: Bd = 3500000 / (zero + one).

### tmzedit v1.3.0
- Nová volba `--sinclair-speed <Bd>` pro editaci časování bloku 0x11
  (Turbo Speed Data). Akceptuje přesně 4 hodnoty: 1381, 1772, 2074, 2487.
  Přepočítává všechny časovací parametry (pilot, sync, zero, one)
  se zachováním ZX poměru zero:one = 1:2.

### wav_analyzer v1.4.0
- Podpora SINCLAIR formátu (ZX Spectrum protokol na Sharp MZ).
  SINCLAIR používá ZX kódování (pilot -> sync -> data s 1:2 pulzním
  poměrem) při různých rychlostech. Intercopy nahrává v tomto režimu
  při 1381, 1772, 2074 a 2487 Bd.
- ZX fallback po selhání FM tapemark detekce: když FM dekodér nenajde
  tapemark, zkusí ZX Spectrum dekodér. Při úspěchu klasifikuje výsledek
  jako SINCLAIR. Bezpečné pro všechny FM formáty (u NORMAL FM ZX sync
  detekce spolehlivě selže, protože nemá dostatečně krátké pulzy).
- Nový test `test_ic_sinclair_all` - ověření 4 kopií Turbo Copy V1.21
  nahraných Intercopy v SINCLAIR režimu (1381-2487 Bd).

### wav2tmz v2.4.0
- SINCLAIR bloky se ukládají jako TZX blok 0x11 (Turbo Speed Data)
  s časováním škálovaným z průměrné půlperiody leader tónu vůči
  standardnímu ZX pilotu (2168 T-states = 619.4 us). Zachovává
  rychlost přes TMZ round-trip.
- Oprava segfault při konverzi SINCLAIR nahrávek: SINCLAIR výsledky
  mají tap_data (ne mzf), ale dříve se směrovaly na create_block_turbo,
  která přistupovala k mzf (NULL pointer dereference).
- Zobrazení rychlosti SINCLAIR bloků v souhrnném výpisu
  (např. "SINCLAIR flag=0xFF, speed=1.30x (476 us)").

### wav_analyzer v1.3.0
- Podpora dekódování Intercopy FAST IPL nahrávek ve všech rychlostech
  (1200, 2400, 2800, 3200 Bd). FASTIPL je standardní dvoublokový MZF:
  header blok (LGAP 22000 p.p. + LTM + $BB hlavička) při NORMAL rychlosti,
  body blok (LGAP 11000 p.p. + STM + body data) při TURBO rychlosti.
- FASTIPL dekodér založený na analýze Intercopy 10.2 write handleru
  (sub_19EE): body LGAP má 5500 pulzů (polovina header LGAP), body
  tapemark je STM (20+20, ne LTM 40+40). Snížený FM threshold faktor
  1.4x (vs globální 1.6x) pro spolehlivou detekci tapemarku při 2800+ Bd.
- FASTIPL body leader vracen přes nový parametr `out_data_leader`
  pro správnou rychlostní třídu a leader info ve výsledcích.
- Nové pole `header_leader_pulse` ve výsledcích pro správný výpočet
  pokrytí u dvoudílných formátů (TURBO/FASTIPL).

### wav2tmz v2.3.0
- Odhad rychlosti FASTIPL z body LGAP leader avg půlperiody.
- Chronologické prokládání dekódovaných souborů a raw bloků (Direct
  Recording) ve výstupu TMZ. Dříve se všechny soubory zapsaly první
  a raw bloky až na konec.
- Pokrytí raw bloků používá `header_leader_pulse` místo
  `leader.start_index`, aby nezahrnovalo header část záznamu.

### mzf2tmz v1.2.0
- `--speed` nyní přijímá i hodnoty baudrate (např. `--speed 2800`)
  kromě poměrů (`--speed 7:3`). Baudrate se mapuje na nejbližší CMTSPEED.
- FASTIPL rychlost zobrazena jako baudrate (např. "2800 Bd") místo poměru.

### tmzedit v1.2.0
- `--speed` nyní přijímá i hodnoty baudrate (např. `--speed 2800`)
  kromě poměrů (`--speed 7:3`). Baudrate se mapuje na nejbližší CMTSPEED.
- FASTIPL rychlost zobrazena jako baudrate v příkazech `convert` a `set`.

### tmzinfo v1.1.0
- FASTIPL rychlost zobrazena jako baudrate (např. "2800 Bd") místo
  formátu poměru ("7:3 - 2800 Bd"), shodně s konvencí Intercopy.

### cmtspeed v2.0.0
- Nová funkce: `cmtspeed_from_bdspeed()` - mapuje baudrate na nejbližší
  hodnotu `en_CMTSPEED`.

### mzcmt_fastipl v1.2.0
- Oprava signálové struktury podle analýzy Intercopy 10.2 (sub_19EE):
  - Header LGAP: 11000 pulzů (dříve 22000 - dvojnásobek oproti Intercopy).
  - Body LGAP: 5500 pulzů (dříve 22000, Intercopy: HL=$157C=5500).
  - Body tapemark: STM 20+20 (dříve LTM 40+40, Intercopy píše STM pro body).
  - Odstraněna SGAP+STM+CRC(=0) sekce z header bloku (Intercopy ji nepíše).
  - Odstraněna pauza 1000 ms mezi bloky (Intercopy píše bez pauzy).
  - Loader binary rozšířen z 96 B na 110 B (pokrývá offsety $12-$7F,
    shodně s Intercopy sub_2035 layoutem).
- Readpoint: lookup tabulka z referenční nahrávky ic-loader-all.wav
  (1:1=77, 2:1=32, 7:3=22, 8:3=17). Předchozí vzorec floor(82/divisor)
  dával špatné hodnoty (82, 41, 35, 30) - Intercopy počítá readpoint
  přes sub_201C/sub_1E09, ne prostým dělením.
- Pulseset MZ-800: asymetrický (227/272 us SHORT, 476/499 us LONG)
  z referenční nahrávky. Při 44100 Hz: SHORT=10+12=22 smp,
  LONG=21+22=43 smp. Asymetrický pulseset je nutný pro správné
  škálování turbo rychlosti (2:1 dává 5+6=11 smp, ne sym. 5+5=10).
  Readpoint a pulseset musí být konzistentní (oba ze stejné reference).
- Ověřeno v emulátoru: ROM načte všechny 4 rychlosti (1200-3200 Bd),
  Intercopy statistiky odpovídají referenční nahrávce.

### mzcmt_bsd v1.1.0
- Pulseset MZ-800: symetrické 498/498 us LONG, 249/249 us SHORT
  (dříve asymetrické 470/494, 246/278 z Intercopy měření).
- Pulseset MZ-700: symetrické 504/504, 252/252 (dříve 464/494, 240/264).
- Konzistentní s mzcmt_turbo a mztape vstream pulsesety.

### mzcmt_turbo v2.0.1
- Oprava ROM delay: vzorec změněn z `round(82/speed_ratio)` na
  `floor(82/speed_ratio)` (truncation). TurboCopy na Z80 používá
  celočíselné dělení, které ořezává desetinnou část. Rounding
  způsoboval chybu u rychlosti 8:3 (delay 31 místo 30), což vedlo
  k ~2.5% odchylce v naměřené rychlosti (2625 Bd místo 2692 Bd).

### mztape v2.0.1
- Oprava MZ-800 vstream pulsesetu: přechod z asymetrických pulzů
  (Intercopy 10.2 měření: 245.8/278.2 us SHORT) na symetrické
  (249/249 us SHORT, 498/498 us LONG). Asymetrické hodnoty se
  při 44100 Hz zaokrouhlovaly na 11+12=23 vzorků místo 11+11=22,
  což způsobovalo ~4.5% odchylku v rychlosti (1099 Bd místo 1150 Bd).
  Symetrické hodnoty odpovídají ROM chování a jsou shodné
  s mzcmt_turbo g_pulses_800.

## 2026-04-07

### wav_analyzer v1.2.0
- Podpora dekódování TurboCopy TURBO nahrávek (rychlosti 2:1, 7:3, 8:3, 3:1 atd.).
  TurboCopy TURBO preloader (fsize=90, fstrt=$D400) patchne ROM a použije
  standardní CMT read rutinu pro čtení těla v TURBO rychlosti. Dekodér
  extrahuje metadata (fsize/fstrt/fexec) z preloader body a dekóduje
  TURBO data jako standardní FM (body-only, STM tapemark).
- Podpora mzftools TURBO formátu (fsize=0, loader v comment). Metadata
  se extrahují z comment oblasti hlavičky (cmnt[1..6]).
- Nová funkce: `wav_decode_turbo_turbocopy_mzf()` - TurboCopy body-only dekódování.
- Nová funkce: `wav_decode_turbo_mzftools_mzf()` - mzftools body-only dekódování.
- Oprava: TURBO dispatch v `process_leader()` - chybějící `else` mezi TurboCopy
  a mzftools TURBO cestami způsoboval zahození výsledku dekódování.
- Oprava: leader skip podmínka `leader_end < skip_until_pulse` místo
  `start_index < skip_until_pulse` - leader na hranici consumed oblasti
  se chybně přeskakoval (round-trip 3/5 -> 5/5).
- Vylepšení: TURBO dekodér používá `wav_leader_detect()` pro nalezení
  skutečného TURBO leaderu místo syntetického leaderu s pulse_count=0.

### mzcmt_turbo v2.0.0
- Kompletní přepis tape encoderu na TurboCopy kompatibilní formát.
  Preloader nyní generuje fsize=90, fstrt=$D400 (místo mzftools fsize=0, $1110).
  TurboCopy formát funguje na reálném HW, v emulátoru i v TurboCopy/Intercopy.
- Embedded TurboCopy loader (75 B generický kód z reverse eng. TurboCopy V1.21)
  s patchovatelnou datovou částí (speed_val, fsize/fstrt/fexec, ROM params).
- Preloader hlavička: TurboCopy identifikační signatura v cmnt[0..6],
  originální comment data v cmnt[7..103].
- TURBO datová sekce: body-only formát (STM tapemark + body + CRC).
  Odpovídá struktuře reálné TurboCopy nahrávky.
- ROM delay: vzorec `round(82/speed_ratio)` odvozený z měření reálných
  TurboCopy nahrávek. Nahrazuje chybnou lookup tabulku z mzftools.
- Pulseset MZ-800: symetrické pulzy (249/249 us SHORT, 498/498 us LONG).
  ROM generuje symetrické pulzy; původní asym. hodnoty (246/278, 470/494)
  způsobovaly špatné zaokrouhlování na 44100 Hz.
- Pulseset MZ-700: analogicky symetrické (252/252, 504/504).

### wav2tmz v2.2.0
- Odhad rychlosti TURBO souborů z leader avg půlperiody
  (dříve jen pro NORMAL/MZ-80B, nyní i pro TURBO formát).
- Přesnější rychlost pro TurboCopy ze speed_val bajtu preloaderu ($4B).

### Nová utilita: extract_preloader
- Extrakce TurboCopy TURBO preloader binárky (90 B) z WAV nahrávky.

### Nová testovací data
- `tstdata/tc-loader-all.wav` - 5 kopií Turbo Copy V1.21 s TURBO loaderem
  (rychlosti 1:1, 2:1, 7:3, 8:3, 3:1).
- `tstdata/mzf/Turbo_Copy_V1.21.mzf` - referenční MZF pro verifikaci.
- `tests/test_tc_loader_all.c` - integrační test (5 kopií, body match, CRC OK).

### Atribuce
- Opravena atribuce: TURBO = TurboCopy (Michal Kreidl),
  FAST IPL = Intercopy (Marek Šmihla - NIPSOFT).

## 2026-04-06

### wav2tmz v2.1.0
- Obnova částečných BSD dat: nekompletní BSD soubory (chybějící ukončovací
  chunk ID=0xFFFF) lze nyní zachránit pomocí volby `--recover-bsd`.
- Diagnostika je vždy zapnuta - při selhání BSD dekódování se vypíše
  název souboru, typ chyby a nápověda na `--recover-bsd`.
- Obnovené soubory jsou ve výstupu označeny `[RECOVERED]`
  a v TMZ archivu jsou doplněny blokem Text Description (0x30) s varováním.
- Nové volby: `--recover` (všechny recovery módy),
  `--recover-bsd`, `--recover-body`, `--recover-header`
  (poslední dvě připraveny pro fázi 2, zatím bez efektu).

### wav_analyzer v1.1.0
- BSD dekodér: podpora obnovy částečných dat (allow_partial).
- BSD dekodér: validace sekvenčních chunk ID - zamezuje čtení dat
  z následujících souborů na pásce jako falešných BSD chunků.
- BSD dekodér: oprava consumed_until - při chybějícím terminátoru
  se používá pozice za posledním úspěšně přečteným chunkem.
- Nové datové typy: `en_WAV_RECOVERY_STATUS`, recovery pole v konfiguraci
  a ve výsledku analýzy.
- Nová funkce: `wav_recovery_status_string()`.

### mzf2tmz v1.1.0
- Přidána volba `--fsk-speed 0-6` pro výběr rychlostní úrovně FSK formátu.
- Přidána volba `--slow-speed 0-4` pro výběr rychlostní úrovně SLOW formátu.
- `--speed` (poměr) se nyní pro FSK a SLOW formáty odmítne s chybovým hlášením
  a nápovědou na správnou volbu.
- `--fsk-speed` a `--slow-speed` se odmítnou pro ostatní formáty.
- Hodnoty rychlosti se do bloku 0x41 ukládají jako nativní formátově specifické
  úrovně (dříve se ukládal generický CMTSPEED index, který neodpovídal
  skutečným FSK/SLOW rychlostním úrovním).

### tmzedit v1.1.0
- Příkaz `set`: přidány volby `--fsk-speed 0-6` a `--slow-speed 0-4`
  se stejnými validačními pravidly jako v mzf2tmz.

### bsd2dat v1.0.1
- Oprava: export dat nyní zahrnuje i data terminačního chunku (ID=0xFFFF),
  což odpovídá chování BSD dekodéru a reálného MZ-800 BASIC.

### dat2bsd v1.0.1
- Oprava: poslední datový chunk se nyní označí jako terminační (ID=0xFFFF)
  a nese poslední porci dat. Dříve se přidával extra terminační chunk
  s nulovými daty, což způsobovalo nekonzistenci při WAV round-tripu.

### wav2tmz v2.0.1
- Oprava: při vytváření TMZ bloku 0x45 (MZ BASIC Data) z dekódovaného
  BSD záznamu se nyní poslední chunk označí jako terminační (ID=0xFFFF)
  a nese data. Sjednocení logiky s dat2bsd a BSD dekodérem.

### Všechny knihovny
- Přidána funkce *_version() vracející řetězec s verzí knihovny.

### Všechny nástroje
- Přidána volba --version pro zobrazení verze programu.
- Přidána volba --lib-versions pro zobrazení verzí použitých knihoven.

## 2026-04-05

### Verze 1.0.0 - První vydání
- Knihovny: tmz, tmz_blocks, tmz_player, tzx, mzf, mzf_tools, wav,
  cmt_stream (bitstream, vstream), cmtspeed, endianity, sharpmz_ascii,
  sharpmz_utf8, mztape, zxtape, generic_driver, memory_driver
- Kodéry: mzcmt_bsd, mzcmt_cpmtape, mzcmt_direct, mzcmt_fastipl,
  mzcmt_fsk, mzcmt_slow, mzcmt_turbo
- WAV analyzer: wav_preprocess, wav_pulse, wav_leader, wav_histogram,
  wav_classify, wav_decode_fm, wav_decode_bsd, wav_decode_cpmtape,
  wav_decode_direct, wav_decode_fastipl, wav_decode_fsk, wav_decode_slow,
  wav_decode_turbo, wav_decode_zx
- Nástroje: tmzinfo, mzf2tmz, tmz2mzf, tmz2wav, wav2tmz, tap2tmz,
  tmz2tap, tmzconv, tmzedit, dat2bsd, bsd2dat
