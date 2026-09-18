# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 4.4)
file(READ "${CMAKE_SOURCE_DIR}/package.json" PACKAGE_JSON)
string(JSON PACKAGE_JSON_PROJECT_VERSION GET "${PACKAGE_JSON}" version)
string(
    REGEX REPLACE "^([0-9]+\\.[0-9]+\\.[0-9]+)-dev\\.([0-9]+)"
    "\\1.\\2"
    BASE_VERSION
    "${PACKAGE_JSON_PROJECT_VERSION}"
)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")

project(cocktail-maker VERSION ${BASE_VERSION} LANGUAGES CXX)
cmake_policy(SET CMP0167 NEW)
message(STATUS "Project version: ${PROJECT_VERSION}")

option(BUILD_SIMULATED "Use simulated pods" OFF)

list(
    APPEND CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sanitizers"
)

include(CTest)
include(GenerateExportHeader)
include(FeatureSummary)
include(GNUInstallDirs)
include(FindPkgConfig)
include(GetGitRevisionDescription)
include(CPM)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/runtime")

get_git_head_revision(git_branch git_sha)
message(STATUS "Project git sha: ${git_sha}")
message(STATUS "Project git branch: ${git_branch}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # llvm import-std workaround
    add_compile_options(-fno-aligned-allocation)
endif()

find_package(Sanitizers REQUIRED)
set(BOOST_VERSION "1.92.0")
set(BOOST_INCLUDE_LIBRARIES "asio\\\\;hash2")
cpmaddpackage(
  NAME Boost
  VERSION "${BOOST_VERSION}"
  URL https://github.com/boostorg/boost/releases/download/boost-${BOOST_VERSION}/boost-${BOOST_VERSION}-cmake.tar.xz
  URL_HASH SHA256=9bed76128d4e46755dbe818487788c6fceb6f72b378f4daa49b7e1e600d9088d
  EXCLUDE_FROM_ALL ON
  SYSTEM ON
  FIND_PACKAGE_ARGUMENTS "CONFIG"
  OPTIONS
    "BOOST_ENABLE_CMAKE ON"
    "BOOST_INCLUDE_LIBRARIES ${BOOST_INCLUDE_LIBRARIES}"
)
# stdexec has to be added after Boost, because its asio integration reuses the Boost package added above.
cpmaddpackage(
  NAME stdexec
  GITHUB_REPOSITORY NVIDIA/stdexec
  GIT_TAG 3de97310d26d4abf6af4cb65af2574838030aac9
  SYSTEM ON
  OPTIONS
    "STDEXEC_BUILD_TESTS OFF"
    "STDEXEC_BUILD_EXAMPLES OFF"
    "STDEXEC_BUILD_DOCS OFF"
    "STDEXEC_INSTALL OFF"
    "STDEXEC_BUILD_MODULES OFF"
    "STDEXEC_ENABLE_ASIO ON"
    "STDEXEC_ASIO_IMPLEMENTATION boost"
)

cpmaddpackage(NAME gsl-lite VERSION 1.1.0 GITHUB_REPOSITORY gsl-lite/gsl-lite)
cpmaddpackage(URI "gh:gabime/spdlog@1.17.0" OPTIONS "SPDLOG_USE_STD_FORMAT ON")
cpmaddpackage(URI "gh:CLIUtils/CLI11@2.7.2" OPTIONS "CLI11_PRECOMPILED ON")
cpmaddpackage(URI "gh:mpusz/mp-units#daaed987fa55419d0d5daffbad95e4463f2e3f74" SOURCE_SUBDIR src OPTIONS "MP_UNITS_BUILD_CXX_MODULES ON"
)
# enable reflection as soon as gcc is available SIMDJSON_STATIC_REFLECTION ON
cpmaddpackage(URI "gh:simdjson/simdjson@4.6.11" OPTIONS "SIMDJSON_DISABLE_DEPRECATED_API OFF" "BUILD_SHARED_LIBS OFF")
cpmaddpackage(
  URI "gh:slint-ui/slint@1.18.0"
  NAME Slint
  SOURCE_SUBDIR api/cpp
)
cpmaddpackage(URI "gh:commschamp/comms@5.5.3" NAME LibComms)
cpmaddpackage(URI "gh:mathisloge/cocktail-maker-protocol@1.0.0" SOURCE_SUBDIR generated
)
cpmaddpackage(URI "gh:jeremy-rifkin/libassert@2.2.1")
find_package(PkgConfig REQUIRED)
pkg_check_modules(libudev IMPORTED_TARGET libudev REQUIRED)

if(BUILD_TESTING)
    cpmaddpackage("gh:catchorg/Catch2@3.16.0")
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(CTest)
    include(Catch)
endif()

add_subdirectory(core)
add_subdirectory(cocktail-maker)
add_subdirectory(gui)
add_subdirectory(hw_simulator)

install(
    FILES cmake/cocktail-maker.service
    DESTINATION lib/systemd/system
    RENAME cocktail-maker.service
)

set(CPACK_PACKAGE_NAME "cocktail-maker")
set(CPACK_PACKAGE_CONTACT "Mathis Logemann <mathis@quite.rocks>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
include(CPack)
