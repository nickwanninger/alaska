#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alaska.h>
#include <omp.h>


int main(int argc, char **argv) {
  while (1) {
#pragma omp parallel
    {
      int *ptr;
      // #pragma omp critical
      { ptr = (int *)malloc(sizeof(int)); }


      *(volatile int *)ptr = 0;
      // (*ptr)++;
      if (omp_get_thread_num() == 0) {
        uint64_t start = alaska_timestamp();
        alaska_barrier();
        uint64_t end = alaska_timestamp();
        printf("%zu\n", end - start);
      }
      // #pragma omp critical
      { free(ptr); }
    }
  }



  return 0;
}
