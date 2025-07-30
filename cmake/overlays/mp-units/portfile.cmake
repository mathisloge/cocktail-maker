if(VCPKG_TARGET_IS_LINUX)
    message("Note: `mp-units` requires Clang16+ or GCC11+")
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO mpusz/mp-units
    REF d9dce46c21f6d915424112fd8b5e2163addc051b
    SHA512 8c2b8a64293fcc8c314ee8305a1f2b75ec0b1051cb0f13a2195a59946dbe6f393303579ea352fac17ec359aad86cb6c310eb076964e4a5f3c62ec69202508e0c
    PATCHES
      config.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}/src"
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/${PORT}")

# Handle copyright/readme/package files
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.md")
file(
    INSTALL "${SOURCE_PATH}/README.md"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
)

file(
    REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug"
    "${CURRENT_PACKAGES_DIR}/lib"
) # Header only
