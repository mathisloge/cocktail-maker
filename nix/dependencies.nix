# nix/dependencies.nix
{ pkgs, cmake_4_3, llvmStdenv }:

let
  mkCppDerivation = args: llvmStdenv.mkDerivation (args // {
    nativeBuildInputs = (args.nativeBuildInputs or [ ]) ++ [ cmake_4_3 pkgs.ninja ];
  });
in

rec {
  mp-units = mkCppDerivation rec {
    pname = "mp-units";
    version = "2.6.0-unstable";
    propagatedBuildInputs = [ pkgs.gsl-lite ];
    src = pkgs.fetchFromGitHub {
      owner = "mpusz";
      repo = "mp-units";
      rev = "8df25adeefc73931ff3a1da52804e5c7a061e2d1";
      hash = "sha256-oSovhu5yFR/XQQOI3N23zwyketQJ0BDVIEKG9VyayZQ=";
    };
    cmakeDir = "src";
    cmakeFlags = [
      "-DMP_UNITS_BUILD_CXX_MODULES=ON"
      "-DMP_UNITS_BUILD_TESTS=OFF"
      "-DCMAKE_CXX_STANDARD=26"
    ];
  };

  libassert = mkCppDerivation rec {
    pname = "libassert";
    version = "2.2.1";
    propagatedBuildInputs = [ pkgs.cpptrace ];
    src = pkgs.fetchFromGitHub {
      owner = "jeremy-rifkin";
      repo = "libassert";
      rev = "v${version}";
      hash = "sha256-ognudQ3NgpYxiDEucbIRWYQPs0XLRUQwg1eMxJm+aPs=";
    };

    # Configure CMake to search for external package instead of using FetchContent
    cmakeFlags = [
      "-DLIBASSERT_USE_EXTERNAL_CPPTRACE=ON"
    ];
  };

  libcomms = mkCppDerivation rec {
    pname = "libcomms";
    version = "5.5.2";
    src = pkgs.fetchFromGitHub {
      owner = "commschamp";
      repo = "comms";
      rev = "v${version}";
      hash = "sha256-U0KTpj3H+GcjiAPfKTl4h8MDZIZ3zZmQr6TpCdMk3bg=";
    };
  };

  cocktail-maker-protocol = mkCppDerivation rec {
    pname = "cocktail-maker-protocol";
    version = "main";
    src = pkgs.fetchFromGitHub {
      owner = "mathisloge";
      repo = "cocktail-maker-protocol";
      rev = "main";
      hash = "sha256-qKadWeS0rFaMJ/5uftJHlMD3E8ykyBtC1gqlsXPnASw=";
    };
    cmakeDir = "generated";

    # Propagate libcomms so find_package(LibComms) resolves during compilation
    propagatedBuildInputs = [ libcomms ];
  };

  slint-cpp = llvmStdenv.mkDerivation rec {
    pname = "slint-cpp";
    version = "1.7.0-unstable";

    src = pkgs.fetchFromGitHub {
      owner = "slint-ui";
      repo = "slint";
      rev = "a978809d37135b6d3abefb3f888baed7d1b41467";
      hash = "sha256-9F4KbG2pfO2Nl6WUXiDTxmvfXwoaHyw8S79IWXc9kBA=";
    };

    cargoDeps = pkgs.rustPlatform.fetchCargoVendor {
      inherit src;
      name = "${pname}-${version}-cargo-vendor";
      hash = "sha256-BApQGhoyWTLzv/jpcQHITgFboC9CYk+3UiMCkzpfJKo=";
    };

    nativeBuildInputs = [
      cmake_4_3
      pkgs.ninja
      pkgs.rustPlatform.cargoSetupHook
      pkgs.cargo
      pkgs.rustc
      pkgs.pkg-config
    ];

    buildInputs = [
      pkgs.libx11
      pkgs.libxcursor
      pkgs.libxrandr
      pkgs.libxi
      pkgs.libxkbcommon
      pkgs.fontconfig
    ];

    postUnpack = "sourceRoot=\${sourceRoot}/api/cpp";

    preConfigure = ''
      export RUSTC_WRAPPER="sccache"
    '';
  };
}
