/*
 * ============================================================================
 * AI ASSISTANCE DECLARATION
 * ============================================================================
 * Portions of this file were developed with the assistance of an AI tool (Claude).
 * Specifically, AI was used to help design structure,
 * including argument validation,fixing code and understanding of correct usage of overall flow.
 * 
 * ============================================================================
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>

#include "bun.h"

static void print_asset_name(FILE *file, const BunHeader *header, const BunAssetRecord *record) {
    // Labelling output
    printf("  Asset Name: ");
    // Get the asset name from the file
    u64 asset_name_position = header->string_table_offset + record->name_offset;  // find the location of the asset name in the file
    if (fseeko(file, (off_t)asset_name_position, SEEK_SET) != 0) { 
        printf(" <could not seek to asset name>\n");
        return;
    }
    // Asset name has already been checked to be all ASCII characters
    // Print asset name depending on length
    if (record->name_length > MAX_PREFIX_LENGTH) {
        // Truncate printing to MAX_PREFIX_LENGTH number of characters
        for (u32 i = 0; i < MAX_PREFIX_LENGTH; i++) {
            int byte = fgetc(file);
            if (byte == EOF) {
                printf("<could not read byte>");
                break;
            }
            printf("%c", byte);
        }
        printf(" <truncated to %"PRIu32" characters; %"PRIu32" more character(s) hidden>", (u32)MAX_PREFIX_LENGTH, record->name_length - (u32)MAX_PREFIX_LENGTH);
    } else {
        // Print full asset name (under MAX_PREFIX_LENGTH chars)
        for (u32 i = 0; i < record->name_length; i++) {
            int byte = fgetc(file);
            if (byte == EOF) {
                printf("<could not read byte>");
                break;
            }
            printf("%c", byte);
        }
    }
    printf("\n");
}

static int is_payload_printable_ascii(FILE *file, const BunHeader *header, const BunAssetRecord *record) {
    u64 payload_position = header->data_section_offset + record->data_offset; //finding the real location of asset data inside the file
    int payload_printable_ascii = 1;  // true by default; updates to false if proven otherwise
    
    if (fseeko(file, (off_t)payload_position, SEEK_SET) != 0) { 
        printf("  <could not seek to payload for ASCII printable check>\n");
        payload_printable_ascii = 0;  // false value still allows hexdump to be printed as long as later fseek's succeed
        return payload_printable_ascii;
    }
    
    if (record->compression == 0) {
        // Not compressed
        for (u64 i = 0; i < record->data_size; i++) {
            int byte = fgetc(file);
            if (byte == EOF) {
                printf("  <could not read byte for ASCII printable check>\n");
                payload_printable_ascii = 0;  // false value still allows hexdump to be printed as long as later fgetc's succeed
                break;
            }
            
            if (!is_printable_ascii((u8)byte)) {
                payload_printable_ascii = 0;  // entire payload not printable in ASCII
                break;
            }
        }
    } else if (record->compression == 1) {
        // RLE Compressed
        for (u64 i = 1; i < record->data_size; i+=2) {
            int byte = fgetc(file);  // skip count
            byte = fgetc(file);  // get value
            if (byte == EOF) {
                printf("  <could not read byte for ASCII printable check>\n");
                payload_printable_ascii = 0;  // false value still allows hexdump to be printed as long as later fgetc's succeed
                break;
            }
            
            if (!is_printable_ascii((u8)byte)) {
                payload_printable_ascii = 0;  // entire payload not printable in ASCII
                break;
            }
        }
    }

    return payload_printable_ascii;
}

static void print_payload(FILE *file, const BunHeader *header, const BunAssetRecord *record, const int payload_printable_ascii) {
    // Set file position indicator to the start of the payload
    u64 payload_position = header->data_section_offset + record->data_offset; //finding the real location of asset data inside the file
    if (fseeko(file, (off_t)payload_position, SEEK_SET) != 0) { 
        printf("  Payload: <could not seek to payload>\n");
        return;
    }
    // Labelling output
    printf("  Payload:");
    if (payload_printable_ascii) {
        printf(" ");
    }
    // Print payload depending on length
    if (record->data_size > MAX_PREFIX_LENGTH) {
        // Truncate printing to MAX_PREFIX_LENGTH number of chars/bytes
        for (u64 i = 0; i < MAX_PREFIX_LENGTH; i++) {
            int byte = fgetc(file);
            if (byte == EOF) {
                printf(" <could not read byte>");
                break;
            }
            // Output as either ASCII or bytes
            if (payload_printable_ascii) {
                printf("%c", byte);
            } else {
                printf(" %02X", (unsigned char)byte);
            }
        }
        if (payload_printable_ascii) {
            printf(" <truncated to %"PRIu32" characters; %"PRIu64" more character(s) hidden>", (u32)MAX_PREFIX_LENGTH, record->data_size - (u64)MAX_PREFIX_LENGTH);
        } else {
            printf(" <truncated to %"PRIu32" bytes; %"PRIu64" more byte(s) hidden>", (u32)MAX_PREFIX_LENGTH, record->data_size - (u64)MAX_PREFIX_LENGTH);
        }
    } else {
        // Print full asset payload (under MAX_PREFIX_LENGTH chars/bytes)
        for (u64 i = 0; i < record->data_size; i++) {
            int byte = fgetc(file);
            if (byte == EOF) {
                printf(" <could not read byte>");
                break;
            }
            // Output as either ASCII or bytes
            if (payload_printable_ascii) {
                printf("%c", byte);
            } else {
                printf(" %02X", (unsigned char)byte);
            }
        }
    }
    printf("\n");
}

static void print_rle_payload(FILE *file, const BunHeader *header, const BunAssetRecord *record, const int payload_printable_ascii) {
    // Set file position indicator to the start of the payload
    u64 payload_position = header->data_section_offset + record->data_offset;
    if (fseeko(file, (off_t)payload_position, SEEK_SET) != 0) {
        printf("  Decompressed Payload: <could not seek to payload>\n");
        return;
    }
    // Labelling output
    printf("  Decompressed Payload:");
    if (payload_printable_ascii) {
        printf(" ");
    }
    // Loop over every other byte in payload
    u64 total_bytes_printed = 0;  // keep track for truncation purposes
    int prefix_limit_reached = 0;  // to allow for breaking out of nested loops
    for (u64 i = 0; i < record->data_size; i += 2) {
        int count = fgetc(file);  // count = how many times to repeat (first byte in pair)
        int value = fgetc(file);  // value = what value to repeat (second byte in pair)
        if (count == EOF || value == EOF) {
            printf(" <could not read RLE pair>");
            break;
        }
        // Output value count number of times
        for (int j = 0; j < count; j++) {
            // Check if prefix limit has been reached
            if (total_bytes_printed >= MAX_PREFIX_LENGTH) {
                prefix_limit_reached = 1;
                break;
            }
            // Output value as either ASCII or bytes
            if (payload_printable_ascii) {
                printf("%c", value);
            } else {
                printf(" %02X", (unsigned char)value);
            }
            total_bytes_printed++;  // keep track of how many chars/bytes have been output
        }
        // Truncate output now that MAX_PREFIX_LENGTH has been reached
        if (prefix_limit_reached) {
            if (payload_printable_ascii) {
                printf(" <truncated to %"PRIu32" characters; %"PRIu64" more character(s) hidden>", (u32)MAX_PREFIX_LENGTH, record->uncompressed_size - (u64)MAX_PREFIX_LENGTH);
            } else {
                printf(" <truncated to %"PRIu32" bytes; %"PRIu64" more byte(s) hidden>", (u32)MAX_PREFIX_LENGTH, record->uncompressed_size - (u64)MAX_PREFIX_LENGTH);
            }
            break;
        }
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    // 1. Validate Arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file.bun>\n", argv[0]);
        return BUN_ERR_ARGS; 
    }
    
    const char *path = argv[1];
    BunParseContext ctx = {0};
    BunHeader header = {0};

    // 2. Open the File
    bun_result_t result = bun_open(path, &ctx);
    if (result != BUN_OK) {
        fprintf(stderr, "Error: (3) Could not open '%s' or determine its size.\n", path);
        return result; 
    }

    // 3. Parse the Header
    bun_parse_header(&ctx, &header);

     // 4. Parse the Assets (Only if header was BUN_OK)
    if (ctx.result == BUN_OK) {
        result = bun_parse_assets(&ctx, &header);
    }

    // 5. Print header if printable state allows it
    //BUN_ALL(0) and BUN_HEADER_ONLY(1) allow header printing
    if (ctx.printable == BUN_ALL || ctx.printable == BUN_HEADER_ONLY) {
        printf("=== BUN Header ===\n");
        printf("Magic: 0x%08"PRIX32"\n", header.magic);
        printf("Version: %u.%u\n", header.version_major, header.version_minor);
        printf("Asset Count: %"PRIu32"\n", header.asset_count);
        printf("Asset Table Offset: %"PRIu64"\n", header.asset_table_offset);
        printf("String Table Offset: %"PRIu64"\n", header.string_table_offset);
        printf("String Table Size: %"PRIu64"\n", header.string_table_size);
        printf("Data Section Offset: %"PRIu64"\n", header.data_section_offset);
        printf("Data Section Size: %"PRIu64"\n", header.data_section_size);
    }

    // 6. Print assets only if printable state is BUN_ALL
    //BUN_ALL is the only state that allows asset printing
    int payload_printable_ascii;  // false (0) if entire payload cannot be ASCII printed
    if (ctx.printable == BUN_ALL) {
        printf("\n=== Assets (%"PRIu32") ===\n", header.asset_count);
        for (u32 i = 0; i < header.asset_count; i++) {
            if (ctx.assets_printable[i]) {
                BunAssetRecord record = ctx.assets[i];
                printf("Asset %"PRIu32":\n", i);
                print_asset_name(ctx.file, &header, &record);
                printf("  Name Offset: %"PRIu32"\n", record.name_offset);
                printf("  Name Length: %"PRIu32"\n", record.name_length);
                printf("  Data Offset: %"PRIu64"\n", record.data_offset);
                printf("  Data Size: %"PRIu64"\n", record.data_size);
                printf("  Uncompressed Size: %"PRIu64"\n", record.uncompressed_size);
                printf("  Compression: %"PRIu32"\n", record.compression);
                printf("  Type: %"PRIu32"\n", record.type);
                printf("  Checksum: %"PRIu32"\n", record.checksum);
                printf("  Flags: %"PRIu32"\n", record.flags);
                payload_printable_ascii = is_payload_printable_ascii(ctx.file, &header, &record);
                if (record.compression == 0) {
                    print_payload(ctx.file, &header, &record, payload_printable_ascii);
                } else if (record.compression == 1) {
                    print_payload(ctx.file, &header, &record, 0);  // guaranteed to not be ASCII printable due to RLE count representations
                    print_rle_payload(ctx.file, &header, &record, payload_printable_ascii);
                }
                printf("\n");
            }
        }
    }

    // 7. Print errors to stderr
    //error_count may exceed BUN_MAX_ERRORS (64) - only 64 errors can be stored
    //Print all stored errors, but indicate if there were more than 64
    if (ctx.error_count > 0) {
        long printed_errors = ctx.error_count < BUN_MAX_ERRORS ? ctx.error_count : BUN_MAX_ERRORS;
        fprintf(stderr, "\n=== Errors ===\n");
        for (long i = 0; i < printed_errors; i++) {
            fprintf(stderr, "Error %ld: %s\n", i + 1, ctx.errors[i].message);
        }
            
        //state if there were more errors that were not printed due to BUN_MAX_ERRORS limit
        if (ctx.error_count > BUN_MAX_ERRORS) {
            fprintf(stderr, "Note: %ld additional errors not shown (exceeds BUN_MAX_ERRORS)\n", ctx.error_count - BUN_MAX_ERRORS);
        }
    }

    // 8. Cleanup and Exit
    bun_close(&ctx);
    
    // Strict compliance: return the accumulated result code from the parsing process (0 = OK, 1 = MALFORMED, 2 = UNSUPPORTED)
    return ctx.result; 
}
