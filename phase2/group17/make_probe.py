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