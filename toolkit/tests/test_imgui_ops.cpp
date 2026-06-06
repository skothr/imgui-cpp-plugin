// === Findings #10 / #11: imgui_ops.hpp must coexist with ImGui's own math
// operators. Defining IMGUI_DEFINE_MATH_OPERATORS *and* including imgui_ops.hpp
// must compile — the old header redefined ImVec2*float / ImVec4 op ImVec4 with
// a different return type, which is ill-formed (can't overload on return type).
// If the #ifndef guard regresses, this translation unit fails to compile. ===
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

#include "imtest.hpp"

#include <imtool/common/imgui_ops.hpp>

using namespace imtool;

int main() {
  IMTEST_SUITE("imgui_ops");

  // ImGui's own ImVec2 * float (returns ImVec2) — provided by ImGui because the
  // macro is set; imtool's same-signature overload is correctly guarded out.
  ImVec2 a(2.0f, 4.0f);
  ImVec2 b = a * 2.0f;
  CHECK_NEAR(b.x, 4.0f, 1e-5);
  CHECK_NEAR(b.y, 8.0f, 1e-5);

  // imtool mixed interop (ImVec2 <op> Vec2f -> Vec2f) — never conflicts with
  // ImGui.
  Vec2f m = a + Vec2f(1.0f, 1.0f);
  CHECK_NEAR(m.x, 3.0f, 1e-5);
  CHECK_NEAR(m.y, 5.0f, 1e-5);

  Vec2f m2 = Vec2f(10.0f, 20.0f) + ImVec2(1.0f, 2.0f);
  CHECK_NEAR(m2.x, 11.0f, 1e-5);
  CHECK_NEAR(m2.y, 22.0f, 1e-5);

  // conversion helpers
  Vec2f fv = fromImVec(ImVec2(5.0f, 6.0f));
  CHECK(fv == Vec2f(5.0f, 6.0f));
  ImVec2 iv = toImVec(Vec2f(7.0f, 8.0f));
  CHECK_NEAR(iv.x, 7.0f, 1e-5);
  CHECK_NEAR(iv.y, 8.0f, 1e-5);

  // ImVec4 mixed interop
  Vec4f v4 = ImVec4(1, 2, 3, 4) + Vec4f(10, 20, 30, 40);
  CHECK_NEAR(v4.x, 11.0f, 1e-5);
  CHECK_NEAR(v4.w, 44.0f, 1e-5);

  return imtest::report();
}
