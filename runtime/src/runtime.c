#include <alaska.h>
#include <alaska/internal.h>
#include <assert.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <jemalloc/jemalloc.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>



// High priority constructor: todo: do this lazily when you call halloc the first time.
static void __attribute__((constructor(102))) alaska_init(void) {
  // initialize the translation table first
  alaska_table_init();

#ifdef ALASKA_CLASS_TRACKING
  alaska_classify_init();
#endif
}

static void __attribute__((destructor)) alaska_deinit(void) {
#ifdef ALASKA_CLASS_TRACKING
  alaska_classify_deinit();
#endif

  // delete the table at the end
  alaska_table_deinit();
}

#define BT_BUF_SIZE 100
void alaska_dump_backtrace(void) {
	return;
  int nptrs;

  void* buffer[BT_BUF_SIZE];
  char** strings;

  nptrs = backtrace(buffer, BT_BUF_SIZE);
  printf("Backtrace:\n", nptrs);

  // The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
  // would produce similar output to the following:

  strings = backtrace_symbols(buffer, nptrs);
  if (strings == NULL) {
    perror("backtrace_symbols");
    exit(EXIT_FAILURE);
  }

  for (int j = 0; j < nptrs; j++)
    printf("\x1b[92m%d\x1b[0m: %s\n", j, strings[j]);

  free(strings);
}
