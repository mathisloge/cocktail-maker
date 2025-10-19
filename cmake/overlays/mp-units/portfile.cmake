# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later

if(VCPKG_TARGET_IS_LINUX)
    message("Note: `mp-units` requires Clang16+ or GCC11+")
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO mpusz/mp-units
    REF 6bb399b832640bbfa2c1edd02d63024ed8635724
    SHA512 ae2ff7891294d9203fea6e6b3c2fbf550ee2d896d65432fd753caba7501cbeb7500a3ed675f21b33f12b7c772bd48677d1c343b7b2293c5d34752028561e1edb
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
