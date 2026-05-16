#pragma once
#include "bun.h"

/** Verify name_length is non-zero. */
void validate_name_length(BunParseContext *ctx, u32 idx, u32 name_length);

/** Verify (name_offset + name_length) fits within the string table. */
void name_fits_string_table(BunParseContext *ctx, u32 idx,
                             u32 name_offset, u64 string_table_size,
                             u32 name_length);

/** Verify (data_offset + data_size) fits within the data section. */
void data_fits_data_section(BunParseContext *ctx, u32 idx,
                             u64 data_offset, u64 data_size,
                             const BunHeader *header);

/** Verify compression value and any compression-specific invariants
 *  (uncompressed_size constraints, RLE pair structure, zlib unsupported). */
void validate_compression(BunParseContext *ctx, u32 idx,
                           u32 compression, u64 uncompressed_size,
                           u64 data_size, u64 data_offset,
                           const BunHeader *header);

/** CRC-32 validation is not supported; reject any non-zero checksum. */
void validate_non_zero_checksum(BunParseContext *ctx, u32 idx, u32 checksum);

/** Reject any flag bits outside ENCRYPTED|EXECUTABLE. */
void validate_flags(BunParseContext *ctx, u32 idx, u32 flags);

/** Verify every byte of the asset name is printable ASCII (0x20-0x7E).
 *  Reads in chunks via a fixed-size stack buffer for sub-linear memory use. */
void validate_asset_name(BunParseContext *ctx, u32 idx,
                          const BunHeader *header,
                          u32 name_offset, u32 name_length);