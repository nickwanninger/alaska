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

#include <alaska/core/ThreadCache.hpp>
#include <alaska/util/Logger.hpp>
#include <alaska/core/Runtime.hpp>
#include <alaska/heaps/SizeClass.hpp>
#include <alaska/alaska.hpp>
#include "alaska/heaps/Heap.hpp"
#include "alaska/heaps/HeapPage.hpp"
#include <alaska/util/utils.h>
#include <alaska/util/lphash_set.h>
#include <alaska/heaps/HugeAllocator.hpp>

#include <execinfo.h>


namespace alaska {

  ThreadCache::ThreadCache(int id, alaska::Runtime &rt)
      : id(id)
      , runtime(rt) {
    this->current_slab = runtime.handle_table.fresh_slab();
  }

  ThreadCache::~ThreadCache() {
  }








  void ThreadCache::maybe_collect(size_t size) {
  }


  alaska::ObjectHeader *ThreadCache::allocate_object(size_t size, alaska::Mapping &m) {
    return allocate_object_generic(size, m);
  }

  __attribute__((noinline)) alaska::ObjectHeader *ThreadCache::allocate_object_generic(
      size_t size, alaska::Mapping &m) {
    if (active_block == nullptr) active_block = runtime.arena_heap.newBlock();
    auto *header = active_block ? active_block->allocate(size) : nullptr;
    if (header == nullptr) {
      active_block = runtime.arena_heap.newBlock();
      header = active_block ? active_block->allocate(size) : nullptr;
    }
    if (header == nullptr) return nullptr;
    header->set_mapping(&m);
    m.set_pointer(header->data());
    return header;
  }



  __attribute__((noinline)) void *ThreadCache::halloc_generic_empty_ht(size_t size) {
    return halloc_generic(size, *new_mapping_generic());
  }


  // noinline
  __attribute__((noinline)) void *ThreadCache::halloc_generic(size_t size, alaska::Mapping &m) {
    FTR_SCOPE("HallocGeneric");
    // Now, if we are being called here, it means either we are
    // allocating a large object (size>1024) or one of the following
    // checks failed in ::halloc.
    if (unlikely(size == 0)) return NULL;
    void *result = nullptr;

    maybe_collect(size);

    ALASKA_ASSERT(size < alaska::max_large_size,
                  "HallocGeneric should only be called for small allocations");

    if (likely(size >= alaska::max_large_size)) {
      result = runtime.huge_allocator.alloc(size);
    } else {
      if (active_block == nullptr) active_block = runtime.arena_heap.newBlock();
      auto *header = active_block ? active_block->allocate(size) : nullptr;
      if (header == nullptr) {
        active_block = runtime.arena_heap.newBlock();
        header = active_block ? active_block->allocate(size) : nullptr;
      }
      if (unlikely(header == nullptr)) {
        free_mapping(&m);
        return NULL;
      }
      header->set_mapping(&m);
      m.set_pointer(header->data());
      result = m.to_handle(0);
    }

    return result;
  }



  LTO_INLINE void *ThreadCache::halloc(size_t size) {
    FTR_SCOPE("Halloc");

    if (unlikely(size >= alaska::max_large_size)) {
      return runtime.huge_allocator.alloc(size);
    }



    // Allocate a mapping. This *must* succeed (it is an error to return null).
    // auto *mapping = new_mapping();
    auto slab = this->current_slab;
    auto &htfl = slab->get_freelist();
    auto *mp = TC_ALIGNED(htfl.peek());
    if (unlikely(mp == nullptr)) {
      return halloc_generic_empty_ht(size);
    }


    htfl.pop_unchecked(mp);
    auto *mapping = (alaska::Mapping *)mp;

    if (active_block != nullptr) {
      auto *header = active_block->allocate(size);
      if (header != nullptr) {
        header->set_mapping(mapping);
        mapping->set_pointer(header->data());
        return mapping->to_handle(0);
      }
    }
    return halloc_generic(size, *mapping);
  }




  LTO_INLINE void *ThreadCache::hrealloc(void *handle, size_t new_size) {
    // TODO: There is a race here... I think its okay, as a realloc really should
    // be treated like a UAF, and ideally another thread would not access the handle
    // while it is being reallocated.
    alaska::Mapping *m = alaska::Mapping::from_handle_safe(handle);

    auto original_size = this->get_size(handle);
    // alaska::printf("ThreadCache::hrealloc: handle=%p, sz %zu -> %zu (%d -> %d)\n", handle,
    //                original_size, new_size, alaska::size_to_class(original_size),
    //                alaska::size_to_class(new_size));

    void *new_handle = this->halloc(new_size);

    // We should copy the minimum of the two sizes between the allocations.
    size_t copy_size = original_size > new_size ? new_size : original_size;

    handle_memcpy(new_handle, handle, copy_size);

    this->hfree(handle);

    return new_handle;
  }



