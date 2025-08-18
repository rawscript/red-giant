@echo off
REM Red Giant Protocol - Windows Build Script
REM Builds the C wrapper and test programs using GCC (MinGW)

echo Red Giant Protocol - Windows Build Script
echo ==========================================

REM Check if GCC is available
gcc --version >nul 2>&1
if errorlevel 1 (
    echo Error: GCC not found. Please install MinGW or TDM-GCC.
    echo Download from: https://www.mingw-w64.org/
    pause
    exit /b 1
)

REM Set compiler flags
set CFLAGS=-std=c99 -Wall -Wextra -O3 -DNDEBUG
set SOURCES=red_giant.c red_giant_reliable.c red_giant_wrapper.c

echo.
echo Building test program...
gcc %CFLAGS% -o test_wrapper.exe test_wrapper.c %SOURCES%
if errorlevel 1 (
    echo Error: Failed to build test program
    pause
    exit /b 1
)
echo ✅ test_wrapper.exe built successfully

echo.
echo Building example program...
gcc %CFLAGS% -o example_usage.exe example_usage.c %SOURCES%
if errorlevel 1 (
    echo Error: Failed to build example program
    pause
    exit /b 1
)
echo ✅ example_usage.exe built successfully

echo.
echo Building integration test...
gcc %CFLAGS% -o integration_test.exe integration_test.c %SOURCES%
if errorlevel 1 (
    echo Error: Failed to build integration test
    pause
    exit /b 1
)
echo ✅ integration_test.exe built successfully

echo.
echo 🎉 All programs built successfully!
echo.
echo Available programs:
echo   test_wrapper.exe      - Comprehensive test suite
echo   example_usage.exe     - Usage examples and demonstrations
echo   integration_test.exe  - Integration tests
echo.
echo To run tests: test_wrapper.exe
echo To run examples: example_usage.exe
echo To run integration tests: integration_test.exe
echo.
pause