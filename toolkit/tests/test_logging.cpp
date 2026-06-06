#include "imtest.hpp"

#include <thread>
#include <vector>

#include <imtool/common/logging.hpp>

using namespace imtool;

int main() {
  IMTEST_SUITE("logging");

  Logger lg(/*terminal_echo=*/false);
  lg.setPrintLevel(LogLevel::Debug);

  // flush() commits a buffered line; clear() empties.
  CHECK(lg.lineCount() == 0);
  lg << LogLevel::Info << "hello " << 42;
  lg.flush();
  CHECK(lg.lineCount() == 1);
  lg << LogLevel::Warning << "second line";
  lg.flush();
  CHECK(lg.lineCount() == 2);
  lg.clear();
  CHECK(lg.lineCount() == 0);

  // embedded newlines split into separate lines
  lg << LogLevel::Info << "a\nb\nc";
  lg.flush();
  CHECK(lg.lineCount() == 3);
  lg.clear();

  // level filtering (use fresh loggers — once a buffer's level exceeds the
  // print level, the stream stays latched; that recovery quirk is tracked
  // separately and is out of scope for this PR).
  {
    Logger f(false);
    f.setPrintLevel(LogLevel::Warning);
    f << LogLevel::Debug << "below threshold";
    f.flush();
    CHECK(f.lineCount() == 0);
  }
  {
    Logger f(false);
    f.setPrintLevel(LogLevel::Warning);
    f << LogLevel::Error << "kept";
    f.flush();
    CHECK(f.lineCount() == 1);
  }

  // pushLevel / popLevel must balance without deadlock
  lg.pushLevel(LogLevel::Error);
  lg.popLevel();

  // === Findings #6 / #17: the logger mutex must not deadlock under concurrent
  // log+flush. A lock-ordering regression would hang here; the CTest TIMEOUT
  // on this test turns a hang into a reported failure rather than a stuck CI.
  // ===
  {
    constexpr int kThreads = 8;
    constexpr int kIters = 2000;
    Logger clg(false);
    clg.setPrintLevel(LogLevel::Debug);
    std::vector<std::thread> pool;
    for (int t = 0; t < kThreads; t++) {
      pool.emplace_back([&clg, t]() {
        for (int i = 0; i < kIters; i++) {
          clg << LogLevel::Info << "t" << t << " i" << i;
          clg.flush();
        }
      });
    }
    for (auto &th : pool) {
      th.join();
    }
    CHECK(clg.lineCount() > 0); // reached here => no deadlock
  }

  return imtest::report();
}
