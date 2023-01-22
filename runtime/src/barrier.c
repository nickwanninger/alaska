#include <alaska.h>
#include <alaska/internal.h>

extern uint64_t alaska_next_usage_timestamp;

extern void* alaska_defrag(alaska_mapping_t* ent);

extern void trace_defrag_hook(void* oldptr, void* newptr, size_t size);


void (*alaska_barrier_hook)(void*, void*, size_t) = NULL;


// This just means that the application has decided it is willing to let alaska
// potentially stop the world and do what it needs to do. This function doesn't
// do anything yet, but it could :)
__declspec(noinline) void alaska_barrier(void) {
  size_t hit = 0;     // how many allocations have moved?
  size_t miss = 0;    // how many allocations didn't move?
  size_t locked = 0;  // how many locked allocations were skipped?
  size_t total = 0;   // how many allocations were considered?
  float rss_before = alaska_get_rss_kb() / 1024.0;

  // This is REALLY inefficient. We walk over *all* the handles even if they aren't allocated
  for (alaska_mapping_t* cur = alaska_table_begin(); cur != alaska_table_end(); cur++) {
    if (cur->size == -1 || cur->ptr == NULL) {
      continue;
    }

    total++;
    if (cur->locks != 0) {
      locked++;
      continue;
    }

    void* ptr = cur->ptr;
    void* newptr = alaska_defrag(cur);
    if (newptr != ptr) {
      if (alaska_barrier_hook) alaska_barrier_hook(ptr, newptr, cur->size);
      hit++;
    } else {
      miss++;
    }
  }
  float rss_after = alaska_get_rss_kb() / 1024.0;
  printf("[barrier] hit: %-7zu miss: %-7zu locked: %-7zu(%6.2f%% hitrate)  save: %.3f MB (%6.2f%% from %.3f MB)\n", hit,
      miss, locked, (float)hit / (float)total * 100, rss_before - rss_after, (1 - (rss_after / rss_before)) * 100.0,
      rss_before);
}
