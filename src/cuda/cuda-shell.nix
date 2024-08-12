# Run with `nix-shell cuda-shell.nix`
{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell rec {
 name = "cuda-env-shell";
  packages = with pkgs; [
    clang_18
    cudaPackages_11_7.cudatoolkit
    cudaPackages_11_7.cuda_cudart
    linuxPackages.nvidia_x11
    stdenv.cc.libcxx
  ];
  
  shellHook = ''
      export LD_LIBRARY_PATH="${pkgs.lib.makeLibraryPath packages}:$LD_LIBRARY_PATH"
      export CUDA_DIR="${pkgs.cudaPackages_11_7.cudatoolkit}"
   '';          
}
