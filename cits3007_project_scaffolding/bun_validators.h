#pragma once
#include "bun.h"

bun_result_t validate_name_length(u32 name_length);

bun_result_t name_fits_string_table(u32 name_offset, u64 string_table_size, u32 name_length);

bun_result_t data_fits_data_section(u64 data_offset, u64 data_size, const BunHeader *header);

bun_result_t validate_compression(u32 compression, u64 uncompressed_size, u64 data_size, u64 data_offset, BunParseContext *ctx, const BunHeader *header);

bun_result_t validate_non_zero_checksum(u32 checksum);

bun_result_t validate_flags(u32 flags);

bun_result_t validate_asset_name(const BunHeader *header, BunParseContext *ctx, u32 name_offset, u32 name_length, char *asset_name);
