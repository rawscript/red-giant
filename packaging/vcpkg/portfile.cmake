# vcpkg portfile for RGTP (Red Giant Transport Protocol)
#
# Usage:
#   vcpkg install rgtp
#   find_package(rgtp CONFIG REQUIRED)
#   target_link_libraries(my_app PRIVATE rgtp::rgtp)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO your-org/red-giant
    REF "v${VERSION}"
    SHA512 0  # Replace with actual SHA512 after first release
    HEAD_REF main
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        fec     RGTP_ENABLE_FEC
        simd    RGTP_ENABLE_SIMD
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DRGTP_CRYPTO_BACKEND=libsodium
        -DRGTP_BUILD_TESTS=OFF
        -DRGTP_BUILD_EXAMPLES=OFF
        -DRGTP_BUILD_BINDINGS=OFF
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME rgtp CONFIG_PATH lib/cmake/rgtp)
vcpkg_fixup_pkgconfig()

# Remove debug include directory (headers are the same)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

# Install license
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

# Usage instructions
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
