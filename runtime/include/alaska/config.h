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


// Now do other configuration!
#define ALASKA_LOCK_TRACKING // By default, enable lock tracking as some services *require* it


#define ALASKA_SQUEEZE_BITS 3
