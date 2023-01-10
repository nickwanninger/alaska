#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <alaska.hpp>

#define CLASS_VECTOR 1
#define CLASS_MAP 2

int main() {
  printf("doing vector stuff\n");
	alaska::vector<int> vec;
  // std::vector<int, alaska::allocator<int, CLASS_VECTOR>> vec;
  for (int i = 0; i < 32; i++) {
    vec.push_back(rand());
  }


 	printf("doing map stuff\n");
	alaska::map<int, int> map;
	map[1] = rand();
	map[2] = rand();
	for (auto &[k, v] : map) {
		printf("%d => %d\n", k, v);
	}
}
