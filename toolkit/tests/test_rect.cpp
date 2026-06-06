#include "imtest.hpp"

#include <imtool/common/rect.hpp>

using namespace imtool;

int main() {
  IMTEST_SUITE("rect");

  Rect2f r(Vec2f(0, 0), Vec2f(10, 10));
  CHECK(r.size() == Vec2f(10, 10));
  CHECK(r.center() == Vec2f(5, 5));
  CHECK_NEAR(r.sizeX(), 10.0f, 1e-5);
  CHECK_NEAR(r.aspect(), 1.0f, 1e-5);

  CHECK(r.contains(Vec2f(5, 5)));
  CHECK(!r.contains(Vec2f(11, 5)));
  CHECK(r.contains(Rect2f(Vec2f(2, 2), Vec2f(5, 5))));

  // === Finding #14: intersects() must be a real AABB-overlap test, not a
  // per-axis-edge OR. Rects overlapping in x but disjoint in y do NOT overlap;
  // the old intersects() returned true for this case. ===
  {
    Rect2f a(Vec2f(0, 0), Vec2f(10, 10));
    Rect2f xOverlapYDisjoint(Vec2f(5, 20), Vec2f(15, 30));
    CHECK(!a.intersects(xOverlapYDisjoint)); // must agree with overlaps()
    CHECK(!a.overlaps(xOverlapYDisjoint));

    Rect2f touching(Vec2f(10, 0), Vec2f(20, 10)); // shares an edge
    CHECK(a.intersects(touching));
    CHECK(a.overlaps(touching));

    Rect2f inside(Vec2f(2, 2), Vec2f(5, 5)); // fully contained
    CHECK(a.intersects(inside));
    CHECK(a.overlaps(inside));

    Rect2f apart(Vec2f(100, 100), Vec2f(110, 110)); // far away
    CHECK(!a.intersects(apart));
  }

  // intersection geometry
  {
    Rect2f a(Vec2f(0, 0), Vec2f(10, 10));
    Rect2f b(Vec2f(5, 5), Vec2f(15, 15));
    Rect2f x = a.intersection(b);
    CHECK(x.p1 == Vec2f(5, 5));
    CHECK(x.p2 == Vec2f(10, 10));
  }

  // combine grows to include a point / rect
  {
    Rect2f a(Vec2f(0, 0), Vec2f(10, 10));
    Rect2f c = a.combined(Vec2f(20, -5));
    CHECK(c.p1 == Vec2f(0, -5));
    CHECK(c.p2 == Vec2f(20, 10));
  }

  // fix() / fixed() normalize inverted corners
  {
    Rect2f bad(Vec2f(10, 10), Vec2f(0, 0));
    CHECK(!bad.valid());
    CHECK(bad.fixed().valid());
    bad.fix();
    CHECK(bad.valid());
    CHECK(bad.p1 == Vec2f(0, 0) && bad.p2 == Vec2f(10, 10));
  }

  // clampPoint
  {
    Rect2f a(Vec2f(0, 0), Vec2f(10, 10));
    CHECK(a.clampPoint(Vec2f(-5, 5)) == Vec2f(0, 5));
    CHECK(a.clampPoint(Vec2f(5, 99)) == Vec2f(5, 10));
    CHECK(a.clampPoint(Vec2f(5, 5)) == Vec2f(5, 5));
  }

  return imtest::report();
}
