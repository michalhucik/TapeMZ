# TMZ Format Specification v1.0

## 1. Úvod

TMZ (TapeMZ) je souborový formát pro uchovávání obsahu magnetofonových kazet počítačů Sharp MZ (MZ-700, MZ-800, MZ-1500, MZ-80B) a ZX Spectrum. Formát vychází z TZX v1.20 (ZX Spectrum tape format) a rozšiřuje ho o bloky specifické pro platformu Sharp MZ.

TMZ je plně kompatibilní s TZX na úrovni blokové struktury. Hlavička "TapeMZ!" (místo "ZXTape!") zaručuje, že extenzivní bloková ID (0x40-0x4F) jsou použita pro Sharp MZ specifické údaje. Reader TZX formátu může TMZ soubor přečíst - neznámé bloky přeskočí díky uniformní blokové struktuře.

## 2. Hlavička souboru (10 bajtů)

```
Offset  Velikost  Hodnota    Popis
0x00    7         "TapeMZ!"  Signatura (ASCII)
0x07    1         0x1A       EOF marker (end-of-file)
0x08    1         1          Major verze
0x09    1         0          Minor verze
```

Signatura "TapeMZ!" odlišuje TMZ od TZX ("ZXTape!"). EOF marker 0x1A zastavuje DOS příkaz TYPE při zobrazení souboru (konvence zděděná z TZX). Verze 1.0 je první vydaná verze specifikace.

## 3. Bloková struktura

### 3.1 Obecná konvence
- Každý blok začíná 1bajtovým ID
- Audio bloky (0x10-0x15, 0x18, 0x19) mají blokově-specifické hlavičky s vlastní definicí délky (viz TZX v1.20 specifikace pro každý typ)
- Řídící a informační bloky (0x20-0x5A) většinou začínají DWORD LE délkou (ale některé mají speciální formát: 0x20=2B, 0x21=1B+text, 0x22/0x25/0x27=0B, atd.)
- TMZ extenzivní bloky (0x40-0x4F) mají vždy DWORD LE délku za ID
- Reader přeskočí neznámé bloky pomocí délkového pole
- Všechna vícebajtová pole jsou v little-endian (LE) pořadí

### 3.2 Standardní TZX bloky

Všechny TZX v1.20 bloky (0x10-0x5A) jsou platné v TMZ beze změny. Jejich sémantika a časování zůstávají dle TZX specifikace - T-states při 3.5 MHz (ZX Spectrum Z80).

Kompletní seznam viz TZX v1.20 specifikace.

## 4. TMZ Extension bloky (0x40-0x4F)

Rozsah ID 0x40-0x4F je v TZX specifikaci vyhrazen pro extenze. TMZ tyto bloky využívá pro Sharp MZ specifická data.

### 4.1 Blok 0x40 - MZ Standard Data Block

Kompletní MZF záznam ve standardním NORMAL 1200 Bd formátu (kategorie 1). Tento blok je vyhrazen pro základní rychlost 1200 Bd. Pro NORMAL FM při jiných rychlostech (2400, 2800, 3200 Bd atd. - kategorie 2) se používá blok 0x41 s format=NORMAL a příslušným rychlostním poměrem.

Player z tohoto bloku generuje kompletní kazetový signál: LGAP + LTM + header + CHKH + SGAP + STM + body + CHKF.

```
Offset  Typ    Popis
0x00    BYTE   ID bloku (0x40)
0x01    DWORD  Délka dat (= 131 + N)
0x05    BYTE   Cílový stroj (0=generic, 1=MZ-700, 2=MZ-800, 3=MZ-1500, 4=MZ-80B)
0x06    BYTE   Pulsní sada (0=MZ-700, 1=MZ-800, 2=MZ-80B)
0x07    WORD   Pauza po bloku (ms), 0 = žádná pauza
0x09    128B   MZF hlavička (raw 128 bajtů v originálním formátu)
0x89    WORD   Velikost těla N (redundantní s MZF header.fsize)
0x8B    N B    Data těla
```

