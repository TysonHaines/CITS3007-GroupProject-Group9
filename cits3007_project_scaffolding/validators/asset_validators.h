#pragma once
#include "bun.h"

void validate_name_length(BunParseContext *ctx, u32 idx, u32 name_length);

void name_fits_string_table(BunParseContext *ctx, u32 idx,
                             u32 name_offset, u64 string_table_size,
                             u32 name_length);

void data_fits_data_section(BunParseContext *ctx, u32 idx,
                             u64 data_offset, u64 data_size,
                             const BunHeader *header);

void validate_compression(BunParseContext *ctx, u32 idx,
                           u32 compression, u64 uncompressed_size,
                           u64 data_size, u64 data_offset,
                           const BunHeader *header);

void validate_non_zero_checksum(BunParseContext *ctx, u32 idx, u32 checksum);

void validate_flags(BunParseContext *ctx, u32 idx, u32 flags);

void validate_asset_name(BunParseContext *ctx, u32 idx,
                          const BunHeader *header,
                          u32 name_offset, u32 name_length);