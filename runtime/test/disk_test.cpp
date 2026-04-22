/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2026, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2026, The Constellation Project
 * All rights reserved.
 *
 */

#include <gtest/gtest.h>

#include <ck/box.h>

#include <alaska/disk/BufferPool.hpp>
#include <alaska/disk/Disk.hpp>

namespace {
  using alaska::disk::BufferPool;
  using alaska::disk::Disk;
  using alaska::disk::MemoryDisk;
  using alaska::disk::page_size;

  TEST(MemoryDiskTest, StartsEmptyAndGrowsWithEnsureFileSize) {
    MemoryDisk disk;
    EXPECT_EQ(disk.pageCount(), 0u);

    EXPECT_TRUE(disk.ensureFileSize(page_size * 3));
    EXPECT_EQ(disk.pageCount(), 3u);

    EXPECT_TRUE(disk.ensureFileSize(page_size));
    EXPECT_EQ(disk.pageCount(), 3u);
  }

  TEST(MemoryDiskTest, WritePageThenReadPageRoundTripsData) {
    MemoryDisk disk;
    alignas(16) unsigned char written[page_size];
    alignas(16) unsigned char read_back[page_size];

    for (size_t i = 0; i < page_size; ++i) {
      written[i] = static_cast<unsigned char>((i * 17) & 0xff);
      read_back[i] = 0;
    }

    EXPECT_TRUE(disk.writePage(2, written));
    EXPECT_EQ(disk.pageCount(), 3u);
    EXPECT_TRUE(disk.readPage(2, read_back));
    EXPECT_EQ(memcmp(written, read_back, page_size), 0);
  }

  TEST(BufferPoolMemoryDiskTest, NewPagesRemainReadableAcrossEvictionAndFlush) {
    BufferPool pool(ck::box<Disk>(new MemoryDisk()), 1);

    static constexpr uint64_t kNumTestPages = 300;
    for (uint64_t i = 0; i < kNumTestPages; ++i) {
      auto page = pool.newPage();
      *page.getMut<uint64_t>() = 0xabc00000ULL + i;
      *page.getMut<uint64_t>(sizeof(uint64_t)) = i;
    }

    pool.flush();

    for (uint64_t i = 0; i < kNumTestPages; ++i) {
      auto page = pool.getPage(i + 1);
      EXPECT_EQ(*page.get<uint64_t>(), 0xabc00000ULL + i);
      EXPECT_EQ(*page.get<uint64_t>(sizeof(uint64_t)), i);
    }
  }
}
