#!/usr/bin/env python3
"""Generate crafted BUN fixtures for the reproduction package.

Usage: python3 scripts/gen_probes.py <output_dir>

Produces:
  probe_two_assets.bun  -- two valid uncompressed assets (for F-01)
  probe_empty.bun       -- valid zero-asset file           (for F-02)
  probe_trunc.bun       -- 10,001 assets, last has unsupported
                           compression value that overflows the 128-byte
                           snprintf buffer                  (for F-03)

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
    print("Done.")
