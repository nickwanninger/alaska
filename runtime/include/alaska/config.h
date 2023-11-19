#pragma once

// Include the autoconf.h header from menuconfig.
#include "./autoconf.h"


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

// The `none` configuration is pretty simple, and is a good starting point
// for when you want to make new services in the future. It's a good idea
// to copy paste it :)
#ifdef ALASKA_SERVICE_NONE
#define ALASKA_REPLACE_MALLOC
#endif


#ifdef ALASKA_SERVICE_ANCHORAGE
#define ALASKA_REPLACE_MALLOC
#endif

#ifdef ALASKA_SERVICE_JEMALLOC
#define ALASKA_REPLACE_MALLOC
#endif


#ifdef ALASKA_SERVICE_CORDOVA
// Do not optimize translations
#define ALASKA_UNOPT_TRANSLATIONS
#endif
