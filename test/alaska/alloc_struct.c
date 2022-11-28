#include <alaska.h>

struct foo {
  int a, b;
};

int main() {
  struct foo *x = halloc(sizeof(*x));
  x->a = 3;
  x->b = 4;
  return 0;
}
