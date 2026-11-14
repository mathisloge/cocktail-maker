# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/read_project_version.cmake")

function(expect_version content expected)
    set(file
        "${CMAKE_CURRENT_BINARY_DIR}/read_project_version_test_pyproject.toml"
    )
    file(WRITE "${file}" "${content}")
    cm_read_project_version("${file}" actual)
    file(REMOVE "${file}")
    if(NOT actual STREQUAL expected)
        message(
            FATAL_ERROR
            "expected '${expected}' but got '${actual}' for:\n${content}"
        )
    endif()
endfunction()

expect_version("[project]\nname = \"x\"\nversion = \"1.0.0\"\n" "1.0.0")
expect_version("[project]\nversion = \"1.2.0a1\"\n" "1.2.0")
expect_version("[project]\nversion = \"1.2.0-rc.1\"\n" "1.2.0")
expect_version("[project]\nversion = \"2.3.4\"\n\n[tool.x]\nversion = \"9.9.9\"\n" "2.3.4")
expect_version("[tool.a]\nversion = \"8.8.8\"\n[project]\nversion = \"3.1.0\"\n" "3.1.0")
