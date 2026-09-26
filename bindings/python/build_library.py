#!/usr/bin/env python3
"""
build_library.py — Build librgtp for bundling with Python package.

This script builds the shared library for the current platform and places
it in the rgtp package directory for bundling.

Usage:
    python build_library.py [--platform PLATFORM] [--config CONFIG]

Examples:
    python build_library.py                     # Build for current platform
    python build_library.py --platform linux   # Build for Linux
    python build_library.py --platform windows --config Release
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path


def get_platform_info():
    """Get platform-specific build information."""
    system = platform.system()
    machine = platform.machine()
    
    if system == "Windows":
        return {
            "system": "windows",
            "lib_name": "rgtp.dll",
            "generator": "Visual Studio 17 2022",
            "extra_cmake_args": ["-A", "x64" if machine == "AMD64" else "Win32"],
        }
    elif system == "Darwin":
        return {
            "system": "macos",
            "lib_name": "librgtp.dylib",
            "generator": "Xcode" if shutil.which("xcodebuild") else "Unix Makefiles",
            "extra_cmake_args": [],
        }
    else:  # Linux and others
        return {
            "system": "linux",
            "lib_name": "librgtp.so",
            "generator": "Unix Makefiles",
            "extra_cmake_args": [],
        }


def find_cmake():
    """Find CMake executable."""
    cmake = shutil.which("cmake")
    if not cmake:
        print("ERROR: CMake not found. Please install CMake.")
        print("  Windows: choco install cmake")
        print("  macOS: brew install cmake")
        print("  Linux: sudo apt-get install cmake")
        sys.exit(1)
    return cmake


def build_library(build_dir: Path, install_dir: Path, config: str = "Release", 
                  platform_info: dict = None) -> bool:
    """Build librgtp using CMake."""
    
    if platform_info is None:
        platform_info = get_platform_info()
    
    cmake = find_cmake()
    root_dir = build_dir.parent.parent.parent  # Go up from bindings/python/build
    
    # Create build directory
    build_dir.mkdir(parents=True, exist_ok=True)
    
    print(f"[build] Building librgtp for {platform_info['system']}")
    print(f"[build] Build directory: {build_dir}")
    print(f"[build] Install directory: {install_dir}")
    
    # CMake configure
    configure_args = [
        cmake,
        "-G", platform_info["generator"],
        f"-DCMAKE_BUILD_TYPE={config}",
        f"-DCMAKE_INSTALL_PREFIX={install_dir}",
        "-DRGTP_BUILD_TESTS=OFF",
        "-DRGTP_BUILD_EXAMPLES=OFF",
        "-DRGTP_BUILD_BINDINGS=OFF",
        "-DRGTP_ENABLE_SATELLITE=ON",
    ] + platform_info.get("extra_cmake_args", [])
    
    # Add source directory
    configure_args.append(str(root_dir))
    
    print(f"[build] Configuring with: {' '.join(configure_args)}")
    
    result = subprocess.run(
        configure_args,
        cwd=build_dir,
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0:
        print(f"[build] CMake configure failed:")
        print(result.stderr)
        return False
    
    # CMake build
    build_args = [
        cmake,
        "--build", ".",
        "--config", config,
        "--parallel", str(os.cpu_count() or 4),
    ]
    
    print(f"[build] Building...")
    
    result = subprocess.run(
        build_args,
        cwd=build_dir,
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0:
        print(f"[build] CMake build failed:")
        print(result.stderr)
        return False
    
    # Find and copy the library
    lib_name = platform_info["lib_name"]
    lib_paths = [
        build_dir / lib_name,
        build_dir / config / lib_name,  # Windows multi-config
        build_dir / "Release" / lib_name,
        build_dir / "Debug" / lib_name,
    ]
    
    for lib_path in lib_paths:
        if lib_path.exists():
            dest = install_dir / lib_name
            print(f"[build] Copying library: {lib_path} -> {dest}")
            shutil.copy(lib_path, dest)
            return True
    
    print(f"[build] ERROR: Built library not found")
    print(f"[build] Searched paths:")
    for p in lib_paths:
        print(f"  {p}")
    
    return False


def main():
    parser = argparse.ArgumentParser(
        description="Build librgtp for bundling with Python package"
    )
    parser.add_argument(
        "--platform", "-p",
        choices=["windows", "linux", "macos", "current"],
        default="current",
        help="Target platform (default: current)"
    )
    parser.add_argument(
        "--config", "-c",
        choices=["Release", "Debug", "RelWithDebInfo"],
        default="Release",
        help="Build configuration (default: Release)"
    )
    parser.add_argument(
        "--build-dir", "-b",
        type=Path,
        default=None,
        help="Build directory (default: build/<platform>)"
    )
    parser.add_argument(
        "--install-dir", "-i",
        type=Path,
        default=None,
        help="Install directory (default: rgtp/)"
    )
    
    args = parser.parse_args()
    
    # Get platform info
    if args.platform == "current":
        platform_info = get_platform_info()
    else:
        # Manual platform specification
        if args.platform == "windows":
            platform_info = {
                "system": "windows",
                "lib_name": "rgtp.dll",
                "generator": "Visual Studio 17 2022",
                "extra_cmake_args": ["-A", "x64"],
            }
        elif args.platform == "macos":
            platform_info = {
                "system": "macos",
                "lib_name": "librgtp.dylib",
                "generator": "Unix Makefiles",
                "extra_cmake_args": [],
            }
        else:  # linux
            platform_info = {
                "system": "linux",
                "lib_name": "librgtp.so",
                "generator": "Unix Makefiles",
                "extra_cmake_args": [],
            }
    
    # Set directories
    script_dir = Path(__file__).parent
    if args.build_dir is None:
        args.build_dir = script_dir / "build" / platform_info["system"]
    if args.install_dir is None:
        args.install_dir = script_dir / "rgtp"
    
    # Build
    success = build_library(
        build_dir=args.build_dir,
        install_dir=args.install_dir,
        config=args.config,
        platform_info=platform_info
    )
    
    if success:
        print(f"[build] SUCCESS: Library built and installed to {args.install_dir}")
        return 0
    else:
        print(f"[build] FAILED: Could not build library")
        return 1


if __name__ == "__main__":
    sys.exit(main())
