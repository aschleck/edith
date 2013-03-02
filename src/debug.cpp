#include "debug.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

void xassert_error(const char *message, ...) {
  va_list parameters;
  va_start(parameters, message);

  char error[2000];
  vsprintf(error, message, parameters);

  va_end(parameters);

  printf("%s\n", error);
  exit(1);
}

