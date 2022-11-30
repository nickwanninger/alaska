#include "../runtime/include/alaska.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


struct data {
  int x;
  int y;
};

void inc(int *x) { (*x) += 1; }
void inc_y(struct data *d) {
	d->x--;
	d->y++;
}

int main(int argc, char **argv) {
  int *ptr = (int *)halloc(sizeof(int));
  *ptr = 0;
  printf("%p\n", ptr);
  inc(ptr + 1);

  hfree(ptr);
  return 0;
}
