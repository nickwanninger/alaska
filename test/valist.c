#include <stdlib.h>
#include <stdio.h>
#include <string.h>

ssize_t FormatLocaleStringList(
    char *restrict string, const size_t length, const char *restrict format, va_list operands) {
  ssize_t n;
  n = (ssize_t)vsprintf(string, format, operands);
  if (n < 0) string[length - 1] = '\0';
  return (n);
}

ssize_t FormatLocaleString(
    char *restrict string, const size_t length, const char *restrict format, ...) {
  ssize_t n;

  va_list operands;

  va_start(operands, format);
  n = FormatLocaleStringList(string, length, format, operands);
  va_end(operands);
  return (n);
}

int main(int argc, char **argv) {
	char *out = malloc(512);

	char *msg = malloc(16);
	strcpy(msg, "hello");
	FormatLocaleString(out, 512, "msg = '%s'\n", msg);

	puts(out);
	return 0;
}
