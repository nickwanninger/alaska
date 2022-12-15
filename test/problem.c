#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <alaska.h>
struct thing {
  int x, y;
};

void (*global)(struct thing *);
int *field;
struct thing **arg;

void foo(struct thing *s) {
  field = &s->x;
  s->x += 1;
}
void bar(struct thing *s) {
  field = &s->y;
  s->y *= 2;
}


int main(int argc, char **argv) {
  if (argv[1][0] == 'a') {
    global = foo;
  } else {
    global = bar;
  }

  struct thing *thing1 = (struct thing *)halloc(sizeof(*thing1));
  struct thing *thing2 = (struct thing *)malloc(sizeof(*thing2));
  thing1->x = 1;
  thing1->y = 2;

  thing2->x = 3;
  thing2->y = 4;

  if (argv[2][0] == 'a') {
    arg = &thing1;
  } else {
    arg = &thing2;
  }

  global(*arg);

  printf("field: %p = %d\n", field, *field);
  printf("arg:   %p = %p\n", arg, *arg);
}
