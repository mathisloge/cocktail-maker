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
    pkgs.clang-tools
    pkgs.lldb
    pkgs.cargo
    pkgs.rustc
    pkgs.corrosion
  ];

  buildInputs = [
    pkgs.cpptrace
    pkgs.libx11
    pkgs.libxcursor
    pkgs.libxrandr
    pkgs.libxi
    pkgs.libxkbcommon
    pkgs.fontconfig
  ] ++ pkgs.lib.optionals pkgs.stdenv.isLinux [ pkgs.udev ];

  shellHook = ''
    export CC=clang
    export CXX=clang++
    export CXXFLAGS="-stdlib=libc++"
    export LDFLAGS="-stdlib=libc++ -fuse-ld=lld"

    # Set up local cargo offline config inside the development shell
    mkdir -p .cargo
    ln -sf ${deps.slint-cargo-vendor}/config.toml .cargo/config.toml
    
    # Export local source configurations for offline CPM
    export CPM_Boost_SOURCE="${deps.boost-src}"
    export CPM_gsl_lite_SOURCE="${deps.gsl-lite-src}"
    export CPM_spdlog_SOURCE="${deps.spdlog-src}"
    export CPM_CLI11_SOURCE="${deps.cli11-src}"
    export CPM_mp-units_SOURCE="${deps.mp-units-src}"
    export CPM_simdjson_SOURCE="${deps.simdjson-src}"
    export CPM_Slint_SOURCE="${deps.slint-src}"
    export CPM_LibComms_SOURCE="${deps.libcomms-src}"
    export CPM_cocktail_maker_protocol_SOURCE="${deps.cocktail-maker-protocol-src}"
    export CPM_libassert_SOURCE="${deps.libassert-src}"
    export CPM_Catch2_SOURCE="${deps.catch2-src}"
    
    echo "========================================================="
    echo " Cocktail Maker C++26 Dev Environment (LLVM Clang + libc++)"
    echo " - CMake version: $(cmake --version | head -n 1)"
    echo " - Compiler:      $(${CC} --version | head -n 1)"
    echo "========================================================="
  '';
}
