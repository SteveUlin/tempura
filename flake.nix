{
  description = "C++ Libary & Examples";

  inputs = {
    nixpkgs.url = "github:NixOs/nixpkgs/nixos-unstable";

    flake-utils = {
      url = "github:numtide/flake-utils";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = import nixpkgs {
        inherit system;
      };
      in {
        devShells.default = pkgs.mkShell.override {
            stdenv = pkgs.llvmPackages_18.libcxxStdenv;
        } {
          name = "Tempura";

          packages = with pkgs; [
            bazel
            cmake
            ninja
          ];

          shellHook = ''
            export CMAKE_GENERATOR="Ninja";
          '';
        };
      }
    );
}
