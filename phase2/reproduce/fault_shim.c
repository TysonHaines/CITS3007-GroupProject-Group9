/*
 * ============================================================================
 * AI ASSISTANCE DECLARATION
 * ============================================================================
 * This file was developed with the assistance of Claude.
 * Specifically, AI was used to:
 * 1. LD_PRELOAD shim structure (Lab 7 – week 9, Dynamic shared libraries):
 *    Claude generated the shared library boilerplate using
 *    dlsym(RTLD_NEXT, ...) to intercept fseeko and forward all other calls
 *    to the real implementation, as covered in the lab's section on
 *    overriding libc functions via LD_PRELOAD.
 * 2. Constructor attribute: Claude suggested __attribute__((constructor))
 *    for one-time initialisation of the real function pointer and FAULT_NTH
 *    environment variable parsing.
 * ============================================================================
 *
 * fault_shim.c — fseeko fault-injection LD_PRELOAD shim.
 *
 * Build:
 *   gcc -shared -fPIC fault_shim.c -o fault_shim.so -ldl
 *
 * Usage:
 *   Set FAULT_NTH=K before running the target binary to make the K-th
 *   fseeko call return -1 with errno=EIO.  All other calls pass through
 *   to the real implementation.
 *
 * Purpose:
 *   Drives bun_parse_assets() down an early-return path mid-loop so that
 *   LeakSanitizer can confirm the str_buf heap allocation is not freed
 *   on that path (Finding F-01).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>
#include <sys/types.h>

static int  (*real_fseeko)(FILE *, off_t, int) = NULL;
static long counter = 0;
static long fault_at = -1;

__attribute__((constructor))
static void init(void) {
    real_fseeko = dlsym(RTLD_NEXT, "fseeko");
    const char *e = getenv("FAULT_NTH");
    if (e) fault_at = atol(e);
}

int fseeko(FILE *stream, off_t offset, int whence) {
    counter++;
    if (counter == fault_at) {
        fprintf(stderr, "[shim] failing fseeko call #%ld\n", counter);
        errno = EIO;
        return -1;
    }
    return real_fseeko(stream, offset, whence);
}
