# RgtpAnalysis.cmake
# Optional static analysis targets for RGTP sources.
# Adds an `analyze` target that runs cppcheck and clang-tidy when available.
# Gracefully skips any tool that is not found on the host system.

cmake_minimum_required(VERSION 3.20)

# Collect all RGTP source and header files for analysis
get_target_property(_rgtp_sources rgtp_static SOURCES)
get_target_property(_rgtp_include_dirs rgtp_static INCLUDE_DIRECTORIES)

# Resolve generator expressions in include dirs for use in command lines
set(_rgtp_include_flags "")
foreach(_dir IN LISTS _rgtp_include_dirs)
    # Strip generator expressions — only use plain paths
    if(NOT _dir MATCHES "\\$<")
        list(APPEND _rgtp_include_flags "-I${_dir}")
    endif()
endforeach()

# Always add the primary include directory
list(APPEND _rgtp_include_flags "-I${CMAKE_CURRENT_SOURCE_DIR}/include")

# ── cppcheck ────────────────────────────────────────────────────────────────
find_program(CPPCHECK_EXECUTABLE NAMES cppcheck)

if(CPPCHECK_EXECUTABLE)
    message(STATUS "cppcheck found: ${CPPCHECK_EXECUTABLE}")
    set(_cppcheck_args
        --enable=all
        --std=c17
        --suppress=missingIncludeSystem
        --suppress=unusedFunction
        --error-exitcode=1
        --inline-suppr
        --quiet
        ${_rgtp_include_flags}
        ${_rgtp_sources}
    )
    add_custom_target(analyze_cppcheck
        COMMAND ${CPPCHECK_EXECUTABLE} ${_cppcheck_args}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running cppcheck on RGTP sources"
        VERBATIM
    )
else()
    message(STATUS "cppcheck not found — cppcheck analysis target skipped")
    add_custom_target(analyze_cppcheck
        COMMAND ${CMAKE_COMMAND} -E echo "cppcheck not found; skipping"
        COMMENT "cppcheck not available"
    )
endif()

# ── clang-tidy ──────────────────────────────────────────────────────────────
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy clang-tidy-17 clang-tidy-16 clang-tidy-15)

if(CLANG_TIDY_EXECUTABLE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXECUTABLE}")

    # Build the list of source files with full paths
    set(_tidy_sources "")
    foreach(_src IN LISTS _rgtp_sources)
        if(IS_ABSOLUTE "${_src}")
            list(APPEND _tidy_sources "${_src}")
        else()
            list(APPEND _tidy_sources "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        endif()
    endforeach()

    set(_clang_tidy_args
        --warnings-as-errors=*
        --extra-arg=-std=c17
        ${_tidy_sources}
        --
        ${_rgtp_include_flags}
    )

    # Use compile_commands.json if available
    if(EXISTS "${CMAKE_BINARY_DIR}/compile_commands.json")
        list(PREPEND _clang_tidy_args -p "${CMAKE_BINARY_DIR}")
    endif()

    add_custom_target(analyze_clang_tidy
        COMMAND ${CLANG_TIDY_EXECUTABLE} ${_clang_tidy_args}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-tidy on RGTP sources"
        VERBATIM
    )
else()
    message(STATUS "clang-tidy not found — clang-tidy analysis target skipped")
    add_custom_target(analyze_clang_tidy
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found; skipping"
        COMMENT "clang-tidy not available"
    )
endif()

# ── Combined `analyze` target ────────────────────────────────────────────────
add_custom_target(analyze
    DEPENDS analyze_cppcheck analyze_clang_tidy
    COMMENT "Running all static analysis tools on RGTP sources"
)
