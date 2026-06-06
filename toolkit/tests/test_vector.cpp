#include "imtest.hpp"

#include <cmath>

#include <imtool/common/vector.hpp>

using namespace imtool;

int main() {
  IMTEST_SUITE("vector");

  // --- construction + named members ---
  Vec3f a(1.0f, 2.0f, 3.0f);
  CHECK(a.x == 1.0f && a.y == 2.0f && a.z == 3.0f);
  CHECK(a[0] == 1.0f && a[1] == 2.0f && a[2] == 3.0f);

  // --- arithmetic ---
  Vec2f p(1.0f, 2.0f), q(3.0f, 4.0f);
  CHECK((p + q) == Vec2f(4.0f, 6.0f));
  CHECK((q - p) == Vec2f(2.0f, 2.0f));
  CHECK((p * 2.0f) == Vec2f(2.0f, 4.0f));
  CHECK((2.0f * p) == Vec2f(2.0f, 4.0f));
  CHECK(p.dot(q) == 11.0f);

  // --- length / normalize ---
  Vec2f v(3.0f, 4.0f);
  CHECK_NEAR(v.length(), 5.0, 1e-5);
  CHECK_NEAR(v.normalized().length(), 1.0, 1e-5);

  // --- cross product (right-handed) ---
  Vec3f cx = cross(Vec3f(1, 0, 0), Vec3f(0, 1, 0));
  CHECK(cx == Vec3f(0, 0, 1));

  // === Finding #15 (PR review): qMult/rotate layout claimed inconsistent. ===
  // These pass on the CURRENT (unmodified) rotate()/qMult() — evidence the
  // finding is a false positive. rotate() uses the first component as the
  // quaternion scalar, which is exactly the convention qMult() implements.
  {
    const float pi = 3.14159265358979323846f;
    Vec3f r =
        rotate(Vec3f(1, 0, 0), Vec3f(0, 0, 1), pi / 2.0f); // +90 deg about z
    CHECK_NEAR(r.x, 0.0f, 1e-5);
    CHECK_NEAR(r.y, 1.0f, 1e-5);
    CHECK_NEAR(r.z, 0.0f, 1e-5);

    Vec3f same =
        rotate(Vec3f(1, 2, 3), Vec3f(0, 0, 1), 0.0f); // identity rotation
    CHECK_NEAR(same.x, 1.0f, 1e-5);
    CHECK_NEAR(same.y, 2.0f, 1e-5);
    CHECK_NEAR(same.z, 3.0f, 1e-5);

    // rotation preserves length about an arbitrary axis
    Vec3f w(1, 2, 3);
    Vec3f rot = rotate(w, normalize(Vec3f(1, 1, 1)), 0.7f);
    CHECK_NEAR(rot.length(), w.length(), 1e-4);
  }

  // === Finding #16 (PR review): generic Vector<T,N> for N>4 must be usable.
  // ===
  {
    Vector<float, 5> g;
    for (int i = 0; i < 5; i++) {
      g[i] = static_cast<float>(i + 1);
    } // (1,2,3,4,5)
    Vector<float, 5> two(2.0f);
    auto sum = g + two; // (3,4,5,6,7)
    CHECK(sum[0] == 3.0f && sum[4] == 7.0f);
    auto scaled = g * 2.0f; // (2,4,6,8,10)
    CHECK(scaled[0] == 2.0f && scaled[4] == 10.0f);
    auto diff = g - two; // (-1,0,1,2,3)
    CHECK(diff[0] == -1.0f && diff[4] == 3.0f);
    CHECK_NEAR(g.length(), std::sqrt(55.0), 1e-4);
    CHECK_NEAR(g.normalized().length(), 1.0, 1e-5);
  }

  return imtest::report();
}
