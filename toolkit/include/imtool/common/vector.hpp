#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <istream>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>

#include <nlohmann/json.hpp>

namespace imtool {

template<typename T> concept Arithmetic = std::is_arithmetic_v<T>;

template<typename T, int N> struct Vector;

template<typename T> using Vec1 = Vector<T, 1>;
template<typename T> using Vec2 = Vector<T, 2>;
template<typename T> using Vec3 = Vector<T, 3>;
template<typename T> using Vec4 = Vector<T, 4>;
template<int N>      using Vecf = Vector<float,  N>;
template<int N>      using Vecd = Vector<double, N>;

using Vec1i  = Vector<int, 1>;
using Vec2i  = Vector<int, 2>;
using Vec3i  = Vector<int, 3>;
using Vec4i  = Vector<int, 4>;
using Vec1ui = Vector<unsigned int, 1>;
using Vec2ui = Vector<unsigned int, 2>;
using Vec3ui = Vector<unsigned int, 3>;
using Vec4ui = Vector<unsigned int, 4>;
using Vec1f  = Vector<float, 1>;
using Vec2f  = Vector<float, 2>;
using Vec3f  = Vector<float, 3>;
using Vec4f  = Vector<float, 4>;
using Vec1d  = Vector<double, 1>;
using Vec2d  = Vector<double, 2>;
using Vec3d  = Vector<double, 3>;
using Vec4d  = Vector<double, 4>;
using Vec1l  = Vector<long double, 1>;
using Vec2l  = Vector<long double, 2>;
using Vec3l  = Vector<long double, 3>;
using Vec4l  = Vector<long double, 4>;

template<typename T, int N_>
struct Vector {
    using type = T;
    static constexpr int N = N_;
    std::array<T, N> data;

    [[nodiscard]] static Vector zeros() { return Vector(); }
    [[nodiscard]] static Vector ones()  { Vector r; for(int i = 0; i < N; i++) { r[i] = T{1}; } return r; }

    Vector() : data{{0}} {}
    Vector(const Vector &other) : data(other.data) {}
    Vector(const std::array<T, N> &d) : data(d) {}
    Vector(const T *d) { for(int i = 0; i < N; i++) { data[i] = d[i]; } }
    Vector(T val) { for(int i = 0; i < N; i++) { data[i] = val; } }
    Vector(const std::string &str) { fromString(str); }

    template<typename U>
    Vector(const Vector<U, N> &other) { for(int i = 0; i < N; i++) { data[i] = static_cast<T>(other.data[i]); } }

    [[nodiscard]] std::string toString() const { std::ostringstream ss; ss << (*this); return ss.str(); }
    void fromString(const std::string &str) { std::istringstream ss(str); ss >> (*this); }

    [[nodiscard]] T& operator[](int dim) { return data[dim]; }
    [[nodiscard]] const T& operator[](int dim) const { return data[dim]; }

    Vector& operator=(T scalar) { for(int i = 0; i < N; i++) { data[i] = scalar; } return *this; }
    Vector& operator=(const Vector &other) { data = other.data; return *this; }

    [[nodiscard]] bool operator==(const Vector &other) const { return data == other.data; }
    [[nodiscard]] bool operator!=(const Vector &other) const { return data != other.data; }

