# nix/overlay.nix
final: prev:
let
  # Pin Kitware CMake to exactly 4.3.0 to bypass upstream 4.4.x issues
  cmake_4_3 = prev.cmake.overrideAttrs (old: rec {
    version = "4.3.0";
    src = prev.fetchurl {
      url = "https://github.com/Kitware/CMake/releases/download/v${version}/cmake-${version}.tar.gz";
      hash = "sha256-9Rs8cp+F2N3kapLAcdKCbqavt32FD0aJQSXefMUbqnc=";
    };
    doCheck = false; 
  });

  # Resolve the latest LLVM toolchain from unstable
  llvm = prev.llvmPackages_latest;

  # Define LLVM stdenv using the pre-wrapped Clang configuration
  # This uses LLVM bintools (LLD) and libc++ out of the box and avoids wrapper configuration errors
  llvmStdenv = prev.overrideCC prev.stdenv llvm.clangUseLLVM;

  # Use the provided Nixpkgs-native packages directly
  gsl-lite = prev.gsl-lite; # Header-only
  cli11 = prev.cli11;       # Header-only

  # Use Nixpkgs recipes, but override compiling with llvmStdenv for ABI consistency
  spdlog = prev.spdlog.override { stdenv = llvmStdenv; };
  simdjson = prev.simdjson.override { stdenv = llvmStdenv; };

  # Custom dependencies (not in nixpkgs or requiring customized source branches)
  customDeps = import ./dependencies.nix {
    pkgs = final;
    inherit cmake_4_3 llvmStdenv;
  };
in
{
  inherit cmake_4_3 llvmStdenv;

  # Merge native Nixpkgs dependencies with our custom packages
  cocktailMakerDeps = customDeps // {
    inherit gsl-lite cli11 spdlog simdjson;
  };

  # Main cocktail-maker package
  cocktail-maker = final.callPackage ./packages.nix {
    inherit llvmStdenv cmake_4_3;
    deps = final.cocktailMakerDeps;
  };
}
