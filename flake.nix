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
        # Resolve legacy package set from the staging-next branch
        pkgsStagingNext = nixpkgs-staging-next.legacyPackages.${system};

        pkgs = import nixpkgs {
          inherit system;
          overlays = [
            # Inject pre-patched cmake from staging-next directly as 'cmake_4_3'
            (final: prev: {
              cmake_4_3 = pkgsStagingNext.cmake;
            })
            self.overlays.default
          ];
        };
      in
      {
        packages = {
          default = pkgs.cocktail-maker;
          cocktail-maker = pkgs.cocktail-maker;
          cmake_4_3 = pkgs.cmake_4_3;
        } // pkgs.cocktailMakerDeps;

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
