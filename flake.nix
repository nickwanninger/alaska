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


          runInputs = with pkgs; [
            llvmPackages_16.libllvm
            llvmPackages_16.clang
            llvmPackages_16.stdenv
            llvmPackages_16.libunwind

            gllvm

            coreutils # For our bash scripts
            python3 # For running bin/alaska-transform
            file

            getconf
          ];

          buildInputs = with pkgs; runInputs ++ [
            cmake

            makeWrapper

            python3Packages.pip
            gdb ps which git
            zlib

            bashInteractive
          ];

        in
        with pkgs; {
          devShell = mkShell {
            buildInputs = buildInputs;

            shellHook = '' 
              source $PWD/enable
            '';
          };

          packages.default = pkgs.stdenv.mkDerivation {
            name = "alaska";
            nativeBuildInputs = [ cmake ];

            src = pkgs.nix-gitignore.gitignoreSource [] ./.;
            preConfigure = ''
              make defconfig
            '';


            buildInputs = buildInputs;

            postFixup = ''
              for b in $out/bin/*; do
                wrapProgram $b --set PATH ${lib.makeBinPath runInputs } \
                               --prefix PATH : $out/bin
              done
            '';


          };
        }
      );
}
