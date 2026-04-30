# BUN parser

A C11 parser for the BUN (Binary UNified assets) container format, developed for CITS3007 Phase 1.

## Group 9

| Name | Student # |
|------|-----------|
| Cameron Kelly      | 23862126 |
| Tyson Haines       | 23779585 |
| Darcy Tyler        | 23390948 |
| Xavier Kuang       | 24466935 |
| Yashwardhan Laharia | 24295462 |

Repository: <https://github.com/TysonHaines/CITS3007-GroupProject-Group9>

## Build and run

```sh
make all
./bun_parser path/to/file.bun
```

## Tests

```sh
./setup.sh    # one-time, installs pkg-config and check
make test
```

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | OK — file parsed successfully |
| 1 | Malformed — file violates the BUN spec |
| 2 | Unsupported — file uses features this parser does not implement |
| 3 | I/O error during read or seek |
| 4 | File not found |
| 5 | Wrong number of command-line arguments |
| 6 | Memory allocation failed |

## Output

- Valid file: header and asset records printed to stdout.
- Invalid file: as much of the file as can be safely shown is printed to stdout; one violation per line is printed to stderr.

See the project report for full output format documentation, design decisions, and security analysis.