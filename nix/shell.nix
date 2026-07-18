# nix/shell.nix
{ pkgs
, llvmStdenv
, cmake_4_3
, deps
}:

pkgs.mkShell.override { stdenv = llvmStdenv; } {
  name = "cocktail-maker-devshell";

  packages = [
    cmake_4_3
    pkgs.ninja
    pkgs.pkg-config
    pkgs.sccache
    pkgs.clang-tools
    pkgs.lldb
    pkgs.cargo
    pkgs.rustc
  ];

  buildInputs = [
    pkgs.udev
    pkgs.boost190
    deps.gsl-lite
    deps.spdlog
    deps.cli11
    deps.mp-units
    deps.simdjson
    deps.slint-cpp
    deps.libcomms
    deps.cocktail-maker-protocol
    deps.libassert
    pkgs.catch2_3
  ];

  shellHook = ''
    export CC=clang
    export CXX=clang++
    export CXXFLAGS="-stdlib=libc++"
    export LDFLAGS="-stdlib=libc++ -fuse-ld=lld"
    
    export CMAKE_C_COMPILER_LAUNCHER=sccache
    export CMAKE_CXX_COMPILER_LAUNCHER=sccache
    export RUSTC_WRAPPER=sccache
    
    export SCCACHE_DIR="$(pwd)/.sccache"
    mkdir -p "$SCCACHE_DIR"
    
    echo "========================================================="
    echo " Cocktail Maker C++26 Dev Environment (LLVM Clang + libc++)"
    echo " - CMake version: $(cmake --version | head -n 1)"
    echo " - Compiler:      $($CC --version | head -n 1)"
    echo " - sccache is active for C++ and Rust builds."
    echo "========================================================="
  '';
}
