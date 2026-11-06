"""Kontrola veřejných textů: v tom, co jde na GitHub, nesmí být odkazy na interní vývoj.

Použití: check_public_text.py [cesta ...]   (bez cest: všechny veřejné stromy)
Veřejné stromy odpovídají synchronizaci do veřejného repozitáře. Vypíše
soubor:řádek: vzor: text a skončí 1, když něco najde.
"""
import os
import re
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
PUBLIC = ["src", "tests", "scripts", "dist-docs", "CMakeLists.txt", "CMakePresets.json", "Makefile"]
EXT = (".c", ".h", ".py", ".md", ".txt", ".asm", ".sh", ".json", ".cmake", "Makefile")
SKIP = {os.path.join("tests", "check_public_text.py"), os.path.join("tests", "cli", "test_check_public_text.py")}

PATTERNS = [
    ("plan number", re.compile(r"\bpl[aá]n(?:y|ů|em|ech|u|a)?\s+\d{1,2}\b", re.I)),
    ("review", re.compile(r"\brevize\b|\brevizi\b|\brevize\s+20\d\d", re.I)),
    ("finding id", re.compile(r"(?:nález|nálezy|nálezu|finding|rozhodnutí|decision)\s+[A-Z]\d{1,2}\b", re.I)),
    ("finding id in parens", re.compile(r"\((?!C(?:89|90|99|11|17|23)\b)[CDEFKLMNSTW]\d{1,2}(?:[,:;)\s/-]|$)")),
    # holé ID nálezu před ":" nebo ")" ("W3:", "D14)", "T4:"); jazykové normy C se nepočítají
    ("bare finding id", re.compile(r"(?:^|[\s(,/])(?!C(?:89|90|99|11|17|23)\b)[CDEFKLMNSTW]\d{1,2}(?:-[a-z])?[:)]")),
    ("analysis ref", re.compile(r"\bR\d \d+\.\d")),
    ("dated decision", re.compile(r"rozhodnutí\s+20\d\d", re.I)),
    ("private tool path", re.compile(r"devtools/(?!emu-verify/emu-verify\.mk\b)")),
    ("private doc path", re.compile(r"docs/(?:analyza|plany|revize|emu-verify|detailne-prozkoumat)|\br\d-[a-z0-9-]+\.md\b|emu-verify-nalezy")),
    ("private docs dir", re.compile(r"(?<![\w-])docs/(?!cz/|en/)[A-Za-z0-9_]")),
    ("knowledge base", re.compile(r"mz800-knowledge|(?<!\d)(?<!\d )\bKB\b")),
    ("hwlog", re.compile(r"\bhwlog\b", re.I)),
    ("scratchpad", re.compile(r"scratchpad", re.I)),
    ("owner decision", re.compile(r"Michal\s+20\d\d|(?:rozhodl|řekl|rozhodnutí)\s+Michal|Michalov[aáy]\s+rozhodnutí")),
]


def find(line):
    """Vrátí jméno prvního vzoru, který řádek porušuje, nebo None."""
    for name, rx in PATTERNS:
        if rx.search(line):
            return name
    return None


def files(paths):
    for p in paths:
        full = os.path.join(REPO, p)
        if os.path.isfile(full):
            yield p
            continue
        for root, _, names in os.walk(full):
            for n in names:
                rel = os.path.relpath(os.path.join(root, n), REPO)
                if rel in SKIP or not (n.endswith(EXT) or n in EXT):
                    continue
                yield rel


def main():
    paths = sys.argv[1:] or PUBLIC
    hits = 0
    for rel in files(paths):
        try:
            text = open(os.path.join(REPO, rel), encoding="utf-8", errors="replace").read().splitlines()
        except OSError:
            continue
        for i, line in enumerate(text, 1):
            name = find(line)
            if name:
                print("%s:%d: %s: %s" % (rel, i, name, line.strip()[:120]))
                hits += 1
    print("=== Public text check: %s (%d) ===" % ("FAIL" if hits else "PASS", hits))
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main())
