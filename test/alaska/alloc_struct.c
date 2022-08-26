#include <alaska.h>

struct foo {
  int a, b;
};

int main() {
  struct foo *x = alaska_alloc(sizeof(*x));
  x->a = 3;
  x->b = 4;
  return 0;
}
