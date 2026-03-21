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


#include <sys/mman.h>
#include <alaska/util/Logger.hpp>
#include <alaska/heaps/Heap.hpp>
#include "alaska/heaps/HeapPage.hpp"
#include "alaska/heaps/LocalityPage.hpp"
#include "alaska/heaps/SizeClass.hpp"
#include "alaska/util/utils.h"
#include <alaska/core/ThreadCache.hpp>


namespace alaska {
  ////////////////////////////////////
  Heap::Heap(alaska::Configuration &config)
      : heap_start(nullptr)
      , heap_end(nullptr)
      , heap_bump(nullptr)
      , heap_size(alaska::default_heap_size) {
    // Allow overriding the heap size via environment variable.
    if (const char *env = getenv("ALASKA_HEAP_SIZE")) {
      char *end = nullptr;
      size_t val = strtoull(env, &end, 10);
      if (end != env) {
        switch (*end) {
          case 'T':
          case 't':
            val *= 1024;
            [[fallthrough]];
          case 'G':
          case 'g':
            val *= 1024;
            [[fallthrough]];
          case 'M':
          case 'm':
            val *= 1024;
            [[fallthrough]];
          case 'K':
          case 'k':
            val *= 1024;
            [[fallthrough]];
          case '\0':
            break;
          default:
            alaska::printf("ALASKA_HEAP_SIZE: unrecognized suffix '%c'. Aborting.\n", *end);
            abort();
        }
        heap_size = val;
      } else {
        alaska::printf("ALASKA_HEAP_SIZE: could not parse '%s'. Aborting.\n", env);
        abort();
      }
    }

    auto prot = PROT_READ | PROT_WRITE;
    auto flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
    heap_start = mmap(NULL, heap_size, prot, flags, -1, 0);

    ALASKA_ASSERT(heap_start != MAP_FAILED,
                  "Failed to allocate the heap's backing memory. Aborting.");
    madvise(heap_start, heap_size, MADV_NOHUGEPAGE);

    heap_bump = heap_start;
    heap_bump = (void *)(((uintptr_t)heap_bump + alaska::page_size - 1) & ~(alaska::page_size - 1));
    heap_end = (void *)((uintptr_t)heap_start + heap_size);
    INIT_LIST_HEAD(&m_age_list);

    log_debug("Heap: Backing memory allocated at %p", heap_start);
  }

  Heap::~Heap(void) {
    if (heap_start != nullptr) {
      log_debug("Heap: Deallocating backing memory at %p", heap_start);
      munmap(heap_start, heap_size);
    }
  }

  void *Heap::alloc_heap_page() {
    FTR_FUNCTION();
    void *page = heap_bump;
    heap_bump = (void *)((uintptr_t)heap_bump + alaska::page_size);

    ALASKA_ASSERT(page < heap_end, "Out of memory in heap backing store.");
    return page;
  }

  SizedPage *Heap::get_sizedpage(size_t size, ThreadCache *owner) {
    FTR_FUNCTION();
    ck::scoped_lock lk(this->lock);  // TODO: don't lock.
    int cls = alaska::size_to_class(size);
    auto &mag = this->size_classes[cls];
    // Look for a sized page in the magazine with at least one allocation space available.
    // TODO: it would be smart to adjust this requirement dynamically based on the allocation
    // request.
    auto *p = this->find_or_alloc_page<SizedPage>(mag, owner, 1, [=](auto p) {
      FTR_SCOPE("InitSizedPage");
      // alaska::printf("Allocating sized page for class %d (%zu bytes)\n", cls, size);
      p->set_size_class(cls);
    });
    return p;
  }


  LocalityPage *Heap::get_localitypage(size_t size_requirement, ThreadCache *owner) {
    FTR_FUNCTION();
    ck::scoped_lock lk(this->lock);  // TODO: don't lock.
    auto *p = this->find_or_alloc_page<LocalityPage>(locality_pages, owner, size_requirement,
                                                     [](auto *p) {
                                                     });
    return p;
  }


  void Heap::put_page(SizedPage *page) {
    FTR_FUNCTION();
    // Return a SizedPage back to the global (unowned) heap.
    ck::scoped_lock lk(this->lock);  // TODO: don't lock.
    page->set_owner(nullptr);
    if (page->magazine) {
      if (page->available() > 0) {
        page->magazine->move_to_available(page);
      } else {
        page->magazine->move_to_full(page);
      }
    }
  }


  void Heap::put_page(LocalityPage *page) {
    FTR_FUNCTION();
    // Return a LocalityPage back to the global (unowned) heap.
    ck::scoped_lock lk(this->lock);  // TODO: don't lock.
    page->set_owner(nullptr);
    if (page->magazine) {
      if (page->available() > 0) {
        page->magazine->move_to_available(page);
      } else {
        page->magazine->move_to_full(page);
      }
    }
  }


