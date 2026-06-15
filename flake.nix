{
  description = "C++ Libary & Examples";

  inputs = {
    nixpkgs.url = "github:NixOs/nixpkgs/nixos-unstable";

    flake-utils.url = "github:numtide/flake-utils";
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

        # GCC trunk source (includes P2996 reflection, merged January 2026)
        gccTrunkSrc = pkgs.fetchgit {
          url = "git://gcc.gnu.org/git/gcc.git";
          rev = "refs/heads/trunk";
          hash = "sha256-EYCcEVw6WOt5sEldS9jton5MD9I0vh3YvjiMN/ETcTE=";
          fetchSubmodules = false;
        };

        # Override GCC unwrapped, then wrap it
        gccTrunkUnwrapped = pkgs.gcc15.cc.overrideAttrs (old: {
          pname = "gcc-trunk";
          version = "16.0.1";
          src = gccTrunkSrc;
          patches = [];

          nativeBuildInputs = (old.nativeBuildInputs or []) ++ [
            pkgs.file
            pkgs.flex
            pkgs.bison
            pkgs.perl
            pkgs.texinfo
          ];

          postPatch = ''
            configureScripts=$(find . -name configure)
            for configureScript in $configureScripts; do
              patchShebangs $configureScript
            done

            echo "fixing the {GLIBC,UCLIBC,MUSL}_DYNAMIC_LINKER macros..."
            for header in "gcc/config/"*-gnu.h "gcc/config/"*"/"*.h
            do
              grep -q _DYNAMIC_LINKER "$header" || continue
              echo "  fixing $header..."
              sed -i "$header" \
                  -e 's|define[[:blank:]]*\([UCG]\+\)LIBC_DYNAMIC_LINKER\([0-9]*\)[[:blank:]]"\([^\"]\+\)"$|define \1LIBC_DYNAMIC_LINKER\2 "${pkgs.glibc.out}\3"|g' \
                  -e 's|define[[:blank:]]*MUSL_DYNAMIC_LINKER\([0-9]*\)[[:blank:]]"\([^\"]\+\)"$|define MUSL_DYNAMIC_LINKER\1 "${pkgs.musl.out or pkgs.glibc.out}\2"|g'
            done
          '';
        });

        gccTrunk = pkgs.wrapCCWith {
          cc = gccTrunkUnwrapped;
          libc = pkgs.glibc;
        };

        gccTrunkStdenv = pkgs.overrideCC pkgs.stdenv gccTrunk;

      in {
        packages = {
          gcc-trunk = gccTrunk;
        };

        # Default shell uses GCC 15 (stable)
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
              cudaPackages_12_6.cudatoolkit
              cudaPackages_12_6.cuda_cudart
              linuxPackages.nvidia_x11
              glfw
              glm
              libGL
              vulkan-headers
              vulkan-loader
              vulkan-validation-layers
              jq
              python3
              liburing
            ];

            env = {
              LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath packages;
              CUDA_HOME = pkgs.cudaPackages_12_6.cudatoolkit;
              CUDA_PATH = pkgs.cudaPackages_12_6.cudatoolkit;
              CMAKE_CUDA_COMPILER = "${pkgs.cudaPackages_12_6.cudatoolkit}/bin/nvcc";
              NIX_ENFORCE_NO_NATIVE = "";
            };
          };

        # GCC trunk shell (C++26 with P2996 reflection)
        devShells.trunk =
          pkgs.mkShell.override {
            stdenv = gccTrunkStdenv;
          } rec {
            name = "Tempura-Trunk";

            packages = with pkgs; [
              gccTrunk
              gdb
              cmake
              ninja
              jq
              python3
              liburing
            ];

            env = {
              LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath packages;
              NIX_ENFORCE_NO_NATIVE = "";
            };

            shellHook = ''
              echo "🔬 GCC Trunk (C++26 with P2996 reflection)"
              echo "   g++ --version: $(g++ --version | head -1)"
              echo ""
              echo "   Usage: cmake --preset gcc-trunk && cmake --build build-gcc-trunk"
              echo "   Reflection flag: -freflection (set by toolchain)"
            '';
          };

      }
    );
}
