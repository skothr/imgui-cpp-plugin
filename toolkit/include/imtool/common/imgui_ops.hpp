#pragma once

#include <imgui.h>

#include <imtool/common/vector.hpp>

namespace imtool {

inline Vec2f::Vector(const ::ImVec2 &v) : x(static_cast<float>(v.x)), y(static_cast<float>(v.y)) {}
inline Vec4f::Vector(const ::ImVec4 &v) : x(static_cast<float>(v.x)), y(static_cast<float>(v.y)), z(static_cast<float>(v.z)), w(static_cast<float>(v.w)) {}

}

// Free operators bridging ImGui's primitive vector types and imtool's. Implicit
// conversion at call sites lets node-render and widget code mix freely
// (e.g. `drawList->AddRectFilled(p, p + imtool::Vec2f(w, w), ...)`).

inline imtool::Vec2f operator+(const ::ImVec2 &v0, const imtool::Vec2f &v1) { return imtool::Vec2f(v0.x + v1.x, v0.y + v1.y); }
inline imtool::Vec2f operator-(const ::ImVec2 &v0, const imtool::Vec2f &v1) { return imtool::Vec2f(v0.x - v1.x, v0.y - v1.y); }
inline imtool::Vec2f operator*(const ::ImVec2 &v0, const imtool::Vec2f &v1) { return imtool::Vec2f(v0.x * v1.x, v0.y * v1.y); }
inline imtool::Vec2f operator/(const ::ImVec2 &v0, const imtool::Vec2f &v1) { return imtool::Vec2f(v0.x / v1.x, v0.y / v1.y); }

inline imtool::Vec2f operator+(const imtool::Vec2f &v0, const ::ImVec2 &v1) { return imtool::Vec2f(v0.x + v1.x, v0.y + v1.y); }
inline imtool::Vec2f operator-(const imtool::Vec2f &v0, const ::ImVec2 &v1) { return imtool::Vec2f(v0.x - v1.x, v0.y - v1.y); }
inline imtool::Vec2f operator*(const imtool::Vec2f &v0, const ::ImVec2 &v1) { return imtool::Vec2f(v0.x * v1.x, v0.y * v1.y); }
inline imtool::Vec2f operator/(const imtool::Vec2f &v0, const ::ImVec2 &v1) { return imtool::Vec2f(v0.x / v1.x, v0.y / v1.y); }

inline imtool::Vec2f operator*(const ::ImVec2 &v0, float s)          { return imtool::Vec2f(v0.x * s, v0.y * s); }
inline imtool::Vec2f operator/(const ::ImVec2 &v0, float s)          { return imtool::Vec2f(v0.x / s, v0.y / s); }
inline imtool::Vec2f operator*(float s, const ::ImVec2 &v)           { return imtool::Vec2f(s * v.x, s * v.y); }
inline imtool::Vec2f operator/(float s, const ::ImVec2 &v)           { return imtool::Vec2f(s / v.x, s / v.y); }

inline imtool::Vec4f operator+(const ::ImVec4 &v0, const imtool::Vec4f &v1) { return imtool::Vec4f(v0.x + v1.x, v0.y + v1.y, v0.z + v1.z, v0.w + v1.w); }
inline imtool::Vec4f operator-(const ::ImVec4 &v0, const imtool::Vec4f &v1) { return imtool::Vec4f(v0.x - v1.x, v0.y - v1.y, v0.z - v1.z, v0.w - v1.w); }
inline imtool::Vec4f operator*(const ::ImVec4 &v0, const imtool::Vec4f &v1) { return imtool::Vec4f(v0.x * v1.x, v0.y * v1.y, v0.z * v1.z, v0.w * v1.w); }
inline imtool::Vec4f operator/(const ::ImVec4 &v0, const imtool::Vec4f &v1) { return imtool::Vec4f(v0.x / v1.x, v0.y / v1.y, v0.z / v1.z, v0.w / v1.w); }

inline imtool::Vec4f operator+(const imtool::Vec4f &v0, const ::ImVec4 &v1) { return imtool::Vec4f(v0.x + v1.x, v0.y + v1.y, v0.z + v1.z, v0.w + v1.w); }
inline imtool::Vec4f operator-(const imtool::Vec4f &v0, const ::ImVec4 &v1) { return imtool::Vec4f(v0.x - v1.x, v0.y - v1.y, v0.z - v1.z, v0.w - v1.w); }
inline imtool::Vec4f operator*(const imtool::Vec4f &v0, const ::ImVec4 &v1) { return imtool::Vec4f(v0.x * v1.x, v0.y * v1.y, v0.z * v1.z, v0.w * v1.w); }
inline imtool::Vec4f operator/(const imtool::Vec4f &v0, const ::ImVec4 &v1) { return imtool::Vec4f(v0.x / v1.x, v0.y / v1.y, v0.z / v1.z, v0.w / v1.w); }

inline imtool::Vec4f operator+(const ::ImVec4 &v0, const ::ImVec4 &v1) { return imtool::Vec4f(v0.x + v1.x, v0.y + v1.y, v0.z + v1.z, v0.w + v1.w); }
inline imtool::Vec4f operator-(const ::ImVec4 &v0, const ::ImVec4 &v1) { return imtool::Vec4f(v0.x - v1.x, v0.y - v1.y, v0.z - v1.z, v0.w - v1.w); }
inline imtool::Vec4f operator*(const ::ImVec4 &v0, const ::ImVec4 &v1) { return imtool::Vec4f(v0.x * v1.x, v0.y * v1.y, v0.z * v1.z, v0.w * v1.w); }
inline imtool::Vec4f operator/(const ::ImVec4 &v0, const ::ImVec4 &v1) { return imtool::Vec4f(v0.x / v1.x, v0.y / v1.y, v0.z / v1.z, v0.w / v1.w); }

inline imtool::Vec4f operator*(const ::ImVec4 &v0, float s)          { return imtool::Vec4f(v0.x * s, v0.y * s, v0.z * s, v0.w * s); }
inline imtool::Vec4f operator/(const ::ImVec4 &v0, float s)          { return imtool::Vec4f(v0.x / s, v0.y / s, v0.z / s, v0.w / s); }
inline imtool::Vec4f operator*(float s, const ::ImVec4 &v)           { return imtool::Vec4f(s * v.x, s * v.y, s * v.z, s * v.w); }
inline imtool::Vec4f operator/(float s, const ::ImVec4 &v)           { return imtool::Vec4f(s / v.x, s / v.y, s / v.z, s / v.w); }

namespace imtool {

[[nodiscard]] inline ::ImVec2 toImVec(const Vec2f &v) { return ::ImVec2(v.x, v.y); }
[[nodiscard]] inline ::ImVec4 toImVec(const Vec4f &v) { return ::ImVec4(v.x, v.y, v.z, v.w); }

}
