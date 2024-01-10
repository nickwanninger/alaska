#include <stdlib.h>

int sumarray(int* restrict vals, int len) {
  int sum = 0;
  for (int i = 0; i < len; i++) {
    sum += vals[i];
  }
  return sum;
}


int main() {
  return 0;
}
