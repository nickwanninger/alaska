#include <stdio.h>
#include <stdlib.h>

#define NUM_OBJECTS 512

// Allocate variable sized objects, and free some of them
// (To test a VariablePage heap)
void *objects[NUM_OBJECTS];

int main() {
  srand(0);
  // Initialize the objects
  for (int i = 0; i < NUM_OBJECTS; i++) {
    size_t sz = 32;
    sz = (rand() & 0x7F);
    // printf("sz = %zu\n", sz);
    objects[i] = calloc(1, sz);
  }

  // Free half of the objects
  for (int i = 0; i < NUM_OBJECTS; i++) {
    free(objects[i]);
  }

  return 0;
}
