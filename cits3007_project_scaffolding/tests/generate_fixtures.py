#!/usr/bin/env python3
"""
Generate test fixture BUN files for testing the parser.
"""

import struct
import os
import io

BUN_MAGIC = 0x304E5542
BUN_VERSION_MAJOR = 1
BUN_VERSION_MINOR = 0
BUN_COMPRESS_NONE = 0
BUN_COMPRESS_RLE = 1
BUN_COMPRESS_ZLIB = 2

HEADER_FMT = "<IHHIQQQQQQ"
RECORD_FMT = "<IIQQQIIII"
HEADER_SIZE = struct.calcsize(HEADER_FMT)
RECORD_SIZE = struct.calcsize(RECORD_FMT)

def _align4(n):
    return (n + 3) & ~3

def write_header(f, **kwargs):
    defaults = {
        'magic': BUN_MAGIC,
        'version_major': BUN_VERSION_MAJOR,
        'version_minor': BUN_VERSION_MINOR,
        'reserved': 0,
    }
    defaults.update(kwargs)
    data = struct.pack(HEADER_FMT,
        defaults['magic'],
        defaults['version_major'],
        defaults['version_minor'],
        defaults['asset_count'],
        defaults['asset_table_offset'],
        defaults['string_table_offset'],
        defaults['string_table_size'],
        defaults['data_section_offset'],
        defaults['data_section_size'],
        defaults['reserved'],
    )
    f.write(data)

def write_asset_record(f, **kwargs):
    defaults = {
        'name_offset': 0,
        'name_length': 0,
        'data_offset': 0,
        'data_size': 0,
        'uncompressed_size': 0,
        'compression': BUN_COMPRESS_NONE,
        'type': 0,
        'checksum': 0,
        'flags': 0,
    }
    defaults.update(kwargs)
    data = struct.pack(RECORD_FMT,
        defaults['name_offset'],
        defaults['name_length'],
        defaults['data_offset'],
        defaults['data_size'],
        defaults['uncompressed_size'],
        defaults['compression'],
        defaults['type'],
        defaults['checksum'],
        defaults['flags'],
    )
    f.write(data)

def build_single_asset(name, data, compression=BUN_COMPRESS_NONE, uncompressed_size=0, name_offset_override=None, data_offset_override=None, flags=0, checksum=0):
    """Build a single-asset BUN file with proper layout."""
    f = io.BytesIO()
    
    asset_table_offset = _align4(HEADER_SIZE)
    string_table_offset = _align4(asset_table_offset + RECORD_SIZE)
    string_table_size = _align4(len(name))
    string_table_start = string_table_offset
    
    name_offset = name_offset_override if name_offset_override is not None else 0
    data_offset = data_offset_override if data_offset_override is not None else 0
    
    # First compute expected data_section_offset
    data_section_offset = _align4(string_table_offset + string_table_size)
    data_section_size = _align4(len(data))
    
    write_header(f,
        asset_count=1,
        asset_table_offset=asset_table_offset,
        string_table_offset=string_table_offset,
        string_table_size=string_table_size,
        data_section_offset=data_section_offset,
        data_section_size=data_section_size,
    )
    
    # Padding to asset table
    f.write(b'\x00' * (asset_table_offset - HEADER_SIZE))
    
    # Asset record
    write_asset_record(f, 
        name_offset=name_offset,
        name_length=len(name),
        data_offset=data_offset,
        data_size=len(data),
        compression=compression,
        uncompressed_size=uncompressed_size,
        flags=flags,
        checksum=checksum,
    )
    
    # Padding to string table
    f.write(b'\x00' * (string_table_offset - (asset_table_offset + RECORD_SIZE)))
    
    # String table
    f.write(name)
    f.write(b'\x00' * (string_table_size - len(name)))
    
    # Padding to data section
    f.write(b'\x00' * (data_section_offset - (string_table_offset + string_table_size)))
    
    # Data section
    f.write(data)
    f.write(b'\x00' * (data_section_size - len(data)))
    
    return f.getvalue()

