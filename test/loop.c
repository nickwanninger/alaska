int sumarray(int* restrict vals, int len) {
  int sum = 0;
  for (int i = 0; i < len; i++) {
		if (vals[i] > 10)
    	sum += vals[i];
  }
  return sum;
}

int main() { return 0; }
