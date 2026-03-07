
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


#include <alaska/core/Runtime.hpp>
#include <alaska/heaps/SizeClass.hpp>
#include <alaska/work/BarrierManager.hpp>
#include <alaska/Localizer.hpp>
#include "alaska/alaska.hpp"
#include "alaska/util/utils.h"
#include <stdlib.h>
#include <alaska/util/utils.h>
#include <alaska/internal/alaska_internal_malloc.h>

namespace alaska {
  // The default instance of a barrier manager.
  static BarrierManager global_nop_barrier_manager;
  // The current global instance of the runtime, since we can only have one at a time
  static Runtime *g_runtime = nullptr;
  static volatile bool runtime_initialized = false;


  Runtime::Runtime(alaska::Configuration config)
      : config(config)
      , handle_table(config)
      , heap(config) {
    if (g_runtime != nullptr) {
      log_error("Cannot create a new Alaska Runtime, one already exists at %p", g_runtime);
      abort();
    }


    // Assign the global runtime to be this instance
    atomic_set(g_runtime, this);
    // Attach a default barrier manager
    this->barrier_manager = &global_nop_barrier_manager;

    log_debug("Created a new Alaska Runtime @ %p", this);
    atomic_set(runtime_initialized, true);
  }

  Runtime::~Runtime() {
    log_debug("Destroying Alaska Runtime");
    // Unset the global instance so anoruntime can be allocated
    atomic_set(g_runtime, nullptr);
  }


  Runtime &Runtime::get() {
    ALASKA_ASSERT(g_runtime != nullptr, "Runtime not initialized");
    return *g_runtime;
  }
  Runtime *Runtime::get_ptr() { return g_runtime; }

  bool Runtime::is_valid_handle(void *p) {
    alaska::Mapping *m = alaska::Mapping::from_handle_safe(p);
    if (m == nullptr) return false;
    return this->handle_table.valid_handle(m);
  }

  ThreadCache *Runtime::new_threadcache(void) {
    auto tc = new ThreadCache(next_thread_cache_id++, *this);
    tcs_lock.lock();
    tcs.add(tc);
    tcs_lock.unlock();
    return tc;
  }

  void Runtime::del_threadcache(ThreadCache *tc) {
    tcs_lock.lock();
    tcs.remove(tc);
    delete tc;
    tcs_lock.unlock();
  }


  void Runtime::lock_all_thread_caches(void) {
    tcs_lock.lock();

    for (auto *tc : tcs)
      tc->lock.lock();
  }
  void Runtime::unlock_all_thread_caches(void) {
    for (auto *tc : tcs)
      tc->lock.unlock();
    tcs_lock.unlock();
  }


  void wait_for_initialization(void) {
    log_debug("waiting for initialization!\n");
    while (not is_initialized()) {
      sched_yield();
    }
    log_debug("Initialized!\n");
  }


  void *do_handle_fault_and_translate(uint64_t handle) {
    auto &rt = alaska::Runtime::get();
    rt.handle_fault(handle);
    return alaska::Mapping::translate((void *)handle);
  }


  int Runtime::handle_fault(uint64_t handle) {
    auto *m = alaska::Mapping::from_handle((void *)handle);

    // printf("Handle fault on %p\n", (void *)m);

    // With domains removed, we simply clear the fault pending bit.
    // If we had more complex logic (like paging from disk), it would go here.
    m->set_fault_pending(false);

    // printf("fault on %p\n", m);
    handle_faults.track_atomic(1);
    return 0;
  }


  bool is_initialized(void) { return atomic_get(runtime_initialized); }


  //  6 -   64b (cache line)
  // 10 - 1024b
  // 12 - 4096b (page)
  static constexpr int grade_page_shift = 12;  // 12 for page, 6 for cache line

