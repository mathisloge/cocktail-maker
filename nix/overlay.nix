# nix/overlay.nix
final: prev:
let
  # Inherit the pre-patched CMake 4.3 package injected by flake.nix
  cmake_4_3 = prev.cmake_4_3;

  # Resolve the latest LLVM toolchain from unstable
  llvm = prev.llvmPackages_21;

  # Define LLVM stdenv using the pre-wrapped Clang configuration
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
  inherit llvmStdenv;

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
