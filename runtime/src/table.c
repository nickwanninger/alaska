#include <alaska/internal.h>
#include <string.h>
#include <sys/mman.h>

#define MAP_ENTRY_SIZE sizeof(alaska_mapping_t)  // enforced by a static assert in translation_types.h
#define MAP_GRANULARITY 0x200000LU               // minimum size of a "huge page" if we ever get around to that


// This file contains the implementation of the creation and management of the
// handle translation table. It abstracts the management of the table's backing
// memory, as well as the get/put interface to allocate a new handle

struct alaska_table {
  // How many handles are in the table
  size_t size;
  size_t nfree; // how many entries are free?
  // A contiguous block of memory containing all the mappings
  alaska_mapping_t *map;
};

static struct alaska_table table;

void alaska_table_init(void) {
  memset(&table, 0, sizeof(table));


  int fd = -1;
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  size_t sz = MAP_GRANULARITY * 64;

  // TODO: do this using hugetlbfs :)
  table.map = (alaska_mapping_t *)mmap((void *)MAP_GRANULARITY, sz, PROT_READ | PROT_WRITE, flags | MAP_FIXED, fd, 0);
  table.size = sz / MAP_ENTRY_SIZE;

  for (size_t i = 0; i < table.size; i++) {
    // A size of -1 indicates that the handle is not allocated
    table.map[i].size = -1;
  }
	table.nfree = table.size;
}

void alaska_table_deinit(void) {
  munmap(table.map, table.size * MAP_ENTRY_SIZE);
}


unsigned alaska_table_get_canonical(alaska_mapping_t *ent) {
  return (unsigned)(ent - table.map);
}

alaska_mapping_t *alaska_table_from_canonical(unsigned canon) {
  return &table.map[canon];
}


alaska_mapping_t *alaska_table_get(void) {

  // slow and bad, but correct
  for (size_t i = 0; i < table.size; i++) {
    alaska_mapping_t *ent = &table.map[i];
    if (ent->size == -1) {
      ent->size = 0;
      return ent;
    }
  }

  return NULL;
}


void alaska_table_put(alaska_mapping_t *ent) {
  ent->size = -1;
}