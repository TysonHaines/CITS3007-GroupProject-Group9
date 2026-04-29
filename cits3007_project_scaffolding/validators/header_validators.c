#include <stdio.h>
#include <stdlib.h>

#include "header_validators.h"

void validate_magic(const BunHeader *header) {
  // validate magic number is 'BUN0' (0x304E5542 (little-endian))
  if (header->magic != BUN_MAGIC) {
    fprintf(stderr, "\nMagic number is not 'BUN0' (0x304E5542 (little-endian))\n");
    file_status = BUN_MALFORMED;
  }
}

void validate_offsets(const BunHeader *header) {
  // validate offsets and sizes are divisible by 4
  if (header->asset_table_offset % 4 != 0 ||
    header->string_table_offset % 4 != 0 || 
    header->string_table_size % 4 != 0 ||
    header->data_section_offset % 4 != 0 ||
    header->data_section_size % 4 != 0) {
      fprintf(stderr, "\nOffsets and sizes must be divisible by 4\n");
      file_status = BUN_MALFORMED;
  }
}

void validate_version(const BunHeader *header) {
  // validate version is 1 or 0 otherwise unsupported
  if (header->version_major != BUN_VERSION_MAJOR || header->version_minor != BUN_VERSION_MINOR) {
    fprintf(stderr, "\nVersion is not 1 or 0\n");
    file_status = BUN_UNSUPPORTED;
  }
}

void validate_asset_count(const BunHeader *header) {
  // Guard against overflow when computing asset table size
  if (header->asset_count > UINT64_MAX / 48) {
    fprintf(stderr, "\nAsset count is too large\n");
    file_status = BUN_MALFORMED;
  }
}

void validate_asset_table_size(const BunHeader *header, u64 file_size, u64 asset_table_size) {
  if (header->asset_table_offset > file_size ||
      asset_table_size > file_size - header->asset_table_offset) {
    fprintf(stderr, "\nAsset table is too large\n");
    file_status = BUN_MALFORMED;
  }
}

void validate_string_table_size(const BunHeader *header, u64 file_size) {
  if (header->string_table_offset > file_size ||
    header->string_table_size > file_size - header->string_table_offset) {
    fprintf(stderr, "\nString table is too large\n");
    file_status = BUN_MALFORMED;
  }
}

void validate_data_section_size(const BunHeader *header, u64 file_size) {
  if (header->data_section_offset > file_size ||
      header->data_section_size > file_size - header->data_section_offset) {
    fprintf(stderr, "\nData section is too large\n");
    file_status = BUN_MALFORMED;
  }
}

void validate_no_overlap(const BunHeader *header, u64 asset_table_size) {
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
    file_status = BUN_MALFORMED;
  }
  // if asset table and data section overlap
  if (asset_table_start < data_section_end && data_section_start < asset_table_end) {
    fprintf(stderr, "\nAsset table and data section overlap\n");
    file_status = BUN_MALFORMED;
  }
  // if string table and data section overlap
  if (string_table_start < data_section_end && data_section_start < string_table_end) {
    fprintf(stderr, "\nString table and data section overlap\n");
    file_status = BUN_MALFORMED;
  }
}