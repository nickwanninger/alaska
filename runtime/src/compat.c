#include <stdio.h>

extern void *alaska_lock_for_escape(const void *ptr); // in runtime.c

int alaska_wrapped_puts(const char *s) {
	return puts(alaska_lock_for_escape(s));
}
