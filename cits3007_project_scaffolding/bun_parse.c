#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "bun.h"
#include "validators/asset_validators.h"
#include "validators/header_validators.h"

//
// Little-endian read helpers
//

static u32 read_u32_le(const u8 *buf, size_t offset) {
  return (u32)buf[offset]
     | (u32)buf[offset + 1] << 8
     | (u32)buf[offset + 2] << 16
     | (u32)buf[offset + 3] << 24;
}

static u16 read_u16_le(const u8 *buf, size_t offset) {
  return (u16)buf[offset]
     | (u16)buf[offset + 1] << 8;
}

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
  // Open file, measure its size, rewind ready for parsing.

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

  if (ctx->file_size < (long)BUN_HEADER_SIZE) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "file too short for header (file_size=%ld, header_size=%d)",
        ctx->file_size, BUN_HEADER_SIZE);
    return BUN_MALFORMED;
  }

  if (fread(buf, 1, BUN_HEADER_SIZE, ctx->file) != BUN_HEADER_SIZE) {
    return BUN_ERR_IO;
  }

  // Populate header from buffer.
  header->magic               = read_u32_le(buf, 0);
  header->version_major       = read_u16_le(buf, 4);
  header->version_minor       = read_u16_le(buf, 6);
  header->asset_count         = read_u32_le(buf, 8);
  header->asset_table_offset  = read_u64_le(buf, 12);
  header->string_table_offset = read_u64_le(buf, 20);
  header->string_table_size   = read_u64_le(buf, 28);
  header->data_section_offset = read_u64_le(buf, 36);
  header->data_section_size   = read_u64_le(buf, 44);
  header->reserved            = read_u64_le(buf, 52);

  // Run all header validators -- they accumulate violations in ctx.
  validate_magic(ctx, header);
  validate_offsets(ctx, header);
  validate_version(ctx, header);

  u64 asset_table_size = (u64)header->asset_count * BUN_ASSET_RECORD_SIZE;
  u64 file_size = (u64)ctx->file_size;

  validate_asset_table_size(ctx, header, file_size, asset_table_size);
  validate_string_table_size(ctx, header, file_size);
  validate_data_section_size(ctx, header, file_size);
  validate_no_overlap(ctx, header, asset_table_size);

  return bun_worst_violation(ctx);
}

bun_result_t bun_parse_assets(BunParseContext *ctx, const BunHeader *header) {
  if (fseek(ctx->file, (long)header->asset_table_offset, SEEK_SET) != 0) {
    return BUN_ERR_IO;
  }

  for (u32 i = 0; i < header->asset_count; i++) {
    u8 buf[BUN_ASSET_RECORD_SIZE];
    if (fread(buf, 1, BUN_ASSET_RECORD_SIZE, ctx->file) != BUN_ASSET_RECORD_SIZE) {
      return BUN_ERR_IO;
    }

    u32 name_offset       = read_u32_le(buf, 0);
    u32 name_length       = read_u32_le(buf, 4);
    u64 data_offset       = read_u64_le(buf, 8);
    u64 data_size         = read_u64_le(buf, 16);
    u64 uncompressed_size = read_u64_le(buf, 24);
    u32 compression       = read_u32_le(buf, 32);
    u32 checksum          = read_u32_le(buf, 40);
    u32 flags             = read_u32_le(buf, 44);

    // Run all asset validators -- they accumulate violations in ctx.
    validate_name_length(ctx, i, name_length);
    name_fits_string_table(ctx, i, name_offset, header->string_table_size, name_length);
    data_fits_data_section(ctx, i, data_offset, data_size, header);
    validate_compression(ctx, i, compression, uncompressed_size, data_size, data_offset, header);
    validate_non_zero_checksum(ctx, i, checksum);
    validate_flags(ctx, i, flags);
    validate_asset_name(ctx, i, header, name_offset, name_length);
  }

  return bun_worst_violation(ctx);
}

bun_result_t bun_close(BunParseContext *ctx) {
  // Caller must hold an open file
  assert(ctx->file);

  // Free the violation list before closing the file.
  bun_free_violations(ctx);

  // Close the stream and clear the file pointer.
  int res = fclose(ctx->file);
  ctx->file = NULL;

  return res ? BUN_ERR_IO : BUN_OK;
}
