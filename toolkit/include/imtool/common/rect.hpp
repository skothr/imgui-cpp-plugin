#pragma once

#include <algorithm>
#include <cmath>
#include <ostream>

#include <imtool/common/vector.hpp>

namespace imtool {

template<typename T> struct Rect;

using Rect2i = Rect<int>;
using Rect2f = Rect<float>;
using Rect2d = Rect<double>;

template<typename T>
struct Rect {
    using Point = Vec2<T>;

    Point p1;
    Point p2;

    Rect() = default;
    Rect(const Point &a, const Point &b) : p1(a), p2(b) {}
    template<typename U>
    Rect(const Rect<U> &o) : p1(o.p1), p2(o.p2) {}

    ~Rect() = default;

    Rect& operator=(const Rect &o) { p1 = o.p1; p2 = o.p2; return *this; }
    template<typename U>
    Rect& operator=(const Rect<U> &o) { p1 = o.p1; p2 = o.p2; return *this; }

    [[nodiscard]] bool operator==(const Rect &o) const { return (p1 == o.p1 && p2 == o.p2); }
    [[nodiscard]] bool operator!=(const Rect &o) const { return (p1 != o.p1 || p2 != o.p2); }

    Rect& operator+=(const Vec2<T> &offset) { move(offset); return *this; }
    [[nodiscard]] Rect operator+(const Vec2<T> &offset) const { return (Rect(*this) += offset); }
    Rect& operator-=(const Vec2<T> &offset) { move(-offset); return *this; }
    [[nodiscard]] Rect operator-(const Vec2<T> &offset) const { return (Rect(*this) -= offset); }
    Rect& operator*=(const Vec2<T> &v) { scale(v); return *this; }
    [[nodiscard]] Rect operator*(const Vec2<T> &v) const { return (Rect(*this) *= v); }
    Rect& operator/=(const Vec2<T> &v) { scale(Vec2<T>(T{1}/v.x, T{1}/v.y)); return *this; }
    [[nodiscard]] Rect operator/(const Vec2<T> &v) const { return (Rect(*this) /= v); }
    Rect& operator*=(T s) { scale(Vec2<T>(s, s)); return *this; }
    [[nodiscard]] Rect operator*(T s) const { return (Rect(*this) *= s); }
    Rect& operator/=(T s) { scale(Vec2<T>(T{1}/s, T{1}/s)); return *this; }
    [[nodiscard]] Rect operator/(T s) const { return (Rect(*this) /= s); }

    [[nodiscard]] bool valid() const { return (p1.x <= p2.x && p1.y <= p2.y); }
    void fix() {
        if(p1.x > p2.x) std::swap(p1.x, p2.x);
        if(p1.y > p2.y) std::swap(p1.y, p2.y);
    }
    [[nodiscard]] Rect fixed() const {
        return Rect(Point(std::min(p1.x, p2.x), std::min(p1.y, p2.y)),
                    Point(std::max(p1.x, p2.x), std::max(p1.y, p2.y)));
    }

    [[nodiscard]] Vec2<T> size() const  { return (p2 - p1); }
    [[nodiscard]] const Point& pos() const { return p1; }
    [[nodiscard]] Point center() const  { return (p2 + p1) / T{2}; }
    [[nodiscard]] T posX()  const { return p1.x; }
    [[nodiscard]] T posY()  const { return p1.y; }
    [[nodiscard]] T sizeX() const { return (p2.x - p1.x); }
    [[nodiscard]] T sizeY() const { return p2.y - p1.y; }
    [[nodiscard]] T aspect() const { return sizeX() / sizeY(); }

    bool setPos  (const Point &p)   { const bool changed = (p  != p1);      p2   = (p  + size());  p1   = p;  return changed; }
    bool setPosX (T px)             { const bool changed = (px != posX());  p2.x = (px + sizeX()); p1.x = px; return changed; }
    bool setPosY (T py)             { const bool changed = (py != posY());  p2.y = (py + sizeY()); p1.y = py; return changed; }
    bool setSize (const Vec2<T> &s) { const bool changed = ((p1+s) != p2);  p2   = (p1 + s);    return changed; }
    bool setSizeX(T sx)             { const bool changed = (sx != sizeX()); p2.x = (p1.x + sx); return changed; }
    bool setSizeY(T sy)             { const bool changed = (sy != sizeY()); p2.y = (p1.y + sy); return changed; }

