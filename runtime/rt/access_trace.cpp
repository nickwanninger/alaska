/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2026, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2026, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

// Access tracing. A program compiled with ALASKA_TRACE_ACCESS calls
// __alaska_track_access before each access through a translated address. The
// runtime finds the object of the address and records (handle id, offset), so
// the trace does not depend on the heap layout. Allocations and frees go into
// the same stream, because handle ids are reused.
//
// Output: $ALASKA_ACCESS_TRACE, else $RESULTS_DIR/access.trace. The file must
// not exist. With ALASKA_ACCESS_COUNT=1 and no output, the runtime only
// counts. Otherwise the hooks do nothing. At exit the runtime prints
// ALASKA_ACCESS_TRACE_END with the counts to stderr.
//
// Format: "ACTRACE\0", version, then records. Integers are LEB128 varints;
// "delta" is the zigzag difference to the previous value of the same field.
//   1 ACCESS     handle_id (delta)  offset  size << 1 | write
//   2 RAW        address (delta)  size << 1 | write   (not a live heap object)
//   3 ALLOC      handle_id  size
//   4 FREE       handle_id
//   5 ALLOC_RAW  address  size                       (raw pointer, e.g. huge)
//   6 FREE_RAW   address
//   7 MARK       value                               (yukon_change_roi)
//   8 THREAD     tid
//   9 END        accesses  raws  allocs  frees

#include <alaska.h>
#include <alaska/alaska.hpp>
#include <alaska/core/Runtime.hpp>
#include <alaska/heaps/SizedPage.hpp>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <rt/access_trace.hpp>

namespace {
  enum : uint8_t {
    OP_ACCESS = 1,
    OP_RAW = 2,
    OP_ALLOC = 3,
    OP_FREE = 4,
    OP_ALLOC_RAW = 5,
    OP_FREE_RAW = 6,
    OP_MARK = 7,
    OP_THREAD = 8,
    OP_END = 9,
  };

  enum { STATE_WRITE, STATE_COUNT, STATE_OFF };

  constexpr size_t BUFFER_SIZE = 1 << 22;
  constexpr size_t RECORD_MAX = 64;

  int state = STATE_OFF;
  int fd = -1;
  pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  uint8_t buffer[BUFFER_SIZE];
  size_t used = 0;
  uint64_t last_handle = 0, last_raw = 0, last_tid = 0;
  uint64_t accesses = 0, raws = 0, stale = 0, allocs = 0, frees = 0;
  char path[4096];
  __thread uint64_t my_tid = 0;


  void say(const char *text) {
    ssize_t r = write(2, text, strlen(text));
    (void)r;
  }


  void flush(void) {
    size_t done = 0;
    while (done < used) {
      ssize_t n = write(fd, buffer + done, used - done);
      if (n < 0 && errno == EINTR) continue;
      if (n <= 0) {
        say("alaska: access trace write failed; tracing stops without an END record\n");
        state = STATE_OFF;
        break;
      }
      done += n;
    }
    used = 0;
  }


  void put_byte(uint8_t b) { buffer[used++] = b; }

  void put_varint(uint64_t v) {
    while (v >= 0x80) {
      put_byte((uint8_t)(v | 0x80));
      v >>= 7;
    }
    put_byte((uint8_t)v);
  }

  void put_delta(uint64_t value, uint64_t &last) {
    int64_t delta = (int64_t)(value - last);
    put_varint(((uint64_t)delta << 1) ^ (uint64_t)(delta >> 63));
    last = value;
  }


