# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later

# Sets <out_var> to the leading X.Y.Z of the first `version = "..."` line in the
# [project] table of <pyproject_path>.
function(cm_read_project_version pyproject_path out_var)
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS "${pyproject_path}"
    )
    file(STRINGS "${pyproject_path}" lines)
    set(in_project FALSE)
    foreach(line IN LISTS lines)
        if(line MATCHES "^[ \t]*\\[")
            if(line MATCHES "^[ \t]*\\[project\\][ \t]*(#.*)?$")
                set(in_project TRUE)
            else()
                set(in_project FALSE)
            endif()
        elseif(
            in_project
            AND line
                MATCHES
                "^[ \t]*version[ \t]*=[ \t]*\"([0-9]+\\.[0-9]+\\.[0-9]+)"
        )
            set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(
        FATAL_ERROR
        "No version found in the [project] table of ${pyproject_path}"
    )
endfunction()
