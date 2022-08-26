#include <alaska.h>

int main() {
  volatile int *x = alaska_alloc(sizeof(*x));
  *x = 30;
  return 0;
}
