#ifndef BUN_H
#define BUN_H

// Definition for fseeko()
#define _FILE_OFFSET_BITS 64

#include <stdint.h>
#include <stdio.h>

//
// Result codes (per BUN spec section 2)
//

typedef enum {
    BUN_OK           = 0,
    BUN_MALFORMED    = 1,
    BUN_UNSUPPORTED  = 2,
    BUN_ERR_IO       = 3,
    BUN_ERR_OVERFLOW = 4,
    BUN_ERR_ALLOC    = 5,
    BUN_ERR_ARGS     = 6
} bun_result_t;


// Codes for what data (not including errors) can safely be printed
typedef enum {
    BUN_ALL = 0, // Assets can safely be printed
    BUN_HEADER_ONLY = 1, // Only the header can be safely printed
    BUN_NONE   = 2, // No data can be safely printed
} bun_print_state_t;

//
// Data types (per BUN spec section 2)
// All multi-byte integers are little-endian on disk.
//

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;



//
// On-disk structures (per BUN spec sections 4 and 5)
//

#define BUN_MAGIC         0x304E5542u   // "BUN0" in little-endian
#define BUN_VERSION_MAJOR 1
#define BUN_VERSION_MINOR 0

#define BUN_FLAG_ENCRYPTED  0x1u
#define BUN_FLAG_EXECUTABLE 0x2u
#define BUN_SUPPORTED_FLAGS (BUN_FLAG_ENCRYPTED | BUN_FLAG_EXECUTABLE)

typedef struct {
    u32 magic;
    u16 version_major;
    u16 version_minor;
    u32 asset_count;
    u64 asset_table_offset;
    u64 string_table_offset;
    u64 string_table_size;
    u64 data_section_offset;
    u64 data_section_size;
    u64 reserved;
} BunHeader;

typedef struct {
    u32 name_offset;
    u32 name_length;
    u64 data_offset;
    u64 data_size;
    u64 uncompressed_size;
    u32 compression;
    u32 type;
    u32 checksum;
    u32 flags;
} BunAssetRecord;

//
// Expected on-disk sizes -- these can be used in assertions or static_asserts.
//

#define BUN_HEADER_SIZE       60
#define BUN_ASSET_RECORD_SIZE 48

// AI usage declaration: ChatGPT was used to suggest a reasonable maximum number of errors reported
#define BUN_MAX_ERRORS 64

// Maximum prefix length for asset names and data payloads
#define MAX_PREFIX_LENGTH 60

//
// Parse context
//
// A struct to store information about the state of your parser (rather than
// passing multiple arguments to every function).
//
// You will likely want to add fields to it as your implementation grows.
//

typedef struct {
    char message[128]; // error message
} BunError;

typedef struct {
    FILE   *file;           // open file handle
    long    file_size;      // total file size in bytes
    BunError errors[BUN_MAX_ERRORS]; // array to store error messages and codes
    long error_count;      // number of errors
    bun_result_t result;    // current result code
    bun_print_state_t printable; // what data can be safely printed
    int *assets_printable; // array to store whether each asset is printable (1) or not (0)
    // add further fields here as needed
    BunAssetRecord *assets; // stores every parsed asset record
    char **asset_names; // stores every parsed asset name
    u32 asset_count; // number of assets stored
} BunParseContext;

//
// Public API
//
// The function declarations below define the public API for your parser;
// you implement them in the `bun_parse.c` file.
//
// A note on I/O and output:
//   The functions below return result codes; the intention is that they
//   should not print to stdout or stderr themselves.
//   Keeping I/O out of these functions makes them much easier to test (your
//   tests can call them and inspect the return value without terminal output
//   getting cluttered with other content).
//   If you need to pass additional information in or out, `ctx` is a good place
//   to put it.
//
//   So printing (human-readable output for valid files and error messages
//   for invalid ones) should happen in main.c, based on the result code and
//   the content of `ctx`.
//
//   (This is a suggestion, not a requirement. But mixing output deeply into
//   parsing logic tends to make both harder to maintain.)

/**
 * Open a BUN file and populate ctx. Returns BUN_ERR_IO if the file cannot
 * be opened or its size determined.
 */
bun_result_t bun_open(const char *path, BunParseContext *ctx);

/**
 * Parse and validate the BUN header from ctx->file, populating *header.
 * Returns BUN_OK, BUN_MALFORMED, or BUN_UNSUPPORTED.
 */
bun_result_t bun_parse_header(BunParseContext *ctx, BunHeader *header);

/**
 * Parse and validate header from ctx->file.
 * Should only be called after bun_open() returns BUN_OK.
 * Returns BUN_OK on success, otherwise a valid bun_result_t error code.
 *
 * Populates header with the parsed header data from the input BUN file.
 * Adds error messages for up to 64 errors found to ctx->errors, and sets ctx->error_count to the total number of errors.
 * Updates ctx->printable to indicate what data can be safely printed based on the bun_print_state_t.
 */
bun_result_t bun_parse_assets(BunParseContext *ctx, const BunHeader *header);

/**
 * Parse and validate asset table from ctx->file, using information from header.
 * Should only be called after bun_parse_header returns BUN_OK, leaving ctx->result as BUN_OK, otherwise, behaviour is undefined.
 * Returns BUN_OK on success, otherwise a valid bun_result_t error code.
 * 
 * Populates ctx->assets with the parsed asset records, and ctx->assets_printable with whether each asset can be safely printed.
 * Adds error messages for up to 64 errors found to ctx->errors, and sets ctx->error_count to the total number of errors.
 */
bun_result_t bun_close(BunParseContext *ctx);
/**
 * Close the file handle in ctx and frees memory allocated by the parser. Must only be called on a BunParseContext
 * holding an open FILE*. Returns BUN_OK on success, BUN_ERR_IO on error.
 */

int is_printable_ascii(u8 byte);

#endif // BUN_H
