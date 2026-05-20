# Contributor's guide

Notes for anyone working on the BUN parser.

## Project layout

- `bun.h`: public API, types, spec constants
- `bun_parse.c`: `bun_open` / `bun_parse_header` / `bun_parse_assets` / `bun_close`
- `bun_print.c`: header and asset record output
- `bun_violations.c`: violation list helpers
- `validators/header_validators.{c,h}`: magic, offsets, version, section bounds, overlap
- `validators/asset_validators.{c,h}`: name, bounds, compression, checksum, flags
- `main.c`: CLI entry point
- `tests/test_bun.c`: libcheck test suite
- `tests/generate_fixtures.py`: builds the .bun fixtures used by the tests

## Build

- `make all`: builds `bun_parser`
- `make test`: generates fixtures if missing, then runs the test suite
- `make clean`: removes binaries, `.o` files, and `tests/fixtures/`

The Makefile has a commented-out line that enables AddressSanitizer and UndefinedBehaviorSanitizer. Uncomment it while debugging; recomment before submitting.

## Conventions

- C11, built with `-Wall -Wextra -Wpedantic`. Anything that warns gets fixed.
- 2-space indents, no tabs.
- Use `PRIu64` / `PRIX32` from `<inttypes.h>` for fixed-width printf; avoid `%llu` with casts.
- On-disk structures are parsed field-by-field with the `read_u32_le` / `read_u64_le` helpers. Do not cast a `struct` pointer over file bytes, since struct padding makes that undefined behaviour.
- Validators don't print. They call `bun_add_violation(ctx, severity, "...", ...)` to append to the list on the parse context; `main` prints the list.
- Public functions have one-line docstrings in the header. Implementations stay light on commentary.

## Adding a validator

1. Declare it in `header_validators.h` or `asset_validators.h` with a one-line docstring.
2. Implement it. Append violations rather than printing.
3. Call it from `bun_parse_header` or `bun_parse_assets`.
4. Add a fixture in `tests/generate_fixtures.py` that triggers it.
5. Add a test in `tests/test_bun.c` and register it under the matching TCase.

## Memory ownership

- The violation list belongs to the parse context. `bun_close` frees it.
- Validators that allocate (`validate_asset_name`) free before they return.
- `bun_open` clears `ctx->file` on every failure path, so a stray `bun_close` after a failed open is a no-op rather than a double-free.

## Tools used during development

- `clang-tidy` (clang-analyzer)
- `flawfinder`
- AddressSanitizer + UndefinedBehaviorSanitizer
- libcheck

To rerun:

- `clang-tidy *.c validators/*.c -- -std=c11 -I. -Ivalidators`
- `flawfinder --quiet --columns .`
- `make clean && make test` (with the sanitizer line uncommented)
