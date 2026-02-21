# Seraph

Seraph is a C++ data-structure library for Apple ARM64.

## Project Layout

- `include/seraph/stack.hpp`: stack API skeleton
- `include/seraph/queue.hpp`: queue API skeleton
- `tests/basic_compile_test.cpp`: basic compile/link smoke test
- `src/`: implementation files (minimal scaffold)
- `VERSION`: package semantic version (`MAJOR.MINOR.PATCH`)


## Build Locally

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Consumer Usage

### Option 1: add_subdirectory (local checkout)

```cmake
add_subdirectory(path/to/Seraph)
target_link_libraries(my_app PRIVATE seraph::seraph)
```

### Option 2: FetchContent (remote source)

```cmake
include(FetchContent)
FetchContent_Declare(
  seraph
  GIT_REPOSITORY https://github.com/<you>/Seraph.git
  GIT_TAG main
)
FetchContent_MakeAvailable(seraph)
target_link_libraries(my_app PRIVATE seraph::seraph)
```

### Option 3: Installed package (find_package)

Install Seraph:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix "$HOME/.local"
```

Use in another project:

```cmake
find_package(seraph CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE seraph::seraph)
```

## Formatting

```bash
cmake --build build --target format
```

## Notes

- TODO markers in headers and tests identify the next implementation steps.
