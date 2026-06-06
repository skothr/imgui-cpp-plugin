#pragma once

#include <cmath>

#include <imtool/common/rect.hpp>
#include <imtool/common/vector.hpp>

namespace imtool {

template<typename T>
[[nodiscard]] inline Vec2<T> toPolar(const Vec2<T> &p) {
    return Vec2<T>(std::sqrt(p.x*p.x + p.y*p.y), std::atan2(p.y, p.x));
}
template<typename T>
[[nodiscard]] inline Vec2<T> toCartesian(const Vec2<T> &p) {
    return Vec2<T>(p.x*std::cos(p.y), p.x*std::sin(p.y));
}

template<typename T>
[[nodiscard]] inline Rect<T> toPolar(const Rect<T> &r) {
    return Rect<T>(toPolar(r.p1), toPolar(r.p2));
}
template<typename T>
[[nodiscard]] inline Rect<T> toCartesian(const Rect<T> &r) {
    return Rect<T>(toCartesian(r.p1), toCartesian(r.p2));
}

// Standard linear interpolation: alpha=0 -> x0, alpha=1 -> x1.
template<typename T, int N>
[[nodiscard]] inline Vector<T, N> lerp(const Vector<T, N> &x0, const Vector<T, N> &x1, T alpha) {
    return x0 * (T{1} - alpha) + x1 * alpha;
}

template<typename T>
[[nodiscard]] inline T lerp(T x0, T x1, T alpha) {
    return x0 * (T{1} - alpha) + x1 * alpha;
}

}
