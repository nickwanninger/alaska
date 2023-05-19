/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2023, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2023, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */
#include <alaska.h>
#include <alaska/service.hpp>
#include <alaska/alaska.hpp>
#include <alaska/table.hpp>
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <sys/mman.h>



size_t alaska_usable_size(void *ptr) {
  void *tr = alaska_translate(ptr);
  if (tr != ptr) {
    return alaska::service::usable_size(tr);
  }
  return malloc_usable_size(ptr);
}


static void *_halloc(size_t sz, int zero) {
  alaska::Mapping *ent = alaska::table::get();
  if (unlikely(ent == NULL)) {
    fprintf(stderr, "alaska: out of space!\n");
    exit(-1);
  }

  ent->ptr = NULL;  // Set to NULL as a sanity check
  // Defer to the service to alloc
  alaska::service::alloc(ent, sz);
  ALASKA_SANITY(ent->ptr != NULL, "Service did not allocate anything");
  if (zero) memset(ent->ptr, 0, sz);
  void *out = ent->to_handle(0);
  // printf("allocate %p %zu\n", out, sz);
  return out;
}

void *halloc(size_t sz) noexcept {
  return _halloc(sz, 0);
}

void *hcalloc(size_t nmemb, size_t size) {
  void *out = _halloc(nmemb * size, 1);
  return out;
}

// Reallocate a handle
void *hrealloc(void *handle, size_t new_size) {
  if (handle == NULL) return halloc(new_size);

  auto *m = alaska::Mapping::from_handle_safe(handle);
  if (m == NULL) {
    // If it wasn't a handle, just forward to the system realloc
    return realloc(handle, new_size);
  }

  void *old_ptr = m->ptr;
  (void)old_ptr;

  // Defer to the service to realloc
  alaska::service::alloc(m, new_size);

  // realloc in alaska will always return the same pointer...
  // Ahh the benefits of using handles!
  // hopefully, nobody is relying on the output of realloc changing :^)
  return handle;
}

void hfree(void *ptr) {
  if (ptr == NULL) return;
  auto *m = alaska::Mapping::from_handle_safe(ptr);
  // printf("freeing %p %p\n", m, ptr);
  if (m == NULL) {
    // Not a handle? Pass it to the system allocator.
    return ::free(ptr);
  }

  // Defer to the service to free
  alaska::service::free(m);

  if (m->ptr == NULL) {
    // return the mapping to the table
    alaska::table::put(m);
  }
}
