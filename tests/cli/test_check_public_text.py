"""Testy vzorů kontrolního skriptu veřejných textů (plánovací odkazy nesmí na GitHub)."""
import importlib.util
import os
import sys

sys.dont_write_bytecode = True   # žádný __pycache__ ve stromu tests/
HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("cpt", os.path.join(HERE, "..", "check_public_text.py"))
cpt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cpt)

BAD = [
    "/* plán 17: CPM-TAPE */",
    " * (revize C4, ověřeno v emulátoru)",
    " * docs/analyza-2026-10-17/r8-bsd.md kap. B",
    " * viz r7-cpm-tape.md",
    " * (E10: Intercopy pauza)",
    " * rozhodnutí D3",
    " * nález N6",
    " * nemělo oporu (M4/L3): ROM",
    " * KB reference/agent/os/basic-mz800",
    " * mz800-knowledge/public",
    " * záznam hwlog z emulátoru",
    " * docs/emu-verify-nalezy.md",
    " * (Michal 2026-10-18)",
    " * rozhodl Michal",
    " * plan 18 converts",
    " * docs/tmz-v2-specification.md kap. 9",
    " * (docs/existing-mzformats.md)",
    "/** @brief Rezervované ID (spec v1.1 tab. bloků, D14). */",
    "    double hook = 0;   /* háček zavaděče FAST IPL V07, R3 2.4) */",
    " * @brief Rychlost Intercopy V10.2 (R3 2.1, ověřeno E10).",
    " * předvolby v módu 3 obvodu 8253, E6), LONG S a S.",
    "        /* úroveň nízká (rozhodnutí 2026-10-18) */",
    " * se vždy (plány 17 a 18).",
    " * opraveno plánem 19",
    "    x = y;   /* W3: začátek záznamu */",
    "    # D17: veřejná kopie nemá tstdata/",
    " * @brief  Regresní test W1: FM dekodér",
    "/** @brief T4: celý soubor, useknutá data */",
    " * stroji ověřuje devtools/emu-verify (emulátor)",
]
GOOD = [
    "#define TONE 0xC4",
    "    LD A,0xD3",
    "    out ( 0xE5 ), a",
    " * změřeno v emulátoru (záznam hran PC1)",
    " * BASIC 1Z-016 V1.0A, WOPEN#/PRINT#",
    "uint8_t e10 = 0;",
    " * DIRECT, D400h, C4h",
    " * Intercopy V10.2",
    "see dist-docs/cz/tools-wav2tmz.md",
    "viz docs/cz/tmz-v1.1-specification.md",
    "tělo nad 16 KB se nepřehraje",
    "body of 16384 bytes (16 KB parts)",
    " * - mzftools direct.asm (Z80 dekoder)",
    " * a kod loaderu (V02 nebo V07).",
    "\"to_mzf from 0x46 (V2)\" );",
    "- Fixed attribution: TURBO = TurboCopy (Michal Kreidl),",
    "Knihovna je napsaná v C (C99/C11).",
    "C11, -Wall -Wextra",
    " * DIRECT: datové bity D0-D7, D7 první",
    "-include devtools/emu-verify/emu-verify.mk",
]
fails = 0
for s in BAD:
    if not cpt.find(s):
        print("FAIL: not detected: %r" % s)
        fails += 1
for s in GOOD:
    if cpt.find(s):
        print("FAIL: false positive: %r -> %s" % (s, cpt.find(s)))
        fails += 1
print("=== check_public_text patterns: %s ===" % ("FAIL" if fails else "PASS"))
sys.exit(1 if fails else 0)
