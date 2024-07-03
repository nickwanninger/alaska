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
#include <alaska/Logger.hpp>
#include <alaska/Heap.hpp>
#include "alaska/HeapPage.hpp"
#include "alaska/SizeClass.hpp"
#include "alaska/SizedPage.hpp"
#include "alaska/utils.h"

namespace alaska {


  PageManager::PageManager(void) {
    auto prot = PROT_READ | PROT_WRITE;
    auto flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
    this->heap = mmap(NULL, alaska::heap_size, prot, flags, -1, 0);
    ALASKA_ASSERT(
        this->heap != MAP_FAILED, "Failed to allocate the heap's backing memory. Aborting.");

    // Set the bump allocator to the start of the heap.
    this->bump = this->heap;
    this->end = (void *)((uintptr_t)this->heap + alaska::heap_size);

    // Initialize the free list w/ null, so the first allocation is a simple bump.
    this->free_list = nullptr;

    log_debug("PageManager: Heap allocated at %p", this->heap);
  }

  PageManager::~PageManager() {
    log_debug("PageManager: Deallocating heap at %p", this->heap);
    munmap(this->heap, alaska::heap_size);
  }


  void *PageManager::alloc_page(void) {
    ck::scoped_lock lk(this->lock);  // TODO: don't lock.


    if (this->free_list != nullptr) {
      // If we have a free page, pop it off the free list and return it.
      FreePage *fp = this->free_list;
      this->free_list = fp->next;
      log_trace("PageManager: reusing free page at %p", fp);
      alloc_count++;
      return (void *)fp;
    }

    // If we don't have a free page, we need to allocate a new one with the bump allocator.
    void *page = this->bump;
    log_trace("PageManager: bumping to %p", this->bump);
    this->bump = (void *)((uintptr_t)this->bump + alaska::page_size);

    log_trace("PageManager: end = %p", this->end);

    // TODO: this is *so unlikely* to happen. This check is likely expensive and not needed.
    ALASKA_ASSERT(page < this->end, "Out of memory in the page manager.");
    alloc_count++;

    return page;
  }

  void PageManager::free_page(void *page) {
    // check that the pointer is within the heap and early return if it is not
    if (unlikely(page < this->heap || page >= this->end)) {
      return;
    }

    ck::scoped_lock lk(this->lock);  // TODO: don't lock.

    // cast the page to a FreePage to store metadata in.
    FreePage *fp = (FreePage *)page;

    // Super simple: push to the free list.
    fp->next = this->free_list;
    this->free_list = fp;

    alloc_count--;
  }


  static void *allocate_page_table(void) {
    return calloc(1LU << bits_per_pt_level, sizeof(void *));
  }

  HeapPageTable::HeapPageTable(void *heap_start)
      : heap_start(heap_start) {
    // Allocate the root of the page table. The subsequent mappings will be allocated on demand.
    root = (alaska::HeapPage ***)allocate_page_table();
  }
  HeapPageTable::~HeapPageTable() {
    // Free all the entries.
  }



  alaska::HeapPage *HeapPageTable::get(void *page) { return *walk(page); }
  alaska::HeapPage *HeapPageTable::get_unaligned(void *addr) {
    uintptr_t heap_offset = (uintptr_t)addr - (uintptr_t)heap_start;
    uintptr_t page_ind = heap_offset / alaska::page_size;
    void *page = (void *)((uintptr_t)heap_start + page_ind * alaska::page_size);

    return *walk(page);
  }

  void HeapPageTable::set(void *page, alaska::HeapPage *hp) { *walk(page) = hp; }


  alaska::HeapPage **HeapPageTable::walk(void *vpage) {
    // `page` here means the offset from the start of the heap.
    uintptr_t page_off = (uintptr_t)vpage - (uintptr_t)heap_start;
    // Extrac the page number (just an index into the page table structure)
    off_t page_number = page_off >> alaska::page_shift_factor;

    // Gross math here. Can't avoid it.
    // Effectively, we are using the bits in the page number to index into two-level page table
    // structure in the exact same way that we would on a real x86_64 system's page table.
    off_t ind1 = page_number & pt_level_mask;
    off_t ind2 = (page_number >> bits_per_pt_level) & pt_level_mask;

    log_debug(
        "HeapPageTable: walk(%p) -> pn: %lu, inds: (%zu, %zu)", vpage, page_number, ind1, ind2);

    // Grab the entry from the root page table.
    HeapPage **pt1 = root[ind1];
    // It is null, allocate a new entry and set it.
    if (pt1 == nullptr) {
      // If the first level page table entry is null, we need to allocate a new page table.
      pt1 = (HeapPage **)allocate_page_table();
      root[ind1] = pt1;
    }

    return &pt1[ind2];
  }




  ////////////////////////////////////
  Heap::Heap(void)
      : pm()
      , pt(pm.get_start()) {
    log_debug("Heap: Initialized heap");
  }



  Heap::~Heap(void) {
    // dump(stderr);


  SizedPage *Heap::get(size_t size, ThreadCache *owner) {
    ck::scoped_lock lk(this->lock);  // TODO: don't lock.
    int cls = alaska::size_to_class(size);
    auto &mag = this->size_classes[cls];

    if (mag.size() != 0) {
      auto p = mag.find([](SizedPage *p) {
        auto sp = p;
        if (sp->available() > 0 and sp->get_owner() == nullptr) return true;
        return false;
      });

      if (p != NULL) {
        p->set_owner(owner);
        return static_cast<SizedPage *>(p);
      }
    }


    // Allocate backing memory
    void *memory = this->pm.alloc_page();
    log_trace("Heap::get(%zu) :: allocating new SizedPage to manage %p", size, memory);

    // Allocate a new SizedPage for that memory
    auto *p = new SizedPage(memory);
    p->set_size_class(cls);


    // Map it in the page table for fast lookup
    pt.set(memory, p);
    mag.add(p);
    p->set_owner(owner);
    return p;
  }


  void Heap::put(SizedPage *page) {
    ck::scoped_lock lk(this->lock);  // TODO: don't lock.
    page->set_owner(nullptr);
    int cls = page->get_size_class();
    // TODO: there's more to this, I think. We should somehow split up or sort pages by their
    // fullness somewhere...
    size_classes[cls].add(page);
  }


#define out(...) fprintf(stream, __VA_ARGS__)
  void Heap::dump(FILE *stream) {
    out("========== HEAP DUMP ==========\n");

    for (int cls = 0; cls < alaska::num_size_classes; cls++) {
      auto &mag = size_classes[cls];
      if (mag.size() == 0) continue;

      out("sc %d: %zu heaps\n", cls, mag.size());


      int page_ind = 0;
      mag.foreach ([&](HeapPage *hp) {
        SizedPage *sp = static_cast<SizedPage *>(hp);
        out(" - %p owned by %p. %zu avail\n", sp, sp->get_owner(), sp->available());
        page_ind++;
        if (page_ind > mag.size()) return false;
        return true;
      });
    }
  }

#undef O

  void Heap::collect() {
    ck::scoped_lock lk(this->lock);
    // TODO:
  }
}  // namespace alaska
