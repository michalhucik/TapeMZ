# TMZ Format Specification v1.1

Tato specifikace rozšiřuje TMZ v1.0 o blok 0x46 (MZ File). Co tato
specifikace nemění, platí podle v1.0 (`tmz-specification.md`).

Verze souboru je 1.1, protože všechny bloky v1.0 zůstávají platné
(kap. 2).

## 1. Úvod a principy

1. **TZX je jádro.** Všechny bloky TZX v1.20 platí beze změny. Signál, který
   nepopisuje žádný profil (neznámý formát, archivní nahrávka), se ukládá
   do bloků TZX (0x10-0x15, 0x19, 0x20). MZ bloky nesou **význam**: MZF
   soubor, profil zapisovače a strukturu záznamu.
2. **Blok je soběstačný.** Datový blok 0x46 nese efektivní hodnoty všeho,
   co player potřebuje: strukturu záznamu (seznam částí), časování a
   kódování. ID profilu je jen popisek a slouží k validaci. Čtečka, která
   profil nezná, blok přesto přehraje.
3. **Čas je celé číslo v jednotkách časové základny části.** Časová základna
   je takt v Hz (T-stavy CPU, tiky CTC, µs, vzorky). Jednotku us*100
   z v1.0 tato verze nepoužívá.
4. **Rozšiřitelnost řeší TLV** s příznakem "povinné k pochopení" (kap. 5).
5. **Rodiny modulace jsou pevná sada** (FM, FSK, SLOW, DIRECT, MANCHESTER,
   PILOT). Nová rodina znamená novou verzi této specifikace.
6. **Formát TMZ nepoužívá nic mimo projekt TapeMZ** (knihovna, nástroje,
   integrace do mz800emu). Kompatibilita s v1.0 slouží jen existujícím
   souborům. Kompatibilita s nástroji TZX platí dál.

Pojmy:
- **pulz** = dvě půlvlny, první a druhá (kap. 5.4). SHORT (S) a LONG (L)
  jsou dva druhy pulzu rodiny FM.
- **část** = jeden fyzický záznam na pásce (tón, tapemark, data, checksum,
  koncovka, kopie). Program zapíše jeden soubor jako jednu nebo více částí.
- **profil** = popis toho, jak konkrétní program nebo ROM zapisuje (registr
  profilů, kap. 8).

## 2. Hlavička souboru

Beze změny proti v1.0 kromě verze:

```
Offset  Velikost  Hodnota    Popis
0x00    7         "TapeMZ!"  Signatura (ASCII)
0x07    1         0x1A       EOF marker
0x08    1         1          Major verze
0x09    1         1          Minor verze (1.1 = tato specifikace)
```

- Čtečka v1.1 přijme verze 1.0 a 1.1. Major verze jiná než 1 je chyba
  načtení.
- Zapisovač v1.1 zapisuje verzi 1.1. Soubor bez bloku 0x46 smí mít 1.0.
- Hlavička "ZXTape!" (nativní TZX) zůstává podporovaná podle v1.0.

## 3. Bloky MZ ve v1.1

| ID | název | stav ve v1.1 |
|---|---|---|
| 0x40 | MZ Standard Data | zastaralý: čte se a přehraje (kap. 9), nové soubory ho nezapisují |
| 0x41 | MZ Turbo Data | zastaralý: dtto; pro rodiny mimo FM ho zapisovač používá, dokud jejich rodina není implementovaná (kap. 7) |
| 0x42 | MZ Extra Body | zastaralý, nepodporovaný (kap. 9.3) |
| 0x43 | MZ Machine Info | platí: cílový stroj, takt CPU; takt je výchozí časová základna (kap. 5.3) |
| 0x44 | MZ Loader | zastaralý, nepodporovaný (kap. 9.3) |
| 0x45 | MZ BASIC Data | zastaralý: čte se a přehraje (kap. 9) |
| **0x46** | **MZ File** | **nový: jeden MZF soubor a seznam jeho částí (kap. 4)** |
| 0x47-0x4F | - | rezervované; čtečka je přeskočí s varováním |

Všechny MZ bloky mají za ID délku DWORD LE (v1.0 kap. 3.1), čtečka tedy
každý blok umí přeskočit.

## 4. Blok 0x46 - MZ File

Jeden MZF soubor tak, jak ho program uložil, a seznam částí, které tvoří
jeho záznam na pásce. Player části přehraje v uvedeném pořadí a doslova.
ID profilu k tomu nepotřebuje.

### 4.1 Rozvržení

```
Offset  Typ      Popis
0x00    BYTE     ID bloku (0x46)
0x01    DWORD    L = délka zbytku bloku (od offsetu 0x05 do konce bloku)
0x05    BYTE     verze bloku (1)
0x06    BYTE     n = délka ID profilu (0..32; 0 = bez ID)
0x07    n B      ID profilu, ASCII (kap. 8.1)
        128 B    MZF hlavička (bajty, jak je program uložil; LE jako v MZF)
        DWORD    N = délka těla
        N B      tělo
        WORD     T = délka oblasti blokových TLV
        T B      blokové TLV (kap. 5.5)
        BYTE     P = počet částí (1..255)
        P x      část (kap. 4.2)
```

Invarianty (porušení = chyba struktury bloku, kap. 4.4):
- Všechna pole leží uvnitř bloku a jejich součet délek je přesně L.
- `n <= 32`, `P >= 1`.
- Délka těla N nemusí souhlasit s `fsize` v MZF hlavičce. Rozhoduje N.
  Příklad: soubory BASIC (BSD/BRD) mají `fsize = 0` a tělo nese chunky
  (kap. 6.7).

MZF hlavička a tělo jsou **soubor, jak ho program uložil**. Liší-li se
bajty na pásce od vstupního souboru, uloží se bajty z pásky. Bajty, které na pásce nejsou
součástí MZF (hlavička předzavaděče, předzavaděč), nesou části jako inline
data.

### 4.2 Část

```
Typ      Popis
DWORD    délka části (bajty za tímto polem až do konce části)
BYTE     role (tab. 4.3.1)
BYTE     rodina (tab. 4.3.2)
BYTE     zdroj dat (tab. 4.3.3)
[...]    pole zdroje dat podle tab. 4.3.3
TLV[]    parametry části až do konce části (kap. 5)
```

Bajty části (data, ze kterých rodina generuje signál) určuje zdroj dat.
Checksum a další bajty, které rodina přidává, mezi ně nepatří (kap. 6.5).

### 4.3 Číselníky

#### 4.3.1 Role

Role popisuje význam části pro člověka a nástroje (tmzinfo, validace,
export). Player ji nepoužívá. Neznámá hodnota se čte jako 0.

| hodnota | role | význam |
|---|---|---|
| 0 | OTHER | jiná nebo neznámá část |
| 1 | HEADER | MZF hlavička programu |
| 2 | BODY | tělo programu (nebo jeho úsek) |
| 3 | LOADER_HEADER | hlavička předzavaděče (např. TurboCopy Loader Y) |
| 4 | LOADER | předzavaděč nebo zavaděč |
| 5 | BSD_CHUNK | chunk souboru BASIC (BSD/BRD) |

#### 4.3.2 Rodina

| hodnota | rodina | kapitola | stav |
|---|---|---|---|
| 0 | - | - | neplatná |
| 1 | FM | 6 | definovaná |
| 2 | FSK | 7.1 | rezervovaná, parametry definuje budoucí verze specifikace |
| 3 | SLOW | 7.1 | rezervovaná |
| 4 | DIRECT | 7.1 | rezervovaná |
| 5 | MANCHESTER | 7.2 | definovaná |
| 6 | PILOT | 7.3 | rezervovaná |

Část s neznámou nebo rezervovanou rodinou player nepřehraje (kap. 4.4).

#### 4.3.3 Zdroj dat

| hodnota | zdroj | pole | bajty části |
|---|---|---|---|
| 0 | MZF_HEADER | - | 128 B MZF hlavičky bloku |
| 1 | MZF_BODY | - | celé tělo bloku |
| 2 | BODY_SLICE | DWORD offset, DWORD délka | výřez těla `[offset, offset+délka)` |
| 3 | INLINE | DWORD délka, bajty | bajty uložené v části |

Výřez musí ležet uvnitř těla. Neznámý zdroj je chyba části (délku polí
nelze určit, část se přeskočí podle své délky).

### 4.4 Chyby

