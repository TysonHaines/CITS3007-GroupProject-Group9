#include "header_validators.h"

void validate_magic(BunParseContext *ctx, const BunHeader *header) {
  if (header->magic != BUN_MAGIC) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "magic number invalid: expected 0x%08X (\"BUN0\"), got 0x%08X",
        BUN_MAGIC, header->magic);
  }
}

void validate_offsets(BunParseContext *ctx, const BunHeader *header) {
  if (header->asset_table_offset % 4 != 0) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "asset_table_offset (%llu) not divisible by 4",
        (unsigned long long)header->asset_table_offset);
  }
  if (header->string_table_offset % 4 != 0) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "string_table_offset (%llu) not divisible by 4",
        (unsigned long long)header->string_table_offset);
  }
  if (header->string_table_size % 4 != 0) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "string_table_size (%llu) not divisible by 4",
        (unsigned long long)header->string_table_size);
  }
  if (header->data_section_offset % 4 != 0) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "data_section_offset (%llu) not divisible by 4",
        (unsigned long long)header->data_section_offset);
  }
  if (header->data_section_size % 4 != 0) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "data_section_size (%llu) not divisible by 4",
        (unsigned long long)header->data_section_size);
  }
}

void validate_version(BunParseContext *ctx, const BunHeader *header) {
  if (header->version_major != BUN_VERSION_MAJOR ||
      header->version_minor != BUN_VERSION_MINOR) {
    bun_add_violation(ctx, BUN_UNSUPPORTED,
        "version %u.%u not supported (expected %u.%u)",
        header->version_major, header->version_minor,
        BUN_VERSION_MAJOR, BUN_VERSION_MINOR);
  }
}

void validate_asset_table_size(BunParseContext *ctx, const BunHeader *header,
                                u64 file_size, u64 asset_table_size) {
  if (header->asset_table_offset > file_size ||
      asset_table_size > file_size - header->asset_table_offset) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "asset table extends past file end (offset=%llu, size=%llu, file_size=%llu)",
        (unsigned long long)header->asset_table_offset,
        (unsigned long long)asset_table_size,
        (unsigned long long)file_size);
  }
}

void validate_string_table_size(BunParseContext *ctx, const BunHeader *header,
                                 u64 file_size) {
  if (header->string_table_offset > file_size ||
      header->string_table_size > file_size - header->string_table_offset) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "string table extends past file end (offset=%llu, size=%llu, file_size=%llu)",
        (unsigned long long)header->string_table_offset,
        (unsigned long long)header->string_table_size,
        (unsigned long long)file_size);
  }
}

void validate_data_section_size(BunParseContext *ctx, const BunHeader *header,
                                 u64 file_size) {
  if (header->data_section_offset > file_size ||
      header->data_section_size > file_size - header->data_section_offset) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "data section extends past file end (offset=%llu, size=%llu, file_size=%llu)",
        (unsigned long long)header->data_section_offset,
        (unsigned long long)header->data_section_size,
        (unsigned long long)file_size);
  }
}

void validate_no_overlap(BunParseContext *ctx, const BunHeader *header,
                          u64 asset_table_size) {
  u64 a_start = header->asset_table_offset;
  u64 a_end   = a_start + asset_table_size;

  u64 s_start = header->string_table_offset;
  u64 s_end   = s_start + header->string_table_size;

  u64 d_start = header->data_section_offset;
  u64 d_end   = d_start + header->data_section_size;

  if (a_start < s_end && s_start < a_end) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "asset table and string table overlap");
  }
  if (a_start < d_end && d_start < a_end) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "asset table and data section overlap");
  }
  if (s_start < d_end && d_start < s_end) {
    bun_add_violation(ctx, BUN_MALFORMED,
        "string table and data section overlap");
  }
}
