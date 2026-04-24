/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2025, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2025, The Constellation Project
 * All rights reserved.
 *
 */

#include <alaska/disk/BufferPool.hpp>
#include <alaska/disk/Structure.hpp>
#include <alaska/heaps/Heap.hpp>


namespace alaska::disk {
  BufferPool::BufferPool(ck::box<Disk> disk, size_t size_mb)
      : disk(move(disk)) {
    size_t pages = (size_mb * 1024 * 1024) / page_size;
    pool_memory = mmap_alloc(page_size * pages);

    this->lru_head = this->lru_tail = nullptr;
    for (size_t i = 0; i < pages; i++) {
      auto f = new Frame();
      f->clear();
      f->memory = (void *)((uint8_t *)pool_memory + (page_size * i));

      f->lru_next = this->lru_head;
      this->lru_head = f;

      if (this->lru_tail == nullptr) this->lru_tail = f;

      frames.push(f);
    }


    if (this->disk->pageCount() == 0) {
      // Allocate a first page.
      newPage(false);
    }

    // Validate the header
    header = readValue<Header>(0);
    if (header.cookie != Header::COOKIE_VALUE) {
      // Initialize the header
      header.cookie = Header::COOKIE_VALUE;
      header.next_free = 0;
      header.num_structures = 0;
      syncHeader();
    }
  }

  BufferPool::~BufferPool(void) {
    flush();
    mmap_free(pool_memory, page_size * frames.size());
    if (getenv("ALASKA_BUFFERPOOL_STATS") != nullptr) {
      dumpStats();
    }
  }


  void BufferPool::flush(void) {
    for (auto &f : frames) {
      if (f->valid && f->dirty) {
        // RACE! (someone could be actively writing)
        f->dirty = false;
        writePage(f->page_id, f->memory);
      }
    }
  }

  bool BufferPool::readPage(uint64_t page_id, void *buf) {
    stat_disk_reads.track();
    // printf("[BufferPool] READ  %zu\n", page_id);
    return disk->readPage(page_id, buf);
  }



  bool BufferPool::writePage(uint64_t page_id, void *buf) {
    stat_disk_writes.track();
    // printf("[BufferPool] WRITE %zu\n", page_id);
    return disk->writePage(page_id, buf);
  }

  FrameGuard BufferPool::newPage(bool useFreeList) {
    if (useFreeList && header.next_free != 0) {
      FrameGuard new_page = getPage(header.next_free);
      header.next_free = *new_page.get<uint64_t>();  // pop!
      syncHeader();                                  // write out the header
      new_page.wipePage();                           // clear the new page
      return new_page;
    }
    // Bump allocate
    uint64_t new_id = disk->pageCount();
    disk->ensureFileSize(new_id * page_size + page_size);
    // No need to wipe the new page if we bump allocate, as the kernel
    // guarentees the data will be zeroed for us.
    return getPage(new_id);
  }

  void BufferPool::freePage(FrameGuard &&frame) {
    auto id = frame.page_id();
    *frame.getMut<uint64_t>() = header.next_free;
    header.next_free = id;
  }



  FrameGuard BufferPool::getPage(uint64_t page_id) {
    ck::scoped_lock lock(frame_table_lock);
    if (page_id >= disk->pageCount()) {
      printf("cannot read page %zu outside bounds of file. call newPage()\n", page_id);
      abort();
    }

    stat_accesses++;

    auto it = frame_table.find(page_id);
    if (it != frame_table.end()) {
      stat_hits++;
      auto *f = it->value;
      ALASKA_ASSERT(f->valid, "must be valid to be in the frame table");
      if (f->valid) {
        // Bump the LRU for this node.
        // bump_frame(f);

        ALASKA_ASSERT(f->page_id == page_id, "frame table invalid");
        lruReference(f);
        f->ref = true;
        return f;
      }
    }

    uint64_t len = 0;
    for (Frame *f = lru_head; f != nullptr; f = f->lru_next) {
      len++;
    }

    // Walk the lru list from lru_head to end
    for (Frame *f = lru_head; f != nullptr; f = f->lru_next) {
      if (f->get_refcount() != 0) continue;
      // If the frame is valid (has a page), we might need to flush it.
      if (f->valid) {
        if (f->dirty) {
          // evict
          stat_writebacks++;
          writePage(f->page_id, f->memory);
          f->dirty = false;
        }
        frame_table.remove(f->page_id);  // Remove old frame mapping
      }

      f->reset(page_id);                // Reset the frame to the new page
      frame_table.set(page_id, f);      // Add new frame mapping
      readPage(f->page_id, f->memory);  // Read the data
      f->valid = true;
      frame_to_end(f);  // Move the frame to the end of the LRU list

      return f;
    }
    return nullptr;

    // int retry_loop_count = 3;
    // // LRU Evict
    // for (size_t i = 0; i < (size_t)frames.size() * retry_loop_count; i++) {
    //   // Grab the location to look at using clock algorithm
    //   off_t loc = clock_hand++;
    //   if (clock_hand >= (off_t)frames.size()) clock_hand = 0;
    //   auto &f = frames[loc];

    //   // If someone is using it, we can't use it, skip.
    //   if (f->get_refcount() != 0) continue;

    //   // [clock] if the frame has been referenced, clear that fact and continue.
    //   if (f->ref) {
    //     f->ref = false;
    //     continue;
    //   }

    //   // If the frame is valid (has a page), we might need to flush it.
    //   if (f->valid) {
    //     if (f->dirty) {
    //       // evict
    //       stat_writebacks++;
    //       writePage(f->page_id, f->memory);
    //       f->dirty = false;
    //     }
    //     frame_table.remove(f->page_id);  // Remove old frame mapping
    //   }

    //   f->reset(page_id);                // Reset the frame to the new page
    //   frame_table.set(page_id, f);      // Add new frame mapping
    //   readPage(f->page_id, f->memory);  // Read the data
    //   f->valid = true;
    //   frame_to_end(f);  // Move the frame to the end of the LRU list

    //   return f;
    // }
    // printf("found nothing.\n");

    return nullptr;
  }




