#include <stdio.h>
#include <signal.h>

extern void *alaska_lock_for_escape(const void *ptr); // in runtime.c

int alaska_wrapped_puts(const char *s) {
	return puts(alaska_lock_for_escape(s));
}


int alaska_wrapped_sigaction(int signum, const struct sigaction *act,
                     struct sigaction *oldact) {
	if (signum == SIGSEGV) {
		return -1;
	}
	return sigaction(signum, act, oldact);
}