Pole "Cílový stroj" určuje, pro který model je záznam určen. Pole "Pulsní sada" určuje, jaké délky pulzů se použijí pro generování signálu. Redundantní pole "Velikost těla" umožňuje rychlý přístup bez parsování MZF hlavičky.

### 4.2 Blok 0x41 - MZ Turbo Data Block

MZF záznam s konfigurovatelnou rychlostí, formátem a timingem. Používá se pro:
- Kategorii 2: NORMAL FM při nestandardní rychlosti (2400, 2800, 3200 Bd atd.)
- Kategorii 3: loader v hlavičce (TURBO, FASTIPL)
- Kategorii 4: loader jako program (FSK, SLOW, DIRECT)
- CP/M Tape: Pezik/MarVan manchesterské kódování

Pole "Formát" a "Rychlost" určují, jaký signál player generuje.

```
Offset  Typ    Popis
0x00    BYTE   ID bloku (0x41)
0x01    DWORD  Délka dat
0x05    BYTE   Cílový stroj (0=generic, 1=MZ-700, 2=MZ-800, 3=MZ-1500, 4=MZ-80B)
0x06    BYTE   Pulsní sada (0=MZ-700, 1=MZ-800, 2=MZ-80B)
0x07    BYTE   Formát (viz en_TMZ_FORMAT)
0x08    BYTE   Rychlost (interpretace závisí na formátu - viz tabulka níže)
0x09    WORD   LGAP délka (počet krátkých pulzů, 0=default pro formát)
0x0B    WORD   SGAP délka (počet krátkých pulzů, 0=default)
0x0D    WORD   Pauza po bloku (ms), 0 = žádná pauza
0x0F    WORD   Dlouhý pulz high (us*100, 0=default z pulsní sady)
0x11    WORD   Dlouhý pulz low (us*100, 0=default)
0x13    WORD   Krátký pulz high (us*100, 0=default)
0x15    WORD   Krátký pulz low (us*100, 0=default)
0x17    BYTE   Flags (bit0=s kopií hlavičky, bit1=s kopií těla)
0x18    128B   MZF hlavička (raw 128 bajtů)
0x98    WORD   Velikost těla N
0x9A    N B    Data těla
```

Pulsní délky s hodnotou 0 znamenají "použij default z pulsní sady". Jednotka us*100 = 10 ns rozlišení, dostatečně přesné pro všechny známé formáty.

#### Interpretace pole "Rychlost" podle formátu

Pole "Rychlost" (offset 0x08) se interpretuje podle hodnoty pole "Formát":

| Formát                            | Interpretace rychlosti                               | Rozsah |
|-----------------------------------|------------------------------------------------------|--------|
| NORMAL, TURBO, FASTIPL, SINCLAIR  | en_CMTSPEED index (FM poměr)                         | 0-9    |
| FSK                               | en_MZCMT_FSK_SPEED (0=nejpomalejší, 6=nejrychlejší)  | 0-6    |
| SLOW                              | en_MZCMT_SLOW_SPEED (0=nejpomalejší, 4=nejrychlejší) | 0-4    |
| DIRECT                            | ignorováno (0)                                        | 0      |
| CPM_TAPE                          | en_MZCMT_CPMTAPE_SPEED (0=1200, 1=2400, 2=3200 Bd)  | 0-2    |

#### Rychlostní poměry FM formátů (NORMAL, TURBO, FASTIPL)

FM formáty používají en_CMTSPEED jako poměr vůči základní rychlosti 1200 Bd. Rychlost určuje škálování pulsních délek - vyšší poměr = kratší pulzy = vyšší baudrate.

