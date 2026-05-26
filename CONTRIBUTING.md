# Contributing to RGTP

Thank you for your interest in contributing to the Red Giant Transport Protocol. All contributions are welcome — bug fixes, performance improvements, documentation, new examples, and language bindings.

## Code of Conduct

Please read and follow the [Code of Conduct](CODE_OF_CONDUCT.md).

## Development Setup

### Prerequisites

- CMake 3.20+
- GCC 11+, Clang 14+, or MSVC 19.30+ (C17 mode)
- libsodium 1.0.18+ (default) or OpenSSL 3.0+
- Go 1.21+ (for Go binding)
- Node.js 18 LTS, 20 LTS, or 22 LTS (for Node.js binding)
- Python 3.9+ (for Python binding)

### Build and Test

```bash
git clone https://github.com/rawscript/red-giant.git
cd red-giant

# Configure with tests and FEC enabled
cmake -B build \
  -DRGTP_CRYPTO_BACKEND=libsodium \
  -DRGTP_ENABLE_FEC=ON \
  -DRGTP_BUILD_TESTS=ON \
  -DRGTP_BUILD_EXAMPLES=ON

cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Static Analysis

```bash
cmake --build build --target analyze
```

### Binding Tests

```bash
# Node.js
cd bindings/node && npm test

# Go
cd bindings/go && go test ./...

# Python
cd bindings/python && python -m pytest tests/ -v
```

## Workflow

1. Fork the repository and create a feature branch:
   ```bash
   git checkout -b feature/your-feature
   ```

2. Make your changes. All C code must be C17 and compile cleanly under `-Wall -Wextra -Wpedantic` with zero warnings.

3. Add or update tests. New features require unit tests in `tests/unit/`. Bug fixes require a regression test in `tests/regression/`.

4. Run the full test suite and confirm it passes:
   ```bash
   ctest --test-dir build --output-on-failure
   ```

5. Run sanitizers:
   ```bash
   cmake -B build-asan \
     -DCMAKE_C_FLAGS="-fsanitize=address,undefined" \
     -DRGTP_BUILD_TESTS=ON -DRGTP_ENABLE_FEC=ON
   cmake --build build-asan --parallel
   ctest --test-dir build-asan --output-on-failure
   ```

6. Commit and push:
   ```bash
   git commit -m "feat: describe your change"
   git push origin feature/your-feature
   ```

7. Open a Pull Request. PRs are reviewed within 2 business days.

## Code Style

- **C17** throughout. No compiler extensions (`-std=c17`, not `-std=gnu17`).
- All public API functions return `rgtp_error_t`. No `void` functions that can fail.
- All heap allocations must check for `NULL` and return `RGTP_ERR_NOMEM` on failure.
- No `strcpy`, `sprintf`, or `gets`. Use `strncpy`, `snprintf`, `strnlen`.
- No signed integer overflow in size or index arithmetic. Use `uint32_t`/`uint64_t` with explicit overflow guards.
- Key material must be zeroized with `rgtp_zeroize()` before `free`.
- Global mutable state must be protected by `pthread_once` or `InitOnceExecuteOnce`.
- New source files must include a Doxygen `@file` comment and `@brief` description.

## What to Contribute

- **Bug fixes** — check the issue tracker for open bugs. All fixes need a regression test.
- **Performance** — benchmarks are in `tests/bench/`. Profile before and after.
- **Documentation** — docs live in `docs/`. Architecture diagrams use Mermaid.
- **Language bindings** — new bindings go in `bindings/<language>/`. Follow the existing Node.js, Go, and Python patterns.
- **Examples** — C examples go in `examples/c/`. Keep them self-contained.
- **Platform support** — new toolchain files go in `cmake/toolchains/`.

## Questions

Open a GitHub Discussion or email [jasemwaura@gmail.com](mailto:jasemwaura@gmail.com).
