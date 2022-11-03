// Automatically generated. Do not edit!
#include <stdlib.h>
#include <stdint.h>
#include <alaska/translation_types.h>
#define unlikely(x) __builtin_expect((x),0)

// n=0
// size=64
// FAAASSSS_____444444444333333333222222222111111111000000000......
uint64_t *__alaska_drill_size_64(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 6) & 0b1111111111;
  uint64_t ind1 = (address >> 15) & 0b1111111111;
  uint64_t ind2 = (address >> 24) & 0b1111111111;
  uint64_t ind3 = (address >> 33) & 0b1111111111;
  uint64_t ind4 = (address >> 42) & 0b1111111111;
  // level 4
  next = (uint64_t*)current[ind4];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind4] = (uint64_t)next;
  }
  current = next;
  // level 3
  next = (uint64_t*)current[ind3];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind3] = (uint64_t)next;
  }
  current = next;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=1
// size=128
// FAAASSSS____444444444333333333222222222111111111000000000.......
uint64_t *__alaska_drill_size_128(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 7) & 0b1111111111;
  uint64_t ind1 = (address >> 16) & 0b1111111111;
  uint64_t ind2 = (address >> 25) & 0b1111111111;
  uint64_t ind3 = (address >> 34) & 0b1111111111;
  uint64_t ind4 = (address >> 43) & 0b1111111111;
  // level 4
  next = (uint64_t*)current[ind4];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind4] = (uint64_t)next;
  }
  current = next;
  // level 3
  next = (uint64_t*)current[ind3];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind3] = (uint64_t)next;
  }
  current = next;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=2
// size=256
// FAAASSSS___444444444333333333222222222111111111000000000........
uint64_t *__alaska_drill_size_256(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 8) & 0b1111111111;
  uint64_t ind1 = (address >> 17) & 0b1111111111;
  uint64_t ind2 = (address >> 26) & 0b1111111111;
  uint64_t ind3 = (address >> 35) & 0b1111111111;
  uint64_t ind4 = (address >> 44) & 0b1111111111;
  // level 4
  next = (uint64_t*)current[ind4];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind4] = (uint64_t)next;
  }
  current = next;
  // level 3
  next = (uint64_t*)current[ind3];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind3] = (uint64_t)next;
  }
  current = next;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=3
// size=512
// FAAASSSS__444444444333333333222222222111111111000000000.........
uint64_t *__alaska_drill_size_512(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 9) & 0b1111111111;
  uint64_t ind1 = (address >> 18) & 0b1111111111;
  uint64_t ind2 = (address >> 27) & 0b1111111111;
  uint64_t ind3 = (address >> 36) & 0b1111111111;
  uint64_t ind4 = (address >> 45) & 0b1111111111;
  // level 4
  next = (uint64_t*)current[ind4];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind4] = (uint64_t)next;
  }
  current = next;
  // level 3
  next = (uint64_t*)current[ind3];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind3] = (uint64_t)next;
  }
  current = next;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=4
// size=1024
// FAAASSSS_444444444333333333222222222111111111000000000..........
uint64_t *__alaska_drill_size_1024(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 10) & 0b1111111111;
  uint64_t ind1 = (address >> 19) & 0b1111111111;
  uint64_t ind2 = (address >> 28) & 0b1111111111;
  uint64_t ind3 = (address >> 37) & 0b1111111111;
  uint64_t ind4 = (address >> 46) & 0b1111111111;
  // level 4
  next = (uint64_t*)current[ind4];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind4] = (uint64_t)next;
  }
  current = next;
  // level 3
  next = (uint64_t*)current[ind3];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind3] = (uint64_t)next;
  }
  current = next;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=5
// size=2048
// FAAASSSS444444444333333333222222222111111111000000000...........
uint64_t *__alaska_drill_size_2048(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 11) & 0b1111111111;
  uint64_t ind1 = (address >> 20) & 0b1111111111;
  uint64_t ind2 = (address >> 29) & 0b1111111111;
  uint64_t ind3 = (address >> 38) & 0b1111111111;
  uint64_t ind4 = (address >> 47) & 0b1111111111;
  // level 4
  next = (uint64_t*)current[ind4];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind4] = (uint64_t)next;
  }
  current = next;
  // level 3
  next = (uint64_t*)current[ind3];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind3] = (uint64_t)next;
  }
  current = next;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=6
