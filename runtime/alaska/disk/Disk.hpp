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
#include <alaska/alaska.hpp>


namespace alaska::disk {

  static constexpr size_t page_order = 12;
  static constexpr size_t page_size = 1LU << page_order;


  class Disk : public alaska::InternalHeapAllocated {
   public:
    virtual ~Disk() = default;

    // Read a page
    virtual bool readPage(uint64_t page, void *buf) = 0;
    // Write a page
    virtual bool writePage(uint64_t page, const void *buf) = 0;
    // How many pages are there?
    virtual size_t pageCount(void) const = 0;

    // Ensure the disk is at least a specific size
    virtual bool ensureFileSize(size_t min_size) = 0;
  };


  // A disk is a simple wrapper aruond a file descriptor, exposing
  // reading/writing of pages
  class FileDisk final : public Disk {
   public:
    FileDisk(const char *path);
    ~FileDisk() override;

    // Read a page
    bool readPage(uint64_t page, void *buf) override;
    // Write a page
    bool writePage(uint64_t page, const void *buf) override;
    // How many pages are there?
    inline size_t pageCount(void) const override { return last_known_size / page_size; }

    // Ensure the disk is at least a specific size
    bool ensureFileSize(size_t min_size) override;

   private:
    int m_file;
    size_t last_known_size = 0;
  };

  class MemoryDisk final : public Disk {
   public:
    MemoryDisk() = default;
    ~MemoryDisk() override;

    bool readPage(uint64_t page, void *buf) override;
    bool writePage(uint64_t page, const void *buf) override;
    inline size_t pageCount(void) const override { return size_bytes / page_size; }
    bool ensureFileSize(size_t min_size) override;

   private:
    void *mapping = nullptr;
    size_t capacity_bytes = 0;
    size_t size_bytes = 0;
  };


}  // namespace alaska::disk