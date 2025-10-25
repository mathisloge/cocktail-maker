# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
file(READ "${CMAKE_SOURCE_DIR}/package.json" PACKAGE_JSON)
string(JSON PACKAGE_JSON_PROJECT_VERSION GET "${PACKAGE_JSON}" version)
string(
    REGEX REPLACE
    "^([0-9]+\\.[0-9]+\\.[0-9]+)-dev\\.([0-9]+)"
    "\\1.\\2"
    BASE_VERSION
    "${PACKAGE_JSON_PROJECT_VERSION}"
)

project(cocktail-maker VERSION ${BASE_VERSION} LANGUAGES CXX)
#cmake_policy(SET CMP0167 NEW)
message(STATUS "Project version: ${PROJECT_VERSION}")

list(
    APPEND
    CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sanitizers"
)

include(CTest)
include(GenerateExportHeader)
include(FeatureSummary)
include(GNUInstallDirs)
include(FindPkgConfig)
include(GetGitRevisionDescription)

get_git_head_revision(git_branch git_sha)
message(STATUS "git_sha: ${git_sha}")
message(STATUS "git_branch: ${git_branch}")

find_package(Sanitizers REQUIRED)
find_package(Boost REQUIRED COMPONENTS asio signals2)
find_package(mp-units CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(libassert CONFIG REQUIRED)
pkg_check_modules(libgpiod REQUIRED IMPORTED_TARGET libgpiod)
pkg_check_modules(libgpiodcxx REQUIRED IMPORTED_TARGET libgpiodcxx)

find_package(
    Qt6
    COMPONENTS Core Widgets Gui Qml Quick QuickControls2 LinguistTools
    REQUIRED
)
qt_standard_project_setup(I18N_SOURCE_LANGUAGE de I18N_TRANSLATED_LANGUAGES en)
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0003 NEW)
qt_policy(SET QTP0004 NEW)
qt_policy(SET QTP0005 NEW)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/runtime")

if(BUILD_TESTING)
    find_package(Catch2 3 REQUIRED)
    include(Catch)
endif()

add_subdirectory(src)
add_subdirectory(utils)
add_subdirectory(ui)
add_subdirectory(i18n)

set(CPACK_PACKAGE_NAME "cocktail-maker")
set(CPACK_PACKAGE_CONTACT "Mathis Logemann <mathis@quite.rocks>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libqt6widgets6 (>= 6.8), libqt6quick6 (>= 6.8)"
)
include(CPack)
