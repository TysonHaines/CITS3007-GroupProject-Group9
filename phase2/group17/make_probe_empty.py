import struct, os
 
BUN_MAGIC = 0x304E5542
header = struct.pack("<IHHIQQQQQQ",
    BUN_MAGIC, 1, 0,   # magic, version 1.0
    0,                  # asset_count = 0
    60, 60, 0, 60, 0, 0)  # ATO, STO, STS, DSO, DSS, reserved

with open("probe_empty.bun", "wb") as f:
    f.write(header)
print("Wrote", os.path.getsize("probe_empty.bun"), "bytes")