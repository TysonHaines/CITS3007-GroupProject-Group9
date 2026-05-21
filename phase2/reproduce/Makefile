# =============================================================================
# Reproduction package — Group 17 BUN parser audit
#
# Usage:
#   1. Unzip Group 17's source into target/ within this directory.
#   2. Run: make reproduce
#
# Requirements: gcc, python3, ldd, awk (all standard on the SDE)
# =============================================================================

.NOTPARALLEL:
.PHONY: reproduce build_san build_plain build_shims gen_fixtures \
        test_f01 test_f02 test_f03 test_f04 test_f05 test_f06 clean

# ---------------------------------------------------------------------------
# Build flags
#
# SAN_CFLAGS:   original flags + sanitizers; used for F-01 (leak detection).
# PLAIN_CFLAGS: original flags only; used for F-02 and F-03 (LD_PRELOAD
#               shims don't mix cleanly with ASan's allocator).
#
# Neither set adds or removes preprocessor definitions (-D / -U), as required
# by the rules of engagement.
# ---------------------------------------------------------------------------
ORIG_FLAGS   = -std=c11 -pedantic-errors -Wall -Wextra -Wconversion \
               -Werror=vla -Werror=implicit-function-declaration

SAN_CFLAGS   = $(ORIG_FLAGS) \
               -fsanitize=address,undefined,leak \
               -fno-omit-frame-pointer -g -O0

PLAIN_CFLAGS = $(ORIG_FLAGS) -O0

FIXTURES_DIR = tests/fixtures/crafted

# ---------------------------------------------------------------------------
# Top-level target
# ---------------------------------------------------------------------------
reproduce: build_san build_plain build_shims gen_fixtures
	@echo ""
	@echo "############################################################"
	@echo "#  Running reproduction tests"
	@echo "############################################################"
	@$(MAKE) test_f01
	@$(MAKE) test_f02
	@$(MAKE) test_f03
	@$(MAKE) test_f04
	@$(MAKE) test_f05
	@$(MAKE) test_f06
	@echo ""
	@echo "############################################################"
	@echo "#  All tests complete"
	@echo "############################################################"

# ---------------------------------------------------------------------------
# Build targets
# ---------------------------------------------------------------------------
build_san:
	@echo ""
	@echo "--- Building bun_parser_san (with sanitizers) ---"
	@if [ ! -d target ]; then \
	  echo "ERROR: target/ directory not found."; \
	  echo "       Unzip Group 17's source into target/ and re-run."; \
	  exit 1; \
	fi
	cd target && $(MAKE) clean
	cd target && $(MAKE) CFLAGS="$(SAN_CFLAGS)"
	mv target/bun_parser ./bun_parser_san
	@echo "Built: bun_parser_san"

build_plain:
	@echo ""
	@echo "--- Building bun_parser_plain (default flags, no sanitizers) ---"
	@if [ ! -d target ]; then \
	  echo "ERROR: target/ directory not found."; \
	  exit 1; \
	fi
	cd target && $(MAKE) clean
	cd target && $(MAKE) CFLAGS="$(PLAIN_CFLAGS)"
	mv target/bun_parser ./bun_parser_plain
	@echo "Built: bun_parser_plain"

build_shims:
	@echo ""
	@echo "--- Building LD_PRELOAD shims ---"
	gcc -shared -fPIC fault_shim.c      -o fault_shim.so      -ldl
	gcc -shared -fPIC zero_calloc_shim.c -o zero_calloc_shim.so -ldl
	@echo "Built: fault_shim.so  zero_calloc_shim.so"

gen_fixtures:
	@echo ""
	@echo "--- Generating test fixtures ---"
	python3 scripts/gen_probes.py $(FIXTURES_DIR)