| Hodnota | Poměr   | Baudrate | Poznámka                          |
|---------|---------|----------|-----------------------------------|
| 0       | (žádná) | -        | Neplatná hodnota                  |
| 1       | 1:1     | 1200 Bd  | Standardní rychlost (ROM)         |
| 2       | 2:1     | 2400 Bd  | Dvojnásobná                       |
| 3       | 2:1 CPM | 2400 Bd  | CP/M varianta (cmt.com)           |
| 4       | 3:1     | 3600 Bd  | Trojnásobná                       |
| 5       | 3:2     | 1800 Bd  |                                   |
| 6       | 7:3     | 2800 Bd  | Intercopy 10.2                    |
| 7       | 8:3     | 3200 Bd  | CP/M cmt.com                      |
| 8       | 9:7     | ~1543 Bd |                                   |
| 9       | 25:14   | ~2143 Bd |                                   |

#### Rychlosti SINCLAIR formátu

SINCLAIR formát používá stejné en_CMTSPEED poměry, ale základní rychlost je 1400 Bd (ne 1200 Bd jako u Sharp MZ). Pulsní délky jsou odvozeny z ZX Spectrum timingu (pilot ~612 us, zero ~244 us, one ~488 us).

| Hodnota | Poměr   | Baudrate | Poznámka                          |
|---------|---------|----------|-----------------------------------|
| 1       | 1:1     | 1400 Bd  | Standardní ZX Spectrum rychlost   |
| 5       | 3:2     | 2100 Bd  |                                   |
| 8       | 9:7     | 1800 Bd  |                                   |
| 9       | 25:14   | 2500 Bd  |                                   |

Ostatní poměry z en_CMTSPEED jsou rovněž platné, výše uvedené jsou typické hodnoty.

#### Rychlosti FSK formátu

FSK (Frequency Shift Keying) má 7 rychlostních úrovní. Rychlost je index do tabulky pulsních délek v Z80 loaderu. Délky jsou definovány počty vzorků při referenční frekvenci 44100 Hz (long pulz = bit "1", short pulz = bit "0").

| Hodnota | Long (low+high) | Short (low+high) | Poměr L:S | Popis          |
|---------|------------------|-------------------|-----------|----------------|
| 0       | 6+2 = 8 vzorků   | 2+2 = 4 vzorky   | 2.00      | Nejpomalejší   |
| 1       | 5+2 = 7 vzorků   | 2+2 = 4 vzorky   | 1.75      |                |
| 2       | 4+2 = 6 vzorků   | 2+2 = 4 vzorky   | 1.50      |                |
| 3       | 4+2 = 6 vzorků   | 1+2 = 3 vzorky   | 2.00      |                |
| 4       | 3+2 = 5 vzorků   | 1+1 = 2 vzorky   | 2.50      |                |
| 5       | 2+2 = 4 vzorky   | 1+1 = 2 vzorky   | 2.00      |                |
| 6       | 2+1 = 3 vzorky   | 1+1 = 2 vzorky   | 1.50      | Nejrychlejší   |

Při jiném sample rate se počty vzorků proporcionálně škálují: result = round(ref * rate / 44100).

#### Rychlosti SLOW formátu

SLOW (kvarternální kódování, 2 bity na pulz) má 5 rychlostních úrovní. Každý bajt je zakódován 4 symboly (0-3), každý symbol má odlišnou délku pulzu. Délky jsou v počtech vzorků při 44100 Hz.

| Hodnota | Symbol 0 (L+H) | Symbol 1 (L+H) | Symbol 2 (L+H) | Symbol 3 (L+H) | Popis        |
|---------|-----------------|-----------------|-----------------|-----------------|--------------|
| 0       | 3+3 = 6         | 6+6 = 12        | 9+9 = 18        | 12+12 = 24      | Nejpomalejší |
| 1       | 3+3 = 6         | 5+5 = 10        | 7+7 = 14        | 9+9 = 18        |              |
| 2       | 1+2 = 3         | 3+3 = 6         | 4+5 = 9         | 6+6 = 12        |              |
| 3       | 1+1 = 2         | 2+3 = 5         | 3+5 = 8         | 4+7 = 11        |              |
| 4       | 1+1 = 2         | 2+2 = 4         | 3+3 = 6         | 4+4 = 8         | Nejrychlejší |