  Runtime::HeapReport Runtime::grade_heap(void) {
    auto start_time = alaska_timestamp();
    HeapReport report{0};



    struct SizeclassStats {
      size_t count = 0;
      uint64_t in_pointers = 0;
      uint64_t out_pointers = 0;
    };

    SizeclassStats size_class_histogram[alaska::num_size_classes] = {};

    handle_table.for_each_handle([&](alaska::Mapping *m) {
      void *p = m->get_pointer();
      if (p == nullptr) return;
      auto header = alaska::ObjectHeader::from(p);
      if (!header) return;

      if (header->localized) report.total_localized++;

      size_t obj_size = header->object_size();

      size_t sc = alaska::size_to_class(obj_size);
      if (sc >= alaska::num_size_classes) {
        return;  // continue, ignore invalid size classes
      }
      size_class_histogram[sc].count++;

      report.total_handles++;
      report.object_bytes += obj_size;

      uintptr_t object_page = (uintptr_t)p >> grade_page_shift;
      uintptr_t handle_page = (uintptr_t)m >> grade_page_shift;

      // alaska::printf("Walking object %p (size=%zu, data=%p)\n", m, obj_size, header->data());
      // Walk the mapping to count in/out pointers.
      header->walk([&](alaska::Mapping *om, alaska::ObjectHeader *oheader) {
        void *p = oheader->data();
        uintptr_t opage = (uintptr_t)p >> grade_page_shift;
        if (opage != object_page) {
          size_class_histogram[sc].out_pointers++;
          report.out_pointers++;
        } else {
          size_class_histogram[sc].in_pointers++;
          report.in_pointers++;
        }

        uintptr_t hpage = (uintptr_t)om >> grade_page_shift;
        if (hpage != handle_page) {
          report.out_handles++;
        } else {
          report.in_handles++;
        }
      });
    });


    // Walk over every heap page to count committed bytes.
    heap.for_each_page([&](alaska::HeapPage *page) {
      report.committed_bytes += page->committed_bytes();
    });


    auto end_time = alaska_timestamp();

#if 0
    // TODO: remove this dumping when done debugging.
    alaska::printf("-- HEAP GRADE REPORT --\n");
    alaska::printf("Graded heap in %.3f ms\n", (end_time - start_time) / 1e6f);
    alaska::printf("Total committed bytes:      %zu\n", report.committed_bytes);
    alaska::printf("Total handles:              %zu\n", report.total_handles);
    alaska::printf("Localized:                  %zu (%f%%)\n", report.total_localized,
        report.total_localized * 100.0f / (float)report.total_handles);
    alaska::printf(
        "Handle table bytes:         %zu\n", report.total_handles * sizeof(alaska::Mapping));
    alaska::printf("Total object bytes:         %zu\n", report.object_bytes);
    alaska::printf("Average Size:               %.1f\n",
        report.total_handles == 0 ? 0.0
                                  : (double)report.object_bytes / (double)report.total_handles);
    alaska::printf("Heap Utilization:           %.2f%%\n",
        report.committed_bytes == 0
            ? 0.0
            : 100.0 * (double)report.object_bytes / (double)report.committed_bytes);

    // in/out pointers
    if (report.in_pointers + report.out_pointers > 0) {
      alaska::printf("Pointer Locality:           %.2f%%  %zu/%zu\n",
          100.0 * (double)report.in_pointers / (double)(report.in_pointers + report.out_pointers),
          report.in_pointers, report.out_pointers);
    }


    // in/out handles
    if (report.in_handles + report.out_handles > 0) {
      alaska::printf("Handle Locality:            %.2f%%  %zu/%zu\n",
          100.0 * (double)report.in_handles / (double)(report.in_handles + report.out_handles),
          report.in_handles, report.out_handles);
    }


    /*
    alaska::printf("\nSize Class Histogram:\n");
    alaska::printf(" Size Class | Size |  Count  |  In Ptrs  | Out Ptrs |  Locality \n");
    alaska::printf("-------------------------------------------------------------\n");
    for (size_t i = 0; i < alaska::num_size_classes; i++) {
      size_t count = size_class_histogram[i].count;
      if (count == 0) continue;
      uint64_t in_ptrs = size_class_histogram[i].in_pointers;
      uint64_t out_ptrs = size_class_histogram[i].out_pointers;
      double locality =
          in_ptrs + out_ptrs == 0 ? 0.0 : 100.0 * (double)in_ptrs / (double)(in_ptrs + out_ptrs);
      alaska::printf(" %10zu | %4zu | %7zu | %9zu | %8zu | %8.2f%% \n", i, alaska::class_to_size(i),
          count, in_ptrs, out_ptrs, locality);
    }
    */


    alaska::printf("-----------------------\n");
#endif



    return report;
  }


  void LocalityReport::dump() {
    alaska::printf("Locality Report:\n");
    alaska::printf("  In-Pointers:  %zu\n", in_pointers);
    alaska::printf("  Out-Pointers: %zu\n", out_pointers);
    alaska::printf("  Object Bytes: %zu\n", object_bytes);
    if (in_pointers + out_pointers > 0) {
      alaska::printf("  Pointer Locality: %.2f%%\n",
                     100.0 * (double)in_pointers / (double)(in_pointers + out_pointers));
    } else {
      alaska::printf("  Pointer Locality: N/A\n");
    }
  }

