#include <stdio.h>
#include <stdlib.h>

#define NUM_OBJECTS 10000

int main() {
  // Allocate memory for an array of objects
  int* objects = (int*)malloc(NUM_OBJECTS * sizeof(int));
  if (objects == NULL) {
    printf("Memory allocation failed!\n");
    return 1;
  }

  // Initialize the objects
  for (int i = 0; i < NUM_OBJECTS; i++) {
    objects[i] = i + 1;
  }

  // Free half of the objects
  for (int i = 0; i < NUM_OBJECTS / 2; i++) {
    free(&objects[i]);
  }

  // Print the remaining objects
  printf("Remaining objects: ");
  for (int i = 0; i < NUM_OBJECTS; i++) {
    if (objects[i] != 0) {
      printf("%d ", objects[i]);
    }
  }
  printf("\n");

  // Free the memory allocated for the objects
  free(objects);

  return 0;
}