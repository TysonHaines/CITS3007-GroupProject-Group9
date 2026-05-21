# Makefile for CITS3007 group project phase 2 report.
#
# Replace "XX" in the filenames below with your group number.
# e.g. if your group is 3, change group-XX to group-03.

SOURCE = group-09-phase-2-report.md
OUTPUT = group-09-phase-2-report.pdf

PANDOC  = pandoc
PFLAGS  = --shift-heading-level-by=-1 --table-of-contents --pdf-engine=typst

#___AI_____
TARGET_DIR = target
PARSER     = $(TARGET_DIR)/bun_parser
CFLAGS     = -std=c11 -pedantic-errors -Wall -Wextra
#__________

.PHONY: all clean

all: $(OUTPUT)

$(OUTPUT): $(SOURCE)
	$(PANDOC) $(PFLAGS) -o $@ $<

clean:
	rm -f $(OUTPUT)

# AI claude sonnet 4.6 was used to help generate the following code to reproduce findings f1 and f2
# ── Build target parser ──────────────────────────────────────────────────────
build:
	cd $(TARGET_DIR) && make CFLAGS="$(CFLAGS)"

# ── Run all reproduction tests ───────────────────────────────────────────────
reproduce: reproduce_f1 reproduce_f2

# ── F-01: Excessive memory use ───────────────────────────────────────────────
reproduce_f1: build
	@echo "=== F-01: Excessive memory use ==="
	/usr/bin/time -v $(PARSER) tests/f1-large-asset-count.bun 2>&1 | \
		grep -E "Maximum resident|Exit status"
	@echo "PASS if Maximum resident set size exceeds 1,000,000 kB (1 GB)"

# ── F-02: Parser continues after RLE uncompressed_size mismatch ─────────────
reproduce_f2: build
	@echo "=== F-02: Incorrect output - parser does not abort ==="
	$(PARSER) tests/f2-rle-no-abort.bun; \
	echo "Exit code: $$?"
	@echo "PASS if Asset 1 (name: good, payload: ABCD) appears in output above"