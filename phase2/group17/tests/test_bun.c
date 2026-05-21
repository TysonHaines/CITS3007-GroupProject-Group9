/*
 * ============================================================================
 * AI ASSISTANCE DECLARATION
 * ============================================================================
 * Portions of this file were developed with the assistance of an AI tool (Gemini).
 * Specifically, AI was used as a QA testing mentor to guide the creation of 
 * unit tests (such as test_asset_overlap) and to debug the libcheck test suite 
 * environment and missing file fixtures.
 * ============================================================================
 */

#include "../bun.h"

#include <check.h>

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

// Helper: terminate abnormally, after printing a message to stderr
void die(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  fprintf(stderr, "fatal error: ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");

  va_end(args);

  abort();
}


// Helper: open a test fixture by name, relative to the tests/ directory.
static const char *fixture(const char *filename) {
    // For simplicity, tests assume they are run from the project root, and
    // test BUN files live in tests/fixtures/{valid,invalid}. Adjust if needed.
    static char path[256];
    int res = snprintf(path, sizeof(path), "tests/fixtures/%s", filename);
    if (res < 0) {
      die("snprintf failed: %s", strerror(errno));
    }
    if ((size_t) res > sizeof(path)) {
      die("filename '%s' too big for buffer (would write %d bytes to %zu-size buffer)",
          filename, res, sizeof(path));
    }
    return path;
}

// Example test suite: header parsing

START_TEST(test_valid_minimal) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("valid/01-empty.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);
    ck_assert_uint_eq(header.magic, BUN_MAGIC);
    ck_assert_uint_eq(header.version_major, 1);
    ck_assert_uint_eq(header.version_minor, 0);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_bad_magic) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/01-bad-magic.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_unsupported_version) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/02-bad-version.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_UNSUPPORTED);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_asset_overlap) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    // We are testing if the overlap check catches a bad file
    bun_result_t r = bun_open(fixture("invalid/05-overlapping-sections.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    if (r == BUN_OK) {
        r = bun_parse_assets(&ctx, &header);
    }
    
    // We expect the program to catch the error and return 1 (BUN_MALFORMED)
    ck_assert_int_eq(r, BUN_MALFORMED);
    bun_close(&ctx);
}
END_TEST

START_TEST(test_unsupported_compression) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    // The project spec says we only support compression types 0 and 1.
    // Let's feed it a file that claims to have an unsupported compression type.
    bun_result_t r = bun_open(fixture("invalid/16-rle-truncated.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    if (r == BUN_OK) {
        r = bun_parse_assets(&ctx, &header);
    }
    
    // It should throw BUN_MALFORMED or BUN_UNSUPPORTED
    ck_assert_int_eq(r, BUN_MALFORMED);
    bun_close(&ctx);
}
END_TEST

START_TEST(test_unprintable_name) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    // We are testing if the parser correctly catches hidden/unprintable
    // characters inside the asset name string table.
    bun_result_t r = bun_open(fixture("invalid/07-asset-name-nonprintable.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    if (r == BUN_OK) {
        r = bun_parse_assets(&ctx, &header);
    }
    
    ck_assert_int_eq(r, BUN_MALFORMED);
    bun_close(&ctx);
}
END_TEST

// Assemble a test suite from our tests

static Suite *bun_suite(void) {
    Suite *s = suite_create("bun-suite");

    // Note that "TCase" is more like a sub-suite than a single test case
    TCase *tc_header = tcase_create("header-tests");
    tcase_add_test(tc_header, test_valid_minimal);
    tcase_add_test(tc_header, test_bad_magic);
    tcase_add_test(tc_header, test_unsupported_version);
    suite_add_tcase(s, tc_header);

    // Asset tests
    TCase *tc_assets = tcase_create("asset-tests");
    tcase_add_test(tc_assets, test_asset_overlap);
    tcase_add_test(tc_assets, test_unprintable_name);
    suite_add_tcase(s, tc_assets);

    // Compression tests
    TCase *tc_comp = tcase_create("compression-tests");
    tcase_add_test(tc_comp, test_unsupported_compression);
    suite_add_tcase(s, tc_comp);

    return s;
}

int main(void) {
    Suite   *s  = bun_suite();
    SRunner *sr = srunner_create(s);

    // see https://libcheck.github.io/check/doc/check_html/check_3.html#SRunner-Output for different output options.
    // e.g. pass CK_VERBOSE if you want to see successes as well as failures.
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

