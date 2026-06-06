#pragma once

#include <algorithm>

namespace imtool {

template<typename T>
struct Range {
    T lower;
    T upper;

    Range(const T &l = T{0}, const T &u = T{0}) : lower(l), upper(u) {}
    Range(const Range &r) : lower(r.lower), upper(r.upper) {}

    Range& operator=(const Range &r) { lower = r.lower; upper = r.upper; return *this; }

    [[nodiscard]] bool operator==(const Range &r) const { return (lower == r.lower && upper == r.upper); }
    [[nodiscard]] bool operator!=(const Range &r) const { return (lower != r.lower || upper != r.upper); }

    [[nodiscard]] bool contains(const T &x) const { return (x >= lower && x <= upper); }

    [[nodiscard]] T clip(const T &x) const { return std::max(std::min(x, upper), lower); }

    [[nodiscard]] T span() const { return upper - lower; }

    void extend(const T &amount) { lower -= amount; upper += amount; }
    [[nodiscard]] Range extended(const T &amount) const { return Range(lower - amount, upper + amount); }

    void fit(const T &x) { lower = std::min(lower, x); upper = std::max(upper, x); }
};

using Rangei = Range<int>;
using Rangef = Range<float>;
using Ranged = Range<double>;

}
