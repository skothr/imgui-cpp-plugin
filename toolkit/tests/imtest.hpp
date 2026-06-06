#pragma once

// imtest — a tiny, dependency-free assertion harness for the imtool unit tests.
//
// Each test file is a standalone executable: it sets a suite name, runs a
// series of CHECK / CHECK_NEAR macros, and ends with `return
// imtest::report();`. The process exit code is non-zero iff any check failed,
// which is exactly what CTest keys off. No GoogleTest / Catch dependency keeps
// the suite buildable offline and in minimal CI containers.

#include <cmath>
#include <cstdio>

namespace imtest {

inline int &checks() {
  static int n = 0;
  return n;
}
inline int &failures() {
  static int n = 0;
  return n;
}
inline const char *&suite() {
  static const char *s = "?";
  return s;
}

[[nodiscard]] inline bool is_near(double a, double b, double eps) {
  return std::fabs(a - b) <= eps;
}

inline int report() {
  std::printf("[%s] %d/%d checks passed%s\n", suite(), checks() - failures(),
              checks(), failures() ? "   <<< FAILURES" : "");
  return failures() ? 1 : 0;
}

} // namespace imtest

#define IMTEST_SUITE(name) (::imtest::suite() = (name))

#define CHECK(cond)                                                            \
  do {                                                                         \
    ++::imtest::checks();                                                      \
    if (!(cond)) {                                                             \
      ++::imtest::failures();                                                  \
      std::fprintf(stderr, "FAIL %s:%d: CHECK(%s)\n", __FILE__, __LINE__,      \
                   #cond);                                                     \
    }                                                                          \
  } while (0)

#define CHECK_NEAR(a, b, eps)                                                  \
  do {                                                                         \
    ++::imtest::checks();                                                      \
    const double _ia = (double)(a), _ib = (double)(b), _ie = (double)(eps);    \
    if (!::imtest::is_near(_ia, _ib, _ie)) {                                   \
      ++::imtest::failures();                                                  \
      std::fprintf(                                                            \
          stderr,                                                              \
          "FAIL %s:%d: CHECK_NEAR(%s, %s): |%.8g - %.8g| = %.3g > %.3g\n",     \
          __FILE__, __LINE__, #a, #b, _ia, _ib, std::fabs(_ia - _ib), _ie);    \
    }                                                                          \
  } while (0)
