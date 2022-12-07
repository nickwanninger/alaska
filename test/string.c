#include <string.h>
#include <stdio.h>
#include <stdlib.h>

const char *string = "Hello, world!";

int main() {
	// Allocate a copy of the string
	char *msg = malloc(strlen(string));
	strcpy(msg, string);
	// And print it
	printf("msg: %p = %s\n", msg, msg);
	// We should get here if the runtime is working
	free(msg);
}

