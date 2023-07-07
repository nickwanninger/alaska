/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2023, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2023, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */
#include <assert.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <alaska.h>
#include <alaska/alaska.hpp>
#include <alaska/config.h>


/**
 * Note: This file is inlined by the compiler to make locks faster.
 * Do not declare any global variables here, as they may get overwritten
 * or duplciated needlessly. (Which can lead to linker errors)
 */

extern int alaska_verify_is_locally_locked(void *ptr);

static ALASKA_INLINE void alaska_track_hit(void) {
#ifdef ALASKA_TRACK_TRANSLATION_HITRATE
  alaska::record_translation_info(true);
  // alaska::translation_hits++;
#endif
}

static ALASKA_INLINE void alaska_track_miss(void) {
#ifdef ALASKA_TRACK_TRANSLATION_HITRATE
  alaska::record_translation_info(false);
  // alaska::translation_misses++;
#endif
}



void *alaska_translate(void *ptr) {
  // This function is written in a strange way on purpose. It's written
  // with a close understanding of how it will be lowered into LLVM IR
  // (mainly how it will be lowered into PHI instructions).
  //
  // On ARM64 with swapping disabled, this function compiles into only
  // three instructions after the branch:
  //
  //   tbz   x0, 63, skip         ; Skip translation if the top bit is 0
  //   lsr   x0, x0, SHIFT_AMT    ; Shift the handle mapping addr down
  //   ldr   x8, [x8]             ; Load the address from the mapping
  //   add   x0, x8, w0, uxtw     ; Add the low 32 bits to the address
  // skip:
  //   <use x0 as a pointer>      ; Now we can use it!
  //

  // Optionally, Sanity check that a handle has been locked on this thread
  // *before* translating. This ensures it won't be moved during
  // the invocation of this function
  // ALASKA_SANITY(alaska_verify_is_locally_locked(ptr),
  //     "Pointer '%p' is not locked on the shadow stack before calling translate\n", ptr);
  int64_t bits = (int64_t)ptr;

  // If the pointer is "greater than zero", then it is not a handle. This is because we rely on the
  // fact that most architectures have a "branch if less than 0" instruction to detect this. Most
  // arch just check the top bit to implement that, which is all we need!
  if (unlikely(bits >= 0)) {
    alaska_track_miss();
    return ptr;
  }

  alaska_track_hit();

  // Grab the mapping from the runtime
  auto m = alaska::Mapping::from_handle(ptr);
  // Pull the address from the mapping
  void *mapped = m->ptr;
#ifdef ALASKA_SWAP_SUPPORT
  // If swapping is enabled, the top bit will be set, so we need to check that
  if (unlikely(m->swap.flag)) {
    // Ask the runtime to "swap" the object back in. We blindly assume that
    // this will succeed for performance reasons.
    mapped = alaska_ensure_present(m);
  }
#endif
  ALASKA_SANITY(mapped != NULL, "Mapped pointer is null for handle %p\n", ptr);
  // Apply the offset to the mapping and return it.
  ptr = (void *)((uint64_t)mapped + (uint32_t)bits);
  return ptr;
}




void alaska_release(void *ptr) {
  // This function is just a marker that `ptr` is now dead (no longer used)
  // and should not have any real meaning in the runtime
}




static int needs_barrier = 0;
void alaska_time_hook_fire(void) {
  // printf("checking for barrier!\n");
  if (needs_barrier) {
    printf("needs barrier!\n");
  }
}
