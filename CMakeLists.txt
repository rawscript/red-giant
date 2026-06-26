cmake_minimum_required(VERSION 3.15)
project(red-giant C)

# ─────────────────────────────────────────────────────────────────[...]
# Project metadata
# ─────────────────────────────────────────────────────────────────[...]
set(PROJECT_VERSION 1.0.0)
set(PROJECT_DESCRIPTION "Red Giant Exposure-based Transport Protocol")

# ──────────────────────────────────────────────────────────────────[...]
# Compilation settings
# ──────────────────────────────────────────────────────────────────[...]
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Position independent code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ──────────────────────────────────────────────────────────────────[...]
# Build options
# ──────────────────────────────────────────────────────────────────[...]
set(RGTP_CRYPTO_BACKEND "libsodium" CACHE STRING "Cryptography backend (libsodium or openssl)")
set_property(CACHE RGTP_CRYPTO_BACKEND PROPERTY STRINGS libsodium openssl)

option(RGTP_BUILD_TESTS "Build test suite" ON)
option(RGTP_BUILD_EXAMPLES "Build example programs" ON)
option(RGTP_ENABLE_FEC "Enable Forward Error Correction" ON)
option(RGTP_ENABLE_SIMD "Enable SIMD optimizations" ON)

# Fuzz-specific flags (applied only to fuzz targets, not compiler checks)
set(RGTP_FUZZ_FLAGS "" CACHE STRING "Compiler flags for fuzz targets only")

# ──────────────────────────────────────────────────────────────────[...]
# Compiler flags
# ──────────────────────────────────────────────────────────────────[...]
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-g -O2 -DNDEBUG")

# Common warnings for all builds
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-prototypes)
endif()

# ──────────────────────────────────────────────────────────────────[...]
# SIMD support
# ──────────────────────────────────────────────────────────────────[...]
if(RGTP_ENABLE_SIMD)
    include(CheckCCompilerFlag)
    check_c_compiler_flag(-march=native COMPILER_SUPPORTS_MARCH_NATIVE)
    if(COMPILER_SUPPORTS_MARCH_NATIVE AND NOT APPLE)
        add_compile_options(-march=native)
    endif()
endif()

# ──────────────────────────────────────────────────────────────────[...]
# Feature definitions
# ──────────────────────────────────────────────────────────────────[...]
if(RGTP_ENABLE_FEC)
    add_compile_definitions(RGTP_ENABLE_FEC)
endif()

if(RGTP_ENABLE_SIMD)
    add_compile_definitions(RGTP_ENABLE_SIMD)
endif()

#
──────────────────────────────────────────────────────────────────[...]
# CCSDS adaptor 
# ──────────────────────────────────────────────────────────────────[...]
option(RGTP_TRANSPORT_CCSDS "Build with CCSDS Space Packet transport" OFF)

if(RGTP_TRANSPORT_CCSDS)
  target_sources(rgtp PRIVATE src/io/transport_ccsds.c)
  target_compile_definitions(rgtp PRIVATE RGTP_HAVE_CCSDS=1)
endif() ──────────────────────────────────────────────────────────────────[...]
# Crypto backend detection
# ──────────────────────────────────────────────────────────────────[...]
if(RGTP_CRYPTO_BACKEND STREQUAL "libsodium")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SODIUM REQUIRED libsodium)
    set(CRYPTO_LIBRARIES ${SODIUM_LIBRARIES})
    set(CRYPTO_INCLUDE_DIRS ${SODIUM_INCLUDE_DIRS})
    add_compile_definitions(RGTP_USE_LIBSODIUM)
    message(STATUS "Using libsodium for cryptography")
elseif(RGTP_CRYPTO_BACKEND STREQUAL "openssl")
    find_package(OpenSSL REQUIRED)
    set(CRYPTO_LIBRARIES OpenSSL::Crypto)
    set(CRYPTO_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
    add_compile_definitions(RGTP_USE_OPENSSL)
    message(STATUS "Using OpenSSL for cryptography")
else()
    message(FATAL_ERROR "Unknown crypto backend: ${RGTP_CRYPTO_BACKEND}")
endif()

# ──────────────────────────────────────────────────────────────────[...]
# Source files
# ──────────────────────────────────────────────────────────────────[...]
file(GLOB_RECURSE RGTP_SOURCES "src/*.c")
file(GLOB_RECURSE RGTP_HEADERS "include/*.h")

# ──────────────────────────────────────────────────────────────────[...]
# Library targets
# ──────────────────────────────────────────────────────────────────[...]
add_library(rgtp_shared SHARED ${RGTP_SOURCES})
add_library(rgtp_static STATIC ${RGTP_SOURCES})

# Set library names
set_target_properties(rgtp_shared PROPERTIES OUTPUT_NAME rgtp)
set_target_properties(rgtp_static PROPERTIES OUTPUT_NAME rgtp)

# Public headers
target_include_directories(rgtp_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CRYPTO_INCLUDE_DIRS}
)