# ---------------------------------------------------------------------------
# F-01: Memory leak in bun_parse_assets
#
# bun_parse_assets allocates str_buf via realloc for each asset name.
# When fseeko fails mid-loop the function returns BUN_ERR_IO without freeing
# str_buf.  We inject the failure with a fault-injection shim and observe
# the leak via LeakSanitizer.
#
# Baseline: no fault injection -> no leak.
# Flaw:     fault injected on call #4 (seek to asset 1's string-table entry)
#           -> LeakSanitizer reports 5-byte direct leak at bun_parse.c:474.
# ---------------------------------------------------------------------------
test_f01:
	@echo ""
	@echo "========================================="
	@echo "F-01: Memory leak in bun_parse_assets"
	@echo "========================================="
	@echo ""
	@echo "--- Baseline: no fault injection (expect exit 0, no leak) ---"
	@ASAN_OPTIONS=detect_leaks=1:exitcode=23 \
	 ./bun_parser_san $(FIXTURES_DIR)/probe_two_assets.bun > /dev/null 2>&1; \
	 RC=$$?; \
	 if [ $$RC -eq 0 ]; then \
	   echo "  Baseline OK: no leak (rc=$$RC)"; \
	 else \
	   echo "  Baseline UNEXPECTED: rc=$$RC"; \
	 fi
	@echo ""
	@echo "--- With fault injection (FAULT_NTH=4, expect LeakSanitizer report) ---"
	@ASAN_LIB=$$(ldd ./bun_parser_san | awk '/libasan/{print $$3}'); \
	 LD_PRELOAD="$$ASAN_LIB:$$(pwd)/fault_shim.so" \
	 FAULT_NTH=4 \
	 ASAN_OPTIONS=detect_leaks=1:exitcode=23 \
	 ./bun_parser_san $(FIXTURES_DIR)/probe_two_assets.bun 2>&1; \
	 RC=$$?; \
	 echo ""; \
	 if [ $$RC -eq 23 ]; then \
	   echo "[FLAW TRIGGERED] F-01: LeakSanitizer detected memory leak (exit $$RC)"; \
	 else \
	   echo "[NOT TRIGGERED]  F-01: No leak detected (exit $$RC)"; \
	 fi

# ---------------------------------------------------------------------------
# F-02: Spec-conformant zero-asset file wrongly rejected
#
# The parser calls calloc(asset_count, sizeof(...)) without guarding against
# asset_count == 0.  C11 §7.22.3 permits calloc(0,*) to return NULL; glibc
# returns a non-NULL stub but musl and others return NULL.  We simulate the
# latter with a shim and observe the parser returning BUN_ERR_ALLOC (5) on
# a file that is valid per bun-spec §4 / §9.4.
#
# Baseline: no shim -> glibc returns non-NULL -> file accepted (exit 0).
# Flaw:     with shim, calloc(0,*) returns NULL -> file rejected (exit 5).
# ---------------------------------------------------------------------------
test_f02:
	@echo ""
	@echo "========================================="
	@echo "F-02: Spec-conformant zero-asset file wrongly rejected"
	@echo "========================================="
	@echo ""
	@echo "--- Baseline: no shim (expect exit 0) ---"
	@./bun_parser_plain $(FIXTURES_DIR)/probe_empty.bun 2>&1; \
	 RC=$$?; \
	 if [ $$RC -eq 0 ]; then \
	   echo "  Baseline OK: file accepted (rc=$$RC)"; \
	 else \
	   echo "  Baseline UNEXPECTED: file rejected without shim (rc=$$RC)"; \
	 fi
	@echo ""
	@echo "--- With calloc(0)->NULL shim (expect rejection, exit != 0) ---"
	@LD_PRELOAD="$$(pwd)/zero_calloc_shim.so" \
	 ./bun_parser_plain $(FIXTURES_DIR)/probe_empty.bun 2>&1; \
	 RC=$$?; \
	 echo ""; \
	 if [ $$RC -ne 0 ]; then \
	   echo "[FLAW TRIGGERED] F-02: Valid zero-asset file rejected (rc=$$RC, expected 0)"; \
	 else \
	   echo "[NOT TRIGGERED]  F-02: File accepted correctly (rc=$$RC)"; \
	 fi

