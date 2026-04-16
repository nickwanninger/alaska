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

#include <alaska/core/Runtime.hpp>
#include <alaska/heaps/SizedPage.hpp>
#include <alaska/heaps/SizeClass.hpp>
#include <alaska/util/Logger.hpp>
#include <alaska/util/SizedAllocator.hpp>
#include <string.h>
#include <ck/template_lib.h>

namespace alaska {


  SizedPage::~SizedPage() { return; }


  long SizedPage::extend(long count) {
    FTR_SCOPE("Extend");
    size_t real_size = this->object_size + sizeof(ObjectHeader);
    long extended_count = 0;
    off_t start = (off_t)bump_next;
    off_t end = start + real_size * count;
    if (end > (off_t)objects_end) end = (off_t)objects_end;
    bump_next = (void *)end;

    for (off_t o = end - real_size; o >= start; o -= real_size) {
      freelist.free_local((SizePageBlock *)o);
      extended_count++;
    }

    return extended_count;
  }

  __attribute__((noinline))  // Don't inline this function, we want it to be a slow path.
  void *SizedPage::alloc_slow(const alaska::Mapping &m, alaska::AlignedSize size) {
    FTR_FUNCTION();

    // 1. Bump-allocate one slot directly — prefer fresh memory, touch only what we need.
    size_t real_size = object_size + sizeof(ObjectHeader);
    if ((char *)bump_next + real_size <= (char *)objects_end) {
      auto *block = (SizePageBlock *)bump_next;
      bump_next = (char *)bump_next + real_size;
      auto &header = block->header;
      header.set_mapping(&m);
      // Store the rounded size (object_size), not the requested size.
      // This ensures ObjectIterator uses correct offsets when walking objects.
      header.set_object_size(object_size);
      header.placement_badness = 0;
      return header.data();
    }

    // 2. Bump exhausted — swap remote_free into local_free and retry.
    freelist.swap();
    if (freelist.has_local_free()) return alloc(m, size);

    // 3. Truly out of space.
    return nullptr;
  }

  void *SizedPage::alloc(const alaska::Mapping &m, alaska::AlignedSize size) {
    SizePageBlock *p = freelist.pop();
    if (unlikely(p == nullptr)) {
      return alloc_slow(m, size);
    }

    auto &header = p->header;
    header.set_mapping(&m);
    // Store the rounded size (object_size), not the requested size.
    // This ensures ObjectIterator uses correct offsets when walking objects.
    header.set_object_size(object_size);
    header.placement_badness = 0;

    return p->header.data();
  }



  void SizedPage::set_size_class(int cls) {
    this->size_class = cls;

    // The size of the object.
    this->object_size = alaska::class_to_size(this->size_class);
    // The size of the block, which includes the header
    size_t real_size = this->object_size + sizeof(ObjectHeader);
    // The number of objects (and headers) that can fit in this page.

    size_t byte_capacity = memory_end() - memory_start();
    this->capacity = byte_capacity / real_size;

    void *objects = (void *)this->memory_start();


    this->objects_start = this->bump_next = objects;
    // the number of objects in this array
    this->objects_end = (void *)((uintptr_t)objects + (capacity * real_size));

    // alaska::printf("SizedPage<%lu> created with %zu objects of size %zu  (%p)\n",
    // this->object_size, this->capacity, real_size, this); alaska::printf("   %016lx to %016lx\n",
    // memory_start(), this->memory_end()); alaska::printf("   %016lx to %016lx\n",
    // (uintptr_t)this->objects_start, (uintptr_t)this->objects_end);

    // alaska::printf("  header: %016lx, owned by %016lx, this: %p\n", (uintptr_t)this->header(),
    // this->header()->owner, this);


    freelist = ShardedFreeList<SizePageBlock>();
  }



