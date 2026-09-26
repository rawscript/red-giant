"""
setup.py — Build script for RGTP Python bindings.

This script builds the C library (librgtp) and bundles it with the Python
package, ensuring that `pip install rgtp` works out of the box without
requiring users to manually install the C library.

Usage:
    pip install .                    # Build and install
    python setup.py build            # Build the library
    python setup.py bdist_wheel      # Build wheel with bundled library
"""

import os
import sys
import shutil
import subprocess
import platform
from pathlib import Path
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
from setuptools.command.sdist import sdist
from setuptools.command.bdist_wheel import bdist_wheel


# ── Platform detection ──────────────────────────────────────────────────────

def get_library_name():
    """Return platform-specific library name."""
    system = platform.system()
    if system == "Windows":
        return "rgtp.dll"
    elif system == "Darwin":
        return "librgtp.dylib"
    else:
        return "librgtp.so"


def get_library_path():
    """Return the path where the bundled library should be placed."""
    return Path("rgtp") / get_library_name()


# ── Custom build command ────────────────────────────────────────────────────

class BuildLibraryCommand(build_ext):
    """Build the C library before building Python extensions."""
    
    def run(self):
        # Try to build the C library
        self.build_c_library()
        # Continue with normal extension build
        super().run()
    
    def build_c_library(self):
        """Build librgtp using CMake."""
        # Find the root directory (3 levels up from this file)
        setup_dir = Path(__file__).parent
        root_dir = setup_dir.parent.parent
        
        # Check if library already exists in the package
        lib_dest = setup_dir / "rgtp" / get_library_name()
        
        # Check environment variable for pre-built library
        env_lib_path = os.environ.get("RGTP_LIB_PATH")
        if env_lib_path and Path(env_lib_path).exists():
            print(f"[setup] Using pre-built library from RGTP_LIB_PATH: {env_lib_path}")
            shutil.copy(env_lib_path, lib_dest)
            return
        
        # Check if library already bundled
        if lib_dest.exists():
            print(f"[setup] Library already bundled: {lib_dest}")
            return
        
        # Try to find system-installed library
        system_lib = self.find_system_library()
        if system_lib:
            print(f"[setup] Found system library: {system_lib}")
            shutil.copy(system_lib, lib_dest)
            return
        
        # Try to build from source
        print("[setup] Building librgtp from source...")
        self.build_from_source(root_dir, lib_dest)
    
    def find_system_library(self):
        """Try to find an installed librgtp on the system."""
        lib_name = get_library_name()
        
        # Common library paths
        search_paths = [
            f"/usr/local/lib/{lib_name}",
            f"/usr/lib/{lib_name}",
            f"/usr/lib/x86_64-linux-gnu/{lib_name}",
            f"/opt/homebrew/lib/{lib_name}",
            f"C:\\Windows\\System32\\{lib_name}",
        ]
        
        for path in search_paths:
            if Path(path).exists():
                return path
        
        return None
    
    def build_from_source(self, root_dir: Path, lib_dest: Path):
        """Build librgtp using CMake."""
        build_dir = root_dir / "build"
        
        # Create build directory
        build_dir.mkdir(exist_ok=True)
        
        # Detect generator
        if platform.system() == "Windows":
            # Try Visual Studio first, then MinGW
            generators = ["Visual Studio 17 2022", "MinGW Makefiles", "Ninja"]
        else:
            generators = ["Unix Makefiles", "Ninja"]
        
        cmake_success = False
        last_error = None
        
        for generator in generators:
            try:
                print(f"[setup] Trying CMake generator: {generator}")
                
                # Configure
                configure_cmd = [
                    "cmake",
                    "-G", generator,
                    f"-DRGTP_BUILD_TESTS=OFF",
                    f"-DRGTP_BUILD_EXAMPLES=OFF",
                    f"-DRGTP_BUILD_BINDINGS=OFF",
                    ".."
                ]
                
                result = subprocess.run(
                    configure_cmd,
                    cwd=build_dir,
                    capture_output=True,
                    text=True,
                    timeout=300
                )
                
                if result.returncode != 0:
                    print(f"[setup] CMake configure failed: {result.stderr}")
                    continue
                
                # Build
                build_cmd = ["cmake", "--build", ".", "--config", "Release"]
                
                result = subprocess.run(
                    build_cmd,
                    cwd=build_dir,
                    capture_output=True,
                    text=True,
                    timeout=600
                )
                
                if result.returncode != 0:
                    print(f"[setup] CMake build failed: {result.stderr}")
                    continue
                
                cmake_success = True
                break
                
            except (subprocess.TimeoutExpired, FileNotFoundError) as e:
                last_error = e
                continue
        
        if not cmake_success:
            print("[setup] Warning: Could not build librgtp from source.")
            print("[setup] The Python package will be installed, but you must")
            print("[setup] manually install librgtp and set RGTP_LIB_PATH.")
            if last_error:
                print(f"[setup] Error: {last_error}")
            return
        
        # Find and copy the built library
        lib_name = get_library_name()
        built_lib = None
        
        # Check common build output locations
        possible_paths = [
            build_dir / lib_name,
            build_dir / "Release" / lib_name,
            build_dir / "Debug" / lib_name,
        ]
        
        for path in possible_paths:
            if path.exists():
                built_lib = path
                break
        
        if built_lib:
            print(f"[setup] Copying built library: {built_lib} -> {lib_dest}")
            shutil.copy(built_lib, lib_dest)
        else:
            print("[setup] Warning: Built library not found in expected locations")
            print(f"[setup] Expected one of: {possible_paths}")


# ── Custom sdist command to include library source ──────────────────────────

class CustomSdist(sdist):
    """Include C library source in source distribution."""
    
    def run(self):
        # Ensure C library source is included
        root_dir = Path(__file__).parent.parent.parent
        
        # Run standard sdist
        super().run()


# ── Custom wheel command ────────────────────────────────────────────────────

class CustomBdistWheel(bdist_wheel):
    """Build wheel with bundled library."""
    
    def run(self):
        # Ensure library is built before creating wheel
        self.run_command("build_ext")
        super().run()


# ── Setup configuration ──────────────────────────────────────────────────────

setup(
    name="rgtp",
    version="1.2.0",
    cmdclass={
        "build_ext": BuildLibraryCommand,
        "sdist": CustomSdist,
        "bdist_wheel": CustomBdistWheel,
    },
    # No C extensions needed - we use ctypes
    ext_modules=[],
    zip_safe=False,  # Required for bundled library
)
