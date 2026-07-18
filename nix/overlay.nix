# nix/overlay.nix
final: prev:
let
  # Inherit the pre-patched CMake 4.3 package injected by flake.nix
  cmake_4_3 = prev.cmake_4_3;

  llvm = prev.llvmPackages_21;

  # Define LLVM stdenv using the pre-wrapped Clang configuration
  llvmStdenv = prev.overrideCC prev.stdenv llvm.clangUseLLVM;

  # Fetch our custom (non-Nixpkgs) dependencies
  customDeps = import ./dependencies.nix {
    pkgs = final;
    inherit cmake_4_3 llvmStdenv;
  };
in
{
  inherit llvmStdenv;

  # 1. Globally override Nixpkgs-provided libraries to use our LLVM stdenv
  spdlog = prev.spdlog.override { stdenv = llvmStdenv; };
  simdjson = prev.simdjson.override { stdenv = llvmStdenv; };
  cpptrace = prev.cpptrace.override { stdenv = llvmStdenv; };

  # 2. Inject custom, out-of-tree dependencies directly into the global package set
  inherit (customDeps) mp-units libassert libcomms cocktail-maker-protocol slint-cpp;

  # 3. Compile cocktail-maker, letting callPackage automatically resolve dependencies from final
  cocktail-maker = final.callPackage ./packages.nix {
    inherit llvmStdenv cmake_4_3;
  };
}
