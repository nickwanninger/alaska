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

#include <alaska/heaps/LocalityPage.hpp>
#include <alaska/heaps/Heap.hpp>


namespace alaska {


  LocalityPage::~LocalityPage() {}


  bool LocalityPage::release_local(const alaska::Mapping &m, void *ptr) {
    auto *header = alaska::ObjectHeader::from(ptr);
    atomic_inc(this->freed_bytes, header->real_object_size());
    header->set_mapping(nullptr);  // This is how we free.

    if (--live_count == 0) {
      // All objects have been freed — reset the bump pointer so this page can be reused
      // rather than sitting in m_recent_full forever.
      bump_next = (void *)memory_start();
      freed_bytes = 0;
    }
    return true;
  }


  bool LocalityPage::localize(alaska::Mapping &m) {
    // Gonna assume the mapping is valid.
    void *old_location = m.get_pointer();
    auto *old_header = alaska::ObjectHeader::from(old_location);
    auto old_page = alaska::Heap::get_page(old_location);
    size_t object_size = old_header->object_size();

    // alaska::printf(
    //     "Localizing object %p of size %zu from page %p\n", old_location, object_size, old_page);

    // Bump allocate a new location.
    void *new_location = this->alloc(m, object_size);

    // Did we fail to allocate?
    if (new_location == nullptr) return false;

    // Copy the object over.
    memcpy(new_location, old_location, object_size);

    // Update the mapping to point to the new location.
    m.set_pointer(new_location);

    // Release the old mapping (TODO: does this need to be remote?)
    old_page->release_remote(m, old_location);

    return true;
  }

}  // namespace alaska
