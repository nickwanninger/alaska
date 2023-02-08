#include <stdexcept>
#include <limits>
#include <iostream>


void test(int c) {
  if (c == 42) throw std::invalid_argument("Invalid arguent");

  printf("c = %d\n", c);
  //...
}

int main(int argc, char **argv) {
	try {
  	test(argv[1][1]);
	} catch (std::exception e) {
		printf("got exception\n");
	}
  return 0;
}
