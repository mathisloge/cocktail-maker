# nix/packages.nix
{ pkgs
, llvmStdenv
, cmake_4_3
, deps
}:

let
  packageJson = builtins.fromJSON (builtins.readFile ../package.json);
in
llvmStdenv.mkDerivation {
  pname = "cocktail-maker";
  version = packageJson.version;

  src = pkgs.lib.cleanSource ../.;

  # Registers Slint's vendor package so cargo builds can run offline
  cargoDeps = deps.slint-cargo-vendor;

  nativeBuildInputs = [
    cmake_4_3
    pkgs.ninja
    pkgs.pkg-config
    pkgs.rustPlatform.cargoSetupHook
    pkgs.cargo
    pkgs.rustc
  ];

  # System/runtime libraries required for the final executable
  buildInputs = [
    pkgs.cpptrace # Backtrace support for libassert
    pkgs.libx11
    pkgs.libxcursor
    pkgs.libxrandr
    pkgs.libxi
    pkgs.libxkbcommon
    pkgs.fontconfig
  ] ++ pkgs.lib.optionals pkgs.stdenv.isLinux [ pkgs.udev ];

  cmakeFlags = [
    "-DBUILD_TESTING=ON"
    # Instructs the libassert subproject to use the system cpptrace library
    "-DLIBASSERT_USE_EXTERNAL_CPPTRACE=ON"

    # CPM Local Source Overrides
    # This prevents CPM from making network calls and compiles dependencies inline
    "-DCPM_Boost_SOURCE=${deps.boost-src}"
    "-DCPM_gsl-lite_SOURCE=${deps.gsl-lite-src}"
    "-DCPM_spdlog_SOURCE=${deps.spdlog-src}"
    "-DCPM_CLI11_SOURCE=${deps.cli11-src}"
    "-DCPM_mp-units_SOURCE=${deps.mp-units-src}"
    "-DCPM_simdjson_SOURCE=${deps.simdjson-src}"
    "-DCPM_Slint_SOURCE=${deps.slint-src}"
    "-DCPM_LibComms_SOURCE=${deps.libcomms-src}"
    "-DCPM_cocktail-maker-protocol_SOURCE=${deps.cocktail-maker-protocol-src}"
    "-DCPM_libassert_SOURCE=${deps.libassert-src}"
    "-DCPM_Catch2_SOURCE=${deps.catch2-src}"
  ];

  enableParallelBuilding = true;
}