// size=4096
// FAAASSSS________333333333222222222111111111000000000............
uint64_t *__alaska_drill_size_4096(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 12) & 0b1111111111;
  uint64_t ind1 = (address >> 21) & 0b1111111111;
  uint64_t ind2 = (address >> 30) & 0b1111111111;
  uint64_t ind3 = (address >> 39) & 0b1111111111;
  // level 3
  next = (uint64_t*)current[ind3];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind3] = (uint64_t)next;
  }
  current = next;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=7
// size=8192
// FAAASSSS_______333333333222222222111111111000000000.............
uint64_t *__alaska_drill_size_8192(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 13) & 0b1111111111;
  uint64_t ind1 = (address >> 22) & 0b1111111111;
  uint64_t ind2 = (address >> 31) & 0b1111111111;
  uint64_t ind3 = (address >> 40) & 0b1111111111;
  // level 3
  next = (uint64_t*)current[ind3];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind3] = (uint64_t)next;
  }
  current = next;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=8
// size=16384
// FAAASSSS______333333333222222222111111111000000000..............
uint64_t *__alaska_drill_size_16384(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 14) & 0b1111111111;
  uint64_t ind1 = (address >> 23) & 0b1111111111;
  uint64_t ind2 = (address >> 32) & 0b1111111111;
  uint64_t ind3 = (address >> 41) & 0b1111111111;
  // level 3
  next = (uint64_t*)current[ind3];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind3] = (uint64_t)next;
  }
  current = next;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=9
// size=32768
// FAAASSSS_____333333333222222222111111111000000000...............
uint64_t *__alaska_drill_size_32768(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 15) & 0b1111111111;
  uint64_t ind1 = (address >> 24) & 0b1111111111;
  uint64_t ind2 = (address >> 33) & 0b1111111111;
  uint64_t ind3 = (address >> 42) & 0b1111111111;
  // level 3
  next = (uint64_t*)current[ind3];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind3] = (uint64_t)next;
  }
  current = next;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=10
// size=65536
// FAAASSSS____333333333222222222111111111000000000................
uint64_t *__alaska_drill_size_65536(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 16) & 0b1111111111;
  uint64_t ind1 = (address >> 25) & 0b1111111111;
  uint64_t ind2 = (address >> 34) & 0b1111111111;
  uint64_t ind3 = (address >> 43) & 0b1111111111;
  // level 3
  next = (uint64_t*)current[ind3];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind3] = (uint64_t)next;
  }
  current = next;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=11
// size=131072
// FAAASSSS___333333333222222222111111111000000000.................
uint64_t *__alaska_drill_size_131072(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 17) & 0b1111111111;
  uint64_t ind1 = (address >> 26) & 0b1111111111;
  uint64_t ind2 = (address >> 35) & 0b1111111111;
  uint64_t ind3 = (address >> 44) & 0b1111111111;
  // level 3
  next = (uint64_t*)current[ind3];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind3] = (uint64_t)next;
  }
  current = next;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=12
// size=262144
// FAAASSSS__333333333222222222111111111000000000..................
uint64_t *__alaska_drill_size_262144(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 18) & 0b1111111111;
  uint64_t ind1 = (address >> 27) & 0b1111111111;
  uint64_t ind2 = (address >> 36) & 0b1111111111;
  uint64_t ind3 = (address >> 45) & 0b1111111111;
  // level 3
  next = (uint64_t*)current[ind3];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind3] = (uint64_t)next;
  }
  current = next;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=13
// size=524288
// FAAASSSS_333333333222222222111111111000000000...................
uint64_t *__alaska_drill_size_524288(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 19) & 0b1111111111;
  uint64_t ind1 = (address >> 28) & 0b1111111111;
  uint64_t ind2 = (address >> 37) & 0b1111111111;
  uint64_t ind3 = (address >> 46) & 0b1111111111;
  // level 3
  next = (uint64_t*)current[ind3];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind3] = (uint64_t)next;
  }
  current = next;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=14
