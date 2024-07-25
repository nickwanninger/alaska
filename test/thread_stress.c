#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>


#define MAX_SIZE 512
#define ARRAY_LENGTH (4096 * 1024 * 2)
void *objects[ARRAY_LENGTH];

extern void alaska_dump(void);


int main() {
  for (int run = 0; run < 25; run++) {
    printf("Allocate...\n");
    unsigned int seed = 0;


#pragma omp parallel for shared(objects) private(seed)
    for (int i = 0; i < ARRAY_LENGTH; i++) {
      objects[i] = malloc(rand_r(&seed) % MAX_SIZE);
      // objects[i] = malloc(256);
    }

    // alaska_dump();

    printf("Free...\n");
#pragma omp parallel for shared(objects)
    for (int i = 0; i < ARRAY_LENGTH; i++) {
      free(objects[i]);
      objects[i] = NULL;
    }


    memset(objects, 0, sizeof(objects));
  }

  return 0;
}


// 3e1f43dc009fab8
// 3e1f43dc009fab8
// 3e1e90c4cc82560
