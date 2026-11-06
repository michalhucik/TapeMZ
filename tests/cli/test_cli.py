"""Kontroly CLI nástrojů (mzf2tmz, tmz2mzf, wav2tmz, tmzedit, ...).

Použití: python tests/cli/test_cli.py [EXE_SUFFIX [BUILD_DIR]] z libovolného
adresáře. BUILD_DIR je adresář s binárkami (výchozí <repozitář>/build),
make test ho předává. Dočasné soubory jsou v BUILD_DIR/temp/cli, testovací
data v <repozitář>/tstdata. Výpis je anglicky.
"""
import os
import shutil
import struct
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SUFFIX = sys.argv[1] if len(sys.argv) > 1 else ""
BUILD = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.join(REPO, "build")
TMP = os.path.join(BUILD, "temp", "cli")
TSTDATA = os.path.join(REPO, "tstdata")
fails = 0


def exe(name):
    """Cesta k binárce utility v build/."""
    return os.path.join(BUILD, name + SUFFIX)


def run_tool(name, *args, cwd=None):
    """Spustí utilitu, vrátí (návratový kód, stdout + stderr)."""
    r = subprocess.run([exe(name)] + list(args), cwd=cwd, capture_output=True, text=True, errors="replace")
    return r.returncode, r.stdout + r.stderr


def check(cond, msg, out=""):
    """Vypíše OK/FAIL a při selhání výstup utility."""
    global fails
    print(("OK: " if cond else "FAIL: ") + msg)
    if not cond:
        fails += 1
        print("   " + out.strip().replace("\n", "\n   "))


def mzf(byte11=0x00):
    """MZF s tělem 4 B, jménem ABCDEFGHIJKLMNOP a zadaným bajtem 11h."""
    h = bytearray(128)
    h[0] = 1
    h[1:17] = b"ABCDEFGHIJKLMNOP"
    h[0x11] = byte11
    struct.pack_into("<HHH", h, 0x12, 4, 0x1200, 0x1200)
    return bytes(h) + b"\x01\x02\x03\x04"


def read(path):
    """Obsah souboru, nebo b"" když neexistuje."""
    return open(path, "rb").read() if os.path.exists(path) else b""


def mzf2tmz(*args):
    """mzf2tmz se smazaným výstupem z předchozí kontroly."""
    if os.path.exists(OUT):
        os.remove(OUT)
    return run_tool("mzf2tmz", *args)


# bez binárek nic nemazat ani nezakládat (špatný BUILD_DIR nebo nesestavený projekt)
for tool in ("mzf2tmz", "tmz2mzf", "tmz2wav", "wav2tmz"):
    if not os.path.isfile(exe(tool)):
        print("FAIL: %s not found (build the project or pass BUILD_DIR)" % exe(tool))
        sys.exit(1)

shutil.rmtree(TMP, ignore_errors=True)
os.makedirs(TMP)
SRC = os.path.join(TMP, "t.mzf")
OUT = os.path.join(TMP, "o.tmz")
MZT = os.path.join(TMP, "t.mzt")
open(SRC, "wb").write(mzf())
open(MZT, "wb").write(mzf(0x0D) + mzf(0x0D))

# --- mzf2tmz: hlavička MZF u profilů
rc, o = mzf2tmz(SRC, OUT, "--profile", "ROM-800-EXACT")
d = read(OUT)
i = d.find(b"ABCDEFGHIJKLMNOP")
check(rc == 0 and i > 0 and d[i + 16] == 0x00, "profile block keeps header byte 11h = 00h", o)
rc, o = mzf2tmz(SRC, OUT)
d = read(OUT)
i = d.find(b"ABCDEFGHIJKLMNOP")
check(rc == 0 and i > 0 and d[i + 16] == 0x00, "v1.0 block (no --profile) keeps header byte 11h = 00h", o)

# --- mzf2tmz: volby, které profil nepoužívá (LEGACY)
rc, o = mzf2tmz(SRC, OUT, "--profile", "ROM-800-NOMINAL", "--loader", "n")
check(rc != 0 and "--loader" in o, "--loader with ROM profile rejected", o)
rc, o = mzf2tmz(SRC, OUT, "--profile", "TC-1.21", "--gap", "rom")
check(rc != 0 and "--gap" in o, "--gap with TC-1.21 rejected", o)
rc, o = mzf2tmz(SRC, OUT, "--profile", "IC-10.2-TURBO", "--gap", "short")
check(rc != 0 and "--gap" in o, "--gap with IC rejected", o)
rc, o = mzf2tmz(SRC, OUT, "--profile", "IC-10.2-TURBO", "--speed", "2:1")
check(rc != 0 and "--bd" in o, "--speed with IC rejected, points to --bd", o)
rc, o = mzf2tmz(SRC, OUT, "--profile", "UNICMT", "--speed", "2:1")
check(rc != 0 and "CMTSPEED" in o, "--speed with UNICMT rejected, explains CMTSPEED", o)
rc, o = mzf2tmz(SRC, OUT, "--profile", "TAPEMZ-V1-LEGACY")
check(rc != 0 and "tmzconv" in o, "TAPEMZ-V1-LEGACY rejected with explanation", o)

# --- mzf2tmz: --machine s profilem
rc, o = mzf2tmz(SRC, OUT, "--profile", "ROM-800-NOMINAL", "--machine", "mz700")
check(rc == 0 and b"machine=mz700" in read(OUT) and "Machine  : MZ-700" in o,
      "--machine goes to PROFILE_ARGS and summary", o)

# --- mzf2tmz: MZT
rc, o = mzf2tmz(MZT, OUT, "--gap", "rom")
check(rc == 0 and read(OUT).count(b"gap=rom") == 2, "MZT default UNICMT accepts --gap", o)
if os.path.isfile(os.path.join(TSTDATA, "popis.txt")):
    v5 = read(os.path.join(TSTDATA, "tmz-v2", "v5-mzt-cmtspeed.mzt"))
    only = os.path.join(TMP, "only.mzt")
    open(only, "wb").write(v5[132:260])
    rc, o = mzf2tmz(only, OUT)
    check(len(v5) >= 260 and rc != 0 and "no MZF file" in o, "MZT with only CMTSPEED: clear message", o)
else:
    # veřejná kopie nemá tstdata/
    print("SKIP: MZT with only CMTSPEED (needs tstdata/, not in the public repository)")
rc, o = mzf2tmz(os.path.join(TMP, "missing.mzt"), OUT)
check(rc != 0 and "cannot open" in o, "missing MZT: cannot open", o)