  void BufferPool::dumpStats(void) {
    printf("Buffer Pool Stats:\n");

    printf("  accesses:    %12zu (%.1f/s)\n", stat_accesses.read(), stat_accesses.digest());
    printf("  hits:        %12zu (%.1f/s)\n", stat_hits.read(), stat_hits.digest());
    printf("  writebacks:  %12zu (%.1f/s)\n", stat_writebacks.read(), stat_writebacks.digest());

    printf("  disk reads:  %12zu (%.1f/s)\n", stat_disk_reads.read(), stat_disk_reads.digest());
    printf("  disk writes: %12zu (%.1f/s)\n", stat_disk_writes.read(), stat_disk_writes.digest());

    printf("  -----\n");
    printf("  hitrate:     %12.4f%%\n", 100.0 * stat_hits.read() / stat_accesses.read());
    printf("\n");
  }



  ck::opt<uint64_t> BufferPool::getStructure(const char *name) {
    auto max_structures = page_size / sizeof(StructureEntry);
    for (size_t i = 0; i < header.num_structures; i++) {
      if (strcmp(header.structures[i].name, name) == 0) {
        return header.structures[i].page_id;
      }
    }
    return None;
  }

  void BufferPool::addStructure(const char *name, uint64_t page_id) {
    if (strlen(name) >= sizeof(StructureEntry::name)) {
      fprintf(stderr, "structure name '%s' too long\n", name);
      abort();
    }


    for (size_t i = 0; i < header.num_structures; i++) {
      if (strcmp(header.structures[i].name, name) == 0) {
        fprintf(stderr, "structure %s already exists\n", name);
        abort();
        return;
      }
    }


    if (header.num_structures >= Header::MAX_STRUCTURE_COUNT) {
      fprintf(stderr, "too many structures. Cannot add %s\n", name);
      abort();
    }
    strncpy(header.structures[header.num_structures].name, name,
            sizeof(header.structures[header.num_structures].name));
    header.structures[header.num_structures].page_id = page_id;

    header.num_structures++;
    syncHeader();
  }


  void BufferPool::lruReference(Frame *frame) {
    // printf("LRU: %zu\n", frame->page_id);
    frame->ref = true;
  }


}  // namespace alaska::disk


#include <alaska/disk/ChainedHashTable.hpp>
#if 0
static void __attribute__((constructor)) init(void) {
  uint64_t count = 512 * 512;

  {
    alaska::disk::BufferPool bp("heap.db", 8);
    alaska::disk::ChainedHashTable<uint64_t, uint64_t> ht(bp, "hashtable");
    for (uint64_t i = 0; i < count; i++) {
      ht.insert(i, i);
    }
  }


  {
    alaska::disk::BufferPool bp("heap.db", 4);
    alaska::disk::ChainedHashTable<uint64_t, uint64_t> ht(bp, "hashtable");
    for (uint64_t i = 0; i < count; i++) {
      auto s = ht.get(i);
      ALASKA_ASSERT(s.has_value(), "must be found");
      ALASKA_ASSERT(s.take() == i, "must be equal");
    }
  }




  // alaska::disk::Structure s(bp, "test");

  // exit(-1);
}
#endif
