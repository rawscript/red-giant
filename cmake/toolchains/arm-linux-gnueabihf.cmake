# arm-linux-gnueabihf.cmake — CMake toolchain for ARM32 Linux hard-float cross-compilation
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-linux-gnueabihf.cmake ..

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Toolchain prefix
set(CROSS_COMPILE arm-linux-gnueabihf-)

set(CMAKE_C_COMPILER   ${CROSS_COMPILE}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++)
set(CMAKE_AR           ${CROSS_COMPILE}ar)
set(CMAKE_RANLIB       ${CROSS_COMPILE}ranlib)
set(CMAKE_STRIP        ${CROSS_COMPILE}strip)

# Sysroot (optional — set if you have a dedicated sysroot)
# set(CMAKE_SYSROOT /path/to/arm-sysroot)

# Search paths — only look in the cross-compile sysroot, not the host
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ABI flags — ARMv7-A with hard-float VFPv3-D16 (Cortex-A class)
set(CMAKE_C_FLAGS_INIT   "-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard")
