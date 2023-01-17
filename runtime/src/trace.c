#include <alaska/internal.h>
#include <alaska.h>
#include <string.h>


#define TRACE(...) fprintf(get_trace_file(), __VA_ARGS__)
static void close_trace_file(void);
static FILE *get_trace_file(void) {
	static FILE *stream = NULL;
	if (stream == NULL) {
		atexit(close_trace_file);
		stream = fopen("alaska.trace", "w");
	}
	return stream;
}

static void close_trace_file(void) {
	fclose(get_trace_file());
}

void *halloc_trace(size_t sz) {
	void *p = malloc(sz);
	TRACE("M %p %zu\n", p, sz);
	return p;
}


void *hcalloc_trace(size_t nmemb, size_t size) {
	void *p = halloc_trace(nmemb * size);
	memset(p, 0, nmemb * size);
	return p;
}


void *hrealloc_trace(void *handle, size_t sz) {
	void *new_ptr = realloc(handle, sz);
	TRACE("R %p %p %zu\n", handle, sz);
	return new_ptr;
}


void hfree_trace(void *ptr) {
	TRACE("F %p\n", ptr);
	free(ptr);
}



void *alaska_lock_trace(void *restrict ptr) {
	TRACE("L %p\n", ptr);
	return ptr;
}

void alaska_unlock_trace(void *restrict ptr) {
	TRACE("U %p\n", ptr);
}
