/* Simulate a perfectly legitimate caller pattern: open something, on failure
 * clean up. bun_close()'s only documented precondition is "an open FILE*",
 * but library users routinely zero-initialize the ctx and call cleanup
 * unconditionally. With -DNDEBUG the assert vanishes and we hit fclose(NULL). */
#include "bun.h"
#include <stdio.h>
int main(void) {
    BunParseContext ctx = {0};            // ctx.file == NULL
    bun_result_t r = bun_close(&ctx);     // assert(ctx->file) — gone under NDEBUG
    printf("bun_close returned %d (we should not reach here under NDEBUG)\n", r);
    return 0;
}
