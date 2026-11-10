# TapeMZ - kazetový archivní formát pro Sharp MZ

[![CI](https://github.com/michalhucik/TapeMZ/actions/workflows/ci.yml/badge.svg)](https://github.com/michalhucik/TapeMZ/actions/workflows/ci.yml)

## 1. Úvod

TapeMZ (TMZ) je nový souborový formát pro uchovávání obsahu magnetofonových kazet počítačů Sharp MZ (MZ-700, MZ-800, MZ-1500, MZ-80B) a ZX Spectrum. Vznikl jako odpověď na dlouhodobou absenci jednotného formátu, který by dokázal věrně zachytit kompletní obsah kazety - včetně nestandardních rychlostí, turbo loaderů, proprietárních kódování a víceblokových programů.

Formát TMZ vychází z TZX v1.20 (ZX Spectrum tape image format) a rozšiřuje ho o bloky specifické pro platformu Sharp MZ. Díky tomu je plně kompatibilní s existujícím TZX ekosystémem a zároveň přináší nativní podporu pro všechny známé kazetové formáty Sharp MZ.

Součástí projektu je sada C knihoven pro práci s kazetovými formáty, WAV analyzér schopný automaticky dekódovat nahrávky kazet (WAV) a jedenáct CLI nástrojů pro konverze, editaci a analýzu.

## 2. Motivace

### 2.1 Proč nový formát?

Existující formáty pro Sharp MZ mají zásadní omezení:

- **MZF/MZT** - uchovávají pouze čistá data (128B hlavička + tělo). Nenesou žádnou informaci o časování signálu, nestandardních formátech, turbo loaderech ani metadatech. Soubor MZF s ftype=0x03 (BSD) a fsize=0 ztratí veškerá BASIC data, protože chunkovací protokol není součástí formátu.

- **WAV** - zachovává přesný analogový signál, ale produkuje obrovské soubory (řádově desítky MB na program), neumožňuje strukturovanou editaci a je závislý na kvalitě záznamu.

Žádný z nich nedokáže v jednom souboru uchovat celou kazetu s programy v různých formátech a rychlostech, s metadaty a s možností bezztrátové editace.

### 2.2 Proč TZX jako základ?

Formát TZX (ZX Spectrum tape image) je de facto standard pro archivaci kazetových nahrávek v retro computing komunitě. Jeho bloková struktura je modulární, rozšiřitelná a obsahuje mechanismy pro:
- Standardní i nestandardní datové bloky
- Přímý záznam signálu (Direct Recording)
- Metadata (Text Description, Archive Info)
- Logické seskupování (Group Start/End)
- Pauzy a řídící bloky

TZX v1.20 rozsah ID 0x40-0x4F nedefinuje (ID 0x40 byl zastaralý blok Snapshot, ve v1.20 odstraněný) a dává obecné pravidlo, že bloky přidané po v1.10 nesou za ID 4bajtovou délku. TMZ tento rozsah využívá pro Sharp MZ specifické bloky; odlišení od případných budoucích bloků TZX zajišťuje hlavička "TapeMZ!".

Kompatibilita s TZX znamená, že:
- TMZ reader umí přehrát i čisté TZX soubory (ZX Spectrum)
- TZX reader automaticky přeskočí neznámé MZ bloky (díky uniformní délkové konvenci)
- Jeden soubor může obsahovat jak ZX Spectrum, tak Sharp MZ nahrávky
- Programy používající CMT extenze v různých formátech (ZX, CP/M Tape, Sharp MZ) mohou koexistovat na jedné virtuální pásce

### 2.3 Absence analytických nástrojů

Dalším zásadním důvodem pro vznik tohoto projektu byla naprostá absence analytických programů pro konverzi WAV nahrávek z kazet Sharp MZ do strukturovaných formátů. Zatímco ZX Spectrum komunita disponuje řadou nástrojů (audio2tape, tzxwav, wav2pzx, maketzx), pro Sharp MZ existovaly jen soliterní, či proprietální nástroje a neexistoval žádný srovnatelný program schopný:

- Automaticky detekovat formát nahrávky (NORMAL, TURBO, FASTIPL, FSK, SLOW, DIRECT, BSD, CP/M Tape, ZX Spectrum)
- Dekódovat data z WAV signálu s kontrolou checksumů
- Exportovat výsledek do strukturovaného formátu

Existující nástroje jako mzftools (mz-fuzzy) nebo cmttool podporovaly pouze základní NORMAL formát. Intercopy 10.2 (Sharp MZ nativní kopírka) sice obsahovala sofistikovanou autodetekci, ale běžela pouze na reálném hardware.

## 3. Kazetové formáty Sharp MZ

Počítače Sharp MZ ukládají data na magnetofonovou pásku pomocí několika různých kódovacích metod. Všechny využívají zabudovaný kazetový interface řízený přes PPI 8255 (SENSE vstup pro čtení, MOTOR pro řízení motoru).

### 3.1 Kategorie nahrávání

Na platformě Sharp MZ existují 4 základní kategorie nahrávání programů:

#### Kategorie 1: Sharp standard (NORMAL 1200 Bd)

Standardní MZF záznam složený ze dvou bloků (header a body), oba v rychlosti 1200 Bd. Přečte ho standardní ROM monitor. Kódování: FM modulace (bit "1" = dlouhý pulz, bit "0" = krátký pulz), 8 bitů/bajt MSB first, 1 stop bit.

Struktura na pásce:
```
[Leader tone]  22000 krátkých pulzů (synchronizační tón)
[Tape mark]    40 dlouhých + 40 krátkých pulzů + 1 krátký
[Header]       128 bajtů (MZF hlavička)
[Checksum]     2 bajty (16-bit population count)
[Gap]          11000 krátkých pulzů
[Tape mark]    20 dlouhých + 20 krátkých
[Body]         N bajtů (data)
[Checksum]     2 bajty
```

Do této kategorie patří i chunkovací formát **BSD/BRD** (datový soubor BASICu), kde za hlavičkou následuje sekvence kusů po 258 B (2B ID + 256B data), ukončená kusem s ID=0xFFFF, který nese zbytek dat. BASIC 1Z-016 (`WOPEN#`, `PRINT#`) zapisuje před každý kus vlastní tón a tapemark, za něj checksum a kopii a mezi kusy zastaví motor. V TMZ jde o blok 0x46 s profilem `BASIC-1Z016` (časování změřené v emulátoru); starý blok 0x45 se přehraje stejně.

#### Kategorie 2: NORMAL FM při nestandardní rychlosti

Stejná struktura jako kategorie 1, ale při jiné rychlosti (2400, 2800, 3200+ Bd). Oba bloky (header i body) mají identickou rychlost. Přečte jen upravená ROM, kopírky (Intercopy) nebo speciální programy.

Do této kategorie patří i **CP/M CMT** (cmt.com pod CP/M, 2400 Bd) a **MZ-80B** (vlastní pulsní sada, 1800 Bd základ).

#### Kategorie 3: Loader v hlavičce (komentářová oblast)

Záznam vypadá jako kategorie 1, ale do komentářové oblasti MZF hlavičky byl vložen krátký program (loader). Standardní pole hlavičky jsou nastavena tak, aby ROM načetl pouze hlavičku a spustil loader: SIZE=0 (ROM přeskočí čtení těla), EXEC ukazuje do komentářové oblasti.

Na pásce je standardní dvou-blokový MZF záznam:
- Header blok v NORMAL 1:1 rychlosti (hlavička s embedded loaderem)
- Body blok v konfigurovatelné rychlosti (vlastní data programu)

ROM přečte pouze header blok, přeskočí body (SIZE=0) a spustí loader. Loader pak sám přečte body blok z pásky na cílové rychlosti.

Formáty v této kategorii:
- **FASTIPL** - $BB prefix, V02/V07 loader (Intercopy, Marek Šmihla - NIPSOFT). Skutečné parametry (fsize, fstrt, fexec) uloženy na offsetech $1A-$1F v hlavičce. Rychlost body bloku: 1200-3200 Bd.

#### Kategorie 4: Loader jako samostatný program

Preloader se skládá z hlavičky + body v NORMAL FM 1:1. ROM načte hlavičku i body a spustí loader. Loader patchne ROM rutiny a načte uživatelská data z pásky v cílovém formátu.

Formáty v této kategorii:
- **TURBO** (TurboCopy varianta) - 90B loader na $D400, fsize=90, fstrt/fexec=$D400, identifikace přes signaturu v cmnt[0..6] (TurboCopy, Michal Kreidl). Loader patchne ROM rychlost a volá standardní CMT read ($002A). TURBO data = body-only (STM tapemark + body + CRC, bez hlavičky).
- **FSK** (Frequency Shift Keying) - bit určen frekvencí cyklu, 7 rychlostních úrovní
- **SLOW** (kvarternální) - 2 bity na pulz (4 symboly), 5 rychlostních úrovní
- **DIRECT** (přímý bitový zápis) - 1 vzorek = 1 bit, rychlost daná sample ratem

#### Mimo kategorie: CP/M Tape

CP/M Tape (Pezik/MarVan, ZTAPE/TAPE.COM, a stejný formát v režimu CPM programu Intercopy) používá zcela odlišný protokol - manchesterské kódování, LSB first, bez stop bitů, pilot "011". Běží mimo Sharp ROM pod CP/M. TAPE.COM zapisuje 2400 nebo 3200 Bd (1200 Bd neexistuje), Intercopy 2400 Bd. V TMZ jde o blok 0x46 s profilem `PEZIK-2.0A` nebo `IC-10.2-CPM`.

### 3.2 Přehled formátů

| Formát    | Kategorie | Modulace     | Rychlost    | Loader  | TMZ blok |
|-----------|-----------|--------------|-------------|---------|----------|
| NORMAL    | 1         | FM           | 1200 Bd     | žádný   | 0x40     |
| NORMAL    | 2         | FM           | 2400+ Bd    | žádný   | 0x41     |
| CP/M CMT  | 2         | FM           | 2400 Bd     | žádný   | 0x41     |
| MZ-80B    | 2         | FM           | 1800 Bd     | žádný   | 0x41     |
| BSD/BRD   | 1         | FM (kusy)    | 1200 Bd     | žádný   | 0x46     |
| TURBO     | 4         | FM           | konfig.     | $D400   | 0x41     |
| FASTIPL   | 3         | FM + $BB     | konfig.     | comment | 0x41     |
| FSK       | 4         | FSK          | 7 úrovní    | program | 0x41     |
| SLOW      | 4         | kvarternální | 5 úrovní    | program | 0x41     |
| DIRECT    | 4         | bitový       | sample rate | program | 0x41     |
| CP/M Tape | (mimo)    | Manchester   | 2400-3200   | CP/M    | 0x46     |
| SINCLAIR  | -         | vlastní      | 1400 Bd     | -       | TZX 0x10 |

### 3.3 Specialní případy

#### Formáty s ochranou proti kopírování

V 90. letech se u některých programů pro Sharp MZ-800 objevily obskurní kazetové formáty sloužící k obfuskaci kódu nebo ochraně proti kopírování. Tyto formáty ke své činnosti zpravidla využívaly refresh registr CPU Z80 nebo (předem pevně dané) dynamicky se měnící nastavení CTC i8253, popřípadě kombinaci obojího. Tento zdroj entropie pak byl použit k dynamickým změnám pulsesetu během záznamu/přehrávání, nebo k prostému XORování obsahu datového bloku.

Vzhledem k tomu, že tyto snahy byly Sharp komunitou většinou přijaty jako výzva, tak vždy poměrně rychle došlo k cracknutí původní ochrany a k masivnímu rozšíření těchto programů v již běžném formátu. Z tohoto důvodu se TapeMZ těmito formáty dále do hloubky nezabývá. Nic však nebrání tomu, aby byly v TMZ formátu uloženy jako neidentifikovaný audio blok (TZX blok 0x15 - Direct Recording).

#### Předělávky ze ZX Spectra

Poměrně častou kombinaci vytvářely hry, které se na MZ-800 dostaly jako předělávky z platformy ZX Spectrum. Ty měly na pásce většinou NORMAL FM loader, za kterým následoval načítací obrázek ve formátu SINCLAIR a za ním pak pokračoval NORMAL FM blok obsahující upravenou hru.

Díky různým předělávkám za účelem zjednodušeného spouštění z disket nebo z prostých MZF formátů se bohužel často zachovala jen ta druhá část programu (hra). Pokud však ještě někde naleznete tyto programy v kompletní podobě, tak TMZ je připraven k tomu, aby je zachoval tak, jak se používaly původně - se všemi třemi bloky v původním pořadí.

#### Audio labely Intercopy

Program Intercopy generuje při ukládání na pásku speciální audio bloky umístěné před vlastním headerem záznamu. Prostřednictvím hradla i8255 PPI syntetizuje hlasový label oznamující jméno programu, který následuje. Tato audio data mohou být v TMZ uložena jako neidentifikovaný audio blok (TZX blok 0x15 - Direct Recording).

### 3.4 Pulsní sady

Délky pulzů se liší podle modelu počítače a podle programu, který záznam zapsal. ROM MZ-800 zapisuje pulzy asymetricky (HIGH a LOW mají různou délku). Nominál podle dokumentace HW:

| Parametr      | MZ-800/1500 ROM (nominál) |
|---------------|---------------------------|
| Long (H + L)  | 470 + 494 us              |
| Short (H + L) | 240 + 278 us              |

Výchozí pulzní sady v1.0, které používají kodéry (symetrické):

| Parametr      | MZ-700/80K/80A | MZ-800/1500   | MZ-80B        |
|---------------|----------------|---------------|---------------|
| Long (H + L)  | ~504 + 504 us  | ~498 + 498 us | ~333 + 334 us |
| Short (H + L) | ~252 + 252 us  | ~249 + 249 us | ~167 + 166 us |

Hodnoty pro MZ-700/80K/80A a MZ-80B nemají doložený zdroj [neověřeno]. Předzavaděče FSK, SLOW a DIRECT používají pro MZ-700 jinou sadu: LONG 464 + 494 us, SHORT 240 + 264 us [neověřeno].

ROM má pevný čtecí bod: signál vzorkuje v pevném čase po hraně pulzu a podle úrovně v tom okamžiku rozliší krátký a dlouhý pulz. Čitelnost záznamu proto závisí na délkách pulzů vůči čtecímu bodu, ne na symetrii pulzu. Zavaděče TurboCopy a Intercopy čtecí bod mění (přepíší konstantu čtecí rutiny ROM).

### 3.5 Checksum

Všechny Sharp MZ formáty používají 16bitový checksum založený na population count - součtu jedničkových bitů přes všechny bajty bloku. Mechanismus je jednoduchý, ale dostatečný pro detekci většiny chyb při čtení z pásky.

## 4. TZX formát - základ pro TMZ

### 4.1 Co je TZX

TZX (ZX Spectrum Tape Image) je standardní formát pro uchovávání obsahu ZX Spectrum kazet. Byl navržen Tomaszem Slaninou v roce 1997 a od té doby se stal de facto standardem v retro computing komunitě. Aktuální verze je v1.20.

### 4.2 Struktura TZX

TZX soubor se skládá z:
- **Hlavičky** (10 bajtů): signatura "ZXTape!", EOF marker 0x1A, verze (major.minor)
- **Sekvence bloků**: každý blok začíná 1bajtovým ID, následuje délka a data

Typy bloků:
- **Audio bloky** (0x10-0x15, 0x18, 0x19): nesou zvuková data nebo parametry pro generování signálu
- **Řídící bloky** (0x20-0x27): pauzy, skupiny, smyčky
- **Informační bloky** (0x30-0x35, 0x5A): metadata, archivní informace
- **Rozsah 0x40-0x4F**: TZX v1.20 ho nedefinuje (0x40 byl zastaralý blok Snapshot) - zde se zařazují TMZ bloky

### 4.3 TMZ rozšíření

TMZ používá signaturu "TapeMZ!" místo "ZXTape!" a definuje 7 typů MZ-specifických bloků v rozsahu 0x40-0x46:

| ID   | Název              | Účel                                            |
|------|--------------------|-------------------------------------------------|
| 0x40 | MZ Standard Data   | NORMAL 1200 Bd záznam (kompletní MZF)           |
| 0x41 | MZ Turbo Data      | Konfigurovatelný formát/rychlost/timing          |
| 0x42 | MZ Extra Body      | Dodatečný datový blok pro víceblokové programy   |
| 0x43 | MZ Machine Info    | Cílový stroj, CPU takt, ROM verze                |
| 0x44 | MZ Loader          | Preloader program (TURBO/FASTIPL/FSK/SLOW/...)   |
| 0x45 | MZ BASIC Data      | BSD/BRD chunkovací formát                        |
| 0x46 | MZ File            | MZF soubor a seznam jeho částí (TMZ v1.1)        |

Bloky 0x47-0x4F jsou rezervovány pro budoucí rozšíření.

Formální specifikace TMZ formátu je v samostatném dokumentu: **[tmz-specification.md](https://github.com/michalhucik/TapeMZ/blob/main/docs/cz/tmz-specification.md)**; blok 0x46 popisuje **[tmz-v1.1-specification.md](https://github.com/michalhucik/TapeMZ/blob/main/docs/cz/tmz-v1.1-specification.md)**

## 5. WAV analyzér

### 5.1 Přehled

WAV analyzér je klíčová součást projektu - umožňuje automatickou analýzu a dekódování nahrávek (WAV) magnetofonových kazet Sharp MZ. Jedná se o pětivrstevnou pipeline zpracovávající surový PCM audio signál až na strukturovaná data.

### 5.2 Architektura pipeline

```
WAV soubor (PCM 8/16/24/32/64-bit)
  |
  v
[Vrstva 0] Předzpracování signálu
  |-- Odstranění DC offsetu
  |-- Horní propust (high-pass filtr)
  |-- Normalizace amplitudy
  |
  v
[Vrstva 1] Extrakce pulzů
  |-- Zero-crossing detektor (výchozí)
  |-- Schmitt trigger (volitelný, pro zašuměné nahrávky)
  |
  v
[Vrstva 2] Detekce leader tónu
  |-- Adaptivní kalibrace (měření průměrné pul-periody)
  |-- Toleranční okno pro rozptyl délek pulzů
  |
  v
[Vrstva 2b] Histogramová analýza
  |-- Detekce modů (peaků) v distribuci délek pulzů
  |-- Automatický výpočet prahů mezi SHORT/LONG
  |-- Diagnostika kvality signálu
  |
  v
[Vrstva 3] Klasifikace formátu
  |-- Dvoustupňová detekce:
  |     1. Třída rychlosti z průměru leader tónu
  |     2. Signatura v hlavičce (ftype, $BB prefix, předzavaděč TURBO)
  |-- Rozpoznávané formáty: NORMAL, TURBO, FASTIPL, BSD,
  |   CP/M CMT, CP/M Tape, FSK, SLOW, DIRECT, ZX Spectrum
  |   (MZ-80B se automaticky nerozpoznává)
  |
  v
[Vrstva 4] Specializované dekodéry
  |-- wav_decode_fm    : NORMAL, SINCLAIR, CP/M CMT, MZ-80B
  |-- wav_decode_turbo : TURBO (FM konfigurovatelné časování)
  |-- wav_decode_fastipl : FASTIPL ($BB prefix)
  |-- wav_decode_bsd   : BSD/BRD (chunkovací protokol)
  |-- wav_decode_fsk   : FSK (Frequency Shift Keying)
  |-- wav_decode_slow  : SLOW (kvarternální kódování)
  |-- wav_decode_direct: DIRECT (přímý bitový zápis)
  |-- wav_decode_cpmtape: CP/M Tape (Manchester)
  |-- wav_decode_zx    : ZX Spectrum (pilot ~612 us)
  |
  v
Výstup: MZF soubory nebo TMZ archiv
```

### 5.3 Analytické metody

Analyzér kombinuje několik nezávislých analytických metod:

1. **Adaptivní kalibrace leader tónu** - měření 100+ po sobě jdoucích pulzů, výpočet průměru, kontrola konzistence (inspirováno algoritmem Intercopy 10.2)

2. **Histogramová analýza** - statistická distribuce délek pulzů odhaluje bimodální (FM: SHORT/LONG) nebo multimodální (SLOW: 4 symboly) rozdělení

3. **Zero-crossing detektor** - standardní metoda extrakce pulzů z analogového signálu, přesná pro kvalitní nahrávky

4. **Schmitt trigger** - volitelná alternativa s hysterezí pro zašuměné nahrávky s kolísáním amplitudy

5. **Dvoustupňová klasifikace** - nejprve určí třídu rychlosti z leader tónu, poté identifikuje konkrétní formát z hlavičkových signatur

6. **CRC verifikace** - kontrola checksumů hlavičky i těla pro ověření integrity dekódovaných dat

7. **Copy2 search** - hledání zálohy dat v případě chyby v primárním bloku (dvojitý záznam na pásce)

### 5.4 Výstupní formáty

Analyzér produkuje:
- Jednotlivé **MZF soubory** (výchozí režim) - jeden soubor na každý dekódovaný program
- **TMZ archiv** (volba --output-format tmz) - jeden soubor se všemi bloky, včetně metadat o zdroji

Pro neidentifikované úseky signálu lze volitelně (--keep-unknown) generovat TZX bloky Direct Recording (0x15).

## 6. Zdroje a reference

Projekt čerpá z následujících zdrojů:

### 6.1 Hardwarová dokumentace
- **Sharp MZ-800 ROM** - rutiny RHEAD ($04D8), RDATA ($04F8) pro čtení z pásky, timing konstanty
- **Sharp MZ-700 dokumentace** - pulsní sady, ROM monitor
- **Sharp MZ-80B dokumentace** - odlišný timing (1800 Bd základ), vlastní pulsní sada

### 6.2 Software
- **Intercopy 10.2** (MZ-800, Marek Šmihla - NIPSOFT) - referenční implementace autodetekce, Z80 disassembly
- **TurboCopy V1.21** (MZ-800, Michal Kreidl) - TURBO loader implementace
- **cmt.com** (CP/M) - CP/M CMT, FM 2400 Bd
- **ZTAPE/TAPE.COM** (CP/M, Pezik/MarVan) - CP/M Tape, manchesterské kódování
- **mzftools** (mz-fuzzy, GPLv3) - referenční C implementace MZF konverzí, FSK/SLOW/DIRECT Z80 loadery

### 6.3 Formátové specifikace
- **TZX v1.20** - základní formát, bloková struktura
- **TAP** (ZX Spectrum) - jednoduchý kazetový formát
- **CSW v2** - kompresní formát pro záznam signálu

### 6.4 Analytické reference
- **audio2tape** (fuse-utils) - Schmitt trigger reference
- **wavdec.c** - částečný C port algoritmu Intercopy
- **cmttool** - dekodér NORMAL/CPM formátů

## 7. Knihovny

Projekt obsahuje sadu C knihoven (C99/C11) s modulární architekturou:

### 7.1 Základní I/O a utility
- **generic_driver** - abstraktní I/O handler pro soubory i paměťové buffery
- **endianity** - konverze little-endian (Z80) a host byte order
- **sharpmz_ascii** - Sharp MZ znaková sada (EU/JP varianty), konverze do standardní
  ASCII a UTF-8/Unicode, KOI8-CS a display kódy

### 7.2 Formátové knihovny
- **wav** - čtení/zápis RIFF WAVE (PCM 8/16/24/32/64-bit), normalizace na float
- **mzf** - MZF formát (128B hlavička + tělo), validace, endianita
- **mzf_tools** - pomocné funkce (konverze názvů, factory, dump)
- **cmtspeed** - poměry rychlosti pásky (1:1, 2:1, 7:3, 3:1, ...)

### 7.3 CMT audio vrstva
- **cmt_bitstream** - 1 bit = 1 vzorek, O(1) náhodný přístup
- **cmt_vstream** - RLE kódované pulzy (1/2/4 bajty), prostorově úsporné
- **cmt_stream** - polymorfní wrapper sjednocující bitstream/vstream

### 7.4 TZX/TMZ formátová vrstva
- **tzx** - parsování a generování audio streamů ze všech TZX v1.20 bloků
- **tmz** - čtení/zápis TMZ/TZX souborů, bloková struktura, parsování MZ bloků
- **tmz_blocks** - definice a parsování jednotlivých typů bloků (0x40-0x45)
- **tmz_mzfile** - čtení, validace a zápis bloku 0x46 (MZ File)
- **tmz_profile** - registr profilů zapisovače: z MZF sestaví blok 0x46 tak, jak soubor zapisuje ROM nebo konkrétní program (ROM-800-*, TC-1.21, IC-10.2-*, PEZIK-2.0A, BASIC-1Z016, UNICMT, TAPEMZ-V1-LEGACY). Profil `MZ800EMU-CMT` popisuje signál, který emulátor mz800emu hraje z MZF (mztape bez kopií, tóny 6400/11000, časová základna takt GDG 17 721 600 Hz, všechny poměry rychlosti včetně 2:1 CP/M i vlastní pulzy v µs); přehraný při 17 721 600 Hz dá tytéž pulzy jako mztape
- **tmz_player** - generování CMT audio streamů z TMZ bloků

### 7.5 CMT kodéry
Samostatné knihovny pro každý kazetový formát, včetně embedded Z80 loaderů:
- **mztape** - NORMAL FM (standardní a zvýšené rychlosti)
- **zxtape** - ZX Spectrum TAP (1400 Bd základ)
- **mzcmt_turbo** - TURBO (konfigurovatelné FM časování, TurboCopy loader)
- **mzcmt_fastipl** - FASTIPL ($BB prefix, Intercopy V02/V07 loader)
- **mzcmt_fsk** - FSK (Frequency Shift Keying, 7 rychlostí, dvoufázový loader)
- **mzcmt_slow** - SLOW (kvarternální kódování, 5 rychlostí, dvoufázový loader)
- **mzcmt_direct** - DIRECT (přímý bitový zápis, dvoufázový loader)
- **mzcmt_bsd** - BSD/BRD v1.0 (tón jen před prvním kusem; BASIC takový záznam za prvním kusem nepřečte). Používá ho jen profil TAPEMZ-V1-LEGACY bsd, player hraje BSD profilem BASIC-1Z016
- **mzcmt_manchester** - obecný generátor rodiny Manchester pro blok 0x46 (CP/M Tape: profily PEZIK-2.0A 2400/3200 Bd a IC-10.2-CPM 1500/2400/2800/3200/3600 Bd)

### 7.6 WAV analyzér
- **wav_analyzer** - kompletní analytická pipeline (preprocessing, extrakce pulzů, detekce leaderu, klasifikace formátu, dekódování)

Podrobná dokumentace jednotlivých knihoven bude publikována v samostatných dokumentech.

## 8. CLI nástroje

Projekt obsahuje 11 příkazových nástrojů pro konverze, analýzu a editaci:

| Nástroj      | Účel                                              |
|--------------|---------------------------------------------------|
| **tmzinfo**  | Zobrazení obsahu TMZ/TZX souboru                  |
| **mzf2tmz**  | Konverze MZF/MZT do TMZ                           |
| **tmz2mzf**  | Extrakce MZF souborů z TMZ/TZX                    |
| **tmz2wav**  | Generování WAV audio z TMZ/TZX (přehrávač)        |
| **wav2tmz**  | Analýza WAV nahrávek a dekódování do MZF/TMZ      |
| **tap2tmz**  | Konverze ZX Spectrum TAP do TZX/TMZ               |
| **tmz2tap**  | Extrakce TAP bloků z TMZ/TZX                      |
| **tmzconv**  | Konverze signatury TMZ a TZX                      |
| **tmzedit**  | Editor páskového souboru (list, dump, edit, merge) |
| **dat2bsd**  | Import binárních dat do TMZ jako BSD (blok 0x46)   |
| **bsd2dat**  | Export BSD dat z TMZ (bloky 0x45 a 0x46)           |

Podrobná dokumentace každého nástroje s příklady je v samostatných souborech **tools-*.md**.

## 9. Budoucí plány

### 9.1 GUI aplikace

V přípravě je grafická aplikace umožňující:
- Drag & drop přenosy souborů mezi virtuálními páskami
- Vizuální editor páskového obsahu
- Přehledné zobrazení bloků s metadaty
- Interaktivní konverze mezi formáty

### 9.2 Integrace do mz800emu

TMZ formát a všechny přidružené knihovny budou integrovány do emulátoru **mz800emu**, kde budou fungovat jako:
- **VirtualCMT** - virtuální kazetový přehrávač/rekordér schopný přehrávat TMZ/TZX soubory v reálném čase a nahrávat výstup z emulovaného počítače
- **Online WAV analyzér** - analýza a dekódování WAV nahrávek přímo v emulátoru

Stávající funkce pro načítání prostých MZF/MZT a WAV souborů zůstanou zachovány.

## 10. Licence

Projekt je licencován pod GNU General Public License v3 (GPLv3).

Některé knihovny (mzcmt_fsk, mzcmt_slow) obsahují embedded Z80 loader binárky odvozené z projektu mzftools (mz-fuzzy, GPLv3). Knihovna mzcmt_turbo obsahuje loader odvozený z reverse engineeringu TurboCopy V1.21 (Michal Kreidl).

## 11. Autoři

- Michal Hucik (hucik@ordoz.com) - autor a hlavní vývojář

Copyright (C) 2017-2026 Michal Hucik
