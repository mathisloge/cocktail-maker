# nix/packages.nix
{ pkgs
, llvmStdenv
, cmake_4_3
}:

let
  packageJson = builtins.fromJSON (builtins.readFile ../package.json);
in
llvmStdenv.mkDerivation rec {
  pname = "cocktail-maker";
  version = packageJson.version;

  src = pkgs.lib.cleanSource ../.;

  nativeBuildInputs = [
    cmake_4_3
    pkgs.ninja
    pkgs.pkg-config
    pkgs.sccache
  ];

  buildInputs = [
    pkgs.boost190
    pkgs.gsl-lite
    pkgs.spdlog
    pkgs.cli11
    pkgs.mp-units
    pkgs.simdjson
    pkgs.slint-cpp
    pkgs.libcomms
    pkgs.cocktail-maker-protocol
    pkgs.libassert
    pkgs.catch2_3
  ] ++ pkgs.lib.optionals pkgs.stdenv.isLinux [ pkgs.udev ];

  cmakeFlags = [
    "-DCPM_LOCAL_PACKAGES_ONLY=ON"
    "-DBUILD_TESTING=ON"
    "-DCMAKE_C_COMPILER_LAUNCHER=sccache"
    "-DCMAKE_CXX_COMPILER_LAUNCHER=sccache"
  ];

  preConfigure = ''
    export RUSTC_WRAPPER="sccache"
    export SCCACHE_DIR=$(pwd)/.sccache
  '';

  enableParallelBuilding = true;
}