target_include_directories(rgtp_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CRYPTO_INCLUDE_DIRS}
)

# Link crypto libraries
target_link_libraries(rgtp_shared PRIVATE ${CRYPTO_LIBRARIES} m pthread)
target_link_libraries(rgtp_static PRIVATE ${CRYPTO_LIBRARIES} m pthread)

# Platform-specific linker settings
if(WIN32)
    target_link_libraries(rgtp_shared PRIVATE ws2_32 iphlpapi)
    target_link_libraries(rgtp_static PRIVATE ws2_32 iphlpapi)
endif()

# ──────────────────────────────────────────────────────────────────[...]
# Version information
# ──────────────────────────────────────────────────────────────────[...]
set_target_properties(rgtp_shared PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# ──────────────────────────────────────────────────────────────────[...]
# Tests
# ──────────────────────────────────────────────────────────────────[...]
if(RGTP_BUILD_TESTS)
    enable_testing()
    
    file(GLOB TEST_SOURCES "tests/*.c" "test/*.c")
    foreach(test_file ${TEST_SOURCES})
        get_filename_component(test_name ${test_file} NAME_WE)
        add_executable(test_${test_name} ${test_file})
        target_include_directories(test_${test_name}
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
            PRIVATE ${CRYPTO_INCLUDE_DIRS}
        )
        target_link_libraries(test_${test_name} PRIVATE rgtp_static ${CRYPTO_LIBRARIES} m pthread)
        add_test(NAME ${test_name} COMMAND test_${test_name})
    endforeach()
    
    # Fuzz tests
    file(GLOB FUZZ_SOURCES "tests/fuzz/*.c")
    foreach(fuzz_file ${FUZZ_SOURCES})
        get_filename_component(fuzz_name ${fuzz_file} NAME_WE)
        add_executable(${fuzz_name} ${fuzz_file})
        target_include_directories(${fuzz_name}
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
            PRIVATE ${CRYPTO_INCLUDE_DIRS}
        )
        target_link_libraries(${fuzz_name} PRIVATE rgtp_static ${CRYPTO_LIBRARIES} m pthread)
        
        # Apply fuzz-specific flags only to fuzz targets
        if(RGTP_FUZZ_FLAGS)
            target_compile_options(${fuzz_name} PRIVATE ${RGTP_FUZZ_FLAGS})
            target_link_options(${fuzz_name} PRIVATE ${RGTP_FUZZ_FLAGS})
        endif()
    endforeach()
endif()

# ──────────────────────────────────────────────────────────────────[...]
# Examples
# ──────────────────────────────────────────────────────────────────[...]
if(RGTP_BUILD_EXAMPLES)
    file(GLOB EXAMPLE_SOURCES "examples/c/*.c")
    foreach(example_file ${EXAMPLE_SOURCES})
        get_filename_component(example_name ${example_file} NAME_WE)
        add_executable(${example_name} ${example_file})
        target_include_directories(${example_name}
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
            PRIVATE ${CRYPTO_INCLUDE_DIRS}
        )
        target_link_libraries(${example_name} PRIVATE rgtp_static ${CRYPTO_LIBRARIES} m pthread)
    endforeach()
endif()

# ──────────────────────────────────────────────────────────────────[...]
# Static analysis target
# ──────────────────────────────────────────────────────────────────[...]
add_custom_target(analyze
    COMMAND cppcheck --enable=all --std=c99 ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include || true
    COMMAND clang-tidy-14 ${RGTP_SOURCES} -- -I${CMAKE_CURRENT_SOURCE_DIR}/include || true
    COMMENT "Running static analysis tools..."
)

# ─────────────────────────────────────────────────────────────────[...]
# Installation
# ──────────────────────────────────────────────────────────────────[...]
include(GNUInstallDirs)

# Install libraries
install(TARGETS rgtp_shared rgtp_static
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# ──────────────────────────────────────────────────────────────────[...]
# Print configuration summary
# ──────────────────────────────────────────────────────────────────[...]
message(STATUS "")
message(STATUS "=== RGTP Build Configuration ===")
message(STATUS "  Version:           ${PROJECT_VERSION}")
message(STATUS "  Build Type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler:          ${CMAKE_C_COMPILER}")
message(STATUS "  Crypto Backend:    ${RGTP_CRYPTO_BACKEND}")
message(STATUS "  Enable FEC:        ${RGTP_ENABLE_FEC}")
message(STATUS "  Enable SIMD:       ${RGTP_ENABLE_SIMD}")
message(STATUS "  Build Tests:       ${RGTP_BUILD_TESTS}")
message(STATUS "  Build Examples:    ${RGTP_BUILD_EXAMPLES}")
message(STATUS "===================================")
message(STATUS "")
