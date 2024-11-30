#include <stdio.h>
#include <stdlib.h>

#define NUM_OBJECTS 10000


struct object {
  int id;
  char name[20];
};


int main() {
  // Allocate memory for an array of objects
  struct object **objects = calloc(NUM_OBJECTS, sizeof(struct object *));

  // Initialize the objects
  for (int i = 0; i < NUM_OBJECTS; i++) {
    objects[i] = calloc(1, sizeof(struct object));
  }

  // Free half of the objects
  for (int i = 0; i < NUM_OBJECTS; i += 2) {
    free(objects[i]);
    objects[i] = NULL;
  }


  // Free the other half
  for (int i = 1; i < NUM_OBJECTS; i += 2) {
    free(objects[i]);
    objects[i] = NULL;
  }

  // Free the memory allocated for the objects
  free(objects);

  return 0;
}