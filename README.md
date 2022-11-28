<div style="text-align:center"><img src="https://i.imgur.com/SOLIBp6.png"/></div>

# Alaska
A compiler and runtime framework for handle based memory management.


## Building

The build scripts will download and build all the needed dependencies. First, you must configure the project with `make menuconfig`. Once you save your configuration (the default config is a safe bet), run `make deps` to build alaska's dependencies, then `make` to build the project:
```
make menuconfig
make deps
make
```

Once it finishes, `local/` should contain the tools required to use alaska.


## Using Alaska

Once built, `local/bin/alaska` functions as a drop-in replacement for `clang`. You can test alaska with the following commands:
```
local/bin/alaska -O3 test/locality.c -o build/locality && build/locality
```
