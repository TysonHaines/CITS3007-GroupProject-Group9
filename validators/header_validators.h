#pragma once
#include "bun.h"

/** Verify the magic field equals BUN_MAGIC. */
void validate_magic(BunParseContext *ctx, const BunHeader *header);

/** Verify all offsets and sizes are divisible by 4 (per spec §4.1). */
void validate_offsets(BunParseContext *ctx, const BunHeader *header);

/** Verify version_major.version_minor matches the supported version. */
void validate_version(BunParseContext *ctx, const BunHeader *header);

/** Verify the asset table fits inside the file. */
void validate_asset_table_size(BunParseContext *ctx, const BunHeader *header,
                                u64 file_size, u64 asset_table_size);

/** Verify the string table fits inside the file. */
void validate_string_table_size(BunParseContext *ctx, const BunHeader *header,
                                 u64 file_size);

/** Verify the data section fits inside the file. */
void validate_data_section_size(BunParseContext *ctx, const BunHeader *header,
                                 u64 file_size);

/** Verify the three sections do not overlap each other. */
void validate_no_overlap(BunParseContext *ctx, const BunHeader *header,
                          u64 asset_table_size);