// size=1048576
// FAAASSSS333333333222222222111111111000000000....................
uint64_t *__alaska_drill_size_1048576(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 20) & 0b1111111111;
  uint64_t ind1 = (address >> 29) & 0b1111111111;
  uint64_t ind2 = (address >> 38) & 0b1111111111;
  uint64_t ind3 = (address >> 47) & 0b1111111111;
  // level 3
  next = (uint64_t*)current[ind3];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind3] = (uint64_t)next;
  }
  current = next;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=15
// size=2097152
// FAAASSSS________222222222111111111000000000.....................
uint64_t *__alaska_drill_size_2097152(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 21) & 0b1111111111;
  uint64_t ind1 = (address >> 30) & 0b1111111111;
  uint64_t ind2 = (address >> 39) & 0b1111111111;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=16
// size=4194304
// FAAASSSS_______222222222111111111000000000......................
uint64_t *__alaska_drill_size_4194304(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 22) & 0b1111111111;
  uint64_t ind1 = (address >> 31) & 0b1111111111;
  uint64_t ind2 = (address >> 40) & 0b1111111111;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=17
// size=8388608
// FAAASSSS______222222222111111111000000000.......................
uint64_t *__alaska_drill_size_8388608(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 23) & 0b1111111111;
  uint64_t ind1 = (address >> 32) & 0b1111111111;
  uint64_t ind2 = (address >> 41) & 0b1111111111;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=18
// size=16777216
// FAAASSSS_____222222222111111111000000000........................
uint64_t *__alaska_drill_size_16777216(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 24) & 0b1111111111;
  uint64_t ind1 = (address >> 33) & 0b1111111111;
  uint64_t ind2 = (address >> 42) & 0b1111111111;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=19
// size=33554432
// FAAASSSS____222222222111111111000000000.........................
uint64_t *__alaska_drill_size_33554432(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 25) & 0b1111111111;
  uint64_t ind1 = (address >> 34) & 0b1111111111;
  uint64_t ind2 = (address >> 43) & 0b1111111111;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=20
// size=67108864
// FAAASSSS___222222222111111111000000000..........................
uint64_t *__alaska_drill_size_67108864(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 26) & 0b1111111111;
  uint64_t ind1 = (address >> 35) & 0b1111111111;
  uint64_t ind2 = (address >> 44) & 0b1111111111;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=21
// size=134217728
// FAAASSSS__222222222111111111000000000...........................
uint64_t *__alaska_drill_size_134217728(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 27) & 0b1111111111;
  uint64_t ind1 = (address >> 36) & 0b1111111111;
  uint64_t ind2 = (address >> 45) & 0b1111111111;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=22
// size=268435456
// FAAASSSS_222222222111111111000000000............................
uint64_t *__alaska_drill_size_268435456(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 28) & 0b1111111111;
  uint64_t ind1 = (address >> 37) & 0b1111111111;
  uint64_t ind2 = (address >> 46) & 0b1111111111;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=23
// size=536870912
// FAAASSSS222222222111111111000000000.............................
uint64_t *__alaska_drill_size_536870912(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 29) & 0b1111111111;
  uint64_t ind1 = (address >> 38) & 0b1111111111;
  uint64_t ind2 = (address >> 47) & 0b1111111111;
  // level 2
  next = (uint64_t*)current[ind2];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind2] = (uint64_t)next;
  }
  current = next;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=24
// size=1073741824
// FAAASSSS________111111111000000000..............................
uint64_t *__alaska_drill_size_1073741824(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 30) & 0b1111111111;
  uint64_t ind1 = (address >> 39) & 0b1111111111;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=25
// size=2147483648
// FAAASSSS_______111111111000000000...............................
uint64_t *__alaska_drill_size_2147483648(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 31) & 0b1111111111;
  uint64_t ind1 = (address >> 40) & 0b1111111111;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=26
// size=4294967296
// FAAASSSS______111111111000000000................................
uint64_t *__alaska_drill_size_4294967296(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 32) & 0b1111111111;
  uint64_t ind1 = (address >> 41) & 0b1111111111;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=27
