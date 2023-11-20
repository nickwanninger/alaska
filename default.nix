{ pkgs ? import <nixpkgs> {} }:

with pkgs.lib;




pkgs.stdenv.mkDerivation {
  name = "alaska";

  nativeBuildInputs = with pkgs; [ cmake ];

  src = pkgs.nix-gitignore.gitignoreSource [] ./.;


  # build a default configuration
  preConfigure = ''
    make defconfig
  '';


  buildInputs = with pkgs; [
    autoconf automake cmake
    coreutils moreutils binutils
    cmake libunwind

    # General
    python3 patch git wget
    python3 python3Packages.pip

    gdb ps which


    # Compiler dependencies
    llvmPackages_15.libllvm
    llvmPackages_15.clang
    llvmPackages_15.stdenv
    llvmPackages_15.libunwind

    gllvm

    bashInteractive
  ];

  hardeningDisable = [ "all" ];

  shellHook = ''
    # export PATH=$PWD/local/bin:$PATH
    # export LD_LIBRARY_PATH=$PWD/local/lib:$LD_LIBRARY_PATH
  '';
}
