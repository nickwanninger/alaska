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


#include <anchorage/Chunk.hpp>
#include <anchorage/Block.hpp>
#include <anchorage/Defragmenter.hpp>

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


anchorage::MainHeap *the_heap = nullptr;

// using ChunkedMmapHeap = HL::ChunkHeap<2 * 1024 * 1024, HL::SizedMmapHeap>;
using ChunkedMmapHeap = HL::ChunkHeap<4 * 1024, HL::SizedMmapHeap>;

static HL::ANSIWrapper<HL::KingsleyHeap<HL::SizeHeap<HL::FreelistHeap<ChunkedMmapHeap>>,
    HL::SizeHeap<HL::FreelistHeap<ChunkedMmapHeap>>>>
    theHeap;

void alaska::service::alloc(alaska::Mapping *ent, size_t size) {
  auto &m = *ent;
  using namespace anchorage;
  Block *new_block = NULL;
  Chunk *new_chunk = NULL;
  // printf("alloc %zu into %p (there are %zu chunks)\n", size, &m, anchorage::Chunk::all().size());
  //  for (auto *chunk : anchorage::Chunk::all()) {
  // 	printf("  chunk %p (wl:%zu)\n", chunk, chunk->high_watermark);
  // }

  // attempt to allocate from each chunk
  for (auto *chunk : anchorage::Chunk::all()) {
    auto blk = chunk->alloc(size);
    // if the chunk could allocate, use the pointer it created
    if (blk) {
      new_chunk = chunk;
      new_block = blk;
      break;
    }
  }
  (void)new_chunk;

  if (new_block == NULL) {
    size_t required_pages = (size / anchorage::page_size) * 2;
    if (required_pages < anchorage::min_chunk_pages) {
      required_pages = anchorage::min_chunk_pages;
    }

    new anchorage::Chunk(required_pages);  // add a chunk

    return alaska::service::alloc(ent, size);
  }

  if (m.ptr != NULL) {
    Block *old_block = anchorage::Block::get(m.ptr);
    auto *old_chunk = anchorage::Chunk::get(m.ptr);
    old_block->mark_as_free(*old_chunk);
    size_t copy_size = old_block->size();
    if (size < copy_size) copy_size = size;
    memcpy(new_block->data(), ent->ptr, copy_size);
  }

  new_block->set_handle(&m);
  ent->ptr = new_block->data();

	new_chunk->dump(new_block, "Allocate");
  // ent->size = size;
  // printf("alloc: %p %p (%zu)", &m, ent->ptr, ent->size);
  // new_chunk->dump(new_block, "Alloc");
  return;
}


void alaska::service::free(alaska::Mapping *ent) {
  auto *chunk = anchorage::Chunk::get(ent->ptr);
  if (chunk == NULL) {
    fprintf(
        stderr, "[anchorage] attempt to free a pointer not managed by anchorage (%p)\n", ent->ptr);
    return;
  }

  // Poison the buffer
  // memset(m.ptr, 0xFA, m.size);
  // Free the block
  auto *blk = anchorage::Block::get(ent->ptr);
  chunk->free(blk);

  // m.size = 0;
  ent->ptr = nullptr;
  // tell the block to coalesce the best it can
}

size_t alaska::service::usable_size(void *ptr) {
  // return malloc_usable_size(ptr);
  // return theHeap.getSize(ptr);
  return 0;
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
  // Make sure the anchorage heap is available
  // the_heap = new anchorage::MainHeap;
  anchorage::allocator_init();
  // pthread_create(&anchorage_barrier_thread, NULL, barrier_thread_fn, NULL);
}

void alaska::service::deinit(void) {
  // TODO:
  anchorage::allocator_deinit();
}


void alaska::service::barrier(void) {
	for (auto chunk : anchorage::Chunk::all()) {
		chunk->dump(NULL, "barrier");
	}
	return;
  anchorage::Defragmenter defrag;
  defrag.run(anchorage::Chunk::all());
}


void alaska::service::commit_lock_status(alaska::Mapping *ent, bool locked) {
  auto *block = anchorage::Block::get(ent->ptr);
  block->mark_locked(locked);
}



/// =================================================
const unsigned char anchorage::SizeMap::class_array_[anchorage::SizeMap::kClassArraySize] = {
#include "size_classes.def"
};

const int32_t anchorage::SizeMap::class_to_size_[anchorage::classSizesMax] = {
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
