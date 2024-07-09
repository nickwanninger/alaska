{
  description = "The Alaska Handle-Based Memory Management System";

  inputs = {
    nixpkgs.url = "nixpkgs/nixos-24.05";
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
            llvmPackages_16.openmp

            (gllvm.overrideAttrs {
              doCheck = false;
            })

            coreutils # For our bash scripts
            python3 # For running bin/alaska-transform
            file

            getconf
          ];

          buildInputs = with pkgs; runInputs ++ [
            cmake


            gtest
            makeWrapper

            python3Packages.pip
            gdb ps which git
            zlib
            wget

            bashInteractive
          ];

        in
        with pkgs; {
          devShell = mkShell {
            inherit buildInputs;


            LOCALE_ARCHIVE = "${glibcLocales}/lib/locale/locale-archive";
            hardeningDisable = ["all"];

            shellHook = ''
              source $PWD/enable
            '';
          };

          packages.default = pkgs.stdenv.mkDerivation {
            name = "alaska";
            nativeBuildInputs = [ cmake ];

            src = pkgs.nix-gitignore.gitignoreSource [] ./.;

            cmakeFlags = [
              "-DCMAKE_C_COMPILER=clang"
              "-DCMAKE_CXX_COMPILER=clang++"
            ];
            preConfigure = ''
              make defconfig
            '';


            inherit buildInputs;

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
