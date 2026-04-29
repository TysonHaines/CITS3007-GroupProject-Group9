#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "bun.h"
#include "bun_validators.h"
#include <errno.h>

/**
 * Example helper: convert 4 bytes in `buf`, positioned at `offset`,
 * into a little-endian u32.
 */
static u32 read_u32_le(const u8 *buf, size_t offset) {
  return (u32)buf[offset]
     | (u32)buf[offset + 1] << 8
     | (u32)buf[offset + 2] << 16
     | (u32)buf[offset + 3] << 24;
}

// Helper: convert 2 bytes in `buf`, positioned at `offset`, into a little-endian u16.
static u16 read_u16_le(const u8 *buf, size_t offset) {
  return (u16)buf[offset]
     | (u16)buf[offset + 1] << 8;
}

// Helper: convert 8 bytes in `buf`, positioned at `offset`, into a little-endian u64.
static u64 read_u64_le(const u8 *buf, size_t offset) {
  return (u64)buf[offset]
     | (u64)buf[offset + 1] << 8
     | (u64)buf[offset + 2] << 16
     | (u64)buf[offset + 3] << 24
     | (u64)buf[offset + 4] << 32
     | (u64)buf[offset + 5] << 40
     | (u64)buf[offset + 6] << 48
     | (u64)buf[offset + 7] << 56;
}

//
// API implementation
//

bun_result_t bun_open(const char *path, BunParseContext *ctx) {
  // we open the file; seek to the end, to get the size; then jump back to the
  // beginning, ready to start parsing.

  // Open in binary mode; distinguish missing file from other I/O errors.
  ctx->file = fopen(path, "rb");
  if (!ctx->file) {
    return errno == ENOENT ? BUN_ERR_NOT_FOUND : BUN_ERR_IO;
  }

  // Seek to end to measure size.
  if (fseek(ctx->file, 0, SEEK_END) != 0) {
    fclose(ctx->file);
    ctx->file = NULL;
    return BUN_ERR_IO;
  }

  // ftell after SEEK_END gives file size; negative means error.
  ctx->file_size = ftell(ctx->file);
  if (ctx->file_size < 0) {
    fclose(ctx->file);
    ctx->file = NULL;
    return BUN_ERR_IO;
  }

  // Rewind for header parse.
  rewind(ctx->file);

  return BUN_OK;
}

bun_result_t bun_parse_header(BunParseContext *ctx, BunHeader *header) {
  u8 buf[BUN_HEADER_SIZE];

  // our file is far too short, and cannot be valid!
  // (query: how do we let `main` know that "file was too short"
  // was the exact problem? Where can we put details about the
  // exact validation problem that occurred?)
  if (ctx->file_size < (long)BUN_HEADER_SIZE) {
    return BUN_MALFORMED;
  }

  // slurp the header into `buf`
  if (fread(buf, 1, BUN_HEADER_SIZE, ctx->file) != BUN_HEADER_SIZE) {
    return BUN_ERR_IO;
  }


  // populate `header` from `buf`.
  header->magic = read_u32_le(buf, 0);
  header->version_major = read_u16_le(buf, 4);
  header->version_minor = read_u16_le(buf, 6);
  header->asset_count = read_u32_le(buf, 8);
  header->asset_table_offset = read_u64_le(buf, 12);
  header->string_table_offset = read_u64_le(buf, 20);
  header->string_table_size = read_u64_le(buf, 28);
  header->data_section_offset = read_u64_le(buf, 36);
  header->data_section_size = read_u64_le(buf, 44);
  header->reserved = read_u64_le(buf, 52);


  // validate fields and return BUN_MALFORMED or BUN_UNSUPPORTED

  // validate magic number is 'BUN0' (0x304E5542 (little-endian))
  if (header->magic != BUN_MAGIC) {
    fprintf(stderr, "\nMagic number is not 'BUN0' (0x304E5542 (little-endian))\n");
    return BUN_MALFORMED;
  }
  // validate offsets and sizes are divisible by 4
  if (header->asset_table_offset % 4 != 0 ||
    header->string_table_offset % 4 != 0 || 
    header->string_table_size % 4 != 0 ||
    header->data_section_offset % 4 != 0 ||
    header->data_section_size % 4 != 0) {
      fprintf(stderr, "\nOffsets and sizes must be divisible by 4\n");
      return BUN_MALFORMED;
  }
  
  // validate version is 1 or 0 otherwise unsupported
  if (header->version_major != BUN_VERSION_MAJOR || header->version_minor != BUN_VERSION_MINOR) {
    fprintf(stderr, "\nVersion is not 1 or 0\n");
    return BUN_UNSUPPORTED;
  }

  // Guard against overflow when computing asset table size
  if (header->asset_count > UINT32_MAX / 48) {
    fprintf(stderr, "\nAsset count is too large\n");
    return BUN_MALFORMED;
  }
  u64 asset_table_size = (u64)header->asset_count * 48;

  // Validate all sections lie entirely within the file
  u64 file_size = (u64)ctx->file_size;

  if (header->asset_table_offset > file_size ||
      asset_table_size > file_size - header->asset_table_offset) {
    fprintf(stderr, "\nAsset table is too large\n");
    return BUN_MALFORMED;
  }
  if (header->string_table_offset > file_size ||
      header->string_table_size > file_size - header->string_table_offset) {
    fprintf(stderr, "\nString table is too large\n");
    return BUN_MALFORMED;
  }
  if (header->data_section_offset > file_size ||
      header->data_section_size > file_size - header->data_section_offset) {
    fprintf(stderr, "\nData section is too large\n");
    return BUN_MALFORMED;
  }

  // Validate sections do not overlap each other
  u64 asset_table_start = header->asset_table_offset;
  u64 asset_table_end   = asset_table_start + asset_table_size;

  u64 string_table_start = header->string_table_offset;
  u64 string_table_end   = string_table_start + header->string_table_size;

  u64 data_section_start = header->data_section_offset;
  u64 data_section_end   = data_section_start + header->data_section_size;

  // if asset table and string table overlap
  if (asset_table_start < string_table_end && string_table_start < asset_table_end) {
    fprintf(stderr, "\nAsset table and string table overlap\n");
    return BUN_MALFORMED;
  }
  // if asset table and data section overlap
  if (asset_table_start < data_section_end && data_section_start < asset_table_end) {
    fprintf(stderr, "\nAsset table and data section overlap\n");
    return BUN_MALFORMED;
  }
  // if string table and data section overlap
  if (string_table_start < data_section_end && data_section_start < string_table_end) {
    fprintf(stderr, "\nString table and data section overlap\n");
    return BUN_MALFORMED;
  }

  return BUN_OK;
}