    // AND-semantic per-component compares: true iff ALL components satisfy.
    [[nodiscard]] bool operator> (const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] <= o.data[i]) return false; return true; }
    [[nodiscard]] bool operator< (const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] >= o.data[i]) return false; return true; }
    [[nodiscard]] bool operator>=(const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] <  o.data[i]) return false; return true; }
    [[nodiscard]] bool operator<=(const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] >  o.data[i]) return false; return true; }
    [[nodiscard]] bool operator> (T s) const { for(int i = 0; i < N; i++) if(data[i] <= s) return false; return true; }
    [[nodiscard]] bool operator< (T s) const { for(int i = 0; i < N; i++) if(data[i] >= s) return false; return true; }
    [[nodiscard]] bool operator>=(T s) const { for(int i = 0; i < N; i++) if(data[i] <  s) return false; return true; }
    [[nodiscard]] bool operator<=(T s) const { for(int i = 0; i < N; i++) if(data[i] >  s) return false; return true; }

    Vector& operator+=(const Vector &o) { for(int i = 0; i < N; i++) data[i] += o.data[i]; return *this; }
    Vector& operator-=(const Vector &o) { for(int i = 0; i < N; i++) data[i] -= o.data[i]; return *this; }
    // Full arithmetic suite so the generic Vector<T,N> (N not 1/2/3/4) is usable:
    // without these, binary +,-,*,/ and normalize() exist only in the named-member
    // specializations and any Vector<T,5+> expression fails to compile.
    Vector& operator*=(const Vector &o) { for(int i = 0; i < N; i++) data[i] *= o.data[i]; return *this; }
    Vector& operator/=(const Vector &o) { for(int i = 0; i < N; i++) data[i] /= o.data[i]; return *this; }
    Vector& operator*=(T s) { for(int i = 0; i < N; i++) data[i] *= s; return *this; }
    Vector& operator/=(T s) { for(int i = 0; i < N; i++) data[i] /= s; return *this; }
    [[nodiscard]] Vector operator+(const Vector &o) const { Vector r(*this); r += o; return r; }
    [[nodiscard]] Vector operator-(const Vector &o) const { Vector r(*this); r -= o; return r; }
    [[nodiscard]] Vector operator*(const Vector &o) const { Vector r(*this); r *= o; return r; }
    [[nodiscard]] Vector operator/(const Vector &o) const { Vector r(*this); r /= o; return r; }
    [[nodiscard]] Vector operator*(T s) const { Vector r(*this); r *= s; return r; }
    [[nodiscard]] Vector operator/(T s) const { Vector r(*this); r /= s; return r; }
    Vector& operator%=(const T &s) {
        for(int i = 0; i < N; i++) {
            if constexpr(std::is_integral_v<T>) { data[i] %= s; }
            else { data[i] = std::fmod(data[i], s); }
        }
        return *this;
    }
    Vector& operator%=(const Vector &o) {
        for(int i = 0; i < N; i++) {
            if constexpr(std::is_integral_v<T>) { data[i] %= o[i]; }
            else { data[i] = std::fmod(data[i], o[i]); }
        }
        return *this;
    }
    [[nodiscard]] Vector operator%(const T &s) const { Vector r(*this); return (r %= s); }
    [[nodiscard]] Vector operator%(const Vector &o) const { Vector r(*this); return (r %= o); }

    [[nodiscard]] T min() const { T v = std::numeric_limits<T>::max(); for(auto &d : data) v = std::min(v, d); return v; }
    [[nodiscard]] T max() const { T v = std::numeric_limits<T>::lowest(); for(auto &d : data) v = std::max(v, d); return v; }
    void ceil()  { for(auto &d : data) d = std::ceil(d); }
    void floor() { for(auto &d : data) d = std::floor(d); }
    [[nodiscard]] Vector getCeil() const  { Vector r(*this); r.ceil(); return r; }
    [[nodiscard]] Vector getFloor() const { Vector r(*this); r.floor(); return r; }

    [[nodiscard]] T length2() const { T s = T{}; for(auto d : data) s += d*d; return s; }
    [[nodiscard]] T length()  const { return std::sqrt(length2()); }

    void normalize() { (*this) /= length(); }
    [[nodiscard]] Vector normalized() const { return Vector(*this) / length(); }

    template<typename U>
    [[nodiscard]] T dot(const Vector<U, N> &o) const {
        T total = T{};
        for(int i = 0; i < N; i++) { total += data[i] * o.data[i]; }
        return total;
    }
};

template<typename T, int N>
inline std::ostream& operator<<(std::ostream &os, const Vector<T, N> &v) {
    os << "<";
    for(int i = 0; i < N; i++) { os << v.data[i] << ((i < N-1) ? ", " : ""); }
    os << ">";
    return os;
}
template<typename T, int N>
inline std::istream& operator>>(std::istream &is, Vector<T, N> &v) {
    is.ignore(1,'<');
    for(int i = 0; i < N; i++) { is >> v.data[i]; is.ignore(1,','); }
    is.ignore(1,'>');
    return is;
}

template<typename T>
struct Vector<T, 1> {
    using type = T;
    static constexpr int N = 1;
    union { struct { T x; }; std::array<T, N> data; };

    [[nodiscard]] static Vector zeros() { return Vector(T{0}); }
    [[nodiscard]] static Vector ones()  { return Vector(T{1}); }

    Vector() : x(T{0}) {}
    Vector(T x_) : x(x_) {}
    Vector(const Vector &o) : x(o.x) {}
    Vector(const std::array<T, N> &d) : x(d[0]) {}
    Vector(const T *d) : x(d[0]) {}
    Vector(const std::string &str) { fromString(str); }
    template<typename U>
    Vector(const Vector<U, N> &o) { for(int i = 0; i < N; i++) data[i] = static_cast<T>(o.data[i]); }

    [[nodiscard]] T& operator[](int dim) { return data[dim]; }
    [[nodiscard]] const T& operator[](int dim) const { return data[dim]; }

    [[nodiscard]] std::string toString() const { std::ostringstream ss; ss << (*this); return ss.str(); }
    void fromString(const std::string &str) { std::istringstream ss(str); ss >> (*this); }

    [[nodiscard]] bool operator==(const Vector &o) const { return data == o.data; }
    [[nodiscard]] bool operator!=(const Vector &o) const { return data != o.data; }

