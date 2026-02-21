cmake_minimum_required(VERSION 3.25)
project(FastStack VERSION 0.1.0 LANGUAGES CXX)

# This targets Apple Silicon only.
if(NOT APPLE)
    message(FATAL_ERROR "FastStack currently supports macOS on Apple Silicon only.")
endif()

if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    message(FATAL_ERROR "FastStack currently supports Apple Silicon (arm64) only.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FASTSTACK_BUILD_TESTS "Build FastStack tests" ON)

add_library(faststack INTERFACE)
target_include_directories(faststack INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(FASTSTACK_BUILD_TESTS)
    enable_testing()

    add_executable(faststack_tests
        tests/basic_compile_test.cpp
    )
    target_link_libraries(faststack_tests PRIVATE faststack)
    add_test(NAME faststack_tests COMMAND faststack_tests)
endif()

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE FASTSTACK_SOURCE_FILES
        include/*.hpp
        src/*.cpp
        tests/*.cpp
    )

    if(FASTSTACK_SOURCE_FILES)
        add_custom_target(
            format
            COMMAND ${CLANG_FORMAT_EXE} -i ${FASTSTACK_SOURCE_FILES}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-format on FastStack sources"
        )
    endif()
endif()
