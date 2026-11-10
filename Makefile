# Detekce platformy: MSYS2/MinGW vs Linux/POSIX
# Pozn: MSYS2 make neimportuje promennou OS, pouzijeme MSYSTEM (nastavuje MSYS2)
ifneq ($(MSYSTEM),)
    # MSYS2/MinGW: GCC linker (collect2) vyzaduje zapisovatelny TEMP adresar
    export TEMP := $(shell cygpath -w /tmp)
    export TMP  := $(TEMP)
    CMAKE_CONFIGURE := cmake --preset default
    EXE_SUFFIX := .exe
else
    CMAKE_CONFIGURE := cmake -B build
    EXE_SUFFIX :=
endif

BUILD_DIR := build
JOBS      := $(shell nproc)

# Seznam testu spoustenych prikazem "make test".
# test_ic_wav neni zahrnut - vyzaduje povinne argumenty (je pro rucni pouziti).
TESTS := \
    test_cpm_roundtrip \
    test_fm_header_only \
    test_fm_copy2 \
    test_fm_recovery \
    test_wav_formats \
    test_tmz_blocks_parse \
    test_tmz_tlv \
    test_cmtspeed \
    test_mztape_pulses \
    test_tmz_mzfile \
    test_tmz_fuzz \
    test_tzx_level \
    test_tzx_csw \
    test_tmz_player_level \
    test_mzcmt_fm \
    test_mzcmt_manchester \
    test_tmz_part_manchester \
    test_tmz_part_fm \
    test_tmz_v2_ref \
    test_tmz_profile \
    test_tmz_player_mzfile \
    test_tmz_v1legacy \
    test_mzf_terminator \
    test_tmz_v1conv \
    test_tmz_mzt \
    test_tzx_load \
    test_turbo_readpoint \
    test_turbo_roundtrip \
    test_tmz_player_turbo \
    test_mz80b_roundtrip \
    test_cpmtape_roundtrip \
    test_fsk_roundtrip \
    test_slow_roundtrip \
    test_direct_roundtrip \
    test_fsk_tape_roundtrip \
    test_slow_tape_roundtrip \
    test_direct_tape_roundtrip \
    test_tape_body_crc \
    test_direct_tape_wav \
    test_bsd_wav \
    test_bsd_incomplete \
    test_zx_roundtrip \
    test_ic_normal \
    test_ic_turbo_files \
    test_tc_normal \
    test_tc_loader_all \
    test_ic_loader_all \
    test_ic_sinclair_all

.PHONY: all clean configure rebuild test html-docs portable portable-clean release release-clean

# Python pro CLI testy a skripty; přednost má python3
# (některé distribuce Linuxu příkaz python nemají)
PYTHON ?= $(shell command -v python3 >/dev/null 2>&1 && echo python3 || echo python)


all: $(BUILD_DIR)/Makefile
	cmake --build $(BUILD_DIR) -j$(JOBS)

configure: $(BUILD_DIR)/Makefile

$(BUILD_DIR)/Makefile:
	$(CMAKE_CONFIGURE)

clean:
	cmake --build $(BUILD_DIR) --target clean

rebuild: clean all

DOCS_SRC := $(shell if [ -d dist-docs ]; then echo dist-docs; else echo docs; fi)

html-docs:
	@echo "=== Generating HTML docs from $(DOCS_SRC)/ ==="
	@python3 scripts/md2html.py $(DOCS_SRC)
	@echo "=== Done ==="

# --- Portable distribuce a archiv vydání ---
#
# Verze vydání celé sady nástrojů (tag vX.Y.Z ve veřejném repozitáři).
# Nezávisí na verzi formátu TMZ ani na verzích jednotlivých nástrojů.
RELEASE_VERSION := 1.0.0

# Nástroje, které jdou do balíku (ty, které mají uživatelský manuál).
TOOLS := tmzinfo mzf2tmz tmz2mzf tmz2wav wav2tmz tap2tmz tmz2tap tmzconv tmzedit \
         dat2bsd bsd2dat

PORTABLE_NAME := tapemz-cli-$(RELEASE_VERSION)
PORTABLE_DIR  := portable/$(PORTABLE_NAME)
PORTABLE_ZIP  := portable/$(PORTABLE_NAME).zip

