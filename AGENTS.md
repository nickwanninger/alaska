# AGENTS.md

## Project At A Glance
- Alaska is a compiler plus runtime for handle-based memory management.
- Roots to know: `compiler` (LLVM pass plugin) and `runtime` (C++ runtime).
- Requires LLVM/Clang 21; helper scripts fetch the toolchain if you do not use Nix.

## Setup And Build
- `make` is usually enough to build.
- Handy targets: `make clean`, `make mrproper` for deeper cleanup.

## Using The Compiler
- Binary lands in `local/bin/alaska`.
- Example end-to-end: `local/bin/alaska -O3 test/sanity.c -o build/sanity && ./build/sanity`.

## Tests
- Runtime tests: `make test` or run `build/runtime/alaska_test`.
- Compiler suite: `make unit`.
- Quick smoke test: `make sanity`.

## Directory Notes
- `compiler/passes`, `compiler/lib`: transformation passes and support code.
- `runtime/core`, `runtime/rt`, `runtime/test`: runtime engine, C bindings, unit tests.
- `test/`: integration programs exercised by the compiler.

## Extra Tips
- `ccache` speeds up rebuilds automatically when available.
- Toggle debug info with `CMAKE_BUILD_TYPE=Debug` (enables extra logging).
- Yukon/RISC-V flow: run `tools/build_yukon.sh` from a fresh build directory with `RISCV=/opt/riscv`.
