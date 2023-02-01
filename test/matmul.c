#include <stdio.h>

#define SZ 8
void matmul(double a[SZ][SZ], double b[SZ][SZ], double c[SZ][SZ]) {
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
