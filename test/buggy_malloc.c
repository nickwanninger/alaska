#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

static bool eep = false;

void *buggy_malloc(int sz) {
  if (eep) {
    return (void*)-1L; // 0xFFFFFFFFFFFFFFFF
  }
  return malloc(sz);
}

struct thingy {
  int val;
};

struct thingy *foo(int v) {
  struct thingy *x = buggy_malloc(sizeof(struct thingy));
  if (x != (void *)-1L) {
    x->val = v;
  }
  return x;
}


int main(int argc, char **argv) {
  if (argc > 1) eep = true;
  struct thingy *v = foo(42);

  printf("%p\n", v);
  return 0;
}