def build_two_assets(name1, data1, name2, data2):
    """Build a two-asset BUN file."""
    f = io.BytesIO()
    asset_count = 2
    
    asset_table_offset = _align4(HEADER_SIZE)
    string_table_offset = _align4(asset_table_offset + asset_count * RECORD_SIZE)
    string_table_size = _align4(len(name1) + len(name2))
    data_section_offset = _align4(string_table_offset + string_table_size)
    data_section_size = _align4(len(data1) + len(data2))
    
    write_header(f,
        asset_count=asset_count,
        asset_table_offset=asset_table_offset,
        string_table_offset=string_table_offset,
        string_table_size=string_table_size,
        data_section_offset=data_section_offset,
        data_section_size=data_section_size,
    )
    
    # Padding to asset table
    pos = f.tell()
    f.write(b'\x00' * (asset_table_offset - HEADER_SIZE))
    
    # Asset records
    write_asset_record(f, name_offset=0, name_length=len(name1),
                      data_offset=0, data_size=len(data1))
    write_asset_record(f, name_offset=len(name1), name_length=len(name2),
                      data_offset=len(data1), data_size=len(data2))
    
    # Padding to string table
    f.write(b'\x00' * (string_table_offset - (asset_table_offset + asset_count * RECORD_SIZE)))
    
    # String table
    f.write(name1)
    f.write(name2)
    f.write(b'\x00' * (string_table_size - len(name1) - len(name2)))
    
    # Padding to data section
    f.write(b'\x00' * (data_section_offset - (string_table_offset + string_table_size)))
    
    # Data section
    f.write(data1)
    f.write(data2)
    f.write(b'\x00' * (data_section_size - len(data1) - len(data2)))
    
    return f.getvalue()

def generate_valid_files(output_dir):
    """Generate all valid test fixtures."""
    os.makedirs(output_dir + '/valid', exist_ok=True)
    
    # 01-empty.bun
    with open(output_dir + '/valid/01-empty.bun', 'wb') as f:
        f.write(build_single_asset(b"hello", b"Hello, BUN world!"))
    
    # 02-single-asset.bun
    with open(output_dir + '/valid/02-single-asset.bun', 'wb') as f:
        f.write(build_single_asset(b"test_asset", b"test data content"))
    
    # 03-multi-asset.bun
    with open(output_dir + '/valid/03-multi-asset.bun', 'wb') as f:
        f.write(build_two_assets(b"asset_one", b"data_one", b"asset_two", b"data_two"))
    
    # 04-rle-asset.bun - RLE unsupported (but valid format)
    with open(output_dir + '/valid/04-rle-asset.bun', 'wb') as f:
        f.write(build_single_asset(b"rle_asset", b"\x03\x07", compression=BUN_COMPRESS_RLE, uncompressed_size=3))
    
    # 05-encrypted.bun - encrypted flag
    with open(output_dir + '/valid/05-encrypted.bun', 'wb') as f:
        f.write(build_single_asset(b"enc_asset", b"encrypted data", flags=0x1))
    
    # 06-executable.bun - executable flag
    with open(output_dir + '/valid/06-executable.bun', 'wb') as f:
        f.write(build_single_asset(b"exe_asset", b"executable data", flags=0x2))
    
    # 07-two-simple.bun - two simple assets
    with open(output_dir + '/valid/07-two-simple.bun', 'wb') as f:
        f.write(build_two_assets(b"alpha", b"AAA", b"beta", b"BBB"))
    
    print("Generated valid fixtures in", output_dir + '/valid')

