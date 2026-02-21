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

## Build Locally

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Consumer Usage

### Option 1: add_subdirectory (local checkout)

```cmake
add_subdirectory(path/to/FastStack)
target_link_libraries(my_app PRIVATE faststack::faststack)
```

### Option 2: FetchContent (remote source)

```cmake
include(FetchContent)
FetchContent_Declare(
  faststack
  GIT_REPOSITORY https://github.com/<you>/FastStack.git
  GIT_TAG main
)
FetchContent_MakeAvailable(faststack)
target_link_libraries(my_app PRIVATE faststack::faststack)
```

### Option 3: Installed package (find_package)

Install FastStack:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix "$HOME/.local"
```

Use in another project:

```cmake
find_package(faststack CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE faststack::faststack)
```

## Formatting

```bash
cmake --build build --target format
```

## Notes

- TODO markers in headers and tests identify the next implementation steps.
