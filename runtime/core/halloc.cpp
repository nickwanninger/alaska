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
#include <malloc.h>
#include <string.h>
#include <sys/mman.h>




static unsigned long num_alive = 0;

extern "C" size_t alaska_usable_size(void *ptr) {
  void *tr = alaska_translate(ptr);
  if (tr != ptr) {
    return alaska::service::usable_size(tr);
  }
  return malloc_usable_size(ptr);
}


static void *_halloc(size_t sz, int zero) {
	// return malloc(sz);
	// Just some big number.
	if (sz > (1LLU << (uint64_t)(ALASKA_SIZE_BITS - ALASKA_SQUEEZE_BITS - 1)) - 1) {
		return ::malloc(sz);
	}

  alaska::Mapping *ent = alaska::table::get();
  if (unlikely(ent == NULL)) {
    fprintf(stderr, "alaska: out of space!\n");
    exit(-1);
  }

  ent->set_pointer(NULL);  // Set to NULL as a sanity check
  // Defer to the service to alloc
  alaska::service::alloc(ent, sz);
  ALASKA_SANITY(ent->ptr != NULL, "Service did not allocate anything");
  if (zero) memset(ent->get_pointer(), 0, sz);
  void *out = ent->to_handle(0);
  num_alive++;

  if (out == NULL) abort();

  return out;
}

void *halloc(size_t sz) noexcept {
  return _halloc(sz, 0);
}

void *hcalloc(size_t nmemb, size_t size) {
  void *out = _halloc(nmemb * size, 1);
	// printf("hcalloc %p\n", out);
  return out;
}

// Reallocate a handle
void *hrealloc(void *handle, size_t new_size) {
  // return realloc(handle, new_size);

  // If the handle is null, then this call is equivalent to malloc(size)
  if (handle == NULL) {
    // printf("realloc edge case: NULL pointer (sz=%zu)\n", new_size);
    return halloc(new_size);
  }
	// printf("hrealloc %p\n", handle);
  auto *m = alaska::Mapping::from_handle_safe(handle);
  if (m == NULL) {
    // printf("realloc edge case: not a handle %p!\n", handle);
    // If it wasn't a handle, just forward to the system realloc
    return ::realloc(handle, new_size);
  }

  // If the size is equal to zero, and the handle is not null, realloc acts like free(handle)
  if (new_size == 0) {
    // printf("realloc edge case: zero size\n");
    // If it wasn't a handle, just forward to the system realloc
    hfree(handle);
    return NULL;
  }

  // Defer to the service to realloc
  alaska::service::alloc(m, new_size);

  // realloc in alaska will always return the same pointer...
  // Ahh... the benefits of using handles!
  // hopefully, nobody is relying on the return value of realloc changing :^)
  return handle;
}



extern void alaska_remove_from_local_lock_list(void *ptr);

void hfree(void *ptr) {

  // no-op if NULL is passed
  if (ptr == NULL) {
    return;
  }
  // Grab the Mapping
  auto *m = alaska::Mapping::from_handle_safe(ptr);
  if (m == NULL) {
    // Not a handle? Pass it to the system allocator.
    return ::free(ptr);
  }

  alaska_remove_from_local_lock_list(ptr);

  // Defer to the service to free
  alaska::service::free(m);
  num_alive--;
  alaska::table::put(m);
}


// operator new
extern "C" void *alaska_Znwm(size_t sz) {
	return halloc(sz);
}

extern "C" void *alaska_Znam(size_t sz) {
	return halloc(sz);
}

// operator delete[]
extern "C" void alaska_ZdaPv(void *ptr) {
	hfree(ptr);
}

// operator delete
extern "C" void alaska_ZdlPv(void *ptr) {
	hfree(ptr);
}

extern "C" void alaska_ZdlPvm(void *ptr, unsigned long s) {
	hfree(ptr);
}