# --- mzf2tmz: --gap u MZT s volbou formátu
rc, o = mzf2tmz(MZT, OUT, "--gap", "rom", "--machine", "mz700")
check(rc != 0 and "--machine" in o and "--profile UNICMT" in o,
      "MZT --gap with --machine explains format options", o)
rc, o = mzf2tmz(MZT, OUT, "--profile", "UNICMT", "--gap", "rom", "--machine", "mz700")
d = read(OUT)
check(rc == 0 and d.count(b"gap=rom") == 2 and d.count(b"machine=mz700") == 2,
      "MZT --profile UNICMT with --gap and --machine", o)
rc, o = mzf2tmz(SRC, OUT, "--gap", "rom")
check(rc != 0 and "--gap requires --profile" in o, "MZF --gap without --profile keeps the old message", o)
rc, o = mzf2tmz(MZT, OUT, "--loader", "n")
check(rc != 0 and "--loader and --bd require --profile" in o, "MZT --loader without --profile keeps the old message", o)

# --- mzf2tmz: SINCLAIR v 0x41, FSK/SLOW/DIRECT pro MZ-80B jen s varováním
rc, o = mzf2tmz(SRC, OUT, "--format", "sinclair")
check(rc != 0 and "tap2tmz" in o, "--format sinclair rejected, points to TZX blocks", o)
for fmt in ("fsk", "slow", "direct"):
    for mach in (None, "mz700", "mz800", "mz1500"):
        args = (SRC, OUT, "--format", fmt) + (("--machine", mach) if mach else ())
        rc, o = mzf2tmz(*args)
        check(rc == 0 and "not verified" not in o, "--format %s --machine %s works without a warning" % (fmt, mach), o)
    rc, o = mzf2tmz(SRC, OUT, "--format", fmt, "--machine", "mz80b")
    check(rc == 0 and "not verified" in o and "MZ-80B" in o, "--format %s --machine mz80b warns" % fmt, o)

# --- tmz2mzf: souhrn se selhaným blokem
two = os.path.join(TMP, "two.tmz")
for _ in range(2):
    run_tool("mzf2tmz", SRC, two, "--profile", "ROM-800-NOMINAL")
d = bytearray(read(two))
i = d.find(b"ABCDEFGHIJKLMNOP")
d[i + 17] = 5   # fsize 5 při těle 4 B: první blok nejde do MZF
open(two, "wb").write(d)
xdir = os.path.join(TMP, "x")
os.makedirs(xdir)
rc, o = run_tool("tmz2mzf", two, cwd=xdir)
check(rc != 0 and "Extracted 1 MZF file(s)" in o and "1 failed" in o,
      "tmz2mzf summary counts only extracted files, reports failures", o)

# --- tmz2wav: zastavení pásky
tzx = os.path.join(TMP, "stop.tzx")
tone = bytes([0x12]) + struct.pack("<HH", 1000, 10)
open(tzx, "wb").write(b"ZXTape!\x1a\x01\x14" + tone + bytes([0x20, 0, 0]) + tone)
wav = os.path.join(TMP, "stop.wav")
rc, o = run_tool("tmz2wav", tzx, wav)
check(rc == 0 and "STOP TAPE" in o and os.path.exists(wav) and os.path.getsize(wav) > 44,
      "tmz2wav reports Pause 0 as a tape stop and keeps going", o)

# --- tmz2wav: chyba bloku = rc 1, WAV se uloží
bad = os.path.join(TMP, "bad.tzx")
csw = bytes([0x19]) + struct.pack("<I", 14) + bytes(14)   # 0x19 Generalized Data: player ho nepřehraje
open(bad, "wb").write(b"ZXTape!\x1a\x01\x14" + tone + csw + tone)
badwav = os.path.join(TMP, "bad.wav")
rc, o = run_tool("tmz2wav", bad, badwav)
check(rc != 0 and os.path.exists(badwav) and "1 block(s) failed" in o,
      "tmz2wav: a failed block gives rc 1, WAV still written", o)
rc, o = run_tool("tmz2wav", bad, badwav, "--blocks", "0")
check(rc == 0 and "failed to play" not in o,
      "tmz2wav --blocks: an unselected failed block does not fail the run", o)

# --- wav2tmz: bloky bez MZF v režimu MZF
if os.path.isfile(os.path.join(TSTDATA, "popis.txt")):
    sdir = os.path.join(TMP, "sinclair")
    os.makedirs(sdir)
    rc, o = run_tool("wav2tmz", os.path.join(TSTDATA, "ic-sinclair-all.wav"), "-o", os.path.join(sdir, "x.mzf"))
    check(rc != 0 and "skipping" in o and not os.listdir(sdir),
          "wav2tmz MZF mode: SINCLAIR blocks reported, rc 1 when nothing saved", o)
    rc, o = run_tool("wav2tmz", os.path.join(TSTDATA, "ic-sinclair-all.wav"))
    check(rc == 0, "wav2tmz analysis only (no -o): rc 0", o)
else:
    print("SKIP: wav2tmz SINCLAIR checks (need tstdata/, not in the public repository)")

# --- wav2tmz: roundtrip MZF -> TMZ -> WAV -> MZF
def mzf_big(name=b"ROUNDTRIP", fstrt=0x1200, fsize=1024, cmnt=b""):
    """MZF s tělem fsize B (vzor i*7+3), jménem a komentářem."""
    h = bytearray(128)
    h[0] = 1
    h[1:1 + len(name)] = name
    h[1 + len(name)] = 0x0D
    struct.pack_into("<HHH", h, 0x12, fsize, fstrt, fstrt)
    h[0x18:0x18 + len(cmnt)] = cmnt
    return bytes(h) + bytes((i * 7 + 3) & 0xFF for i in range(fsize))


def roundtrip(name, mzf_bytes, mzf2tmz_args, tmz2wav_args=()):
    """mzf2tmz + tmz2wav + wav2tmz -o; vrátí (rc, výstup, tělo prvního MZF, počet souborů)."""
    d = os.path.join(TMP, "rt-" + name)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    src, tmz, wav, out = (os.path.join(d, n) for n in ("in.mzf", "x.tmz", "x.wav", "out.mzf"))
    open(src, "wb").write(mzf_bytes)
    rc1, o1 = run_tool("mzf2tmz", src, tmz, *mzf2tmz_args)
    rc2, o2 = run_tool("tmz2wav", tmz, wav, *tmz2wav_args)
    rc3, o3 = run_tool("wav2tmz", wav, "-o", out)
    files = sorted(f for f in os.listdir(d) if f.startswith("out") and f.endswith(".mzf"))
    body = read(os.path.join(d, files[0]))[128:] if files else None
    n = 0
    for line in o3.splitlines():
        if line.startswith("Files decoded:"):
            n = int(line.split(":")[1])
    return rc1 or rc2 or rc3, o1 + o2 + o3, body, n


