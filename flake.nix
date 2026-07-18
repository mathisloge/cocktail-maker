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
            # Inject pre-patched cmake from staging-next
            (final: prev: {
              cmake_4_3 = pkgsStagingNext.cmake;
            })

            # Main project overlay
            self.overlays.default
          ];
        };
      in
      {
        packages = {
          default = pkgs.cocktail-maker;
          cocktail-maker = pkgs.cocktail-maker;
        };

        devShells.default = import ./nix/shell.nix {
          inherit pkgs;
          llvmStdenv = pkgs.llvmStdenv;
          cmake_4_3 = pkgs.cmake_4_3;
        };

        formatter = pkgs.nixpkgs-fmt;

        checks.build = self.packages.${system}.cocktail-maker;
      }
    ) // {
      overlays.default = import ./nix/overlay.nix;
    };
}