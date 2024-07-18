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


#include <alaska/Heap.hpp>
#include <alaska/HugeObjectAllocator.hpp>

namespace alaska {
  HugeObjectAllocator::HugeObjectAllocator() {}
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
    printf("size = 0x%zx\n", size);
    ck::scoped_lock l(m_lock);

    size_t mapping_size = (size + 4095) & ~4095;
    if (mapping_size < 4096) {
      mapping_size = 4096;
    }
    printf("mapping_size = 0x%zx\n", mapping_size);
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
    ck::scoped_lock l(m_lock);
    HugeHeader* header = find_header(ptr);
    if (header != nullptr) {
      return header->allocation_size;
    }
    return 0;
  }


  bool HugeObjectAllocator::owns(void* ptr) {
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