| situace | reakce |
|---|---|
| délky polí přesahují blok, součet nesedí s L, `n > 32`, nepovolený znak v ID profilu, `P = 0`, délka části přesahuje blok | chyba struktury: chyba načtení souboru |
| verze bloku jiná než 1 | blok se nepřehraje, varování, pokračuje se dalším blokem |
| neznámá rodina nebo zdroj, výřez mimo tělo, neznámý povinný tag, neplatná hodnota povinného tagu | chyba části: player část nepřehraje a ohlásí chybu; nic nedomýšlí |
| neznámý nepovinný tag | přeskočí se |

## 5. TLV

### 5.1 Kódování

```
WORD   tag     bit 15 = povinné k pochopení, bity 0-14 = číslo tagu
WORD   délka   délka hodnoty v bajtech
B[]    hodnota čísla LE
```

- Tag identifikuje bity 0-14. Bit 15 zapisovač nastaví přesně podle
  sloupce "povinný" v tabulkách této specifikace. U **známého** tagu
  čtečka řídí povinnost tímto sloupcem, ne bitem v souboru (chybně
  zapsaný bit tak nezmění chybu na tiché přeskočení). Bit 15 rozhoduje
  jen u neznámých tagů.
- **Neznámý tag s bitem 15 = 1** je chyba části (kap. 4.4). **Neznámý tag
  s bitem 15 = 0** čtečka přeskočí.
- Známý tag s nečekanou délkou je neplatná hodnota (u povinného chyba
  části, nepovinný se přeskočí).
- Každý tag se v jedné oblasti TLV smí vyskytnout nejvýš jednou, není-li
  u něj uvedeno jinak. Opakování je neplatná hodnota: u povinného tagu
  chyba části, u nepovinného platí první výskyt a další se přeskočí.
- Chybějící tag znamená výchozí hodnotu z tabulky.

### 5.2 Rozsahy čísel tagů

| rozsah | použití |
|---|---|
| 0001h-003Fh | společné tagy části (všechny rodiny) |
| 0040h-007Fh | blokové tagy |
| 0100h-01FFh | rodina FM |
| 0200h-02FFh | FSK |
| 0300h-03FFh | SLOW |
| 0400h-04FFh | DIRECT |
| 0500h-05FFh | MANCHESTER |
| 0600h-06FFh | PILOT |
| ostatní | rezervované |

### 5.3 Společné tagy části

| tag | název | povinný | hodnota | výchozí |
|---|---|---|---|---|
| 0001h | TIMEBASE | ano | DWORD takt v Hz, > 0 | takt CPU z posledního předchozího bloku 0x43, jinak 3 546 895 |
| 0002h | LEVEL | ano | BYTE 1 = první půlvlna HIGH, 0 = první půlvlna LOW | 1 |
| 0003h | PAUSE | ano | DWORD ticho za částí v jednotkách časové základny; volitelně BYTE úroveň během pauzy (0 LOW, 1 HIGH) | 0, LOW |
| 0004h | STOP | ne | BYTE 1 = po části (a pauze) zastavit pásku | 0 |
| 0005h | READER | ne | BYTE rutina, WORD N (kap. 5.6) | bez kontroly |

**Časová základna.** Všechny časy v části (půlvlny, pauza) jsou celá čísla
v jednotkách `1 / TIMEBASE` s. Příklady: ROM v T-stavech CPU (3 546 895 Hz),
TurboCopy v tikách CTC0 (1 108 405 Hz), UniCMT v µs (1 000 000 Hz),
mzftools ve vzorcích (44 100 Hz).

### 5.4 Přehrávání (normativní)

- Pulz = první a druhá půlvlna. Při LEVEL = 1 je první na úrovni HIGH
  a druhá LOW. Při LEVEL = 0 obráceně.
- Pauza začíná po druhé půlvlně posledního pulzu části. Během pauzy je
  úroveň podle PAUSE, výchozí LOW. Při úrovni HIGH (Intercopy drží výstup
  HIGH) následuje po druhé půlvlně posledního pulzu náběžná hrana
  a pauza splyne s první půlvlnou HIGH další části. Při LEVEL = 1 a pauze
  LOW splývá druhá půlvlna posledního pulzu s pauzou (ticho = LOW). To
  odpovídá záznamům ROM i TurboCopy.
- STOP: emulátor po pauze zastaví pásku. Při exportu do WAV se
  ignoruje.
- Úroveň mezi bloky drží player. Blok TZX po pauze (blok TZX s pauzou, 0x20,
  ticho bloku MZ na úrovni LOW) a na začátku přehrávání začne hranou: první
  pulz je HIGH, jako na ZX. TZX 1.20 by ho zapsal LOW a splynul by s pauzou
  (na záznamu Intercopy SINCLAIR je po mezeře vidět celý pilot).
- Převod na výstupní vzorkovací frekvenci: player akumuluje přesný čas od
  začátku části a hranu umístí na vzorek podle akumulovaného času.
  Zaokrouhlovací chyba se tak nesčítá (na rozdíl od zaokrouhlení každé
  půlvlny zvlášť). Úsek x jednotek má
  pak floor nebo ceil(x·rate/timebase) vzorků. Části s READER player
  před převodem zkontroluje s nejhorším zaokrouhlením (kap. 5.6).

### 5.5 Blokové tagy

| tag | název | povinný | hodnota |
|---|---|---|---|
| 0040h | PROFILE_ARGS | ne | ASCII parametry profilu, kap. 8.2 |

### 5.6 READER (kontrola čtecího bodu)

Popisuje, kdo bude část číst, aby nástroje mohly zkontrolovat rezervy
čtecího bodu (tape lint, `tmzinfo --lint`) a player při převodu na
vzorky. Lint počítá s nominálními délkami. Player délky převede na celé
vzorky s nejhorším zaokrouhlením (první půlvlna SHORT ceil, první
půlvlna LONG a perioda SHORT floor). Rezerva pod 0 = blok se nepřehraje
(chyba), rezerva pod 10 µs = varování.

| rutina | význam | model |
|---|---|---|
| 0 | ROM čtecí rutina s konstantou N = 82 (`LD A,82`, 0A4Ah) | vzorek 86+14N+(0..52) T po hraně, podmínky SHORT_H < 86+14N, LONG_H > 138+14N, perioda SHORT > 256+14N (mzcmt_turbo) |
| 1 | táž rutina ROM se změněnou konstantou N (předzavaděč TurboCopy a Intercopy přepíše 0A4Bh) | totéž s N z hodnoty |
| 2 | rutina 1 se zavaděčem FAST IPL V07 (Intercopy), který po každé náběžné hraně přidá 86 T (háček na 061Fh) | jako 1, všechny meze o 86 T dál |
| ostatní | rezervované | lint část přeskočí |

## 6. Rodina FM

FM kóduje bit délkou pulzu: SHORT = 0, LONG = 1. Pokrývá ROM, TurboCopy,
Intercopy NORMAL/TURBO/FASTIPL, CP/M-CMT a BASIC (BSD/BRD).

### 6.1 Tagy

| tag | název | povinný | hodnota | výchozí |
|---|---|---|---|---|
| 0101h | PULSES | ano | 4x DWORD: SHORT první, SHORT druhá, LONG první, LONG druhá půlvlna; každá > 0 | **musí být uveden** |
| 0102h | TONE | ano | DWORD počet SHORT v tónu (GAP) | 0 |
| 0103h | TAPEMARK | ano | WORD počet L, WORD počet S, BYTE počet L za tapemarkem | 0, 0, 0 |
| 0104h | FRAMING | ano | BYTE počet start bitů (LONG) před bajtem, BYTE pořadí bitů (0 = MSB první, 1 = LSB první) | 1, 0 |
| 0105h | CHECKSUM | ano | BYTE algoritmus (kap. 6.5), volitelně za ním bajty checksumu tak, jak se zapíší na pásku | 0 (bez checksumu) |
| 0106h | END | ano | BYTE počet L za checksumem, WORD počet S na konci | 0, 0 |
| 0107h | COPIES | ano | BYTE počet kopií, WORD počet S oddělovače | 0, 0 |
| 0108h | CONTEXT | ano | n x 11 B položek (kap. 6.3); smí se vyskytnout víckrát, položky se sčítají | žádné |

Tón se ukládá vždy **počtem pulzů**. Pravidla jako "tón asi 3,15 s"
(TurboCopy) patří do registru profilů a uplatní se při vytváření souboru.

