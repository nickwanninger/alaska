#include <stdio.h>
#include <stdint.h>

__attribute__((always_inline)) void *translate(void *x) {
  uint64_t v = (uint64_t)x;

  if ((int64_t)v < 0) {
    v = v >> 32;
    return *(void **)(v + (uint32_t)(uint64_t)x);
  }
  return x;
}


int sum_array(int *arr, int n) {
  void *translated_arr = NULL;
  int sum = 0;

  for (int i = 0; i < n; i++) {
    if (translated_arr == NULL) {
      translated_arr = translate(arr);
    }
    sum += ((int *)translated_arr)[i];


    if (translated_arr == NULL) translated_arr = translate(arr);
    ((int *)translated_arr)[i] = 0;
  }

  return sum;
}

int main() {
}
