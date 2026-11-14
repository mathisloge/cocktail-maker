# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later

FROM mcr.microsoft.com/devcontainers/base:resolute
RUN apt-get update && apt-get install -y --no-install-recommends wget && rm -rf /var/lib/apt/lists/*
RUN test -f /usr/share/doc/kitware-archive-keyring/copyright || wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null && \
    echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ resolute main' | sudo tee /etc/apt/sources.list.d/kitware.list >/dev/null

# llvm.sh prerequisites (lsb-release/wget/gnupg/add-apt-repository), the build
# tooling, and the runtime deps Slint needs — same set the Linux CI installs.
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl wget cmake gnupg lsb-release software-properties-common \
        git ninja-build pkg-config mold xvfb \
        libudev-dev libx11-xcb-dev libwayland-dev libfontconfig1-dev \
        libxkbcommon-x11-0 libgl1 libegl1 libinput10 \
    && rm -rf /var/lib/apt/lists/*

# LLVM 23, pinned to match CI. "all" is what pulls in libc++/libc++abi (required
# for `import std`) and libclang-rt (required for the sanitizers).
RUN curl -fsSL https://apt.llvm.org/llvm.sh -o /tmp/llvm.sh \
    && chmod +x /tmp/llvm.sh \
    && /tmp/llvm.sh 23 all \
    && rm -f /tmp/llvm.sh \
    && rm -rf /var/lib/apt/lists/*

# apt.llvm.org installs version-suffixed binaries (clang-23, ...). The developer
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
    for tool in clang clang++ clang-tidy clang-format clangd lldb lldb-dap lld ld.lld llvm-symbolizer; do \
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

# The repo's dev tooling (pre-commit, pytest, ruff, commitizen) is managed by uv
# (pyproject.toml). python3-venv is what pre-commit's python-language hooks
# (gersemi, codespell, reuse, ...) need to build their environments on first run.
COPY --from=ghcr.io/astral-sh/uv:0.13.8 /uv /uvx /bin/
RUN apt-get update && apt-get install -y --no-install-recommends \
        python3 python3-venv \
    && rm -rf /var/lib/apt/lists/*
