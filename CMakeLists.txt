cmake_minimum_required(VERSION 3.20)
project(RogueByteStegoPro
    VERSION 1.0.0
    DESCRIPTION "Cross-platform steganography and digital-forensics suite"
    HOMEPAGE_URL "https://github.com/SidoDev-Dz/roguebyte-stego-pro"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags and warnings
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -fno-strict-aliasing)
endif()

# Dependencies
find_package(OpenSSL REQUIRED)

# Core Library
set(ROGUEBYTE_CORE_SOURCES
    src/core/common.cpp
    src/core/forensics.cpp
    src/crypto/crypto.cpp
    src/container/container.cpp
    src/container/manifest.cpp
    src/image/image_backend.cpp
    src/image/png_backend.cpp
    src/image/bmp_backend.cpp
    src/image/jpeg_backend.cpp
    src/image/lodepng/lodepng.cpp
)

add_library(roguebyte_stego_core STATIC ${ROGUEBYTE_CORE_SOURCES})

target_include_directories(roguebyte_stego_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/image>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(roguebyte_stego_core PUBLIC
    OpenSSL::Crypto
)

if(UNIX AND NOT APPLE)
    target_link_libraries(roguebyte_stego_core PUBLIC pthread dl)
endif()

# CLI Executable: rb-stego
add_executable(rb-stego src/cli/main.cpp)
target_link_libraries(rb-stego PRIVATE roguebyte_stego_core)
set_target_properties(rb-stego PROPERTIES OUTPUT_NAME "rb-stego")

# Testing
enable_testing()

macro(add_rb_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE roguebyte_stego_core)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endmacro()

# Unit Tests
add_rb_test(test_crypto tests/unit/test_crypto.cpp)
add_rb_test(test_container tests/unit/test_container.cpp)
add_rb_test(test_manifest tests/unit/test_manifest.cpp)
add_rb_test(test_image_backends tests/unit/test_image_backends.cpp)

# Integration Tests
add_rb_test(test_roundtrip tests/integration/test_roundtrip.cpp)
add_rb_test(test_forensics tests/integration/test_forensics.cpp)
add_rb_test(test_security tests/integration/test_security.cpp)

# Installation
install(TARGETS rb-stego roguebyte_stego_core
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/roguebyte_stego DESTINATION include)
