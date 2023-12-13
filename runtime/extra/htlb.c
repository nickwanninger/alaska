#include <asm-generic/errno-base.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define TAG_BITS 1  // 6 bits
#define WIDTH (1 << TAG_BITS)


#define SIZE 32  // how many entries can you hold?
#define ASSOCIATIVITY 32
#define NUM_SETS (SIZE / ASSOCIATIVITY)

struct line {
  uint32_t handle;     // the handle ID
  uint64_t last_used;  // what time was this line last used?
  bool valid;
};

struct set {
  struct line lines[ASSOCIATIVITY];
};

struct tlb {
  uint64_t hits;
  uint64_t misses;
  uint64_t time;  // for LRU
  struct set sets[NUM_SETS];
  // struct line lines[WIDTH];
};


void tlb_init(struct tlb *tlb) {
  memset(tlb, 0, sizeof(*tlb));
  for (int set = 0; set < NUM_SETS; set++) {
    for (int line = 0; line < ASSOCIATIVITY; line++) {
      tlb->sets[set].lines[line].handle = 0;
      tlb->sets[set].lines[line].valid = false;
    }
  }
}

void tlb_access(struct tlb *tlb, uint64_t handle) {
  struct line *line = NULL;         // The line we end up chosing
  struct line *oldest_line = NULL;  // The LRU line
  uint32_t handle_id = (handle >> 32);

  // grab the set
  struct set *set = &tlb->sets[handle_id % NUM_SETS];
  oldest_line = &set->lines[0];
  // now find a line!
  for (int l = 0; l < ASSOCIATIVITY; l++) {
    struct line *cur = &set->lines[l];
    if (cur->last_used < oldest_line->last_used) {
      oldest_line = cur;
    }

    if (cur->valid && cur->handle == handle_id) {
      line = cur;
      break;
    }
  }

  // If we found a line it's a hit
  if (line != NULL) {
    tlb->hits++;
  } else {
    // otherwise, miss! (eviction)
    tlb->misses++;
    line = oldest_line;
  }

  line->valid = true;
  line->handle = handle_id;
  line->last_used = tlb->time++;
}

void tlb_dump(struct tlb *tlb) {
  printf("Hits:   %zu\n", tlb->hits);
  printf("Misses: %zu\n", tlb->misses);
  printf("rate:   %lf%%\n", (tlb->hits / (double)(tlb->hits + tlb->misses)) * 100.0);
}

int tlb_drive_with_file(struct tlb *tlb, const char *path) {
  size_t len;
  ssize_t read;
  char *line = NULL;
  FILE *stream;
  stream = fopen(path, "r");
  if (stream == NULL) {
    fprintf(stderr, "File not found!\n");
    return -ENOENT;
  }

  while ((read = getline(&line, &len, stream)) != -1) {
    uint64_t handle = 0;
    if (sscanf(line, "tr 0x%zx", &handle) == 1) {
      tlb_access(tlb, handle);
    }
  }

  fclose(stream);

  if (line) {
    free(line);
  }
  return 0;
}

// This file implements a simple "Handle TLB" simulator
int main(int argc, char *argv[]) {

  struct tlb tlb;
  tlb_init(&tlb);

  if (argc != 2) {
    fprintf(stderr, "usage: htlb <trace>\n");
    return EXIT_FAILURE;
  }

  if (tlb_drive_with_file(&tlb, argv[1]) != 0) {
    return EXIT_FAILURE;
  }

  tlb_dump(&tlb);

  return EXIT_SUCCESS;
}