def generate_invalid_files(output_dir):
    """Generate all invalid test fixtures."""
    os.makedirs(output_dir + '/invalid', exist_ok=True)
    
    # 01-bad-magic.bun
    with open(output_dir + '/invalid/01-bad-magic.bun', 'wb') as f:
        f.write(build_single_asset(b"test", b"data", name_offset_override=0))  # will patch magic
    
    # Patch the magic in file 01
    with open(output_dir + '/invalid/01-bad-magic.bun', 'r+b') as f:
        f.seek(0)
        f.write(struct.pack('<I', 0xDEADBEEF))
    
    # 02-bad-version.bun - version 2.0
    f = io.BytesIO()
    write_header(f,
        magic=BUN_MAGIC,
        version_major=2,
        version_minor=0,
        asset_count=1,
        asset_table_offset=_align4(HEADER_SIZE),
        string_table_offset=_align4(HEADER_SIZE + RECORD_SIZE),
        string_table_size=4,
        data_section_offset=_align4(HEADER_SIZE + RECORD_SIZE + 4),
        data_section_size=4,
    )
    f.write(b'\x00' * (_align4(HEADER_SIZE) - HEADER_SIZE))
    write_asset_record(f, name_offset=0, name_length=4, data_offset=0, data_size=4)
    f.write(b"test")
    f.write(b"data")
    with open(output_dir + '/invalid/02-bad-version.bun', 'wb') as out:
        out.write(f.getvalue())
    
    # 03-offsets-not-div4.bun
    f = io.BytesIO()
    write_header(f,
        magic=BUN_MAGIC,
        version_major=1,
        version_minor=0,
        asset_count=1,
        asset_table_offset=61,  # Not divisible by 4!
        string_table_offset=_align4(61 + RECORD_SIZE),
        string_table_size=4,
        data_section_offset=_align4(61 + RECORD_SIZE + 4),
        data_section_size=4,
    )
    f.write(b'\x00' * 61)
    write_asset_record(f, name_offset=0, name_length=4, data_offset=0, data_size=4)
    f.write(b"test")
    f.write(b"data")
    with open(output_dir + '/invalid/03-offsets-not-div4.bun', 'wb') as out:
        out.write(f.getvalue())
    
    # 04-truncated-header.bun - file ends during header read
    with open(output_dir + '/invalid/04-truncated-header.bun', 'wb') as f:
        f.write(b'\x00' * 30)  # Only 30 bytes instead of 60
    
    # 05-section-out-of-bounds.bun - string_table_size too large
    with open(output_dir + '/invalid/04-section-out-of-bounds.bun', 'wb') as f:
        f.write(build_single_asset(b"test", b"data", name_offset_override=9999))
    
    # 05-sections-overlap.bun
    f = io.BytesIO()
    asset_table_offset = _align4(HEADER_SIZE)
    write_header(f,
        magic=BUN_MAGIC,
        version_major=1,
        version_minor=0,
        asset_count=1,
        asset_table_offset=asset_table_offset,
        string_table_offset=asset_table_offset + 20,  # Overlaps!
        string_table_size=20,
        data_section_offset=asset_table_offset + 20 + 20,
        data_section_size=20,
    )
    f.write(b'\x00' * (asset_table_offset - HEADER_SIZE))
    write_asset_record(f)
    f.write(b"test")
    with open(output_dir + '/invalid/05-sections-overlap.bun', 'wb') as out:
        out.write(f.getvalue())
    
    # 06-name-oob.bun - name_offset beyond string table
    with open(output_dir + '/invalid/06-name-oob.bun', 'wb') as f:
        f.write(build_single_asset(b"test", b"data", name_offset_override=9999))
    
    # 07-data-oob.bun - data_offset beyond data section
    with open(output_dir + '/invalid/07-data-oob.bun', 'wb') as f:
        f.write(build_single_asset(b"test", b"data", data_offset_override=9999))
    
    # 08-name-non-printable.bun
    with open(output_dir + '/invalid/08-name-non-printable.bun', 'wb') as f:
        f.write(build_single_asset(b"\x01test", b"data"))
    
    # 09-zero-name.bun
    f = io.BytesIO()
    asset_table_offset = _align4(HEADER_SIZE)
    write_header(f,
        magic=BUN_MAGIC,
        version_major=1,
        version_minor=0,
        asset_count=1,
        asset_table_offset=asset_table_offset,
        string_table_offset=_align4(asset_table_offset + RECORD_SIZE),
        string_table_size=4,
        data_section_offset=_align4(asset_table_offset + RECORD_SIZE + 4),
        data_section_size=4,
    )
    f.write(b'\x00' * (asset_table_offset - HEADER_SIZE))
    write_asset_record(f, name_offset=0, name_length=0, data_offset=0, data_size=4)
    f.write(b'\x00' * 4)
    f.write(b"data")
    with open(output_dir + '/invalid/09-zero-name.bun', 'wb') as out:
        out.write(f.getvalue())
    
    # 10-rle-compression.bun - RLE
    with open(output_dir + '/invalid/10-rle-compression.bun', 'wb') as f:
        f.write(build_single_asset(b"test", b"\x03\x07", compression=BUN_COMPRESS_RLE, uncompressed_size=3))
    
    # 11-zlib-compression.bun - zlib
    with open(output_dir + '/invalid/11-zlib-compression.bun', 'wb') as f:
        f.write(build_single_asset(b"test", b"\x78\x9c", compression=BUN_COMPRESS_ZLIB, uncompressed_size=10))
    
    # 12-unknown-compression.bun - unknown
    with open(output_dir + '/invalid/12-unknown-compression.bun', 'wb') as f:
        f.write(build_single_asset(b"test", b"data", compression=99))
    
    # 13-rle-odd-size.bun - odd RLE data
    with open(output_dir + '/invalid/13-rle-odd-size.bun', 'wb') as f:
        f.write(build_single_asset(b"test", b"\x03\x07\x01", compression=BUN_COMPRESS_RLE, uncompressed_size=3))
    
    # 14-rle-zero-count.bun - zero RLE count
    with open(output_dir + '/invalid/14-rle-zero-count.bun', 'wb') as f:
        f.write(build_single_asset(b"test", b"\x00\x07", compression=BUN_COMPRESS_RLE, uncompressed_size=1))
    
    # 15-empty-file.bun
    with open(output_dir + '/invalid/15-empty-file.bun', 'wb') as f:
        pass
    
    # 16-bad-version-major.bun - version_major != 1
    f = io.BytesIO()
    write_header(f,
        magic=BUN_MAGIC,
        version_major=99,  # Not 1!
        version_minor=0,
        asset_count=1,
        asset_table_offset=_align4(HEADER_SIZE),
        string_table_offset=_align4(HEADER_SIZE + RECORD_SIZE),
        string_table_size=4,
        data_section_offset=_align4(HEADER_SIZE + RECORD_SIZE + 4),
        data_section_size=4,
    )
    f.write(b'\x00' * (_align4(HEADER_SIZE) - HEADER_SIZE))
    write_asset_record(f, name_offset=0, name_length=4, data_offset=0, data_size=4)
    f.write(b"test")
    f.write(b"data")
    with open(output_dir + '/invalid/16-bad-version-major.bun', 'wb') as out:
        out.write(f.getvalue())
    
    # 17-truncated-header.bun - only partial header
    with open(output_dir + '/invalid/17-truncated-header.bun', 'wb') as f:
        f.write(b'\x00' * 30)  # Too short for header
    
    # 18-checksum-nonzero.bun - checksum != 0
    with open(output_dir + '/invalid/18-checksum-nonzero.bun', 'wb') as f:
        f.write(build_single_asset(b"test", b"data", checksum=0xDEADBEEF))
    
    # 19-flags-unknown.bun
    with open(output_dir + '/invalid/19-flags-unknown.bun', 'wb') as f:
        f.write(build_single_asset(b"test", b"data", flags=0x3))
    
    print("Generated invalid fixtures in", output_dir + '/invalid')

if __name__ == "__main__":
    output_dir = "tests/fixtures"
    generate_valid_files(output_dir)
    generate_invalid_files(output_dir)
    print("All fixtures generated!")