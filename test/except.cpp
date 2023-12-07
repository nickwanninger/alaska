#include <stdexcept>

__attribute__((noinline)) int compare(unsigned char a, wchar_t b) {
  if (a < 0 || b < 0) {
    throw std::invalid_argument("received negative value");
  }
  return a - b;
}

__attribute__((noinline)) unsigned short square(unsigned short x) {
  if (x == 0) {
    throw std::invalid_argument("received zero");
  }
  return x * x;
}



int main() {
  try {
    // int res = compare(-1, 3);
    int res = square(0x101073);
    printf("res = %x\n", res);
  } catch (const std::invalid_argument& e) {
    // do stuff with exception...
  }
  return 0;
}