  LTO_INLINE void ThreadCache::hfree(void *handle) {
    FTR_SCOPE("hfree");
    alaska::Mapping *m = FTR_EXPR("GetMapping", alaska::Mapping::from_handle_safe(handle));

    // The first case in hfree is handling huge allocations.
    // These allocations are not tracked in the handle table, so we
    // need to handle them by calling out to the huge allocator.
    // This is behind an unlikely check because it is expected that these
    // are relatively rare.

    if (unlikely(m == nullptr)) {
      FTR_SCOPE("NonHandle");
      runtime.huge_allocator.free(handle);
      return;
    }

    // --- Free the data allocation --- //
    void *ptr = m->get_pointer();

    auto *handle_slab = FTR_EXPR("GetSlab", this->runtime.handle_table.get_slab(m));

    auto *header = alaska::ObjectHeader::from(ptr);
    alaska::get_arena_block(ptr)->free(header);

    // Return the handle to the slab using thread-safe atomic operations.
    // This works correctly even if freed from a different thread than allocation.
    FTR_SCOPE("HandleSlabFree");
    handle_slab->free(m);
    return;
  }

  // #define STUB_ALLOCATES_HANDLES


  LTO_INLINE alaska::Mapping *ThreadCache::reverse_lookup(void *heap_ptr) {
    if (this->runtime.heap.contains(heap_ptr)) {
      return ObjectHeader::from(heap_ptr)->get_mapping();
    }
    return nullptr;
  }

  // -------------------------------------------------------------- //
  __attribute__((noinline)) void *ThreadCache::malloc_generic(size_t size, alaska::Mapping &m) {
    if (unlikely(size == 0)) return NULL;

    maybe_collect(size);

    if (unlikely(size >= alaska::max_large_size)) {
      return runtime.huge_allocator.alloc(size);
    }

    if (active_block == nullptr) active_block = runtime.arena_heap.newBlock();
    auto *header = active_block ? active_block->allocate(size) : nullptr;
    if (header == nullptr) {
      active_block = runtime.arena_heap.newBlock();
      header = active_block ? active_block->allocate(size) : nullptr;
    }
    if (unlikely(header == nullptr)) return nullptr;
    header->set_mapping(&m);
    m.set_pointer(header->data());
    return header->data();
  }


  LTO_INLINE void *ThreadCache::malloc(size_t size, bool zero_ignored) {
    if (unlikely(size >= alaska::max_large_size)) {
      return runtime.huge_allocator.alloc(size);
    }
#ifdef STUB_ALLOCATES_HANDLES
    auto *mapping = new_mapping();
#else
    // auto *mapping = (alaska::Mapping *)0x400000008UL;
    auto *mapping = alaska::Mapping::from_handle_id(1);
#endif

    return malloc_generic(size, *mapping);
  }


  LTO_INLINE void *ThreadCache::realloc(void *ptr, size_t new_size) {
    auto original_size = this->get_size(ptr);
    void *new_ptr = this->malloc(new_size, true);
    // We should copy the minimum of the two sizes between the allocations.
    size_t copy_size = original_size > new_size ? new_size : original_size;
    handle_memcpy(new_ptr, ptr, copy_size);
    this->free(ptr);
    return new_ptr;
  }



  LTO_INLINE void ThreadCache::free(void *ptr) {
    if (this->runtime.heap.contains(ptr)) {
      auto *heap_page = alaska::Heap::get_page(ptr);


#ifdef STUB_ALLOCATES_HANDLES
      alaska::Mapping *m = ObjectHeader::from(ptr)->get_mapping();
      // Release the mapping

      auto *handle_slab = this->runtime.handle_table.get_slab(m);
      // Use thread-safe atomic free operation
      handle_slab->free(m);
#else
      // auto *m = (alaska::Mapping *)0x400000008UL;
      auto *m = alaska::Mapping::from_handle_id(1);
#endif


      bool heap_owned = FTR_EXPR("ChkOwner", heap_page->is_owned_by(this));

      // Now the slow path.
      if (likely(heap_owned)) {
        FTR_SCOPE("LocalFree");
        heap_page->release_local(*m, ptr);
      } else {
        FTR_SCOPE("RemoteFree");
        heap_page->release_remote(*m, ptr);
      }

    } else {
      runtime.huge_allocator.free(ptr);
    }
  }

