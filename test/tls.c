#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

uint64_t timestamp() {
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

#define TRIALS 1000000


__thread volatile uint64_t tls = 0;
volatile uint64_t global = 0;

__attribute__((noinline)) void inc_tls(void) {
	tls++;
}

__attribute__((noinline)) void inc_glob(void) {
	global++;
}


int main(int argc, char** argv) {
  uint64_t start, end;

  for (int t = 0; t < 100; t++) {

    start = timestamp();
    for (int i = 0; i < TRIALS; i++)
      inc_tls();
    end = timestamp();


    double t_tls = (end - start) / (double)TRIALS;

    start = timestamp();
    for (int i = 0; i < TRIALS; i++)
      inc_glob();
    end = timestamp();
    double t_glob = (end - start) / (double)TRIALS;

    printf("%lf,%lf,%lf\n", t_tls, t_glob, t_tls - t_glob);
  }
  return 0;
}
