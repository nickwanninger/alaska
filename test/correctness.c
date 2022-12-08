#include <alaska.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static inline uint64_t timestamp() {
#if defined(__x86_64__)
  uint32_t rdtsc_hi_, rdtsc_lo_;
  __asm__ volatile("rdtsc" : "=a"(rdtsc_lo_), "=d"(rdtsc_hi_));
  return (uint64_t)rdtsc_hi_ << 32 | rdtsc_lo_;
#else

  struct timespec spec;
  clock_gettime(1, &spec);
  return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
#endif
}


alaska_rt int main() {
  volatile int *p = (volatile int *)halloc(sizeof(int));

  for (int trials = 1; trials < 1000000; trials *= 2) {
    uint64_t start = timestamp();
    for (int i = 0; i < trials; i++) {
      *p = i;
    }
    uint64_t end = timestamp();
    printf("emulation avg over %d trials: %f\n", trials, (end - start) / (float)trials);
  }
  return 0;
}
