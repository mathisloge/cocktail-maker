# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
# Copyright (C)
#
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-License-Identifier: MIT

option(
    SANITIZE_UNDEFINED
    "Enable UndefinedBehaviorSanitizer for sanitized targets."
    Off
)

set(FLAG_CANDIDATES
    # MSVC uses
    "/fsanitize=undefined"
    # This Clang candidate disables the function check. stdexec is included as headers into several modules and
    # translation units, and its sender descriptors are lambdas whose types differ between them. The function check
    # reports the resulting indirect calls within stdexec, although they are sound.
    "-g -fsanitize=undefined -fno-sanitize=function"
    # GNU/Clang
    "-g -fsanitize=undefined"
)

include(sanitize-helpers)

if(SANITIZE_UNDEFINED)
    sanitizer_check_compiler_flags("${FLAG_CANDIDATES}"
        "UndefinedBehaviorSanitizer" "UBSan"
    )
endif()

function(add_sanitize_undefined TARGET)
    if(NOT SANITIZE_UNDEFINED)
        return()
    endif()

    sanitizer_add_flags(${TARGET} "UndefinedBehaviorSanitizer" "UBSan")
endfunction()
