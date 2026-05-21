/* LD_PRELOAD shim: make calloc(0, *) and calloc(*, 0) return NULL,
 * exercising the implementation-defined behaviour permitted by C11 7.22.3.
 * Build: gcc -shared -fPIC zero_calloc_shim.c -o zero_calloc_shim.so -ldl
 * Use:   LD_PRELOAD=./zero_calloc_shim.so ./bun_parser file.bun
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
void *calloc(size_t n, size_t s) {
    if (n == 0 || s == 0) {
        fprintf(stderr, "[calloc_shim] returning NULL for calloc(%zu,%zu)\n", n, s);
        return NULL;
    }
    static void *(*real)(size_t, size_t);
    if (!real) real = dlsym(RTLD_NEXT, "calloc");
    return real(n, s);
}
