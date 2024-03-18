{
  description = "The Alaska Handle-Based Memory Management System";

  inputs = {
    nixpkgs.url = "nixpkgs/nixos-23.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem
      (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          devShell = with pkgs; pkgs.mkShell {
            buildInputs = [
              cmake

              libunwind


              python3 python3Packages.pip
              gdb ps which git

              # Compiler dependencies
              llvmPackages_16.libllvm
              llvmPackages_16.clang
              llvmPackages_16.stdenv
              llvmPackages_16.libunwind

              gllvm

              bashInteractive
            ];
          };
        }
      );
}