### 6.2 Posloupnost pulzů (normativní)

Část rodiny FM se přehraje jako posloupnost pulzů. Každý pulz má druh
(S/L) a kontext (kap. 6.3):

1. `TONE` x počet z TONE. Poslední z nich má kontext `TONE_LAST`.
2. `TM_L` x počet L z TAPEMARK (poslední `TM_L_LAST`), `TM_S` x počet S
   (poslední `TM_S_LAST`), `TM_END_L` x počet L za tapemarkem.
3. **Datová sekce:**
   1. pro každý bajt části: `START` (L) x počet start bitů, pak 8 bitů
      v pořadí podle FRAMING. Bity na pozicích 0-6 mají kontext `BIT`,
      bit na pozici 7 má kontext `BIT_LAST`. U posledního bajtu části má
      pozice 7 kontext `BIT_LAST_DATA_END`.
   2. bajty checksumu (kap. 6.5) stejně, s kontexty `CK_START`, `CK_BIT`
      a `CK_BIT_LAST`. U posledního bajtu checksumu má pozice 7 kontext
      `CK_BIT_END`.
   3. `END_L` (L) x první číslo z END, pak `TAIL` (S) x druhé číslo
      (poslední `TAIL_LAST`).
4. Pro každou kopii z COPIES: `COPY_SEP` (S) x počet oddělovače (poslední
   `COPY_SEP_LAST`) a znovu celá datová sekce (bod 3). Tón ani tapemark
   se neopakují.

Pozice znamená pořadí vysílání: pozice 0 je první vyslaný bit (při MSB
první je to bit 7 hodnoty).

### 6.3 Kontexty a délky půlvln

Položka CONTEXT (11 B):

```
BYTE   kontext (tab. níže)
BYTE   druh pulzu: 0 = S, 1 = L
BYTE   druh následujícího pulzu: 0 = S, 1 = L, FFh = libovolný
DWORD  první půlvlna
DWORD  druhá půlvlna
```

| hodnota | kontext | rodič |
|---|---|---|
| 0 | TONE | - |
| 1 | TONE_LAST | TONE |
| 2 | TM_L | - |
| 3 | TM_L_LAST | TM_L |
| 4 | TM_S | - |
| 5 | TM_S_LAST | TM_S |
| 6 | TM_END_L | - |
| 7 | START | - |
| 8 | BIT | - |
| 9 | BIT_LAST | - |
| 10 | BIT_LAST_DATA_END | BIT_LAST |
| 11 | CK_START | START |
| 12 | CK_BIT | BIT |
| 13 | CK_BIT_LAST | BIT_LAST |
| 14 | CK_BIT_END | CK_BIT_LAST |
| 15 | END_L | - |
| 16 | TAIL | - |
| 17 | TAIL_LAST | TAIL |
| 18 | COPY_SEP | - |
| 19 | COPY_SEP_LAST | COPY_SEP |

**Vyhodnocení délek pulzu** s kontextem K, druhem D a druhem následujícího
pulzu X (X neexistuje u posledního pulzu části):
1. položka (K, D, X), pokud X existuje,
2. položka (K, D, libovolný),
3. totéž pro rodiče K, pak pro jeho rodiče,
4. základ z PULSES podle D.

Neznámá hodnota kontextu nebo druhu v CONTEXT je neplatná hodnota (chyba
části). Dvě položky se stejnou trojicí (K, D, X) jsou neplatná hodnota.

Proč kontexty: ROM časuje smyčkou procesoru a délka druhé půlvlny závisí
na tom, kudy kód jde. Například datový bit S má druhou půlvlnu 920 T,
ale 910 T, když následuje L, a bit na pozici 7 má 1028 T. Profily
časované čítačem (TurboCopy) kontexty nepotřebují.

### 6.4 Příklad: ROM-800-EXACT

Základ PULSES = 844, 920, 1664, 1750 (T-stavy, výchozí časová základna) a
22 položek CONTEXT:

| kontext | druh | další | půlvlny |
|---|---|---|---|
| TONE | S | * | 844 / 918 |
| TONE_LAST | S | * | 844 / 913 |
| TM_L | L | * | 1664 / 1728 |
| TM_L_LAST | L | * | 1664 / 1723 |
| TM_S | S | * | 844 / 908 |
| TM_S_LAST | S | * | 844 / 903 |
| TM_END_L | L | * | 1664 / 1861 |
| START | L | S | 1664 / 1726 |
| START | L | L | 1664 / 1716 |
| BIT | S | L | 844 / 910 |
| BIT | L | L | 1664 / 1740 |
| BIT_LAST | S | * | 844 / 1028 |
| BIT_LAST | L | * | 1664 / 1858 |
| BIT_LAST_DATA_END | S | * | 844 / 1041 |
| BIT_LAST_DATA_END | L | * | 1664 / 1871 |
| CK_BIT_LAST | S | * | 844 / 965 |
| CK_BIT_LAST | L | * | 1664 / 1795 |
| CK_BIT_END | S | * | 844 / 926 |
| CK_BIT_END | L | * | 1664 / 1756 |
| END_L | L | * | 1664 / 1733 |
| COPY_SEP | S | * | 844 / 906 |
| COPY_SEP_LAST | S | * | 844 / 1000 |

Struktura: hlavička TONE 22000, TAPEMARK 40/40/1; tělo TONE 11000,
TAPEMARK 20/20/1; obojí CHECKSUM 1, END 1/0, COPIES 1/256.

Ověření: referenční model přehraje tento popis a výsledek se
s referenčním záznamem ROM SAVE z emulátoru shoduje **v každé
půlvlně na T** (108 889 půlvln; srovnává se vše kromě druhé půlvlny
posledního pulzu, která splývá s pauzou). Platí pro emulaci bez modelu
WAIT 8255; na skutečném HW zatím neověřeno.

### 6.5 Checksum

| hodnota | algoritmus | bajty na pásce |
|---|---|---|
| 0 | NONE | žádné |
| 1 | MZ_POPCOUNT16_BE | počet jedničkových bitů v bajtech části, 16 bit, vyšší bajt první (ROM `@CHECK` 071Ah) |
| ostatní | rezervované pro rodiny kap. 7 | - |

Pokud CHECKSUM nese za algoritmem i bajty, player zapíše tyto bajty
místo vypočtených. Jejich počet musí odpovídat algoritmu (pro 1 přesně 2),
jinak jde o neplatnou hodnotu. Slouží k věrnému uložení nahrávky se
špatným checksumem (wav2tmz).

### 6.6 Příklad: TurboCopy V1.21 (TC-1.21)

Časová základna 1 108 405 Hz (tiky CTC0), S = 71 + floor(480·b/a) (model odvozený z měření TurboCopy v emulátoru; pro neměřené rychlosti hypotéza):

```
PULSES   = (S+1)/2, S/2, S, S           (celočíselné dělení; 2/1: 156, 155, 311, 311; 7/3: 138, 138, 276, 276)
TONE     = počet podle S, tabulka níže (např. 1/1: 6344, 2/1: 11239, 7/3: 12664, 8/3: 13925, 3/1: 15130)
TAPEMARK = 40/40/1 (hlavička, hlavička předzavaděče), 20/20/1 (ostatní části)
CHECKSUM = 1, END = 1/99, COPIES = 0, CONTEXT žádný, STOP = 1
```

Loader Y: tři části (LOADER_HEADER inline 128 B a LOADER inline 90 B
v 1/1, BODY v a/b). Loader N: HEADER a BODY v a/b.

Rychlost je zlomek a/b, a i b jsou číslice 1-9, 1/2 <= a/b <= 3 (34 různých poměrů).
Čtecí bod předzavaděče (bajt 4Bh) je floor(82·b/a). Počet pulzů tónu podle S
(změřeno v emulátoru; součin počet × S je 3 495 030 až 3 497 152):

