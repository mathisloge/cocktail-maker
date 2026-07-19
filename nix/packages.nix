# nix/packages.nix
{ pkgs
, llvmStdenv
, cmake_4_3
, deps
}:

let
  packageJson = builtins.fromJSON (builtins.readFile ../package.json);
  
  # Resolve the exact Nix paths for the C and C++ standard library headers
  libcxxDev = pkgs.llvmPackages_22.libcxx.dev;
  libcDev = pkgs.stdenv.cc.libc.dev;
in
llvmStdenv.mkDerivation {
  pname = "cocktail-maker";
  version = packageJson.version;

  src = pkgs.lib.cleanSource ../.;

  nativeBuildInputs = [
    cmake_4_3
    pkgs.ninja
    pkgs.pkg-config
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

  cmakeFlags = [
    "-DBUILD_TESTING=ON"
    "-DCMAKE_CXX_STDLIB_MODULES_JSON=${pkgs.llvmPackages_22.libcxx}/lib/libc++.modules.json"
    "-DCPM_LOCAL_PACKAGES_ONLY=ON"

    # CPM Local Source Overrides
    "-DCPM_Boost_SOURCE=${deps.boost-src}"
    "-DCPM_spdlog_SOURCE=${deps.spdlog-src}"
    "-DCPM_CLI11_SOURCE=${deps.cli11-src}"
    "-DCPM_mp-units_SOURCE=${deps.mp-units-src}"
    "-DCPM_simdjson_SOURCE=${deps.simdjson-src}"
    "-DCPM_Slint_SOURCE=${deps.slint-src}"
    "-DCPM_LibComms_SOURCE=${deps.libcomms-src}"
    "-DCPM_cocktail-maker-protocol_SOURCE=${deps.cocktail-maker-protocol-src}"
    "-DCPM_libassert_SOURCE=${deps.libassert-src}"
    "-DLIBASSERT_USE_EXTERNAL_CPPTRACE=ON"
    "-DCPM_Catch2_SOURCE=${deps.catch2-src}"
  ];

  preConfigure = ''
    # Safely inject flags containing spaces directly into the CMake array 
    # to prevent Bash from splitting them into invalid arguments.
    cmakeFlagsArray+=(
      "-DCMAKE_CXX_FLAGS=-std=c++26 -stdlib=libc++ -nostdinc++ -isystem ${libcxxDev}/include/c++/v1 -isystem ${libcDev}/include"
      "-DCMAKE_C_FLAGS=-isystem ${libcDev}/include"
    )

    # Set up completely offline cargo environment
    export CARGO_HOME="$TMPDIR/cargo-home"
    mkdir -p "$CARGO_HOME"
    
    # Copy the offline registry config, make it writable, and replace the placeholder
    cp ${deps.slint-cargo-vendor}/.cargo/config.toml "$CARGO_HOME/config.toml"
    chmod u+w "$CARGO_HOME/config.toml"
    sed -i "s|@vendor@|${deps.slint-cargo-vendor}|g" "$CARGO_HOME/config.toml"
  '';

  enableParallelBuilding = true;
}
