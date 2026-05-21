# CITS3007 Report

Group: Richie Ng (24561394), Vanessa Do (24342062), Tep Nguyen (24465269), Diarmuid O'Connor (23962159), Harrison Gardener (23626175)

### Output Format

The bun_parser program produces output on two streams:  
Standard Output (stdout): Human-readable representation of the BUN file contents  
Standard Error (stderr): Validation errors and specification violations

1. Header Output (stdout)  
   If the header can be safely parsed, it is printed in the following format:  
   === BUN Header ===  
   Magic: 0xXXXXXXXX  
   Version: \<major>.\<minor>  
   Asset Count: \<number>  
   Asset Table Offset: \<number>  
   String Table Offset: \<number>  
   String Table Size: \<number>  
   Data Section Offset: \<number>  
   Data Section Size: \<number>

Notes:  
All header fields defined in the specification are displayed  
Magic is printed in hexadecimal  
All other fields are printed in decimal

2. Asset Output (stdout)  
   If asset records can be safely parsed, they are printed as follows:  
   === Assets (\<asset_count>) ===  
   Asset \<index>  
   Name Offset: \<number>  
   Name Length: \<number>  
   Data Offset: \<number>  
   Data Size: \<number>  
   Uncompressed Size: \<number>  
   Compression: \<number>  
   Type: \<number>  
   Checksum: \<number>  
   Flags: \<number>  
   Payload: \<hex bytes...>

Payload Formatting:  
Payload data is displayed as a sequence of hexadecimal bytes:  
Payload: 48 65 6C 6C 6F ...  
Only the first ~60 bytes are shown  
If the payload exceeds this limit, it is truncated with: ...

3. RLE Decompressed Output (if applicable)  
   If an asset uses RLE compression (compression == 1), an additional line is printed:  
   Decompressed Payload: \<hex bytes...>

Notes:  
Represents the expanded data after RLE decoding  
Also truncated to ~60 bytes if necessary  
If the decompressed size does not match expectations, a warning is shown:  
Warning: decompressed size was \<actual>, expected \<expected>

4. Error Output (stderr)  
   All detected errors are printed to standard error in the following format:  
   === Errors ===  
   Error 1: \<error code> \<description>  
   Error 2: \<error code> \<description>  
   ...

Notes:
One violation per line  
Messages include enough detail to locate the issue  
Errors are numbered sequentially  
If too many errors occur: Note: \<N> additional errors not shown (exceeds BUN_MAX_ERRORS)

5. Partial Output for Invalid Files  
   For malformed or unsupported files:  
   The parser prints as much information as can be safely determined  
   Possible cases:  
   Only header printed  
   Header + some valid assets printed  
   No output if file is completely unreadable

#### Error Codes

1 (BUN_MALFORMED): Input contains information that would cause BUN_MALFORMED as per the spec.  
2 (BUN_UNSUPPORTED): Input contains an unsupported version or feature, such as non-RLE compression.  
3 (BUN_ERR_IO): Input causes an I/O error, such as a file read error or the file cannot be found.  
4 (BUN_ERR_OVERFLOW): Input causes program logic to overflow (as in assumption described below).  
5 (BUN_ERR_ALLOC): Program memory allocation fails.  
6 (BUN_ERR_ARGS): Input contains the incorrect number of arguments (expect 1).

### Assumptions

BUN files that would cause overflow are treated as erroneous by the parser. This will cause the return of the special error code 4 (BUN_ERR_OVERFLOW).  
The parser is assumed to be run on the CITS3007 development environment, and longs are thus assumed to be 64-bit.  
Given that multiple error codes are detected, the final error code returned by the parser is determined by the 'worst error'. Lower error codes were treated as worse than higher ones, with the exception of all errors being worse than the ok state 0 (BUN_OK).

- This is due to lower error codes representing errors in the BUN file itself (particularly 1 (BUN_MALFORMED)), whilst higher errors can possibly be amended by a different parser (like (BUN_UNSUPPORTED)) or are caused by device context (for instance, lack of memory).

The assumption has been made that for flags, the last and second last bits can both be 1, so 0x1, 0x2, and 0x3 are valid flags.

### Libraries Used

Our main executable (`bun_parser`) does not depend on any third-party libraries. It was built entirely using the C Standard Library (`libc`) and standard POSIX APIs (such as `sys/stat.h` for file size validation).

