# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 4.3)
file(READ "${CMAKE_SOURCE_DIR}/package.json" PACKAGE_JSON)
string(JSON PACKAGE_JSON_PROJECT_VERSION GET "${PACKAGE_JSON}" version)
string(
    REGEX REPLACE "^([0-9]+\\.[0-9]+\\.[0-9]+)-dev\\.([0-9]+)"
    "\\1.\\2"
    BASE_VERSION
    "${PACKAGE_JSON_PROJECT_VERSION}"
)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
set(CMAKE_CXX_MODULE_STD ON)

project(cocktail-maker VERSION ${BASE_VERSION} LANGUAGES CXX)
cmake_policy(SET CMP0167 NEW)
message(STATUS "Project version: ${PROJECT_VERSION}")

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
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/runtime")

get_git_head_revision(git_branch git_sha)
message(STATUS "Project git sha: ${git_sha}")
message(STATUS "Project git branch: ${git_branch}")

find_package(Sanitizers REQUIRED)
set(BOOST_VERSION "1.91.0-1")
set(BOOST_INCLUDE_LIBRARIES "cobalt;asio")
cpmaddpackage(
  NAME Boost
  VERSION "${BOOST_VERSION}"
  URL https://github.com/boostorg/boost/releases/download/boost-${BOOST_VERSION}/boost-${BOOST_VERSION}-cmake.tar.xz
  URL_HASH SHA256=cc5dc5006ecbdf0051f90979be31b4eee5987d9ae14ae9fb9c03cfa43fa3cdad
  EXCLUDE_FROM_ALL ON
  SYSTEM ON
  FIND_PACKAGE_ARGUMENTS "CONFIG"
  OPTIONS
    "BOOST_ENABLE_CMAKE ON"
    "BOOST_INCLUDE_LIBRARIES ${BOOST_INCLUDE_LIBRARIES}"
)
cpmaddpackage(NAME gsl-lite VERSION 1.1.0 GITHUB_REPOSITORY gsl-lite/gsl-lite )
cpmaddpackage(URI "gh:fmtlib/fmt#12.1.0" OPTIONS "FMT_MODULE ON")
# Prevent linking errors while not using spdlog as module
target_compile_definitions(fmt PUBLIC FMT_ATTACH_TO_GLOBAL_MODULE)

cpmaddpackage(URI "gh:gabime/spdlog@1.17.0" OPTIONS "SPDLOG_FMT_EXTERNAL ON")
target_link_libraries(spdlog PUBLIC fmt::fmt)
cpmaddpackage(URI "gh:CLIUtils/CLI11@2.6.2" OPTIONS "CLI11_PRECOMPILED ON")
cpmaddpackage(URI "gh:mpusz/mp-units@2.5.0" SOURCE_SUBDIR src OPTIONS "MP_UNITS_BUILD_CXX_MODULES ON")
# enable reflection as soon as gcc is available SIMDJSON_STATIC_REFLECTION ON
cpmaddpackage(URI "gh:simdjson/simdjson@4.6.0" OPTIONS "SIMDJSON_DISABLE_DEPRECATED_API OFF")
cpmaddpackage(
  NAME Slint
  GIT_REPOSITORY https://github.com/slint-ui/slint.git
  GIT_TAG 6c564a6c043f883bacc4fd65faff3c0aaf4ca694
  SOURCE_SUBDIR api/cpp
)

if(BUILD_TESTING)
    cpmaddpackage("gh:catchorg/Catch2@3.15.0")
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(CTest)
    include(Catch)
endif()

add_subdirectory(cocktail-maker)
add_subdirectory(gui)

#add_subdirectory(src)
#add_subdirectory(utils)
#add_subdirectory(ui)
#add_subdirectory(i18n)

install(
    FILES cmake/cocktail-maker.service
    DESTINATION lib/systemd/system
    RENAME cocktail-maker.service
)

set(CPACK_PACKAGE_NAME "cocktail-maker")
set(CPACK_PACKAGE_CONTACT "Mathis Logemann <mathis@quite.rocks>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
include(CPack)
