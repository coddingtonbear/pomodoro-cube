#pragma once

// Minimal shared assert harness. Deliberately not a test framework: there is
// one test binary in this project and a dependency would cost more than these
// few lines.
#include <cstdio>

inline int &checkFailures() {
  static int failures = 0;
  return failures;
}

inline void checkImpl(bool condition, const char *what, int line, const char *file) {
  if (condition) return;
  std::printf("FAIL (%s:%d): %s\n", file, line, what);
  checkFailures()++;
}

#define CHECK(expr) checkImpl((expr), #expr, __LINE__, __FILE__)
#define CHECK_MSG(cond, msg) checkImpl((cond), (msg), __LINE__, __FILE__)