# ---------------------------------------------------------------------------
# F-03: snprintf truncation in unsupported-compression diagnostic
#
# The error message at bun_parse.c:360 is written into a 128-byte buffer.
# With a 5-digit record index (10000) and 10-digit compression value
# (UINT32_MAX = 4294967295) the formatted string is 129 bytes including the
# null terminator; snprintf silently drops the closing ')'.
#
# The full message should end: "...types 0 (uncompressed) and 1 (RLE)"
# The truncated message ends:  "...types 0 (uncompressed) and 1 (RLE"
#                                                                      ^
# ---------------------------------------------------------------------------
test_f03:
	@echo ""
	@echo "========================================="
	@echo "F-03: snprintf truncation in error diagnostic"
	@echo "========================================="
	@echo ""
	@OUTPUT=$$(./bun_parser_plain $(FIXTURES_DIR)/probe_trunc.bun 2>&1); \
	 echo "  Emitted message:"; \
	 echo "$$OUTPUT" | grep "compression type" | sed 's/^/    /'; \
	 echo ""; \
	 if echo "$$OUTPUT" | grep -q "and 1 (RLE)"; then \
	   echo "[NOT TRIGGERED]  F-03: Full message present"; \
	 else \
	   echo "[FLAW TRIGGERED] F-03: Diagnostic truncated — closing ')' missing"; \
	 fi

# ---------------------------------------------------------------------------
# F-04: Excessive memory use — linear allocation in bun_parse_assets
#
# Lines 301-302: calloc(asset_count, sizeof(int))      -> 4  bytes per record
#                calloc(asset_count, sizeof(BunAssetRecord)) -> 48 bytes / rec
# Line 337:      ctx->assets[i] = record (writes every record into the array)
#
# For asset_count = 25M, the calloc reserves ~1.2 GB; the loop then touches
# every page, pushing RSS over the 1 GB threshold defined by Phase 2 §5.3.
# The fixture is a sparse file so on-disk footprint is ~4 KB.
# ---------------------------------------------------------------------------
test_f04:
	@echo ""
	@echo "========================================="
	@echo "F-04: Excessive memory use (linear allocation in asset loop)"
	@echo "========================================="
	@echo ""
	@echo "--- Measuring peak RSS while parsing 25M-asset sparse file ---"
	@if command -v /usr/bin/time >/dev/null 2>&1; then \
	   RSS_LINE=$$(/usr/bin/time -v ./bun_parser_plain \
	     $(FIXTURES_DIR)/probe_large_assets.bun 2>&1 >/dev/null \
	     | grep "Maximum resident set size"); \
	 else \
	   echo "  (/usr/bin/time not available, using Python fallback)"; \
	   RSS_LINE=$$(python3 scripts/measure_rss.py ./bun_parser_plain \
	     $(FIXTURES_DIR)/probe_large_assets.bun | grep "Maximum resident"); \
	 fi; \
	 echo "  $$RSS_LINE"; \
	 RSS_KB=$$(echo "$$RSS_LINE" | awk '{print $$NF}'); \
	 RSS_MB=$$((RSS_KB / 1024)); \
	 echo "  -> Peak RSS: $$RSS_MB MB"; \
	 if [ $$RSS_KB -gt 1048576 ]; then \
	   echo "[FLAW TRIGGERED] F-04: Peak RSS $$RSS_MB MB exceeds 1 GB threshold"; \
	 else \
	   echo "[NOT TRIGGERED]  F-04: Peak RSS $$RSS_MB MB within 1 GB threshold"; \
	 fi

