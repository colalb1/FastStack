cmake_minimum_required(VERSION 3.25)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" FASTSTACK_VERSION LIMIT_COUNT 1)
if(NOT FASTSTACK_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION file must contain SemVer 'MAJOR.MINOR.PATCH'.")
endif()

project(FastStack VERSION ${FASTSTACK_VERSION} LANGUAGES CXX)

# This targets Apple Silicon only.
if(NOT APPLE)
    message(FATAL_ERROR "FastStack currently supports macOS on Apple Silicon only.")
endif()

if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    message(FATAL_ERROR "FastStack currently supports Apple Silicon (arm64) only.")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FASTSTACK_BUILD_TESTS "Build FastStack tests" ON)
option(FASTSTACK_ENABLE_INSTALL "Enable install/export rules" ON)

# Header-first library
add_library(faststack INTERFACE)
add_library(faststack::faststack ALIAS faststack)

target_compile_features(faststack INTERFACE cxx_std_23)
target_include_directories(faststack
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(FASTSTACK_BUILD_TESTS)
    enable_testing()

    add_executable(faststack_tests
        tests/basic_compile_test.cpp
    )
    target_link_libraries(faststack_tests PRIVATE faststack::faststack)
    add_test(NAME faststack_tests COMMAND faststack_tests)
endif()

if(FASTSTACK_ENABLE_INSTALL)
    install(
        TARGETS faststack
        EXPORT faststackTargets
    )

    install(
        DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(
        EXPORT faststackTargets
        FILE faststackTargets.cmake
        NAMESPACE faststack::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/faststack
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/faststackConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/faststackConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/faststack
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/faststackConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(
        FILES
            ${CMAKE_CURRENT_BINARY_DIR}/faststackConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/faststackConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/faststack
    )
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
