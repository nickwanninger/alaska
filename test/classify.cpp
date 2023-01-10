#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <alaska.hpp>

int main() {
  std::vector<int, alaska::allocator<int, 4>> vec;
  for (int i = 0; i < 32; i++) {
    vec.push_back(rand());
  }
	for (auto &i : vec) {
		printf("%d\n", i);
	}
}