    [[nodiscard]] bool operator> (const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] <= o.data[i]) return false; return true; }
    [[nodiscard]] bool operator< (const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] >= o.data[i]) return false; return true; }
    [[nodiscard]] bool operator>=(const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] <  o.data[i]) return false; return true; }
    [[nodiscard]] bool operator<=(const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] >  o.data[i]) return false; return true; }
    [[nodiscard]] bool operator> (T s) const { for(int i = 0; i < N; i++) if(data[i] <= s) return false; return true; }
    [[nodiscard]] bool operator< (T s) const { for(int i = 0; i < N; i++) if(data[i] >= s) return false; return true; }
    [[nodiscard]] bool operator>=(T s) const { for(int i = 0; i < N; i++) if(data[i] <  s) return false; return true; }
    [[nodiscard]] bool operator<=(T s) const { for(int i = 0; i < N; i++) if(data[i] >  s) return false; return true; }

    Vector& operator+=(const Vector &o) { for(int i = 0; i < N; i++) data[i] += o.data[i]; return *this; }
    [[nodiscard]] Vector operator+(const Vector &o) const { Vector r(*this); r += o; return r; }
    Vector& operator-=(const Vector &o) { for(int i = 0; i < N; i++) data[i] -= o.data[i]; return *this; }
    [[nodiscard]] Vector operator-(const Vector &o) const { Vector r(*this); r -= o; return r; }
    Vector& operator*=(const Vector &o) { for(int i = 0; i < N; i++) data[i] *= o.data[i]; return *this; }
    [[nodiscard]] Vector operator*(const Vector &o) const { Vector r(*this); r *= o; return r; }
    Vector& operator/=(const Vector &o) { for(int i = 0; i < N; i++) data[i] /= o.data[i]; return *this; }
    [[nodiscard]] Vector operator/(const Vector &o) const { Vector r(*this); r /= o; return r; }

    Vector& operator*=(T s) { for(int i = 0; i < N; i++) data[i] *= s; return *this; }
    [[nodiscard]] Vector operator*(T s) const { Vector r(*this); r *= s; return r; }
    Vector& operator/=(T s) { for(int i = 0; i < N; i++) data[i] /= s; return *this; }
    [[nodiscard]] Vector operator/(T s) const { Vector r(*this); r /= s; return r; }

    Vector& operator%=(const T &s) {
        for(int i = 0; i < N; i++) {
            if constexpr(std::is_integral_v<T>) { data[i] %= s; }
            else { data[i] = std::fmod(data[i], s); }
        }
        return *this;
    }
    Vector& operator%=(const Vector &o) {
        for(int i = 0; i < N; i++) {
            if constexpr(std::is_integral_v<T>) { data[i] %= o[i]; }
            else { data[i] = std::fmod(data[i], o[i]); }
        }
        return *this;
    }
    [[nodiscard]] Vector operator%(const T &s) const { Vector r(*this); return (r %= s); }
    [[nodiscard]] Vector operator%(const Vector &o) const { Vector r(*this); return (r %= o); }

    [[nodiscard]] T min() const { T v = std::numeric_limits<T>::max(); for(auto &d : data) v = std::min(v, d); return v; }
    [[nodiscard]] T max() const { T v = std::numeric_limits<T>::lowest(); for(auto &d : data) v = std::max(v, d); return v; }
    void ceil()  { for(auto &d : data) d = std::ceil(d); }
    void floor() { for(auto &d : data) d = std::floor(d); }
    [[nodiscard]] Vector getCeil() const  { Vector r(*this); r.ceil(); return r; }
    [[nodiscard]] Vector getFloor() const { Vector r(*this); r.floor(); return r; }

    [[nodiscard]] T length2() const { T s = T{}; for(auto d : data) s += d*d; return s; }
    [[nodiscard]] T length()  const { return std::sqrt(length2()); }
    void normalize() { (*this) /= length(); }
    [[nodiscard]] Vector normalized() const { return Vector(*this) / length(); }
    template<typename U>
    [[nodiscard]] T dot(const Vector<U, N> &o) const { T t = T{}; for(int i = 0; i < N; i++) t += data[i] * o.data[i]; return t; }
};

template<typename T>
struct Vector<T, 2> {
    using type = T;
    static constexpr int N = 2;
    union { struct { T x, y; }; std::array<T, N> data; };

    [[nodiscard]] static Vector zeros() { return Vector(T{0}, T{0}); }
    [[nodiscard]] static Vector ones()  { return Vector(T{1}, T{1}); }

    Vector() : x(T{0}), y(T{0}) {}
    Vector(T x_, T y_) : x(x_), y(y_) {}
    Vector(const Vector &o) : x(o.x), y(o.y) {}
    Vector(const std::array<T, N> &d) : x(d[0]), y(d[1]) {}
    Vector(const T *d) : x(d[0]), y(d[1]) {}
    Vector(T val) : x(val), y(val) {}
    Vector(const std::string &str) { fromString(str); }
    template<typename U>
    Vector(const Vector<U, N> &o) { for(int i = 0; i < N; i++) data[i] = static_cast<T>(o.data[i]); }

