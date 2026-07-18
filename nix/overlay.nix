# nix/overlay.nix
final: prev:
let
  # Inherit the pre-patched CMake 4.3 package injected by flake.nix
  cmake_4_3 = prev.cmake_4_3;

  # Use quite recent llvm
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

  # Globally override Nixpkgs C++ libraries to use our LLVM toolchain, 

  catch2_3 = (prev.catch2_3.override { stdenv = llvmStdenv; }).overrideAttrs (old: {
    doCheck = false;
  });

spdlog = (prev.spdlog.override { stdenv = llvmStdenv; }).overrideAttrs (old: {
    cmakeFlags = (builtins.filter (flag: 
      builtins.match ".*(SPDLOG_FMT_EXTERNAL|SPDLOG_USE_STD_FORMAT).*" flag == null
    ) old.cmakeFlags) ++ [
      "-DSPDLOG_FMT_EXTERNAL:BOOL=OFF"
      "-DSPDLOG_USE_STD_FORMAT:BOOL=ON"
    ];
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
