#pragma once

#include <cmath>
#include <type_traits>

#include <imgui.h>

namespace imtool {

template<typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template<Arithmetic T>
struct Vec2 {
    T x{};
    T y{};

    constexpr Vec2() noexcept = default;
    constexpr Vec2(T x_, T y_) noexcept : x(x_), y(y_) {}
    explicit constexpr Vec2(T v) noexcept : x(v), y(v) {}

    template<Arithmetic U>
    explicit constexpr Vec2(Vec2<U> o) noexcept
        : x(static_cast<T>(o.x)), y(static_cast<T>(o.y)) {}

    explicit Vec2(ImVec2 v) noexcept
        : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)) {}
    [[nodiscard]] explicit operator ImVec2() const noexcept
        requires std::is_floating_point_v<T> {
        return ImVec2{static_cast<float>(x), static_cast<float>(y)};
    }

    [[nodiscard]] constexpr T length_sq() const noexcept { return x*x + y*y; }
    [[nodiscard]] T length() const noexcept { return std::sqrt(static_cast<double>(length_sq())); }
    [[nodiscard]] constexpr T dot(Vec2 o) const noexcept { return x*o.x + y*o.y; }
    [[nodiscard]] Vec2 normalized() const noexcept {
        const auto len = length();
        return (len > T{0}) ? Vec2{static_cast<T>(x/len), static_cast<T>(y/len)} : Vec2{};
    }

    [[nodiscard]] constexpr bool operator==(Vec2 const&) const noexcept = default;

    constexpr Vec2& operator+=(Vec2 o) noexcept { x += o.x; y += o.y; return *this; }
    constexpr Vec2& operator-=(Vec2 o) noexcept { x -= o.x; y -= o.y; return *this; }
    constexpr Vec2& operator*=(T s) noexcept { x *= s; y *= s; return *this; }
    constexpr Vec2& operator/=(T s) noexcept { x /= s; y /= s; return *this; }
};

template<Arithmetic T> [[nodiscard]] constexpr Vec2<T> operator+(Vec2<T> a, Vec2<T> b) noexcept { return a += b; }
template<Arithmetic T> [[nodiscard]] constexpr Vec2<T> operator-(Vec2<T> a, Vec2<T> b) noexcept { return a -= b; }
template<Arithmetic T> [[nodiscard]] constexpr Vec2<T> operator*(Vec2<T> v, T s) noexcept { return v *= s; }
template<Arithmetic T> [[nodiscard]] constexpr Vec2<T> operator*(T s, Vec2<T> v) noexcept { return v *= s; }
template<Arithmetic T> [[nodiscard]] constexpr Vec2<T> operator/(Vec2<T> v, T s) noexcept { return v /= s; }
template<Arithmetic T> [[nodiscard]] constexpr Vec2<T> operator-(Vec2<T> v) noexcept { return {-v.x, -v.y}; }

using Vec2f = Vec2<float>;
using Vec2i = Vec2<int>;
using Vec2d = Vec2<double>;

template<Arithmetic T>
struct Rect2 {
    Vec2<T> lo{};
    Vec2<T> hi{};

    constexpr Rect2() noexcept = default;
    constexpr Rect2(Vec2<T> lo_, Vec2<T> hi_) noexcept : lo(lo_), hi(hi_) {}

    [[nodiscard]] static constexpr Rect2 from_pos_size(Vec2<T> pos, Vec2<T> size) noexcept {
        return {pos, {pos.x + size.x, pos.y + size.y}};
    }

    [[nodiscard]] constexpr Vec2<T> size() const noexcept { return hi - lo; }
    [[nodiscard]] constexpr Vec2<T> center() const noexcept { return (lo + hi) / T{2}; }
    [[nodiscard]] constexpr T width() const noexcept { return hi.x - lo.x; }
    [[nodiscard]] constexpr T height() const noexcept { return hi.y - lo.y; }

    [[nodiscard]] constexpr bool contains(Vec2<T> p) const noexcept {
        return p.x >= lo.x && p.x <= hi.x && p.y >= lo.y && p.y <= hi.y;
    }
    [[nodiscard]] constexpr bool intersects(Rect2 o) const noexcept {
        return lo.x <= o.hi.x && hi.x >= o.lo.x
            && lo.y <= o.hi.y && hi.y >= o.lo.y;
    }

    [[nodiscard]] constexpr bool operator==(Rect2 const&) const noexcept = default;
};

using Rect2f = Rect2<float>;
using Rect2i = Rect2<int>;
using Rect2d = Rect2<double>;

}