| a/b | S | TONE |
|---|---|---|
| 1/2 | 1031 | 3392 |
| 5/9 | 935 | 3740 |
| 4/7 | 911 | 3838 |
| 3/5 | 871 | 4014 |
| 5/8 | 839 | 4167 |
| 2/3 | 791 | 4420 |
| 5/7 | 743 | 4705 |
| 3/4 | 711 | 4917 |
| 7/9 | 688 | 5081 |
| 4/5 | 671 | 5210 |
| 5/6 | 647 | 5403 |
| 6/7 | 631 | 5540 |
| 7/8 | 619 | 5648 |
| 8/9 | 611 | 5722 |
| 1/1 | 551 | 6344 |
| 9/8 | 497 | 7033 |
| 8/7 | 491 | 7119 |
| 7/6 | 482 | 7252 |
| 6/5 | 471 | 7422 |
| 5/4 | 455 | 7683 |
| 9/7 | 444 | 7873 |
| 4/3 | 431 | 8110 |
| 7/5 | 413 | 8464 |
| 3/2 | 391 | 8940 |
| 8/5 | 371 | 9421 |
| 5/3 | 359 | 9736 |
| 7/4 | 345 | 10131 |
| 9/5 | 337 | 10372 |
| 2/1 | 311 | 11239 |
| 9/4 | 284 | 12307 |
| 7/3 | 276 | 12664 |
| 5/2 | 263 | 13290 |
| 8/3 | 251 | 13925 |
| 3/1 | 231 | 15130 |

Ověření proti referenčním záznamům z emulátoru: struktura
přesně, průměrná odchylka půlvlny 0,000 T, největší odchylka 24,8 T. První
HIGH každé části je v záznamu o 152 až 163 T kratší (náběh čítače). Tento
rozkmit TurboCopy není opakovatelný ani mezi dvěma běhy emulátoru, profil
proto nese nominál.

### 6.7 BASIC (BSD/BRD), profil BASIC-1Z016

Tělo bloku nese kusy za sebou (každý 258 B: ID WORD LE a 256 B dat,
poslední ID FFFFh nese zbytek dat). MZF hlavička je uložená tak, jak ji
program zapsal (`fsize = 0`). Části: HEADER (MZF_HEADER) a pro každý kus
BSD_CHUNK (BODY_SLICE `i·258`, 258), nejvýš 254 kusů.

Profil `BASIC-1Z016` (BASIC 1Z-016 V1.0A, `WOPEN#`/`PRINT#`/`CLOSE#`,
změřeno v emulátoru):

- HEADER: tón 22000, tapemark 40/40, 1 L, data, checksum, 1 L, oddělovač 256 S, kopie, 1 L;
- BSD_CHUNK: tón 11000, tapemark 20/20, jinak stejně;
- časová základna 3 546 895 Hz, kontexty podle tabulky, bez READER a STOP
  (čtecí rutina BASICu není rozebraná);
- pauza LOW za hlavičkou a za každým kusem kromě posledního 6 610 752 T
  (s END_L 1806 T je ticho 6 612 558 T: konec signálu -> zastavení motoru
  1905 T, rozjezd motoru -> další úsek 6 610 653 T); za posledním kusem
  PROFILE_ARGS `pause` (ms, výchozí 1000);
- PROFILE_ARGS: `pause`, `machine`;
- ID kusů profil nekontroluje, bajty jdou do signálu beze změny.

| kontext | bit | další | půlvlny (T, HIGH/LOW) |
|---|---|---|---|
| základ SHORT / LONG | 0 / 1 | - | 868/944, 1739/1825 |
| TONE | 0 | * | 868/942 |
| TONE_LAST | 0 | * | 868/937 |
| TM_L | 1 | * | 1739/1803 |
| TM_L_LAST | 1 | * | 1739/1798 |
| TM_S | 0 | * | 868/932 |
| TM_S_LAST | 0 | * | 868/927 |
| TM_END_L | 1 | * | 1739/1946 |
| START | 1 | 0 / 1 | 1739/1801, 1739/1791 |
| BIT | 0 | 1 | 868/934 |
| BIT | 1 | 1 | 1739/1815 |
| BIT_LAST | 0 / 1 | * | 868/1053, 1739/1934 |
| BIT_LAST_DATA_END | 0 / 1 | * | 868/1061, 1739/1942 |
| CK_BIT_LAST | 0 / 1 | * | 868/989, 1739/1870 |
| CK_BIT_END | 0 / 1 | * | 868/950, 1739/1831 |
| END_L | 1 | * | 1739/1806 |
| COPY_SEP | 0 | * | 868/929 |
| COPY_SEP_LAST | 0 | * | 868/1020 |

Ověření: blok sestavený z dat referenčního záznamu se s tímto záznamem
z emulátoru shoduje přesně na T (kap. 12).

## 7. Ostatní rodiny (rezervované)

Rodiny jsou ve v1.1 rezervované. Jejich tagy definuje budoucí verze
specifikace spolu s jejich implementací. Do té doby zapisovač tyto formáty ukládá
jako 0x41 (v1.0). Níže je rozsah parametrů, který budoucí verze musí pokrýt
(zdroj: kód `src/libs/mzcmt_*`).

### 7.1 FSK, SLOW, DIRECT (mzftools)

- Společné: leader (počet cyklů, délky), polaritní pulz, sync, fade-out
  (počáteční a koncová šířka, krok), XOR checksum.
- FSK: tabulka symbolů pro bit 0/1 podle úrovně rychlosti (7 úrovní).
- SLOW: 4 symboly po 2 bitech (5 úrovní).
- DIRECT: délka bitu, synchro bity po každých 2 datových bitech.
- Časová základna 44 100 Hz: tyto formáty vytváří program v PC, jeho
  originál je signál o 44,1 kHz. Přesné hodnoty se převezmou z kódu
  `mzcmt_fsk/slow/direct` a ze zdrojů mzftools.

### 7.2 MANCHESTER (CPM-TAPE)

Zdroje hodnot: Intercopy V10.2 "CPM" 1500 až 3600 Bd změřený v emulátoru, Pezik/MarVan
TAPE.COM V2.0a výpočtem z T-stavů.

**Bit.** Každý bit má dvě půlky. Při LEVEL = 1 je bit 1 první půlka HIGH,
druhá LOW, bit 0 obráceně (Manchester, změna uprostřed bitu; úroveň PC1
zápisových rutin). LEVEL = 0 úrovně prohodí. Bajty se vysílají LSB první,
bez start a stop bitu.

**Tagy:**

| tag | název | povinný | hodnota | výchozí |
|---|---|---|---|---|
| 0501h | HALVES | ano | 4x DWORD: první půlka bitu 0, první půlka bitu 1, druhá půlka bitu 0, druhá půlka bitu 1 (kontext BIT); každá > 0 | **musí být uveden** |
| 0502h | PILOT | ano | DWORD počet vzorů, BYTE délka vzoru v bitech (1-8), BYTE vzor (bit 0 = první vyslaný bit) | 0 vzorů |
| 0503h | SYNC | ano | BYTE počet bitů (0-8), BYTE bity (bit 0 první) | 0 bitů |
| 0504h | FLAG | ano | BYTE hodnota flagu; přítomnost tagu = flag se vysílá | bez flagu |
| 0505h | CHECKSUM | ano | BYTE algoritmus (0 = žádný, 1 = SUM16_LE), volitelně za ním 2 B checksumu tak, jak se zapíší | 0 |
| 0506h | END | ano | BYTE počet koncových bitů (0-8), BYTE bity (bit 0 první) | 0 bitů |
| 0507h | CARRY | ano | BYTE se znaménkem: úprava druhé půlky v kontextech DATA_FIRST a DATA_BYTE při přenosu součtu (níže) | 0 |
| 0508h | CONTEXT | ano | n x 6 B položek: BYTE kontext, BYTE bit (0, 1, FFh = oba), DWORD druhá půlka; smí se vyskytnout víckrát, položky se sčítají | žádné |

Společné tagy (kap. 5.3) platí; READER se u rodiny MANCHESTER nekontroluje
(lint část přeskočí). Opakovaná dvojice (kontext, bit) v CONTEXT je
neplatná hodnota.

**Posloupnost (normativní):**
1. pilot: vzor PILOT x počet;
2. sync: bity SYNC;
3. flag: 8 bitů hodnoty FLAG (je-li tag uveden);
4. data části (zdroj dat, kap. 4.3.3), každý bajt 8 bitů LSB první;
5. checksum: SUM16_LE = součet datových bajtů (bez flagu), 16 bitů,
   modulo 65536, zapsaný nižší bajt, pak vyšší; s explicitními bajty
   se zapíšou ty;
6. koncové bity END;
7. pauza podle PAUSE (kap. 5.4).

