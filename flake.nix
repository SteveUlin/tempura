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

        # Bloomberg's clang-p2996 - the reference implementation of C++26 reflection
        # https://github.com/bloomberg/clang-p2996
        clangP2996Src = pkgs.fetchFromGitHub {
          owner = "bloomberg";
          repo = "clang-p2996";
          rev = "2ea0a79fe7bb5f6fdb8c687ba0e21ab63696e7f7";
          hash = "sha256-uvAAYsUH+4WJTsf7/kB0v2ZsiVj1pekQ0f/pJwXU1rI=";
        };

        # Build clang-p2996 with reflection support
        # This is a large build (~30-60 min on first run)
        clangP2996 = pkgs.stdenv.mkDerivation {
          pname = "clang-p2996";
          version = "19.0.0-p2996";
          src = clangP2996Src;

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            python3
            perl
          ];

          buildInputs = with pkgs; [
            zlib
            ncurses
            libxml2
          ];

          # Build from llvm subdirectory, enable clang + clang-tools-extra (clangd)
          # libc++ headers are provided separately by p2996LibcxxHeaders
          cmakeDir = "../llvm";
          cmakeFlags = [
            "-DLLVM_ENABLE_PROJECTS=clang;clang-tools-extra"
            "-DLLVM_TARGETS_TO_BUILD=X86"
            "-DCMAKE_BUILD_TYPE=Release"
            "-DLLVM_ENABLE_ASSERTIONS=OFF"
            "-DLLVM_INCLUDE_TESTS=OFF"
            "-DLLVM_INCLUDE_EXAMPLES=OFF"
            "-DLLVM_INCLUDE_BENCHMARKS=OFF"
            "-DCLANG_INCLUDE_TESTS=OFF"
          ];

          meta = with pkgs.lib; {
            description = "Clang with C++26 P2996 reflection support (Bloomberg fork)";
            homepage = "https://github.com/bloomberg/clang-p2996";
            license = licenses.asl20;
            platforms = platforms.linux;
          };
        };

        # NixOS wrapper for clang-p2996 — adds glibc/CRT/linker paths.
        # NixOS doesn't put system headers at /usr/include or CRT objects at
        # /usr/lib, so bare clang can't compile anything. This trivial wrapper
        # injects the nix store paths into every clang invocation.
        clangP2996NixWrapper = pkgs.runCommand "clang-p2996-nix" {} ''
          mkdir -p $out/bin

          cat > $out/bin/clang <<'ENDSCRIPT'
#!/bin/sh
exec ${clangP2996}/bin/clang \
  -isystem ${pkgs.glibc.dev}/include \
  -isystem ${pkgs.linuxHeaders}/include \
  -B${pkgs.glibc}/lib \
  -B${pkgs.gcc15.cc}/lib/gcc/x86_64-unknown-linux-gnu/15.2.0 \
  -L${pkgs.glibc}/lib \
  -L${pkgs.gcc15.cc.lib}/lib \
  -L${pkgs.gcc15.cc}/lib/gcc/x86_64-unknown-linux-gnu/15.2.0 \
  -Wl,--dynamic-linker=${pkgs.glibc}/lib/ld-linux-x86-64.so.2 \
  "$@"
ENDSCRIPT
          chmod +x $out/bin/clang

          cat > $out/bin/clang++ <<'ENDSCRIPT'
#!/bin/sh
exec ${clangP2996}/bin/clang++ \
  -isystem ${pkgs.glibc.dev}/include \
  -isystem ${pkgs.linuxHeaders}/include \
  -B${pkgs.glibc}/lib \
  -B${pkgs.gcc15.cc}/lib/gcc/x86_64-unknown-linux-gnu/15.2.0 \
  -L${pkgs.glibc}/lib \
  -L${pkgs.gcc15.cc.lib}/lib \
  -L${pkgs.gcc15.cc}/lib/gcc/x86_64-unknown-linux-gnu/15.2.0 \
  -Wl,--dynamic-linker=${pkgs.glibc}/lib/ld-linux-x86-64.so.2 \
  "$@"