  // -------------------------------------------------------------- //


  LTO_INLINE size_t ThreadCache::get_size(void *handle) {
    alaska::Mapping *m = alaska::Mapping::from_handle_safe(handle);

    if (m) {
      void *ptr = m->get_pointer();
      auto header = alaska::ObjectHeader::from(ptr);
      return header->object_size();
    }


    void *pointer = handle;
    if (runtime.heap.contains(pointer)) {
      // it has an object header.
      return alaska::ObjectHeader::from(pointer)->object_size();
    } else {
      return runtime.huge_allocator.usable_size(pointer);
    }
  }




  void ThreadCache::dump_info(FILE *f) {
    fprintf(f, "ThreadCache %d\n", id);
    fprintf(f, "  heap_churn:       %lu\n", heap_churn.read());
    fprintf(f, "  alloc_rate:       %lu\n", allocation_rate.read());
    fprintf(f, "  free_rate:        %lu\n", free_rate.read());
    fprintf(f, "  active_block:     %p\n", (void *)active_block);
  }

  __attribute__((noinline)) Mapping *ThreadCache::new_mapping_generic(void) {
    auto m = current_slab->alloc();
    if (m != nullptr) {
      return m;
    }

    // We need to get a new slab from the handle table
    auto new_slab = runtime.handle_table.fresh_slab();
    ALASKA_ASSERT(new_slab != nullptr, "Failed to allocate new handle slab");

    // Update our current slab
    this->current_slab = new_slab;

    // Allocate from the new slab
    return new_slab->alloc();
  }

  void ThreadCache::free_mapping(alaska::Mapping *m) { this->runtime.handle_table.put(m, this); }



  constexpr long required_size_for_new_locality_page = 4096;
  long ThreadCache::localize_one(alaska::Mapping *m) {
    alaska::printf("TODO: localize_one for mapping %p\n", (void *)m);
    return 0;
  }

  long ThreadCache::localize(alaska::Mapping *m, long allowed_depth, long depth) {
    if (m->is_free() || m->is_pinned()) return 0;

    void *old_data = m->get_pointer();
    auto *old_header = alaska::ObjectHeader::from(old_data);


    size_t object_size = old_header->object_size();

    if (object_size > 512 || old_header->localized) {
      return 0;
    }

    bool localized = false;
    localized += localize_one(m);

    if (allowed_depth == 1) return localized;

    long num_recursed = 0;
    auto header = alaska::ObjectHeader::from(m);

    // alaska::Mapping *to_walk[object_size / sizeof(void *)];
    // long walk_top = 0;


    header->walk([&](alaska::Mapping *om, alaska::ObjectHeader *oheader) {
      // to_walk[walk_top++] = om;
      localized += localize_one(om);
      num_recursed += this->localize(om, allowed_depth - 1, depth + 1);
    });

    // for (long i = 0; i < walk_top; i++) {
    //   num_recursed += this->localize(to_walk[i], allowed_depth - 1, depth + 1);
    // }


    return (localized ? 1 : 0) + num_recursed;
  }

  ThreadCache::LocalizationResult ThreadCache::localize(alaska::handle_id_t *hids, size_t count) {
    static int dump_number = 0;
    int dump = dump_number++;
    LocalizationResult res;
    res.count = 0;  // We haven't localized anything yet.


    runtime.with_barrier([&]() {
      LocalityReport report;
      long localizedHandles = 0;

      size_t walk_distance = 1;
      walk_distance = count;
      int num_duplicates = 0;

      for (size_t i = 0; i < walk_distance; i++) {
        alaska::Mapping *m = nullptr;
        void *data = nullptr;

        auto hid = hids[i];
        if (hid == 0) continue;
        if (!alaska::check_mapping(hid, m, data)) continue;

        // check that the handle is unique in the hids list.
        // bool unique = true;
        // for (size_t j = 0; j < i; j++) {
        //   if (hids[j] == hid) {
        //     unique = false;
        //     num_duplicates++;
        //     break;
        //   }
        // }
        // if (!unique) continue;
        constexpr int depth = 16;

        // alaska::grade_locality(*m, depth, report);
        localizedHandles += localize(m, depth);
      }
      // alaska::printf("dump %d had %d duplicate handles\n", dump, num_duplicates);

      // report.dump();
      // alaska::printf("YUKON_LOCALITY: %6.2f%%, %zuB %fMB, %ld localized\n",
      //     report.locality() * 100.0f, report.object_bytes,
      //     report.object_bytes / (1024.0f * 1024.0f), localizedHandles);

      res.count = localizedHandles;
    });

    return res;
  }
  __attribute__((tls_model("initial-exec"))) thread_local alaska::ThreadCache *g_tc = nullptr;

