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
#include <alaska/service.h>
#include <alaska/internal.h>
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <sys/mman.h>


void alaska_halloc_init(void) {
}

void alaska_halloc_deinit(void) {
}


size_t alaska_usable_size(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  if (unlikely((h & HANDLE_MARKER) != 0)) {
    return GET_ENTRY(h)->size;
  }
  return malloc_usable_size(ptr);
}


void *halloc(size_t sz) {
  assert(sz < (1LLU << ALASKA_OFFSET_BITS));
  alaska_mapping_t *ent = alaska_table_get();

  if (ent == NULL) {
    fprintf(stderr, "alaska: out of space!\n");
    exit(-1);
  }

  ent->ptr = NULL; // Set to NULL as a sanity check
  // Defer to the service to alloc
  alaska_service_alloc(ent, sz);
  ALASKA_SANITY(ent->ptr != NULL, "Service did not allocate anything");
  ALASKA_SANITY(ent->size == sz, "Service did not update the handle's size");


  memset(ent->ptr, 0, sz);

#ifdef ALASKA_SIM_MODE
  // record, then return the pointer itself
  extern void sim_on_alloc(alaska_mapping_t *);
  sim_on_alloc(ent);
  return ent->ptr;
#else
  uint64_t handle = HANDLE_MARKER | (((uint64_t)ent) << ALASKA_OFFSET_BITS);
  return (void *)handle;
#endif
}

void *hcalloc(size_t nmemb, size_t size) {
  return halloc(nmemb * size);
}

// Reallocate a handle
void *hrealloc(void *handle, size_t new_size) {
  if (handle == NULL) return halloc(new_size);

  alaska_mapping_t *m = alaska_lookup(handle);
  if (m == NULL) {
    // If it wasn't a handle, just forward to the system realloc
    return realloc(handle, new_size);
  }

  long old_size = m->size;
	void *old_ptr = m->ptr;
	(void)old_size;
	(void)old_ptr;

  // Defer to the service to realloc
  alaska_service_alloc(m, new_size);

#ifdef ALASKA_SIM_MODE
  // record, then return the pointer itself
  extern void sim_on_realloc(alaska_mapping_t *, void *oldptr, size_t oldsz);
  sim_on_realloc(m, old_ptr, old_size);
  // record, then return the pointer itself
  return m->ptr;
#else
  // realloc in alaska will always return the same pointer...
  // Ahh the benefits of using handles!
  // hopefully, nobody is relying on the output of realloc changing :^)
  return handle;
#endif
}

void hfree(void *ptr) {
  if (ptr == NULL) return;

  alaska_mapping_t *m = alaska_lookup(ptr);
  if (m == NULL) {
    return;
  }

#ifdef ALASKA_SIM_MODE
  extern void sim_on_free(alaska_mapping_t *);
  sim_on_free(m);
#endif
  // Defer to the service to free
  alaska_service_free(m);

  if (m->ptr == NULL) {
    // return the mapping to the table
    alaska_table_put(m);
  }
}
