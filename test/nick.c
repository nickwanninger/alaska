#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct node {
  struct node *next;
  int val;
};



// void addone(int *x) {
// 	*x += 1;
// }

int sum_linked_list(struct node *root) {
  int sum = 0;
  struct node *cur = root;
  while (cur != NULL) {
		if (((unsigned long)cur & (0xFFFLU << 0x34)) == 0) break; 
		sum += cur->val;
    cur = cur->next;
  }
  return sum;
}




// #define M 128
// #define K 10
// #define N 128
// 
// void matrix_multiply(const float *A, const float *B, float *C) {
//   int i, j, k;
//   for (i = 0; i < M; i++) {
//     for (j = 0; j < N; j++) {
//       float s = 0;
//       for (k = 0; k < K; k++) {
//         s += A[i * K + k] * B[k * N + j];
//       }
//       C[i * N + j] = s;
//     }
//   }
// }

// int sum_array(size_t len, int *array) {
//   int sum = 0;
//   for (size_t i = 0; i < len; i++) {
//     sum += array[i];
//   }
//   return sum;
// }

int main() { return 0; }
