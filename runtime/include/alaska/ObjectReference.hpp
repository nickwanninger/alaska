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

#include <alaska/HeapPage.hpp>
#include <alaska/SizeClass.hpp>
#include <alaska/ShardedFreeList.hpp>
#include <alaska/SizedAllocator.hpp>
#include <alaska/Runtime.hpp>

namespace alaska {

  class HeapPage;


  // This class is used as a way to represent an object in an abstract
  // heap. It is meant to abstract away much of the differences
  // between different heap implementations
  class ObjectReference final {
   public:
    ObjectReference(alaska::Mapping &m, void *data, size_t size, HeapPage &owner)
        : m_mapping(m)
        , m_data(data)
        , m_size(size)
        , m_owner(owner) {}


    static ObjectReference get(alaska::Mapping *m);
    static ObjectReference get(alaska::Mapping &m);

    // Get the mapping used for the object.
    alaska::Mapping &mapping(void) const { return this->m_mapping; }
    // Get a pointer to the object's data
    void *data(void) const { return m_data; }
    // Get the object's size, in bytes
    size_t size(void) const { return m_size; }
    // Get the owner's heap page.
    HeapPage &owner(void) const { return m_owner; }

    // Helpers:
    bool is_pinned(void) { return mapping().is_pinned(); }

    bool validate(void) const;

   private:
    alaska::Mapping &m_mapping;
    void *m_data;
    size_t m_size;
    HeapPage &m_owner;
  };



  inline bool ObjectReference::validate(void) const {
    // The data must exist (TODO: this may be wrong)
    if (data() == NULL) return false;
    // The mapping must point to the data.
    if (mapping().get_pointer() != data()) return false;
    // The owner page must manage the data
    if (not owner().contains(data())) return false;
    // The size must be equal to what the heap page says it is
    if (owner().size_of(data()) != size()) return false;
    return true;
  }


  inline ObjectReference ObjectReference::get(alaska::Mapping *m) { return get(*m); }
  inline ObjectReference ObjectReference::get(alaska::Mapping &m) {
    void *data = m.get_pointer();

    auto &rt = alaska::Runtime::get();
    HeapPage *owner = rt.heap.pt.get_unaligned(data);
    ALASKA_ASSERT(owner != NULL, "A heap must own each handle");
    size_t size = owner->size_of(data);


    return ObjectReference(m, data, size, *owner);
  }


  // helper methods `alaska::oref(mapping)`
  inline ObjectReference oref(alaska::Mapping *m) { return ObjectReference::get(*m); }
  inline ObjectReference oref(alaska::Mapping &m) { return ObjectReference::get(m); }

}  // namespace alaska
