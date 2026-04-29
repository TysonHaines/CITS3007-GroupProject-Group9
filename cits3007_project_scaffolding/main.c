#include <stdio.h>
#include <stdlib.h>

#include "bun.h"

int main(int argc, char *argv[]) {
  // Confirm correct number of command-line arguments.
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <file.bun>\n", argv[0]);
    return BUN_ERR_USAGE;
  }

  const char *path = argv[1];

  BunParseContext ctx = {0};
  BunHeader header  = {0};

  // Open the file and populate ctx.
  bun_result_t result = bun_open(path, &ctx);
  if (result != BUN_OK) {
    if (result == BUN_ERR_NOT_FOUND) {
      fprintf(stderr, "Error: file '%s' not found\n", path);
    } else {
      fprintf(stderr, "Error: I/O error opening '%s'\n", path);
    }
    return result;
  }

  // Parse header. Even on failure, header struct is populated and worth showing.
  result = bun_parse_header(&ctx, &header);

  // Show whatever the parser was able to read on stdout, per brief 5.2.e.
  bun_print_header(&header);

  // Only attempt asset records if the header was good enough to trust offsets.
  if (result == BUN_OK) {
    bun_result_t assets_result = bun_parse_assets(&ctx, &header);
    bun_print_assets(&ctx, &header);
    if (assets_result != BUN_OK) {
      result = assets_result;
    }
  }

  // Print all collected violations to stderr, one per line, per brief 5.2.d.
  bun_print_violations(&ctx);

  // Final summary line on success; runtime errors get a stderr note.
  if (result == BUN_OK) {
    printf("Parse complete: %u asset(s), no violations found.\n",
           header.asset_count);
  } else if (result == BUN_ERR_IO) {
    fprintf(stderr, "Parse failed: I/O error during parse (code %d)\n", result);
  }
  // BUN_MALFORMED and BUN_UNSUPPORTED don't need a summary line --
  // bun_print_violations already enumerated them.

  bun_close(&ctx);
  return result;
}