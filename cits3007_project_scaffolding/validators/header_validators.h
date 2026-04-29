#pragma once
#include "bun.h"

void validate_magic(const BunHeader *header);

void validate_offsets(const BunHeader *header);

void validate_version(const BunHeader *header);

void validate_asset_table_size(const BunHeader *header, u64 file_size, u64 asset_table_size);

void validate_string_table_size(const BunHeader *header, u64 file_size);

void validate_data_section_size(const BunHeader *header, u64 file_size);

void validate_no_overlap(const BunHeader *header, u64 asset_table_size);