{
  description = "The Alaska Handle-Based Memory Management System";

  inputs = {
    nixpkgs.url = "nixpkgs/nixos-25.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem
      (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};


          runInputs = with pkgs; [
            llvmPackages_21.libllvm
            llvmPackages_21.clang # -unwrapped
            llvmPackages_21.clang-unwrapped
            llvmPackages_21.stdenv
            llvmPackages_21.libunwind
            llvmPackages_21.mlir
            # llvmPackages_21.libcxxClang
            llvmPackages_21.openmp

            (gllvm.overrideAttrs {
              doCheck = false;
            })

            valgrind
            valgrind.dev

            coreutils # For our bash scripts
            python3 # For running bin/alaska-transform
            file

            gcc-unwrapped


            getconf
          ];

          buildInputs = with pkgs; runInputs ++ [
            cmake
            ccache

            gtest
            makeWrapper

            python3Packages.pip
            gdb ps which git
            zlib
            zstd
            wget

            bashInteractive
          ];

        in
        with pkgs; {
          devShell = mkShell {
            inherit buildInputs;


            LOCALE_ARCHIVE = "${glibcLocales}/lib/locale/locale-archive";
            hardeningDisable = ["all"];

            # LD_LIBRARY_PATH = "${pkgs.gcc-unwrapped.lib}/lib";

            shellHook = ''
              unset NIX_ENFORCE_NO_NATIVE
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


            inherit buildInputs;

            preBuild = ''
              export CCACHE_DISABLE=1
            '';

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
