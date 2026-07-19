# flake.nix
{
  description = "Production-grade C++26 development and build environment for cocktail-maker";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    nixpkgs-staging-next.url = "github:NixOS/nixpkgs/staging-next";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, nixpkgs-staging-next, utils }@inputs:
    utils.lib.eachDefaultSystem (system:
      let
        pkgsStagingNext = nixpkgs-staging-next.legacyPackages.${system};

        pkgs = import nixpkgs {
          inherit system;
          overlays = [
            # 1. Workaround: Patch LLVM 22 compiler-rt-no-libc regression on aarch64 (Nixpkgs Issue #495155)
            (final: prev: {
              llvmPackages_22 = prev.llvmPackages_22 // {
                compiler-rt-no-libc = prev.llvmPackages_22.compiler-rt-no-libc.overrideAttrs (old: {
                  patches = (old.patches or []) ++ [
                    (prev.fetchpatch {
                      url = "https://github.com/llvm/llvm-project/commit/7b820b28353e788603e56698035db42bb3327713.patch";
                      hash = "sha256-sBOkvrECfOHAUQ2JewQyTQ7QbSH1ZzsenUBgfqVa7Bc=";
                      revert = true;
                      stripLen = 1;
                    })
                  ];
                });
              };
            })

            # 2. Inject pre-patched cmake from staging-next
            (final: prev: {
              cmake_4_3 = pkgsStagingNext.cmake;
            })

            # 3. Main project overlay
            self.overlays.default
          ];
        };
      in
      {
        packages = {
          default = pkgs.cocktail-maker;
          cocktail-maker = pkgs.cocktail-maker;
          cmake_4_3 = pkgs.cmake_4_3;
        };

        devShells.default = import ./nix/shell.nix {
          inherit pkgs;
          llvmStdenv = pkgs.llvmStdenv;
          cmake_4_3 = pkgs.cmake_4_3;
          deps = pkgs.cocktailMakerDeps;
        };

        formatter = pkgs.nixpkgs-fmt;

        checks.build = self.packages.${system}.cocktail-maker;
      }
    ) // {
      overlays.default = import ./nix/overlay.nix;
    };
}
