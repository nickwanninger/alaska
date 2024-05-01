<div style="text-align:center"><img src="https://i.imgur.com/SOLIBp6.png"/></div>

# Alaska
A compiler and runtime framework for handle based memory management.

## Releases + Development

This project is under active development, and APIs are still in flux.
If you would like to get the version used in our [ASPLOS'24 Paper](https://nickw.io/papers/asplos24-alaska.pdf), you can access it through the tagged [release](https://github.com/nickwanninger/alaska/releases/tag/asplos-24-revision).

Otherwise, this project uses `main` as a pseudo "stable" branch (for some definition of that word).
The `dev` branch is where most of the development is based.
`main` is protected and cannot be pushed to without a PR.

## Building

Alaska requires two main dependencies: `clang`, `llvm 16` and [`gclang`](https://github.com/SRI-CSL/gllvm).
We provide both a nix flake, and tools to download these dependencies for you, but if you already have them in your path you don't need to worry.
You can test this by simply running `make`, and you may get this output:

```
...
-- Detecting CXX compile features
-- Detecting CXX compile features - done
CMake Error at CMakeLists.txt:36 (find_package):
  Could not find a configuration file for package "LLVM" that is compatible
  with requested version "16".

  The following configuration files were considered but not accepted:

    /usr/lib/llvm-14/cmake/LLVMConfig.cmake, version: 14.0.0
    /lib/llvm-14/cmake/LLVMConfig.cmake, version: 14.0.0

...
```

If this is the case, CMake was unable to find clang 16 in your environment, and you should use one of the following steps.

### Dependencies with `nix`

We prefer to manage Alaska's dependencies through [nix](https://nixos.org/) if you have it installed.
The main way to develop alaska with `nix` is to use flakes, and the following commands can be used to build alaska:
```bash
$ nix develop
# in the development shell, with with all the dependencies installed
$ make
```

Alternatively, you can run `nix shell .` to automatically compile alaska and include it in your shell.


### Dependencies without `nix`

Alaska can manage it's own dependencies by simply running `make deps`.
This will download and install a copy of `clang-16`, as well as a copy of `gclang` into `./local` automatically.
Once this is done, re-run `make` to compile alaska.



---


## Using Alaska

Once built, `local/bin/alaska` functions as a drop-in replacement for `clang`. You can test alaska with the following commands:
```
local/bin/alaska -O3 test/sanity.c -o build/sanity && build/sanity
```
You can also run `make sanity`, which will do it for you.

## Developing
