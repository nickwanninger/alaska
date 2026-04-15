/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2025, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2025, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

#pragma once

#include <stdint.h>
#include <alaska/alaska.hpp>
#include <alaska/handles/HandleTable.hpp>  // for alaska::last_mapping

namespace alaska {



  struct ObjectHeader final {
    union {
      struct {
        // The first 32 bits of the object header are the handle id. We use this
        // very often, so we want it to be fast as possible to access (without masks or anything)
        uint32_t handle_id;
        uint16_t size;  // The size of the object in bytes, not including the header.



        // And then theres some metadata
        union {
          struct {
            bool localized : 1;  // A marker to quickly indicate if the object is localized
            bool marked : 1;
          };
          uint8_t __metadata : 8;  // don't use this manually. just here to ensure space
        };
        int8_t placement_badness;
      };

      uint64_t header_data;
    };

    inline size_t object_size(void) const { return this->size; }
    inline size_t real_object_size(void) const { return object_size() + sizeof(ObjectHeader); }

    void set_object_size(size_t size_bytes) { this->size = size_bytes; }
    void *data(void) { return (void *)((off_t)this + sizeof(ObjectHeader)); }

    inline void reset(void) {
      __metadata = 0;
      // placement_badness = 0;
    }

    // Bitwise reset
    inline void reset(alaska::Mapping &m, size_t size_bytes) {
      uint32_t new_handle_id = m.handle_id();
      uint16_t new_size = size_bytes;
      this->header_data = new_handle_id | ((uint64_t)new_size << 32);
    }


    alaska::Mapping *get_mapping(void) const { return alaska::Mapping::from_handle_id(handle_id); }
    // Passing null here means the object is not mapped.
    inline void set_mapping(const alaska::Mapping *m) {
      // clear metadata. This is important because we don't want to accidentally
      // have uninitialized metadata.
      reset();
      handle_id = m->handle_id();
    }


    inline void update_mapping(void) {
      alaska::Mapping *m = get_mapping();
      if (m == nullptr) return;
      m->set_pointer(this);
    }


    static ObjectHeader *from(alaska::Mapping &m) { return from(m.get_pointer()); }
    static ObjectHeader *from(alaska::Mapping *m) { return from(m->get_pointer()); }
    static ObjectHeader *from(void *ptr) {
      return WORD_ALIGNED((ObjectHeader *)((off_t)ptr - sizeof(ObjectHeader)));
    }



    template <typename Fn>

    void walk(Fn fn) {
      size_t size = object_size();
      // alaska::printf("Walking header %p, %p, %zu\n", this, get_mapping(), size);

      uint64_t *start = (uint64_t *)data();
      uint64_t *end = (uint64_t *)((char *)start + size);
      uintptr_t offset = 0;


      for (uint64_t *ptr = start; ptr < end; ptr++, offset++) {
        // Grab the pointer
        uint64_t value = *ptr;

        void *pointee_data;
        alaska::Mapping *pointee_handle = nullptr;
        // Check if it is a valid handle.
        if (!alaska::check_mapping((void *)value, pointee_handle, pointee_data)) continue;
        alaska::ObjectHeader *header = alaska::ObjectHeader::from(pointee_data);
        // Skip if the handle is invalid because the offset into it is out of range/too large
        // auto handle_offset = alaska::Mapping::offset_from_handle(p);
        // if (handle_offset > header->object_size()) continue;

        // Check that the offset is valid.
        fn(pointee_handle, header);
      }
    }

    // Print a hex dump of the object for debugging.
    void hexdump(void);


  } __attribute__((packed));

  struct ObjectIterator {
    ObjectHeader *current;
    void *end_ptr;

    ObjectIterator(ObjectHeader *start, void *end_ptr)
        : current(start)
        , end_ptr(end_ptr) {
      if ((void *)current >= end_ptr) {
        current = nullptr;
      }
    }

    ObjectHeader *operator*() const { return current; }
    ObjectHeader *operator->() const { return current; }

    ObjectIterator &operator++() {
      if (current) {
        current = (ObjectHeader *)((char *)current + current->real_object_size());
        if ((void *)current >= end_ptr) {
          current = nullptr;
        }
      }
      return *this;
    }

    ObjectIterator operator++(int) {
      ObjectIterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const ObjectIterator &other) const { return current == other.current; }
    bool operator!=(const ObjectIterator &other) const { return current != other.current; }
  };


  struct ObjectRange {
    ObjectHeader *start;
    void *end_ptr;
    ObjectRange(ObjectHeader *start, void *end_ptr)
        : start(start)
        , end_ptr(end_ptr) {}
    ObjectIterator begin() const { return ObjectIterator(start, end_ptr); }
    ObjectIterator end() const { return ObjectIterator(nullptr, end_ptr); }
  };

  static constexpr size_t OBJECT_HEADER_SIZE = sizeof(ObjectHeader);
  // static_assert(sizeof(ObjectHeader) == 16, "ObjectHeader is not the right size!");
  static_assert(sizeof(ObjectHeader) == 8, "ObjectHeader is not the right size!");
}  // namespace alaska