    [[nodiscard]] T& operator[](int dim) { return data[dim]; }
    [[nodiscard]] const T& operator[](int dim) const { return data[dim]; }

    [[nodiscard]] std::string toString() const { std::ostringstream ss; ss << (*this); return ss.str(); }
    void fromString(const std::string &str) { std::istringstream ss(str); ss >> (*this); }

    [[nodiscard]] bool operator==(const Vector &o) const { return data == o.data; }
    [[nodiscard]] bool operator!=(const Vector &o) const { return data != o.data; }

    [[nodiscard]] bool operator> (const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] <= o.data[i]) return false; return true; }
    [[nodiscard]] bool operator< (const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] >= o.data[i]) return false; return true; }
    [[nodiscard]] bool operator>=(const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] <  o.data[i]) return false; return true; }
    [[nodiscard]] bool operator<=(const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] >  o.data[i]) return false; return true; }
    [[nodiscard]] bool operator> (T s) const { for(int i = 0; i < N; i++) if(data[i] <= s) return false; return true; }
    [[nodiscard]] bool operator< (T s) const { for(int i = 0; i < N; i++) if(data[i] >= s) return false; return true; }
    [[nodiscard]] bool operator>=(T s) const { for(int i = 0; i < N; i++) if(data[i] <  s) return false; return true; }
    [[nodiscard]] bool operator<=(T s) const { for(int i = 0; i < N; i++) if(data[i] >  s) return false; return true; }

    Vector& operator+=(const Vector &o) { for(int i = 0; i < N; i++) data[i] += o.data[i]; return *this; }
    [[nodiscard]] Vector operator+(const Vector &o) const { Vector r(*this); r += o; return r; }
    Vector& operator-=(const Vector &o) { for(int i = 0; i < N; i++) data[i] -= o.data[i]; return *this; }
    [[nodiscard]] Vector operator-(const Vector &o) const { Vector r(*this); r -= o; return r; }
    Vector& operator*=(const Vector &o) { for(int i = 0; i < N; i++) data[i] *= o.data[i]; return *this; }
    [[nodiscard]] Vector operator*(const Vector &o) const { Vector r(*this); r *= o; return r; }
    Vector& operator/=(const Vector &o) { for(int i = 0; i < N; i++) data[i] /= o.data[i]; return *this; }
    [[nodiscard]] Vector operator/(const Vector &o) const { Vector r(*this); r /= o; return r; }

    Vector& operator*=(T s) { for(int i = 0; i < N; i++) data[i] *= s; return *this; }
    [[nodiscard]] Vector operator*(T s) const { Vector r(*this); r *= s; return r; }
    Vector& operator/=(T s) { for(int i = 0; i < N; i++) data[i] /= s; return *this; }
    [[nodiscard]] Vector operator/(T s) const { Vector r(*this); r /= s; return r; }

    Vector& operator%=(const T &s) {
        for(int i = 0; i < N; i++) {
            if constexpr(std::is_integral_v<T>) { data[i] %= s; }
            else { data[i] = std::fmod(data[i], s); }
        }
        return *this;
    }
    Vector& operator%=(const Vector &o) {
        for(int i = 0; i < N; i++) {
            if constexpr(std::is_integral_v<T>) { data[i] %= o[i]; }
            else { data[i] = std::fmod(data[i], o[i]); }
        }
        return *this;
    }
    [[nodiscard]] Vector operator%(const T &s) const { Vector r(*this); return (r %= s); }
    [[nodiscard]] Vector operator%(const Vector &o) const { Vector r(*this); return (r %= o); }

    [[nodiscard]] T min() const { T v = std::numeric_limits<T>::max(); for(auto &d : data) v = std::min(v, d); return v; }
    [[nodiscard]] T max() const { T v = std::numeric_limits<T>::lowest(); for(auto &d : data) v = std::max(v, d); return v; }
    void ceil()  { for(auto &d : data) d = std::ceil(d); }
    void floor() { for(auto &d : data) d = std::floor(d); }
    [[nodiscard]] Vector getCeil() const  { Vector r(*this); r.ceil(); return r; }
    [[nodiscard]] Vector getFloor() const { Vector r(*this); r.floor(); return r; }

    [[nodiscard]] T length2() const { T s = T{}; for(auto d : data) s += d*d; return s; }
    [[nodiscard]] T length()  const { return std::sqrt(length2()); }
    void normalize() { (*this) /= length(); }
    [[nodiscard]] Vector normalized() const { return Vector(*this) / length(); }
    template<typename U>
    [[nodiscard]] T dot(const Vector<U, N> &o) const { T t = T{}; for(int i = 0; i < N; i++) t += data[i] * o.data[i]; return t; }
};

template<typename T>
struct Vector<T, 3> {
    using type = T;
    static constexpr int N = 3;
    union { struct { T x, y, z; }; std::array<T, N> data; };

    [[nodiscard]] static Vector zeros() { return Vector(T{0}, T{0}, T{0}); }
    [[nodiscard]] static Vector ones()  { return Vector(T{1}, T{1}, T{1}); }

