# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later

FROM mcr.microsoft.com/devcontainers/base:noble

# CMakeLists.txt pins us below 4.4.x until
# https://gitlab.kitware.com/cmake/cmake/-/work_items/27930 is fixed.
ARG CMAKE_VERSION=4.3.4

# llvm.sh prerequisites (lsb-release/wget/gnupg/add-apt-repository), the build
# tooling, and the runtime deps Slint needs — same set the Linux CI installs.
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl wget gnupg lsb-release software-properties-common \
        git ninja-build pkg-config mold xvfb \
        libudev-dev libx11-xcb-dev libwayland-dev libfontconfig1-dev \
        libxkbcommon-x11-0 libgl1 libegl1 libinput10 \
    && rm -rf /var/lib/apt/lists/*

# Latest stable LLVM. Passing only "all" (no version) keeps llvm.sh on its own
# CURRENT_LLVM_STABLE, and "all" is what pulls in libc++/libc++abi (required for
# `import std`) and libclang-rt (required for the sanitizers).
RUN curl -fsSL https://apt.llvm.org/llvm.sh -o /tmp/llvm.sh \
    && chmod +x /tmp/llvm.sh \
    && /tmp/llvm.sh all \
    && rm -f /tmp/llvm.sh \
    && rm -rf /var/lib/apt/lists/*

# apt.llvm.org installs version-suffixed binaries (clang-22, ...). The developer
# preset asks for plain clang/clang++, so alias whichever major landed above.
#
# The symlink works around Ubuntu's multiarch layout: clang resolves libc++'s
# module manifest to /usr/lib/x86_64-linux-gnu/libc++.modules.json, whose
# "source-path": "../share/libc++/v1/std.cppm" then points at /usr/lib/share/...,
# which the packages never create — the sources only exist under llvm-N/share.
# ci-linux dodges this by pinning CMAKE_CXX_STDLIB_MODULES_JSON, but the
# developer preset doesn't set it, so make the resolved path real instead.
#
# The closing checks fail the image build rather than letting a broken manifest
# surface much later as an unexplained `import std` error.
RUN set -eux; \
    llvm_ver="$(ls -d /usr/lib/llvm-* | sed 's|.*/llvm-||' | sort -n | tail -1)"; \
    for tool in clang clang++ clang-tidy clang-format clangd lldb lld ld.lld llvm-symbolizer; do \
        if [ -x "/usr/lib/llvm-${llvm_ver}/bin/${tool}" ]; then \
            update-alternatives --install "/usr/bin/${tool}" "${tool}" \
                "/usr/lib/llvm-${llvm_ver}/bin/${tool}" 100; \
        fi; \
    done; \
    mkdir -p /usr/lib/share; \
    ln -sfn "/usr/lib/llvm-${llvm_ver}/share/libc++" /usr/lib/share/libc++; \
    clang++ --version; \
    manifest="$(clang++ -stdlib=libc++ -print-library-module-manifest-path)"; \
    test "${manifest}" != "<NOT PRESENT>"; \
    test -f "$(dirname "${manifest}")/../share/libc++/v1/std.cppm"

RUN set -eux; \
    case "$(uname -m)" in \
        x86_64)  cmake_arch=linux-x86_64 ;; \
        aarch64) cmake_arch=linux-aarch64 ;; \
        *) echo "unsupported architecture: $(uname -m)" >&2; exit 1 ;; \
    esac; \
    curl -fsSL -o /tmp/cmake.sh \
        "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-${cmake_arch}.sh"; \
    sh /tmp/cmake.sh --skip-license --prefix=/usr/local; \
    rm -f /tmp/cmake.sh; \
    cmake --version
