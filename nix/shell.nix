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
    pkgs.gsl-lite
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
    export CXXFLAGS="-stdlib=libc++ -std=c++26"
    export LDFLAGS="-stdlib=libc++ -fuse-ld=lld"
    
    # Export compiler wrapper overrides to allow local module scans to resolve libc++.modules.json
    export NIX_CFLAGS_COMPILE="-stdlib=libc++ -Wno-unused-command-line-argument -B${pkgs.llvmPackages_latest.libcxx}/lib -isystem ${pkgs.llvmPackages_latest.libcxx.dev}/include/c++/v1"

    # Set up local cargo offline config inside the development shell
    mkdir -p .cargo
    cp ${deps.slint-cargo-vendor}/.cargo/config.toml .cargo/config.toml
    chmod u+w .cargo/config.toml
    sed -i "s|@vendor@|${deps.slint-cargo-vendor}|g" .cargo/config.toml
    
    # Export local source configurations
    export CPM_Boost_SOURCE="${deps.boost-src}"
    export CPM_spdlog_SOURCE="${deps.spdlog-src}"
    export CPM_CLI11_SOURCE="${deps.cli11-src}"
    export CPM_mp-units_SOURCE="${deps.mp-units-src}"
    export CPM_simdjson_SOURCE="${deps.simdjson-src}"
    export CPM_Slint_SOURCE="${deps.slint-src}"
    export CPM_LibComms_SOURCE="${deps.libcomms-src}"
    export CPM_cocktail_maker_protocol_SOURCE="${deps.cocktail-maker-protocol-src}"
    export CPM_libassert_SOURCE="${deps.libassert-src}"
    export CPM_Catch2_SOURCE="${deps.catch2-src}"

    # Force local builds to emulate the same sandbox lookups
    export CPM_USE_LOCAL_PACKAGES="ON"
    export CPM_LOCAL_PACKAGES_ONLY="ON"

    # Export global Boost settings to the terminal environment
    export BOOST_ENABLE_CMAKE="ON"
    export BOOST_INCLUDE_LIBRARIES="asio;cobalt;hash2"
    
    # Export global Boost version settings
    export BOOST_VERSION="1.91.0-1"
    export Boost_VERSION="1.91.0-1"
    export CPM_Boost_VERSION="1.91.0-1"

    # Configure C++ Modules standard library path dynamically for the Nix environment
    export CMAKE_CXX_STDLIB_MODULES_JSON="${pkgs.llvmPackages_latest.libcxx}/lib/libc++.modules.json"

    echo "========================================================="
    echo " Cocktail Maker C++26 Dev Environment (LLVM Clang + libc++)"
    echo " - CMake version: $(cmake --version | head -n 1)"
    echo " - Compiler:      $(${CC} --version | head -n 1)"
    echo "========================================================="
  '';
}
