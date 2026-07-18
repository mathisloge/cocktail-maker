# nix/overlay.nix
final: prev:
let
  # Pin Kitware CMake to exactly 4.3.0 to bypass upstream 4.4.x issues
  cmake_4_3 = prev.cmake.overrideAttrs (old: rec {
    version = "4.3.0";
    src = prev.fetchurl {
      url = "https://github.com/Kitware/CMake/releases/download/v${version}/cmake-${version}.tar.gz";
      hash = "sha256-RCH+fF7kGg0u33gA8T1XoA0z+6yO+M2wWwO7mK7g0D0="; # Replace with valid hash if changed
    };
    doCheck = false;
  });

  # Resolve the latest LLVM toolchain from unstable
  llvm = prev.llvmPackages_latest;

  # Define LLVM stdenv using Clang, libc++, and lld
  llvmStdenv = prev.overrideCC prev.stdenv (llvm.clangUseLLVM.override {
    bintools = llvm.lld;
    libcxx = llvm.libcxx;
  });

  # 1. Use the provided Nixpkgs-native packages directly
  gsl-lite = prev.gsl-lite; # Header-only
  cli11 = prev.cli11;       # Header-only

  # 2. Use Nixpkgs recipes, but override compiling with llvmStdenv for ABI consistency
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