# Bajt 11h hlavičky beze změny celou cestou: MZF -> mzf2tmz -> (tmz2mzf | tmz2wav -> wav2tmz
# -> tmz2mzf), výsledné MZF bajtově shodné se vstupem (11h = 00h jako ROM SAVE z paměti)
def mzf_term(term):
    b = bytearray(mzf_big(name=b"TERM", fsize=256))
    b[0x11] = term
    return bytes(b)


for term in (0x00, 0x0D):
    src_b = mzf_term(term)
    for tag, args in (("normal", ()), ("turbo", ("--format", "turbo", "--speed", "2:1")),
                      ("rom", ("--profile", "ROM-800-NOMINAL")), ("tc-y", ("--profile", "TC-1.21", "--loader", "y")),
                      ("iccpm", ("--profile", "IC-10.2-CPM", "--bd", "3200")), ("fsk", ("--format", "fsk"))):
        d = os.path.join(TMP, "term-%s-%02x" % (tag, term))
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        src, tmz, wav, tmz2, m1, m2 = (os.path.join(d, n) for n in ("in.mzf", "x.tmz", "x.wav", "y.tmz", "a.mzf", "b.mzf"))
        open(src, "wb").write(src_b)
        rc1, o1 = run_tool("mzf2tmz", src, tmz, *args)
        rc2, o2 = run_tool("tmz2mzf", tmz, "--output", m1, "--overwrite")
        rc3, o3 = run_tool("tmz2wav", tmz, wav)
        rc4, o4 = run_tool("wav2tmz", wav, "--output-format", "tmz", "-o", tmz2)
        rc5, o5 = run_tool("tmz2mzf", tmz2, "--output", m2, "--overwrite")
        a, b = read(m1), read(m2)
        check(rc1 == 0 and rc2 == 0 and a == src_b, "11h = %02Xh kept: mzf2tmz %s -> tmz2mzf" % (term, tag),
              o1 + o2 + ("11h=%02X" % a[0x11] if len(a) > 0x11 else ""))
        # bajtová shoda nestačí: dekodér při chybném checksumu MZF vrátí s varováním
        crc_ok = "header=OK body=OK" in o4 and "CRC mismatch" not in o4 and "ERROR" not in o4
        check(rc3 == 0 and rc4 == 0 and rc5 == 0 and b == src_b and crc_ok,
              "11h = %02Xh kept: %s -> tmz2wav -> wav2tmz -> tmz2mzf" % (term, tag),
              o3 + o4 + o5 + ("11h=%02X diff=%s" % (b[0x11], [i for i in range(min(len(b), len(src_b))) if b[i] != src_b[i]][:8])
                              if len(b) > 0x11 else ""))

# UniCMT export bloku v1.0 (0x40) s 11h = 00h: hlavička v MZT shodná se zdrojem
d = os.path.join(TMP, "term-unicmt")
shutil.rmtree(d, ignore_errors=True)
os.makedirs(d)
src_b = mzf_term(0x00)
open(os.path.join(d, "in.mzf"), "wb").write(src_b)
rc1, o1 = run_tool("mzf2tmz", os.path.join(d, "in.mzf"), os.path.join(d, "x.tmz"))
rc2, o2 = run_tool("tmz2mzf", os.path.join(d, "x.tmz"), "--mzt-unicmt", "--output", os.path.join(d, "x.mzt"), "--overwrite")
got = read(os.path.join(d, "x.mzt"))
check(rc1 == 0 and rc2 == 0 and got == src_b, "11h = 00h kept: v1.0 block -> tmz2mzf --mzt-unicmt", o1 + o2)

m = mzf_big(cmnt=b"(C) NIPSOFT")
rc, o, body, n = roundtrip("nipsoft", m, ("--format", "normal"))
check(n == 1 and body == m[128:], "NORMAL with NIPSOFT in the comment decodes", o)
m = mzf_big(fstrt=0xD400, fsize=90)
rc, o, body, n = roundtrip("d400", m, ("--format", "normal"))
check(n == 1 and body == m[128:], "NORMAL program at D400h, 90 B, no signature decodes", o)
# velikosti předzavaděčů FSK (191), SLOW (249) a DIRECT (360): bez dat
# FSK/SLOW/DIRECT za nimi zůstane záznam NORMAL
for fsize in (191, 249, 360):
    m = mzf_big(fstrt=0xD400, fsize=fsize)
    rc, o, body, n = roundtrip("d400-%d" % fsize, m, ("--format", "normal"))
    check(n == 1 and body == m[128:], "NORMAL program at D400h, %d B, no signature decodes" % fsize, o)

# --- wav2tmz: TURBO z vlastních nástrojů
m = mzf_big()
for label, args in (("turbo-1to1", ("--format", "turbo", "--speed", "1:1")),
                    ("turbo-2to1", ("--format", "turbo", "--speed", "2:1")),
                    ("turbo-3to2", ("--format", "turbo", "--speed", "3:2")),
                    ("tc121", ("--profile", "TC-1.21", "--speed", "2:1")),
                    ("ic-turbo-1200", ("--profile", "IC-10.2-TURBO", "--bd", "1200"))):
    rc, o, body, n = roundtrip(label, m, args)
    check(n == 1 and body == m[128:], "wav2tmz decodes %s" % label, o)
m0 = mzf_big(fsize=0)
rc, o, body, n = roundtrip("turbo-empty", m0, ("--format", "turbo", "--speed", "3:1"))
check(n == 1 and body == b"", "wav2tmz decodes TURBO with an empty body", o)

