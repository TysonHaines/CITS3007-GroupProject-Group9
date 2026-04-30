#pragma once
#include "../bun.h"

void validate_magic(BunParseContext *ctx, const BunHeader *header);
void validate_offsets(BunParseContext *ctx, const BunHeader *header);
void validate_version(BunParseContext *ctx, const BunHeader *header);
void validate_asset_table_size(BunParseContext *ctx, const BunHeader *header,
                                u64 file_size, u64 asset_table_size);
void validate_string_table_size(BunParseContext *ctx, const BunHeader *header,
                                 u64 file_size);
void validate_data_section_size(BunParseContext *ctx, const BunHeader *header,
                                 u64 file_size);
void validate_no_overlap(BunParseContext *ctx, const BunHeader *header,
                          u64 asset_table_size);