The Check framework, `libcheck`, was utilized exclusively for our separate unit-testing binary and is not compiled into or required by the main executable. It allowed us to write individual tests for our function behaviour by asserting expected behaviour, and allowed us to run all of our tests at once automatically and generate a report with `make test`.

### Tools Used

Throughout the development and quality assurance (QA) phases of this project, our team utilized several tools to ensure memory safety, strict parsing validation, and robust version control.

1.  AddressSanitizer (ASan):
    Purpose: A dynamic analysis tool used to detect memory leaks, buffer overflows, and use-after-free errors.
    How it was run: Integrated into the `Makefile` via the `CFLAGS += -fsanitize=address` flag and executed during our `make test` suite.
    Findings & Evidence: ASan successfully detected a 5-byte memory leak in `bun_parse_assets` originating from a `realloc` call. This was found when the `free(str_buf)` statement was intentionally omitted during QA testing. A marker can reproduce this by commenting out `free(str_buf)` at the end of `bun_parse_assets` and running `make test` with ASan enabled. (See attached evidence: [ASan Sabotage Code](https://github.com/harri-g/cits3007_project/blob/main/images/evidence_asan_sabotage_code.png) and [ASan Terminal Error](https://github.com/harri-g/cits3007_project/blob/main/images/evidence_asan_terminal_error.png)).

2.  UndefinedBehaviorSanitizer (UBSan):
    Purpose: A dynamic analysis tool used to catch undefined behavior such as illegal math operations and signed integer overflows.
    How it was run: Integrated into the `Makefile` via the `CFLAGS += -fsanitize=undefined` flag.
    Findings & Evidence: UBSan caught a runtime error (`signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'`) when an intentional integer overflow was injected into `bun_parse_header` during QA testing. A marker can reproduce this by injecting `int a = 2147483647; int b = a + 1;` into `bun_parse_header` and running `make test` with UBSan enabled. (See attached evidence: [UBSan Sabotage Code](https://github.com/harri-g/cits3007_project/blob/main/images/evidence_ubsan_sabotage_code.png) and [UBSan Terminal Error](https://github.com/harri-g/cits3007_project/blob/main/images/evidence_ubsan_terminal_error.png)).

3.  GCC Compiler (Static Analysis):
    Purpose: Enforced strict C11 coding standards and caught potential vulnerabilities at compile-time.
    How it was run: Configured in the `Makefile` using strict security flags: `gcc -std=c11 -pedantic-errors -Wall -Wextra -Wconversion -Werror=vla -Werror=implicit-function-declaration -o bun_parser main.c bun_parse.c`.
    Findings & Evidence: The compiler successfully prevented Variable Length Array (VLA) vulnerabilities by upgrading them to compile-time errors. It also caught silent sign conversion risks (e.g., implicit casts in `fseek`), forcing the team to explicitly cast unsigned integers to signed longs to ensure memory safety.
    (See attached evidence: [Makefile Release Mode Settings](https://github.com/harri-g/cits3007_project/blob/main/images/evidence_makefile_release_mode.png)).
    The link to the pull request for this change is: [Pull Request #29](https://github.com/harri-g/cits3007_project/pull/29/changes/60a4c62bf511ecca31cd8d41be15d48e43128495).

4.  Check Framework (`libcheck`):
    Purpose: The primary C unit-testing framework used to validate parsing logic and edge cases.
    How it was run: Executed via `make test`, which compiles and runs the `tests/test_runner` binary.
    Findings & Evidence: Allowed the team to build a comprehensive suite (`test_bun.c`) that caught logical bugs. Most notably, the test suite caught a bug where the `add_error` function was failing to correctly record errors due to a `<` comparison against `BUN_OK` (0). The test suite allowed us to confidently patch the bug and verify the fix. (See attached evidence: [Custom Unit Tests Source Code](https://github.com/harri-g/cits3007_project/blob/main/images/evidence_custom_unit_tests.png) and [Clean Terminal Pass](https://github.com/harri-g/cits3007_project/blob/main/images/evidence_terminal_release_mode.png)).

5.  Generative AI (Gemini, Claude, ChatGPT):
    Purpose: Used transparently as coding assistants and secure-coding mentors throughout the project.
    Usage:
    Claude: Assisted with error message data types, fast binary deserialization generation, `fseek` offset logic, and architectural flow in `main.c`.
    ChatGPT: Assisted with understanding bitwise logic for flag validation and suggesting maximum bounds for error reporting arrays.
    Gemini: Acted as a QA and secure-coding mentor, advising on POSIX compliance (`fstat`), strict compiler flags, resolving VLA vulnerabilities, and unit test generation.
    Evidence: All AI usage is formally declared and detailed in the source code headers of `bun_parse.c`, `bun.h`, `main.c`, and the `Makefile`.

### Security Aspects

It is assumed for this question that 'player-created content' implies that players can, through some means, upload their own BUN files to the server to be downloaded and parsed by other playres' clients.  
This can cause a variety of issues. These include:

- There exists a flag 0x2u that marks a file as executable. Attackers submitting executable files can possibly cause the parser to try to execute malicious code inserted into the BUN files. The severity of this issue depends on the level of privilege the parser has. Even without many privileges, the parser still could have privileges to do with the game, which could result in threats such as corruption or extraction of player data.
  - A fix to this could be to remove the ability to flag BUN files as executable, or at the very least a change to the spec that makes user submitted BUN files non-executable, perhaps adding a field set by the server that indicates whether a file is from a user.
- Small files can declare huge sizes for allocated values such as asset names or number of assets. This could overload users' devices, as the parser could try to allocate a huge amount of memory.
  - This could be fixed by either changing the specification to mark a maximum size for certain fields, or defining a minimum size that a parser must accept (allowing it to reject larger). This would allow the parser to be implemented such that it has an upper limit to the amount of memory it must allocate, preventing the allocation of large amounts.
- Similar to above, a BUN file can be small when compressed but massive when uncompressed, causing the parser to allocate a huge amount of memory to decompression.
  - To fix this, as above, a maximum uncompressed size or a minimum that the parser must accept could be implemented, with reasoning mirroring the above issue.
- Fields such asset names and data payloads can overlap, as overlap for them is never checked. As the parser feeds information directly into a game client, this could cause unexpected behaviour within the game, depending on how the game handles the data and how overlapping data could affect in-game assets.
  - To fix this, the spec could be amended to require checking that these bounds do not overlap. The parser could be updated with a loop that checks for no overlaps on these values in addition to those it already checks.

### Coding Standards

Our coding standards were primarily to keep code looking consistent, and make debugging and reading each others' code easier.  
For naming variables, we decided to use `snake_case` for normal variables, and kept with the scaffolding standard of `SCREAMING_SNAKE_CASE` for enum names such as error codes and constants.  
`PascalCase` is used as in the scaffolding for struct names.  
We agreed to use same line braces, for example:

```
if () {
    ...
}
```

rather than

```
if ()
{
    ...
}
```

To make git easier to use, we decided to name our branches (branch creator name)-(branch title).  
We also prevented pushing to main on our github, so that all changes would need to be pull requested.  
We mandated that all code changes needed to be reviewed by another group member before mergiing.

### Challenges

Throughout the development of the BUN parser, our group encountered several technical and logistical challenges.

1. GitHub Collaboration Issues  
   We faced several challenges working together using GitHub.  
   Merge Conflicts  
   Multiple people edited shared files like bun.h and main.c, which caused frequent merge conflicts. At times, resolving these was confusing and we were unsure which version to keep.  
   We improved this by:  
   Communicating before making changes to shared files  
   Committing more frequently  
   Carefully reviewing conflicts instead of rushing

2. Understanding the BUN Specification  
   At the start, one of the biggest challenges was simply understanding how the file format worked. The specification is quite detailed, and it wasn’t immediately obvious how all the sections (header, asset table, string table, data section) connected together.  
   We initially underestimated how important it was to fully understand offsets and how they relate to each section. This led to some confusion when implementing parsing logic. We broke the spec into smaller parts and mapping it out visually, making it much easier to follow.

3. Testing and Debugging  
   Testing the parser was harder than expected because we needed both valid and invalid .bun files. While sample files were provided, they didn’t cover all edge cases.  
   We spent time:  
   Running the generator  
   Modifying files manually  
   Testing unusual inputs  
   Debugging was also challenging because errors in binary parsing often don’t produce obvious symptoms—they just give incorrect values.

4. Integrating Different Parts
   Since different people worked on different modules (header, assets, validation, output), combining everything was not straightforward. Sometimes one part depended on another being finished or working correctly.  
   We solved this by:  
   Agreeing on shared structures in bun.h  
   Using placeholder implementations early on  
   Testing components individually before integrating