  void start(void) {
    const char *out = getenv("ALASKA_ACCESS_TRACE");
    const char *dir = getenv("RESULTS_DIR");
    if (out != nullptr && out[0] != '\0') {
      snprintf(path, sizeof(path), "%s", out);
    } else if (dir != nullptr && dir[0] != '\0') {
      snprintf(path, sizeof(path), "%s/access.trace", dir);
    } else {
      if (getenv("ALASKA_ACCESS_COUNT") != nullptr) state = STATE_COUNT;
      return;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (fd < 0) {
      say("alaska: cannot create the access trace (it must not exist); tracing is off\n");
      state = STATE_OFF;
      return;
    }
    memcpy(buffer, "ACTRACE\0", 8);
    used = 8;
    put_varint(1);
    state = STATE_WRITE;
  }


  // Called with the lock held. Returns true if the caller must write a record.
  bool begin_record(void) {
    if (state != STATE_WRITE) return false;
    if (BUFFER_SIZE - used < 2 * RECORD_MAX) flush();
    if (state != STATE_WRITE) return false;
    if (my_tid == 0) my_tid = (uint64_t)syscall(SYS_gettid);
    if (my_tid != last_tid) {
      put_byte(OP_THREAD);
      put_varint(my_tid);
      last_tid = my_tid;
    }
    return true;
  }


  // The (handle id, offset) of a heap address, or false if no live object holds it.
  bool locate(uint64_t address, uint32_t &handle_id, uint64_t &offset) {
    auto &heap = alaska::Runtime::get().heap;
    if (!heap.contains((void *)address)) return false;
    auto page = (alaska::SizedPage *)alaska::Heap::get_page((void *)address);
    if (page == nullptr) return false;
    auto header = page->slot_containing((void *)address);
    if (header == nullptr || header->handle_id == 0) return false;
    auto data = (uint64_t)header->data();
    if (address < data) return false;
    handle_id = header->handle_id;
    offset = address - data;
    return true;
  }


  void prepare_fork(void) { pthread_mutex_lock(&lock); }
  void parent_fork(void) { pthread_mutex_unlock(&lock); }
  void child_fork(void) {
    // The parent owns the file and the buffer.
    state = STATE_OFF;
    fd = -1;
    used = 0;
    my_tid = 0;
    pthread_mutex_unlock(&lock);
  }

  __attribute__((constructor)) void init(void) {
    start();
    pthread_atfork(prepare_fork, parent_fork, child_fork);
  }


  __attribute__((destructor)) void finish(void) {
    pthread_mutex_lock(&lock);
    if (state == STATE_WRITE) {
      put_byte(OP_END);
      put_varint(accesses);
      put_varint(raws);
      put_varint(allocs);
      put_varint(frees);
      flush();
    }
    if (state == STATE_WRITE || state == STATE_COUNT) {
      char line[4400];
      snprintf(line, sizeof(line),
          "ALASKA_ACCESS_TRACE_END accesses=%lu raws=%lu stale=%lu allocs=%lu frees=%lu path=%s\n",
          (unsigned long)accesses, (unsigned long)raws, (unsigned long)stale,
          (unsigned long)allocs, (unsigned long)frees, state == STATE_WRITE ? path : "none");
      say(line);
      if (fd >= 0) close(fd);
    }
    // Calls after this point are not traced.
    state = STATE_OFF;
    pthread_mutex_unlock(&lock);
  }
}  // namespace


extern "C" void __alaska_track_access(uint64_t address, uint64_t size, uint64_t kind) {
  if (state == STATE_OFF) return;
  uint32_t handle_id;
  uint64_t offset;
  bool found = locate(address, handle_id, offset);

  pthread_mutex_lock(&lock);
  if (found) {
    accesses++;
  } else {
    raws++;
    // An address in the heap that no live object holds.
    if (alaska::Runtime::get().heap.contains((void *)address)) stale++;
  }
  if (begin_record()) {
    if (found) {
      put_byte(OP_ACCESS);
      put_delta(handle_id, last_handle);
      put_varint(offset);
    } else {
      put_byte(OP_RAW);
      put_delta(address, last_raw);
    }
    put_varint(size << 1 | (kind & 1));
  }
  pthread_mutex_unlock(&lock);
}


void alaska::access_trace::alloc(void *result, size_t size) {
  if (state == STATE_OFF || result == nullptr) return;
  pthread_mutex_lock(&lock);
  allocs++;
  if (begin_record()) {
    if (auto m = alaska::Mapping::from_handle_safe(result)) {
      put_byte(OP_ALLOC);
      put_varint(m->handle_id());
    } else {
      put_byte(OP_ALLOC_RAW);
      put_varint((uint64_t)result);
    }
    put_varint(size);
  }
  pthread_mutex_unlock(&lock);
}


void alaska::access_trace::free(void *ptr) {
  if (state == STATE_OFF || ptr == nullptr) return;
  pthread_mutex_lock(&lock);
  frees++;
  if (begin_record()) {
    if (auto m = alaska::Mapping::from_handle_safe(ptr)) {
      put_byte(OP_FREE);
      put_varint(m->handle_id());
    } else {
      put_byte(OP_FREE_RAW);
      put_varint((uint64_t)ptr);
    }
  }
  pthread_mutex_unlock(&lock);
}


// The Redis YCSB fork calls this at the start (1) and the end (0) of the
// measured phase.
extern "C" __attribute__((visibility("default"))) void yukon_change_roi(int enabled) {
  if (state == STATE_OFF) return;
  pthread_mutex_lock(&lock);
  if (begin_record()) {
    put_byte(OP_MARK);
    put_varint((uint64_t)enabled);
  }
  pthread_mutex_unlock(&lock);
}
