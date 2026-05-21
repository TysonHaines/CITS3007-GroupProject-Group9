
### Finding F-01
 
- **ID:** F-01
- **Category:** Excessive memory use
- **Spec reference:** Brief §5.3 (parser must not crash or produce incorrect
  output on well-formed files) — the leak triggers on a *malformed* file, but
  the brief also requires no resource leaks under any input. Bug occurs while
  processing spec §5 (asset records).
- **Assumptions:** The marker can use a small fault-injection `LD_PRELOAD`
  shim (15 lines, source provided). Without the shim, the same leak occurs
  whenever a multi-asset file is truncated between asset records — but
  triggering it deterministically with a static fixture is brittle, so the
  shim is the clean repro.
**Description**
 
`bun_parse_assets` allocates `str_buf` via `realloc` for asset names while
iterating over asset records. When any `fseeko`/`fread` fails mid-loop, the
function takes an early `return BUN_ERR_IO` without freeing `str_buf`. There
are four such early-return sites in the loop (lines 322, 433, 444, 471). The
realloc on line 474 also has the classic `p = realloc(p, n)` bug: on failure
the original block is leaked because `p` has been overwritten to NULL.
 
**Expected behaviour**
 
For any input — well-formed or malformed — the parser should exit without
leaking heap memory. LeakSanitizer should report zero direct leaks.
 
**Actual behaviour**
 
LeakSanitizer reports a direct leak rooted at `bun_parse.c:474`, with the
allocation traced back to the asset-loop realloc.
 
**Reproduction steps**
 
1. Build the parser with sanitizers:
   ```
   gcc -std=c11 -O0 -g -fsanitize=address,undefined,leak \
       -fno-omit-frame-pointer main.c bun_parse.c -o bun_parser_san
   ```
 
2. Build the fault-injection shim. Place `fault_shim.c` in the same
   directory, then:
   ```
   gcc -shared -fPIC fault_shim.c -o fault_shim.so -ldl
   ```
   (Drop the `-D_GNU_SOURCE` flag if your compiler warns about a
   redefinition — the source file already defines it.)
3. Build the probe input. Save the following as `make_probe.py` and run it
   from the project directory (`python3 make_probe.py`). No external
   dependencies — only Python's `struct` module.
   ```python
   import struct, os
 
   BUN_MAGIC = 0x304E5542
   HDR = "<IHHIQQQQQQ"   # 60 bytes
   REC = "<IIQQQIIII"    # 48 bytes
 
   # String table: "hello" at offset 0 (len 5), "world" at offset 8 (len 5)
   string_table = b"hello\x00\x00\x00world\x00\x00\x00"  # 16 bytes
 
   ATO = 60
   STO = ATO + 2 * 48   # 156
   STS = 16
   DSO = STO + STS       # 172
 
   header = struct.pack(HDR, BUN_MAGIC, 1, 0, 2, ATO, STO, STS, DSO, 0, 0)
   rec0   = struct.pack(REC, 0, 5, 0, 0, 0, 0, 0, 0, 0)  # name "hello"
   rec1   = struct.pack(REC, 8, 5, 0, 0, 0, 0, 0, 0, 0)  # name "world"
 
   with open("probe_two_assets.bun", "wb") as f:
       f.write(header + rec0 + rec1 + string_table)
   print("Wrote", os.path.getsize("probe_two_assets.bun"), "bytes")
   ```
 
