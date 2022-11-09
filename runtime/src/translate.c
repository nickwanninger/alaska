// Automatically generated. Do not edit!
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <alaska/translation_types.h>


#define unlikely(x) __builtin_expect((x), 0)

// FAAASSSSS_____________000000000111111111222222222333333333......
alaska_map_entry_t *alaska_map_drill_64(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 33) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 24) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 15) & 0b111111111;  // index into uint64_t
  uint64_t ind3 = (address >> 6) & 0b111111111;   // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  uint64_t *lvl2 = (uint64_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(uint64_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  alaska_map_entry_t *lvl3 = (alaska_map_entry_t *)lvl2[ind2];
  if (unlikely(lvl3 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl3 = alaska_alloc_map_frame(ind2, sizeof(alaska_map_entry_t), 512);
    lvl2[ind2] = (uint64_t)lvl3;
  }
  return &lvl3[ind3];
}

// FAAASSSSS____________000000000111111111222222222333333333.......
alaska_map_entry_t *alaska_map_drill_128(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 34) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 25) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 16) & 0b111111111;  // index into uint64_t
  uint64_t ind3 = (address >> 7) & 0b111111111;   // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  uint64_t *lvl2 = (uint64_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(uint64_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  alaska_map_entry_t *lvl3 = (alaska_map_entry_t *)lvl2[ind2];
  if (unlikely(lvl3 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl3 = alaska_alloc_map_frame(ind2, sizeof(alaska_map_entry_t), 512);
    lvl2[ind2] = (uint64_t)lvl3;
  }
  return &lvl3[ind3];
}

// FAAASSSSS___________000000000111111111222222222333333333........
alaska_map_entry_t *alaska_map_drill_256(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 35) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 26) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 17) & 0b111111111;  // index into uint64_t
  uint64_t ind3 = (address >> 8) & 0b111111111;   // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  uint64_t *lvl2 = (uint64_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(uint64_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  alaska_map_entry_t *lvl3 = (alaska_map_entry_t *)lvl2[ind2];
  if (unlikely(lvl3 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl3 = alaska_alloc_map_frame(ind2, sizeof(alaska_map_entry_t), 512);
    lvl2[ind2] = (uint64_t)lvl3;
  }
  return &lvl3[ind3];
}

// FAAASSSSS__________000000000111111111222222222333333333.........
alaska_map_entry_t *alaska_map_drill_512(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 36) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 27) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 18) & 0b111111111;  // index into uint64_t
  uint64_t ind3 = (address >> 9) & 0b111111111;   // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  uint64_t *lvl2 = (uint64_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(uint64_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  alaska_map_entry_t *lvl3 = (alaska_map_entry_t *)lvl2[ind2];
  if (unlikely(lvl3 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl3 = alaska_alloc_map_frame(ind2, sizeof(alaska_map_entry_t), 512);
    lvl2[ind2] = (uint64_t)lvl3;
  }
  return &lvl3[ind3];
}