    Vector() : x(T{0}), y(T{0}), z(T{0}) {}
    Vector(T x_, T y_, T z_) : x(x_), y(y_), z(z_) {}
    Vector(const Vector &o) : x(o.x), y(o.y), z(o.z) {}
    Vector(const std::array<T, N> &d) : x(d[0]), y(d[1]), z(d[2]) {}
    Vector(const T *d) : x(d[0]), y(d[1]), z(d[2]) {}
    Vector(T val) : x(val), y(val), z(val) {}
    Vector(const std::string &str) { fromString(str); }
    template<typename U>
    Vector(const Vector<U, N> &o) { for(int i = 0; i < N; i++) data[i] = static_cast<T>(o.data[i]); }

    [[nodiscard]] T& operator[](int dim) { return data[dim]; }
    [[nodiscard]] const T& operator[](int dim) const { return data[dim]; }

    [[nodiscard]] std::string toString() const { std::ostringstream ss; ss << (*this); return ss.str(); }
    void fromString(const std::string &str) { std::istringstream ss(str); ss >> (*this); }

    [[nodiscard]] bool operator==(const Vector &o) const { return data == o.data; }
    [[nodiscard]] bool operator!=(const Vector &o) const { return data != o.data; }

    [[nodiscard]] bool operator> (const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] <= o.data[i]) return false; return true; }
    [[nodiscard]] bool operator< (const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] >= o.data[i]) return false; return true; }
    [[nodiscard]] bool operator>=(const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] <  o.data[i]) return false; return true; }
    [[nodiscard]] bool operator<=(const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] >  o.data[i]) return false; return true; }
    [[nodiscard]] bool operator> (T s) const { for(int i = 0; i < N; i++) if(data[i] <= s) return false; return true; }
    [[nodiscard]] bool operator< (T s) const { for(int i = 0; i < N; i++) if(data[i] >= s) return false; return true; }
    [[nodiscard]] bool operator>=(T s) const { for(int i = 0; i < N; i++) if(data[i] <  s) return false; return true; }
    [[nodiscard]] bool operator<=(T s) const { for(int i = 0; i < N; i++) if(data[i] >  s) return false; return true; }

    Vector& operator+=(const Vector &o) { for(int i = 0; i < N; i++) data[i] += o.data[i]; return *this; }
    [[nodiscard]] Vector operator+(const Vector &o) const { Vector r(*this); r += o; return r; }
    Vector& operator-=(const Vector &o) { for(int i = 0; i < N; i++) data[i] -= o.data[i]; return *this; }
    [[nodiscard]] Vector operator-(const Vector &o) const { Vector r(*this); r -= o; return r; }
    Vector& operator*=(const Vector &o) { for(int i = 0; i < N; i++) data[i] *= o.data[i]; return *this; }
    [[nodiscard]] Vector operator*(const Vector &o) const { Vector r(*this); r *= o; return r; }
    Vector& operator/=(const Vector &o) { for(int i = 0; i < N; i++) data[i] /= o.data[i]; return *this; }
    [[nodiscard]] Vector operator/(const Vector &o) const { Vector r(*this); r /= o; return r; }

    Vector& operator*=(T s) { for(int i = 0; i < N; i++) data[i] *= s; return *this; }
    [[nodiscard]] Vector operator*(T s) const { Vector r(*this); r *= s; return r; }
    Vector& operator/=(T s) { for(int i = 0; i < N; i++) data[i] /= s; return *this; }
    [[nodiscard]] Vector operator/(T s) const { Vector r(*this); r /= s; return r; }

    Vector& operator%=(const T &s) {
        for(int i = 0; i < N; i++) {
            if constexpr(std::is_integral_v<T>) { data[i] %= s; }
            else { data[i] = std::fmod(data[i], s); }
        }
        return *this;
    }
    Vector& operator%=(const Vector &o) {
        for(int i = 0; i < N; i++) {
            if constexpr(std::is_integral_v<T>) { data[i] %= o[i]; }
            else { data[i] = std::fmod(data[i], o[i]); }
        }
        return *this;
    }
    [[nodiscard]] Vector operator%(const T &s) const { Vector r(*this); return (r %= s); }
    [[nodiscard]] Vector operator%(const Vector &o) const { Vector r(*this); return (r %= o); }

    [[nodiscard]] T min() const { T v = std::numeric_limits<T>::max(); for(auto &d : data) v = std::min(v, d); return v; }
    [[nodiscard]] T max() const { T v = std::numeric_limits<T>::lowest(); for(auto &d : data) v = std::max(v, d); return v; }
    void ceil()  { for(auto &d : data) d = std::ceil(d); }
    void floor() { for(auto &d : data) d = std::floor(d); }
    [[nodiscard]] Vector getCeil() const  { Vector r(*this); r.ceil(); return r; }
    [[nodiscard]] Vector getFloor() const { Vector r(*this); r.floor(); return r; }

    [[nodiscard]] T length2() const { T s = T{}; for(auto d : data) s += d*d; return s; }
    [[nodiscard]] T length()  const { return std::sqrt(length2()); }
    void normalize() { (*this) /= length(); }
    [[nodiscard]] Vector normalized() const { return Vector(*this) / length(); }
    template<typename U>
    [[nodiscard]] T dot(const Vector<U, N> &o) const { T t = T{}; for(int i = 0; i < N; i++) t += data[i] * o.data[i]; return t; }
};

