#include "../bun.h"

#include <check.h>

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

// Helper: terminate abnormally, after printing a message to stderr
//flawfinder: ignore
void die(const char *fmt, ...) __attribute__((format(printf, 1, 2)))
{
  va_list args;
  va_start(args, fmt);

  fprintf(stderr, "fatal error: ");
  // flawfinder: ignore
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");

  va_end(args);

  abort();
}


// Helper: open a test fixture by name, relative to the tests/ directory.
static const char *fixture(const char *filename) {
    // For simplicity, tests assume they are run from the project root, and
    // test BUN files live in tests/fixtures/{valid,invalid}. Adjust if needed.
    // flawfinder: ignore
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

// Header parsing tests

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

START_TEST(test_bad_version_major) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/16-bad-version-major.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_UNSUPPORTED);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_truncated_header) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/17-truncated-header.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_offsets_not_divisible_by_4) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/03-offsets-not-div4.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_section_out_of_bounds) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/04-section-out-of-bounds.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    // The file has name_offset beyond string table - checked in assets
    r = bun_parse_assets(&ctx, &header);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_sections_overlap) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/05-sections-overlap.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_empty_file) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/15-empty-file.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
}
END_TEST

// Asset validation tests

START_TEST(test_valid_single_asset) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("valid/02-single-asset.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_valid_multiple_assets) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("valid/03-multi-asset.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);
    ck_assert_uint_eq(header.asset_count, 2);

    // Full parsing test - should read all asset records
    r = bun_parse_assets(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_valid_encrypted) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("valid/05-encrypted.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_valid_executable) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("valid/06-executable.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_valid_two_simple) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("valid/07-two-simple.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);
    ck_assert_uint_eq(header.asset_count, 2);

    // Parse all asset records
    r = bun_parse_assets(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_asset_name_outside_string_table) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/06-name-oob.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx, &header);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_asset_data_outside_data_section) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/07-data-oob.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx, &header);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_name_non_printable_ascii) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/08-name-non-printable.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx, &header);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_zero_length_name) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/09-zero-name.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx, &header);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
}
END_TEST

// Compression tests

// RLE compression should succeed
START_TEST(test_compression_rle) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("valid/04-rle-asset.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    bun_close(&ctx);
}
END_TEST

// zlib returns BUN_MALFORMED when uncompressed_size is 0, otherwise UNSUPPORTED
START_TEST(test_compression_zlib) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/11-zlib-compression.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx, &header);
    // If uncompressed_size > 0, returns UNSUPPORTED
    ck_assert_int_eq(r, BUN_UNSUPPORTED);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_compression_unknown) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/12-unknown-compression.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx, &header);
    // Unknown compression is unsupported
    ck_assert_int_eq(r, BUN_UNSUPPORTED);

    bun_close(&ctx);
}
END_TEST

// RLE with odd data size is malformed BEFORE checking compression support
START_TEST(test_rle_odd_data_size) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/13-rle-odd-size.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx, &header);
    // Odd data size makes it malformed
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
}
END_TEST

// RLE with zero count is malformed BEFORE checking compression support
START_TEST(test_rle_zero_count) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/14-rle-zero-count.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx, &header);
    // Zero count makes it malformed
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
}
END_TEST

// Edge case tests

START_TEST(test_file_not_found) {
    BunParseContext ctx = {0};

    // File that doesn't exist should return BUN_ERR_NOT_FOUND
    bun_result_t r = bun_open("nonexistent/path/to/file.bun", &ctx);
    ck_assert_int_eq(r, BUN_ERR_NOT_FOUND);
}
END_TEST

START_TEST(test_flags_unknown) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/19-flags-unknown.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    // Unknown flags must return BUN_UNSUPPORTED
    r = bun_parse_assets(&ctx, &header);
    ck_assert_int_eq(r, BUN_UNSUPPORTED);

    bun_close(&ctx);
}
END_TEST

START_TEST(test_checksum_nonzero) {
    BunParseContext ctx = {0};
    BunHeader header    = {0};

    bun_result_t r = bun_open(fixture("invalid/18-checksum-nonzero.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx, &header);
    ck_assert_int_eq(r, BUN_OK);

    // Parser should return UNSUPPORTED for non-zero checksum
    r = bun_parse_assets(&ctx, &header);
    ck_assert_int_eq(r, BUN_UNSUPPORTED);

    bun_close(&ctx);
}
END_TEST

// Assemble a test suite from our tests

static Suite *bun_suite(void) {
    Suite *s = suite_create("bun-suite");

    // Header validation tests
    TCase *tc_header = tcase_create("header-tests");
    tcase_add_test(tc_header, test_valid_minimal);
    tcase_add_test(tc_header, test_bad_magic);
    tcase_add_test(tc_header, test_unsupported_version);
    tcase_add_test(tc_header, test_bad_version_major);
    tcase_add_test(tc_header, test_truncated_header);
    tcase_add_test(tc_header, test_offsets_not_divisible_by_4);
    tcase_add_test(tc_header, test_section_out_of_bounds);
    tcase_add_test(tc_header, test_sections_overlap);
    tcase_add_test(tc_header, test_empty_file);
    suite_add_tcase(s, tc_header);

    // Asset validation tests
    TCase *tc_assets = tcase_create("asset-tests");
    tcase_add_test(tc_assets, test_valid_single_asset);
    tcase_add_test(tc_assets, test_valid_multiple_assets);
    tcase_add_test(tc_assets, test_valid_encrypted);
    tcase_add_test(tc_assets, test_valid_executable);
    tcase_add_test(tc_assets, test_valid_two_simple);
    tcase_add_test(tc_assets, test_asset_name_outside_string_table);
    tcase_add_test(tc_assets, test_asset_data_outside_data_section);
    tcase_add_test(tc_assets, test_name_non_printable_ascii);
    tcase_add_test(tc_assets, test_zero_length_name);
    suite_add_tcase(s, tc_assets);

    // Compression tests
    TCase *tc_compression = tcase_create("compression-tests");
    tcase_add_test(tc_compression, test_compression_rle);
    tcase_add_test(tc_compression, test_compression_zlib);
    tcase_add_test(tc_compression, test_compression_unknown);
    tcase_add_test(tc_compression, test_rle_odd_data_size);
    tcase_add_test(tc_compression, test_rle_zero_count);
    suite_add_tcase(s, tc_compression);

    // Edge case tests
    TCase *tc_edge = tcase_create("edge-case-tests");
    tcase_add_test(tc_edge, test_file_not_found);
    tcase_add_test(tc_edge, test_flags_unknown);
    tcase_add_test(tc_edge, test_checksum_nonzero);
    suite_add_tcase(s, tc_edge);

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