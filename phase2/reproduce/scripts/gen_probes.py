#!/usr/bin/env python3
# =============================================================================
# AI ASSISTANCE DECLARATION
# This file was developed with the assistance of Claude.
# Specifically, AI was used to:
# 1. Binary format construction (Lab 6 – week 8, Binary data formats):
#    Claude assisted with the struct.pack format strings and little-endian
#    field layout matching the BUN spec on-disk structure, as covered in
#    the lab's section on serialisation and endianness.
# 2. Sparse file creation: Claude suggested using os.ftruncate to produce a
#    virtually large file without consuming disk space (probe_large_assets.bun).
# =============================================================================
"""Generate crafted BUN fixtures for the reproduction package.

Usage: python3 scripts/gen_probes.py <output_dir>

Produces:
  probe_two_assets.bun   -- two valid uncompressed assets (for F-01)
  probe_empty.bun        -- valid zero-asset file          (for F-02)
  probe_trunc.bun        -- 10,001 assets, last has unsupported
                            compression value that overflows the 128-byte
                            snprintf buffer                 (for F-03)
  probe_large_assets.bun -- sparse file with 25M assets; triggers >1 GB
                            RSS via linear calloc           (for F-04)
  probe_rle_mismatch.bun -- RLE asset whose uncompressed_size mismatches
                            actual expansion; followed by a valid asset
                            to confirm parser does not abort (for F-05)
  probe_empty_overlap.bun-- zero-size asset table placed inside the string
                            table; exposes false-positive overlap check
                                                            (for F-06)

No dependencies beyond the Python standard library.
"""

import os
import struct
import sys

BUN_MAGIC = 0x304E5542
HDR_FMT = "<IHHIQQQQQQ"   # 60 bytes
REC_FMT = "<IIQQQIIII"    # 48 bytes


def header(asset_count, ato, sto, sts, dso, dss):
    return struct.pack(HDR_FMT, BUN_MAGIC, 1, 0, asset_count,
                       ato, sto, sts, dso, dss, 0)


def record(name_off, name_len, data_off, data_size,
           uncompressed_size=0, compression=0, rtype=0, checksum=0, flags=0):
    return struct.pack(REC_FMT, name_off, name_len, data_off, data_size,
                       uncompressed_size, compression, rtype, checksum, flags)


def write(path, data):
    with open(path, "wb") as f:
        f.write(data)
    print(f"  wrote {path}  ({len(data)} bytes)")


def gen_probe_two_assets(out_dir):
    """Two uncompressed assets ('hello', 'world') in a valid BUN.
    Used by F-01: a mid-loop fseeko failure leaks str_buf allocated
    for asset 0's name.
    """
    # String table: "hello" at offset 0 (len 5), "world" at offset 8 (len 5)
    string_table = b"hello\x00\x00\x00world\x00\x00\x00"  # 16 bytes

    ATO = 60
    STO = ATO + 2 * 48   # 156
    STS = len(string_table)
    DSO = STO + STS       # 172

    hdr  = header(2, ATO, STO, STS, DSO, 0)
    rec0 = record(0, 5, 0, 0)   # "hello"
    rec1 = record(8, 5, 0, 0)   # "world"

    write(os.path.join(out_dir, "probe_two_assets.bun"),
          hdr + rec0 + rec1 + string_table)


def gen_probe_empty(out_dir):
    """Header-only BUN with asset_count=0.
    Spec §4 permits zero assets (u32, no minimum stated).
    Used by F-02: calloc(0, sizeof(...)) returns NULL on spec-conformant
    implementations, causing a valid file to be wrongly rejected.
    """
    # All section offsets point to byte 60 (immediately after header)
    # with zero sizes — valid per spec §9.4.
    hdr = header(0, 60, 60, 0, 60, 0)
    write(os.path.join(out_dir, "probe_empty.bun"), hdr)


def gen_probe_trunc(out_dir):
    """10,001 asset records; the last record has compression=UINT32_MAX.

    The error message at bun_parse.c:360 is:
      "(2) Asset record %u has unsupported compression type %u.
       This parser supports only types 0 (uncompressed) and 1 (RLE)"

    Fixed text = 113 chars. With index=10000 (5 digits) and
    compression=4294967295 (10 digits) the total is 128 chars + null = 129
    bytes, overflowing the 128-byte stack buffer by 1: snprintf silently
    drops the closing ')'.

    Used by F-03.
    """
    ASSET_COUNT = 10001
    ATO = 60
    STO = ATO + ASSET_COUNT * 48
    string_table = b"hello\x00\x00\x00"   # 8 bytes; all records share this name
    STS = len(string_table)
    DSO = STO + STS

    hdr  = header(ASSET_COUNT, ATO, STO, STS, DSO, 0)
    good = record(0, 5, 0, 0, compression=0)
    bad  = record(0, 5, 0, 0, compression=0xFFFFFFFF)

    data = bytearray()
    data += hdr
    for _ in range(ASSET_COUNT - 1):
        data += good
    data += bad
    data += string_table

    write(os.path.join(out_dir, "probe_trunc.bun"), bytes(data))


