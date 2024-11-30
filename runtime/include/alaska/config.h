#pragma once

// Include the autoconf.h header from menuconfig.

#ifdef __amd64__
// On x86, we simply use `ud2` to trigger a SIGILL
#define ALASKA_PATCH_SIZE 2
#endif


#ifdef __aarch64__
// All arm64 instructions are 4 bytes
#define ALASKA_PATCH_SIZE 4
#endif

#ifdef __riscv
// On riscv, we patch using the `unimp` instruction, a compressed
// instruction which has the bytes `0x0000`
#define ALASKA_PATCH_SIZE 2
#endif

#define ALASKA_SQUEEZE_BITS 3
