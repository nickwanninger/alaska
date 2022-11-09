#pragma once

#include <alaska.h>
#include <alaska/translation_types.h>

#define ALASKA_MAX_ARENAS 255

namespace alaska {


  // When translating, should intermediate page tables
  // be allocated, or should it stop when it would?
  enum class AllocateIntermediate { Yes = 1, No = 0 };

  /**
   * HandleTable - the backing page table implementation for mapping
   *
   * This is structured identically to an x86 or other kind of radix page table,
   * where there are multiple levels of indirection indexed by different parts
   * of the address. In Alaska, there is one of these for each size class, which
   * (for the case of our current implementation) means there are 32 (2^5).
   *
   *
   */
  class HandleTable {
   public:
    void init(size_t size, alaska_map_driller_t driller);
    alaska_map_entry_t *translate(alaska_handle_t handle, AllocateIntermediate allocate_intermediate);
    ~HandleTable();

   private:
    size_t m_size;
    alaska_map_driller_t m_driller;  // this indirection will end up being slow. TODO: template on this
    uint64_t *m_lvl0 = nullptr;
  };


  /**
   * Arena - a manager of a certain type of handle.
   *
   * Handles in alaska are broken up into different arenas as a method of encoding
   * capabilities and functionality when the pointer is pinned/unpinned. It manages
   * the ownership and translation of handles of different size classes.
   */
  class Arena {
   public:
    Arena(int id);
    ~Arena(void);

    // allocate a handle in this arena.
    alaska_handle_t allocate(size_t size);
    void free(alaska_handle_t handle);
    // Pin and unpin a handle
    void *pin(alaska_handle_t handle);
    void unpin(alaska_handle_t handle);

   private:
    int m_id;
    alaska::HandleTable m_tables[32];
  };
}  // namespace alaska