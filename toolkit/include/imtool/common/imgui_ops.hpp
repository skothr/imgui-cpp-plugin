#pragma once

#include <imgui.h>

#include <imtool/common/vector.hpp>

namespace imtool {

[[nodiscard]] inline Vec2f fromImVec(const ::ImVec2 &v) { return Vec2f(v.x, v.y); }
[[nodiscard]] inline Vec4f fromImVec(const ::ImVec4 &v) { return Vec4f(v.x, v.y, v.z, v.w); }

[[nodiscard]] inline ::ImVec2 toImVec(const Vec2f &v) { return ::ImVec2(v.x, v.y); }
[[nodiscard]] inline ::ImVec4 toImVec(const Vec4f &v) { return ::ImVec4(v.x, v.y, v.z, v.w); }

}

// Free operators bridging ImGui's primitive vector types and imtool's so mixed
// expressions compile naturally (e.g. `drawList->AddRectFilled(p, p + imtool::Vec2f(w, w), ...)`).
// For pure assignment `Vec2f a = imv`, use `auto a = imtool::fromImVec(imv)`.

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