Při jiném sample rate se počty vzorků proporcionálně škálují.

#### Rychlost DIRECT formátu

DIRECT formát (přímý bitový zápis) nemá konfigurovatelnou rychlost. Pole "Rychlost" je vždy 0 (ignorováno). Skutečná rychlost přenosu je dána sample ratem - každý bit odpovídá jednomu vzorku signálu (HIGH=1, LOW=0), s režijními synchro bity (12 vzorků na bajt: 8 datových + 4 synchronizační).

#### Rychlosti CP/M Tape formátu

CP/M Tape (Pezik/MarVan, manchesterské kódování) má 3 pevné baudové rychlosti:

| Hodnota | Baudrate | Poznámka                              |
|---------|----------|---------------------------------------|
| 0       | 1200 Bd  | Kompatibilní s ROM rychlostí          |
| 1       | 2400 Bd  | Výchozí pro TAPE.COM                  |
| 2       | 3200 Bd  | Maximální rychlost                    |

#### Referenční srovnání časů načtení

Referenční soubor: **Flappy ver 1.0A** (Flappy.mzf, 44161 bajtů: 128B hlavička + 44033B tělo).
Měřeno jako délka WAV signálu generovaného z TMZ přes tmz2wav (44100 Hz, pulseset MZ-800).
Čas zahrnuje kompletní páskový signál včetně leader tónu, GAP, tapemarků a checksumů.
U formátů kategorie 3 a 4 je součástí i NORMAL FM preloader/loader (~16 s).

**FM formáty (NORMAL, TURBO, FASTIPL):**

| Rychlost          | NORMAL  | TURBO   | FASTIPL |
|-------------------|---------|---------|---------|
| 1:1 (1200 Bd)     | 291.5 s | 312.0 s | 312.3 s |
| 3:2 (1800 Bd)     | 193.7 s | 211.4 s | 213.9 s |
| 9:7 (~1543 Bd)    | 225.7 s | 244.4 s | 246.1 s |
| 2:1 (2400 Bd)     | 141.0 s | 157.3 s | 161.0 s |
| 25:14 (~2143 Bd)  | 163.6 s | 180.6 s | 183.7 s |
| 7:3 (~2800 Bd)    | 124.1 s | 140.0 s | 144.0 s |
| 8:3 (~3200 Bd)    | 111.0 s | 126.5 s | 130.8 s |
| 3:1 (3600 Bd)     |  97.8 s | 113.0 s | 117.6 s |

TURBO a FASTIPL jsou pomalejší než NORMAL při stejné rychlosti, protože obsahují loader v hlavičce (header je vždy v NORMAL 1:1, pouze body v cílové rychlosti).

**FSK (Frequency Shift Keying):**

| Rychlost | Čas     | Efektivní Bd |
|----------|---------|--------------|
| 0        |  58.8 s | ~8 400 Bd    |
| 1        |  56.1 s | ~9 000 Bd    |
| 2        |  53.3 s | ~9 700 Bd    |
| 3        |  48.1 s | ~11 300 Bd   |
| 4        |  40.1 s | ~15 200 Bd   |
| 5        |  37.4 s | ~17 200 Bd   |
| 6        |  34.6 s | ~19 900 Bd   |

**SLOW (kvarternální):**

| Rychlost | Čas     | Efektivní Bd |
|----------|---------|--------------|
| 0        |  64.3 s | ~7 500 Bd    |
| 1        |  56.3 s | ~9 000 Bd    |
| 2        |  40.3 s | ~15 300 Bd   |
| 3        |  36.3 s | ~18 500 Bd   |
| 4        |  32.3 s | ~23 500 Bd   |

