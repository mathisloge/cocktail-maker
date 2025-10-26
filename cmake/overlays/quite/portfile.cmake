# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO mathisloge/quite
    REF v1.4.1-dev.1
    SHA512 eb76a4b508b95be1a3f29a6a13e0ab21a300b7d51f89bdccc4c9293faf3f6121094a7f675fc1caf969581bf9d2e8d58e42c30be42c6e2607b806d60af9deea75
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DQUITE_BUILD_PROBE_QT=ON
        -DQUITE_BUILD_PYTHON_TEST_API=OFF
        -DQUITE_BUILD_TEST_API=ON
        -DQUITE_BUILD_REMOTE_MANAGER=OFF
        -DBUILD_TESTING=OFF
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
