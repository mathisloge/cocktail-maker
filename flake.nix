# SPDX-FileCopyrightText: 2026 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Nix is only used here for the *production* build that ships to the
# cocktail-maker "Station" (Raspberry Pi 5, NixOS, aarch64-linux). Day-to-day
# development happens directly on macOS/Linux dev machines with their own
# system toolchains, via the `developer` / `ci-linux` CMake presets -- this
# flake deliberately does not try to cover that workflow, so there's a
# single hardcoded system below rather than a devShell matrix.
#
# Design notes:
#
# 1. This project fetches almost everything (Boost, Slint, mp-units, simdjson,
#    comms, cocktail-maker-protocol, libassert, Catch2, ...) via CPM.cmake at
#    *configure time*, straight from GitHub. That needs network access, which
#    a sandboxed `nix build` derivation does not have by default. Rather than
#    fighting CPM into a fully hermetic Nix package graph (pre-fetching every
#    dependency as its own FOD, wiring CPM_SOURCE_CACHE, pinning a dozen
#    hashes that go stale the moment a `#main` ref moves), this flake gives
#    you a reproducible *toolchain* via `devShells.default`, plus a thin
#    `apps.default` ("nix run") that drives the existing `production`
#    CMakePresets workflow with that toolchain. CPM still hits the network
#    like normal; every compiler/cmake/ninja version around it is pinned.
#
# 2. CMAKE_EXPERIMENTAL_CXX_IMPORT_STD is version-pinned in your CMakeLists.txt
#    (the UUID changes between CMake feature releases), and you're explicitly
#    avoiding 4.4.x. nixos-unstable moves fast and will happily hand you
#    4.4.x or newer, so cmake is pinned here to 4.3.4 explicitly rather than
#    trusting the channel default. This is the one package Nix will have to
#    build from source on-device (it's not an upstream nixpkgs version, so
#    there's no cache.nixos.org binary for it) -- a one-time ~10-20 minute
#    cost on a Pi 5, everything else (llvm, rustc, ninja, ...) should
#    substitute from the binary cache since aarch64-linux is a first-class
#    Hydra platform.
#
# 3. `import std;` needs the compiler's libc++.modules.json manifest. On a
#    normal Linux distro, `clang++ -print-file-name=libc++.modules.json`
#    finds it and CMake auto-detects everything. Nix's clang wrapper does
#    *not* report this correctly (see NixOS/nixpkgs#370217), so we compute
#    the real nix-store path to libcxx's modules.json ourselves and export
#    it as CMAKE_CXX_STDLIB_MODULES_JSON. This is picked up automatically by
#    the `nix-linux` preset in CMakePresets.json via `$env{...}`.
#
# 4. Slint is built from source via CPM (git tag pinned in CMakeLists.txt),
#    which means its Rust core gets compiled by `cargo` as part of your
#    build. The buildInputs/runtime libs below mirror slint-ui/slint's own
#    flake.nix (winit/femtovg/skia backend deps), minus the Qt backend,
#    Node/pnpm and doc-tooling bits you don't need to embed Slint as a
#    C++ dependency, plus DRM/KMS bits (libgbm, seatd, libinput) since a
#    kiosk Station on a Pi is a plausible direct-framebuffer target.
{
  description = "cocktail-maker production build for the Raspberry Pi 5 Station (aarch64-linux)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    rust-overlay = {
      url = "github:oxalica/rust-overlay";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    { self, nixpkgs, rust-overlay }:
    let
      system = "aarch64-linux";
      pkgs = import nixpkgs {
        inherit system;
        overlays = [ (import rust-overlay) ];
      };
      lib = pkgs.lib;

      # ---------------------------------------------------------------
      # CMake pinned to 4.3.4 (see design note 2 above). Hash verified
      # against Kitware/CMake's v4.3.4 tag tarball.
      # If this override ever breaks against a newer nixpkgs (e.g. a
      # version-gated patch in nixpkgs' cmake expression), the fallback
      # is to pin the `nixpkgs` input itself to a commit that still
      # shipped 4.3.x, instead of overriding.
      # ---------------------------------------------------------------
      cmake-pinned = pkgs.cmake.overrideAttrs (_old: rec {
        version = "4.3.4";
        src = pkgs.fetchFromGitHub {
          owner = "Kitware";
          repo = "CMake";
          rev = "v${version}";
          hash = "sha256-u5Z4Qd5o19E4hTLS6nmXE7oF1mQWmF44v0YdZ1kvAh0=";
        };
      });

      # ---------------------------------------------------------------
      # LLVM/Clang + libc++. Tracks nixpkgs' rolling "latest" major.
      # Swap for `pkgs.llvmPackages_22` if you want to pin the exact
      # major your ci-linux CMake preset names (clang-22).
      # ---------------------------------------------------------------
      llvm = pkgs.llvmPackages_latest;

      # libc++'s `import std;` module manifest (see design note 3 above).
      libcxxModulesJson = "${llvm.libcxx}/lib/libc++.modules.json";

      # Rust toolchain for Slint's core crates, built via cargo as part of
      # the C++ build. `rust-src` for rust-analyzer / clangd users on-device.
      rustToolchain = pkgs.rust-bin.stable.latest.default.override {
        extensions = [
          "rust-src"
          "rust-analyzer"
          "clippy"
          "rustfmt"
        ];
      };

      # Slint's own runtime deps for the winit/femtovg/skia backends,
      # mirrored from https://github.com/slint-ui/slint/blob/master/flake.nix
      # (Qt backend, Node/pnpm, and aspell tooling deliberately omitted).
      slintRuntimeLibs = with pkgs; [
        fontconfig
        wayland
        libxkbcommon
        libGL
        xorg.libX11
        xorg.libXcursor
        xorg.libXi
        xorg.libXrandr
        vulkan-loader
        libinput
        libgbm
        seatd
        udev
        alsa-lib
      ];

      nativeBuildInputs = with pkgs; [
        cmake-pinned
        ninja # CMake C++ module scanning needs Ninja >= 1.11
        pkg-config
        sccache
        git # GetGitRevisionDescription
        python3
        rustToolchain
        dpkg # CPack DEB generator (packagePresets.production)
        llvm.clang-tools # clangd / clang-format, matched to the same libc++ toolchain
      ];

      buildInputs =
        with pkgs;
        [
          openssl
          freetype
        ]
        ++ slintRuntimeLibs
        ++ [
          # Works around a jemalloc/glibc interaction some Slint crates hit
          # on Nix, see NixOS/nixpkgs#370494.
          rust-jemalloc-sys
        ];

      shellStdenv = llvm.libcxxStdenv;
    in
    {
      # Mainly useful when you're SSH'd into the Station itself and want an
      # interactive shell to debug a build or iterate without going through
      # `apps.default` each time.
      devShells.${system}.default = pkgs.mkShell.override { stdenv = shellStdenv; } {
        inherit nativeBuildInputs buildInputs;

        # Nix's default hardening flags (_FORTIFY_SOURCE et al.) can clash
        # with ASan/UBSan builds if you ever run `developer` here too.
        hardeningDisable = [ "fortify" ];

        env = {
          CMAKE_GENERATOR = "Ninja";
          CMAKE_CXX_STDLIB_MODULES_JSON = libcxxModulesJson;
        };

        shellHook = ''
          export CPM_SOURCE_CACHE="''${XDG_CACHE_HOME:-$HOME/.cache}/CPM"
          mkdir -p "$CPM_SOURCE_CACHE"
          export LD_LIBRARY_PATH="${lib.makeLibraryPath slintRuntimeLibs}:''${LD_LIBRARY_PATH:-}"

          echo "cocktail-maker Station build shell (${system})"
          echo "  cmake:  $(cmake --version | head -n1)"
          echo "  cc:     $CC"
          echo "  cxx:    $CXX"
          echo "  rustc:  $(rustc --version)"
          echo "  import std json: $CMAKE_CXX_STDLIB_MODULES_JSON"
          echo "  CPM cache: $CPM_SOURCE_CACHE"
        '';
      };

      # `nix run` wrapper for the `production` CMakePresets workflow
      # (configure -> build -> package as .deb). The `production` preset
      # itself reads CC/CXX/CMAKE_CXX_STDLIB_MODULES_JSON via $env{...}
      # (see the `nix-linux` hidden preset in CMakePresets.json), so this
      # script just needs to export them and run the preset as-is.
      #
      # Run this ON the Station (aarch64-linux). If you ever want to trigger
      # it remotely from a dev machine over SSH instead of logging in, add
      # the Pi as a Nix remote builder and use
      # `nix run .#default --system aarch64-linux --builders 'ssh://<pi-host> aarch64-linux'`.
      apps.${system}.default = {
        type = "app";
        program = "${pkgs.writeShellApplication {
          name = "cocktail-maker-build";
          runtimeInputs = nativeBuildInputs ++ buildInputs ++ [ shellStdenv.cc ];
          text = ''
            export CC="${shellStdenv.cc}/bin/cc"
            export CXX="${shellStdenv.cc}/bin/c++"
            export CMAKE_CXX_STDLIB_MODULES_JSON="${libcxxModulesJson}"
            cmake --preset production
            cmake --build --preset production
            cpack --preset production
          '';
        }}/bin/cocktail-maker-build";
      };

      formatter.${system} = pkgs.nixfmt-rfc-style;
    };
}