# --- wav2tmz: Intercopy 2800/3200 a FASTIPL při různých frekvencích
m = mzf_big()
for label, args, rate in (("icf-3200-44k", ("--profile", "IC-10.2-FASTIPL", "--bd", "3200"), "44100"),
                          ("icf-2800-32k", ("--profile", "IC-10.2-FASTIPL", "--bd", "2800"), "32000"),
                          ("icf-2400-22k", ("--profile", "IC-10.2-FASTIPL", "--bd", "2400"), "22050"),
                          ("icf-3200-22k", ("--profile", "IC-10.2-FASTIPL", "--bd", "3200"), "22050"),
                          ("fastipl-8to3", ("--format", "fastipl", "--speed", "8:3"), "44100"),
                          ("ict-2800", ("--profile", "IC-10.2-TURBO", "--bd", "2800"), "44100"),
                          ("ict-3200", ("--profile", "IC-10.2-TURBO", "--bd", "3200"), "44100"),
                          ("icf-2400-44k", ("--profile", "IC-10.2-FASTIPL", "--bd", "2400"), "44100")):
    rc, o, body, n = roundtrip(label, m, args, ("--rate", rate))
    check(n == 1 and body == m[128:], "wav2tmz decodes %s" % label, o)


# --- mzf2tmz: CPM-TAPE přes profil PEZIK-2.0A
src = os.path.join(TMP, "cpm.mzf")
open(src, "wb").write(mzf_big(name=b"CPM", fsize=256))
for label, args, expect in (("cpm-tape", ("--format", "cpm-tape"), "PEZIK-2.0A (bd=2400"),
                            ("cpm-tape 3200", ("--format", "cpm-tape", "--bd", "3200"), "PEZIK-2.0A (bd=3200"),
                            ("profile PEZIK 3200", ("--profile", "PEZIK-2.0A", "--bd", "3200"), "PEZIK-2.0A (bd=3200"),
                            ("profile IC-10.2-CPM", ("--profile", "IC-10.2-CPM"), "IC-10.2-CPM (bd=2400")):
    out = os.path.join(TMP, "cpm-%s.tmz" % label.replace(" ", "_"))
    rc, o = run_tool("mzf2tmz", src, out, *args)
    rc2, o2 = run_tool("tmzinfo", out)
    check(rc == 0 and rc2 == 0 and ("Profile : " + expect) in o2, "mzf2tmz %s -> %s" % (label, expect), o + o2)
rc, o = run_tool("mzf2tmz", src, os.path.join(TMP, "cpm-bad.tmz"), "--format", "cpm-tape", "--speed", "2:1")
check(rc == 1 and "--bd" in o, "mzf2tmz --format cpm-tape --speed rejected with --bd hint", o)
# wav2tmz: CPM-TAPE z nahrávky -> blok 0x46 podle rozpoznané varianty
for label, args, expect in (("ic-cpm", ("--profile", "IC-10.2-CPM"), "IC-10.2-CPM (bd=2400"),
                            ("pezik-3200", ("--format", "cpm-tape", "--bd", "3200"), "PEZIK-2.0A (bd=3200")):
    d = os.path.join(TMP, "w2t-" + label)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    tmz, wav, out = (os.path.join(d, n) for n in ("x.tmz", "x.wav", "y.tmz"))
    rc1, o1 = run_tool("mzf2tmz", src, tmz, *args)
    rc2, o2 = run_tool("tmz2wav", tmz, wav)
    rc3, o3 = run_tool("wav2tmz", wav, "--output-format", "tmz", "-o", out)
    rc4, o4 = run_tool("tmzinfo", out)
    check(rc1 == 0 and rc2 == 0 and rc3 == 0 and ("Profile : " + expect) in o4,
          "wav2tmz CPM-TAPE %s -> 0x46 %s" % (label, expect), o1 + o2 + o3 + o4)
# wav2tmz: TURBO/FASTIPL nesmí propadnout do větve CPM-TAPE; zapisovač se pozná
# a zapíše profilem 0x46
for label, args, expect in (("turbo", ("--profile", "TC-1.21"), "Profile : TC-1.21 (speed=2:1,loader=Y"),
                            ("fastipl", ("--profile", "IC-10.2-FASTIPL", "--bd", "2400"),
                             "Profile : IC-10.2-FASTIPL (bd=2400")):
    d = os.path.join(TMP, "w2t-" + label)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    tmz, wav, out = (os.path.join(d, n) for n in ("x.tmz", "x.wav", "y.tmz"))
    rc1, o1 = run_tool("mzf2tmz", src, tmz, *args)
    rc2, o2 = run_tool("tmz2wav", tmz, wav)
    rc3, o3 = run_tool("wav2tmz", wav, "--output-format", "tmz", "-o", out)
    rc4, o4 = run_tool("tmzinfo", out)
    check(rc1 == 0 and rc2 == 0 and rc3 == 0 and expect in o4 and "PEZIK" not in o4,
          "wav2tmz %s -> %s" % (label, expect), o3 + o4)
# tmzedit: blok 0x41 nelze přepnout na cpm-tape (zapisuje se jako 0x46)
t41 = os.path.join(TMP, "cpm-41.tmz")
rc, o = run_tool("mzf2tmz", src, t41, "--format", "turbo")
rc2, o2 = run_tool("tmzedit", "set", t41, "0", "--format", "cpm-tape", "-o", os.path.join(TMP, "cpm-41b.tmz"))
check(rc == 0 and rc2 == 1 and "mzf2tmz" in o2, "tmzedit set --format cpm-tape rejected with mzf2tmz hint", o + o2)


# --- wav2tmz: předzavaděč TurboCopy bez části TURBO, za ním nahrávka NORMAL
# tapemark LTM cizí hlavičky se nesmí vzít jako TURBO.
def wav_pcm8(path):
    """Vrátí (hlavička do 'data' včetně, vzorky) 8bitového mono WAV z tmz2wav."""
    b = read(path)
    k = b.index(b"data") + 8
    return b[:k], b[k:]


def save_pcm8(path, head, samples):
    h = bytearray(head)
    h[4:8] = (len(h) - 8 + len(samples)).to_bytes(4, "little")
    h[-4:] = len(samples).to_bytes(4, "little")
    open(path, "wb").write(bytes(h) + samples)


d = os.path.join(TMP, "splice")
shutil.rmtree(d, ignore_errors=True)
os.makedirs(d)
parts = {}
for key, name, args in (("a", b"AAA", ("--profile", "TC-1.21", "--speed", "2:1")),
                        ("b", b"BBB", ())):
    src, tmz, wav = (os.path.join(d, key + e) for e in (".mzf", ".tmz", ".wav"))
    mb = mzf_big(name=name, fsize=100)
    open(src, "wb").write(mb)
    run_tool("mzf2tmz", src, tmz, *args)
    run_tool("tmz2wav", tmz, wav)
    parts[key] = (mb, wav_pcm8(wav))