**DIRECT (přímý bitový zápis):**

| Rychlost | Čas     | Efektivní Bd |
|----------|---------|--------------|
| 0        |  28.9 s | ~32 000 Bd   |

**CP/M Tape (Manchester):**

| Rychlost | Čas     |
|----------|---------|
| 0 (1200) | 298.2 s |
| 1 (2400) | 149.3 s |
| 2 (3200) | 116.2 s |

Efektivní Bd u FSK, SLOW a DIRECT je vypočtena z doby přenosu datové sekce (celkový čas minus ~16 s NORMAL FM preloader). Skutečná bitová rychlost kolísá v závislosti na obsahu dat, protože bity 0 a 1 (resp. symboly 0-3) mají různou délku pulzu.

### 4.3 Blok 0x42 - MZ Extra Body Block

Dodatečný datový blok pro vícedílné programy. Používá se ve spojení s blokem 0x44 (MZ Loader Block) - loader se nahraje standardní rychlostí a následně přečte body bloky v rychlejším formátu.

```
Offset  Typ    Popis
0x00    BYTE   ID bloku (0x42)
0x01    DWORD  Délka dat
0x05    BYTE   Formát (viz en_TMZ_FORMAT)
0x06    BYTE   Rychlostní poměr (en_CMTSPEED index)
0x07    WORD   Pauza po bloku (ms)
0x09    WORD   Velikost dat N
0x0B    N B    Data
```

### 4.4 Blok 0x43 - MZ Machine Info

Specifikuje cílový stroj a jeho parametry. Ovlivňuje interpretaci následujících bloků.

```
Offset  Typ    Popis
0x00    BYTE   ID bloku (0x43)
0x01    DWORD  Délka dat (= 6)
0x05    BYTE   Typ stroje (0=generic, 1=MZ-700, 2=MZ-800, 3=MZ-1500, 4=MZ-80B)
0x06    DWORD  CPU takt (Hz), např. 3546900 pro MZ-800
0x0A    BYTE   ROM verze (0=neznámá)
```

### 4.5 Blok 0x44 - MZ Loader Block

Loader program (TURBO/FASTIPL/FSK/SLOW/DIRECT), který se nahraje standardní rychlostí a následně přepne na rychlejší čtení pro další bloky (0x42).

```
Offset  Typ    Popis
0x00    BYTE   ID bloku (0x44)
0x01    DWORD  Délka dat
0x05    BYTE   Typ loaderu (viz en_TMZ_LOADER_TYPE)
0x06    BYTE   Rychlost pro následující body bloky (en_CMTSPEED index)
0x07    WORD   Velikost loader dat M
0x09    128B   MZF hlavička (s loader kódem v komentářové části)
0x89    M B    Loader tělo
```

### 4.6 Blok 0x45 - MZ BASIC Data Block

BASIC datový záznam (BSD/BRD). Na rozdíl od standardního MZF záznamu, kde je velikost dat dána polem fsize, u BSD/BRD záznamu je fsize=0 a data jsou rozdělena do 258B chunků. Každý chunk obsahuje 2B ID (LE) a 256B dat. Chunky jsou na pásce uloženy jako samostatné body bloky s krátkým tapemarkem.

TMZ blok 0x45 uchovává kompletní chunkovací záznam - hlavičku i všechny chunky včetně jejich ID. Player z tohoto bloku generuje kompletní kazetový signál: LGAP + LTM + header + CHKH + (STM + chunk + CHK) * N.

```
Offset  Typ    Popis
0x00    BYTE   ID bloku (0x45)
0x01    DWORD  Délka dat
0x05    BYTE   Cílový stroj (0=generic, 1=MZ-700, 2=MZ-800, 3=MZ-1500, 4=MZ-80B)
0x06    BYTE   Pulsní sada (0=MZ-700, 1=MZ-800, 2=MZ-80B)
0x07    WORD   Pauza po bloku (ms)
0x09    128B   MZF hlavička (ftype=0x03 nebo 0x04, fsize/fstrt/fexec=0)
0x89    WORD   Počet chunků N
0x8B    N*258B Chunky (každý: 2B ID LE + 256B data)
```

