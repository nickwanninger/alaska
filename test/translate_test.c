#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

extern uint64_t *__alaska_drill_size_64(uint64_t address, uint64_t *top_level, int allocate);


uint64_t now_ns() {
  struct timespec spec;
  if (clock_gettime(1, &spec) == -1) { /* 1 is CLOCK_MONOTONIC */
    abort();
  }

  return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
}

#define TRIALS (4096 * 512 * 10)
uint64_t trials[TRIALS * 2];

int main() {
  uint64_t *pagetable = calloc(sizeof(uint64_t), 512);

	for (uint64_t i = 0; i < TRIALS; i++) {
		uint64_t start = now_ns();
		uint64_t *ptr = __alaska_drill_size_64(i, pagetable, 1);

		

		// trials[i] = now_ns() - start;
		printf("%lu\n", now_ns() - start);
	}
	return 0;

	for (uint64_t i = 0; i < TRIALS; i++) {
		uint64_t start = now_ns();
			uint64_t *ptr = __alaska_drill_size_64(i * 512, pagetable, 1);
		trials[i + TRIALS] = now_ns() - start;
	}

	for (uint64_t i = 0; i < TRIALS * 1; i++) {
		printf("%lu\n", trials[i]);
	}
}
