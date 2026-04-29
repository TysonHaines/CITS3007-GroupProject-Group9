#include <stdio.h>
#include <stdlib.h>

#include "bun.h"

int main(int argc, char *argv[]) {
  // Confirm correct number of command-line argumnents
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <file.bun>\n", argv[0]);
    return BUN_ERR_USAGE;
  }

  const char *path = argv[1];

  BunParseContext ctx = {0};
  BunHeader header  = {0};

  bun_result_t result = bun_open(path, &ctx);
  if (result != BUN_OK) {
      if (result == BUN_ERR_NOT_FOUND) {
          fprintf(stderr, "Error: file '%s' not found\n", path);
      } else {
          fprintf(stderr, "Error: I/O error opening '%s'\n", path);
      }
      return result;
  }

  result = bun_parse_header(&ctx, &header);
  if (result != BUN_OK) {
    // bun_parse_header returns a code; printing the specifics is up to
    // you -- you may want to extend the API to return error details
    fprintf(stderr, "Error: header invalid or unsupported (code %d)\n", result);
    bun_close(&ctx);
    return result;
  }


  result = bun_parse_assets(&ctx, &header);

  // TODO: on BUN_OK, print human-readable summary to stdout.
  //     on BUN_MALFORMED / BUN_UNSUPPORTED, print violation list to stderr.
  //     See project brief for output requirements.

 
  //_____
  // print final result summary
  bun_print_header(&header);
  if (result == BUN_OK) {
    printf("\nParse complete: %u asset(s), no violations found.\n",
      header.asset_count);
  } else if (result == BUN_MALFORMED) {
    fprintf(stderr, "\nParse failed: file is malformed (code %d)\n", result);
  } else if (result == BUN_UNSUPPORTED) {
    fprintf(stderr, "\nParse failed: file uses unsupported features (code %d)\n",
      result);
  }
  //_____

  bun_close(&ctx);
  return result;
}