ENDSCRIPT
          chmod +x $out/bin/clang++
        '';

        # Build libc++ from P2996 source — includes <meta> and <experimental/meta>
        # for C++26 reflection. Two-stage: clang (already cached) → libc++ (~10 min).
        p2996Libcxx = pkgs.stdenv.mkDerivation {
          pname = "p2996-libcxx";
          version = "19.0.0-p2996";
          src = clangP2996Src;

          nativeBuildInputs = with pkgs; [ cmake ninja python3 ];

          # Build from the runtimes/ subdirectory (LLVM's standalone runtime build)
          cmakeDir = "../runtimes";
          cmakeFlags = [
            "-DLLVM_ENABLE_RUNTIMES=libcxx;libcxxabi"
            "-DCMAKE_C_COMPILER=${clangP2996NixWrapper}/bin/clang"
            "-DCMAKE_CXX_COMPILER=${clangP2996NixWrapper}/bin/clang++"
            "-DLIBCXXABI_USE_LLVM_UNWINDER=OFF"
            "-DCMAKE_BUILD_TYPE=Release"
            "-DLIBCXX_INCLUDE_BENCHMARKS=OFF"
            "-DLIBCXX_INCLUDE_TESTS=OFF"
            "-DLIBCXXABI_INCLUDE_TESTS=OFF"
          ];

          meta = with pkgs.lib; {
            description = "libc++ with P2996 reflection <meta> header";
            license = licenses.asl20;
            platforms = platforms.linux;
          };
        };

        # GCC trunk source (without reflection - keeping for reference)
        gccTrunkSrc = pkgs.fetchgit {
          url = "git://gcc.gnu.org/git/gcc.git";
          rev = "refs/heads/trunk";
          hash = "sha256-kbZWlrLvi9v8yniIgo/3YOftnLPNV6bu0IlwtgHByQk=";
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
          clang-p2996 = clangP2996;
          p2996-libcxx = p2996Libcxx;
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

        # GCC trunk shell (C++26, no reflection)
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
              echo "🔬 GCC Trunk (C++26 without reflection)"
              echo "   g++ --version: $(g++ --version | head -1)"
              echo ""
              echo "   Note: Full P2996 reflection requires clang-p2996"
              echo "   Use: nix develop .#reflection"
            '';
          };

        # Clang P2996 shell with full reflection support
        # Uses clang-p2996 with libc++ built from P2996 source (includes <meta>)
        devShells.reflection =
          pkgs.mkShell {
            name = "Tempura-Reflection";

            packages = [
              clangP2996
              pkgs.gcc15  # Provides crtbeginS.o/crtendS.o (GCC startup objects)
              pkgs.gdb
              pkgs.cmake
              pkgs.ninja
              pkgs.jq
              pkgs.python3
            ];

            # libc++ from P2996 source provides the C++ standard library AND <meta>.
            env = {
              # P2996 libc++ paths (for cmake toolchain)
              P2996_LIBCXX_INCLUDE = "${p2996Libcxx}/include/c++/v1";
              P2996_LIBCXX_LIB = "${p2996Libcxx}/lib";
              # NixOS CRT/linker paths for clang
              CLANG_CRT_GLIBC = "${pkgs.glibc}/lib";
              CLANG_CRT_GCC = "${pkgs.gcc15.cc}/lib/gcc/x86_64-unknown-linux-gnu/15.2.0";
              GCC_LIB_PATH = "${pkgs.gcc15.cc.lib}/lib";
              NIX_DYNAMIC_LINKER = "${pkgs.glibc}/lib/ld-linux-x86-64.so.2";
              # C system headers for NixOS (glibc + linux) — used by cmake toolchain
              GLIBC_INCLUDE = "${pkgs.glibc.dev}/include";
              LINUX_INCLUDE = "${pkgs.linuxHeaders}/include";
              # Runtime library paths
              LD_LIBRARY_PATH = "${p2996Libcxx}/lib:${pkgs.gcc15.cc.lib}/lib";
            };

            shellHook = ''
              echo "🪞 Clang P2996 with C++26 reflection support"
              echo "   clang++ --version: $(clang++ --version | head -1)"
              echo "   libc++: from P2996 source | <meta>: included in libc++"
              echo ""
              echo "   Usage: cmake --preset clang-p2996 && cmake --build build-reflection"
              echo ""
              echo "   Reflection operators available:"
              echo "     ^^T       - reflect on type T (Bloomberg syntax)"
              echo "     [:r:]     - splice reflection r back to code"
              echo "     template for - compile-time expansion loop"
              echo ""

              # Ad-hoc compilation wrapper — cmake users don't need this
              clang-reflect() {
                clang++ -std=c++26 -freflection-latest \
                  -stdlib=libc++ -nostdinc++ \
                  -isystem "$P2996_LIBCXX_INCLUDE" \
                  -L"$P2996_LIBCXX_LIB" -Wl,-rpath,"$P2996_LIBCXX_LIB" \
                  -B"$CLANG_CRT_GLIBC" -B"$CLANG_CRT_GCC" \
                  -Wl,--dynamic-linker="$NIX_DYNAMIC_LINKER" \
                  "$@"
              }
              export -f clang-reflect
              echo "   Wrapper: clang-reflect <flags> file.cpp"
            '';
          };
      }
    );
}