def gen_probe_large_assets(out_dir):
    """Sparse BUN file with asset_count = 25,000,000.

    The parser calls calloc(asset_count, sizeof(BunAssetRecord)) at line 302
    and writes to ctx->assets[i] for every i at line 337. With 25M records
    that's a 1.2 GB allocation that is actually touched, pushing RSS over
    the 1 GB threshold defined in the Phase 2 brief §5.3.

    Used by F-04. File is sparse — only ~4 KB on disk.
    """
    ASSET_COUNT = 25_000_000
    ATO = 60
    STO = ATO + ASSET_COUNT * 48
    file_size = STO

    hdr = header(ASSET_COUNT, ATO, STO, 0, STO, 0)
    path = os.path.join(out_dir, "probe_large_assets.bun")
    fd = os.open(path, os.O_CREAT | os.O_TRUNC | os.O_WRONLY, 0o644)
    os.write(fd, hdr)
    os.ftruncate(fd, file_size)   # sparse — virtual size = file_size
    os.close(fd)
    print(f"  wrote {path}  ({file_size:,} bytes virtual, sparse)")


def gen_probe_rle_mismatch(out_dir):
    """Two assets: asset 0 is RLE with claimed uncompressed_size that doesn't
    match the actual RLE expansion; asset 1 is a valid uncompressed asset.

    Spec §5.1 note 4: parser must abort parsing and return BUN_MALFORMED on
    the mismatch. The parser detects the mismatch and sets BUN_MALFORMED,
    but does NOT abort — it continues to process and print asset 1.

    Used by F-05.
    """
    # String table: "bad" at offset 0 len 3, "good" at offset 4 len 4
    string_table = b"bad\x00good\x00\x00\x00\x00"   # 12 bytes (div by 4)

    # Data section:
    #   Asset 0 RLE data: (count=3, value='A') -> expands to 3 bytes
    #   Asset 1 raw data: "ABCD"
    data_section = bytes([0x03, 0x41]) + b"\x00\x00" + b"ABCD"   # 8 bytes

    ATO = 60
    STO = ATO + 2 * 48   # 156
    STS = len(string_table)
    DSO = STO + STS
    DSS = len(data_section)

    hdr = header(2, ATO, STO, STS, DSO, DSS)

    # Asset 0: RLE compressed, claims uncompressed_size=10 but actually expands to 3
    rec0 = record(name_off=0, name_len=3, data_off=0, data_size=2,
                  uncompressed_size=10, compression=1)
    # Asset 1: uncompressed, valid
    rec1 = record(name_off=4, name_len=4, data_off=4, data_size=4,
                  uncompressed_size=0, compression=0)

    write(os.path.join(out_dir, "probe_rle_mismatch.bun"),
          hdr + rec0 + rec1 + string_table + data_section)


def gen_probe_empty_overlap(out_dir):
    """An empty asset table (asset_count=0, size 0) placed strictly inside
    the string table.

    Per spec §9.3, "no sections overlap." An empty section occupies zero
    bytes and cannot overlap anything. The parser's overlap check at
    bun_parse.c:278 uses the formula A.start < B.end && A.end > B.start —
    which incorrectly flags zero-size A inside B as overlapping.

    Used by F-06.
    """
    # String table at [60, 76), empty asset table placed at offset 64 (inside)
    STO = 60
    STS = 16
    ATO = 64               # 4-byte aligned, strictly inside string table
    DSO = STO + STS        # 76

    string_table = b"hello\x00\x00\x00world\x00\x00\x00"   # 16 bytes
    hdr = header(0, ATO, STO, STS, DSO, 0)

    write(os.path.join(out_dir, "probe_empty_overlap.bun"),
          hdr + string_table)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <output_dir>", file=sys.stderr)
        sys.exit(1)

    out_dir = sys.argv[1]
    os.makedirs(out_dir, exist_ok=True)

    print(f"Generating fixtures into {out_dir}/")
    gen_probe_two_assets(out_dir)
    gen_probe_empty(out_dir)
    gen_probe_trunc(out_dir)
    gen_probe_large_assets(out_dir)
    gen_probe_rle_mismatch(out_dir)
    gen_probe_empty_overlap(out_dir)
    print("Done.")