mb, (head, sb) = parts["b"]
sa = parts["a"][1][1]
# konec předzavaděče = začátek 2. pauzy (běh >= 4000 stejných vzorků): za
# hlavičkou je 1., za tělem předzavaděče 2.; část TURBO se odřízne
runs, i = [], 0
while i < len(sa):
    j = i
    while j < len(sa) and sa[j] == sa[i]:
        j += 1
    if j - i >= 4000:
        runs.append(i)
    i = j
cut = runs[1] if len(runs) > 1 else len(sa)
for label, gap in (("silence", bytes([128]) * 13230), ("no-gap", b"")):
    wav = os.path.join(d, "splice-%s.wav" % label)
    save_pcm8(wav, head, sa[:cut] + gap + sb)
    for f in os.listdir(d):
        if f.startswith("out"):
            os.remove(os.path.join(d, f))
    rc, o = run_tool("wav2tmz", wav, "-o", os.path.join(d, "out.mzf"))
    got = [read(os.path.join(d, f)) for f in sorted(os.listdir(d)) if f.startswith("out") and f.endswith(".mzf")]
    check(any(g[1:4] == b"BBB" and g[128:] == mb[128:] for g in got),
          "NORMAL after a TurboCopy preloader without TURBO part decodes (%s)" % label, o)

# --- BSD jako blok 0x46 BASIC-1Z016 ---
bdir = os.path.join(TMP, "bsd")
os.makedirs(bdir)
data = bytes(range(256)) * 3 + b"END\x1a"             # 772 B -> 4 kusy
p_dat = os.path.join(bdir, "in18.dat")
with open(p_dat, "wb") as f:
    f.write(data)
p_tmz = os.path.join(bdir, "d18.tmz")
rc, o = run_tool("dat2bsd", p_dat, p_tmz, "--name", "DATA")
check(rc == 0, "dat2bsd rc 0", o)
rc, info = run_tool("tmzinfo", p_tmz)
check("ID 0x46" in info and "BASIC-1Z016" in info, "dat2bsd writes block 0x46 BASIC-1Z016", info)
check("Type     : 0x04" in info, "dat2bsd default ftype 0x04", info)
rc, o = run_tool("bsd2dat", p_tmz)                   # výchozí výstup vedle vstupu: bsd/d18.dat
ext_path = os.path.join(bdir, "d18.dat")
ext = read(ext_path) if os.path.exists(ext_path) else b""
check(rc == 0 and len(ext) > 0, "bsd2dat reads 0x46", o)
check(ext[:len(data)] == data and len(ext) == 4 * 256, "dat2bsd -> bsd2dat roundtrip (padded to 256)", o)
rc, o = run_tool("dat2bsd", p_dat, os.path.join(bdir, "s.tmz"), "--speed", "2:1")
check(rc != 0 and "BASIC-1Z016" in o, "dat2bsd --speed rejected", o)
rc, o = run_tool("dat2bsd", p_dat, os.path.join(bdir, "s.tmz"), "--pulseset", "700")
check(rc != 0 and "BASIC-1Z016" in o, "dat2bsd --pulseset rejected", o)
p_big = os.path.join(bdir, "big.dat")
with open(p_big, "wb") as f:
    f.write(bytes(254 * 256 + 1))
rc, o = run_tool("dat2bsd", p_big, os.path.join(bdir, "big.tmz"))
check(rc != 0 and not os.path.exists(os.path.join(bdir, "big.tmz")), "dat2bsd over 254 chunks rejected", o)
p_empty = os.path.join(bdir, "e.dat")
with open(p_empty, "wb") as f:
    pass
rc, o = run_tool("dat2bsd", p_empty, os.path.join(bdir, "e.tmz"))
check(rc == 0, "dat2bsd empty input -> 1 chunk", o)
rc, o = run_tool("tmz2wav", os.path.join(bdir, "e.tmz"), os.path.join(bdir, "e.wav"))
check(rc == 0 and os.path.exists(os.path.join(bdir, "e.wav")), "empty BSD plays", o)
# bsd2dat bez --output zapisuje vedle vstupu, ne do aktuálního adresáře
sub = os.path.join(bdir, "sub")
os.makedirs(sub)
shutil.copy(p_tmz, os.path.join(sub, "x.tmz"))
rc, o = run_tool("bsd2dat", os.path.join(sub, "x.tmz"), cwd=bdir)
check(rc == 0 and os.path.exists(os.path.join(sub, "x.dat")) and not os.path.exists(os.path.join(bdir, "x.dat")),
      "bsd2dat output next to input", o)
v10 = os.path.join(TSTDATA, "bsd-v10-data300.tmz")
if os.path.exists(v10):
    # bsd2dat čte i v1.0 blok 0x45
    rc, o = run_tool("bsd2dat", v10, "--output", os.path.join(bdir, "v10.dat"))
    got = read(os.path.join(bdir, "v10.dat")) if os.path.exists(os.path.join(bdir, "v10.dat")) else b""
    check(rc == 0 and got[:4] == b" 1\r ", "bsd2dat reads 0x45", o)
    # wav2tmz zapíše BSD jako 0x46
    wt = os.path.join(bdir, "w.tmz")
    rc, o = run_tool("wav2tmz", os.path.join(TSTDATA, "basic-bsd.wav"), "-o", wt, "--output-format", "tmz")
    rc2, info = run_tool("tmzinfo", wt)
    check(rc == 0 and "ID 0x46" in info and "BASIC-1Z016" in info, "wav2tmz writes BSD as 0x46 BASIC-1Z016", o + info)
else:
    print("SKIP: BSD checks with tstdata (need tstdata/, not in the public repository)")

# --- tmzedit validate kontroluje blok 0x45 ---
if os.path.exists(v10):
    rc, o = run_tool("tmzedit", "validate", v10)
    check(rc == 0 and "Result: VALID" in o, "validate valid 0x45", o)
    raw = bytearray(read(v10))
    last = len(raw) - 258                      # kusy jsou na konci bloku i souboru
    check(raw[last] == 0xFF and raw[last + 1] == 0xFF, "fixture: last chunk id FFFF")
    raw[last] = 0x05
    raw[last + 1] = 0x00
    bad = os.path.join(bdir, "bad45.tmz")
    with open(bad, "wb") as f:
        f.write(bytes(raw))
    rc, o = run_tool("tmzedit", "validate", bad)
    check(rc == 1 and "Result: INVALID" in o and "FFFF" in o, "validate 0x45 without FFFF chunk -> INVALID", o)
    raw2 = bytearray(read(v10))
    raw2[len(raw2) - 6 * 258] = 0x07           # ID prvního kusu 0007h
    bad2 = os.path.join(bdir, "bad45b.tmz")
    with open(bad2, "wb") as f:
        f.write(bytes(raw2))
    rc, o = run_tool("tmzedit", "validate", bad2)
    check(rc == 1 and "Result: INVALID" in o, "validate 0x45 chunk id out of sequence -> INVALID", o)
