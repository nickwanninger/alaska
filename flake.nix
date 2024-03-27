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
          buildInputs = with pkgs; [
            cmake

            python3 python3Packages.pip
            gdb ps which git
            zlib

            # Compiler dependencies
            llvmPackages_16.libllvm
            llvmPackages_16.clang
            llvmPackages_16.stdenv
            llvmPackages_16.libunwind

            gllvm

            bashInteractive
          ];
        in
        with pkgs; {
          devShell = mkShell {
            buildInputs = buildInputs;
          };

          packages.default = pkgs.stdenv.mkDerivation {
            name = "alaska";
            nativeBuildInputs = [ cmake ];

            src = pkgs.nix-gitignore.gitignoreSource [] ./.;
            preConfigure = ''
              make defconfig
            '';

            buildInputs = buildInputs;

          };
        }
      );
}
