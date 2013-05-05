#ifndef _DEBUG_H
#define _DEBUG_H

void xassert_error(const char *message, ...) __attribute__((noreturn));

#define XASSERT(test, msg, ...) do { \
  if (!(test)) { \
    xassert_error("\nAssertion at %s:%u failed: %s\n" msg, __FILE__, __LINE__, (#test), \
      ##__VA_ARGS__); \
  } \
} while (0);

#define XERROR(msg, ...) do { \
  xassert_error("\nError at %s:%u: \n" msg, __FILE__, __LINE__, ##__VA_ARGS__); \
} while (0);

#endif

