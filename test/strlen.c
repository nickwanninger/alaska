#include <stdlib.h>

int my_strlen(const char *c) {
  int len = 0;
  while (1) {
    // if (rand()) c = "hello";
    if (*c == 0) break;
    len++;
    c++;
  }

  return len;
}

int main() { return 0; }
