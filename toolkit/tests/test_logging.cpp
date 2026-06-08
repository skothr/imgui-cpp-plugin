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

  // level filtering: content below the print level is dropped.
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

  // MAIN-363 regression: a LogLevel manipulator must always be honored, so a
  // later `<< LogLevel::X` un-latches the stream after a filtered higher level.
  // (Previously the print-level guard ran before the LogLevel branch, so once
  // latched the stream dropped everything — including the un-latching token.)
  {
    Logger f(false);
    f.setPrintLevel(LogLevel::Warning);
    f << LogLevel::Debug << "hidden"      // filtered (Debug > Warning): dropped
      << LogLevel::Error << "shown";      // Error <= Warning: must un-latch + record
    f.flush();
    CHECK(f.lineCount() == 1);            // only "shown" — the pre-fix latch recorded 0
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