else:
    print("SKIP: tmzedit validate 0x45 (needs tstdata/, not in the public repository)")

# --- jednotka vlastních pulzů bloku 0x41 je 0,1 us ---
rc, o = run_tool("tmzedit", "set")
check("0.1 us" in o and "us*100" not in o, "tmzedit set usage: custom pulse unit is 0.1 us", o)
# --- SINCLAIR patří do TZX bloku 0x11; tmzedit ho do bloku 0x41 nezapíše ---
st = os.path.join(TMP, "sinc.tmz")
rc, o = run_tool("mzf2tmz", SRC, st, "--format", "turbo")
rc, info = run_tool("tmzinfo", st)
idx = "0"
for line in info.splitlines():
    if "ID 0x41" in line:
        idx = line.split("]")[0].split("[")[1].strip()
rc, o = run_tool("tmzedit", "set", st, idx, "--format", "sinclair", "-o", os.path.join(TMP, "sinc2.tmz"))
check(rc != 0 and "0x11" in o and "--sinclair-speed" in o, "tmzedit set --format sinclair rejected for block 0x41", o)
check("sinclair," not in run_tool("tmzedit", "set")[1], "tmzedit set usage does not offer format sinclair")

# --- soubor bez těla: Body CRC N/A, ne OK ---
hdr = bytearray(128); hdr[0] = 1; hdr[1:6] = b"EMPTY"; hdr[6] = 0x0D
p0 = os.path.join(TMP, "empty.mzf")
with open(p0, "wb") as f:
    f.write(bytes(hdr))
t0 = os.path.join(TMP, "empty.tmz"); w0 = os.path.join(TMP, "empty.wav")
for p in (t0, w0):
    if os.path.exists(p):
        os.remove(p)
rc, o = run_tool("mzf2tmz", p0, t0)
rc, o = run_tool("tmz2wav", t0, w0)
rc, o = run_tool("wav2tmz", w0)
check("Body CRC: N/A" in o, "header-only file reports Body CRC N/A", o)
rc, o = run_tool("wav2tmz", w0, "--verbose")
check("body CRC=N/A" in o and "body CRC=ERROR" not in o, "header-only file: --verbose also reports body CRC N/A", o)

# --- wav2tmz: identifikace formátu a rychlosti (TMZ výstup)
def ident(name, mzf_bytes, mzf2tmz_args, tmz2wav_args=()):
    """mzf2tmz + tmz2wav + wav2tmz --output-format tmz + tmzinfo.

    Vrátí (rc_tvorby, rc_wav2tmz, výpis tmzinfo, výstup wav2tmz). rc_tvorby != 0
    znamená, že WAV nevznikl (např. player kombinaci odmítl jako nečitelnou).
    """
    d = os.path.join(TMP, "id-" + name)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    src, tmz, wav, out = (os.path.join(d, n) for n in ("in.mzf", "x.tmz", "x.wav", "y.tmz"))
    open(src, "wb").write(mzf_bytes)
    rc1, o1 = run_tool("mzf2tmz", src, tmz, *mzf2tmz_args)
    rc2, o2 = run_tool("tmz2wav", tmz, wav, *tmz2wav_args) if rc1 == 0 else (1, "")
    if rc1 or rc2:
        return rc1 or rc2, 1, "", o1 + o2
    rc3, o3 = run_tool("wav2tmz", wav, "--output-format", "tmz", "-o", out)
    info = run_tool("tmzinfo", out)[1] if os.path.exists(out) else ""
    return 0, rc3, info, o3


# FSK 0-6 a SLOW 0-4: úroveň z bajtu zpoždění v zavaděči, podle frekvence WAV
m = mzf_big(fsize=256)
for fmt, opt, levels in (("fsk", "--fsk-speed", range(7)), ("slow", "--slow-speed", range(5))):
    for rate in ("44100", "48000"):
        for lvl in levels:
            rcm, rc3, info, o = ident("%s%d-%s" % (fmt, lvl, rate), m, ("--format", fmt, opt, str(lvl)),
                                      ("--rate", rate))
            if rcm:
                continue        # player kombinaci odmítl jako nečitelnou (změřeno v emulátoru)
            check(rc3 == 0 and ("%s level %d " % (fmt.upper(), lvl)) in info,
                  "wav2tmz %s level %d @ %s Hz identified" % (fmt.upper(), lvl, rate), o + info)

# wav2tmz: zapisovač TurboCopy a Intercopy -> profil bloku 0x46 se správnou rychlostí
m = mzf_big(fsize=512)
for label, args, expect in (
        ("tc-2to1-y", ("--profile", "TC-1.21", "--speed", "2:1", "--loader", "y"), "TC-1.21 (speed=2:1,loader=Y"),
        ("tc-2to1-n", ("--profile", "TC-1.21", "--speed", "2:1", "--loader", "n"), "TC-1.21 (speed=2:1,loader=N"),
        ("tc-9to4-y", ("--profile", "TC-1.21", "--speed", "9:4", "--loader", "y"), "TC-1.21 (speed=9:4,loader=Y"),
        ("tc-9to4-n", ("--profile", "TC-1.21", "--speed", "9:4", "--loader", "n"), "TC-1.21 (speed=9:4,loader=N"),
        ("tc-1to2-n", ("--profile", "TC-1.21", "--speed", "1:2", "--loader", "n"), "TC-1.21 (speed=1:2,loader=N"),
        ("tc-5to6-y", ("--profile", "TC-1.21", "--speed", "5:6", "--loader", "y"), "TC-1.21 (speed=5:6,loader=Y"),
        ("tc-1to1-n", ("--profile", "TC-1.21", "--speed", "1:1", "--loader", "n"), "TC-1.21 (speed=1:1,loader=N"),
        ("icn-600", ("--profile", "IC-10.2-NORMAL", "--bd", "600"), "IC-10.2-NORMAL (bd=600"),
        ("icn-2400", ("--profile", "IC-10.2-NORMAL", "--bd", "2400"), "IC-10.2-NORMAL (bd=2400"),
        # NORMAL 2800/3200: nesouměrné půlvlny tónu, leader se hledá i po periodách
        ("icn-2800", ("--profile", "IC-10.2-NORMAL", "--bd", "2800"), "IC-10.2-NORMAL (bd=2800"),
        ("icn-3200", ("--profile", "IC-10.2-NORMAL", "--bd", "3200"), "IC-10.2-NORMAL (bd=3200"),
        ("ict-600", ("--profile", "IC-10.2-TURBO", "--bd", "600"), "IC-10.2-TURBO (bd=600"),
        ("ict-2400", ("--profile", "IC-10.2-TURBO", "--bd", "2400"), "IC-10.2-TURBO (bd=2400"),
        ("ict-3200", ("--profile", "IC-10.2-TURBO", "--bd", "3200"), "IC-10.2-TURBO (bd=3200"),
        ("icf-600", ("--profile", "IC-10.2-FASTIPL", "--bd", "600"), "IC-10.2-FASTIPL (bd=600"),
        ("icf-3200", ("--profile", "IC-10.2-FASTIPL", "--bd", "3200"), "IC-10.2-FASTIPL (bd=3200")):
    rcm, rc3, info, o = ident(label, m, args)
    check(rcm == 0 and rc3 == 0 and ("Profile : " + expect) in info, "wav2tmz writer %s -> %s" % (label, expect),
          o + info)
