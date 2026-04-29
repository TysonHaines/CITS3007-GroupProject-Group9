#include <stdio.h>
#include <stdlib.h>

#include "asset_validators.h"

#define NAME_VALIDATION_BUF_SIZE 1024


void validate_name_length(BunParseContext *ctx, u32 idx, u32 name_length) {
  if (name_length == 0) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "asset[%u]: name_length is zero (must be non-zero)", idx);
  }
}

void name_fits_string_table(BunParseContext *ctx, u32 idx,
                             u32 name_offset, u64 string_table_size,
                             u32 name_length) {
  // Overflow-safe per spec §9 rule 5: widen u32s to u64 before arithmetic.
  if ((u64)name_offset > string_table_size ||
      (u64)name_length > string_table_size - (u64)name_offset) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "asset[%u]: name out of string table bounds "
        "(offset=%u, length=%u, table_size=%llu)",
        idx, name_offset, name_length,
        (unsigned long long)string_table_size);
  }
}

void data_fits_data_section(BunParseContext *ctx, u32 idx,
                             u64 data_offset, u64 data_size,
                             const BunHeader *header) {
  // Overflow-safe: rearrange addition to avoid possible u64 wrap.
  if (data_offset > header->data_section_size ||
      data_size > header->data_section_size - data_offset) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "asset[%u]: data out of data section bounds "
        "(offset=%llu, size=%llu, section_size=%llu)",
        idx,
        (unsigned long long)data_offset,
        (unsigned long long)data_size,
        (unsigned long long)header->data_section_size);
  }
}

void validate_compression(BunParseContext *ctx, u32 idx,
                           u32 compression, u64 uncompressed_size,
                           u64 data_size, u64 data_offset,
                           const BunHeader *header) {
  if (compression == BUN_COMPRESS_NONE) {
    if (uncompressed_size != 0) {
      bun_add_violation(ctx, BUN_MALFORMED,
          "asset[%u]: uncompressed_size must be 0 when compression=none "
          "(got %llu)", idx, (unsigned long long)uncompressed_size);
    }
    return;
  }

  if (compression == BUN_COMPRESS_RLE) {
    if (uncompressed_size == 0) {
      bun_add_violation(ctx, BUN_MALFORMED,
          "asset[%u]: uncompressed_size must be non-zero for RLE compression",
          idx);
    }
    if (data_size % 2 != 0) {
      bun_add_violation(ctx, BUN_MALFORMED,
          "asset[%u]: RLE data size must be even (got %llu)",
          idx, (unsigned long long)data_size);
      return;  // can't safely walk odd-byte RLE data
    }

    // Bounds-check the actual file position for RLE data.
    u64 actual_offset = header->data_section_offset + data_offset;
    if (actual_offset > (u64)ctx->file_size ||
        data_size > (u64)ctx->file_size - actual_offset) {
      bun_add_violation(ctx, BUN_MALFORMED,
          "asset[%u]: RLE data extends past file end", idx);
      return;
    }

    // Walk every (count, byte) pair and check count != 0.
    long current_pos = ftell(ctx->file);
    if (fseek(ctx->file, (long)actual_offset, SEEK_SET) != 0) {
      return;  // I/O failure; let caller detect via other means
    }
    for (u64 j = 0; j < data_size; j += 2) {
      u8 pair[2];
      if (fread(pair, 1, 2, ctx->file) != 2) {
        return;
      }
      if (pair[0] == 0) {
        bun_add_violation(ctx, BUN_MALFORMED,
            "asset[%u]: RLE pair at offset %llu has zero count",
            idx, (unsigned long long)j);
        break;  // one violation per asset is enough
      }
    }
    if (current_pos >= 0) {
      fseek(ctx->file, current_pos, SEEK_SET);
    }
    return;
  }

  if (compression == BUN_COMPRESS_ZLIB) {
    bun_add_violation(ctx, BUN_UNSUPPORTED,
        "asset[%u]: zlib compression not supported by this parser", idx);
    return;
  }

  bun_add_violation(ctx, BUN_UNSUPPORTED,
      "asset[%u]: unsupported compression value %u", idx, compression);
}

void validate_non_zero_checksum(BunParseContext *ctx, u32 idx, u32 checksum) {
  if (checksum != 0) {
    bun_add_violation(ctx, BUN_UNSUPPORTED,
        "asset[%u]: non-zero checksum (0x%08X) - CRC-32 validation not supported",
        idx, checksum);
  }
}

void validate_flags(BunParseContext *ctx, u32 idx, u32 flags) {
  // Flags is a bitfield: any bit outside the known set is unsupported.
  u32 known = BUN_FLAG_ENCRYPTED | BUN_FLAG_EXECUTABLE;
  if (flags & ~known) {
    bun_add_violation(ctx, BUN_UNSUPPORTED,
        "asset[%u]: unknown flag bits set (0x%X)", idx, flags & ~known);
  }
}

void validate_asset_name(BunParseContext *ctx, u32 idx,
                          const BunHeader *header,
                          u32 name_offset, u32 name_length) {
  // Skip if other validators already flagged length issues.
  if (name_length == 0) return;

  long saved_pos = ftell(ctx->file);
  if (saved_pos < 0) return;

  u64 pos = (u64)header->string_table_offset + (u64)name_offset;
  if (fseek(ctx->file, (long)pos, SEEK_SET) != 0) return;

  u8 *name_buf = malloc(name_length);
  if (!name_buf) {
    fseek(ctx->file, saved_pos, SEEK_SET);
    return;
  }

  // Read the FULL name length so validation covers every byte
  // (fixes the original heap-buffer-overread bug).
  if (fread(name_buf, 1, name_length, ctx->file) != name_length) {
    free(name_buf);
    fseek(ctx->file, saved_pos, SEEK_SET);
    return;
  }

  for (u32 j = 0; j < name_length; j++) {
    if (name_buf[j] < 0x20 || name_buf[j] > 0x7E) {
      bun_add_violation(ctx, BUN_MALFORMED,
          "asset[%u]: name contains non-printable byte 0x%02X at index %u",
          idx, name_buf[j], j);
      break;
    }
  }

  free(name_buf);
  fseek(ctx->file, saved_pos, SEEK_SET);
}