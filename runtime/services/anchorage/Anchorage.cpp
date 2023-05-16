/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2023, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2023, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */


#include <anchorage/Anchorage.hpp>
#include <anchorage/SizeMap.hpp>
#include <anchorage/LinkedList.h>

#include <alaska.h>
#include <alaska/internal.h>
#include <alaska/service.hpp>
#include <alaska/barrier.hpp>

#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <malloc.h>

#include <heaplayers.h>


// using ChunkedMmapHeap = HL::ChunkHeap<2 * 1024 * 1024, HL::SizedMmapHeap>;
using ChunkedMmapHeap = HL::ChunkHeap<4 * 1024, HL::SizedMmapHeap>;

static HL::ANSIWrapper<HL::KingsleyHeap<HL::SizeHeap<HL::FreelistHeap<ChunkedMmapHeap>>,
    HL::SizeHeap<HL::FreelistHeap<ChunkedMmapHeap>>>>
    theHeap;

void alaska::service::alloc(alaska::Mapping *ent, size_t size) {
  // Realloc handles the logic for us. (If the first arg is NULL, it just allocates)
  // ent->ptr = theHeap.realloc(ent->ptr, size);
  ent->ptr = ::realloc(ent->ptr, size);
  // printf("%p\n", ent->ptr);
}


void alaska::service::free(alaska::Mapping *ent) {
  // theHeap.free(ent->ptr);
  ::free(ent->ptr);
  ent->ptr = NULL;
}

size_t alaska::service::usable_size(void *ptr) {
  return malloc_usable_size(ptr);
  // return theHeap.getSize(ptr);
}


namespace anchorage {
  /**
   * Span - a region of allocations of a specific size with associated handles
   */
  class SubHeap {
   public:
    SubHeap(off_t page_address, size_t length, uint16_t object_size, uint16_t object_count)
        : m_page(page_address)
        , m_length(length)
        , m_object_size(object_size)
        , m_object_count(object_count) {
      // We only allow (a maximum of) 256 objects per span. This assertion ensures that, but
      // it will never occur as the Arena that allocates these will (hopefully) be sane.
      assert(m_object_count <= 256);

      m_free = m_object_count;
      // zero out the handles array
      for (int i = 0; i < m_object_count; i++)
        m_handles[i] = 0;
    }


    unsigned get_ind(void *object) {
      return ((off_t)object - m_page) / m_object_size;
    }
    void *get_object(unsigned ind) {
      return (void *)(m_page + ind * m_object_size);
    }

    // Allocate an object for a certain alaska mapping, and
    // return a pointer to the object. Will return NULL if
    // there is nothing, for some reason
    void *alloc(alaska::Mapping &m) {
      // First, try to pop off the free list.
      void *obj = m_freelist.try_pop();

      // Then
      if (obj == nullptr) {
        if (m_next_bump < m_object_count) {
          obj = get_object(m_next_bump++);
        }
      }

      if (obj) {
        int ind = get_ind(obj);
        m_handles[ind] = reinterpret_cast<uint64_t>(&m);
        m.ptr = obj;
        m_free--;
        return obj;
      }
      return nullptr;
    }

    void free(void *object) {
      auto object_off = (uint64_t)object;
      if (object_off < m_page || object_off >= m_page + m_length * 4096) return;
      m_free++;
      auto ind = get_ind(object);
      m_freelist.push(object);
      m_handles[ind] = 0;
    }

   protected:
    anchorage::LinkedList m_freelist;
    uint32_t m_handles[256];   // array of handles (indicating which objects are allocated)
    uint64_t m_page;           // The address of the backing memory for this span
    size_t m_length;           // The length of this span in pages
    uint16_t m_next_bump = 0;  // Where the next bump object is
    uint16_t m_free = 0;       // how many objects are free in this span?
    uint16_t m_object_size;    // the size of each object
    uint16_t m_object_count;   // how many objects
  };
}  // namespace anchorage




pthread_t anchorage_barrier_thread;
static uint64_t barrier_interval_ms = 1000;
static void *barrier_thread_fn(void *) {
  // uint64_t thread_start_time = alaska_timestamp();
  while (1) {
    usleep(barrier_interval_ms * 1000);
    uint64_t start = alaska_timestamp();
    alaska_barrier();
    alaska::barrier::begin();
    alaska::barrier::end();
    uint64_t end = alaska_timestamp();
    (void)(end - start);
    // printf("Barrier %zu\n", end - start);
  }
  return NULL;
}


void alaska::service::init(void) {
  // dump_regions();
  // void *array[256];
  // HL::MmapHeap heap;
  // alaska::Mapping m;
  // size_t lastSize = 0;
  // printf("  size  tracking overhead\n");
  // for (size_t size = 16; size < anchorage::kMaxSize; size += 16) {
  //   const size_t objectClass = anchorage::SizeMap::SizeClass(size);
  //   const size_t objectSize = anchorage::SizeMap::class_to_size(objectClass);
  //   if (objectSize == lastSize) continue;
  //   lastSize = objectSize;
  //
  //   int pagecount = max(1LU, (objectSize * 256) / 4096);
  //   printf("%6zu  %5.2lf%%\n", objectSize,
  //       (sizeof(anchorage::SubHeap) / (float)(pagecount * 4096)) * 100.0);
  //   void *page = heap.malloc(4096 * pagecount);
  //   // Allocate the span
  //   anchorage::SubHeap s((off_t)page, pagecount, size, 256);
  //   // auto start = alaska_timestamp();
  //   int runs = 10000;
  //   for (int run = 0; run < runs; run++) {
  //     // printf("-----\n");
  //     for (int i = 0; i < 256; i++) {
  //       void *x = s.alloc(m);
  //       array[i] = x;
  //       if (x == NULL) {
  //         printf("failed!\n");
  //         exit(0);
  //       }
  //     }
  //     for (int i = 0; i < 256; i++) {
  //       s.free(array[i]);
  //     }
  //   }
  //   // auto end = alaska_timestamp();
  //   // printf(
  //   //     "    %lf seconds per iteration\n", (end - start) / (float)runs / 1000.0 / 1000.0 /
  //   //     1000.0);
  //   heap.free(page, 4096 * pagecount);
  // }
  // exit(-1);
  // for (size_t size = 16; size < anchorage::kMaxSize; size *= 1.1) {
  //   const size_t objectClass = anchorage::SizeMap::SizeClass(size);
  //   const size_t objectSize = anchorage::SizeMap::class_to_size(objectClass);
  //   printf("%zu -> %zu (%zu)\n", size, objectSize, objectClass);
  // }
  // exit(0);
  // pthread_create(&anchorage_barrier_thread, NULL, barrier_thread_fn, NULL);
}

void alaska::service::deinit(void) {
  // TODO:
}


void alaska::service::barrier(void) {
  // TODO:
}


void alaska::service::commit_lock_status(alaska::Mapping *ent, bool locked) {
  // TODO:
}



/// =================================================
const unsigned char anchorage::SizeMap::class_array_[kClassArraySize] = {
#include "size_classes.def"
};

const int32_t anchorage::SizeMap::class_to_size_[kClassSizesMax] = {
    16,
    16,
    32,
    48,
    64,
    80,
    96,
    112,
    128,
    160,
    192,
    224,
    256,
    320,
    384,
    448,
    512,
    640,
    768,
    896,
    1024,
    2048,
    4096,
    8192,
    16384,
};
