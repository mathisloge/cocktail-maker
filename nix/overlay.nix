# nix/overlay.nix
final: prev:
let
  # Inherit the pre-patched CMake 4.3 package injected by flake.nix
  cmake_4_3 = prev.cmake_4_3;

  # Use llvmPackages_latest (resolves to LLVM 22), patched in flake.nix
  llvm = prev.llvmPackages_latest;

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

  # Globally override Nixpkgs C++ libraries to use our LLVM toolchain, 

  catch2_3 = (prev.catch2_3.override { stdenv = llvmStdenv; }).overrideAttrs (old: {
    doCheck = false;
  });

  spdlog = (prev.spdlog.override { stdenv = llvmStdenv; }).overrideAttrs (old: {
    # Replace the default "-DSPDLOG_BUILD_TESTS=ON" flag with "OFF"
    cmakeFlags = map (flag: 
      if flag == "-DSPDLOG_BUILD_TESTS=ON" 
      then "-DSPDLOG_BUILD_TESTS=OFF" 
      else flag
    ) old.cmakeFlags;
    doCheck = false;
  });

  simdjson = (prev.simdjson.override { stdenv = llvmStdenv; }).overrideAttrs (old: {
    doCheck = false;
  });

  cpptrace = (prev.cpptrace.override { stdenv = llvmStdenv; }).overrideAttrs (old: {
    doCheck = false;
  });

  # Inject custom, out-of-tree dependencies directly into the global package set
  inherit (customDeps) mp-units libassert libcomms cocktail-maker-protocol slint-cpp;

  # Compile cocktail-maker, letting callPackage automatically resolve dependencies from final
  cocktail-maker = final.callPackage ./packages.nix {
    inherit llvmStdenv cmake_4_3;
  };
}