  void Heap::dump(FILE *stream) {
    long i = 0;

    size_t total_committed = 0;

    for (auto &mag : size_classes) {
      if (mag.size() == 0) continue;
      auto size = alaska::class_to_size(i);

      fprintf(stream, "SizePages<%zu>\n", size);
      i += 1;

      long page_index = 0;
      mag.for_each([&](SizedPage *p) {
        size_t committed = p->committed();
        total_committed += committed;
        fprintf(stream, "  - %p avail:%zu  commit:%zu  frag:%f\n", p, p->available(), committed,
                p->fragmentation());
        return true;
      });
    }


    fprintf(stream, "LocalityPages\n");
    locality_pages.for_each([&](LocalityPage *p) {
      size_t committed = p->committed();
      total_committed += committed;
      fprintf(stream, "  - %p avail:%zu  commit:%zu  frag:%f\n", p, p->available(), committed,
              p->fragmentation());
      return true;
    });

    fprintf(stream, "Total committed memory: %zu bytes (%fmb)\n", total_committed,
            total_committed / (1024.0 * 1024.0));

    fprintf(stream, "\n");
  }



  void Heap::dump_html(FILE *stream) {
    auto dump_page = [&](auto page) {
      if (page == NULL) return true;
      fprintf(stream, "<tr>");
      fprintf(stream, "<td>%p</td>", page);
      fprintf(stream, "<td>");
      page->dump_html(stream);
      fprintf(stream, "</tr>\n");
      return true;
    };

    locality_pages.for_each(dump_page);
    // for (auto &mag : size_classes)
    //   mag.for_each (dump_page);
  }


  void Heap::dump_json(FILE *stream) {
    fprintf(stream, "{\"pages\": [");
    bool first = true;
    for (uintptr_t addr = (uintptr_t)heap_start; addr < (uintptr_t)heap_bump;
         addr += alaska::page_size) {
      auto *page = get_page((void *)addr);
      if (!page) continue;
      if (!first) fprintf(stream, ",");
      first = false;
      page->dump_json(stream);
    }
    fprintf(stream, "]}");
  }

  void Heap::collect() {
    ck::scoped_lock lk(this->lock);


    // TODO:
  }


  long Heap::compact_sizedpages(void) {
    size_t total_bytes = 0;
    size_t zero_bytes = 0;
    size_t total_objects = 0;


    size_t bytes_saved = 0;
    long c = 0;
    for (auto &mag : size_classes) {
      mag.for_each([&](SizedPage *sp) {
        long z = 0, t = 0;
        if (true || sp->fragmentation() > 0.15) {
          zero_bytes += z;
          total_bytes += t;
          total_objects += t / sp->get_object_size();

          long moved = sp->compact();
          c += moved;
          bytes_saved += moved * sp->get_object_size();
        }
        return true;
      });
    }
    return bytes_saved;
  }

  long Heap::compact_locality_pages(void) {
    return 0;
    long c = 0;
    alaska::printf("Locality pages: %lu\n", locality_pages.size());
    locality_pages.for_each([&](LocalityPage *lp) {
      alaska::printf(" - Locality page %p  commit:%zu, avail:%zu, frag:%.2f%%\n", lp,
                     lp->committed(), lp->available(), lp->fragmentation() * 100);
      return true;
    });
    return c;
  }



  void Heap::sweep(void) {
    long num_marked = 0;
    long num_pages = 0;
    for (auto &mag : size_classes) {
      mag.for_each([&](SizedPage *sp) -> bool {
        num_pages++;
        num_marked += sp->bump_age();
        return true;
      });
    }
    // alaska::printf("Heap sweep: marked %ld objects across %ld pages.\n", num_marked, num_pages);
  }

  void Heap::collect(ThreadCache *tc, int sc) {
    ck::scoped_lock lk(this->lock);
    this->size_classes[sc].collect();
  }




  void *mmap_alloc(size_t bytes) {
    auto prot = PROT_READ | PROT_WRITE;
    auto flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
    // round bytes up to 4096
    bytes = (bytes + 4095) & ~4095;
    void *ptr = mmap(NULL, bytes, prot, flags, -1, 0);
    ALASKA_ASSERT(ptr != MAP_FAILED, "Failed to allocate memory with mmap.");
    return ptr;
  }

  void mmap_free(void *ptr, size_t bytes) {
    // round bytes up to 4096
    bytes = (bytes + 4095) & ~4095;
    munmap(ptr, bytes);
  }




}  // namespace alaska
