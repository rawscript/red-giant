"""
Conan package recipe for RGTP (Red Giant Transport Protocol).

Usage:
    conan create . --version 1.0.0
    conan install rgtp/1.0.0@ -g cmake_find_package
"""

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy
import os


class RgtpConan(ConanFile):
    name = "rgtp"
    version = "1.0.0"
    description = "Red Giant Transport Protocol — stateless, receiver-driven, chunk-based transport"
    license = "MIT"
    url = "https://github.com/rawscript/red-giant"
    homepage = "https://github.com/rawscript/red-giant"
    topics = ("networking", "transport", "protocol", "autonomous-vehicles", "udp")

    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared":          [True, False],
        "fPIC":            [True, False],
        "crypto_backend":  ["libsodium", "openssl"],
        "enable_fec":      [True, False],
        "enable_simd":     [True, False],
    }
    default_options = {
        "shared":          False,
        "fPIC":            True,
        "crypto_backend":  "libsodium",
        "enable_fec":      True,
        "enable_simd":     True,
    }

    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "include/*",
        "src/*",
    )

    def requirements(self):
        if self.options.crypto_backend == "libsodium":
            self.requires("libsodium/1.0.19")
        else:
            self.requires("openssl/3.2.0")

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["RGTP_CRYPTO_BACKEND"] = str(self.options.crypto_backend)
        tc.variables["RGTP_ENABLE_FEC"]     = "ON" if self.options.enable_fec else "OFF"
        tc.variables["RGTP_ENABLE_SIMD"]    = "ON" if self.options.enable_simd else "OFF"
        tc.variables["RGTP_BUILD_TESTS"]    = "OFF"
        tc.variables["RGTP_BUILD_EXAMPLES"] = "OFF"
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE", self.source_folder,
             os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.libs = ["rgtp"]
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_target_name", "rgtp::rgtp")
        self.cpp_info.set_property("pkg_config_name", "rgtp")

        if self.settings.os == "Windows":
            self.cpp_info.system_libs = ["ws2_32", "iphlpapi"]
        elif self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs = ["pthread", "m"]
