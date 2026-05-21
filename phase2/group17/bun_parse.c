/*
 * ============================================================================
 * AI ASSISTANCE DECLARATION
 * ============================================================================
 * Portions of this file were developed with the assistance of an AI tool (Gemini).
 * Specifically, AI was used as a secure coding mentor to implement concepts 
 * taught in the CITS3007 labs:
 * * 1. POSIX Compliance & fstat (Labs 4 & 6): AI suggested using the 
 * _POSIX_C_SOURCE macro and replacing `ftell` with `fstat` inside `bun_open` 
 * to securely handle files >2GB and prevent 32-bit integer overflows.
 * 2. Struct Padding Prevention: AI generated the endian-safe bit-shifting 
 * helpers (read_u32_le, etc.) to safely deserialize binary data into the 
 * BunHeader struct without relying on direct memory mapping, bypassing 
 * compiler padding vulnerabilities.
 * 3. Bounds Checking: AI assisted in structuring the mathematical bounds 
 * and 4-byte alignment checks inside `bun_parse_header`.
 * 4. Memory Security & Casting: AI (Gemini) was used to fix a Variable 
 * Length Array (VLA) vulnerability by replacing it with safe heap allocation 
 * (malloc/free), and to resolve fseek sign conversion warnings by explicitly 
 * casting unsigned integers to signed longs.
 * ============================================================================
 */

//Enable POSIX features (must be defined before ANY includes)
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#include "bun.h"


// AI usage declaration: Claude was used to help with data types for error message snprintf
// Code autocomplete was also used to generate various error messages quickly
static void add_error(BunParseContext *ctx, bun_result_t result, const char *message, bun_print_state_t printable) {
    // Add error only if array is not full 
    if (ctx->error_count < BUN_MAX_ERRORS) {
        snprintf(ctx->errors[ctx->error_count].message, sizeof(ctx->errors[0].message), "%s", message);
    } 

    // Check for error_count overflow
    if (ctx->error_count <= LONG_MAX - 1) {
        ctx->error_count++;
    }

    // Treat lower error values as worse
    if (result < ctx->result || ctx->result == BUN_OK) {
        ctx->result = result;
    }

    // Treat higher printable codes as worse
    if (printable > ctx->printable) {
        ctx->printable = printable;
    }

    return;
}

// Safe, endian-independent deserialization helpers to prevent padding/alignment exploits.
static u16 read_u16_le(const u8 *buf, size_t offset) {
    return (u16)buf[offset] | ((u16)buf[offset + 1] << 8);
}

static u32 read_u32_le(const u8 *buf, size_t offset) {
    return (u32)buf[offset]
         | ((u32)buf[offset + 1] << 8)
         | ((u32)buf[offset + 2] << 16)
         | ((u32)buf[offset + 3] << 24);
}

static u64 read_u64_le(const u8 *buf, size_t offset) {
    return (u64)buf[offset]
         | ((u64)buf[offset + 1] << 8)
         | ((u64)buf[offset + 2] << 16)
         | ((u64)buf[offset + 3] << 24)
         | ((u64)buf[offset + 4] << 32)
         | ((u64)buf[offset + 5] << 40)
         | ((u64)buf[offset + 6] << 48)
         | ((u64)buf[offset + 7] << 56);
}

// Helper function that determines if a given byte can be printed as a valid ASCII character
int is_printable_ascii(u8 byte) {
    if (byte < 0x20 || byte > 0x7E) {
        return 0;  // not ASCII printable
    } else {
        return 1;  // ASCII printable
    }
}

bun_result_t bun_open(const char *path, BunParseContext *ctx) {
    ctx->file = fopen(path, "rb");
    if (!ctx->file) {
        add_error(ctx, BUN_ERR_IO, "(3) Failed to open file", BUN_NONE);
        return BUN_ERR_IO;
    }

    // CITS3007 Secure Coding: Use POSIX fstat to securely get the file size.
    // This prevents 32-bit integer overflows on files larger than 2GB.
    struct stat st;
    if (fstat(fileno(ctx->file), &st) != 0) {
        fclose(ctx->file);
        ctx->file = NULL;
        add_error(ctx, BUN_ERR_IO, "(3) Failed to get file size", BUN_NONE);
        return BUN_ERR_IO;
    }
    
    ctx->file_size = st.st_size;
    return BUN_OK;
}

