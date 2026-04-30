#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "bun.h"

bun_result_t bun_add_violation(BunParseContext *ctx, bun_result_t severity,
                                const char *fmt, ...) {
  // Two-pass formatting: measure required length, then allocate and write.
  va_list args, args_copy;
  va_start(args, fmt);
  va_copy(args_copy, args);

  int needed = vsnprintf(NULL, 0, fmt, args);
  va_end(args);

  if (needed < 0) {
    va_end(args_copy);
    return BUN_ERR_IO;
  }

  char *msg = malloc((size_t)needed + 1);
  if (!msg) {
    va_end(args_copy);
    return BUN_ERR_NOMEM;
  }

  vsnprintf(msg, (size_t)needed + 1, fmt, args_copy);
  va_end(args_copy);

  BunViolation *v = malloc(sizeof(BunViolation));
  if (!v) {
    free(msg);
    return BUN_ERR_NOMEM;
  }
  v->severity = severity;
  v->message  = msg;
  v->next     = NULL;

  // Append to tail so violations stay in insertion order.
  if (ctx->violations_tail) {
    ctx->violations_tail->next = v;
  } else {
    ctx->violations_head = v;
  }
  ctx->violations_tail = v;

  return BUN_OK;
}

bun_result_t bun_worst_violation(const BunParseContext *ctx) {
  bun_result_t worst = BUN_OK;
  for (const BunViolation *v = ctx->violations_head; v; v = v->next) {
    // BUN_MALFORMED beats BUN_UNSUPPORTED.
    if (v->severity == BUN_MALFORMED) return BUN_MALFORMED;
    if (v->severity == BUN_UNSUPPORTED) worst = BUN_UNSUPPORTED;
  }
  return worst;
}

void bun_print_violations(const BunParseContext *ctx) {
  for (const BunViolation *v = ctx->violations_head; v; v = v->next) {
    const char *kind = (v->severity == BUN_UNSUPPORTED) ? "unsupported" : "violation";
    fprintf(stderr, "%s: %s\n", kind, v->message);
  }
}

void bun_free_violations(BunParseContext *ctx) {
  BunViolation *v = ctx->violations_head;
  while (v) {
    BunViolation *next = v->next;
    free(v->message);
    free(v);
    v = next;
  }
  ctx->violations_head = NULL;
  ctx->violations_tail = NULL;
}