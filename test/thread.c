#include <pthread.h>
#include <alaska.h>
#include <stdlib.h>
#include <unistd.h>

void *worker(void *arg) {
  while (1) {
    // Do nothing
  }
  return NULL;
}

int main() {
  pthread_t pt;
  pthread_create(&pt, NULL, worker, NULL);

  while (1) {
    alaska_barrier();
    usleep(1000 * 10);
  }

  pthread_join(pt, NULL);

  return 0;
}