bun_result_t bun_parse_header(BunParseContext *ctx, BunHeader *header) {
    u8 buf[BUN_HEADER_SIZE];
    int err = 0;

    // 1. Validate file is at least large enough to contain a header
    if (ctx->file_size < BUN_HEADER_SIZE) {
        char msg[128];
        snprintf(msg, sizeof(msg), "(1) File too small to contain valid 60 byte header: got %ld bytes", 
            ctx->file_size);
        add_error(ctx, BUN_MALFORMED, msg, BUN_NONE);
        return BUN_MALFORMED;
    }

    // 2. Read exactly 60 bytes safely
    if (fread(buf, 1, BUN_HEADER_SIZE, ctx->file) != BUN_HEADER_SIZE) {
        add_error(ctx, BUN_ERR_IO, "(3) Failed to read header", BUN_NONE);
        return BUN_ERR_IO;
    }

    // 3. Deserialize manually to bypass compiler struct padding vulnerabilities
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

    // 4. Strict Validation Checks (from BUN Spec)
    if (header->magic != BUN_MAGIC) {
        char msg[128];
        snprintf(msg, sizeof(msg), "(1) Invalid magic number: got 0x%08"PRIx32", expected 0x%08"PRIx32,
            header->magic, BUN_MAGIC);
        add_error(ctx, BUN_MALFORMED, msg, BUN_NONE);
        return BUN_MALFORMED;
    }


    if (header->version_major != BUN_VERSION_MAJOR) {
        char msg[128];
        snprintf(msg, sizeof(msg), "(2) Unsupported major version: got %u, expected %u",
            header->version_major, BUN_VERSION_MAJOR);
        add_error(ctx, BUN_UNSUPPORTED, msg, BUN_HEADER_ONLY);
        err = 1;
    }
    if (header->version_minor != BUN_VERSION_MINOR) {
        char msg[128];
        snprintf(msg, sizeof(msg), "(2) Unsupported minor version: got %u, expected %u",
            header->version_minor, BUN_VERSION_MINOR);
        add_error(ctx, BUN_UNSUPPORTED, msg, BUN_HEADER_ONLY);
        err = 1;
    }

    if (err) {
        // If version is unsupported, we cannot trust format
        return ctx->result;
    }

    // Ensure all offsets/sizes are divisble by 4
    if (header->asset_table_offset % 4 != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "(1) Asset table offset not divisible by 4: got %"PRIu64"",
            header->asset_table_offset);
        add_error(ctx, BUN_MALFORMED, msg, BUN_HEADER_ONLY);
        err = 1;
    }
    if (header->string_table_offset % 4 != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "(1) String table offset not divisible by 4: got %"PRIu64"",
            header->string_table_offset);
        add_error(ctx, BUN_MALFORMED, msg, BUN_HEADER_ONLY);
        err = 1;
    }
    if (header->string_table_size % 4 != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "(1) String table size not divisible by 4: got %"PRIu64"",
            header->string_table_size);
        add_error(ctx, BUN_MALFORMED, msg, BUN_HEADER_ONLY);
        err = 1;
    }
    if (header->data_section_offset % 4 != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "(1) Data section offset not divisible by 4: got %"PRIu64"",
            header->data_section_offset);
        add_error(ctx, BUN_MALFORMED, msg, BUN_HEADER_ONLY);
        err = 1;
    }
    if (header->data_section_size % 4 != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "(1) Data section size not divisible by 4: got %"PRIu64"",
            header->data_section_size);
        add_error(ctx, BUN_MALFORMED, msg, BUN_HEADER_ONLY);
        err = 1;
    }

    // AI usage declaration: Claude suggested that incorrect offset divisibility would imply that the offsets cannot be trused as correct
    if (err) {
        return BUN_MALFORMED;
    }

    //guard against asset_count overflow before calculating asset_table_size

    uint64_t asset_table_size = (uint64_t)header->asset_count * BUN_ASSET_RECORD_SIZE;

    // Ensure that sections do not overlap with header
    if (header->asset_table_offset < BUN_HEADER_SIZE) {
        add_error(ctx, BUN_MALFORMED, "(1) Asset table offset overlaps with 60 byte header", BUN_HEADER_ONLY);
    }
    if (header->string_table_offset < BUN_HEADER_SIZE) {
        add_error(ctx, BUN_MALFORMED, "(1) String table offset overlaps with 60 byte header", BUN_HEADER_ONLY);
    }
    if (header->data_section_offset < BUN_HEADER_SIZE) {
        add_error(ctx, BUN_MALFORMED, "(1) Data section offset overlaps with 60 byte header", BUN_HEADER_ONLY);
    }


    // Reject any offset + size overflow 
    if (header->asset_table_offset > UINT64_MAX - asset_table_size) {
        add_error(ctx, BUN_ERR_OVERFLOW, "(4) Asset table offset + size would overflow", BUN_HEADER_ONLY);
        err = 1;
    }
    if (header->string_table_offset > UINT64_MAX - header->string_table_size) {
        add_error(ctx, BUN_ERR_OVERFLOW, "(4) String table offset + size would overflow", BUN_HEADER_ONLY);
        err = 1;
    }
    if (header->data_section_offset > UINT64_MAX - header->data_section_size) {
        add_error(ctx, BUN_ERR_OVERFLOW, "(4) Data section offset + size would overflow", BUN_HEADER_ONLY);
        err = 1;
    }

    if (err) {
        return BUN_MALFORMED;
    }

    // Basic bounds checking: Do the sections point outside the actual physical file
    // or does a section claim to extend past the file size?
    if (header->asset_table_offset + asset_table_size > (u64)ctx->file_size) {
        char msg[128];
        snprintf(msg, sizeof(msg), "(1) Asset table claims to extend past end of file: offset %"PRIu64" + size %"PRIu64" > file size %ld",
            header->asset_table_offset, asset_table_size, ctx->file_size);
        add_error(ctx, BUN_MALFORMED, msg, BUN_HEADER_ONLY);
    }
    if (header->string_table_offset + header->string_table_size > (u64)ctx->file_size) {
        char msg[128];
        snprintf(msg, sizeof(msg), "(1) String table claims to extend past end of file: offset %"PRIu64" + size %"PRIu64" > file size %ld",
            header->string_table_offset, header->string_table_size, ctx->file_size);
        add_error(ctx, BUN_MALFORMED, msg, BUN_HEADER_ONLY);
    }
    if (header->data_section_offset + header->data_section_size > (u64)ctx->file_size) {
        char msg[128];
        snprintf(msg, sizeof(msg), "(1) Data section claims to extend past end of file: offset %"PRIu64" + size %"PRIu64" > file size %ld",
            header->data_section_offset, header->data_section_size, ctx->file_size);
        add_error(ctx, BUN_MALFORMED, msg, BUN_HEADER_ONLY);
    }

    // Ensure that sections do not overlap with each other
    if (header->asset_table_offset + asset_table_size > header->string_table_offset &&
        header->asset_table_offset < header->string_table_offset + header->string_table_size) {
        add_error(ctx, BUN_MALFORMED, "(1) Asset table overlaps with string table", BUN_HEADER_ONLY);
    }
    if (header->asset_table_offset + asset_table_size > header->data_section_offset &&
        header->asset_table_offset < header->data_section_offset + header->data_section_size) {
        add_error(ctx, BUN_MALFORMED, "(1) Asset table overlaps with data section", BUN_HEADER_ONLY);
    }
    if (header->string_table_offset + header->string_table_size > header->data_section_offset &&
        header->string_table_offset < header->data_section_offset + header->data_section_size) {
        add_error(ctx, BUN_MALFORMED, "(1) String table overlaps with data section", BUN_HEADER_ONLY);
    }

    return ctx->result;
}

