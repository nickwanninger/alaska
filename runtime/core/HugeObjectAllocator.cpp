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


#include <malloc.h>
#include <stdlib.h>

#include <alaska/Heap.hpp>
#include <alaska/HugeObjectAllocator.hpp>
#include <alaska/liballoc.h>
#include <alaska/list_head.h>

namespace alaska {
  HugeObjectAllocator::HugeObjectAllocator(HugeAllocationStrategy strat)
      : strat(strat) {
    INIT_LIST_HEAD(&this->allocations);
  }

  HugeObjectAllocator::~HugeObjectAllocator() {
    HugeHeader *entry, *temp;
    // Iterate over the list safely
    list_for_each_entry_safe(entry, temp, &this->allocations, list) {
      // Remove the entry from the list
      list_del(&entry->list);
      alaska::mmap_free((void*)entry, entry->mapping_size);
    }
  }



  void* HugeObjectAllocator::allocate(size_t size) {
    if (strat == HugeAllocationStrategy::MALLOC_BACKED) {
      return ::malloc(size);
    }
    ALASKA_ASSERT(strat == HugeAllocationStrategy::CUSTOM_MMAP_BACKED, "Invalid huge strat");

    ck::scoped_lock l(m_lock);

    size_t mapping_size = ((size + sizeof(HugeHeader)) + 4095) & ~4095;
    if (mapping_size < 4096) {
      mapping_size = 4096;
    }

    // Allocate memory using mmap
    void* memory = alaska::mmap_alloc(mapping_size);

    // Create a HugeHeader object to store metadata
    HugeHeader* header = reinterpret_cast<HugeHeader*>(memory);
    header->mapping_size = mapping_size;
    header->allocation_size = size;

    // Add the allocated memory to the list
    list_add(&header->list, &this->allocations);

    // Return a pointer to the user memory
    return header->data();
  }


  bool HugeObjectAllocator::free(void* ptr) {
    if (strat == HugeAllocationStrategy::MALLOC_BACKED) {
      ::free(ptr);
      return true;
    }
    ALASKA_ASSERT(strat == HugeAllocationStrategy::CUSTOM_MMAP_BACKED, "Invalid huge strat");
    ck::scoped_lock l(m_lock);

    // Get the HugeHeader object from the user pointer
    HugeHeader* header = find_header(ptr);
    if (header == nullptr) return false;

    // Remove the entry from the list
    list_del(&header->list);

    // Free the memory using mmap
    alaska::mmap_free((void*)header, header->mapping_size);
    return true;
  }


  size_t HugeObjectAllocator::size_of(void* ptr) {
    if (strat == HugeAllocationStrategy::MALLOC_BACKED) return ::malloc_usable_size(ptr);

    ck::scoped_lock l(m_lock);
    HugeHeader* header = find_header(ptr);
    if (header != nullptr) {
      return header->allocation_size;
    }
    return 0;
  }


  bool HugeObjectAllocator::owns(void* ptr) {
    if (strat == HugeAllocationStrategy::MALLOC_BACKED) return true;

    ck::scoped_lock l(m_lock);
    // If the header is null, this allocator doesn't own it.
    return find_header(ptr) != nullptr;
  }

  HugeObjectAllocator::HugeHeader* HugeObjectAllocator::find_header(void* ptr) {
    HugeHeader* entry;
    // Iterate over the list
    list_for_each_entry(entry, &this->allocations, list) {
      // Check if the user pointer matches the data pointer in the header
      if (entry->data() == ptr) {
        return entry;
      }
    }
    // If no matching header is found, return nullptr
    return nullptr;
  }
}  // namespace alaska
