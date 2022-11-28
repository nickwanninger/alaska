#include <alaska.h>

int main() {
  volatile int *x = halloc(sizeof(*x));
  *x = 30;
  return 0;
}