template<typename T>
struct Vector<T, 4> {
    using type = T;
    static constexpr int N = 4;
    union { struct { T x, y, z, w; }; std::array<T, N> data; };

    [[nodiscard]] static Vector zeros() { return Vector(T{0}, T{0}, T{0}, T{0}); }
    [[nodiscard]] static Vector ones()  { return Vector(T{1}, T{1}, T{1}, T{1}); }

    Vector() : x(T{0}), y(T{0}), z(T{0}), w(T{0}) {}
    Vector(T x_, T y_, T z_, T w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vector(const Vector &o) : x(o.x), y(o.y), z(o.z), w(o.w) {}
    Vector(const std::array<T, N> &d) : x(d[0]), y(d[1]), z(d[2]), w(d[3]) {}
    Vector(const T *d) : x(d[0]), y(d[1]), z(d[2]), w(d[3]) {}
    Vector(T val) : x(val), y(val), z(val), w(val) {}
    Vector(const std::string &str) { fromString(str); }
    template<typename U>
    Vector(const Vector<U, N> &o) { for(int i = 0; i < N; i++) data[i] = static_cast<T>(o.data[i]); }

    [[nodiscard]] T& operator[](int dim) { return data[dim]; }
    [[nodiscard]] const T& operator[](int dim) const { return data[dim]; }

    [[nodiscard]] std::string toString() const { std::ostringstream ss; ss << (*this); return ss.str(); }
    void fromString(const std::string &str) { std::istringstream ss(str); ss >> (*this); }

    [[nodiscard]] bool operator==(const Vector &o) const { return data == o.data; }
    [[nodiscard]] bool operator!=(const Vector &o) const { return data != o.data; }

    [[nodiscard]] bool operator> (const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] <= o.data[i]) return false; return true; }
    [[nodiscard]] bool operator< (const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] >= o.data[i]) return false; return true; }
    [[nodiscard]] bool operator>=(const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] <  o.data[i]) return false; return true; }
    [[nodiscard]] bool operator<=(const Vector &o) const { for(int i = 0; i < N; i++) if(data[i] >  o.data[i]) return false; return true; }
    [[nodiscard]] bool operator> (T s) const { for(int i = 0; i < N; i++) if(data[i] <= s) return false; return true; }
    [[nodiscard]] bool operator< (T s) const { for(int i = 0; i < N; i++) if(data[i] >= s) return false; return true; }
    [[nodiscard]] bool operator>=(T s) const { for(int i = 0; i < N; i++) if(data[i] <  s) return false; return true; }
    [[nodiscard]] bool operator<=(T s) const { for(int i = 0; i < N; i++) if(data[i] >  s) return false; return true; }

    Vector& operator+=(const Vector &o) { for(int i = 0; i < N; i++) data[i] += o.data[i]; return *this; }
    [[nodiscard]] Vector operator+(const Vector &o) const { Vector r(*this); r += o; return r; }
    Vector& operator-=(const Vector &o) { for(int i = 0; i < N; i++) data[i] -= o.data[i]; return *this; }
    [[nodiscard]] Vector operator-(const Vector &o) const { Vector r(*this); r -= o; return r; }
    Vector& operator*=(const Vector &o) { for(int i = 0; i < N; i++) data[i] *= o.data[i]; return *this; }
    [[nodiscard]] Vector operator*(const Vector &o) const { Vector r(*this); r *= o; return r; }
    Vector& operator/=(const Vector &o) { for(int i = 0; i < N; i++) data[i] /= o.data[i]; return *this; }
    [[nodiscard]] Vector operator/(const Vector &o) const { Vector r(*this); r /= o; return r; }

    Vector& operator*=(T s) { for(int i = 0; i < N; i++) data[i] *= s; return *this; }
    [[nodiscard]] Vector operator*(T s) const { Vector r(*this); r *= s; return r; }
    Vector& operator/=(T s) { for(int i = 0; i < N; i++) data[i] /= s; return *this; }
    [[nodiscard]] Vector operator/(T s) const { Vector r(*this); r /= s; return r; }

    Vector& operator%=(const T &s) {
        for(int i = 0; i < N; i++) {
            if constexpr(std::is_integral_v<T>) { data[i] %= s; }
            else { data[i] = std::fmod(data[i], s); }
        }
        return *this;
    }
    Vector& operator%=(const Vector &o) {
        for(int i = 0; i < N; i++) {
            if constexpr(std::is_integral_v<T>) { data[i] %= o[i]; }
            else { data[i] = std::fmod(data[i], o[i]); }
        }
        return *this;
    }
    [[nodiscard]] Vector operator%(const T &s) const { Vector r(*this); return (r %= s); }
    [[nodiscard]] Vector operator%(const Vector &o) const { Vector r(*this); return (r %= o); }

    [[nodiscard]] T min() const { T v = std::numeric_limits<T>::max(); for(auto &d : data) v = std::min(v, d); return v; }
    [[nodiscard]] T max() const { T v = std::numeric_limits<T>::lowest(); for(auto &d : data) v = std::max(v, d); return v; }
    void ceil()  { for(auto &d : data) d = std::ceil(d); }
    void floor() { for(auto &d : data) d = std::floor(d); }
    [[nodiscard]] Vector getCeil() const  { Vector r(*this); r.ceil(); return r; }
    [[nodiscard]] Vector getFloor() const { Vector r(*this); r.floor(); return r; }

    [[nodiscard]] T length2() const { T s = T{}; for(auto d : data) s += d*d; return s; }
    [[nodiscard]] T length()  const { return std::sqrt(length2()); }
    void normalize() { (*this) /= length(); }
    [[nodiscard]] Vector normalized() const { return Vector(*this) / length(); }
    template<typename U>
    [[nodiscard]] T dot(const Vector<U, N> &o) const { T t = T{}; for(int i = 0; i < N; i++) t += data[i] * o.data[i]; return t; }
};

