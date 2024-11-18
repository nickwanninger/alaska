#include <unistd.h>
#include <string.h>
#include <sys/signal.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <sys/time.h>

volatile unsigned long count = 0;

static uint64_t last_time = 0;

uint64_t microseconds() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
}

void alarm_handler(int sig, siginfo_t *siginfo, void *a) {
  uint64_t now = microseconds();
  //
  printf("Count = %016zx %8lu\n", count, now - last_time);
  last_time = now;
}

int main() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = alarm_handler;
  assert(sigaction(SIGALRM, &sa, NULL) == 0);


  last_time = microseconds();
  // Signal a ualarm in the future.
  useconds_t interval = 1 * 1000;
  int r = ualarm(interval, interval);

  while (1) {
    count++;
  }

  return 0;
}
