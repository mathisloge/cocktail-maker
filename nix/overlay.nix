# nix/overlay.nix
final: prev:
let
  # Inherit the pre-patched CMake 4.3 package injected by flake.nix
  cmake_4_3 = prev.cmake_4_3;

  # Use llvmPackages_22, patched in flake.nix
  llvm = prev.llvmPackages_22;

  # Define LLVM stdenv using the pre-wrapped Clang configuration
  llvmStdenv = prev.overrideCC prev.stdenv llvm.clangUseLLVM;

  # Fetch our dependency sources
  customDeps = import ./dependencies.nix {
    pkgs = final;
  };
in
{
  inherit llvmStdenv;

  cocktailMakerDeps = customDeps;

  # Compile cocktail-maker, letting callPackage automatically resolve dependencies from final
  cocktail-maker = final.callPackage ./packages.nix {
    inherit llvmStdenv cmake_4_3;
    deps = final.cocktailMakerDeps;
  };
}