template<typename T, int N>
[[nodiscard]] inline Vector<T, N> operator-(const Vector<T, N> &v) {
    Vector<T, N> r;
    for(int i = 0; i < N; i++) { r.data[i] = -v[i]; }
    return r;
}
template<typename T, Arithmetic U, int N>
[[nodiscard]] inline Vector<T, N> operator*(U scalar, const Vector<T, N> &v) {
    Vector<T, N> r;
    for(int i = 0; i < N; i++) { r.data[i] = scalar * v[i]; }
    return r;
}
template<typename T, Arithmetic U, int N>
[[nodiscard]] inline Vector<T, N> operator/(U scalar, const Vector<T, N> &v) {
    Vector<T, N> r;
    for(int i = 0; i < N; i++) { r.data[i] = scalar / v[i]; }
    return r;
}

template<typename T, int N> [[nodiscard]] inline Vector<T,N> zeros() { return Vector<T,N>::zeros(); }
template<typename T, int N> [[nodiscard]] inline Vector<T,N> ones()  { return Vector<T,N>::ones();  }

template<typename T, int N> [[nodiscard]] inline Vector<T, N> normalize(const Vector<T, N> &v) { return v.normalized(); }
template<typename T, int N> [[nodiscard]] inline T length2(const Vector<T, N> &v) { return v.length2(); }
template<typename T, int N> [[nodiscard]] inline T length (const Vector<T, N> &v) { return v.length(); }
template<typename T, int N> [[nodiscard]] inline T dot(const Vector<T, N> &v1, const Vector<T, N> &v2) { return v1.dot(v2); }
template<typename T, int N> [[nodiscard]] inline bool isnan(const Vector<T, N> &v) { for(const auto &d : v.data) if(std::isnan(d)) return true; return false; }
template<typename T, int N> [[nodiscard]] inline bool isinf(const Vector<T, N> &v) { for(const auto &d : v.data) if(std::isinf(d)) return true; return false; }

template<typename T, int N> [[nodiscard]] inline Vector<T, N> abs(const Vector<T, N> &v) {
    Vector<T, N> r; for(int i = 0; i < N; i++) r.data[i] = std::abs(v.data[i]); return r;
}
template<typename T, int N> [[nodiscard]] inline T min(const Vector<T, N> &v) {
    T m = std::numeric_limits<T>::max(); for(auto &d : v.data) m = std::min(m, d); return m;
}
template<typename T, int N> [[nodiscard]] inline T max(const Vector<T, N> &v) {
    T m = std::numeric_limits<T>::lowest(); for(auto &d : v.data) m = std::max(m, d); return m;
}
template<typename T, int N> [[nodiscard]] inline Vector<T, N> min(const Vector<T, N> &v1, const Vector<T, N> &v2) {
    Vector<T, N> r; for(int i = 0; i < N; i++) r[i] = std::min(v1[i], v2[i]); return r;
}
template<typename T, int N> [[nodiscard]] inline Vector<T, N> max(const Vector<T, N> &v1, const Vector<T, N> &v2) {
    Vector<T, N> r; for(int i = 0; i < N; i++) r[i] = std::max(v1[i], v2[i]); return r;
}
template<typename T, int N> [[nodiscard]] inline Vector<T, N> mod(const Vector<T, N> &v, const T &s) { return v % s; }
template<typename T, int N> [[nodiscard]] inline Vector<T, N> mod(const Vector<T, N> &v1, const Vector<T, N> &v2) { return v1 % v2; }
template<typename T, int N> [[nodiscard]] inline Vector<T, N> floor(const Vector<T, N> &v) {
    Vector<T, N> r; for(int i = 0; i < N; i++) r[i] = std::floor(v[i]); return r;
}
template<typename T, int N> [[nodiscard]] inline Vector<T, N> ceil(const Vector<T, N> &v) {
    Vector<T, N> r; for(int i = 0; i < N; i++) r[i] = std::ceil(v[i]); return r;
}