  void grade_locality(alaska::Mapping &m, int depth, LocalityReport &report) {
    // alaska::printf("Grading locality at depth %d for object %p\n", depth, &m);

    auto header = alaska::ObjectHeader::from(&m);
    auto p = header->data();
    report.object_bytes += header->object_size();
    uintptr_t object_page = (uintptr_t)p >> grade_page_shift;


    header->walk([&](alaska::Mapping *om, alaska::ObjectHeader *oheader) {
      uintptr_t opage = (uintptr_t)oheader->data() >> grade_page_shift;
      if (opage != object_page) {
        report.out_pointers++;
      } else {
        report.in_pointers++;
      }
      if (depth == 1) return;
      grade_locality(*om, depth - 1, report);
    });
  }


  LocalityReport grade_locality(alaska::Mapping &root, int max_depth) {
    LocalityReport report;
    grade_locality(root, max_depth, report);
    return report;
  }

}  // namespace alaska


// Simply use clock_gettime, which is fast enough on most systems
extern "C" __attribute__((visibility("default"))) uint64_t alaska_timestamp() {
  struct timespec spec;
  clock_gettime(1, &spec);
  return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
}



static void __attribute__((destructor)) alaska_runtime_deinit(void) {
  //
}


#include <alaska/compression/lz4.h>

static size_t compress_zero_removal_size(void *data, size_t size) {
  size_t csize = 0;
  csize += sizeof(uint16_t);  // store the size of the data
  // there should be a bit for each byte, set to 1 if the byte is non-zero
  csize += (size + 7) / 8;

  for (size_t i = 0; i < size; i++) {
    if (((uint8_t *)data)[i] != 0) {
      csize += sizeof(uint8_t);
    }
  }
  return csize;
}


bool test_is_valid(void *handle, alaska::Runtime &rt) { return rt.is_valid_handle(handle); }

extern "C" ALASKA_EXPORT AlaskaCtlResult __alaska_ctl(AlaskaCtlOperation op, uint64_t arg) {
  // printf("alaska_ctl(%d, %p)\n", op, (void *)arg);

#define CTRL_CHECK_HANDLE(_arg)                                \
  ({                                                           \
    auto *m = alaska::Mapping::from_handle_safe((void *)_arg); \
    if (!m) return ALASKA_INVALID;                             \
    m;                                                         \
  })

  auto &rt = alaska::Runtime::get();

  switch (op) {
    case ALASKA_CTL_MARK_FOR_FAULT: {
      auto m = CTRL_CHECK_HANDLE(arg);
      printf("Marking %p for fault\n", (void *)arg);
      m->set_fault_pending(true);
      return ALASKA_SUCCESS;
    }

    case ALASKA_CTL_RUN_BARRIER: {
      auto cfg = (struct alaska_barrier_config *)arg;
      if (!cfg->callback) return ALASKA_INVALID;

      rt.with_barrier([cfg]() {
        cfg->callback(cfg->user);
      });
      return ALASKA_SUCCESS;
    }

    case ALASKA_CTL_COMPRESS_TEST: {
      auto ht = &rt.handle_table;
      uint64_t total_size = 0;
      uint64_t total_compressed_size = 0;


      rt.with_barrier([&]() {
        ht->for_each_handle([&](alaska::Mapping *m) {
          auto header = alaska::ObjectHeader::from(m);
          auto data = header->data();
          auto uncompressed_size = header->object_size();
          total_size += uncompressed_size;
          total_compressed_size += compress_zero_removal_size(data, uncompressed_size);
        });
        float ratio = (total_size == 0) ? 0.0f : (float)total_compressed_size / (float)total_size;
        printf("With zero removal, compressed %fMB to %fMB, ratio %.2f\n",
               (float)total_size / (1024.0f * 1024.0f),
               (float)total_compressed_size / (1024.0f * 1024.0f), ratio);
      });

      return ALASKA_SUCCESS;
    }

    default:
      return ALASKA_INVALID;
  }
}



extern "C" ALASKA_EXPORT void alaska_sweep(void) {
  auto &rt = alaska::Runtime::get();
  rt.heap.sweep();
}
