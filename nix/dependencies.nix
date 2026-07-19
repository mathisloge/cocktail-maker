# nix/dependencies.nix
{ pkgs }:

let
  fetchGithubSource = { owner, repo, rev, hash }: pkgs.fetchFromGitHub {
    inherit owner repo rev hash;
  };
in
rec {
  # CPM overrides expect raw unpack source directories
  boost-src = pkgs.fetchzip {
    url = "https://github.com/boostorg/boost/releases/download/boost-1.91.0-1/boost-1.91.0-1-cmake.tar.xz";
    hash = "sha256-lX4s/HyDIEhn7PNyCocFYhizTx6CSkSfmGCSU+eqOYw=";
  };

  gsl-lite-src = fetchGithubSource {
    owner = "gsl-lite";
    repo = "gsl-lite";
    rev = "v1.1.0";
    hash = "sha256-lX4s/HyDIEhn7PNyCocFYhizTx6CSkSfmGCSU+eqOYw=";
  };

  spdlog-src = fetchGithubSource {
    owner = "gabime";
    repo = "spdlog";
    rev = "v1.17.0";
    hash = "sha256-bL3hQmERXNwGmDoi7+wLv/TkppGhG6cO47k1iZvJGzY=";
  };

  cli11-src = fetchGithubSource {
    owner = "CLIUtils";
    repo = "CLI11";
    rev = "v2.6.2";
    hash = "sha256-TcOmx/qUK/w3mO0bDHX+TRxxMwJpaDFQBcpkQj3hz8A=";
  };

  mp-units-src = fetchGithubSource {
    owner = "mpusz";
    repo = "mp-units";
    rev = "8df25adeefc73931ff3a1da52804e5c7a061e2d1";
    hash = "sha256-oSovhu5yFR/XQQOI3N23zwyketQJ0BDVIEKG9VyayZQ=";
  };

  simdjson-src = fetchGithubSource {
    owner = "simdjson";
    repo = "simdjson";
    rev = "v4.6.4";
    hash = "sha256-8oQzsR7DSaNTN9su1uI9tRQ9HvOwXShPwSrnQj8+lGM=";
  };

  slint-src = fetchGithubSource {
    owner = "slint-ui";
    repo = "slint";
    rev = "a978809d37135b6d3abefb3f888baed7d1b41467";
    hash = "sha256-9F4KbG2pfO2Nl6WUXiDTxmvfXwoaHyw8S79IWXc9kBA=";
  };

  # Local vendor archive so Slint's inline Rust compilation can run fully offline
  slint-cargo-vendor = pkgs.rustPlatform.fetchCargoVendor {
    src = slint-src;
    name = "slint-cargo-vendor";
    hash = "sha256-BApQGhoyWTLzv/jpcQHITgFboC9CYk+3UiMCkzpfJKo=";
  };

  libcomms-src = fetchGithubSource {
    owner = "commschamp";
    repo = "comms";
    rev = "v5.5.2";
    hash = "sha256-U0KTpj3H+GcjiAPfKTl4h8MDZIZ3zZmQr6TpCdMk3bg=";
  };

  cocktail-maker-protocol-src = fetchGithubSource {
    owner = "mathisloge";
    repo = "cocktail-maker-protocol";
    rev = "main";
    hash = "sha256-qKadWeS0rFaMJ/5uftJHlMD3E8ykyBtC1gqlsXPnASw=";
  };

  libassert-src = fetchGithubSource {
    owner = "jeremy-rifkin";
    repo = "libassert";
    rev = "v2.2.1";
    hash = "sha256-ognudQ3NgpYxiDEucbIRWYQPs0XLRUQwg1eMxJm+aPs=";
  };

  catch2-src = fetchGithubSource {
    owner = "catchorg";
    repo = "Catch2";
    rev = "v3.15.1";
    hash = "sha256-JSMAlIDanPLzxhvFXeF3T5NQkj8Gye+bT92OjZS+XOs=";
  };
}
