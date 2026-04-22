/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2025, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2025, The Constellation Project
 * All rights reserved.
 *
 */


#include <alaska/disk/Disk.hpp>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace alaska::disk {

  static size_t roundUpToPageSize(size_t bytes) {
    return round_up(bytes, page_size);
  }

  FileDisk::FileDisk(const char *filename) {
    m_file = open(filename, O_CREAT | O_RDWR, 0644);
    struct stat st;
    if (fstat(m_file, &st) != 0) {
      printf("could not stat!\n");
      abort();
    }
    last_known_size = st.st_size;
  }

  FileDisk::~FileDisk() {
    if (m_file) {
      close(m_file);
    }
  }

  bool FileDisk::readPage(uint64_t page_id, void *buf) {
    off_t offset = page_id * page_size;
    if (!ensureFileSize(offset + page_size)) return false;
    if (lseek(m_file, offset, SEEK_SET) != offset) {
      return false;
    }
    bool success = read(m_file, buf, page_size) == (ssize_t)page_size;
    return success;
  }

  bool FileDisk::writePage(uint64_t page_id, const void *buf) {
    off_t offset = page_id * page_size;
    if (!ensureFileSize(offset + page_size)) return false;
    if (lseek(m_file, offset, SEEK_SET) != offset) return false;
    return write(m_file, buf, page_size) == (ssize_t)page_size;
  }

  bool FileDisk::ensureFileSize(size_t min_size) {
    if (last_known_size < min_size) {
      last_known_size = min_size;
      return ftruncate(m_file, min_size) == 0;
    }
    return true;
  }

  MemoryDisk::~MemoryDisk() {
    if (mapping != nullptr) {
      munmap(mapping, capacity_bytes);
    }
  }

  bool MemoryDisk::readPage(uint64_t page_id, void *buf) {
    size_t offset = page_id * page_size;
    if (!ensureFileSize(offset + page_size)) return false;
    memcpy(buf, (uint8_t *)mapping + offset, page_size);
    return true;
  }

  bool MemoryDisk::writePage(uint64_t page_id, const void *buf) {
    size_t offset = page_id * page_size;
    if (!ensureFileSize(offset + page_size)) return false;
    memcpy((uint8_t *)mapping + offset, buf, page_size);
    return true;
  }

  bool MemoryDisk::ensureFileSize(size_t min_size) {
    if (min_size <= size_bytes) {
      return true;
    }

    if (min_size > capacity_bytes) {
      size_t new_capacity_bytes = roundUpToPageSize(min_size);
      void *new_mapping = nullptr;

      if (mapping == nullptr) {
        new_mapping = mmap(nullptr, new_capacity_bytes, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      } else {
        new_mapping = mremap(mapping, capacity_bytes, new_capacity_bytes, MREMAP_MAYMOVE);
      }

      if (new_mapping == MAP_FAILED) {
        return false;
      }

      mapping = new_mapping;
      capacity_bytes = new_capacity_bytes;
    }

    size_bytes = min_size;
    return true;
  }

}  // namespace alaska::disk