Struktura chunků:
- První chunk: ID = 0x0000
- Následující chunky: ID se inkrementuje (0x0001, 0x0002, ...)
- Poslední chunk: ID = 0xFFFF (ukončující marker)
- Celková velikost BASIC dat = N * 256 bajtů

### 4.7 Bloky 0x46-0x4F - Rezervované

Rezervované pro budoucí rozšíření. Reader musí tyto bloky přeskočit pomocí délky za ID.

## 5. Časování

### 5.1 TMZ MZ-specifické bloky (0x40-0x4F)

Pulsní délky v TMZ MZ-specifických blocích používají jednotku us*100 (1 jednotka = 10 ns). Toto časování je absolutní - nezávisí na CPU taktu cílového stroje.

Hodnota 0 znamená "použij default z pulsní sady".

### 5.2 Default pulsní sady

| Parametr       | MZ-700/80K/80A | MZ-800/1500 | MZ-80B |
|----------------|----------------|-------------|--------|
| Long high (us) | ~464           | ~498        | ~333   |
| Long low (us)  | ~494           | ~498        | ~334   |
| Short high (us)| ~240           | ~249        | ~167   |
| Short low (us) | ~264           | ~249        | ~166   |

MZ-800/1500 používá symetrické pulzy - ROM používá stejnou delay smyčku pro HIGH i LOW
část pulzu. Původní asym. hodnoty z měření Intercopy 10.2 (GDG ticky {8335,8760},
{4356,4930} -> 470/494/246/278 us) způsobovaly špatné zaokrouhlování při 44100 Hz
(short pulz 11+12=23 vzorků místo 11+11=22).

### 5.3 Standardní TZX bloky (0x10-0x19)

Pro standardní TZX bloky zůstává časování v Z80 T-states při 3.5 MHz (dle TZX specifikace). Player přepočítá T-states na cílový CPU takt podle bloku MZ Machine Info (0x43), pokud je přítomen.

## 6. Konverzní strategie

```
MZF -> TMZ:    Blok 0x40 (standard NORMAL 1:1) nebo 0x41 (jiný formát/rychlost)
TMZ -> MZF:    Extrakce dat z 0x40/0x41 (bezztrátová pro standard)
WAV -> TMZ:    Analýza signálu -> 0x40/0x41/0x45, nebo Direct Recording 0x15
TMZ -> WAV:    Player generuje audio -> WAV export (přes cmt_stream)
TZX -> TMZ:    Změna hlavičky + validace obsahu
TMZ -> TZX:    Změna hlavičky (MZ bloky zůstanou, TZX reader je přeskočí)
TAP -> TMZ:    Konverze TAP bloků na TZX 0x10 (Standard Speed Data)
TMZ -> TAP:    Extrakce bloků 0x10
```

Konverze MZF -> TMZ -> MZF je bezztrátová pro standardní data (round-trip). Konverze WAV -> TMZ může být ztrátová v závislosti na metodě.

## 7. Enumy a konstanty

### 7.1 Cílový stroj (en_TMZ_MACHINE)

| Hodnota | Význam                      |
|---------|-----------------------------|
| 0       | Generic (nespecifikovaný)   |
| 1       | MZ-700                      |
| 2       | MZ-800                      |
| 3       | MZ-1500                     |
| 4       | MZ-80B                      |

### 7.2 Pulsní sada (en_TMZ_PULSESET)

| Hodnota | Význam                       |
|---------|------------------------------|
| 0       | MZ-700 (MZ-700, MZ-80K, MZ-80A) |
| 1       | MZ-800 (MZ-800, MZ-1500)    |
| 2       | MZ-80B                       |

