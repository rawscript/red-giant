# aarch64-linux-gnu.cmake — CMake toolchain for ARM64 Linux cross-compilation
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake ..

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Toolchain prefix
set(CROSS_COMPILE aarch64-linux-gnu-)

set(CMAKE_C_COMPILER   ${CROSS_COMPILE}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++)
set(CMAKE_AR           ${CROSS_COMPILE}ar)
set(CMAKE_RANLIB       ${CROSS_COMPILE}ranlib)
set(CMAKE_STRIP        ${CROSS_COMPILE}strip)

# Sysroot (optional — set if you have a dedicated sysroot)
# set(CMAKE_SYSROOT /path/to/aarch64-sysroot)

# Search paths — only look in the cross-compile sysroot, not the host
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ABI flags
set(CMAKE_C_FLAGS_INIT   "-march=armv8-a")
set(CMAKE_CXX_FLAGS_INIT "-march=armv8-a")
