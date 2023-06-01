#include <string>
#include <cstdio>
#include <cstdlib>

#define noinline __attribute__((noinline))

class MyClass {
 public:
  MyClass()
      : i(0)
      , j(0)
      , k(0) {
    // Do nothing.
  }

  int noinline set(int i) {
    this->i = i;
    update();
    return this->k;
  }

  void noinline update() {
    this->j += this->i;
    this->k ^= this->j;
  }

  int i, j, k;
};

int main() {
  auto my_thing = new MyClass();

  auto max = rand() % 100;
  
  for (auto i = 0; i < max; i++) {
    my_thing->set(i);
  }

  printf("Your lucky numbers are %d, %d, %d\n", my_thing->i, my_thing->j, my_thing->k);

  return 0;
}
