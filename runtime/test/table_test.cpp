#include <gtest/gtest.h>
#include <alaska.h>
#include <alaska/table.hpp>


TEST(HandleTable, Sanity) {
  alaska::Mapping *x = alaska::table::get();

  EXPECT_NE(x, nullptr);

  alaska::table::put(x);
}



TEST(HandleTable, Unique) {
  alaska::Mapping *x = alaska::table::get();
  alaska::Mapping *y = alaska::table::get();

  EXPECT_NE(x, y);

  alaska::table::put(x);
  alaska::table::put(y);
}
