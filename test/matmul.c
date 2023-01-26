#include <stdio.h>

#define SZ 10
void matmul(float a[SZ][SZ], float b[SZ][SZ], float c[SZ][SZ]) {
  for (int i = 0; i < SZ; i++) {
    for (int j = 0; j < SZ; j++) {
      c[i][j] = 0;
      for (int k = 0; k < SZ; k++) {
        c[i][j] += a[i][k] * b[k][j];
      }
    }
  }

}

int main() {
	return 0;
}
