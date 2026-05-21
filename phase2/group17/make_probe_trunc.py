import struct, os
 
BUN_MAGIC = 0x304E5542
HDR = "<IHHIQQQQQQ"
REC = "<IIQQQIIII"

ASSET_COUNT = 1_000_001
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