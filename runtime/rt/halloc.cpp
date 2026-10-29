/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2024, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2024, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

// #define MALLOC_BYPASS

#ifdef MALLOC_BYPASS
#include <malloc.h>
#endif

#include <ck/stack.h>
#include <ck/queue.h>
#include <ck/vec.h>
#include <ck/set.h>
#include <alaska/core/Runtime.hpp>
#include <alaska/core/ThreadCache.hpp>
#include <alaska.h>
#include <errno.h>
#include <stdlib.h>




alaska::LockedThreadCache get_tc(void) { return *alaska::ThreadCache::current(); }



void *alaska_halloc(size_t sz) noexcept {
#ifdef MALLOC_BYPASS
  return ::malloc(sz);
#endif
  return alaska::halloc(sz);
}

void *alaska_hcalloc(size_t nmemb, size_t size) {
#ifdef MALLOC_BYPASS
  return ::calloc(nmemb, size);
#endif
  return alaska::hcalloc(nmemb, size);
}

// Reallocate a handle
void *alaska_hrealloc(void *handle, size_t new_size) {
#ifdef MALLOC_BYPASS
  return ::realloc(handle, new_size);
#endif
  return alaska::hrealloc(handle, new_size);
}



void alaska_hfree(void *ptr) {
#ifdef MALLOC_BYPASS
  return ::free(ptr);
#endif
  return alaska::hfree(ptr);
}




size_t alaska_usable_size(void *ptr) {
#ifdef MALLOC_BYPASS
  return ::malloc_usable_size(ptr);
#endif
  return alaska::halloc_usable_size(ptr);
}








// ----------- Translate hit/miss ------------ //


#define ENABLE_HITMISS_TRACKING

#ifdef ENABLE_HITMISS_TRACKING
struct HitMiss {
  const char *key;
  uint64_t hit;
  uint64_t miss;
  uint64_t originalNull;
};

static int compare_hitmiss(const void *a, const void *b) {
  const HitMiss *hm_a = (const HitMiss *)a;
  const HitMiss *hm_b = (const HitMiss *)b;

  // Sort by hits in descending order (more hits at top)
  if (hm_b->hit > hm_a->hit) return 1;
  if (hm_b->hit < hm_a->hit) return -1;

  // Secondary sort by misses in ascending order (fewer misses at top)
  if (hm_a->miss > hm_b->miss) return 1;
  if (hm_a->miss < hm_b->miss) return -1;

  return 0;
}

static ck::map<const char *, HitMiss> hitmiss;

extern "C" void __alaska_track_hitmiss(const char *key, uint64_t original, uint64_t result) {
  auto &hm = hitmiss[key];
  if (original == 0) hm.originalNull++;
  hm.key = key;

  if (original != result) {
    hm.hit++;
  } else {
    hm.miss++;
  }
}



__attribute__((destructor)) void __alaska_hitmiss_exit(void) {
  return;
  auto &rt = alaska::Runtime::get();
  auto faults = rt.handle_faults.read();
  auto fps = rt.handle_faults.digest();
  fprintf(stderr, "Handle faults: %lu (%.2f per second)\n", faults, fps);

  ck::vec<HitMiss> vhm;
  // dump hitmiss
  for (auto &[k, hm] : hitmiss) {
    vhm.push(hm);
  }

  // Sort by total hit+miss in descending order
  qsort(vhm.data(), vhm.size(), sizeof(HitMiss), compare_hitmiss);

  long handles = 0;
  long ptrs = 0;
  long mixes = 0;
  long total_branches = 0;
  long removed_branches = 0;

  FILE *profile_stream = fopen("alaska.hprof", "w");

  for (auto &hm : vhm) {
    printf("[ ");
    auto total = hm.hit + hm.miss;
    total_branches += total;

    char profile_result = '?';

    if (hm.hit == 0 && hm.miss != 0) {
      // all miss! (BLUE) (All pointers, no handles)
      printf("\e[34mPTR\e[0m");
      removed_branches += total;
      profile_result = 'P';
      ptrs++;
    } else if (hm.hit != 0 && hm.miss == 0) {
      // all hit! (GREEN) (All handles, no pointers)
      printf("\e[32mHDL\e[0m");
      handles++;
      profile_result = 'H';
      removed_branches += total;
    } else {
      // unknown! (RED)
      printf("\e[31mMIX\e[0m");
      profile_result = '?';
      mixes++;
    }
    printf(" ]");


    fprintf(profile_stream, "%c %s\n", profile_result, hm.key);

    printf("%12zu %12zu (%7.2f%%) %s\n", hm.hit, hm.miss, 100.0 * (hm.hit) / (float)total, hm.key);
  }

  printf("handles: %ld, ptrs: %ld, mixes: %ld\n", handles, ptrs, mixes);
  printf("total branches:   %16lu\n", total_branches);
  printf("removed branches: %16lu   (%7.2f%%)\n", removed_branches,
         100.0f * removed_branches / (float)total_branches);

  fclose(profile_stream);
}

#endif  // ENABLE_HITMISS_TRACKING