  // Self-resolving dispatch for ThreadCache::current().
  // current_bootstrap runs once (under a mutex), creates the Runtime if needed,
  // then swaps g_current to current_fast so all subsequent calls are a single
  // indirect branch with no init check on the hot path.
  static ThreadCache *current_fast() noexcept;
  static ThreadCache *current_bootstrap() noexcept;

  __attribute__((tls_model("initial-exec"))) thread_local ThreadCache *(*g_current)() noexcept =
      current_bootstrap;

  static ThreadCache *current_fast() noexcept { return g_tc; }

  static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;

  // pthread key used to call del_threadcache when a thread exits.
  static pthread_key_t tc_cleanup_key;
  static pthread_once_t tc_key_once = PTHREAD_ONCE_INIT;

  static void tc_thread_exit(void *arg) {
    auto *tc = (ThreadCache *)arg;
    if (tc && Runtime::get_ptr() != nullptr) {
      Runtime::get().del_threadcache(tc);
    }
  }

  static void tc_key_init() { pthread_key_create(&tc_cleanup_key, tc_thread_exit); }

  __attribute__((noinline)) static ThreadCache *current_bootstrap() noexcept {
    alaska::printf("current_bootstrap called rt=%p\n", Runtime::get_ptr());
    pthread_mutex_lock(&init_mutex);
    if (g_current == current_bootstrap) {
      if (Runtime::get_ptr() == nullptr) {
        new Runtime();
      }
    }

    if (g_tc == nullptr) {
      g_tc = Runtime::get().new_threadcache();
      pthread_once(&tc_key_once, tc_key_init);
      pthread_setspecific(tc_cleanup_key, g_tc);
    }

    g_current = current_fast;
    alaska::printf("current_bootstrap done rt=%p tc=%p\n", Runtime::get_ptr(), g_tc);
    pthread_mutex_unlock(&init_mutex);
    return current_fast();
  }

  ThreadCache *ThreadCache::current() noexcept {

    if (unlikely(g_tc == nullptr)) {
      return current_bootstrap();
    }

    return g_tc;
  }



  // --- Handle-based Allocation functions --- //

  void *halloc(size_t size) noexcept {
    void *handle = ThreadCache::current()->halloc(size);
    return handle;
  }

  void *hcalloc(size_t nmemb, size_t size) noexcept {
    void *handle = ThreadCache::current()->halloc(nmemb * size);
    if (handle != nullptr) {
      memset(alaska::Mapping::translate(handle), 0, nmemb * size);
    }
    return handle;
  }

  void *hrealloc(void *handle, size_t new_size) noexcept {
    if (handle == nullptr) return halloc(new_size);
    if (new_size == 0) {
      hfree(handle);
      return nullptr;
    }
    return g_current()->hrealloc(handle, new_size);
  }

  void hfree(void *handle) noexcept {
    if (handle == nullptr) return;
    g_current()->hfree(handle);
  }

  size_t halloc_usable_size(void *handle) noexcept {
    return ThreadCache::current()->get_size(handle);
  }


  // --- Stub Allocation functions (pointer-based, not handle-based) --- //

  void *stub_malloc(size_t size) noexcept { return ThreadCache::current()->malloc(size); }

  void *stub_calloc(size_t nmemb, size_t size) noexcept {
    void *handle = ThreadCache::current()->malloc(nmemb * size);
    if (handle != nullptr) {
      memset(alaska::Mapping::translate(handle), 0, nmemb * size);
    }
    return handle;
  }

  void *stub_realloc(void *ptr, size_t new_size) noexcept {
    if (ptr == nullptr) return stub_malloc(new_size);
    if (new_size == 0) {
      stub_free(ptr);
      return nullptr;
    }
    return ThreadCache::current()->realloc(ptr, new_size);
  }

  void stub_free(void *ptr) noexcept {
    if (ptr == nullptr) return;
    ThreadCache::current()->free(ptr);
  }

  size_t stub_malloc_usable_size(void *ptr) noexcept {
    return ThreadCache::current()->get_size(ptr);
  }


}  // namespace alaska
