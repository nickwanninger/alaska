#include <stdio.h>
#include <alaska.h>


#define SZ 512

__attribute__((noinline)) void matmul(double a[SZ][SZ], double b[SZ][SZ], double c[SZ][SZ]) {
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

	double a[SZ][SZ];
	double b[SZ][SZ];
	double c[SZ][SZ];
	matmul(a, b, c);
	return 0;
}