# ---------------------------------------------------------------------------
# F-05: Parser does not abort on RLE uncompressed_size mismatch
#
# Spec §5.1 note 4: "If a parser detects that compression is used, and that
# the actual uncompressed size is different to the value of uncompressed_size,
# it must abort parsing and return BUN_MALFORMED."
#
# The parser at bun_parse.c:458-463 records the BUN_MALFORMED error but does
# NOT return — execution continues to the next iteration of the asset loop,
# so subsequent assets are still processed and printed.
#
# Probe: asset 0 is RLE with uncompressed_size=10 (actual expansion=3);
#        asset 1 is a valid uncompressed asset with payload "ABCD".
# Flaw triggered if asset 1's name "good" appears in the parser's stdout.
# ---------------------------------------------------------------------------
test_f05:
	@echo ""
	@echo "========================================="
	@echo "F-05: Parser does not abort on RLE uncompressed_size mismatch"
	@echo "========================================="
	@echo ""
	@STDOUT=$$(./bun_parser_plain $(FIXTURES_DIR)/probe_rle_mismatch.bun 2>/dev/null); \
	 STDERR=$$(./bun_parser_plain $(FIXTURES_DIR)/probe_rle_mismatch.bun 2>&1 >/dev/null); \
	 echo "  Errors (stderr):"; \
	 echo "$$STDERR" | grep -E "Error|uncompressed" | sed 's/^/    /'; \
	 echo ""; \
	 echo "  Assets printed (stdout, after mismatch detected):"; \
	 echo "$$STDOUT" | grep -E "Asset Name|Payload" | sed 's/^/    /'; \
	 echo ""; \
	 if echo "$$STDOUT" | grep -q "Asset Name: good"; then \
	   echo "[FLAW TRIGGERED] F-05: Asset 1 processed and printed despite spec-mandated abort"; \
	 else \
	   echo "[NOT TRIGGERED]  F-05: Parser aborted correctly on mismatch"; \
	 fi

# ---------------------------------------------------------------------------
# F-06: False-positive overlap detection on empty sections
#
# Spec §9.3: "no sections overlap". An empty section (size = 0) occupies no
# bytes and cannot overlap anything. The parser's overlap formula at
# bun_parse.c:278 (and the header-overlap check at line 227) does not
# account for empty sections and reports them as overlapping when their
# offset lies inside another section.
#
# Probe: asset_count = 0 (so asset_table_size = 0) placed at offset 64,
# strictly inside the string table at [60, 76). The parser should accept;
# it reports "Asset table overlaps with string table" and exits 1.
# ---------------------------------------------------------------------------
test_f06:
	@echo ""
	@echo "========================================="
	@echo "F-06: False-positive overlap on empty section"
	@echo "========================================="
	@echo ""
	@STDERR=$$(./bun_parser_plain $(FIXTURES_DIR)/probe_empty_overlap.bun 2>&1 >/dev/null); \
	 ./bun_parser_plain $(FIXTURES_DIR)/probe_empty_overlap.bun > /dev/null 2>&1; \
	 RC=$$?; \
	 echo "  Errors (stderr):"; \
	 echo "$$STDERR" | grep -E "Error|overlap" | sed 's/^/    /'; \
	 echo ""; \
	 if [ $$RC -ne 0 ] && echo "$$STDERR" | grep -q "overlap"; then \
	   echo "[FLAW TRIGGERED] F-06: Spec-conformant file rejected with false overlap error (rc=$$RC)"; \
	 else \
	   echo "[NOT TRIGGERED]  F-06: File accepted correctly (rc=$$RC)"; \
	 fi

# ---------------------------------------------------------------------------
# Housekeeping
# ---------------------------------------------------------------------------
clean:
	-rm -f bun_parser_san bun_parser_plain
	-rm -f fault_shim.so zero_calloc_shim.so
	-rm -f $(FIXTURES_DIR)/probe_two_assets.bun \
	       $(FIXTURES_DIR)/probe_empty.bun \
	       $(FIXTURES_DIR)/probe_trunc.bun \
	       $(FIXTURES_DIR)/probe_large_assets.bun \
	       $(FIXTURES_DIR)/probe_rle_mismatch.bun \
	       $(FIXTURES_DIR)/probe_empty_overlap.bun
	-cd target && $(MAKE) clean 2>/dev/null || true