// FAAASSSSS_________000000000111111111222222222333333333..........
alaska_map_entry_t *alaska_map_drill_1024(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 37) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 28) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 19) & 0b111111111;  // index into uint64_t
  uint64_t ind3 = (address >> 10) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  uint64_t *lvl2 = (uint64_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(uint64_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  alaska_map_entry_t *lvl3 = (alaska_map_entry_t *)lvl2[ind2];
  if (unlikely(lvl3 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl3 = alaska_alloc_map_frame(ind2, sizeof(alaska_map_entry_t), 512);
    lvl2[ind2] = (uint64_t)lvl3;
  }
  return &lvl3[ind3];
}

// FAAASSSSS________000000000111111111222222222333333333...........
alaska_map_entry_t *alaska_map_drill_2048(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 38) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 29) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 20) & 0b111111111;  // index into uint64_t
  uint64_t ind3 = (address >> 11) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  uint64_t *lvl2 = (uint64_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(uint64_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  alaska_map_entry_t *lvl3 = (alaska_map_entry_t *)lvl2[ind2];
  if (unlikely(lvl3 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl3 = alaska_alloc_map_frame(ind2, sizeof(alaska_map_entry_t), 512);
    lvl2[ind2] = (uint64_t)lvl3;
  }
  return &lvl3[ind3];
}

// FAAASSSSS_______000000000111111111222222222333333333............
alaska_map_entry_t *alaska_map_drill_4096(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 39) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 30) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 21) & 0b111111111;  // index into uint64_t
  uint64_t ind3 = (address >> 12) & 0b111111111;  // index into alaska_map_entry_t

  // printf("translate %p: %4d %4d %4d %4d\n", address, ind0, ind1, ind2, ind3);
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  uint64_t *lvl2 = (uint64_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(uint64_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  alaska_map_entry_t *lvl3 = (alaska_map_entry_t *)lvl2[ind2];
  if (unlikely(lvl3 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl3 = alaska_alloc_map_frame(ind2, sizeof(alaska_map_entry_t), 512);
    lvl2[ind2] = (uint64_t)lvl3;
  }
  return &lvl3[ind3];
}

// FAAASSSSS______000000000111111111222222222333333333.............
alaska_map_entry_t *alaska_map_drill_8192(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 40) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 31) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 22) & 0b111111111;  // index into uint64_t
  uint64_t ind3 = (address >> 13) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  uint64_t *lvl2 = (uint64_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(uint64_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  alaska_map_entry_t *lvl3 = (alaska_map_entry_t *)lvl2[ind2];
  if (unlikely(lvl3 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl3 = alaska_alloc_map_frame(ind2, sizeof(alaska_map_entry_t), 512);
    lvl2[ind2] = (uint64_t)lvl3;
  }
  return &lvl3[ind3];
}

// FAAASSSSS_____000000000111111111222222222333333333..............
alaska_map_entry_t *alaska_map_drill_16384(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 41) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 32) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 23) & 0b111111111;  // index into uint64_t
  uint64_t ind3 = (address >> 14) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  uint64_t *lvl2 = (uint64_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(uint64_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  alaska_map_entry_t *lvl3 = (alaska_map_entry_t *)lvl2[ind2];
  if (unlikely(lvl3 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl3 = alaska_alloc_map_frame(ind2, sizeof(alaska_map_entry_t), 512);
    lvl2[ind2] = (uint64_t)lvl3;
  }
  return &lvl3[ind3];
}

// FAAASSSSS____000000000111111111222222222333333333...............
alaska_map_entry_t *alaska_map_drill_32768(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 42) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 33) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 24) & 0b111111111;  // index into uint64_t
  uint64_t ind3 = (address >> 15) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  uint64_t *lvl2 = (uint64_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(uint64_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  alaska_map_entry_t *lvl3 = (alaska_map_entry_t *)lvl2[ind2];
  if (unlikely(lvl3 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl3 = alaska_alloc_map_frame(ind2, sizeof(alaska_map_entry_t), 512);
    lvl2[ind2] = (uint64_t)lvl3;
  }
  return &lvl3[ind3];
}

// FAAASSSSS___000000000111111111222222222333333333................
alaska_map_entry_t *alaska_map_drill_65536(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 43) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 34) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 25) & 0b111111111;  // index into uint64_t
  uint64_t ind3 = (address >> 16) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  uint64_t *lvl2 = (uint64_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(uint64_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  alaska_map_entry_t *lvl3 = (alaska_map_entry_t *)lvl2[ind2];
  if (unlikely(lvl3 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl3 = alaska_alloc_map_frame(ind2, sizeof(alaska_map_entry_t), 512);
    lvl2[ind2] = (uint64_t)lvl3;
  }
  return &lvl3[ind3];
}

// FAAASSSSS__000000000111111111222222222333333333.................
alaska_map_entry_t *alaska_map_drill_131072(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 44) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 35) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 26) & 0b111111111;  // index into uint64_t
  uint64_t ind3 = (address >> 17) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  uint64_t *lvl2 = (uint64_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(uint64_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  alaska_map_entry_t *lvl3 = (alaska_map_entry_t *)lvl2[ind2];
  if (unlikely(lvl3 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl3 = alaska_alloc_map_frame(ind2, sizeof(alaska_map_entry_t), 512);
    lvl2[ind2] = (uint64_t)lvl3;
  }
  return &lvl3[ind3];
}

// FAAASSSSS_000000000111111111222222222333333333..................
alaska_map_entry_t *alaska_map_drill_262144(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 45) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 36) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 27) & 0b111111111;  // index into uint64_t
  uint64_t ind3 = (address >> 18) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  uint64_t *lvl2 = (uint64_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(uint64_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  alaska_map_entry_t *lvl3 = (alaska_map_entry_t *)lvl2[ind2];
  if (unlikely(lvl3 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl3 = alaska_alloc_map_frame(ind2, sizeof(alaska_map_entry_t), 512);
    lvl2[ind2] = (uint64_t)lvl3;
  }
  return &lvl3[ind3];
}

// FAAASSSSS000000000111111111222222222333333333...................
alaska_map_entry_t *alaska_map_drill_524288(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 46) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 37) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 28) & 0b111111111;  // index into uint64_t
  uint64_t ind3 = (address >> 19) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  uint64_t *lvl2 = (uint64_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(uint64_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  alaska_map_entry_t *lvl3 = (alaska_map_entry_t *)lvl2[ind2];
  if (unlikely(lvl3 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl3 = alaska_alloc_map_frame(ind2, sizeof(alaska_map_entry_t), 512);
    lvl2[ind2] = (uint64_t)lvl3;
  }
  return &lvl3[ind3];
}

// FAAASSSSS________000000000111111111222222222....................
alaska_map_entry_t *alaska_map_drill_1048576(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 38) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 29) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 20) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  alaska_map_entry_t *lvl2 = (alaska_map_entry_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(alaska_map_entry_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  return &lvl2[ind2];
}

// FAAASSSSS_______000000000111111111222222222.....................
alaska_map_entry_t *alaska_map_drill_2097152(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 39) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 30) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 21) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  alaska_map_entry_t *lvl2 = (alaska_map_entry_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(alaska_map_entry_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  return &lvl2[ind2];
}

// FAAASSSSS______000000000111111111222222222......................
alaska_map_entry_t *alaska_map_drill_4194304(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 40) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 31) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 22) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  alaska_map_entry_t *lvl2 = (alaska_map_entry_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(alaska_map_entry_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  return &lvl2[ind2];
}

// FAAASSSSS_____000000000111111111222222222.......................
alaska_map_entry_t *alaska_map_drill_8388608(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 41) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 32) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 23) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  alaska_map_entry_t *lvl2 = (alaska_map_entry_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(alaska_map_entry_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  return &lvl2[ind2];
}

// FAAASSSSS____000000000111111111222222222........................
alaska_map_entry_t *alaska_map_drill_16777216(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 42) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 33) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 24) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  alaska_map_entry_t *lvl2 = (alaska_map_entry_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(alaska_map_entry_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  return &lvl2[ind2];
}

// FAAASSSSS___000000000111111111222222222.........................
alaska_map_entry_t *alaska_map_drill_33554432(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 43) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 34) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 25) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  alaska_map_entry_t *lvl2 = (alaska_map_entry_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(alaska_map_entry_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  return &lvl2[ind2];
}

// FAAASSSSS__000000000111111111222222222..........................
alaska_map_entry_t *alaska_map_drill_67108864(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 44) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 35) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 26) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  alaska_map_entry_t *lvl2 = (alaska_map_entry_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(alaska_map_entry_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  return &lvl2[ind2];
}

// FAAASSSSS_000000000111111111222222222...........................
alaska_map_entry_t *alaska_map_drill_134217728(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 45) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 36) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 27) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  alaska_map_entry_t *lvl2 = (alaska_map_entry_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(alaska_map_entry_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  return &lvl2[ind2];
}

// FAAASSSSS000000000111111111222222222............................
alaska_map_entry_t *alaska_map_drill_268435456(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 46) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 37) & 0b111111111;  // index into uint64_t
  uint64_t ind2 = (address >> 28) & 0b111111111;  // index into alaska_map_entry_t
  uint64_t *lvl1 = (uint64_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(uint64_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  alaska_map_entry_t *lvl2 = (alaska_map_entry_t *)lvl1[ind1];
  if (unlikely(lvl2 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl2 = alaska_alloc_map_frame(ind1, sizeof(alaska_map_entry_t), 512);
    lvl1[ind1] = (uint64_t)lvl2;
  }
  return &lvl2[ind2];
}

// FAAASSSSS________000000000111111111.............................
alaska_map_entry_t *alaska_map_drill_536870912(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 38) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 29) & 0b111111111;  // index into alaska_map_entry_t
  alaska_map_entry_t *lvl1 = (alaska_map_entry_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(alaska_map_entry_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  return &lvl1[ind1];
}

// FAAASSSSS_______000000000111111111..............................
alaska_map_entry_t *alaska_map_drill_1073741824(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 39) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 30) & 0b111111111;  // index into alaska_map_entry_t
  alaska_map_entry_t *lvl1 = (alaska_map_entry_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(alaska_map_entry_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  return &lvl1[ind1];
}

// FAAASSSSS______000000000111111111...............................
alaska_map_entry_t *alaska_map_drill_2147483648(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 40) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 31) & 0b111111111;  // index into alaska_map_entry_t
  alaska_map_entry_t *lvl1 = (alaska_map_entry_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(alaska_map_entry_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  return &lvl1[ind1];
}

// FAAASSSSS_____000000000111111111................................
alaska_map_entry_t *alaska_map_drill_4294967296(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 41) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 32) & 0b111111111;  // index into alaska_map_entry_t
  alaska_map_entry_t *lvl1 = (alaska_map_entry_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(alaska_map_entry_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  return &lvl1[ind1];
}

// FAAASSSSS____000000000111111111.................................
alaska_map_entry_t *alaska_map_drill_8589934592(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 42) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 33) & 0b111111111;  // index into alaska_map_entry_t
  alaska_map_entry_t *lvl1 = (alaska_map_entry_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(alaska_map_entry_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  return &lvl1[ind1];
}

// FAAASSSSS___000000000111111111..................................
alaska_map_entry_t *alaska_map_drill_17179869184(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 43) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 34) & 0b111111111;  // index into alaska_map_entry_t
  alaska_map_entry_t *lvl1 = (alaska_map_entry_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(alaska_map_entry_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  return &lvl1[ind1];
}

// FAAASSSSS__000000000111111111...................................
alaska_map_entry_t *alaska_map_drill_34359738368(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 44) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 35) & 0b111111111;  // index into alaska_map_entry_t
  alaska_map_entry_t *lvl1 = (alaska_map_entry_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(alaska_map_entry_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  return &lvl1[ind1];
}

// FAAASSSSS_000000000111111111....................................
alaska_map_entry_t *alaska_map_drill_68719476736(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 45) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 36) & 0b111111111;  // index into alaska_map_entry_t
  alaska_map_entry_t *lvl1 = (alaska_map_entry_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(alaska_map_entry_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  return &lvl1[ind1];
}

// FAAASSSSS000000000111111111.....................................
alaska_map_entry_t *alaska_map_drill_137438953472(uint64_t address, uint64_t *lvl0, int allocate) {
  uint64_t ind0 = (address >> 46) & 0b111111111;  // index into uint64_t
  uint64_t ind1 = (address >> 37) & 0b111111111;  // index into alaska_map_entry_t
  alaska_map_entry_t *lvl1 = (alaska_map_entry_t *)lvl0[ind0];
  if (unlikely(lvl1 == NULL)) {  // slow: allocation
    if (allocate == 0) return NULL;
    lvl1 = alaska_alloc_map_frame(ind0, sizeof(alaska_map_entry_t), 512);
    lvl0[ind0] = (uint64_t)lvl1;
  }
  return &lvl1[ind1];
}
// A table to map the S bits in a handle address to the drill size
alaska_map_driller_t alaska_drillers[32] = {
    alaska_map_drill_64,
    alaska_map_drill_128,
    alaska_map_drill_256,
    alaska_map_drill_512,
    alaska_map_drill_1024,
    alaska_map_drill_2048,
    alaska_map_drill_4096,
    alaska_map_drill_8192,
    alaska_map_drill_16384,
    alaska_map_drill_32768,
    alaska_map_drill_65536,
    alaska_map_drill_131072,
    alaska_map_drill_262144,
    alaska_map_drill_524288,
    alaska_map_drill_1048576,
    alaska_map_drill_2097152,
    alaska_map_drill_4194304,
    alaska_map_drill_8388608,
    alaska_map_drill_16777216,
    alaska_map_drill_33554432,
    alaska_map_drill_67108864,
    alaska_map_drill_134217728,
    alaska_map_drill_268435456,
    alaska_map_drill_536870912,
    alaska_map_drill_1073741824,
    alaska_map_drill_2147483648,
    alaska_map_drill_4294967296,
    alaska_map_drill_8589934592,
    alaska_map_drill_17179869184,
    alaska_map_drill_34359738368,
    alaska_map_drill_68719476736,
    alaska_map_drill_137438953472,
};
