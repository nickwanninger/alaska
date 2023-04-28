#include <stdlib.h>

// int sumarray(int* restrict vals, int len) {
//   int sum = 0;
//   for (int i = 0; i < len; i++) {
// 		if (vals[i] > 10)
//     	sum += vals[i];
//   }
//   return sum;
// }

void squarearray(int* restrict vals, int len) {
  for (int i = 0; i < len; i++) {
    vals[i] *= vals[i];
  }
}

int main() {
  int array[32];
  for (int i = 0; i < 32; i++) {
    i = rand();
  }
  squarearray(array, 32);
  return 0;
}
