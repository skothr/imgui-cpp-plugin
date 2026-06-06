#include "imtest.hpp"

#include <imtool/common/colors.hpp>
#include <imtool/common/paths.hpp>
#include <imtool/common/range.hpp>
#include <imtool/common/timing.hpp>
#include <imtool/common/type_registry.hpp>

using namespace imtool;

// A deliberately non-default-constructible type. getTypeNumArgs<T>() must not
// require T be default-constructible (Finding #5: it used `T t;` to get
// typeid). This whole translation unit fails to compile against the old header.
struct NoDefault {
  int v;
  explicit NoDefault(int x) : v(x) {}
  NoDefault() = delete;
};

int main() {
  IMTEST_SUITE("primitives");

  // --- range ---
  Rangef r(0.0f, 1.0f);
  CHECK_NEAR(r.clip(5.0f), 1.0f, 1e-6);
  CHECK_NEAR(r.clip(-3.0f), 0.0f, 1e-6);
  CHECK_NEAR(r.clip(0.5f), 0.5f, 1e-6);
  CHECK(r.contains(0.5f) && !r.contains(2.0f));
  CHECK_NEAR(r.span(), 1.0f, 1e-6);
  r.fit(2.0f);
  CHECK_NEAR(r.upper, 2.0f, 1e-6);
  CHECK_NEAR(r.lower, 0.0f, 1e-6);

  // --- colors ---
  CHECK(color("red") == Vec4f(1, 0, 0, 1));
  CHECK(color("blue") == Vec4f(0, 0, 1, 1));
  {
    Vec4f g = color("grey50"); // parsed numeric grey: 50/255
    CHECK_NEAR(g.x, 50.0f / 255.0f, 1e-5);
    CHECK_NEAR(g.x, g.y, 1e-6);
    CHECK_NEAR(g.z, g.x, 1e-6);
  }
  CHECK(color("not-a-real-color") == Vec4f(0, 0, 0, 1)); // fallback = black

  // --- type registry ---
  CHECK(getTypeName<Vec3f>() == "Vec3f");
  CHECK(getTypeName<float>() == "float");
  CHECK(getTypeNumArgs<Vec3f>() == 3);
  CHECK(getTypeNumArgs<Vec4f>() == 4);
  CHECK(getTypeNumArgs<float>() == 1);
  CHECK(getTypeNumArgs<bool>() == 0);
  CHECK(getTypeIndex<Vec3f>() == getTypeIndex<Vec3f>());
  CHECK(getTypeIndex<Vec3f>() != getTypeIndex<Vec4f>());
  // Finding #5: works for a non-default-constructible, unregistered type.
  CHECK(getTypeNumArgs<NoDefault>() == 0);

  // --- timing ---
  {
    const auto t1 = getTimestamp();
    const auto t2 = getTimestamp();
    CHECK(t1 > 0);
    CHECK(t2 >= t1);
  }

  // --- paths ---
  {
    CHECK(!getHomeDir().empty());
    const auto a = getLocalStorageDir();
    const auto b = getLocalStorageDir(); // cached: must be stable
    CHECK(a == b);
    CHECK(!a.empty());
  }

  return imtest::report();
}
