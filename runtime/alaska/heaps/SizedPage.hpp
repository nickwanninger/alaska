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

#include <alaska/handles/ObjectHeader.hpp>
#include <alaska/heaps/HeapPage.hpp>
#include <alaska/heaps/SizeClass.hpp>
#include <alaska/util/ShardedFreeList.hpp>
#include <alaska/util/SizedAllocator.hpp>

namespace alaska {
  struct Block;

  // A SizedPage allocates objects of one specific size class.
  class SizedPage final : public alaska::HeapPage {
   public:
    using HeapPage::HeapPage;
    ~SizedPage(void) override;


    void *alloc(const alaska::Mapping &m, alaska::AlignedSize size) override alaska_attr_malloc;
    bool release_local(const alaska::Mapping &m, void *ptr) override;
    bool release_remote(const alaska::Mapping &m, void *ptr) override;
    size_t committed_bytes(void) override { return committed(); }

    void set_size_class(int cls);
    int get_size_class(void) const { return size_class; }
    size_t get_object_size(void) const { return object_size; }


    // Compact the page.
    long compact(void);
    // Run through the page and validate as much info as possible w/ asserts.
    void validate(void);

    long object_capacity(void) const { return this->capacity; }



    long bump_age(void); // Return the number of objects bumped.

    float fragmentation(void) override {
      auto extent = object_extent();
      if (extent == 0) return 0.0f;
      return 1.0f - ((float)num_free_in_bump_allocator() / (float)extent);
    }


    // How many free slots are there? (We return an estimate!)
    inline size_t available(void) override { return num_free_in_bump_allocator() * object_size; }

    inline long num_free_in_bump_allocator(void) const {
      return (((uintptr_t)objects_end - (uintptr_t)bump_next) / object_size);
    }

    size_t committed(void) { return (uintptr_t)bump_next - (uintptr_t)objects_start; }

    inline auto &get_freelist(void) { return freelist; }

   private:
    struct SizePageBlock {
      ObjectHeader header;
      SizePageBlock *next;

      inline void setNext(SizePageBlock *n) { next = n; }
      inline SizePageBlock *getNext(void) const { return next; }
      inline void markFreed(void) {}
      inline void markAllocated(void) {}
    };
    ShardedFreeList<SizePageBlock> freelist;


    long extend(long count);

    void *alloc_slow(const alaska::Mapping &m, alaska::AlignedSize size);

    inline void release_local(ObjectHeader *h) {
      freelist.free_local(h);
      h->handle_id = 0;
    }

    inline void release_remote(ObjectHeader *h) {
      freelist.free_remote(h);
      h->handle_id = 0;
    }

    inline bool is_allocated(ObjectHeader *h) { return h->handle_id != 0; }

    // An internal-ish number which represents the extent of objects
    // which have been bump allocated in this allocator so far.
    inline long object_extent(void) {
      return ((off_t)bump_next - (off_t)objects_start) / object_size;
    }
    ////////////////////////////////////////////////

    int size_class;      // The size class of this page
    size_t object_size;  // The byte size of the size class of this page (saves a load)
    long capacity;       // how many objects + headers fit into this page



    void *objects_start;  // The start of the object memory
    void *objects_end;    // The end of the object memory (exclusive)
    void *bump_next;      // The next object to be bump allocated.

    // SizedAllocator allocator;

    ObjectHeader *ind_to_header(long ind) {
      size_t real_size = object_size + sizeof(ObjectHeader);
      return (ObjectHeader *)((char *)this->memory + (ind * real_size));
    }

    long header_to_ind(ObjectHeader *h) {
      size_t real_size = object_size + sizeof(ObjectHeader);
      return ((char *)h - (char *)this->memory) / real_size;
    }
  };


  inline bool SizedPage::release_local(const alaska::Mapping &m, void *ptr) {
    auto header = alaska::ObjectHeader::from(ptr);
    release_local(header);
    return true;
  }


  inline bool SizedPage::release_remote(const alaska::Mapping &m, void *ptr) {
    auto header = alaska::ObjectHeader::from(ptr);
    release_remote(header);
    return true;
  }



}  // namespace alaska