bun_result_t bun_parse_assets(BunParseContext *ctx, const BunHeader *header) {
    // Jump to asset table
    if (fseeko(ctx->file, (off_t)header->asset_table_offset, SEEK_SET) != 0) {
        add_error(ctx, BUN_ERR_IO, "Failed to seek to asset table", BUN_HEADER_ONLY);
        return BUN_ERR_IO;
    }

    ctx->assets_printable = calloc(header->asset_count, sizeof(int));
    ctx->assets = calloc(header->asset_count, sizeof(BunAssetRecord));
    if (!ctx->assets_printable) {
        add_error(ctx, BUN_ERR_ALLOC, "(5) Failed to allocate memory for asset printability array", BUN_HEADER_ONLY);
        return BUN_ERR_ALLOC;
    }
    if (!ctx->assets){
        add_error(ctx, BUN_ERR_ALLOC, "(5) Failed to allocate memory for asset records", BUN_HEADER_ONLY);
        return BUN_ERR_ALLOC;
    }   

    u8 *str_buf = NULL;  // initialise str_buf to allow for memory reallocation within the loop

    for (u32 i = 0; i < header->asset_count; i++) {
        int err = 0;

        u8 buf[BUN_ASSET_RECORD_SIZE];
        if (fread(buf, BUN_ASSET_RECORD_SIZE, 1, ctx->file) != 1) {
            char msg[128];
            snprintf(msg, sizeof(msg), "(3) Failed to read asset record %u", i);
            add_error(ctx, BUN_ERR_IO, msg, BUN_ALL);
            return BUN_ERR_IO;
        }

        // AI Usage declaration: Claude was used to quickly generate this deserialisation
        BunAssetRecord record;
        record.name_offset       = read_u32_le(buf, 0);
        record.name_length       = read_u32_le(buf, 4);
        record.data_offset       = read_u64_le(buf, 8);
        record.data_size         = read_u64_le(buf, 16);
        record.uncompressed_size = read_u64_le(buf, 24);
        record.compression       = read_u32_le(buf, 32);
        record.type              = read_u32_le(buf, 36);
        record.checksum          = read_u32_le(buf, 40);
        record.flags             = read_u32_le(buf, 44);

        ctx->assets[i] = record;

        // This parser does not support checksums
        if (record.checksum != 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "(2) Asset record %u has a checksum, which is not supported by this parser", i);
            add_error(ctx, BUN_UNSUPPORTED, msg, BUN_ALL);
            err = 1;
        }

        // AI Usage declaration: ChatGPT was used to help generate and understand
        // the bitwise logic of validating flags
        // Inverts the valid bits and checks if there are any present in the flags using bitwise AND
        if (record.flags & ~BUN_SUPPORTED_FLAGS) {
            char msg[128];
            snprintf(msg, sizeof(msg), "(2) Asset record %u has unsupported flags", i);
            add_error(ctx, BUN_UNSUPPORTED, msg, BUN_ALL);
            err = 1;
        }

        // This parser only supports RLE compression
        if (record.compression >= 2) {
            char msg[128];
            snprintf(msg, sizeof(msg), "(2) Asset record %u has unsupported compression type %u. This parser supports only types 0 (uncompressed) and 1 (RLE)", 
                i, record.compression);
            add_error(ctx, BUN_UNSUPPORTED, msg, BUN_ALL);
            err = 1;
        }
        
        // If uncompressed, uncompressed_size must be 0
        if (record.compression == 0 && record.uncompressed_size != 0) {
           char msg[128];
            snprintf(msg, sizeof(msg), "(1) Asset record %u is uncompressed but has non-zero uncompressed size of %"PRIu64"", i, record.uncompressed_size);
            add_error(ctx, BUN_MALFORMED, msg, BUN_ALL);
            err = 1;
        }

        // If compressed, uncompressed_size must not be 0
        if (record.compression != 0 && record.uncompressed_size == 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "(1) Asset record %u is compressed but has zero uncompressed size", i);
            add_error(ctx, BUN_MALFORMED, msg, BUN_ALL);
            err = 1;
        }

        // With RLE compression, data_size must be even
        if (record.compression == 1 && record.data_size % 2 != 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "(1) Asset record %u has RLE compression but odd data size of %"PRIu64"", 
                i, record.data_size);
            add_error(ctx, BUN_MALFORMED, msg, BUN_ALL);
            err = 1;
        }

        if (record.name_length == 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "(1) Asset record %u has a zero length name", i);
            add_error(ctx, BUN_MALFORMED, msg, BUN_ALL);
            err = 1;
        }

        // Ensure that asset name is fully within bounds of string table
        if ((u64)record.name_offset + (u64)record.name_length > header->string_table_size) {
            char msg[128];
            snprintf(msg, sizeof(msg), "(1) Asset record %u has a name that claims to extend beyond string table", i);
            add_error(ctx, BUN_MALFORMED, msg, BUN_ALL);
            err = 1;
        }

        // Reject offset + size overflow
        if (record.data_offset > UINT64_MAX - record.data_size) {
            char msg[128];
            snprintf(msg, sizeof(msg), "(4) Asset record %u has data offset + size would cause overflow", i);
            add_error(ctx, BUN_ERR_OVERFLOW, msg, BUN_ALL);
            err = 1;
            continue; 
        }
        // Ensure that asset data is fully within bounds of data section
        if (record.data_offset + record.data_size > header->data_section_size) {
            char msg[128];
            snprintf(msg, sizeof(msg), "(1) Asset record %u has data that claims to extend beyond data section", i);
            add_error(ctx, BUN_MALFORMED, msg, BUN_ALL);
            err = 1;
        }

        if (err) {
            // Skip reading the string if it is already known to be erroneous
            continue;
        }

        // Check if the true uncompressed sizes are equivalent to the listed uncompressed size
        if (record.compression == 1) {
            u64 payload_position = header->data_section_offset + record.data_offset;
            if (fseeko(ctx->file, (off_t)payload_position, SEEK_SET) != 0) {
                char msg[128];
                snprintf(msg, sizeof(msg), "(3) Failed to seek to payload of asset record %u", i);
                add_error(ctx, BUN_ERR_IO, msg, BUN_ALL);
                return BUN_ERR_IO;
            }
            u64 true_uncompressed_size = 0;  // initialise before adding counts
            int zero_count_err = 0;  // for stopping uncompressed size errors when manually breaking out of the loop below
            for (u64 count_idx = 0; count_idx < record.data_size; count_idx += 2) {
                int count = fgetc(ctx->file);
                int value = fgetc(ctx->file);
                if (count == EOF || value == EOF) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "(3) Failed to seek to payload of asset record %u", i);
                    add_error(ctx, BUN_ERR_IO, msg, BUN_ALL);
                    return BUN_ERR_IO;
                }
                if (count == 0) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "(1) Asset record %u has a zero count on (count, byte) pair %"PRIu64" (zero-indexed)", i, count_idx);
                    add_error(ctx, BUN_MALFORMED, msg, BUN_ALL);
                    err = 1;
                    zero_count_err = 1;
                    break;
                }
                // AI usage declaration: Claude suggested a cast to unsigned char to prevent sign extension issues that could come from just casting to u64
                true_uncompressed_size += (u64)(unsigned char)count;
            }
            if (true_uncompressed_size != record.uncompressed_size && zero_count_err != 1) {
                char msg[128];
                snprintf(msg, sizeof(msg), "(1) Asset record %u claims to have uncompressed size %"PRIu64" but actually has %"PRIu64"", i, record.uncompressed_size, true_uncompressed_size);
                add_error(ctx, BUN_MALFORMED, msg, BUN_ALL);
                err = 1;
            }
        }

        // Jump to the string table entry
        if (fseeko(ctx->file, (off_t)(header->string_table_offset + record.name_offset), SEEK_SET) != 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "(3) Failed to seek to string table name of asset record %u", i);
            add_error(ctx, BUN_ERR_IO, msg, BUN_ALL);
            return BUN_ERR_IO;
        }

        str_buf = realloc(str_buf, record.name_length);  // (re)allocate memory as the value of record.name_length is variable
        if (!str_buf) {
            add_error(ctx, BUN_ERR_ALLOC, "(5) Failed to allocate memory for asset name", BUN_ALL);
            return BUN_ERR_ALLOC;
        }

        if (fread(str_buf, 1, record.name_length, ctx->file) != record.name_length) {
            char msg[128];
            snprintf(msg, sizeof(msg), "(3) Failed to read name of asset record %u", i);
            add_error(ctx, BUN_ERR_IO, msg, BUN_ALL);
            free(str_buf);
            return BUN_ERR_IO;
        }

        for (u32 j = 0; j < record.name_length; j++) {
            // AI usage declaration: Claude was used to identify condition for unprintable character
            // Detects if character is not printable ASCII
            if (!is_printable_ascii(str_buf[j])) {
                char msg[128];
                snprintf(msg, sizeof(msg), "(1) Asset record %u has a name with first unprintable character at position %u", i, j);
                add_error(ctx, BUN_MALFORMED, msg, BUN_ALL);
                err = 1;
                break;
            }
        }

        // With all checks succeeded, the asset record is marked printable
        if (!err) {
            ctx->assets_printable[i] = 1;
        }

        // AI usage declaration: Claude was used to understand fseek logic and how to calculate offset
        // Jumps back to previous position in asset table, which is the start of the next record
        if (fseeko(ctx->file, (off_t)(header->asset_table_offset + ((u64)(i + 1) * BUN_ASSET_RECORD_SIZE)), SEEK_SET) != 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "(3) Failed to seek to next asset record %u", i);
            add_error(ctx, BUN_ERR_IO, msg, BUN_ALL);
            free(str_buf);
            return BUN_ERR_IO;
        }
    }

    free(str_buf);

    return ctx->result;
}

bun_result_t bun_close(BunParseContext *ctx) {
    assert(ctx->file);
    
    // AI usage declaration: Claude was used to identify and understand the need to free the assets_printable array to prevent memory leaks
    //free asserts_printable array to avoid memory leak
    free(ctx->assets_printable);
    ctx->assets_printable = NULL;
    free(ctx->assets);
    ctx->assets = NULL;

    int res = fclose(ctx->file);
    if (res) {
        return BUN_ERR_IO;
    } else {
        ctx->file = NULL;
        return BUN_OK;
    }
}
