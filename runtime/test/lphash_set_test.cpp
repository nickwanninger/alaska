/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2025, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2025, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

#include <gtest/gtest.h>
#include <alaska/util/lphash_set.h>

static constexpr size_t TABLE_SIZE = 16;
using Key = uint64_t;

class LPHashSetTest : public ::testing::Test {
 public:
  void SetUp() override { alaska::lphashset_init(table, TABLE_SIZE); }

  Key table[TABLE_SIZE];
};

TEST_F(LPHashSetTest, InitIsEmpty) {
  EXPECT_EQ(alaska::lphashset_count(table, TABLE_SIZE), 0u);
}

TEST_F(LPHashSetTest, InsertReturnsTrue) {
  EXPECT_TRUE(alaska::lphashset_insert(table, TABLE_SIZE, Key{42}));
}

TEST_F(LPHashSetTest, InsertDuplicateReturnsFalse) {
  alaska::lphashset_insert(table, TABLE_SIZE, Key{42});
  EXPECT_FALSE(alaska::lphashset_insert(table, TABLE_SIZE, Key{42}));
}

TEST_F(LPHashSetTest, InsertIncreasesCount) {
  alaska::lphashset_insert(table, TABLE_SIZE, Key{1});
  EXPECT_EQ(alaska::lphashset_count(table, TABLE_SIZE), 1u);
  alaska::lphashset_insert(table, TABLE_SIZE, Key{2});
  EXPECT_EQ(alaska::lphashset_count(table, TABLE_SIZE), 2u);
}

TEST_F(LPHashSetTest, DuplicateInsertDoesNotIncreaseCount) {
  alaska::lphashset_insert(table, TABLE_SIZE, Key{7});
  alaska::lphashset_insert(table, TABLE_SIZE, Key{7});
  EXPECT_EQ(alaska::lphashset_count(table, TABLE_SIZE), 1u);
}

// These tests expose the bug in lphashset_contains: it always returned false,
// even when the key was present.

TEST_F(LPHashSetTest, ContainsTrueAfterInsert) {
  alaska::lphashset_insert(table, TABLE_SIZE, Key{99});
  EXPECT_TRUE(alaska::lphashset_contains(table, TABLE_SIZE, Key{99}));
}

TEST_F(LPHashSetTest, ContainsFalseForAbsentKey) {
  EXPECT_FALSE(alaska::lphashset_contains(table, TABLE_SIZE, Key{99}));
}

TEST_F(LPHashSetTest, ContainsFalseAfterOnlyOtherInserts) {
  alaska::lphashset_insert(table, TABLE_SIZE, Key{1});
  alaska::lphashset_insert(table, TABLE_SIZE, Key{2});
  EXPECT_FALSE(alaska::lphashset_contains(table, TABLE_SIZE, Key{99}));
}

TEST_F(LPHashSetTest, ContainsAllInsertedKeys) {
  for (Key k = 0; k < 8; k++) {
    alaska::lphashset_insert(table, TABLE_SIZE, k);
  }
  for (Key k = 0; k < 8; k++) {
    EXPECT_TRUE(alaska::lphashset_contains(table, TABLE_SIZE, k));
  }
}

// Keys that hash to the same slot exercise the linear-probing path.
TEST_F(LPHashSetTest, ContainsWorksWithCollisions) {
  // Both keys land in slot (0 % 16) and (16 % 16) = 0, so they collide.
  Key a = 0;
  Key b = TABLE_SIZE;  // same slot as a
  alaska::lphashset_insert(table, TABLE_SIZE, a);
  alaska::lphashset_insert(table, TABLE_SIZE, b);

  EXPECT_TRUE(alaska::lphashset_contains(table, TABLE_SIZE, a));
  EXPECT_TRUE(alaska::lphashset_contains(table, TABLE_SIZE, b));
  EXPECT_FALSE(alaska::lphashset_contains(table, TABLE_SIZE, Key{TABLE_SIZE * 2}));
}