# `make portable` sestaví balík do adresáře s verzí v názvu:
#   portable/tapemz-cli-X.Y.Z/
#     bin/               - nástroje
#     docs/cz/, docs/en/ - dokumentace (.md, a .html, pokud existuje html-docs/)
#     README.md, README_cz.md, LICENSE (pokud v kořeni existuje)
#
# Binárky z MSYS2 UCRT64 závisejí jen na KERNEL32.dll a api-ms-win-crt-*.dll
# (součást Windows 10/11), proto se k nim žádné DLL nepřibalují. Cíl to na
# Windows kontroluje přes objdump a při jiné závislosti skončí chybou.
portable: all
	@echo "=== Building portable package in $(PORTABLE_DIR)/ ==="
	@rm -rf $(PORTABLE_DIR)
	@mkdir -p $(PORTABLE_DIR)/bin $(PORTABLE_DIR)/docs/cz/ $(PORTABLE_DIR)/docs/en/
	@for t in $(TOOLS); do cp $(BUILD_DIR)/$$t$(EXE_SUFFIX) $(PORTABLE_DIR)/bin/ || exit 1; done
	@cp $(DOCS_SRC)/cz/*.md $(PORTABLE_DIR)/docs/cz/
	@cp $(DOCS_SRC)/en/*.md $(PORTABLE_DIR)/docs/en/
	@cp $(DOCS_SRC)/en/TapeMZ.md $(PORTABLE_DIR)/README.md
	@cp $(DOCS_SRC)/cz/TapeMZ.md $(PORTABLE_DIR)/README_cz.md
	@if [ -f LICENSE ]; then cp LICENSE $(PORTABLE_DIR)/; else echo "--- LICENSE not present, skipped ---"; fi
	@if [ -d html-docs ]; then \
	    echo "--- Including HTML docs from html-docs/ ---"; \
	    cp html-docs/cz/*.html $(PORTABLE_DIR)/docs/cz/; \
	    cp html-docs/en/*.html $(PORTABLE_DIR)/docs/en/; \
	else \
	    echo "--- HTML docs not present (run 'make html-docs' to include them) ---"; \
	fi
	@if [ -n "$(EXE_SUFFIX)" ]; then \
	    extra=$$(for f in $(PORTABLE_DIR)/bin/*$(EXE_SUFFIX); do objdump -p $$f | grep 'DLL Name'; done \
	        | grep -v -i -E 'KERNEL32\.dll|api-ms-win-crt-' | sort -u); \
	    if [ -n "$$extra" ]; then \
	        echo "ERROR: tools depend on DLLs that are not part of Windows:"; echo "$$extra"; exit 1; \
	    fi; \
	    echo "--- DLL check: only KERNEL32.dll and api-ms-win-crt-*.dll ---"; \
	fi
	@echo "=== Done: $(PORTABLE_DIR)/ ==="

portable-clean:
	@rm -rf $(PORTABLE_DIR)
	@if [ -d portable ] && [ -z "$$(ls -A portable 2>/dev/null)" ]; then rmdir portable; fi

# `make release` zabalí portable/tapemz-cli-X.Y.Z/ do portable/tapemz-cli-X.Y.Z.zip.
# Archiv obsahuje adresář s verzí v názvu. Vyžaduje příkaz zip.
release: portable
	@echo "=== Creating $(PORTABLE_ZIP) ==="
	@rm -f $(PORTABLE_ZIP)
	@cd portable && zip -rq $(PORTABLE_NAME).zip $(PORTABLE_NAME)
	@echo "=== Done: $(PORTABLE_ZIP) ==="

release-clean:
	@rm -f portable/tapemz-cli-*.zip
	@if [ -d portable ] && [ -z "$$(ls -A portable 2>/dev/null)" ]; then rmdir portable; fi

# Testy v C ze seznamu TESTS, pak kontroly CLI (tests/cli/test_cli.py)
# a kontrola veřejných textů bez interních odkazů (tests/check_public_text.py).
# Python skripty potřebují Python; bez něj se přeskočí s výpisem SKIP.
test: all
	@echo "=== Running tests ==="
	@cd $(BUILD_DIR) && for t in $(TESTS); do \
	    echo "--- $$t ---"; \
	    ./$$t$(EXE_SUFFIX) || exit 1; \
	done
	@echo "--- CLI tests ---"
	@if command -v $(PYTHON) >/dev/null 2>&1; then \
	    $(PYTHON) "$(CURDIR)/tests/cli/test_cli.py" "$(EXE_SUFFIX)" "$(CURDIR)/$(BUILD_DIR)" || exit 1; \
	    $(PYTHON) "$(CURDIR)/tests/cli/test_check_public_text.py" || exit 1; \
	    $(PYTHON) "$(CURDIR)/tests/check_public_text.py" || exit 1; \
	else \
	    echo "SKIP: $(PYTHON) not found, CLI tests not run"; \
	fi
	@echo "=== All tests passed ==="

# Volitelné cíle vývojového prostředí (soubor nemusí existovat).
-include devtools/emu-verify/emu-verify.mk
