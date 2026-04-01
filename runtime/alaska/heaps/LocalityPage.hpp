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

#pragma once

#include <ck/vec.h>
#include <stdlib.h>
#include <stdint.h>
#include <alaska/alaska.hpp>
#include <alaska/util/Logger.hpp>
#include <alaska/heaps/HeapPage.hpp>
#include <alaska/util/SizedAllocator.hpp>
#include <alaska/handles/ObjectHeader.hpp>
namespace alaska {

  /**
   * Heap page specialization that groups long-lived objects by locality using a
   * simple bump allocator. Objects are migrated into the page in the order they
   * are expected to be accessed, and memory is never reclaimed piecemeal, only
   * when the page is destroyed.
   */
  class LocalityPage final : public alaska::HeapPage {
   public:
    /**
     * Construct a locality page backed by the provided page-sized memory
     * region. The bump pointer starts at the first usable byte in the page.
     */
    LocalityPage(void *backing_memory)
        : alaska::HeapPage(backing_memory) {
      bump_next = (void *)this->memory_start();
    }

    /** Bytes still available for allocation within this page. */
    size_t available(void) override { return memory_end() - (uintptr_t)bump_next; }

    /** Bytes that have been committed to objects through bump allocation. */
    size_t committed(void) { return (uintptr_t)bump_next - memory_start(); }

    ~LocalityPage() override;

    /**
     * Allocate storage for an object described by the mapping using the
     * locality page's bump pointer. Returns the object's data pointer or
     * nullptr when the page is exhausted.
     */
    void *alloc(const alaska::Mapping &m, alaska::AlignedSize size) override;

    /**
     * Release an object whose lifetime ended on this page by marking the header
     * as unmapped and tracking the freed payload size for fragmentation stats.
     */
    bool release_local(const alaska::Mapping &m, void *ptr) override;

    /**
     * Migrate an existing mapping's payload into this locality page, updating
     * the mapping to point at the newly allocated storage.
     */
    bool localize(alaska::Mapping &m);


    /** Ratio of bytes freed through release_local to total committed bytes. */
    float fragmentation(void) override { return (float)freed_bytes / (float)committed(); }

   private:
    size_t freed_bytes = 0;
    long live_count = 0;
    void *bump_next;
  };

  inline void *LocalityPage::alloc(const alaska::Mapping &m, alaska::AlignedSize size) {
    size_t real_size = size + sizeof(alaska::ObjectHeader);

    auto bump_after = (uintptr_t)bump_next + real_size;
    if (unlikely(bump_after >= memory_end())) {
      // alaska::printf("LocalityPage: out of memory trying to allocate %zu bytes\n", real_size);
      return nullptr;
    }

    // Bump allocate!
    alaska::ObjectHeader *header =
        (alaska::ObjectHeader *)__builtin_assume_aligned((void *)bump_next, 8);

    header->set_mapping(&m);
    header->set_object_size(size);
    header->localized = 1;

    bump_next = (void *)bump_after;
    live_count++;
    return header->data();
  }

};  // namespace alaska
