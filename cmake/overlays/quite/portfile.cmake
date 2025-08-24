# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO mathisloge/quite
    REF 12d5c89411ad2ee768902d4890ebe142059f7c4f
    SHA512 4263f4c606ab75d8c984604c5a8d5af611ba72c516a10064db934876ef93b2db5d5f4bfd54224dd4fc53f6f6655141ed939054c58dcfdcaed9aeb2e02894878f
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DQUITE_BUILD_PROBES=ON
        -DBUILD_TESTING=OFF
        -DQUITE_EXAMPLES=OFF
        -DCPM_LOCAL_PACKAGES_ONLY=ON
        -DCPM_DONT_UPDATE_MODULE_PATH=ON
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/${PORT}")

# Handle copyright/readme/package files
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
file(
    INSTALL "${SOURCE_PATH}/README.md"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
