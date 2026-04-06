#!/usr/bin/env python3
"""
Konverze Markdown souboru z dist-docs/ do HTML v html-docs/.

Pouziti:
    python3 scripts/md2html.py [dist-docs/]

Generuje html-docs/cz/*.html a html-docs/en/*.html ze vsech .md souboru
v dist-docs/cz/ a dist-docs/en/. Pouziva Python modul 'markdown' s rozsirenim
'tables' a 'fenced_code'.
"""

import os
import sys
import markdown


# CSS styl shodny s puvodnimi HTML soubory v html-docs/
CSS = """\
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif; max-width: 900px; margin: 0 auto; padding: 20px; line-height: 1.6; color: #24292e; }
h1 { border-bottom: 1px solid #eaecef; padding-bottom: 0.3em; }
h2 { border-bottom: 1px solid #eaecef; padding-bottom: 0.3em; }
h3 { margin-top: 24px; }
h4 { margin-top: 20px; }
code { background: #f6f8fa; padding: 0.2em 0.4em; border-radius: 3px; font-size: 85%; }
pre { background: #f6f8fa; padding: 16px; border-radius: 6px; overflow-x: auto; }
pre code { background: none; padding: 0; }
table { border-collapse: collapse; width: 100%; margin: 16px 0; }
th, td { border: 1px solid #dfe2e5; padding: 6px 13px; }
th { background: #f6f8fa; font-weight: 600; }
tr:nth-child(2n) { background: #f6f8fa; }
blockquote { border-left: 4px solid #dfe2e5; padding: 0 16px; color: #6a737d; }
a { color: #0366d6; text-decoration: none; }"""

# mapovani jazyka na html lang atribut
LANG_MAP = {
    "cz": "cs",
    "en": "en",
}


def extract_title(md_text):
    """Extrahuje titulek z prvniho # nadpisu v Markdown textu."""
    for line in md_text.splitlines():
        stripped = line.strip()
        if stripped.startswith("# ") and not stripped.startswith("## "):
            return stripped[2:].strip()
    return "TapeMZ"


def convert_file(src_path, dst_path, lang):
    """Konvertuje jeden Markdown soubor na HTML."""
    with open(src_path, "r", encoding="utf-8") as f:
        md_text = f.read()

    title = extract_title(md_text)
    html_lang = LANG_MAP.get(lang, lang)

    html_body = markdown.markdown(
        md_text,
        extensions=["tables", "fenced_code"],
        output_format="html5",
    )

    html = f"""\
<!DOCTYPE html>
<html lang="{html_lang}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<style>
{CSS}
</style>
</head>
<body>
{html_body}
</body>
</html>
"""

    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
    with open(dst_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(html)


def main():
    src_dir = sys.argv[1] if len(sys.argv) > 1 else "dist-docs"
    dst_dir = "html-docs"

    if not os.path.isdir(src_dir):
        print(f"Error: source directory '{src_dir}' not found", file=sys.stderr)
        sys.exit(1)

    count = 0
    for lang in ("cz", "en"):
        lang_src = os.path.join(src_dir, lang)
        lang_dst = os.path.join(dst_dir, lang)

        if not os.path.isdir(lang_src):
            continue

        for fname in sorted(os.listdir(lang_src)):
            if not fname.endswith(".md"):
                continue

            src_path = os.path.join(lang_src, fname)
            dst_path = os.path.join(lang_dst, fname[:-3] + ".html")

            convert_file(src_path, dst_path, lang)
            count += 1
            print(f"  {src_path} -> {dst_path}")

    print(f"\nGenerated {count} HTML files.")


if __name__ == "__main__":
    main()
