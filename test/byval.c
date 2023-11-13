#include <stdlib.h>
struct S {
    int a[100];
    int b;
};

void func1(struct S s) {
    s.b = 111;
}

void func2(struct S *s) {
    s->b = 222;
}

int main() {
  // struct S *s = malloc(sizeof(*s));
  // func1(*s);
}