**První půlka** bitu je HALVES podle hodnoty bitu. **Druhá půlka** bitu
závisí na tom, co následuje (kontext), a na hodnotě bitu. Hledá se
položka CONTEXT (kontext, bit), pak (kontext, FFh), jinak HALVES (kontext
BIT) podle hodnoty bitu.

| hodnota | kontext | druhá půlka bitu ... |
|---|---|---|
| 0 | BIT | bitů 0-6 každého bajtu (a výchozí) |
| 1 | PILOT_IN | uvnitř vzoru pilotu |
| 2 | PILOT_NEXT | posledního bitu vzoru, za kterým je další vzor |
| 3 | PILOT_END | posledního bitu pilotu |
| 4 | SYNC_IN | bitu sync, za kterým je další bit sync |
| 5 | SYNC_END | posledního bitu sync |
| 6 | FLAG_END | bitu 7 flagu |
| 7 | DATA_FIRST | bitu 7 bajtu data[0], za kterým je data[1] |
| 8 | DATA_BYTE | bitu 7 bajtu data[k], k >= 1, za kterým je data[k+1] |
| 9 | DATA_END | bitu 7 posledního bajtu dat (délka >= 2) |
| 10 | DATA_SINGLE_END | bitu 7 jediného bajtu dat (délka 1) |
| 11 | CK_MID | bitu 7 nižšího bajtu checksumu |
| 12 | CK_END | bitu 7 vyššího bajtu checksumu |
| 13 | END_IN | koncového bitu, za kterým je další koncový bit |
| 14 | END_LAST | posledního koncového bitu |

Chybí-li úsek (bez sync, flagu, dat nebo checksumu), patří poslednímu
bitu předchozího úseku jeho vlastní koncový kontext (např. bez checksumu
má bit 7 posledního bajtu dat DATA_END). Bit 7 bajtů flagu, dat
a checksumu má vždy kontext podle tabulky, ostatní bity bajtů BIT.

**Přenos.** U DATA_FIRST a DATA_BYTE se k druhé půlce přičte CARRY, když
(nižší bajt součtu data[0..k]) + data[k+1] > FFh (Pezik: přetečení
`ADD A,C` zkrátí smyčku o 1 T). Výsledná druhá půlka musí být > 0.

**Výstup.** Sousední půlky stejné úrovně tvoří jednu půlvlnu (od hrany
k hraně). Pauza je poslední půlvlna s úrovní z PAUSE; stejná úroveň
navazuje (kap. 5.4).

**Profily:** `PEZIK-2.0A` a `IC-10.2-CPM` (kap. 8).
Příklad: TLV části hlavičky `IC-10.2-CPM` (role 1, rodina 5, zdroj 0;
pauza 1 221 688 T, půlky 741 T, pilot 2048 x "011", sync 1, flag 00h,
SUM16_LE, koncový bit 1, kontexty PILOT_END/FLAG_END 740, DATA_END/
DATA_SINGLE_END 739, CK_MID 744, CK_END 745):

```
TIMEBASE  01 80 04 00 0F 1F 36 00
PAUSE     03 80 04 00 38 A4 12 00
HALVES    01 85 10 00 E5 02 00 00 E5 02 00 00 E5 02 00 00 E5 02 00 00
PILOT     02 85 06 00 00 08 00 00 03 06
SYNC      03 85 02 00 01 01
FLAG      04 85 01 00 00
CHECKSUM  05 85 01 00 01
END       06 85 02 00 01 01
CONTEXT   08 85 24 00 03 FF E4 02 00 00 06 FF E4 02 00 00 09 FF E3 02 00 00
                      0A FF E3 02 00 00 0B FF E8 02 00 00 0C FF E9 02 00 00
```

### 7.3 PILOT (Intercopy Sinclair)

Pilot (počet, délka), sync 1 a 2, pulzy bitu 0 a 1, flag bajt, XOR
checksum s flagem, koncový pulz (IC-SINCLAIR). Zůstává v 0x46,
protože jde o MZ soubor s významem; čistě ZX záznamy patří do TZX 0x10/0x11.

## 8. Profily

### 8.1 ID profilu

- ASCII, délka 0 až 32 znaků, povolené znaky `A-Z`, `0-9`, `.` a `-`.
  Jiný znak je chyba struktury bloku.
- Vestavěné profily definuje registr (v knihovně
  modul `tmz_profile`): například `ROM-800-NOMINAL`, `ROM-800-EXACT`,
  `TC-1.21`, `IC-10.2-NORMAL`, `IC-10.2-TURBO`, `IC-10.2-FASTIPL`, `UNICMT`,
  `TAPEMZ-V1-LEGACY`, `PEZIK-2.0A`, `IC-10.2-CPM`, `BASIC-1Z016`,
  `MZ800EMU-CMT`.
- Vlastní profil má prefix `X-` (např. `X-DEMO`). Vestavěné ID prefix `X-`
  nikdy nemají.
- ID je popisek. Player ho nečte. Oprava profilu v registru stávající
  soubory nemění, protože bloky nesou efektivní hodnoty.

### 8.2 PROFILE_ARGS

Parametry, se kterými registr blok vytvořil, jako ASCII řetězec
`klíč=hodnota` oddělený čárkami (klíč `[a-z0-9_]+`, hodnota tisknutelné
ASCII bez čárky). Příklad: `speed=2/1,loader=Y`. Slouží k zobrazení a
validaci, player ho nečte. `speed` je poměr a:b (registr píše a:b, čte i a/b): rychlost
en_CMTSPEED, u TC-1.21 i jiný poměr TurboCopy (číslice 1-9, 1/2 až 3/1). Rychlost
2:1 CPM (sada cmt.com) píše registr jako `2:1 (cp/m)` (jen MZ800EMU-CMT).

### 8.3 Registr a validace

Profil v registru má: ID, rodinu, popis, zdroj a míru jistoty (ověřeno
v emulaci / výpočet / hypotéza / neověřeno), povolené parametry a funkci,
která z MZF a parametrů vytvoří seznam částí. `tmzinfo --lint` umí:
- porovnat blok s výstupem registru pro jeho ID a PROFILE_ARGS a vypsat
  rozdíly (varování, ne chyba),
- zkontrolovat rezervy čtecího bodu u částí s READER (rezerva aspoň 10 µs;
  `mzcmt_turbo_check_readpoint`).

### 8.4 Profil MZ800EMU-CMT

Signál, který emulátor mz800emu hraje z MZF: knihovna mztape, formát
`MZTAPE_FORMATSET_MZ800_SANE`, vstream při taktu GDG MZ-800 17 721 600 Hz.
Profil ho přenáší do bloku 0x46, aby emulátor mohl držet každou pásku jako
bloky 0x46 a MZF hrál stejně jako dřív. Míra jistoty `emulator-output`:
popisuje výstup emulátoru, ne zápis konkrétního programu. Vlastní hodnoty
profil nemá: tóny čte přes `mztape_get_format()`, počty pulzů tapemarku
z maker `MZTAPE_LTM_*` a `MZTAPE_STM_*`, délky pulzů přes
`mztape_get_speed_pulses()`. ID je v makru `TMZ_PROFILE_ID_MZ800EMU_CMT`.

| část | role, zdroj | tón | tapemark | pauza za částí |
|---|---|---|---|---|
| 1 | HEADER, MZF_HEADER | lgap formátu (6400 S) | 40 L, 40 S, 1 L | 0 |
| 2 | BODY, MZF_BODY (i při fsize 0) | sgap formátu (11000 S) | 20 L, 20 S, 1 L | PROFILE_ARGS `pause`, LOW |

Obě části: TIMEBASE 17 721 600, 1 start bit L, MSB první, checksum
MZ_POPCOUNT16_BE, END_L 3 (stop bit posledního bajtu checksumu a 2 L
záznamu mztape), bez kopií, bez READER a STOP.

PULSES (v tikách 17 721 600 Hz):
- `speed=a:b` (výchozí 1:1, libovolný poměr en_CMTSPEED včetně 2:1 CPM):
  délky z `mztape_get_speed_pulses()` (sada Intercopy změřená v emulátoru,
  u 2:1 CPM sada cmt.com), vydělené poměrem, `round(sekundy * 17 721 600)`.
  Při 1:1 LONG 8335/8760, SHORT 4356/4930 tiků.
- `cmtspeed=LH/LL/SH/SL` (µs, všechna čtyři nenulová, jen bez `speed`):
  `round(µs * 17,7216)`.

