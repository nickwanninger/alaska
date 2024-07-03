#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>


#define MAX_SIZE 512
#define ARRAY_LENGTH (4096 * 512)
void *objects[ARRAY_LENGTH];

int main() {
  for (int run = 0; run < 10000; run++) {
#pragma omp parallel for shared(objects)
    for (int i = 0; i < ARRAY_LENGTH; i++) {
      objects[i] = malloc(rand() % MAX_SIZE);
    }

#pragma omp parallel for shared(objects)
    for (int i = 0; i < ARRAY_LENGTH; i++) {
      free(objects[i]);
    }

    memset(objects, 0, sizeof(objects));
  }

  return 0;
}
