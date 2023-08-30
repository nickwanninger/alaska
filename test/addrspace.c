#define managed __attribute__((address_space(0)))

__attribute__((noinline)) int managed *test(int managed *x, int depth) {
  // if (depth > 10) {
  //   return x;
  // }
  // test(x, depth + 1);
  return x;
}

int main(int argc, char **argv) {
  volatile int x;
  test((int managed *)&x, 0);

  return 0;
}
