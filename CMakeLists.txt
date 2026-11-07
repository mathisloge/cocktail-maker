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
option(
    CM_SLINT_MCP
    "Build Slint with its embedded MCP server, which starts when SLINT_MCP_PORT is set"
    OFF
)
option(
    CM_COMPILE_TIME_TRACE
    "Collect clang -ftime-trace data for the first-party targets"
    OFF
)

# Only the first-party targets get -ftime-trace. The instrumentation still wraps every compile command, so turning the
# option on or off rebuilds the dependencies once.
function(cm_enable_compile_time_trace target)
    if(CM_COMPILE_TIME_TRACE)
        target_compile_options(
            ${target}
            PRIVATE -ftime-trace -ftime-trace-granularity=500
        )
    endif()
endfunction()

if(CM_COMPILE_TIME_TRACE)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    # compileTrace only collects what -ftime-trace writes. postBuild wouldn't fire under cmake --build.
    cmake_instrumentation(
        API_VERSION 1
        DATA_VERSION 1.1
        HOOKS postCMakeBuild
        OPTIONS compileTrace trace dynamicSystemInformation
        CALLBACK ${Python3_EXECUTABLE}
        ${PROJECT_SOURCE_DIR}/tools/compile_time/summarize.py
    )
endif()

list(
    APPEND CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sanitizers"
)

include(CTest)
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
  GIT_TAG 15ef61ea39433abba50acaeb92b0eeeb885798aa
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

# mp-units checks its API contracts with gsl-lite, so nothing in this repository names it.
cpmaddpackage(NAME gsl-lite VERSION 1.1.0 GITHUB_REPOSITORY gsl-lite/gsl-lite)
cpmaddpackage(URI "gh:gabime/spdlog@1.17.0" OPTIONS "SPDLOG_USE_STD_FORMAT ON")
cpmaddpackage(URI "gh:CLIUtils/CLI11@2.7.2" OPTIONS "CLI11_PRECOMPILED ON")
cpmaddpackage(URI "gh:mpusz/mp-units#18d759722417168dc103ac951154bf0ad34f3c88" SOURCE_SUBDIR src OPTIONS "MP_UNITS_BUILD_CXX_MODULES ON"
)
# enable reflection as soon as gcc is available SIMDJSON_STATIC_REFLECTION ON
cpmaddpackage(URI "gh:simdjson/simdjson@5.1.1" OPTIONS "SIMDJSON_DISABLE_DEPRECATED_API OFF" "BUILD_SHARED_LIBS OFF")
cpmaddpackage(
  URI "gh:slint-ui/slint#8d51770e77db4a9e801427a47ee3f79487504975"
  NAME Slint
  SOURCE_SUBDIR api/cpp
  # The C++ testing API (ElementHandle) is only declared with the experimental feature.
  OPTIONS "SLINT_FEATURE_MCP ${CM_SLINT_MCP}" "SLINT_FEATURE_EXPERIMENTAL ON"
)
cpmaddpackage(URI "gh:commschamp/comms@5.5.3" NAME LibComms)
cpmaddpackage(URI "gh:mathisloge/cocktail-maker-protocol@2.1.0" SOURCE_SUBDIR generated
)
cpmaddpackage(URI "gh:jeremy-rifkin/libassert@2.2.1")
find_package(PkgConfig REQUIRED)
pkg_check_modules(libudev IMPORTED_TARGET libudev REQUIRED)

if(BUILD_TESTING)
    cpmaddpackage("gh:catchorg/Catch2@3.16.1")
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(Catch)
endif()

add_subdirectory(core)
if(BUILD_TESTING)
    add_subdirectory(testing)
endif()
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
