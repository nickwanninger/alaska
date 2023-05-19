#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

int main() {
	uint64_t *ptr = malloc(sizeof(uint64_t));
	*ptr = 0xFFAAFFAA;
	uint64_t addr = (uint64_t)ptr;
	addr |= 0xFF00UL << 48;
	ptr = (uint64_t*)addr;

	printf("ptr = %p\n", ptr);
	printf("val = %zu\n", *ptr);

	return 0;
}
