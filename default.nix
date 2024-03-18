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
    # autoconf automake cmake
    # coreutils moreutils binutils
    cmake

    libunwind
    zlib

    python3

    # General
    # python3 patch git wget
    # python3 python3Packages.pip
    #
    # gdb ps which


    # Compiler dependencies
    llvmPackages_16.libllvm
    llvmPackages_16.clang
    llvmPackages_16.stdenv
    llvmPackages_16.libunwind

    gllvm

    bashInteractive
  ];

  propagatedBuildInputs = with pkgs; [
    python3
    llvmPackages_16.libllvm
    llvmPackages_16.clang
    llvmPackages_16.stdenv
    llvmPackages_16.libunwind
    gllvm
  ];

  hardeningDisable = [ "all" ];

  shellHook = ''
    # export PATH=$PWD/local/bin:$PATH
    # export LD_LIBRARY_PATH=$PWD/local/lib:$LD_LIBRARY_PATH
  '';
}
