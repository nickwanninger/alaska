/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2025, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2025, The Constellation Project
 * All rights reserved.
 *
 */

#pragma once


#include <stdint.h>
#include <stdlib.h>
#include <alaska/util/utils.h>
#include <ck/map.h>
#include <ck/option.h>
#include <ck/lock.h>
#include <ck/vec.h>
#include <ck/box.h>

#include <alaska/util/RateCounter.hpp>

// Disk includes
#include <alaska/disk/Frame.hpp>
#include <alaska/disk/Disk.hpp>

namespace alaska::disk {


  class BufferPool final {
   public:
    BufferPool(ck::box<Disk> disk, size_t size = 64);
    ~BufferPool();


    // No copy, no move, etc..
    BufferPool(BufferPool const &) = delete;
    void operator=(BufferPool const &t) = delete;
    BufferPool(BufferPool &&) = delete;



    // Get a page which was previously allocated - aborts
    // if it is invalid, be careful!
    FrameGuard getPage(uint64_t page_id);
    // Allocate a new Page
    FrameGuard newPage(bool useFreeList = true);
    // Free a Page, can be used again later
    void freePage(FrameGuard &&frame);

    // flush all dirty pages.
    void flush(void);

    // Dump statistics
    void dumpStats(void);


    template <typename T>
    T readValue(off_t byte_offset) {
      return *getPage(byte_offset / page_size).get<T>(byte_offset % page_size);
    }
    template <typename T>
    void writeValue(off_t byte_offset, const T &val) {
      *getPage(byte_offset / page_size).getMut<T>(byte_offset % page_size) = val;
    }

    ck::opt<uint64_t> getStructure(const char *name);
    void addStructure(const char *name, uint64_t root_page_id);


    template <typename T>
    GuardedMut<T> getMutOverlay(uint64_t page_id, off_t byte_offset = 0) {
      return GuardedMut<T>(getPage(page_id), byte_offset);
    }
    template <typename T>
    Guarded<T> getOverlay(uint64_t page_id, off_t byte_offset = 0) {
      return Guarded<T>(getPage(page_id), byte_offset);
    }

    float hitrate(void) { return 100.0 * stat_hits.read() / stat_accesses.read(); }

   private:
    struct StructureEntry {
      char name[16];
      uint64_t page_id;
    };

    struct Header {
      static constexpr uint32_t COOKIE_VALUE = 0xFEF1F0F3;
      static constexpr uint32_t MAX_STRUCTURE_COUNT = 32;
      uint32_t cookie;
      uint64_t next_free;

      uint64_t reserved[16];

      uint64_t num_structures;
      StructureEntry structures[MAX_STRUCTURE_COUNT];
    };

    static_assert(sizeof(Header) < page_size, "Header must be 512 bytes");
    Header header;




    // We implement a trival clock algorithm LRU approx.
    off_t clock_hand = 0;
    ck::vec<Frame *> frames;

    ck::map<uint64_t, Frame *> frame_table;
    ck::mutex frame_table_lock;

    // The head is the least recently used, tail is the most recently used.
    // When a new page is read, it is inserted at the tail.
    // When a page is re-used, it moves closer to the tail by one.
    Frame *lru_head, *lru_tail;

    void *pool_memory = NULL;

    ck::box<Disk> disk;

    // Statistics
    alaska::RateCounter stat_accesses;
    alaska::RateCounter stat_hits;
    alaska::RateCounter stat_writebacks;
    alaska::RateCounter stat_disk_reads;
    alaska::RateCounter stat_disk_writes;


    inline void syncHeader(void) { writeValue<Header>(0, header); }

    // These exist just to track stats. They just forward to the disk
    bool readPage(uint64_t page_id, void *buf);
    bool writePage(uint64_t page_id, void *buf);

    void lruReference(Frame *frame);

    inline void bump_frame(Frame *frame) {
      // if (!frame || !frame->lru_next) return;  // Nothing to do

      // Frame *next = frame->lru_next;
      // Frame *prev = frame->lru_prev;

      // // Update adjacent frames
      // frame->lru_next = next->lru_next;
      // frame->lru_prev = next;
      // next->lru_next = frame;
      // next->lru_prev = prev;

      // // Detach
      // if (prev) {
      //   prev->lru_next = next;
      // } else {
      //   this->lru_head = next;  // Frame was head
      // }
      // if (next) {
      //   next->lru_prev = prev;
      // } else {
      //   this->lru_tail = prev;  // Frame was tail
      // }
    }


    inline void frame_to_end(Frame *frame) {
      if (!frame || frame->lru_next == nullptr) return;  // Already at end or null

      Frame *next = frame->lru_next;
      Frame *prev = frame->lru_prev;

      // if there was a previous, update it.
      // if there wasn't we were at the front, so update the head.
      if (prev) {
        prev->lru_next = next;
      } else {
        this->lru_head = next;  // Frame was head
      }
      // there must have been a head. Update it.
      next->lru_prev = prev;

      // now insert at the end.
      this->lru_tail->lru_next = frame;
      frame->lru_prev = this->lru_tail;
      frame->lru_next = nullptr;
      this->lru_tail = frame;
    }
  };
}  // namespace alaska::disk