### 7.3 Formát záznamu (en_TMZ_FORMAT)

| Hodnota | Název    | Popis                                                         |
|---------|----------|---------------------------------------------------------------|
| 0       | NORMAL   | Standardní 1200 Bd FM modulace (1 bit = 1 pulz)              |
| 1       | TURBO    | Proprietární rychlé kódování (TurboCopy)                      |
| 2       | FASTIPL  | Rychlé IPL kódování s $BB prefixem (Intercopy)                |
| 3       | SINCLAIR | ZX Spectrum kompatibilní kódování                             |
| 4       | FSK      | Frequency Shift Keying (změna frekvence nosného signálu)      |
| 5       | SLOW     | 2 bity na pulz, kompaktnější než FM                           |
| 6       | DIRECT   | Přímý bitový zápis bez modulace, nejrychlejší přenos          |
| 7       | CPM_TAPE | Pezik/MarVan manchesterské kódování (ZTAPE/TAPE.COM pod CP/M)|

### 7.4 Rychlostní poměry FM formátů (en_CMTSPEED)

| Hodnota | Poměr   | Baudrate při 1200 Bd základu |
|---------|---------|------------------------------|
| 0       | (žádná) | neplatná hodnota             |
| 1       | 1:1     | 1200 Bd (standardní)         |
| 2       | 2:1     | 2400 Bd                      |
| 3       | 2:1 CPM | 2400 Bd (CP/M varianta)      |
| 4       | 3:1     | 3600 Bd                      |
| 5       | 3:2     | 1800 Bd                      |
| 6       | 7:3     | 2800 Bd (Intercopy 10.2)     |
| 7       | 8:3     | 3200 Bd (CP/M cmt.com)       |
| 8       | 9:7     | ~1543 Bd                     |
| 9       | 25:14   | ~2143 Bd                     |

### 7.5 Typ loaderu (en_TMZ_LOADER_TYPE)

| Hodnota | Název        | Popis                            |
|---------|--------------|----------------------------------|
| 0       | TURBO_1_0    | Turbo loader verze 1.0           |
| 1       | TURBO_1_2x   | Turbo loader verze 1.2x          |
| 2       | FASTIPL_V2   | Fast-IPL loader verze 2          |
| 3       | FASTIPL_V7   | Fast-IPL loader verze 7          |
| 4       | FSK          | FSK loader                       |
| 5       | SLOW         | SLOW loader                      |
| 6       | DIRECT       | DIRECT loader                    |

## 8. Kompatibilita

### 8.1 TZX kompatibilita
- TMZ reader musí umět přečíst i soubory s hlavičkou "ZXTape!" (nativní TZX)
- TMZ bloky (0x40-0x4F) jsou TZX readerem automaticky přeskočeny (díky uniformní blokové struktuře s DWORD délkou)
- Pro konverzi TZX -> TMZ stačí změnit signaturu v hlavičce

### 8.2 Zpětná kompatibilita
- TMZ soubor bez MZ-specifických bloků je funkčně ekvivalentní TZX souboru
- Player musí podporovat oba typy hlaviček (TapeMZ! i ZXTape!)
- Jeden soubor může obsahovat jak ZX Spectrum (bloky 0x10-0x15), tak Sharp MZ (0x40-0x45) data

## 9. Verifikace

Pro ověření správnosti implementace se doporučují tyto testy:

- **Parser**: načíst existující TZX soubor, ověřit správné přeskočení bloků
- **Round-trip MZF**: mzf2tmz -> tmz2mzf, bit-perfect shoda vstupních a výstupních dat
- **Round-trip WAV**: mzf2tmz -> tmz2wav, porovnání s přímým výstupem z mztape kodéru
- **Player**: generování streamu z TMZ, porovnání s přímým mztape výstupem
- **Validace**: tmzedit validate pro kontrolu integrity souboru
