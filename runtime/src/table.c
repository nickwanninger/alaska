#include <alaska/internal.h>
#include <string.h>
#include <sys/mman.h>

#define MAP_ENTRY_SIZE sizeof(alaska_mapping_t)  // enforced by a static assert in translation_types.h
// #define MAP_GRANULARITY 0x200000LU               // minimum size of a "huge page" if we ever get around to that


#define MAP_GRANULARITY 0x1000LU * MAP_ENTRY_SIZE

// This file contains the implementation of the creation and management of the
// handle translation table. It abstracts the management of the table's backing
// memory, as well as the get/put interface to allocate a new handle

struct alaska_table {
  // How many handles are in the table
  size_t size;
  size_t nfree;  // how many entries are free?
  // A contiguous block of memory containing all the mappings
  alaska_mapping_t *map;
};

static struct alaska_table table;

// static void dump_regions() {
//   FILE *f = fopen("/proc/self/maps", "r");
//   char line_buf[256];
//   while (!feof(f)) {
//     off_t start, end;
//     char flags[5];  // "rwxp\0"
//     if (fgets(line_buf, 256, f) == 0) break;
//
//     int count = sscanf(line_buf, "%lx-%lx %s\n", &start, &end, flags);
//     if (count == 3) {
//       printf("region:%s", line_buf);
//       if (flags[1] == 'w') {
//       }
//     }
//   }
//   fclose(f);
// }

// grow the table by a factor of 2
static void alaska_table_grow() {
  size_t oldbytes = table.size * MAP_ENTRY_SIZE;
  size_t newbytes = oldbytes * 2;

  if (table.map == NULL) {
    newbytes = MAP_GRANULARITY;
    // TODO: use hugetlbfs or something similar
    table.map = (alaska_mapping_t *)mmap(
        (void *)0x200000LU, newbytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  } else {
    table.map = (alaska_mapping_t *)mremap(table.map, oldbytes, newbytes, 0, table.map);
  }
  if (table.map == MAP_FAILED) {
    fprintf(stderr, "could not resize table!\n");
    abort();
  }
  // newsize is now the number of entries
  size_t newsize = newbytes / MAP_ENTRY_SIZE;

  for (size_t i = table.size; i < newsize; i++) {
    alaska_table_put(&table.map[i]);
  }

  // update the total size
  table.size = newsize;
  // printf("alaska: grew table to %zu entries\n", table.size);
  // dump_regions();
}

void alaska_table_init(void) {
  memset(&table, 0, sizeof(table));

  alaska_table_grow();
	return;

  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  size_t sz = MAP_GRANULARITY;

  // TODO: do this using hugetlbfs :)
  table.map = (alaska_mapping_t *)mmap((void *)0x200000LU, sz, PROT_READ | PROT_WRITE, flags | MAP_FIXED, -1, 0);
  table.size = sz / MAP_ENTRY_SIZE;
  table.nfree = 0;
  for (size_t i = 0; i < table.size; i++) {
    alaska_table_put(&table.map[i]);
  }
}

void alaska_table_deinit(void) { munmap(table.map, table.size * MAP_ENTRY_SIZE); }


unsigned alaska_table_get_canonical(alaska_mapping_t *ent) { return (unsigned)(ent - table.map); }
alaska_mapping_t *alaska_table_from_canonical(unsigned canon) { return &table.map[canon]; }


// allocate a table entry
alaska_mapping_t *alaska_table_get(void) {
  // slow and bad, but correct
  for (size_t i = 0; i < table.size; i++) {
    alaska_mapping_t *ent = &table.map[i];
    if (ent->size == -1) {
      ent->size = 0;
      table.nfree--;
      // printf("nfree=%zu\n", table.nfree);
      return ent;
    }
  }

  alaska_table_grow();
  return alaska_table_get();

  return NULL;
}

// free a table entry
void alaska_table_put(alaska_mapping_t *ent) {
  table.nfree++;
  ent->size = -1;
}
