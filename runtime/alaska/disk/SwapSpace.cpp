/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2026, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2026, The Constellation Project
 * All rights reserved.
 *
 */

#include <alaska/core/Runtime.hpp>
#include <alaska/core/ThreadCache.hpp>
#include <alaska/disk/SwapSpace.hpp>
#include <alaska/handles/ObjectHeader.hpp>
#include <alaska/heaps/ArenaHeap.hpp>
#include <alaska/heaps/Heap.hpp>
#include <string.h>

namespace alaska::disk {

  SwapSpace::SwapSpace(alaska::Runtime &runtime, BufferPool &pool, const char *name)
      : Structure(pool, name)
      , runtime(runtime) {
    auto root = load_root();
    if (root.cookie != COOKIE) {
      root.cookie = COOKIE;
      root.head_page_id = pool.newPage().page_id();
      root.head_used_bytes = 0;
      root.swapped_out_count = 0;
      root.fault_in_count = 0;
      root.discard_count = 0;
      store_root(root);
      page_used_bytes.set(root.head_page_id, 0);
      live_records.set(root.head_page_id, 0);
    }
  }

  bool SwapSpace::swap_out(alaska::Mapping *mapping) {
    if (mapping == nullptr) return false;

    // Grab a lock to ensure serialized access.
    ck::scoped_lock guard(lock);

    // Validate the mapping.
    if (mapping->is_free() || mapping->is_pinned() || mapping->is_swapped_out()) return false;

    // Get the resident object and its header.
    void *ptr = mapping->get_pointer();
    if (ptr == nullptr) return false;

    auto *header = alaska::ObjectHeader::from(ptr);
    if (header == nullptr || header->handle_id == 0) return false;

    // Compute how many bytes we need to write for this record and check if it fits in the next page.
    size_t object_size = header->object_size();
    size_t record_bytes = sizeof(alaska::ObjectHeader) + object_size;
    if (record_bytes > page_size) return false;

    RootHeader root = load_root();
    if (page_used_bytes.find(root.head_page_id) == page_used_bytes.end()) {
      page_used_bytes.set(root.head_page_id, root.head_used_bytes);
    }
    if (live_records.find(root.head_page_id) == live_records.end()) {
      live_records.set(root.head_page_id, 0);
    }

    if (root.head_used_bytes + record_bytes > page_size) {
      root.head_page_id = pool.newPage().page_id();
      root.head_used_bytes = 0;
      page_used_bytes.set(root.head_page_id, 0);
      live_records.set(root.head_page_id, 0);
    }

    uint64_t record_offset = head_record_offset(root);
    auto page = pool.getPage(root.head_page_id);
    memcpy(page.getMut<void>(root.head_used_bytes), header, record_bytes);

    root.head_used_bytes += record_bytes;
    root.swapped_out_count++;
    page_used_bytes.set(root.head_page_id, root.head_used_bytes);
    note_live_record(root.head_page_id);
    store_root(root);

    if (!release_resident_object(mapping, ptr)) {
      note_dead_record(root.head_page_id);
      reclaim_page_if_empty(root.head_page_id, root);
      store_root(root);
      return false;
    }

    this->swap_out_byte_rate.track(record_bytes);

    mapping->set_swapped(record_offset, object_size);
    return true;
  }

  bool SwapSpace::handle_fault(alaska::Mapping *mapping, alaska::ThreadCache *actor) {
    if (mapping == nullptr || actor == nullptr) return false;

    ck::scoped_lock guard(lock);

    if (!mapping->is_swapped_out()) return false;

    uint64_t target_offset = mapping->swap_offset();
    uint64_t page_id = target_offset / page_size;

    auto used_it = page_used_bytes.find(page_id);
    if (used_it == page_used_bytes.end()) return false;

    size_t used_bytes = used_it->value;
    auto page = pool.getPage(page_id);

    bool all_restored = true;
    size_t offset = 0;
    while (offset < used_bytes) {
      const auto *disk_header = page.get<alaska::ObjectHeader>(offset);
      if (!validate_record(used_bytes, offset, *disk_header)) return false;

      size_t object_size = disk_header->object_size();
      size_t record_bytes = sizeof(alaska::ObjectHeader) + object_size;
      uint64_t record_offset = page_id * page_size + offset;
      alaska::Mapping *record_mapping = alaska::Mapping::from_handle_id(disk_header->handle_id);

      if (record_mapping != nullptr && record_mapping->is_swapped_out() &&
          record_mapping->swap_offset() == record_offset) {
        auto *dst_header = actor->allocate_object(object_size, *record_mapping);
        this->fault_in_byte_rate.track(object_size + sizeof(alaska::ObjectHeader));
        if (dst_header == nullptr) {
          all_restored = false;
        } else {
          memcpy(dst_header->data(), page.get<void>(offset + sizeof(alaska::ObjectHeader)),
                 object_size);
          dst_header->marked = disk_header->marked;
          dst_header->placement_badness = disk_header->placement_badness;
          dst_header->localized = 0;
          note_dead_record(page_id);
        }
      }

      offset += record_bytes;
    }

    if (page_is_empty(page_id)) {
      RootHeader root = load_root();
      root.fault_in_count++;
      reclaim_page_if_empty(page_id, root);
      store_root(root);
    }

    return all_restored;
  }

