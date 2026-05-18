#!/usr/bin/env python3
import struct
import os
os.makedirs('bun_files', exist_ok=True)

data_size = 50 * 1024 * 1024
uncompressed = data_size // 2

with open('bun_files/f3-rle-timeout.bun', 'wb') as f:
    f.write(struct.pack('<I', 0x304e5542))
    f.write(struct.pack('<H', 1))
    f.write(struct.pack('<H', 0))
    f.write(struct.pack('<I', 1))
    f.write(struct.pack('<Q', 60))
    f.write(struct.pack('<Q', 108))
    f.write(struct.pack('<Q', 16))
    f.write(struct.pack('<Q', 124))
    f.write(struct.pack('<Q', data_size))
    f.write(struct.pack('<Q', 0))
    f.write(struct.pack('<IIQQQIIII', 0, 4, 0, data_size, uncompressed, 1, 0, 0, 0))
    f.write(b'test\x00\x00' + b'\x00' * 10)
    f.write(b'\x01A' * (data_size // 2))

print(f"Created: {data_size} bytes")