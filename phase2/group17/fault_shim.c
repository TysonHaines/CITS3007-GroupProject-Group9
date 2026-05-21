/* fseeko fault-injection shim.
 *
 * Set FAULT_NTH=K to make the K-th fseeko call fail with EIO.
 * All other fseeko calls pass through to the real implementation.
 *
 * Purpose: drive bun_parse_assets() down an early-return path that
 * I claim leaks str_buf, then let LeakSanitizer prove it.
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
