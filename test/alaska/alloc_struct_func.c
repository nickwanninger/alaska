#include <alaska.h>

struct foo {
  int a, b;
};

void modify(struct foo *x) {
  x->a = 3;
  x->b = 4;
}

int main() {
  struct foo *x = halloc(sizeof(*x));
  modify(x);
  return 0;
}
