# RgtpAnalysis.cmake — optional static analysis targets (cppcheck, clang-tidy)

# Collect all RGTP source files for analysis
file(GLOB_RECURSE RGTP_ALL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

# ── cppcheck ────────────────────────────────────────────────────────────────
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK_EXECUTABLE}
            --enable=all
            --std=c17
            --suppress=missingIncludeSystem
            --suppress=unusedFunction
            --error-exitcode=1
            --inline-suppr
            -I "${CMAKE_CURRENT_SOURCE_DIR}/include"
            -I "${CMAKE_CURRENT_SOURCE_DIR}/src"
            ${RGTP_ALL_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running cppcheck static analysis"
        VERBATIM
    )
    message(STATUS "cppcheck found: ${CPPCHECK_EXECUTABLE}")
else()
    add_custom_target(cppcheck
        COMMAND ${CMAKE_COMMAND} -E echo "cppcheck not found; skipping"
        COMMENT "cppcheck not available"
    )
    message(STATUS "cppcheck not found; 'cppcheck' target will be a no-op")
endif()

# ── clang-tidy ──────────────────────────────────────────────────────────────
find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
if(CLANG_TIDY_EXECUTABLE AND EXISTS "${CMAKE_BINARY_DIR}/compile_commands.json")
    add_custom_target(clang-tidy
        COMMAND ${CLANG_TIDY_EXECUTABLE}
            -p "${CMAKE_BINARY_DIR}"
            --warnings-as-errors=*
            ${RGTP_ALL_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-tidy static analysis"
        VERBATIM
    )
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXECUTABLE}")
else()
    add_custom_target(clang-tidy
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found or compile_commands.json missing; skipping"
        COMMENT "clang-tidy not available"
    )
    message(STATUS "clang-tidy not found; 'clang-tidy' target will be a no-op")
endif()

# ── Combined analyze target ─────────────────────────────────────────────────
add_custom_target(analyze
    DEPENDS cppcheck clang-tidy
    COMMENT "Running all static analysis tools"
)
