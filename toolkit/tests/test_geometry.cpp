#include "imtest.hpp"

#include <cmath>

#include <imtool/common/geometry.hpp>

using namespace imtool;

int main() {
  IMTEST_SUITE("geometry");

  // === Finding #1: lerp must satisfy lerp(a,b,0)==a and lerp(a,b,1)==b. The
  // old implementation had alpha inverted (x0*alpha + x1*(1-alpha)). ===
  CHECK_NEAR(lerp(0.0f, 10.0f, 0.0f), 0.0f, 1e-6);  // == a
  CHECK_NEAR(lerp(0.0f, 10.0f, 1.0f), 10.0f, 1e-6); // == b
  CHECK_NEAR(lerp(0.0f, 10.0f, 0.3f), 3.0f, 1e-6);
  CHECK_NEAR(lerp(2.0f, 6.0f, 0.25f), 3.0f, 1e-6);

  // vector overload, same invariant
  {
    Vec2f a(0, 0), b(10, 20);
    Vec2f at0 = lerp(a, b, 0.0f);
    CHECK(at0 == a);
    Vec2f at1 = lerp(a, b, 1.0f);
    CHECK(at1 == b);
    Vec2f mid = lerp(a, b, 0.5f);
    CHECK_NEAR(mid.x, 5.0f, 1e-5);
    CHECK_NEAR(mid.y, 10.0f, 1e-5);
  }

  // polar <-> cartesian round trip
  {
    Vec2f c(3.0f, 4.0f);
    Vec2f polar = toPolar(c);        // (r, theta)
    CHECK_NEAR(polar.x, 5.0f, 1e-5); // radius
    Vec2f back = toCartesian(polar);
    CHECK_NEAR(back.x, 3.0f, 1e-4);
    CHECK_NEAR(back.y, 4.0f, 1e-4);
  }

  return imtest::report();
}
