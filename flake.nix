{
  description = "C++ Libary & Examples";

  inputs = {
    nixpkgs.url = "github:NixOs/nixpkgs/nixos-unstable";

    flake-utils = {
      url = "github:numtide/flake-utils";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
          config.cudaSupport = true;
        };
      in {
        devShells.default =
          pkgs.mkShell.override {
            stdenv = pkgs.gcc15Stdenv;
          } rec {
            name = "Tempura";

            packages = with pkgs; [
              pkgs.llvmPackages_20.clang
              gcc15
              gdb
              bazel
              cmake
              ninja
              cudaPackages_12_1.cudatoolkit
              cudaPackages_12_1.cuda_cudart
              linuxPackages.nvidia_x11
              glfw
              glm
              libGL
              vulkan-headers
              vulkan-loader
              vulkan-validation-layers
              jq
              python3
            ];

            env = {
              LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath packages;
              CUDA_HOME = pkgs.cudaPackages_12_1.cudatoolkit;
              CUDA_PATH = pkgs.cudaPackages_12_1.cudatoolkit;
              CMAKE_CUDA_COMPILER = "${pkgs.cudaPackages_12_1.cudatoolkit}/bin/nvcc";
              NIX_ENFORCE_NO_NATIVE = ""; # Allow -march=native optimizations
            };
          };
      }
    );
}
