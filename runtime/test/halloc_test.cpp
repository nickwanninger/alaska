#include <gtest/gtest.h>
#include <alaska.h>


TEST(Halloc, Unique) {
  void *x = halloc(16);
  void *y = halloc(16);

  EXPECT_NE(x, y);

  hfree(x);
  hfree(y);
}