  long SizedPage::bump_age(void) {
    long count = 0;
    size_t real_object_size = this->object_size + sizeof(ObjectHeader);
    for (void *p = (void *)this->memory_start(); p < this->bump_next;
         p = (void *)((uintptr_t)p + real_object_size)) {
      auto obj = (ObjectHeader *)WORD_ALIGNED(p);
      auto m = obj->get_mapping();
      if (m) {
        m->set_fault_pending(false);
      }
      count++;
    }
    return count;
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
#if 0
    // --------------------------------------------------------------------------------- //
    // TEMP :: NOT LOCALIZING. HACKING THIS FUNCTION FOR ANALYSIS
    size_t real_object_size = this->object_size + sizeof(ObjectHeader);
    auto &rt = alaska::Runtime::get();
    if (real_object_size > 256) return 0;



    for (void *p = (void *)this->memory_start(); p < this->bump_next;
         p = (void *)((uintptr_t)p + real_object_size)) {


      auto obj = (ObjectHeader *)p;
      auto m = obj->get_mapping();
      if (m == nullptr) continue;

      size_t num_pointees = 0;
      rt.walk_handles(m, [&](alaska::Mapping *pointee) {
        num_pointees++;
      });
      if (num_pointees == 0) continue;


      alaska::printf("YUKON_EDGES %lu %d [", m->handle_id(), obj->marked);

      void *ptr = obj->data();

      rt.walk_handles(m, [&](alaska::Mapping *pointee) {
        alaska::printf(" %lu", pointee->handle_id());
      });

      alaska::printf("]\n");
    }

    // alaska::printf("SizedPage%lu analysis: out_pointers:%zu, extra_page:%zu, total:%zu\n",
    //     this->object_size, out_pointers, extra_page_pointers, total_pointers);
    // --------------------------------------------------------------------------------- //

    return 0;
#endif




    // The return value - how many objects this function has moved.
    long moved_objects = 0;

    // The first step is to clear the free list so that we don't have any
    // corruption problems. Later we will update it to point at the end of
    // the allocated objects.
    // allocator.reset_free_list();


    freelist.reset();




    // This algorithm is a two-finger walk. We have two pointers, one
    // pointing to the left (destination) and one pointing to the right
    // (source). We walk the left pointer forward until we find a free slot.
    // We then walk the right pointer forward starting at the left (or where
    // the right used to be, whichever is greater). If the right pointer
    // points to a used slot slot, we move that object to the location of the
    // left pointer and add the right pointer to the free list.
    //
    // One complication here is with object pinning, so the right pointer
    // considers pinned objects to be free, so it skips over them. If the
    // right pointer reaches the end location, we walk the left pointer
    // over the remainder of the heap, and any free slots are added to the
    // free list.

    off_t left = 0;
    off_t right = 0;
    off_t end = object_extent();
    off_t last_object_seen = -1;

    // A helper function to get the object header at a given index.
    // This just cleans up the code.
    auto get_object = [&](off_t index) -> ObjectHeader * {
      return (ObjectHeader *)((off_t)this->memory + index * (object_size + sizeof(ObjectHeader)));
    };


    // printf("Before compaction, %ld objects in %p to %p\n", this->memory, (void *)bump_next);
    // dump_live();


    // first, run the end back until we find an allocated object. This sets the
    // bounds for the right pointer. (It optimizes for SizedAllcoator::extend()
    // being called with a large value)
    while (end > 0) {
      auto obj = get_object(end - 1);
      if (is_allocated(obj)) {
        break;
      }
      end--;
    }

    while (left < end) {
      // if the right pointer is less than the left, we need to set it to
      // the left + 1, so we can move the object. This maintains that invariant
      if (right <= left) {
        right = left + 1;
      }

      auto left_obj = get_object(left);
      if (is_allocated(left_obj)) {
        left++;
        continue;
      }


      if (right == end) {
        // If the right pointer sees the end of the heap, we need to walk it back until
        // it finds an allocated object. That spot + 1 is the new end.
        // We then walk the left to that end and add any free slots found to the
        // free list.

        while (right > left) {
          if (is_allocated(get_object(right))) {
            break;
          }
          right--;
        }

        end = right;

        while (left < end) {
          auto left_obj = get_object(left);
          if (not is_allocated(left_obj)) {
            release_local(left_obj);
          }
          // if the left pointer points to an allocated object, we can't do anything so
          // walk it forward (towards the right)
          left++;
        }
        break;
      }



      // we found a free slot. we need to the object to the right to move here.
      bool found_right = false;  // Did we find a valid object to move?
      while (right < end) {
        // if the right pointer points to a free slot, we can't do anything so
        // move it forward
        auto right_obj = get_object(right);
        if (not is_allocated(right_obj)) {
          // TODO: make this the spot we "skip" to next time we need to pick a left value.
          right++;
          continue;
        }
        // if the right pointer points to a pinned object, we can't do anything
        // so move it forward
        auto handle_mapping = right_obj->get_mapping();
        if (handle_mapping->is_pinned()) {
          right++;
          continue;
        }

        // break from the loop if we have a valid object to move.
        found_right = true;
        break;
      }


      if (found_right) {
        auto obj_r = get_object(right);
        auto map_r = obj_r->get_mapping();

        auto obj_l = get_object(left);
        auto map_l = obj_l->get_mapping();


        // copy the object's data.
        memcpy(obj_l->data(), obj_r->data(), object_size);

        obj_r->set_mapping(0);  // clear the right mapping
        map_r->set_pointer(obj_l->data());
        obj_l->set_mapping(map_r);

        moved_objects++;
      }
    }
    // end = last_object_seen + 1;
    // dump("end of loop");
    this->bump_next = (void *)end;
    // dump_live();

    this->bump_next = get_object(end);

    return moved_objects;
  }


}  // namespace alaska