  bool SwapSpace::discard(alaska::Mapping *mapping) {
    if (mapping == nullptr) return false;

    ck::scoped_lock guard(lock);

    if (!mapping->is_swapped_out()) return false;

    uint64_t page_id = mapping->swap_offset() / page_size;
    if (live_records.find(page_id) == live_records.end()) return false;

    RootHeader root = load_root();
    note_dead_record(page_id);
    mapping->reset();
    root.discard_count++;
    reclaim_page_if_empty(page_id, root);
    store_root(root);
    return true;
  }

  SwapSpace::RootHeader SwapSpace::load_root() {
    return *pool.getOverlay<RootHeader>(root_page_id);
  }

  void SwapSpace::store_root(const RootHeader &root) {
    *pool.getMutOverlay<RootHeader>(root_page_id) = root;
  }

  uint64_t SwapSpace::head_record_offset(const RootHeader &root) const {
    return root.head_page_id * page_size + root.head_used_bytes;
  }

  bool SwapSpace::page_is_empty(uint64_t page_id) const {
    auto it = live_records.find(page_id);
    return it == live_records.end() || it->value == 0;
  }

  void SwapSpace::note_live_record(uint64_t page_id) {
    auto it = live_records.find(page_id);
    if (it == live_records.end()) {
      live_records.set(page_id, 1);
    } else {
      live_records.set(page_id, it->value + 1);
    }
  }

  void SwapSpace::note_dead_record(uint64_t page_id) {
    auto it = live_records.find(page_id);
    if (it == live_records.end()) return;
    if (it->value <= 1) {
      live_records.set(page_id, 0);
    } else {
      live_records.set(page_id, it->value - 1);
    }
  }

  void SwapSpace::reclaim_page_if_empty(uint64_t page_id, RootHeader &root) {
    if (!page_is_empty(page_id)) return;

    auto page = pool.getPage(page_id);
    if (page_id == root.head_page_id) {
      page.wipePage();
      root.head_used_bytes = 0;
      page_used_bytes.set(page_id, 0);
      live_records.set(page_id, 0);
      return;
    }

    pool.freePage(ck::move(page));
    page_used_bytes.remove(page_id);
    live_records.remove(page_id);
  }

  bool SwapSpace::validate_record(size_t used_bytes, size_t offset,
                                  const alaska::ObjectHeader &header) const {
    size_t record_bytes = sizeof(alaska::ObjectHeader) + header.object_size();
    if (record_bytes < sizeof(alaska::ObjectHeader)) return false;
    if (offset + record_bytes > used_bytes) return false;
    return true;
  }

  bool SwapSpace::release_resident_object(alaska::Mapping *mapping, void *ptr) {
    if (runtime.heap.contains(ptr)) {
      auto *page = alaska::Heap::get_page(ptr);
      return page != nullptr && page->release_remote(*mapping, ptr);
    }

    if (runtime.arena_heap.contains(ptr)) {
      auto *header = alaska::ObjectHeader::from(ptr);
      header->handle_id = 0;
      alaska::get_arena_block(ptr)->free(header);
      return true;
    }

    return false;
  }


  void SwapSpace::debug_dump(FILE* out) {
    fprintf(out, "SwapSpace Debug Dump:\n");
    TimeCache tc;
    float swap_out_rate = swap_out_byte_rate.digest(tc);
    float fault_in_rate = fault_in_byte_rate.digest(tc);


    fprintf(out, " Swap out: %8.2f b/s\n", swap_out_rate);
    fprintf(out, " Fault in: %8.2f b/s\n", fault_in_rate);

  }

}  // namespace alaska::disk
