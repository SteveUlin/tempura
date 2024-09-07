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
        config.allowUnfree = true;
        config.cudaSupport = true;
      };
      in {
        devShells.default = pkgs.mkShell.override {
        stdenv = pkgs.llvmPackages_18.libcxxStdenv;
      } rec {
          name = "Tempura";


          packages = with pkgs; [
            pkgs.llvmPackages_18.clang
            gcc12
            gdb
            bazel
            cmake
            ninja
            cudaPackages_12_1.cudatoolkit
            cudaPackages_12_1.cuda_cudart
            linuxPackages.nvidia_x11
          ];
          
          env = {
            LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath packages;
            CUDA_HOME = pkgs.cudaPackages_12_1.cudatoolkit;
            CUDA_PATH = pkgs.cudaPackages_12_1.cudatoolkit;
            CMAKE_CUDA_COMPILER = "${pkgs.cudaPackages_12_1.cudatoolkit}/bin/nvcc";
          };
        };
      }
    );
}
