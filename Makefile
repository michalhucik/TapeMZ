export TEMP := $(shell cygpath -w /tmp)
export TMP  := $(TEMP)

BUILD_DIR := build
JOBS      := $(shell nproc)

# Seznam testu spoustenych prikazem "make test".
# test_ic_wav neni zahrnut - vyzaduje povinne argumenty (je pro rucni pouziti).
TESTS := \
    test_cpm_roundtrip \
    test_mz80b_roundtrip \
    test_cpmtape_roundtrip \
    test_fsk_roundtrip \
    test_slow_roundtrip \
    test_direct_roundtrip \
    test_fsk_tape_roundtrip \
    test_slow_tape_roundtrip \
    test_direct_tape_roundtrip \
    test_direct_tape_wav \
    test_bsd_wav \
    test_zx_roundtrip \
    test_ic_normal \
    test_tc_normal

.PHONY: all clean configure rebuild test html-docs

all: $(BUILD_DIR)/Makefile
	cmake --build $(BUILD_DIR) -j$(JOBS)

configure: $(BUILD_DIR)/Makefile

$(BUILD_DIR)/Makefile:
	cmake --preset default

clean:
	cmake --build $(BUILD_DIR) --target clean

rebuild: clean all

DOCS_SRC := $(shell if [ -d dist-docs ]; then echo dist-docs; else echo docs; fi)

html-docs:
	@echo "=== Generating HTML docs from $(DOCS_SRC)/ ==="
	@python3 scripts/md2html.py $(DOCS_SRC)
	@echo "=== Done ==="

test: all
	@echo "=== Running tests ==="
	@cd $(BUILD_DIR) && for t in $(TESTS); do \
	    echo "--- $$t ---"; \
	    ./$$t.exe || exit 1; \
	done
	@echo "=== All tests passed ==="
