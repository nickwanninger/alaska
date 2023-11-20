{ pkgs ? import <nixpkgs> {} }:

with pkgs.lib;

pkgs.mkShell {
  buildInputs = with pkgs; [
    autoconf automake cmake
    coreutils moreutils binutils
    cmake libunwind

    # General
    python3 patch git wget
    python3 python3Packages.pip

    gdb ps which

    bashInteractive
  ];

  hardeningDisable = [ "all" ];

  shellHook = ''
    export PATH=$PWD/local/bin:$PATH
    export LD_LIBRARY_PATH=$PWD/local/lib:$LD_LIBRARY_PATH
  '';
}