PROFILE_ARGS: `speed=a:b,pause=N` (2:1 CPM jako `speed=2:1 (cp/m)`), nebo
`cmtspeed=LH/LL/SH/SL,pause=N`; případně `machine`. Jiný parametr
(`gap`, `loader`, `bd`, LEGACY) vrátí chybu parametrů.

Ověření (`tests/test_tmz_profile.c`, MZF s tělem 1 KB, `pause=0`): při
17 721 600 Hz je přehraný blok běh po běhu shodný s
`mztape_create_cmt_vstream_from_mztmzf()` pro všechny platné poměry
a s `mztape_create_cmt_vstream_from_mztmzf_pulses()` pro 470/494/240/278
a 235/247/120/139 µs (55 864 běhů, rozdíl 0 vzorků). Při 14 336 640 Hz
(takt GDG MZ-700 a MZ-1500) se běhy liší nejvýš o 1 vzorek: mztape
zaokrouhluje každou půlvlnu zvlášť, model FM akumuluje čas části
(kap. 5.4) z tiků zaokrouhlených na 17 721 600 Hz.

Tělo 0 B: část BODY vznikne i tak (tón, tapemark, žádná data, checksum
0000, závěr), stejně jako v mztape; program, který po hlavičce čte tělo,
by jinak čekal na tapemark. Test porovnává i tento případ s mztape běh po
běhu (1:1, 2:1 CPM, 3:1).

Export MZT (kap. 10.3): rychlost UniCMT je `cmtspeed`, jinak jako 0x41
NORMAL (`floor(1x / dělitel)`); 2:1 CPM strukturu ROM nemá.

## 9. Kompatibilita s v1.0

### 9.1 Čtení

Čtečka v1.1 převede bloky v1.0 v paměti na 0x46 a dál je zpracuje stejně
jako 0x46 (jedna cesta playeru). Výjimkou jsou formáty, jejichž rodina
není implementovaná (kap. 7), a případy, které model FM nevyjádří
(tabulka 9.2). Ty se přehrají původní cestou v1.0. Bloky 0x41 CPM_TAPE
a 0x45 se převádějí vždy, cestu v1.0 nemají; co z nich
převést nejde, se nepřehraje (chyba bloku).

Bloky přehrané cestou v1.0 používají současné pulzní sady kodérů (mztape), které se od historických generátorů v1.0 mohou lišit nejvýš o 2 vzorky při 44,1 kHz (největší rozdíl půlvlny je SHORT LOW 278,2 proti 249 µs při 1:1, tj. 1,29 vzorku); přesný výstup v1.0 reprodukuje jen převod na blok 0x46 (profil `TAPEMZ-V1-LEGACY`).

### 9.2 Mapování

| blok v1.0 | podmínka | výsledek |
|---|---|---|
| 0x40 | pulsní sada MZ-800 | profil `ROM-800-NOMINAL` s výchozími parametry registru, tóny podle Intercopy nebo TurboCopy (přepínatelné). |
| 0x40 | pulsní sada MZ-700 nebo MZ-80B | `TAPEMZ-V1-LEGACY` s pulzní sadou z bloku, dokud ROM-700 a MZ-80B nejsou ověřené. |
| 0x41 | format TURBO | `TC-1.21` s rychlostí z bloku a předzavaděčem; tóny, koncovka, pauza a pulzy podle měření TurboCopy v emulátoru. Vlastní pulzy, kopie těla, sada MZ-80B a rychlost, kterou TurboCopy neumí, se nepřevádějí. |
| 0x41 | format NORMAL, rychlost 1:1 | `TAPEMZ-V1-LEGACY` s parametry, které použil player v1.0 |
| 0x41 | format FASTIPL, rychlost 1:1, 2:1, 7:3, 8:3 | `IC-10.2-FASTIPL` s `bd` 1200, 2400, 2800, 3200; časování Intercopy V10.2 změřené v emulátoru. Jiné rychlosti, vlastní pulzy, tón těla jiný než 5500 a sada MZ-80B se nepřevádějí (cesta v1.0). |
| 0x45 | nejvýš 254 kusů | `BASIC-1Z016` vždy (i bez převodu v1.0 v playeru); stroj do PROFILE_ARGS, pulzní sada se neuplatní. |
| 0x41 | format NORMAL, když player má formát záznamu MZ800 (kopie), nebo rychlost jiná než 1:1 | nepřevádí se, přehraje se cestou v1.0 (model FM nevyjádří první bajt kopie bez start bitu; 2:1 CPM má pulzy cmt.com; ostatní rychlosti: zavedení přes ROM ověřit nejde a převod mění délku záznamu systematicky) |
| 0x45 | víc než 254 kusů | nepřevádí se a nepřehraje (chyba bloku; blok 0x46 má nejvýš 255 částí) |
| 0x41 | format CPM_TAPE | `PEZIK-2.0A`: úroveň rychlosti 2 -> `bd=3200`, jinak `bd=2400` (úroveň 0 "1200" neexistuje, neplatné hodnoty se hrají jako 2400); vlastní pulzy, lgap a flags bloku se ignorují; tělo nad 16 384 B se nepřevede (chyba bloku, profil PEZIK-2.0A ho odmítá). |
| 0x41 | format FSK, SLOW, DIRECT | nepřevádí se, přehraje se cestou v1.0 |
| 0x41 | format SINCLAIR | nepřevádí se a nepřehraje (záznam ZX Spectrum patří do bloků TZX 0x10/0x11) |
| 0x42, 0x44 | - | nepodporované (kap. 9.3) |

Pole "Pauza po bloku" (ms) se převede na PAUSE poslední části (přepočet
na časovou základnu části). "Cílový
stroj" a "Pulsní sada" se uloží do PROFILE_ARGS.

Parametry playeru v1.0, které převod používá, jsou historické hodnoty generátorů uvedené v kap. 5.2 specifikace v1.0 (`tmz-specification.md`); drží je profil sám a současné pulzní sady kodérů se od nich mohou lišit.

Převod `TAPEMZ-V1-LEGACY` se ověřuje testem proti generátoru v1.0:
stejný počet a stejné úrovně půlvln, délka každé půlvlny ve vzorcích
se smí lišit nejvýš o 1. Generátory v1.0 zaokrouhlují
každou půlvlnu zvlášť, model FM akumuluje čas části (kap. 5.4), bajtová
shoda WAV proto není cílem. Čitelnost se ověřuje zavedením v emulátoru.

### 9.3 Bloky 0x42 a 0x44

V projektu je nic nezapisuje a player v1.0 je nepřehrával. Ve v1.1
se nepřevádějí: čtečka je přeskočí s varováním a `tmzconv --upgrade` je
ponechá beze změny s varováním.

### 9.4 Převod souboru

`tmzconv --upgrade` převede soubor v1.0 na v1.1 podle kap. 9.2 a zvýší
verzi v hlavičce. Zápis v1.0 se nepodporuje (formát nemá jiné čtečky než
tento projekt).

## 10. MZT (vstup a výstup)

MZT není součástí TMZ, je to vstupní a výstupní formát jako MZF a WAV.

### 10.1 Formát

Posloupnost MZF souborů (128 B hlavička + tělo délky `fsize`). Rozšíření
UniCMT vkládá mezi soubory 128 B hlavičky **CMTSPEED**:

```
Offset  Obsah
00h     00h (typ souboru)
01h     "CMTSPEED" + 0Dh
12h     0000h, 0000h, 0000h (fsize, fstrt, fexec; tělo není)
20h     01h (význam neznámý, hypotéza: verze)
30h     4x WORD LE: LONG první, LONG druhá, SHORT první, SHORT druhá půlvlna v µs
ostatní 00h
```

Pravidla UniCMT: rychlost platí pro všechny další soubory až do další
hlavičky. Hlavička na prvním místě se nepřehraje (pozorováno, příčina
neznámá).

### 10.2 Import (mzf2tmz)

- Čtečka pozná CMTSPEED sama (typ 00h, jméno `CMTSPEED` + 0Dh, fsize 0).
  Klasické MZT je případ bez hlaviček.
- Každý soubor se stane blokem 0x46 s profilem `UNICMT`, časovou
  základnou 1 MHz a pulzy z poslední předchozí CMTSPEED (bez ní 1x: 470,
  494, 240, 278 µs).
