#include <string.h>
#include <stdio.h>
#include <stdlib.h>
// #include <alaska.h>
const char *string = "Hello, world!";

int main() {
	size_t len = strlen(string) + 1;
	// Allocate a copy of the string
  char *msg = malloc(len);
  strcpy(msg, string);
	// And print it
	printf("msg: %p = %s\n", msg, msg);
	// We should get here if the runtime is working
	free(msg);
}