4. Run with the shim preloaded *after* libasan (the ordering matters),
   failing the 4th `fseeko` call (the seek to asset 1's string-table entry):
   ```
   ASAN_LIB=$(ldd ./bun_parser_san | awk '/libasan/{print $3}')
   LD_PRELOAD="$ASAN_LIB:$(pwd)/fault_shim.so" FAULT_NTH=4 \
   ASAN_OPTIONS=detect_leaks=1:exitcode=23 \
       ./bun_parser_san probe_two_assets.bun
   ```
   Note: `$(pwd)/fault_shim.so` must be an absolute path — `LD_PRELOAD`
   does not resolve relative paths.
5. Observe the LeakSanitizer report on stderr:
   ```
   [shim] failing fseeko call #4
 
   === Errors ===
   Error 1: (3) Failed to seek to string table name of asset record 1
 
   ==…==ERROR: LeakSanitizer: detected memory leaks
 
   Direct leak of 5 byte(s) in 1 object(s) allocated from:
       #0 …  in realloc …
       #1 …  in bun_parse_assets bun_parse.c:474
       #2 …  in main main.c:226
   SUMMARY: AddressSanitizer: 5 byte(s) leaked in 1 allocation(s).
   ```
   Exit code is 23 (from `ASAN_OPTIONS=exitcode=23`).
---
 
### Finding F-02
 
- **ID:** F-02
- **Category:** Incorrect output
- **Spec reference:** bun-spec §4 (`u32 asset_count` — no minimum stated;
  zero is permitted by the type) and §9.4 ("there must be `asset_count` many
  valid Asset Entry Table records" — implicitly allows 0). Also brief §5.3
  ("must not produce incorrect output on well-formed files of any size") and
  §5.2.c ("Exit with status code 0 (BUN_OK)" for valid files).
- **Assumptions:** The marker uses a 15-line `LD_PRELOAD` shim to make
  `calloc(0, *)` return NULL — which is permitted behaviour under
  C11 §7.22.3¶1 (and is what musl and several other libcs actually do; only
  glibc reliably returns non-NULL). The shim simulates the parser being run
  on a non-glibc system without needing to actually build for one.
**Description**
 
`bun_parse_assets` calls `calloc(asset_count, sizeof(int))` and
`calloc(asset_count, sizeof(BunAssetRecord))` at lines 301-302, then bails
with `BUN_ERR_ALLOC` if either returns NULL. When `asset_count == 0`, both
arguments to calloc are zero, and the C11 standard explicitly permits calloc
to return NULL in that case (implementation-defined). The current code
treats that NULL as an allocation failure, rejecting the file.
 
**Expected behaviour**
 
A header with `asset_count = 0` and all other fields valid is a well-formed
BUN file per spec §4 / §9.4. The parser should:
1. Return exit code 0 (BUN_OK), per brief §5.2.c.
2. Print the header on stdout, per brief §5.2.a.
3. Emit no error messages to stderr.
**Actual behaviour**
 
On any libc where `calloc(0, *)` returns NULL — musl, MSVC, some BSDs,
some embedded libcs — the parser emits `Error 1: (5) Failed to allocate
memory for asset printability array` on stderr and exits with status 5.
A valid file is wrongly rejected.
 
**Reproduction steps**
 
1. Build the parser with default flags (`make`).
2. Build the calloc-zero shim (`zero_calloc_shim.c` provided):
   ```
   gcc -shared -fPIC -D_GNU_SOURCE zero_calloc_shim.c \
       -o zero_calloc_shim.so -ldl
   ```
 
3. Build a valid zero-asset BUN file. Save the following as `make_probe_empty.py`
   and run it (`python3 make_probe_empty.py`):
   ```python
   import struct, os
 
   BUN_MAGIC = 0x304E5542
   header = struct.pack("<IHHIQQQQQQ",
       BUN_MAGIC, 1, 0,   # magic, version 1.0
       0,                  # asset_count = 0
       60, 60, 0, 60, 0, 0)  # ATO, STO, STS, DSO, DSS, reserved
 
   with open("probe_empty.bun", "wb") as f:
       f.write(header)
   print("Wrote", os.path.getsize("probe_empty.bun"), "bytes")
   ```
 
4. Confirm the file is accepted under glibc (no shim):
   ```
   ./bun_parser probe_empty.bun ; echo "rc=$?"
   ```
   → exits with `rc=0`, prints the header. As expected.
5. Re-run with the shim, simulating a libc where `calloc(0, *)` returns NULL:
   ```
   LD_PRELOAD=./zero_calloc_shim.so ./bun_parser probe_empty.bun
   echo "rc=$?"
   ```
   → stderr:
   ```
   [calloc_shim] returning NULL for calloc(0,4)
   [calloc_shim] returning NULL for calloc(0,48)
 
   === Errors ===
   Error 1: (5) Failed to allocate memory for asset printability array
   ```
   → exit code: `rc=5`.
   The same file is rejected as malformed/allocation-failed when run on
   any libc with this calloc(0) behaviour.
---
 
---
 
### Finding F-03
 
- **ID:** F-03
- **Category:** Incorrect output
- **Spec reference:** Brief §5.2.d ("a human-readable list of spec violations
  found in the file – one violation per line, *with enough detail for a
  developer to locate and fix the problem*"). The truncated diagnostic loses
  the parser-supported compression types list, removing the actionable
  guidance the brief requires.
- **Assumptions:** None. Reproduces under default build flags on a
  ~480 KB file (or, for a more dramatic 3-character truncation, a 46 MB file).
**Description**
 
The error message at `bun_parse.c:360` is written into a 128-byte stack
buffer with `snprintf`. The fixed text is 113 bytes; the two `%u`
substitutions are an asset record index (bounded by `asset_count`, max 10
digits as u32) and a compression value read directly from the file (also u32,
attacker-controlled, max 10 digits). When the substitutions total ≥15 digits
the message exceeds 128 bytes and `snprintf` silently truncates the tail.
 
GCC `-Wformat-truncation=2` flags this site (and three sibling sites at
lines 260, 266, 272 — those three are gated by explicit overflow checks at
lines 239-249, so they're harder to drive observably; line 360 is not gated).
The team's Makefile uses the default `-Wformat-truncation=1` which doesn't
warn about may-truncate, only will-truncate, so the warning was missed.
 
**Expected behaviour**
 
The full error message should appear on stderr, including the trailing
`". This parser supports only types 0 (uncompressed) and 1 (RLE)"` — the
part that tells the developer which compression types *are* supported.
 
**Actual behaviour**
 
The message is silently truncated. With `asset_count = 10,001` and the
bad record's `compression` field set to `UINT32_MAX (4294967295)`, the
stderr output ends mid-word:
 
```
Error 1: (2) Asset record 10000 has unsupported compression type 4294967295. This parser supports only types 0 (uncompressed) and 1 (RLE
```
 
— the closing `)` is dropped. With `asset_count = 1,000,001` and the same
compression value, 3 characters are dropped (`LE)` becomes `R`):
 
```
Error 1: (2) Asset record 1000000 has unsupported compression type 4294967295. This parser supports only types 0 (uncompressed) and 1 (R
```
 
**Reproduction steps**
 
1. Build the parser with default flags:
   ```
   make
   ```
 
2. Build the probe input. Save the following as `make_probe_trunc.py` and
   run it (`python3 make_probe_trunc.py`):
   ```python
   import struct, os
 
   BUN_MAGIC = 0x304E5542
   HDR = "<IHHIQQQQQQ"
   REC = "<IIQQQIIII"
 
   ASSET_COUNT = 10001
   ATO = 60
   STO = ATO + ASSET_COUNT * 48
   string_table = b"hello\x00\x00\x00"  # 8 bytes, name at offset 0 len 5
   STS = len(string_table)
   DSO = STO + STS
 
   header = struct.pack(HDR, BUN_MAGIC, 1, 0, ASSET_COUNT,
                        ATO, STO, STS, DSO, 0, 0)
   good = struct.pack(REC, 0, 5, 0, 0, 0, 0,          0, 0, 0)
   bad  = struct.pack(REC, 0, 5, 0, 0, 0, 0xFFFFFFFF, 0, 0, 0)
 
   with open("probe_trunc.bun", "wb") as f:
       f.write(header)
       for _ in range(ASSET_COUNT - 1):
           f.write(good)
       f.write(bad)
       f.write(string_table)
   print("Wrote", os.path.getsize("probe_trunc.bun"), "bytes")
   ```
 
3. Run the parser:
   ```
   ./bun_parser probe_trunc.bun 2>&1 | grep 'compression type'
   ```
 
4. Observe the truncated message (ends with `1 (RLE` — missing the `)`):
   ```
   Error 1: (2) Asset record 10000 has unsupported compression type 4294967295. This parser supports only types 0 (uncompressed) and 1 (RLE
   ```
 
   To see a more dramatic 3-character truncation, change `ASSET_COUNT` to
   `1_000_001`. The file grows to ~46 MB.