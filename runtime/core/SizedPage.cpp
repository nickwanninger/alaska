/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2024, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2024, The Constellation Project
 * All rights reserved.
 *
 */
/** This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

#include <alaska/SizedPage.hpp>
#include <alaska/SizeClass.hpp>
#include <alaska/Logger.hpp>
#include <alaska/SizedAllocator.hpp>
#include <string.h>
#include <ck/template_lib.h>

namespace alaska {


  SizedPage::~SizedPage() { return; }

  void *SizedPage::alloc(const alaska::Mapping &m, alaska::AlignedSize size) {
    void *o = allocator.alloc();
    if (unlikely(o == nullptr)) return nullptr;

    // All paths for allocation go through this path.
    long oid = this->object_to_ind(o);
    Header *h = this->ind_to_header(oid);
    h->set_mapping(const_cast<alaska::Mapping *>(&m));
    h->size_slack = this->object_size - size;
    return o;
  }


  bool SizedPage::release_local(const alaska::Mapping &m, void *ptr) {
    long oid = object_to_ind(ptr);
    auto *h = ind_to_header(oid);
    h->set_mapping(nullptr);
    h->size_slack = 0;
    allocator.release_local(ptr);
    return true;
  }


  bool SizedPage::release_remote(const alaska::Mapping &m, void *ptr) {
    auto oid = object_to_ind(ptr);
    auto *h = ind_to_header(oid);
    h->set_mapping(nullptr);
    h->size_slack = 0;
    allocator.release_remote(ptr);
    return true;
  }



  void SizedPage::set_size_class(int cls) {
    size_class = cls;

    size_t object_size = alaska::class_to_size(cls);
    this->object_size = object_size;

    capacity = (double)alaska::page_size /
               (double)(round_up(object_size, alaska::alignment) + sizeof(SizedPage::Header));
    live_objects = 0;

    if (capacity == 0) {
      log_warn(
          "SizedPage allocated with an object which was too large (%zu bytes in a %zu byte page. "
          "max object = %zu). No capacity!",
          object_size, alaska::page_size, alaska::max_object_size);
    } else {
      log_trace("set_size_class(%d). os=%zu, cap=%zu", cls, object_size, capacity);
    }


    // Headers live first
    headers = (SizedPage::Header *)memory;
    memset(headers, 0, sizeof(SizedPage::Header) * capacity);
    // Then, objects are placed later.
    objects = (Block *)round_up((uintptr_t)(headers + capacity), alaska::alignment);


    log_info("cls = %-2d, memory = %p, headers = %p, objects = %p", cls, memory, headers, objects);

    // initialize
    allocator.configure(objects, object_size, capacity);
  }


  // The goal of this function is to take a fragmented heap, and
  // apply a simple two-finger compaction algorithm.  We start with
  // a heap that looks like this (# is allocated, _ is free)
  //  [###_##_#_#__#_#_#__#]
  // and have pointers to the start and end like this:
  //  [###_##_#_#__#_#_#__#]
  //   ^                  ^
  // We iterate until those pointers are the same. If the left
  // pointer points to an allocated object, we increment it. If the
  // right pointer points to a free object (or a pinned), it is
  // decremented.  If neither pointer changes, the left points to a
  // free object and the right to an allocated one, we swap their
  // locations then inc right and dec left.
  //
  // When this process is done you should have a heap that looks
  // like this:
  //  [###########_________]
  // The last step in this process is to "reset" the free list to be
  // empty and for "expansion" to begin at the end of the allocated
  // objects. Then, if deemed beneficial, you can use MADV_DONTNEED
  // to free memory back to the kernel.
  //
  // This function then returns how many objects it moved
  long SizedPage::compact(void) {
    // If there's nothing to do, early return.
    // TODO: threshold this.
    if (live_objects == capacity) return 0;

    // The first step is to clear the free list so that we don't have any
    // corruption problems. Later we will update it to point at the end of
    // the allocated objects.
    allocator.reset_free_list();

    // The return value - how many objects this function has moved.
    long moved_objects = 0;

    // A pointer which tracks the last object in the heap. We have this
    // in the event of a pinned object, P:
    //  [###########_____P___]
    //                   ^ last_object points here.
    // If this is not null, whenever we swap an object such that the right
    // pointer points to a free slot, we add that location to the local free
    // list of the allocator.
    // NOTE: this actually is a pointer to the header, not the object.
    Header *last_object = nullptr;

    Header *left = headers;  // the first object
    Header *right = headers + capacity - 1;




    while (right > left) {
      // if left points to an allocated object, we can't do anything so
      // walk it forward (towards the right)
      if (not left->is_free()) {
        left++;
        continue;
      }

      // if the right points to a free slot, we can't do anything so
      // similarly walk it forward (toward the left)
      if (right->is_free()) {
        // but, if the last object is set, we need to add this to the
        // free list because it constitutes a gap in the heap which
        // would not be allocated in the future by the
        if (last_object != nullptr) {
          void *free_slot = ind_to_object(header_to_ind(right));
          allocator.release_local(free_slot);
        }

        right--;
        continue;
      }

      auto handle_mapping = right->get_mapping();
      // At this point, we have the setup we want: the left pointer
      // points to a free slot where the object pointed to by the
      // right pointer can be moved. The one situation that could
      // block us from doing this is if the object we want to move is
      // pinned. If it is we maybe update `last_object` and tick
      // the right pointer.
      if (handle_mapping->is_pinned()) {
        if (last_object == nullptr) last_object = right;
        right--;
        continue;
      }


      // Now, we are free to apply the compaction!
      void *object_to_move = ind_to_object(header_to_ind(right));
      void *free_slot = ind_to_object(header_to_ind(left));

      // Move the memory of the object to the free slot
      memmove(free_slot, object_to_move, object_size);
      // And poison the old location.
      memset(object_to_move, 0xF0, object_size);

      // Update the mapping so the handle points to the right location.
      handle_mapping->set_pointer(free_slot);
      // make sure the headers make sense
      *left = *right;

      ALASKA_ASSERT(left->get_mapping() == right->get_mapping(), ".");
      right->set_mapping(0);
      right->size_slack = 0;
      moved_objects++;

      // Note that we don't ight along here. We deal with that on the
      // next iteration of the loop to handle edge cases.
      // left++;
    }

    // If we were lucky, and no pinned object were found, we need to
    // point last_object to the end of the heap, which at this point
    // is `right`
    if (last_object == nullptr) last_object = right;

    // Reset the bump pointer of the free list to right after the
    // last object in the heap.
    void *after_heap = ind_to_object(header_to_ind(last_object + 1));
    allocator.reset_bump_allocator(after_heap);


    return moved_objects;
  }



  void SizedPage::validate(void) {}

  long SizedPage::jumble(void) {
    char buf[this->object_size];  // BAD

    // Simple two finger walk to swap every allocation
    long left = 0;
    long right = capacity - 1;
    long swapped = 0;

    while (right > left) {
      auto *lo = ind_to_header(left);
      auto *ro = ind_to_header(right);

      auto *rm = ro->get_mapping();
      auto *lm = lo->get_mapping();

      if (rm == NULL or rm->is_pinned()) {
        right--;
        continue;
      }

      if (lm == NULL or lm->is_pinned()) {
        left++;
        continue;
      }


      // swap the handles!

      // Grab the two pointers
      void *left_ptr = ind_to_object(left);
      void *right_ptr = ind_to_object(right);

      // Swap their data
      memcpy(buf, left_ptr, object_size);
      memcpy(left_ptr, right_ptr, object_size);
      memcpy(right_ptr, buf, object_size);

      // Tick the fingers
      left++;
      right--;


      // Swap the mappings and whatnot
      auto lss = lo->size_slack;
      auto rss = ro->size_slack;

      rm->set_pointer(left_ptr);
      ro->set_mapping(lm);
      ro->size_slack = lss;

      lm->set_pointer(right_ptr);
      lo->set_mapping(rm);
      lo->size_slack = rss;

      swapped++;
    }

    return swapped;
  }


  void SizedPage::dump_html(FILE *stream) {
    fprintf(stream, "<td>SizedPage(%zu)</td>", this->object_size);

    fprintf(stream, "<td class='heapdata'>");
    enum state {
      unknown,
      allocated,
      freed,
    };
    state last_state = unknown;
    size_t objects = 0;
    for (long i = 0; true; i++) {
      auto *h = ind_to_header(i);
      state curstate = h->is_free() ? freed : allocated;
      bool print = false;

      if (last_state != unknown and curstate != last_state) print = true;
      if (i == capacity - 1) print = true;

      if (print) {
        fprintf(stream, "<div class='el %s' style='width: %lupx'>",
            last_state == freed ? "fr" : "al", objects);
        fprintf(stream, "</div>");
        objects = 0;
      }

      last_state = curstate;
      objects++;

      if (i == capacity - 1) {
        break;
      }
    }

    fprintf(stream, "</td>");
  }


  void SizedPage::dump_json(FILE *stream) {
    fprintf(stream, "{\"name\": \"SizedPage\", \"object_size\": %zu, \"avail\": %zu}"
        object_size, this->available());
  }

}  // namespace alaska
