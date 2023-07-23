
__attribute__((noinline)) void inc(volatile int *v) {
	*v += 1;
}

int main() {
	volatile int x = 1;
	inc(&x);
  return 0;
}