// size=8589934592
// FAAASSSS_____111111111000000000.................................
uint64_t *__alaska_drill_size_8589934592(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 33) & 0b1111111111;
  uint64_t ind1 = (address >> 42) & 0b1111111111;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=28
// size=17179869184
// FAAASSSS____111111111000000000..................................
uint64_t *__alaska_drill_size_17179869184(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 34) & 0b1111111111;
  uint64_t ind1 = (address >> 43) & 0b1111111111;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=29
// size=34359738368
// FAAASSSS___111111111000000000...................................
uint64_t *__alaska_drill_size_34359738368(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 35) & 0b1111111111;
  uint64_t ind1 = (address >> 44) & 0b1111111111;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=30
// size=68719476736
// FAAASSSS__111111111000000000....................................
uint64_t *__alaska_drill_size_68719476736(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 36) & 0b1111111111;
  uint64_t ind1 = (address >> 45) & 0b1111111111;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=31
// size=137438953472
// FAAASSSS_111111111000000000.....................................
uint64_t *__alaska_drill_size_137438953472(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 37) & 0b1111111111;
  uint64_t ind1 = (address >> 46) & 0b1111111111;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=32
// size=274877906944
// FAAASSSS111111111000000000......................................
uint64_t *__alaska_drill_size_274877906944(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t *next;
  uint64_t ind0 = (address >> 38) & 0b1111111111;
  uint64_t ind1 = (address >> 47) & 0b1111111111;
  // level 1
  next = (uint64_t*)current[ind1];
  if (unlikely(next == NULL)) {;
    if (allocate == 0) return NULL;
    next = calloc(sizeof(uint64_t), 512);
    current[ind1] = (uint64_t)next;
  }
  current = next;
  // level 0
  return &current[ind0];
}

// n=33
// size=549755813888
// FAAASSSS________000000000.......................................
uint64_t *__alaska_drill_size_549755813888(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t ind0 = (address >> 39) & 0b1111111111;
  // level 0
  return &current[ind0];
}

// n=34
// size=1099511627776
// FAAASSSS_______000000000........................................
uint64_t *__alaska_drill_size_1099511627776(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t ind0 = (address >> 40) & 0b1111111111;
  // level 0
  return &current[ind0];
}

// n=35
// size=2199023255552
// FAAASSSS______000000000.........................................
uint64_t *__alaska_drill_size_2199023255552(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t ind0 = (address >> 41) & 0b1111111111;
  // level 0
  return &current[ind0];
}

// n=36
// size=4398046511104
// FAAASSSS_____000000000..........................................
uint64_t *__alaska_drill_size_4398046511104(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t ind0 = (address >> 42) & 0b1111111111;
  // level 0
  return &current[ind0];
}

// n=37
// size=8796093022208
// FAAASSSS____000000000...........................................
uint64_t *__alaska_drill_size_8796093022208(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t ind0 = (address >> 43) & 0b1111111111;
  // level 0
  return &current[ind0];
}

// n=38
// size=17592186044416
// FAAASSSS___000000000............................................
uint64_t *__alaska_drill_size_17592186044416(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t ind0 = (address >> 44) & 0b1111111111;
  // level 0
  return &current[ind0];
}

// n=39
// size=35184372088832
// FAAASSSS__000000000.............................................
uint64_t *__alaska_drill_size_35184372088832(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t ind0 = (address >> 45) & 0b1111111111;
  // level 0
  return &current[ind0];
}

// n=40
// size=70368744177664
// FAAASSSS_000000000..............................................
uint64_t *__alaska_drill_size_70368744177664(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t ind0 = (address >> 46) & 0b1111111111;
  // level 0
  return &current[ind0];
}

// n=41
// size=140737488355328
// FAAASSSS000000000...............................................
uint64_t *__alaska_drill_size_140737488355328(uint64_t address, uint64_t *top_level, int allocate) {
  uint64_t *current = top_level;
  uint64_t ind0 = (address >> 47) & 0b1111111111;
  // level 0
  return &current[ind0];
}
