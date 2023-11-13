// This program is designed to evaluate how the alaska barrier system handles a thread being "out to
// lunch". This means the thread is out in the kernel, perhaps sleeping, and is meant to validate
// that the barrier system correctly signals the thread without breaking everything :)
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>


struct thingy {
  volatile int value;
};

int fib(int n) {
  if (n < 2) {
    usleep(100);
    return n;
  }

  return fib(n-1) + fib(n-2);
}

void *lunch(void *arg) {
  struct thingy *x = arg;
  while (1) {
    x->value = 4;
    usleep(10000);  // sleep for some time.
    x->value = fib(25);
  }
  return NULL;
}

int main() {
  int num = 1;
  pthread_t threads[num];

  for (int i = 0; i < num; i++) {
    void *arg = malloc(sizeof(struct thingy));
    pthread_create(&threads[i], NULL, lunch, arg);
  }

  for (int i = 0; i < num; i++) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}
