#include <stdio.h>
#include <stdlib.h>

#include "bun.h"

int main(int argc, char *argv[]) {
  // Expect exactly one argument: the path to a .bun file.
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <file.bun>\n", argv[0]);
    return BUN_ERR_USAGE;
  }

  const char *path = argv[1];

  BunParseContext ctx = {0};
  BunHeader header  = {0};

  // Open the file; distinguish missing file from other I/O errors.
  bun_result_t result = bun_open(path, &ctx);
  if (result != BUN_OK) {
    if (result == BUN_ERR_NOT_FOUND) {
      fprintf(stderr, "Error: file '%s' not found\n", path);
    } else {
      fprintf(stderr, "Error: I/O error opening '%s'\n", path);
    }
    return result;
  }

  // Parse header. Even on failure, the header struct is populated and worth showing.
  result = bun_parse_header(&ctx, &header);
  bun_print_header(&header);

  // Only attempt to read asset records if the asset table physically fits in the file.
  u64 asset_table_size = (u64)header.asset_count * BUN_ASSET_RECORD_SIZE;
  int asset_table_readable =
    header.asset_table_offset < (u64)ctx.file_size &&
    asset_table_size <= (u64)ctx.file_size - header.asset_table_offset;

  if (asset_table_readable) {
    bun_result_t assets_result = bun_parse_assets(&ctx, &header);
    bun_print_assets(&ctx, &header);

    // Merge assets_result into result; BUN_MALFORMED beats BUN_UNSUPPORTED.
    if (assets_result != BUN_OK && result == BUN_OK) {
      result = assets_result;
    } else if (assets_result == BUN_MALFORMED && result == BUN_UNSUPPORTED) {
      result = BUN_MALFORMED;
    }
  }

  // Emit collected violations to stderr, one per line (brief 5.2.d).
  bun_print_violations(&ctx);

  // Final summary: success line on stdout, runtime errors on stderr.
  // Malformed/unsupported are already enumerated by bun_print_violations.
  if (result == BUN_OK) {
    printf("Parse complete: %u asset(s), no violations found.\n",
           header.asset_count);
  } else if (result == BUN_ERR_IO) {
    fprintf(stderr, "Parse failed: I/O error during parse (code %d)\n", result);
  }

  // Close failure only changes the exit code if parsing otherwise succeeded.
  bun_result_t close_result = bun_close(&ctx);
  if (result == BUN_OK && close_result != BUN_OK) {
    fprintf(stderr, "Error: failed to close file (code %d)\n", close_result);
    result = close_result;
  }
  return result;
}