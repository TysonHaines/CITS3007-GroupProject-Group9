#ifndef BUN_H
#define BUN_H

#include <stdint.h>
#include <stdio.h>

//
// Result codes (per BUN spec section 2)
//

typedef enum {
    BUN_OK            = 0,
    BUN_MALFORMED     = 1,
    BUN_UNSUPPORTED   = 2,
    BUN_ERR_IO        = 3,  // I/O error during read/seek/close
    BUN_ERR_NOT_FOUND = 4,  // file does not exist or cannot be opened
    BUN_ERR_USAGE     = 5,  // incorrect command-line arguments
    BUN_ERR_NOMEM     = 6,  // memory allocation failed
} bun_result_t;

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

#define BUN_COMPRESS_NONE 0
#define BUN_COMPRESS_RLE  1
#define BUN_COMPRESS_ZLIB 2

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

#define BUN_HEADER_SIZE       60
#define BUN_ASSET_RECORD_SIZE 48

//
// Violation tracking
//
// Validators record format violations into a list on the parse context
// rather than printing directly. main can then iterate the list and emit
// one line per violation to stderr. severity is BUN_MALFORMED or
// BUN_UNSUPPORTED -- the worst severity across the list determines the
// final result code.
//

typedef struct BunViolation {
    bun_result_t severity;     // BUN_MALFORMED or BUN_UNSUPPORTED
    char *message;             // heap-allocated, owned by the violation
    struct BunViolation *next;
} BunViolation;

//
// Parse context
//

typedef struct {
    FILE   *file;
    long    file_size;

    // Violation list -- populated by validators, consumed by main.
    BunViolation *violations_head;
    BunViolation *violations_tail;
} BunParseContext;

//
// Public API
//

bun_result_t bun_open(const char *path, BunParseContext *ctx);
bun_result_t bun_parse_header(BunParseContext *ctx, BunHeader *header);
bun_result_t bun_parse_assets(BunParseContext *ctx, const BunHeader *header);
bun_result_t bun_close(BunParseContext *ctx);

//
// Violation list helpers (defined in bun_violations.c)
//

bun_result_t bun_add_violation(BunParseContext *ctx, bun_result_t severity,
                                const char *fmt, ...);
bun_result_t bun_worst_violation(const BunParseContext *ctx);
void bun_print_violations(const BunParseContext *ctx);
void bun_free_violations(BunParseContext *ctx);

//
// Output helpers (defined in bun_print.c)
//

void bun_print_header(const BunHeader *header);
void bun_print_assets(BunParseContext *ctx, const BunHeader *header);

#endif // BUN_H