"""Build probe_two_assets.bun — 2 uncompressed assets, no external deps."""
import struct, os

BUN_MAGIC = 0x304E5542
HDR_FMT = "<IHHIQQQQQQ"   # 60 bytes
REC_FMT = "<IIQQQIIII"    # 48 bytes

# String table: "hello" at offset 0 (len 5), "world" at offset 8 (len 5)
string_table = b"hello\x00\x00\x00world\x00\x00\x00"   # 16 bytes, divisible by 4
data_section = b""

ATO = 60                   # asset table immediately after header
STO = ATO + 2 * 48         # = 156
STS = len(string_table)    # = 16
DSO = STO + STS            # = 172
DSS = 0

header = struct.pack(HDR_FMT, BUN_MAGIC, 1, 0, 2,
                     ATO, STO, STS, DSO, DSS, 0)
rec0   = struct.pack(REC_FMT, 0, 5, 0, 0, 0, 0, 0, 0, 0)  # name "hello"
rec1   = struct.pack(REC_FMT, 8, 5, 0, 0, 0, 0, 0, 0, 0)  # name "world"

with open("probe_two_assets.bun", "wb") as f:
    f.write(header + rec0 + rec1 + string_table)

print(f"Wrote probe_two_assets.bun ({os.path.getsize('probe_two_assets.bun')} bytes)")
