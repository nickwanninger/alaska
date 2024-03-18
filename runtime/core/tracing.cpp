#include <stdio.h>


extern "C" void alaska_trace_translation(void * handle) {
  printf("Translate %p\n", handle);
}
