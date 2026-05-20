#ifndef BUN_H
#define BUN_H

#include <stdint.h>
#include <stdio.h>

//
// Result codes
//

typedef enum {
    BUN_OK            = 0,
    BUN_MALFORMED     = 1,  // file violates the spec
    BUN_UNSUPPORTED   = 2,  // file uses features this parser doesn't implement
    BUN_ERR_IO        = 3,  // I/O error during read/seek/close
    BUN_ERR_NOT_FOUND = 4,  // file does not exist
    BUN_ERR_USAGE     = 5,  // wrong number of command-line arguments
    BUN_ERR_NOMEM     = 6,  // memory allocation failed
} bun_result_t;

//
// Fixed-width integer aliases (per spec section 2; little-endian on disk)
//

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

//
// Spec constants
//

#define BUN_MAGIC         0x304E5542u   // "BUN0" little-endian
#define BUN_VERSION_MAJOR 1
#define BUN_VERSION_MINOR 0

#define BUN_HEADER_SIZE       60
#define BUN_ASSET_RECORD_SIZE 48

#define BUN_FLAG_ENCRYPTED  0x1u
#define BUN_FLAG_EXECUTABLE 0x2u

#define BUN_COMPRESS_NONE 0
#define BUN_COMPRESS_RLE  1
#define BUN_COMPRESS_ZLIB 2

//
// On-disk header (per spec section 4). Parsed field-by-field from a byte
// buffer rather than by casting, to avoid struct-padding undefined behaviour.
//

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

//
// Violation tracking
//
// Validators append to a list on the parse context instead of printing
// directly. main consumes the list and emits one line per violation to
// stderr. The worst severity across the list determines the exit code.
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
    FILE *file;
    long  file_size;
    BunViolation *violations_head;
    BunViolation *violations_tail;
} BunParseContext;

//
// Public API (defined in bun_parse.c)
//

/** Open `path` for reading and populate ctx (file handle + size). */
bun_result_t bun_open(const char *path, BunParseContext *ctx);

/** Read the BUN header into *header and validate it.
 *  Violations are appended to ctx; result is the worst severity seen. */
bun_result_t bun_parse_header(BunParseContext *ctx, BunHeader *header);

/** Read and validate every asset record per header.asset_count.
 *  Violations are appended to ctx; result is the worst severity seen. */
bun_result_t bun_parse_assets(BunParseContext *ctx, const BunHeader *header);

/** Close the file and free the violation list owned by ctx. */
bun_result_t bun_close(BunParseContext *ctx);

//
// Violation list helpers (defined in bun_violations.c)
//

/** Append a printf-formatted violation to ctx's list. */
bun_result_t bun_add_violation(BunParseContext *ctx, bun_result_t severity,
                                const char *fmt, ...);

/** Return the worst severity across all violations (BUN_MALFORMED beats
 *  BUN_UNSUPPORTED), or BUN_OK if the list is empty. */
bun_result_t bun_worst_violation(const BunParseContext *ctx);

/** Print every violation to stderr, one per line. */
void bun_print_violations(const BunParseContext *ctx);

/** Free the violation list. Called by bun_close. */
void bun_free_violations(BunParseContext *ctx);

//
// Output helpers (defined in bun_print.c)
//

/** Print the BUN header to stdout in human-readable form. */
void bun_print_header(const BunHeader *header);

/** Print every asset record (with name + payload preview) to stdout. */
void bun_print_assets(BunParseContext *ctx, const BunHeader *header);

#endif // BUN_H