template<typename T>
[[nodiscard]] inline Vector<T, 2> cMult(const Vector<T, 2> &a, const Vector<T, 2> &b) {
    return Vector<T, 2>(a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x);
}
template<typename T>
[[nodiscard]] inline Vector<T, 2> cConj(const Vector<T, 2> &a) { return Vector<T, 2>(a.x, -a.y); }
template<typename T>
[[nodiscard]] inline Vector<T, 4> qMult(const Vector<T, 4> &a, const Vector<T, 4> &b) {
    return Vector<T, 4>(a.x*b.x - a.y*b.y - a.z*b.z - a.w*b.w, a.x*b.y + a.y*b.x + a.z*b.w - a.w*b.z,
                        a.x*b.z - a.y*b.w + a.z*b.x + a.w*b.y, a.x*b.w + a.y*b.z - a.z*b.y + a.w*b.x);
}
template<typename T>
[[nodiscard]] inline Vector<T, 4> qConj(const Vector<T, 4> &a) { return Vector<T, 4>(a.x, -a.y, -a.z, -a.w); }

template<typename T>
[[nodiscard]] inline Vector<T, 3> cross(const Vector<T, 3> &a, const Vector<T, 3> &b) {
    return Vector<T, 3>(a.y, a.z, a.x) * Vector<T, 3>(b.z, b.x, b.y)
         - Vector<T, 3>(a.z, a.x, a.y) * Vector<T, 3>(b.y, b.z, b.x);
}

template<typename T>
[[nodiscard]] inline Vector<T, 3> rotate(const Vector<T, 3> &v, const Vector<T, 3> &ax, T theta) {
    const T cos_t2 = static_cast<T>(std::cos(theta/2.0));
    const T sin_t2 = static_cast<T>(std::sin(theta/2.0));
    const Vector<T, 4> q1(0, v.x, v.y, v.z);
    const Vector<T, 4> q2(cos_t2, ax.x*sin_t2, ax.y*sin_t2, ax.z*sin_t2);
    const Vector<T, 4> q3 = qMult(qMult(q2, q1), qConj(q2));
    return Vector<T, 3>(q3.y, q3.z, q3.w);
}

template<typename T> struct is_vec : std::false_type {};
template<typename T, int N> struct is_vec<Vector<T, N>> : std::true_type {};
template<typename T> constexpr bool is_vec_v = is_vec<T>::value;

template<typename T> struct is_any_vec : std::false_type {};
template<typename T, int N> struct is_any_vec<Vector<T, N>> : std::true_type {};
template<typename T> constexpr bool is_any_vec_v = is_any_vec<T>::value;

template<typename T> concept is_vector = is_vec_v<T>;

template<typename T, int N_=1>
struct vec {
    static constexpr int N = (is_vec_v<T> && std::greater<int>{}(T::N, 1)) ? T::N : N_;
    using BASE = typename std::conditional<is_vec_v<T> && std::greater<int>{}(T::N, 1), typename T::type, T>::type;
    using VT    = Vector<BASE, N>;
    using IT    = Vector<int, N>;
    using LOWER = Vector<BASE, N-1>;
};

template<typename T, typename T2=void> struct any_vec {};
template<typename T> struct any_vec<T, typename std::enable_if_t<is_vec_v<T>>> : vec<T,1> {};

template<typename T, int N> [[nodiscard]] inline       T* arr(      Vector<T, N> &v) { return v.data.data(); }
template<typename T, int N> [[nodiscard]] inline const T* arr(const Vector<T, N> &v) { return v.data.data(); }

template<typename T> std::enable_if_t<is_vec_v<T>, std::string> to_string(const T &val, const int precision=6) {
    std::ostringstream out;
    out.precision(precision);
    out << std::fixed << val;
    return out.str();
}
template<typename T> std::enable_if_t<is_vec_v<T>, T> from_string(const std::string &valStr) {
    std::istringstream ss(valStr); T val; ss >> val; return val;
}

template<typename T, int N>
inline void to_json(nlohmann::json &js, const Vector<T, N> &v) {
    js = nlohmann::json::array();
    for(int i = 0; i < N; i++) { js.push_back(v[i]); }
}
template<typename T, int N>
inline void from_json(const nlohmann::json &js, Vector<T, N> &v) {
    for(int i = 0; i < N; i++) { v[i] = js[i]; }
}

}