# NORMAL 1:1 z ROM i ROM-800-NOMINAL s krátkými tóny zůstanou blokem 0x40
for label, args in (("normal-1to1", ("--format", "normal")), ("rom-nominal", ("--profile", "ROM-800-NOMINAL"))):
    rcm, rc3, info, o = ident(label, m, args)
    check(rcm == 0 and rc3 == 0 and "ID 0x40" in info and "Profile" not in info,
          "wav2tmz %s stays block 0x40" % label, o + info)

# komentář MZF za předzavaděčem TurboCopy: cmnt[0..6] nese tělo předzavaděče (53h-59h),
# hlavička předzavaděče má na jeho místě signaturu; po dekódování musí být původní
m = mzf_big(fsize=300, cmnt=b"COMMENT-TAPEMZ")
for label, args in (("tc", ("--profile", "TC-1.21", "--speed", "2:1", "--loader", "y")),
                    ("ic", ("--profile", "IC-10.2-TURBO", "--bd", "2400"))):
    rcm, rc3, info, o = ident("cmnt-" + label, m, args)
    out = os.path.join(TMP, "id-cmnt-" + label, "y.tmz")
    xm = os.path.join(TMP, "id-cmnt-" + label, "y.mzf")
    run_tool("tmz2mzf", out, "--output", xm, "--overwrite")
    got = read(xm) if os.path.exists(xm) else b""
    same = len(got) >= 128 and got[:128] == m[:128]
    check(rcm == 0 and rc3 == 0 and same, "wav2tmz keeps the MZF comment behind the %s preloader" % label.upper(),
          o + info + (got[0x18:0x28].hex() if got else ""))

# Intercopy NORMAL 2800/3200 s více soubory: MZT 3 souborů -> IC-10.2-NORMAL -> WAV -> wav2tmz
# (tón dalšího souboru se hledá po periodách i za daty předchozího; fáze dvojic u tapemarku)
for bd in ("2800", "3200"):
    d = os.path.join(TMP, "icn-multi-" + bd)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    mzt = os.path.join(d, "in.mzt")
    files = [mzf_big(name=("F%d" % k).encode(), fsize=200 + 37 * k) for k in range(3)]
    open(mzt, "wb").write(b"".join(files))
    tmz, wav, out = (os.path.join(d, n) for n in ("x.tmz", "x.wav", "y.tmz"))
    rc1, o1 = run_tool("mzf2tmz", mzt, tmz, "--profile", "IC-10.2-NORMAL", "--bd", bd)
    rc2, o2 = run_tool("tmz2wav", tmz, wav)
    rc3, o3 = run_tool("wav2tmz", wav, "--output-format", "tmz", "-o", out)
    info = run_tool("tmzinfo", out)[1] if os.path.exists(out) else ""
    n_ok = o3.count("CRC: header=OK body=OK")
    check(rc1 == 0 and rc2 == 0 and rc3 == 0 and n_ok == 3 and info.count("IC-10.2-NORMAL (bd=%s" % bd) == 3
          and "ERROR" not in o3,
          "wav2tmz Intercopy NORMAL %s: 3 files on one tape, all decoded" % bd, o1 + o2 + o3 + info)

# Intercopy CPM všech rychlostí: IC-10.2-CPM --bd X -> WAV -> wav2tmz zapíše stejný profil bez varování
m = mzf_big(name=b"CPM", fsize=256)
for bd in ("1500", "2400", "2800", "3200", "3600"):
    rcm, rc3, info, o = ident("iccpm-" + bd, m, ("--profile", "IC-10.2-CPM", "--bd", bd))
    check(rcm == 0 and rc3 == 0 and ("Intercopy, %s Bd" % bd) in o and "Warning" not in o
          and ("IC-10.2-CPM (bd=%s" % bd) in info,
          "wav2tmz Intercopy CPM %s Bd written as IC-10.2-CPM bd=%s" % (bd, bd), o + info)
rc, o = run_tool("mzf2tmz", os.path.join(TMP, "id-iccpm-1500", "in.mzf"), os.path.join(TMP, "iccpm-bad.tmz"),
                 "--profile", "IC-10.2-CPM", "--bd", "2000")
check(rc != 0, "mzf2tmz IC-10.2-CPM rejects bd=2000", o)

# NORMAL bez zapisovače: varovat jen při odchylce odhadu rychlosti nad rozlišení vzorkování
# (při 44,1 kHz je půlvlna 2:1 asi 5 vzorků, odhad z průměru leaderu se liší o desítky %)
m = mzf_big(fsize=256)
for spd in ("2:1", "9:7", "2:1cpm"):
    rcm, rc3, info, o = ident("nw-" + spd.replace(":", "-"), m, ("--format", "normal", "--speed", spd))
    check(rcm == 0 and rc3 == 0 and "speed estimate" not in o and "not representable" not in o,
          "wav2tmz NORMAL %s at 44.1 kHz: no speed warning" % spd, o + info)