- Strukturu signálu UniCMT (tóny, tapemarky) zatím neznáme. Profil
  `UNICMT` ji v registru vede jako `[neověřeno]`: převzal strukturu
  ROM-800-NOMINAL (tóny podle `gap`, kopie, pauzy), PULSES v µs
  z PROFILE_ARGS `cmtspeed=LH/LL/SH/SL`, READER (ROM, N = 82) jen při 1x.
- CMTSPEED na prvním místě MZT se nepoužije (varování), CMTSPEED bez
  dalšího souboru také ne [neověřeno, jak se k nim staví UniCMT].
- Vstup MZT s výslovnými volbami formátu nástroje bez CMTSPEED se převede
  podle voleb; s CMTSPEED a jinou volbou než profil UNICMT je to chyba
  (rychlosti by se ztratily).

### 10.3 Export (tmz2mzf)

| přepínač | výstup |
|---|---|
| `--mzt` (výchozí pro MZT) | klasické MZT, jen MZF za sebou. Časování se ztratí; nástroj vypíše varování u bloků, které neměly standardní rychlost nebo strukturu ROM. |
| `--mzt-unicmt` | MZT s CMTSPEED všude, kde se mění pulzy. Bajt 20h = 01h, ostatní bajty podle 10.1. První soubor musí mít 1x; jinak nástroj odmítne (hlavička nesmí být první). Bloky bez struktury ROM (hlavička + tělo FM, např. předzavaděč TurboCopy) nástroj odmítne nebo vypíše varování. |

Hlavička CMTSPEED vygenerovaná podle 10.1 je bajtově shodná se souborem
`2xspeed.mzf` z UniCMT (vektor V5).

Rychlost bloku pro export: 0x40 a profily ROM = 1x; 0x41 NORMAL
a TAPEMZ-V1-LEGACY normal = floor(1x / dělitel rychlosti), tedy 2:1 a 3:1
přesně 2x a 3x UniCMT; UNICMT = `cmtspeed`; MZ800EMU-CMT = `cmtspeed`, bez
něj jako 0x41 NORMAL (2:1 CPM ne, kap. 8.4). Ostatní bloky nemají
strukturu ROM: `--mzt` je zapíše s varováním, `--mzt-unicmt` odmítne.
Hlavičky MZF jdou do MZT bajt po bajtu z bloku. MZT převedené tam a zpět
(`mzf2tmz`, `tmz2mzf --mzt-unicmt`) je bajtově shodné s ukázkami UniCMT
`interkar_2x.mzt` a `interkar_3x.mzt` (test_tmz_mzt).

## 11. Testovací vektory

Výpisy níže vytvořil referenční model, offsety jsou od začátku bloku
(za hlavičkou souboru `TapeMZ!` 1Ah 01h 01h).

| vektor | obsah | ověření |
|---|---|---|
| V1 `v1-rom-800-nominal.tmz` | ROM-800-NOMINAL v µs, tělo 4 B, tóny jako ROM (22000/11000, ne výchozí tóny registru) | struktura (výpis níže) |
| V2 `v2-tc-2to1-loader-n.tmz` | TC-1.21 2/1 Loader N, emutest.mzf | model = referenční záznam z emulátoru |
| V3 `v3-tc-3to1-loader-y.tmz` | TC-1.21 3/1 Loader Y, předzavaděč inline | model = referenční záznam z emulátoru |
| V4 `v4-custom-x.tmz` | vlastní profil `X-DEMO`, výřez těla, 2 kontexty | struktura |
| V5 `v5-mzt-cmtspeed.mzt` | MZT: soubor + CMTSPEED 2x + soubor | CMTSPEED = UniCMT `2xspeed.mzf` bajtově |
| V6 `v6-rom-800-exact.tmz` | ROM-800-EXACT, 22 kontextů, emutest.mzf | model = referenční záznam z emulátoru v každé půlvlně |

Vektor FASTIPL zatím není součástí sady (šablona hlavičky
`$BB` se převezme z `mzcmt_fastipl`).

### 11.1 V1 - ROM-800-NOMINAL

```
0000  46                                               ID bloku 0x46
0001  32 01 00 00                                      délka zbytku bloku = 306
0005  01                                               verze bloku
0006  0f                                               délka ID profilu = 15
0007  52 4f 4d 2d 38 30 30 2d 4e 4f 4d 49 4e 41 4c     ID profilu "ROM-800-NOMINAL"
0016  01 56 32 54 45 53 54 0d ... (128 B)              MZF hlavička
0096  04 00 00 00                                      délka těla = 4
009A  3e 01 c9 00                                      tělo
009E  00 00                                            délka blokových TLV = 0
00A0  02                                               počet částí = 2
00A1  4b 00 00 00                                      část 0: délka 75
00A5  01 01 00                                         role 1, rodina 1, zdroj 0
00A8  01 80 04 00                                      TLV TIMEBASE (tag 0001h, povinný), délka 4
00AC  40 42 0f 00                                        = 1000000
00B0  03 80 04 00                                      TLV PAUSE (tag 0003h, povinný), délka 4
00B4  40 42 0f 00                                        = 1000000
00B8  01 81 10 00                                      TLV PULSES (tag 0101h, povinný), délka 16
00BC  f0 00 00 00 16 01 00 00 d6 01 00 00 ee 01 00 00    = SH 240, SL 278, LH 470, LL 494
00CC  02 81 04 00                                      TLV TONE (tag 0102h, povinný), délka 4
00D0  f0 55 00 00                                        = 22000
00D4  03 81 05 00                                      TLV TAPEMARK (tag 0103h, povinný), délka 5
00D8  28 00 28 00 01                                     = 40 L, 40 S, 1 L za tapemarkem
00DD  05 81 01 00                                      TLV CHECKSUM (tag 0105h, povinný), délka 1
00E1  01                                                 = algoritmus 1
00E2  06 81 03 00                                      TLV END (tag 0106h, povinný), délka 3
00E6  01 00 00                                           = 1 L, 0 S koncovka
00E9  07 81 03 00                                      TLV COPIES (tag 0107h, povinný), délka 3
00ED  01 00 01                                           = 1 kopie, oddělovač 256 S
00F0  43 00 00 00                                      část 1: délka 67
00F4  02 01 01                                         role 2, rodina 1, zdroj 1
00F7  01 80 04 00                                      TLV TIMEBASE (tag 0001h, povinný), délka 4
00FB  40 42 0f 00                                        = 1000000
00FF  01 81 10 00                                      TLV PULSES (tag 0101h, povinný), délka 16
0103  f0 00 00 00 16 01 00 00 d6 01 00 00 ee 01 00 00    = SH 240, SL 278, LH 470, LL 494
0113  02 81 04 00                                      TLV TONE (tag 0102h, povinný), délka 4
0117  f8 2a 00 00                                        = 11000
011B  03 81 05 00                                      TLV TAPEMARK (tag 0103h, povinný), délka 5
011F  14 00 14 00 01                                     = 20 L, 20 S, 1 L za tapemarkem
0124  05 81 01 00                                      TLV CHECKSUM (tag 0105h, povinný), délka 1
0128  01                                                 = algoritmus 1
0129  06 81 03 00                                      TLV END (tag 0106h, povinný), délka 3
012D  01 00 00                                           = 1 L, 0 S koncovka
0130  07 81 03 00                                      TLV COPIES (tag 0107h, povinný), délka 3
0134  01 00 01                                           = 1 kopie, oddělovač 256 S
```

### 11.2 V4 - vlastní profil

