# FastStack

CHANGE THE NAME OF THIS LATER!!!

FastStack is a C++ data-structure library focused on stack and queue implementations.

## Platform Support

FastStack currently supports **macOS on Apple Silicon (ARM64)** only.
Configuration fails on unsupported platforms by design.

## Project Layout

- `include/faststack/stack.hpp`: stack API skeleton
- `include/faststack/queue.hpp`: queue API skeleton
- `tests/basic_compile_test.cpp`: basic compile/link smoke test
- `src/`: implementation files (minimal scaffold)

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Formatting

```bash
cmake --build build --target format
```

## Notes

- TODO markers in headers and tests identify the next implementation steps.
