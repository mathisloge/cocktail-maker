#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Mathis Logemann <mathis@quite.rocks>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Quick-iterate wrapper around `nixos/nix` in Docker for testing flake.nix
# without installing Nix (or NixOS) on the host.
#
# Step 1 (this script): host's native architecture only, no --platform /
# emulation. flake.nix stays untouched -- it's still hardcoded to
# aarch64-linux (the Pi 5 target), so this only does something useful when
# run on an aarch64 host (Apple Silicon Mac, or the Pi itself). Cross-arch
# testing from an x86_64 host is a follow-up step, not this one.
#
# Usage:
#   ./nix-iterate.sh                # nix run .#default  (full production build)
#   ./nix-iterate.sh develop        # interactive devShell
#   ./nix-iterate.sh flake check    # quick sanity check
#   ./nix-iterate.sh <anything>     # passed straight through to `nix`
set -euo pipefail

# Pin the image so the test harness itself doesn't drift under you.
IMAGE="nixos/nix:latest"

host_arch="$(uname -m)"
volume="nix-store-${host_arch}"

case "$host_arch" in
    aarch64 | arm64) ;;
    *)
        echo "warning: host arch is '${host_arch}', but flake.nix targets aarch64-linux only." >&2
        echo "         this script intentionally doesn't force --platform yet, so 'nix run .#default'" >&2
        echo "         etc. will likely fail here with a missing-flake-output error." >&2
        ;;
esac

docker volume create "$volume" >/dev/null

tty_args=()
if [ -t 1 ]; then
    tty_args=(-it)
fi

args=("$@")
if [ ${#args[@]} -eq 0 ]; then
    args=(run .#default)
fi

docker run --rm "${tty_args[@]}" \
    -v "$(pwd)":/workspace \
    -v "${volume}":/nix \
    -w /workspace \
    -e NIX_CONFIG="experimental-features = nix-command flakes" \
    "$IMAGE" \
    nix "${args[@]}"