    void move(const Vec2<T> &dp) { p1 += dp; p2 += dp; }
    void moveX(const T &dpx) { p1.x += dpx; p2.x += dpx; }
    void moveY(const T &dpy) { p1.y += dpy; p2.y += dpy; }
    [[nodiscard]] Rect moved(const Vec2<T> &dp) const { Rect r(*this); r.move(dp); return r; }

    void scaleX(T dsx) { const T offset = (dsx-T{1})*sizeX()/T{2}; p1.x -= offset; p2.x += offset; }
    void scaleY(T dsy) { const T offset = (dsy-T{1})*sizeY()/T{2}; p1.y -= offset; p2.y += offset; }
    void scale(const Vec2<T> &ds) {
        const Vec2<T> offset((ds.x-T{1})*sizeX()/T{2}, (ds.y-T{1})*sizeY()/T{2});
        p1 -= offset; p2 += offset;
    }
    [[nodiscard]] Rect scaled(const Vec2<T> &ds) const { Rect r(*this); r.scale(ds); return r; }

    void centerAt(const Point &c)            { const Vec2<T> half = size()/T{2}; p1 = (c-half); p2 = (c+half); }
    [[nodiscard]] Rect centeredAt(const Point &c) const { const Vec2<T> half = size()/T{2}; return Rect(c-half, c+half); }

    [[nodiscard]] bool contains(const Point &p) const {
        return ((p.x >= p1.x && p.x <= p2.x) && (p.y >= p1.y && p.y <= p2.y));
    }
    [[nodiscard]] bool contains(const Rect &o) const {
        return (o.p1.x >= p1.x && o.p2.x <= p2.x && o.p1.y >= p1.y && o.p2.y <= p2.y);
    }
    [[nodiscard]] bool overlaps(const Rect &o) const {
        return (p1.x <= o.p2.x && o.p1.x <= p2.x && p1.y <= o.p2.y && o.p1.y <= p2.y);
    }
    // Synonym for overlaps() — a true AABB test (both axes must overlap). The
    // previous per-axis-edge OR returned true for axis-aligned-but-disjoint rects.
    [[nodiscard]] bool intersects(const Rect &o) const { return overlaps(o); }
    [[nodiscard]] Rect intersection(const Rect &o) const {
        return Rect(Point(std::max(p1.x, o.p1.x), std::max(p1.y, o.p1.y)),
                    Point(std::min(p2.x, o.p2.x), std::min(p2.y, o.p2.y)));
    }

    Rect& combine(const Point &p) {
        p1 = Point(std::min(p1.x, p.x), std::min(p1.y, p.y));
        p2 = Point(std::max(p2.x, p.x), std::max(p2.y, p.y));
        return *this;
    }
    [[nodiscard]] Rect combined(const Point &p) const {
        return Rect(Point(std::min(p1.x, p.x), std::min(p1.y, p.y)),
                    Point(std::max(p2.x, p.x), std::max(p2.y, p.y)));
    }
    Rect& combine(const Rect &o) {
        p1 = Point(std::min(p1.x, o.p1.x), std::min(p1.y, o.p1.y));
        p2 = Point(std::max(p2.x, o.p2.x), std::max(p2.y, o.p2.y));
        return *this;
    }
    [[nodiscard]] Rect combined(const Rect &o) const {
        return Rect(Point(std::min(p1.x, o.p1.x), std::min(p1.y, o.p1.y)),
                    Point(std::max(p2.x, o.p2.x), std::max(p2.y, o.p2.y)));
    }

    [[nodiscard]] Rect expanded(T l) const { return Rect(p1 - Point(l, l), p2 + Point(l, l)); }
    Rect& expand(T l) { return (*this = expanded(l)); }
    [[nodiscard]] Rect expanded(const Vec2<T> &v) const { return Rect(p1 - v, p2 + v); }
    Rect& expand(const Vec2<T> &v) { return (*this = expanded(v)); }

    [[nodiscard]] Point clampPoint(Point p) const {
        if(p.x < p1.x) p.x = p1.x; else if(p.x > p2.x) p.x = p2.x;
        if(p.y < p1.y) p.y = p1.y; else if(p.y > p2.y) p.y = p2.y;
        return p;
    }
};

template<typename T>
inline std::ostream& operator<<(std::ostream &os, const Rect<T> &r) {
    os << "RECT[" << r.p1 << " | " << r.p2 << "]";
    return os;
}

template<typename T> [[nodiscard]] inline bool isnan(const Rect<T> &r) { return (isnan(r.p1) || isnan(r.p2)); }
template<typename T> [[nodiscard]] inline bool isinf(const Rect<T> &r) { return (isinf(r.p1) || isinf(r.p2)); }

}
