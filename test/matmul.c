#include <stdio.h>

#define SZ 10
void matmul(int n, float a[n][n], float b[n][n], float c[n][n]) {


  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      c[i][j] = 0;
      for (int k = 0; k < n; k++) {
        c[i][j] += a[i][k] * b[k][j];
      }
    }
  }

}

int main() {
	return 0;
}
