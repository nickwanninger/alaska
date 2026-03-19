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




static void *_halloc(size_t sz, int zero) {
  void *result = get_tc()->halloc(sz);

  // This seems right...
  if (result == NULL) errno = ENOMEM;

  if (zero) {
    alaska::handle_memset(result, 0, sz);
  }

  auto *h = alaska::Mapping::from_handle_safe(result);
  if (h) {
    printf("halloc: %zu bytes -> %p -> %p\n", sz, result, h->get_pointer());
  }
  return result;
}


void *halloc(size_t sz) noexcept {
#ifdef MALLOC_BYPASS
  return ::malloc(sz);
#endif
  return alaska::halloc(sz);
  // return _halloc(sz, 0);
}

void *hcalloc(size_t nmemb, size_t size) {
#ifdef MALLOC_BYPASS
  return ::calloc(nmemb, size);
#endif
  return alaska::hcalloc(nmemb, size);
  // return _halloc(nmemb * size, 1);
}

// Reallocate a handle
void *hrealloc(void *handle, size_t new_size) {
#ifdef MALLOC_BYPASS
  return ::realloc(handle, new_size);
#endif
  return alaska::hrealloc(handle, new_size);
}



void hfree(void *ptr) {
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




static long seen = 0;

static long localize_structure_impl(alaska::Mapping *m, int depth, alaska::ThreadCache &tc) {
  long localized = 0;
  seen++;


  auto header = alaska::ObjectHeader::from(m);
  uint64_t *start = (uint64_t *)m->get_pointer();
  uint64_t *end = (uint64_t *)((char *)start + header->object_size());
  /*
  alaska::printf("%6ld ", seen);
  for (int i = 0; i < depth - 1; i++) alaska::printf("|  ");
  alaska::printf("|--");
  alaska::printf("%p,%p %zu | ", m, m->get_pointer(), header->object_size());
  // gray
  alaska::printf("\e[90m");
  for (uint64_t *p = start; p < end; p++) {
    alaska::printf("%016lx ", *p);
  }
  alaska::printf("\e[0m\n");
  */

  // if (depth > 2000) return localized;

  // then, walk its data

  if (!header->localized) {
    localized += (long)tc.localize(m, 0);
  }

  if (depth == 0) return localized;

  header = alaska::ObjectHeader::from(m);

  // header->walk([&](alaska::Mapping *om, alaska::ObjectHeader *oheader) {
  //   if (oheader->localized) return;
  //   localized += (long)tc.localize(om, 0);
  // });

  header->walk([&](alaska::Mapping *om, alaska::ObjectHeader *oheader) {
    localized += localize_structure_impl(om, depth - 1, tc);
  });
  return localized;
}

extern "C" bool localize_structure(void *ptr) {
  auto &rt = alaska::Runtime::get();

  // auto *tc = alaska::ThreadCache::current();
  // rt.with_barrier([&]() {
  //   rt.grade_heap();
  //   long localized = 0;
  //   rt.handle_table.for_each_handle([&](alaska::Mapping *m) {
  //     localized += localize_structure_impl(m, 2000, *tc);
  //   });
  //   alaska::printf("Localized %ld objects\n", localized);
  //   rt.grade_heap();
  // });

  rt.with_barrier([&]() {
    auto *tc = alaska::ThreadCache::current();
    auto *m = alaska::Mapping::from_handle(ptr);
    seen = 0;

    long loc_depth = 200;
    auto report_before = alaska::grade_locality(*m, 16);
    rt.grade_heap();

    auto localize_count = localize_structure_impl(m, loc_depth, *tc);
    alaska::printf("Localized %ld objects, seen = %lu\n", localize_count, seen);
    auto report_after = alaska::grade_locality(*m, 16);

    rt.grade_heap();

    alaska::printf("Locality report before:\n");
    report_before.dump();
    alaska::printf("Locality report after:\n");
    report_after.dump();
  });

  return true;
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
