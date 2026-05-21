import struct


BUN_MAGIC = 0x304E5542  # BUN0 in little endian order


def construct_header(asset_count, asset_table_offset, string_table_offset, string_table_size, data_section_offset, data_section_size):
    """
    Constructs the header of the BUN file by defining the required fields outlined in the BUN specification. This function 
    uses format strings from python's struct library to pack the header fields into a binary string.
    """
    header = struct.pack('<I', BUN_MAGIC)
    header += struct.pack('<H', 1)           # version_major = 1
    header += struct.pack('<H', 0)           # version_minor = 0
    header += struct.pack('<I', asset_count)
    header += struct.pack('<Q', asset_table_offset)
    header += struct.pack('<Q', string_table_offset)
    header += struct.pack('<Q', string_table_size)
    header += struct.pack('<Q', data_section_offset)
    header += struct.pack('<Q', data_section_size)
    header += struct.pack('<Q', 0)               # reserved
    assert len(header) == 60
    return header

#populate the asset record
def construct_asset_record(name_offset, name_length, data_offset, data_size, uncompressed_size, compression, asset_type=0, checksum=0, flags=0):
    """
    Constructs the asset record of the BUN file by defining the required fields outlined in the BUN specification. This function 
    uses format strings from python's struct library to pack the asset record fields into a binary string.
    """
    asset_record = struct.pack('<I', name_offset)
    asset_record += struct.pack('<I', name_length)
    asset_record += struct.pack('<Q', data_offset)
    asset_record += struct.pack('<Q', data_size)
    asset_record += struct.pack('<Q', uncompressed_size)
    asset_record += struct.pack('<I', compression)
    asset_record += struct.pack('<I', asset_type)
    asset_record += struct.pack('<I', checksum)
    asset_record += struct.pack('<I', flags)
    assert len(asset_record) == 48
    return asset_record

def construct_f1():
    """
    Constructs the f1 test file, which is a BUN file with a large number of assets. This function demonstrates
    how the parser does not utilise sublinear memory allocation for scenario with large number of assets, which is
    a requirement defined in Phase 1 Project Brief section 5.3 -- sub-linear memory requirement. 
    """
    large_asset_count = 25_000_000          
    asset_table_offset = 64  
    asset_table_size = large_asset_count * 48     
    file_size = asset_table_offset + asset_table_size   

    header = construct_header(large_asset_count, asset_table_offset, asset_table_offset, 0, asset_table_offset, 0)

    fname = 'tests/f1-large-asset-count.bun'
    with open(fname, 'wb') as f:
        f.write(header)
        f.seek(file_size - 1)  # jump to the last byte
        f.write(b'\x00') # write a null byte

    #____ AI: claude sonnet 4.6 was used to wrote the following nicely formatted print summary____
    print(f"[Finding - No.1] Created {fname}")
    print(f"       asset_count = {large_asset_count:,}")
    print(f"       calloc(large_asset_count,48) would request {large_asset_count*48:,} bytes = {large_asset_count*48/2**30:.2f} GB")
    print(f"       calloc(large_asset_count, 4) would request {large_asset_count* 4:,} bytes = {large_asset_count* 4/2**20:.0f} MB")
    print(f"       Total allocation attempt: {(large_asset_count*52)/2**30:.2f} GB")
    #____


def construct_f2():
    """
    Constructs the f2 test file, which is a BUN file with a RLE asset that does not match the actual size of the data section.
    This function demonstrates how the parser does not abort when RLE uncompressed_size does not match actual size, which is
    a requirement defined in Phase 1 BUN specification section 5.1 Notes -- 4. Which states that if a parser detects that 
    compression is used, and that the actual uncompressed size is different to the value of uncompressed_size, it must abort 
    parsing and return BUN_MALFORMED.
    """
    asset_table_offset   = 60 
    string_table_offset   = 60 + 2 * 48  
    string_table_size  = 8
    data_section_offset  = string_table_offset + string_table_size 
    data_section_size = 8

    header = construct_header(2, asset_table_offset, string_table_offset, string_table_size, data_section_offset, data_section_size)

    # Asset 0: RLE, claims uncompressed_size=8 but actual=2, so abort should be triggered
    # RLE data: (1,'A'),(1,'B') → "AB" = 2 bytes uncompressed
    asset_zero = construct_asset_record(
        name_offset=0, 
        name_length=4,      
        data_offset=0, 
        data_size=4,    
        uncompressed_size=8,             
        compression=1,            
    )

    # Asset 1: valid and uncompressed, but should not be processed after abort
    asset_one = construct_asset_record(
        name_offset=4, 
        name_length=4,      
        data_offset=4, 
        data_size=4,    
        uncompressed_size=0,              
        compression=0,
    )

    #example 8 bytes in string table where 'fail' is asset 0's name and 'okay' is asset 1's name
    string_table = b'failokay' 

    #example data section "AB" + "ABCD", making the actual size of the data section 6 bytes, 
    # NOT 8 bytes as claimed by the asset record
    data_section = bytes([1,0x41, 1,0x42]) + b'ABCD'  

    out = header + asset_zero + asset_one + string_table + data_section
    assert len(out) == 172

    fname = 'tests/f2-rle-no-abort.bun'
    with open(fname, 'wb') as f:
        f.write(out)
    
    #____ AI: claude sonnet 4.6 was used to wrote the following nicely formatted print summary____
    print(f"\n[Finding - No.2] Created {fname} ({len(out)} bytes)")
    print(f"       Asset 0: RLE, claims uncompressed_size=8, actual=2 (mismatch)")
    print(f"       Asset 1: valid uncompressed 'okay'='ABCD' (should not appear)")
    #____

construct_f1()
construct_f2()

print("\nTest files for f1 and f2 generated successfully.")