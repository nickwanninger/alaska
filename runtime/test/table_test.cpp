#include <gtest/gtest.h>
#include <alaska.h>
#include <alaska/table.hpp>
#include "gtest/gtest.h"


TEST(HandleTable, Sanity) {
  alaska::Mapping *x = alaska::table::get();

  ASSERT_NE(x, nullptr);

  alaska::table::put(x);
}


TEST(HandleTable, Unique) {
  alaska::Mapping *x = alaska::table::get();
  alaska::Mapping *y = alaska::table::get();

  ASSERT_NE(x, y);

  alaska::table::put(x);
  alaska::table::put(y);
}



TEST(HandleTable, UniqueSlab) {
  // If two fresh slabs are allocated, they must be different
  alaska::table::Slab *x = alaska::table::fresh_slab();
  alaska::table::Slab *y = alaska::table::fresh_slab();
  ASSERT_NE(x, y);
}

TEST(HandleTable, LocalOverwrite) {
  // First, allocate a fresh slab, then set the local slab to that.
  alaska::table::Slab *x = alaska::table::fresh_slab();
  alaska::table::set_local_slab(x);

  // Then, allocate a second slab, and attempt to overwrite the current local
  alaska::table::Slab *y = alaska::table::fresh_slab();

  // This should result in program crash.
  ASSERT_DEATH(
      { alaska::table::set_local_slab(y); }, "Cannot set local slab when there already is one");
}


TEST(HandleTable, GetPut) {
  alaska::Mapping *x = alaska::table::get();
  ASSERT_NE(x, nullptr);

  alaska::table::put(x);
}