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
#include <anchorage/MainHeap.hpp>
#include <anchorage/LinkedList.h>

#include <alaska.h>
#include <alaska/alaska.hpp>
#include <alaska/service.hpp>
#include <alaska/barrier.hpp>

#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <malloc.h>

#include <heaplayers.h>



static anchorage::MainHeap heap();

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
const unsigned char anchorage::SizeMap::class_array_[anchorage::SizeMap::kClassArraySize] = {
#include "size_classes.def"
};

const int32_t anchorage::SizeMap::class_to_size_[anchorage::kClassSizesMax] = {
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