```
0000  46                                               ID bloku 0x46
0001  ed 00 00 00                                      délka zbytku bloku = 237
0005  01                                               verze bloku
0006  06                                               délka ID profilu = 6
0007  58 2d 44 45 4d 4f                                ID profilu "X-DEMO"
000D  01 56 32 54 45 53 54 0d ... (128 B)              MZF hlavička
008D  04 00 00 00                                      délka těla = 4
0091  3e 01 c9 00                                      tělo
0095  00 00                                            délka blokových TLV = 0
0097  01                                               počet částí = 1
0098  56 00 00 00                                      část 0: délka 86
009C  02 01 02                                         role 2, rodina 1, zdroj 2
009F  00 00 00 00 04 00 00 00                          výřez: offset 0, délka 4
00A7  01 81 10 00                                      TLV PULSES (tag 0101h, povinný), délka 16
00AB  20 03 00 00 84 03 00 00 40 06 00 00 a4 06 00 00    = SH 800, SL 900, LH 1600, LL 1700
00BB  02 81 04 00                                      TLV TONE (tag 0102h, povinný), délka 4
00BF  a0 0f 00 00                                        = 4000
00C3  03 81 05 00                                      TLV TAPEMARK (tag 0103h, povinný), délka 5
00C7  14 00 14 00 01                                     = 20 L, 20 S, 1 L za tapemarkem
00CC  05 81 01 00                                      TLV CHECKSUM (tag 0105h, povinný), délka 1
00D0  01                                                 = algoritmus 1
00D1  06 81 03 00                                      TLV END (tag 0106h, povinný), délka 3
00D5  01 00 00                                           = 1 L, 0 S koncovka
00D8  08 81 16 00                                      TLV CONTEXT (tag 0108h, povinný), délka 22
00DC  09 00 ff 20 03 00 00 e8 ... (22 B)                 = BIT_LAST S->* 800/1000; BIT_LAST L->* 1600/1800
```

### 11.3 V3 - TurboCopy 3/1 Loader Y

```
0000  46                                               ID bloku 0x46
0001  75 06 00 00                                      délka zbytku bloku = 1653
0005  01                                               verze bloku
0006  07                                               délka ID profilu = 7
0007  54 43 2d 31 2e 32 31                             ID profilu "TC-1.21"
000E  01 45 4d 55 54 45 53 54 ... (128 B)              MZF hlavička
008E  00 04 00 00                                      délka těla = 1024
0092  18 fe 00 01 02 03 04 05 ... (1024 B)             tělo
0492  16 00                                            délka blokových TLV = 22
0494  40 00 12 00                                      TLV PROFILE_ARGS (tag 0040h), délka 18
0498  73 70 65 65 64 3d 33 2f ... (18 B)                 = "speed=3/1,loader=Y"
04AA  03                                               počet částí = 3
04AB  cd 00 00 00                                      část 0: délka 205
04AF  03 01 03                                         role 3, rodina 1, zdroj 3
04B2  80 00 00 00                                      inline délka = 128
04B6  01 45 4d 55 54 45 53 54 ... (128 B)              inline bajty
0536  01 80 04 00                                      TLV TIMEBASE (tag 0001h, povinný), délka 4
053A  b5 e9 10 00                                        = 1108405
053E  03 80 04 00                                      TLV PAUSE (tag 0003h, povinný), délka 4
0542  9a d0 01 00                                        = 118938
0546  04 00 01 00                                      TLV STOP (tag 0004h), délka 1
054A  01                                                 = 01
054B  01 81 10 00                                      TLV PULSES (tag 0101h, povinný), délka 16
054F  14 01 00 00 13 01 00 00 27 02 00 00 27 02 00 00    = SH 276, SL 275, LH 551, LL 551
055F  02 81 04 00                                      TLV TONE (tag 0102h, povinný), délka 4
0563  c8 18 00 00                                        = 6344
0567  03 81 05 00                                      TLV TAPEMARK (tag 0103h, povinný), délka 5
056B  28 00 28 00 01                                     = 40 L, 40 S, 1 L za tapemarkem
0570  05 81 01 00                                      TLV CHECKSUM (tag 0105h, povinný), délka 1
0574  01                                                 = algoritmus 1
0575  06 81 03 00                                      TLV END (tag 0106h, povinný), délka 3
0579  01 63 00                                           = 1 L, 99 S koncovka
057C  ae 00 00 00                                      část 1: délka 174
0580  04 01 03                                         role 4, rodina 1, zdroj 3
0583  5a 00 00 00                                      inline délka = 90
0587  3e 08 d3 ce e5 21 00 00 ... (90 B)               inline bajty
05E1  01 80 04 00                                      TLV TIMEBASE (tag 0001h, povinný), délka 4
05E5  b5 e9 10 00                                        = 1108405
05E9  03 80 04 00                                      TLV PAUSE (tag 0003h, povinný), délka 4
05ED  6f 01 02 00                                        = 131439
05F1  04 00 01 00                                      TLV STOP (tag 0004h), délka 1
05F5  01                                                 = 01
05F6  05 00 03 00                                      TLV READER (tag 0005h), délka 3
05FA  00 52 00                                           = rutina 0, N = 82
05FD  01 81 10 00                                      TLV PULSES (tag 0101h, povinný), délka 16
0601  14 01 00 00 13 01 00 00 27 02 00 00 27 02 00 00    = SH 276, SL 275, LH 551, LL 551
0611  02 81 04 00                                      TLV TONE (tag 0102h, povinný), délka 4
0615  c8 18 00 00                                        = 6344
0619  03 81 05 00                                      TLV TAPEMARK (tag 0103h, povinný), délka 5
061D  14 00 14 00 01                                     = 20 L, 20 S, 1 L za tapemarkem
0622  05 81 01 00                                      TLV CHECKSUM (tag 0105h, povinný), délka 1
0626  01                                                 = algoritmus 1
0627  06 81 03 00                                      TLV END (tag 0106h, povinný), délka 3
062B  01 63 00                                           = 1 L, 99 S koncovka
062E  48 00 00 00                                      část 2: délka 72
0632  02 01 01                                         role 2, rodina 1, zdroj 1
0635  01 80 04 00                                      TLV TIMEBASE (tag 0001h, povinný), délka 4
0639  b5 e9 10 00                                        = 1108405
063D  04 00 01 00                                      TLV STOP (tag 0004h), délka 1
0641  01                                                 = 01
0642  05 00 03 00                                      TLV READER (tag 0005h), délka 3
0646  01 1b 00                                           = rutina 1, N = 27
0649  01 81 10 00                                      TLV PULSES (tag 0101h, povinný), délka 16
064D  74 00 00 00 73 00 00 00 e7 00 00 00 e7 00 00 00    = SH 116, SL 115, LH 231, LL 231
065D  02 81 04 00                                      TLV TONE (tag 0102h, povinný), délka 4
0661  1a 3b 00 00                                        = 15130
0665  03 81 05 00                                      TLV TAPEMARK (tag 0103h, povinný), délka 5
0669  14 00 14 00 01                                     = 20 L, 20 S, 1 L za tapemarkem
066E  05 81 01 00                                      TLV CHECKSUM (tag 0105h, povinný), délka 1
0672  01                                                 = algoritmus 1
0673  06 81 03 00                                      TLV END (tag 0106h, povinný), délka 3
0677  01 63 00                                           = 1 L, 99 S koncovka
```

### 11.4 V5 - hlavička CMTSPEED (2x)

```
0000  00 43 4d 54 53 50 45 45 44 0d 00 00 00 00 00 00
0010  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0020  01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0030  eb 00 f7 00 78 00 8b 00 00 00 00 00 00 00 00 00
0040  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0050  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0060  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0070  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

## 12. Ověření

- Formát: zápis a čtení 0x46 tam a zpět pro všechny zdroje dat; neznámý
  povinný a nepovinný tag; poškozené vstupy (délky přes blok, výřez mimo
  tělo, P = 0, ID delší než 32) končí chybou bez čtení mimo buffer.
- Generátor FM proti referenčním záznamům z emulátoru: ROM-800-EXACT shoda na T;
  TC-1.21 struktura přesně, průměrná odchylka do 0,05 tiku, každá půlvlna
  kromě první v části do ±30 T; BASIC-1Z016 shoda na T (pauzy srovnání
  nehlídá, protože záznam z emulátoru obsahuje i dobu stojícího motoru: kontroluje je test
  parametrů v test_tmz_profile).
- Kompatibilita v1.0: půlvlny převodu TAPEMZ-V1-LEGACY proti generátoru v1.0 (počet a úrovně shodné, délka ±1 vzorek, kap. 9.2), zavedení v emulátoru.
- MZT: klasický export a import tam a zpět; `--mzt-unicmt` (umístění
  CMTSPEED, první soubor 1x, bajtová shoda s UniCMT); import ukázek
  UniCMT `interkar_*.mzt`.
- Emulátor: zavedení souborů v1.1 v emulátoru.
- Lint: rezervy čtecího bodu (TurboCopy 2:1 OK). Player: TurboCopy 8:3
  a 3:1 při 22 050 Hz odmítnuto, 2:1 při 22 050 Hz varování.
- HW: logický analyzátor na výstupu CMT (zatím neprovedeno).
