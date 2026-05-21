/*
 * ============================================================================
 * AI ASSISTANCE DECLARATION
 * ============================================================================
 * This file was developed with the assistance of Claude.
 * Specifically, AI was used to:
 * 1. LD_PRELOAD shim structure (Lab 7 – week 9, Dynamic shared libraries):
 *    Claude generated the shared library boilerplate using
 *    dlsym(RTLD_NEXT, ...) to intercept calloc and forward non-zero calls
 *    to the real implementation, following the LD_PRELOAD interception
 *    pattern covered in the lab.
 * 2. Constructor attribute: Claude suggested __attribute__((constructor))
 *    for one-time initialisation of the real calloc function pointer.
 * ============================================================================
 *
 * zero_calloc_shim.c — calloc(0,*) NULL-return LD_PRELOAD shim.
 *
 * Build:
 *   gcc -shared -fPIC zero_calloc_shim.c -o zero_calloc_shim.so -ldl
 *
 * Usage:
 *   LD_PRELOAD="$(pwd)/zero_calloc_shim.so" ./bun_parser_plain <file>
 *
 * Purpose:
 *   C11 §7.22.3¶1 permits calloc(0, n) to return either NULL or a unique
 *   non-NULL pointer.  glibc returns non-NULL; musl and several BSD libcs
 *   return NULL.  This shim enforces the NULL-returning behaviour so the
 *   parser's handling of zero-asset files can be tested on any platform
 *   (Finding F-02).
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>

static void *(*real_calloc)(size_t, size_t) = NULL;

__attribute__((constructor))
static void init(void) {
    real_calloc = dlsym(RTLD_NEXT, "calloc");
}

void *calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        /* Print to stderr so the test runner can confirm the shim intercepted
         * the call — verifies the test environment is correctly configured. */
        fprintf(stderr, "[calloc_shim] returning NULL for calloc(%zu,%zu)\n", nmemb, size);
        return NULL;
    }
    return real_calloc(nmemb, size);
}
