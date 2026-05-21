/* Mimic a hypothetical platform where calloc(0, X) returns NULL (allowed by C11). */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
static void *(*real_calloc)(size_t, size_t) = NULL;
__attribute__((constructor)) static void init(void) {
    real_calloc = dlsym(RTLD_NEXT, "calloc");
}
void *calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        /* Document at stderr so we can prove the shim ran. */
        fprintf(stderr, "[calloc_shim] returning NULL for calloc(%zu,%zu)\n", nmemb, size);
        return NULL;
    }
    return real_calloc(nmemb, size);
}