bun_result_t bun_parse_assets(BunParseContext *ctx, const BunHeader *header) {
  // implements asset record parsing and validation
  
  //Jump to start position of asset table before reading
  if (fseek(ctx->file, (long)header->asset_table_offset, SEEK_SET) != 0) {
    fprintf(stderr, "\nFailed to jump to start position of asset table\n");
    return BUN_ERR_IO;
  }
  //Loop over each asset record in the asset table
  for (u32 i = 0; i < header->asset_count; i++) {
    u8 buf[BUN_ASSET_RECORD_SIZE];
    
    char asset_name[61] = {0}; // 60 chars + null terminator

    // read 48 bytes from file into buf and check enough bytes exist
    if (fread(buf, 1, BUN_ASSET_RECORD_SIZE, ctx->file) != BUN_ASSET_RECORD_SIZE) {
      fprintf(stderr, "\nFailed to read asset record\n");
      return BUN_ERR_IO; 
    }
    //Parse each field from buffer
    u32 name_offset = read_u32_le(buf, 0);
    u32 name_length = read_u32_le(buf, 4);
    u64 data_offset = read_u64_le(buf, 8);
    u64 data_size = read_u64_le(buf, 16);
    u64 uncompressed_size = read_u64_le(buf, 24);
    u32 compression = read_u32_le(buf, 32);
    u32 type = read_u32_le(buf, 36);
    u32 checksum = read_u32_le(buf, 40);
    u32 flags = read_u32_le(buf, 44);

    //Validate Fields for each asset record

    // validate names are non-zero
    validate_name_length(name_length);

    // validate name fits in string table. Note: (cast to u64 to avoid overflow)
    name_fits_string_table(name_offset, header->string_table_size, name_length);

    // validate data fits inside data section.
    data_fits_data_section(data_offset, data_size, header);

    // validate compression value exists and is recognised
    //If no compression, uncompressed size must be 0 (special value)
    validate_compression(compression, uncompressed_size, data_size, data_offset, ctx, header);
    
    // validate check sum is non-zero
    validate_non_zero_checksum(checksum);

    // validate flags are known
    validate_flags(flags);

    // validate asset names
    validate_asset_name(header, ctx, name_offset, name_length, asset_name);
  }
  return BUN_OK;
}

bun_result_t bun_close(BunParseContext *ctx) {
  assert(ctx->file);

  int res = fclose(ctx->file);
  if (res) {
    return BUN_ERR_IO;
  } else {
    ctx->file = NULL;
    return BUN_OK;
  }
}

//______
void bun_print_header(const BunHeader *header) {
  printf("=== BUN Header ===\n");
  printf("  Magic:               0x%08X\n", header->magic);
  printf("  Version:             %u.%u\n", header->version_major, header->version_minor);
  printf("  Asset count:         %u\n", header->asset_count);
  printf("  Asset table offset:  %llu\n", (unsigned long long)header->asset_table_offset);
  printf("  String table offset: %llu\n", (unsigned long long)header->string_table_offset);
  printf("  String table size:   %llu\n", (unsigned long long)header->string_table_size);
  printf("  Data section offset: %llu\n", (unsigned long long)header->data_section_offset);
  printf("  Data section size:   %llu\n", (unsigned long long)header->data_section_size);
  printf("  Reserved:            %llu\n", (unsigned long long)header->reserved);
  printf("\n");
}
//______
