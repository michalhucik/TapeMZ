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
    test_tmz_mzfile \
    test_tmz_fuzz \
    test_tzx_level \
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

.PHONY: all clean configure rebuild test html-docs

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
