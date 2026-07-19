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

  nativeBuildInputs = [
    cmake_4_3
    pkgs.ninja
    pkgs.pkg-config
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

  cmakeFlags = [
    "-DBUILD_TESTING=ON"
    "-DLIBASSERT_USE_EXTERNAL_CPPTRACE=ON"
    
    # CPM Local Source Overrides
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

  preConfigure = ''
    # Create the local cargo configuration and symlink the pre-baked offline registry
    mkdir -p .cargo
    ln -sf ${deps.slint-cargo-vendor}/config.toml .cargo/config.toml
  '';

  enableParallelBuilding = true;
}
