#pragma once

#include <stddef.h>
#include <stdint.h>


namespace alaska {
  // Header-only functional HtlbCache cache model.
  //
  // Models only the set-associative L1 HtlbCache structure in
  // src/main/scala/v3/lsu/htlb_simple.scala:
  //   - set index = low log2(sets) bits of hid
  //   - tag       = remaining high bits of hid
  //   - hit       = valid && stored tag == query tag
  //   - on hit    = update per-set tree PLRU
  //   - on miss   = allocate into first invalid way, else PLRU victim
  //
  // It intentionally does not model walks, latency, faults, physical-address
  // optimization, passthrough mode, or the two-stage L0 directory cache.
  // `hid` is the handle ID, not the full handle address.
  template <size_t sets, size_t ways>
  class HtlbCache {
    static_assert(sets > 0, "HtlbCache must have at least one set");
    static_assert(ways > 0, "HtlbCache must have at least one way");
    static_assert((sets & (sets - 1)) == 0, "HtlbCache sets must be a power of two");
    static_assert((ways & (ways - 1)) == 0, "HtlbCache ways must be a power of two");

   public:
    HtlbCache() { clear(); }

    // Returns true on hit. On miss, allocates the HID and returns false.
    bool access(uint64_t hid) {
      const size_t set = index(hid);
      const uint64_t tag = hid >> indexBits;

      for (size_t way = 0; way < ways; ++way) {
        Entry& entry = entries_[set][way];
        if (entry.valid && entry.tag == tag) {
          plru_[set].access(way);
          return true;
        }
      }

      insert(set, tag);
      return false;
    }

    // Returns true if HID is currently cached. Does not update PLRU or allocate.
    bool contains(uint64_t hid) const {
      const size_t set = index(hid);
      const uint64_t tag = hid >> indexBits;

      for (size_t way = 0; way < ways; ++way) {
        const Entry& entry = entries_[set][way];
        if (entry.valid && entry.tag == tag) {
          return true;
        }
      }
      return false;
    }

    // Selectively invalidate one HID. Matches htlb_simple.scala: invalidates all
    // matching tags in the indexed set and does not touch replacement state.
    void invalidate(uint64_t hid) {
      const size_t set = index(hid);
      const uint64_t tag = hid >> indexBits;

      for (size_t way = 0; way < ways; ++way) {
        Entry& entry = entries_[set][way];
        if (entry.tag == tag) {
          entry.valid = false;
        }
      }
    }

    void clear() {
      for (size_t set = 0; set < sets; ++set) {
        for (size_t way = 0; way < ways; ++way) {
          entries_[set][way].valid = false;
          entries_[set][way].tag = 0;
        }
        plru_[set].clear();
      }
    }

   private:
    struct Entry {
      bool valid;
      uint64_t tag;
    };

    static constexpr size_t log2Const(size_t value) {
      size_t bits = 0;
      while (value > 1) {
        value >>= 1;
        ++bits;
      }
      return bits;
    }

    static constexpr size_t indexBits = log2Const(sets);
    static constexpr uint64_t indexMask = sets - 1;

    static size_t index(uint64_t hid) { return static_cast<size_t>(hid & indexMask); }

    class TreePLRU {
     public:
      TreePLRU()
          : bits_(0) {}

      void clear() { bits_ = 0; }

      size_t victim() const {
        size_t node = 0;
        for (size_t level = 0; level < depth; ++level) {
          const bool goRight = (bits_ >> node) & 1ULL;
          node = 2 * node + 1 + (goRight ? 1 : 0);
        }
        return node - (ways - 1);
      }

      void access(size_t way) {
        size_t node = 0;
        for (size_t bit = depth; bit-- > 0;) {
          const bool wentRight = (way >> bit) & 1ULL;
          if (wentRight) {
            bits_ &= ~(1ULL << node);  // accessed right subtree; left is now LRU
          } else {
            bits_ |= (1ULL << node);  // accessed left subtree; right is now LRU
          }
          node = 2 * node + 1 + (wentRight ? 1 : 0);
        }
      }

     private:
      static constexpr size_t depth = log2Const(ways);
      uint64_t bits_;
    };

    void insert(size_t set, uint64_t tag) {
      for (size_t way = 0; way < ways; ++way) {
        Entry& entry = entries_[set][way];
        if (!entry.valid) {
          entry.valid = true;
          entry.tag = tag;
          return;
        }
      }

      Entry& victim = entries_[set][plru_[set].victim()];
      victim.valid = true;
      victim.tag = tag;
    }

    Entry entries_[sets][ways];
    TreePLRU plru_[sets];
  };
}  // namespace alaska