# skutečná odchylka: NORMAL 1:1 z 96 kHz s hlavičkou WAV x 1,25 (odhad 1,20x, nejbližší 9:7 o 7 %)
d = os.path.join(TMP, "nwfar")
shutil.rmtree(d, ignore_errors=True)
os.makedirs(d)
src = os.path.join(d, "in.mzf")
open(src, "wb").write(m)
rc1, o1 = run_tool("mzf2tmz", src, os.path.join(d, "x.tmz"), "--format", "normal")
rc2, o2 = run_tool("tmz2wav", os.path.join(d, "x.tmz"), os.path.join(d, "x.wav"), "--rate", "96000")
w = os.path.join(d, "x125.wav")
if rc1 == 0 and rc2 == 0:
    b = bytearray(read(os.path.join(d, "x.wav")))
    rate = round(96000 * 1.25)
    struct.pack_into("<II", b, 24, rate, rate * struct.unpack_from("<H", b, 32)[0])
    open(w, "wb").write(bytes(b))
rc3, o3 = run_tool("wav2tmz", w, "--output-format", "tmz", "-o", os.path.join(d, "y.tmz"))
check(rc3 == 0 and "Warning: NORMAL speed estimate 1.20x differs from nearest 9:7" in o3,
      "wav2tmz NORMAL 1.20x: speed warning beyond sampling resolution", o1 + o2 + o3)

# mzf2tmz: TC-1.21 s poměrem TurboCopy mimo en_CMTSPEED
src = os.path.join(TMP, "tcr.mzf")
open(src, "wb").write(mzf_big(fsize=256))
out = os.path.join(TMP, "tcr.tmz")
rc, o = run_tool("mzf2tmz", src, out, "--profile", "TC-1.21", "--speed", "9:4", "--loader", "n")
info = run_tool("tmzinfo", out)[1]
check(rc == 0 and "TC-1.21 (speed=9:4,loader=N" in info, "mzf2tmz --profile TC-1.21 --speed 9:4", o + info)
for label, args in (("no profile", ("--format", "turbo", "--speed", "9:4")),
                    ("ROM profile", ("--profile", "ROM-800-NOMINAL", "--speed", "9:4")),
                    ("out of range", ("--profile", "TC-1.21", "--speed", "4:9"))):
    rc, o = run_tool("mzf2tmz", src, os.path.join(TMP, "tcr-bad.tmz"), *args)
    check(rc == 1 and "speed" in o, "mzf2tmz TurboCopy ratio rejected: %s" % label, o)

# tmz2mzf --mzt-unicmt: TC-1.21 bez předzavaděče je hlavička + tělo struktury ROM,
# pulzy hlavičky CMTSPEED z S (SHORT (S+1)/2 + S/2 tiků, LONG S + S tiků CTC0)
# (UniCMT: první soubor 1x, proto před ním soubor NORMAL 1:1; TC 1:1 sám je 1x)
out = os.path.join(TMP, "tcu1.tmz")
rc1, o1 = run_tool("mzf2tmz", src, out, "--profile", "TC-1.21", "--speed", "1:1", "--loader", "n")
rc2, o2 = run_tool("tmz2mzf", out, "--mzt-unicmt", "--output", os.path.join(TMP, "tcu1.mzt"), "--overwrite")
check(rc1 == 0 and rc2 == 0 and "0 CMTSPEED" in o2, "tmz2mzf --mzt-unicmt writes TC-1.21 1:1 loader=N as 1x", o1 + o2)
for spd, s in (("9:4", 284), ("2:1", 311)):
    out = os.path.join(TMP, "tcu.tmz")
    mzt = os.path.join(TMP, "tcu.mzt")
    if os.path.exists(out):
        os.remove(out)
    rc0, o0 = run_tool("mzf2tmz", src, out, "--format", "normal")
    rc1, o1 = run_tool("mzf2tmz", src, out, "--profile", "TC-1.21", "--speed", spd, "--loader", "n")
    rc2, o2 = run_tool("tmz2mzf", out, "--mzt-unicmt", "--output", mzt, "--overwrite")
    d = read(mzt) if os.path.exists(mzt) else b""
    k = d.find(b"CMTSPEED") - 1
    us = [round(x * 1e6 / 1108405) for x in (s, s, (s + 1) // 2, s // 2)]
    hdr_ok = k > 0 and list(struct.unpack_from("<4H", d, k + 0x30)) == us
    rc1 = rc0 or rc1
    check(rc1 == 0 and rc2 == 0 and hdr_ok, "tmz2mzf --mzt-unicmt writes TC-1.21 %s loader=N" % spd,
          o1 + o2 + (d[:0x20].hex() if d else ""))

# SINCLAIR s pomalým pilotem: TZX 0x11 s časováním ZX x 1,5 (pilot 3254 T = 929,7 us
# jako Intercopy SINCLAIR 900 Bd), hlavička ZX 19 B
def tzx_turbo_block(data, k):
    """Blok 0x11 se standardním časováním ZX vynásobeným k (T-stavy 3,5 MHz)."""
    t = [round(x * k) for x in (2168, 667, 735, 855, 1710)]
    return bytes([0x11]) + struct.pack("<HHHHHHBH", t[0], t[1], t[2], t[3], t[4], 8063, 8, 1000) + \
        struct.pack("<I", len(data))[:3] + data


hdr = bytes([0x00, 0x03]) + b"SLOWPILOT " + struct.pack("<HHH", 16, 32768, 0)
ck = 0
for b in hdr:
    ck ^= b
zx_hdr = hdr + bytes([ck])
d = os.path.join(TMP, "zx900")
shutil.rmtree(d, ignore_errors=True)
os.makedirs(d)
tzx, wav, out = (os.path.join(d, n) for n in ("x.tzx", "x.wav", "y.tmz"))
open(tzx, "wb").write(b"ZXTape!\x1a\x01\x14" + tzx_turbo_block(zx_hdr, 1.5))
rc2, o2 = run_tool("tmz2wav", tzx, wav)
rc3, o3 = run_tool("wav2tmz", wav, "--output-format", "tmz", "-o", out)
info = run_tool("tmzinfo", out)[1] if os.path.exists(out) else ""
check(rc2 == 0 and rc3 == 0 and "ID 0x11" in info, "wav2tmz decodes ZX with a 930 us pilot (SINCLAIR 900 Bd)",
      o2 + o3 + info)

print("\n=== CLI Test: %s ===" % ("FAIL" if fails else "PASS"))
sys.exit(1 if fails else 0)
