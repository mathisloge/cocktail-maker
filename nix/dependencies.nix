# nix/dependencies.nix
{ pkgs, cmake_4_3, llvmStdenv }:

let
  mkCppDerivation = args: llvmStdenv.mkDerivation (args // {
    nativeBuildInputs = (args.nativeBuildInputs or [ ]) ++ [ cmake_4_3 pkgs.ninja ];
  });
in
{
  mp-units = mkCppDerivation rec {
    pname = "mp-units";
    version = "2.3.0-unstable";
    src = pkgs.fetchFromGitHub {
      owner = "mpusz";
      repo = "mp-units";
      rev = "8df25adeefc73931ff3a1da52804e5c7a061e2d1";
      hash = "sha256-8g6nCis0Z491PypG2o/P32XoE/SyyB9eWfM88P7uO/I="; # Replace with valid hash
    };
    cmakeFlags = [
      "-DMP_UNITS_BUILD_CXX_MODULES=ON"
      "-DMP_UNITS_BUILD_TESTS=OFF"
    ];
  };

  libassert = mkCppDerivation rec {
    pname = "libassert";
    version = "2.2.1";
    src = pkgs.fetchFromGitHub {
      owner = "jeremy-rifkin";
      repo = "libassert";
      rev = "v${version}";
      hash = "sha256-1g6nCis0Z491PypG2o/P32XoE/SyyB9eWfM88P7uO/I="; # Replace with valid hash
    };
  };

  libcomms = mkCppDerivation rec {
    pname = "libcomms";
    version = "5.5.2";
    src = pkgs.fetchFromGitHub {
      owner = "commschamp";
      repo = "comms";
      rev = "v${version}";
      hash = "sha256-0g6nCis0Z491PypG2o/P32XoE/SyyB9eWfM88P7uO/I="; # Replace with valid hash
    };
  };

  cocktail-maker-protocol = mkCppDerivation rec {
    pname = "cocktail-maker-protocol";
    version = "main";
    src = pkgs.fetchFromGitHub {
      owner = "mathisloge";
      repo = "cocktail-maker-protocol";
      rev = "main";
      hash = "sha256-5g6nCis0Z491PypG2o/P32XoE/SyyB9eWfM88P7uO/I="; # Replace with valid hash
    };
    postUnpack = "sourceRoot=\${sourceRoot}/generated";
  };

  slint-cpp = llvmStdenv.mkDerivation rec {
    pname = "slint-cpp";
    version = "1.7.0-unstable";

    src = pkgs.fetchFromGitHub {
      owner = "slint-ui";
      repo = "slint";
      rev = "a978809d37135b6d3abefb3f888baed7d1b41467";
      hash = "sha256-7g6nCis0Z491PypG2o/P32XoE/SyyB9eWfM88P7uO/I="; # Replace with valid hash
    };

    cargoDeps = pkgs.rustPlatform.fetchCargoTarball {
      inherit src;
      name = "${pname}-${version}-cargo-deps";
      hash = "sha256-3g6nCis0Z491PypG2o/P32XoE/SyyB9eWfM88P7uO/I="; # Replace with valid hash
    };

    nativeBuildInputs = [
      cmake_4_3
      pkgs.ninja
      pkgs.rustPlatform.cargoSetupHook
      pkgs.rustPlatform.rust.cargo
      pkgs.rustPlatform.rust.rustc
      pkgs.pkg-config
    ];

    buildInputs = [
      pkgs.xorg.libX11
      pkgs.xorg.libXcursor
      pkgs.xorg.libXrandr
      pkgs.xorg.libXi
      pkgs.libxkbcommon
      pkgs.fontconfig
    ];

    postUnpack = "sourceRoot=\${sourceRoot}/api/cpp";

    preConfigure = ''
      export RUSTC_WRAPPER="sccache"
    '';
  };
}
