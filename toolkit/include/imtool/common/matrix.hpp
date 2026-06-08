#pragma once

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <type_traits>

#include <nlohmann/json.hpp>

#include <imtool/common/logging.hpp>
#include <imtool/common/vector.hpp>

namespace imtool {

template<typename T, int N, int M=N> class Matrix;

template<typename T> using Mat1x1 = Matrix<T, 1>;
template<typename T> using Mat2x2 = Matrix<T, 2>;
template<typename T> using Mat3x3 = Matrix<T, 3>;
template<typename T> using Mat4x4 = Matrix<T, 4>;

using Mat1x1i  = Matrix<int, 1>;
using Mat2x2i  = Matrix<int, 2>;
using Mat3x3i  = Matrix<int, 3>;
using Mat4x4i  = Matrix<int, 4>;
using Mat1x1ui = Matrix<unsigned int, 1>;
using Mat2x2ui = Matrix<unsigned int, 2>;
using Mat3x3ui = Matrix<unsigned int, 3>;
using Mat4x4ui = Matrix<unsigned int, 4>;
using Mat1x1f  = Matrix<float, 1>;
using Mat2x2f  = Matrix<float, 2>;
using Mat3x3f  = Matrix<float, 3>;
using Mat4x4f  = Matrix<float, 4>;
using Mat1x1d  = Matrix<double, 1>;
using Mat2x2d  = Matrix<double, 2>;
using Mat3x3d  = Matrix<double, 3>;
using Mat4x4d  = Matrix<double, 4>;
using Mat1i  = Mat1x1i;  using Mat2i  = Mat2x2i;  using Mat3i  = Mat3x3i;  using Mat4i  = Mat4x4i;
using Mat1ui = Mat1x1ui; using Mat2ui = Mat2x2ui; using Mat3ui = Mat3x3ui; using Mat4ui = Mat4x4ui;
using Mat1f  = Mat1x1f;  using Mat2f  = Mat2x2f;  using Mat3f  = Mat3x3f;  using Mat4f  = Mat4x4f;
using Mat1d  = Mat1x1d;  using Mat2d  = Mat2x2d;  using Mat3d  = Mat3x3d;  using Mat4d  = Mat4x4d;

template<typename T> struct is_mat : std::false_type {};
template<typename T, int N, int M> struct is_mat<Matrix<T, N, M>> : std::true_type {};
template<typename T> constexpr bool is_mat_v = is_mat<T>::value;

template<typename T, int N_=1, int M_=N_>
struct mat_type {
    static constexpr int N = (is_mat_v<T> && std::greater<int>{}(T::N, 1)) ? T::N : N_;
    static constexpr int M = (is_mat_v<T> && std::greater<int>{}(T::M, 1)) ? T::M : M_;
    using BASE = typename std::conditional<(is_mat_v<T> && std::greater<int>{}(T::N, 1) &&
                                            std::greater<int>{}(T::M, 1)), typename T::type, T>::type;
    using MT    = Matrix<BASE, N, M>;
    using IT    = Matrix<int,  N, M>;
    using LOWER = Matrix<BASE, N-1, M-1>;
};

template<typename T> concept is_square_matrix = requires(T) {
    requires is_mat_v<T>;
    requires (T::N == T::M);
};

template<typename T_, int N_, int M_>
class Matrix {
public:
    using T = T_;
    static constexpr int N = N_;
    static constexpr int M = M_;

    using RowVector = Vector<T, M>;
    using ColVector = Vector<T, N>;

    // Lightweight column views: m[c][r] reads/writes element (row r, col c) over the
    // flat column-major buffer, with no separate column objects to alias. The held
    // pointer stays within the single m_data array, so the indexing is well-defined
    // (unlike a reinterpret_cast across vector subobjects).
    class ColView {
        T *m_col;
    public:
        explicit ColView(T *col) : m_col(col) {}
        [[nodiscard]] T&       operator[](int r)       { return m_col[r]; }
        [[nodiscard]] const T& operator[](int r) const { return m_col[r]; }
        [[nodiscard]] operator ColVector() const { ColVector v; for(int r = 0; r < N; r++) { v[r] = m_col[r]; } return v; }
    };
    class ConstColView {
        const T *m_col;
    public:
        explicit ConstColView(const T *col) : m_col(col) {}
        [[nodiscard]] const T& operator[](int r) const { return m_col[r]; }
        [[nodiscard]] operator ColVector() const { ColVector v; for(int r = 0; r < N; r++) { v[r] = m_col[r]; } return v; }
    };

private:
    // Column-major flat storage: element (row r, col c) is at index c*N + r. A
    // single member (no union) keeps the storage portable — no inactive-union-member
    // reads (the prior layout was [class.union] UB off g++). data() hands OpenGL the
    // column-major buffer directly.
    std::array<T, N*M> m_data{};

    template<typename, int, int> friend class Matrix;   // sibling-T conversions read m_data

    template<typename TT = Matrix<T,N,M>>
    void cofactor(Matrix<T,N,M> &result, int row, int col, int dim) const requires is_square_matrix<TT> {
        // Fill the top-left (dim-1)x(dim-1) block with the minor: this matrix with
        // `row` and `col` deleted. Addressing via operator() keeps the storage
        // convention consistent (the previous hand-rolled index walk transposed it).
        int ri = 0;
        for(int r = 0; r < dim; r++) {
            if(r == row) { continue; }
            int ci = 0;
            for(int c = 0; c < dim; c++) {
                if(c == col) { continue; }
                result(ri, ci) = (*this)(r, c);
                ci++;
            }
            ri++;
        }
    }
    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] T determinant(int dim) const requires is_square_matrix<TT> {
        if(dim == 1) { return (*this)(0, 0); }
        T det = T{0}; T sign = T{1}; Matrix<T,N,M> temp;
        for(int c = 0; c < dim; c++) {
            cofactor(temp, 0, c, dim);
            det += sign*(*this)(0, c) * temp.determinant(dim-1);
            sign = -sign;
        }
        return det;
    }
    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] TT adjoint() const requires is_square_matrix<TT> {
        Matrix<T,N,M> result, temp;
        if(N == 1) { result(0,0) = T{1}; return result; }
        // Adjugate = transpose of the cofactor matrix: adj(i,j) = (-1)^(i+j) * M_ji,
        // where M_ji is the minor with row j and column i removed.
        for(int i = 0; i < N; i++)
            for(int j = 0; j < N; j++) {
                cofactor(temp, j, i, N);
                const T sign = ((i+j) % 2 == 0) ? T{1} : T{-1};
                result(i, j) = sign*temp.determinant(N-1);
            }
        return result;
    }

public:
    Matrix() { if constexpr(N == M) { identity(); } else { zero(); } }
    Matrix(const std::array<ColVector, M> &cols) {
        for(int c = 0; c < M; c++) for(int r = 0; r < N; r++) { m_data[c*N + r] = cols[c][r]; }
    }
    Matrix(const std::array<T, N*M> &flat) : m_data(flat) {}   // flat is column-major (index c*N + r)
    Matrix(const Matrix &o) = default;

    template<typename U>
    Matrix(const Matrix<U,N,M> &o) {
        for(int i = 0; i < N*M; i++) { m_data[i] = static_cast<T>(o.m_data[i]); }
    }

    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] static TT makeIdentity() requires is_square_matrix<TT> { return Matrix<T,N,M>(); }

    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] static TT makeTranslate(const Vector<T, N-1> &dPos) requires is_square_matrix<TT> {
        Matrix<T,N,M> r = makeIdentity();
        for(int i = 0; i < N-1; i++) { r(i, N-1) = dPos[i]; }
        return r;
    }
    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] static TT makeScale(const Vector<T, N-1> &dScale) requires is_square_matrix<TT> {
        Matrix<T,N,M> r = makeIdentity();
        for(int i = 0; i < N-1; i++) { r(i, i) = dScale[i]; }
        return r;
    }
    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] static TT makeProjection(float fov, float aspect, float near, float far)
        requires (is_square_matrix<TT> && TT::N == 4) {
        const T t = static_cast<T>(std::tan(fov/2.0)*near);
        const T b = -t;
        const T r =  t * aspect;
        const T l = -t * aspect;
        Matrix<T,N,M> m;
        m(0,0) = 2*near/(r-l); m(0,1) = 0;            m(0,2) = (r+l)/(r-l);            m(0,3) =  0;
        m(1,0) = 0;            m(1,1) = 2*near/(t-b); m(1,2) = (t+b)/(t-b);            m(1,3) =  0;
        m(2,0) = 0;            m(2,1) = 0;            m(2,2) = -(far+near)/(far-near); m(2,3) = -2*far*near/(far-near);
        m(3,0) = 0;            m(3,1) = 0;            m(3,2) = -1;                     m(3,3) =  0;
        return m;
    }

    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] static TT makeLookAt(const Vector<T, 3> &pos, const Vector<T, 3> &focus,
                                       const Vector<T, 3> &upBasis = Vector<T, 3>(0,1,0))
        requires (TT::N == TT::M && TT::N == 4) {
        Vector<T, 3> eye   = normalize(pos-focus);
        Vector<T, 3> up    = upBasis;
        Vector<T, 3> right = normalize(cross(up, eye));
        up = cross(eye, right);
        Matrix<T,N,M> basis; basis.identity();
        basis(0,0) = right.x; basis(0,1) = up.x; basis(0,2) = eye.x;
        basis(1,0) = right.y; basis(1,1) = up.y; basis(1,2) = eye.y;
        basis(2,0) = right.z; basis(2,1) = up.z; basis(2,2) = eye.z;
        Matrix<T,N,M> posMat; posMat.identity();
        posMat(0,3) = -pos.x;
        posMat(1,3) = -pos.y;
        posMat(2,3) = -pos.z;
        return (basis ^ posMat).transposed();
    }

    [[nodiscard]] T* data()             { return m_data.data(); }
    [[nodiscard]] const T* data() const { return m_data.data(); }
    [[nodiscard]] ColVector col(int c) const { ColVector v; for(int r = 0; r < N; r++) { v[r] = m_data[c*N + r]; } return v; }
    [[nodiscard]] RowVector row(int r) const { RowVector v; for(int c = 0; c < M; c++) { v[c] = m_data[c*N + r]; } return v; }

    template<typename TT = Matrix<T,N,M>>
    TT& identity() requires is_square_matrix<TT> {
        for(int c = 0; c < M; c++)
            for(int r = 0; r < N; r++)
                { m_data[c*N + r] = static_cast<T>(r == c ? 1 : 0); }
        return *this;
    }
    template<typename TT = Matrix<T,N,M>>
    TT& zero() { for(auto &d : m_data) { d = T{0}; } return *this; }

    template<typename TT = Matrix<T,N,M>>
    TT& translate(const Vector<T, N-1> &dPos) requires is_square_matrix<TT> {
        return ((*this) = Matrix<T,N,M>::makeTranslate(dPos) ^ (*this));
    }
    template<typename TT = Matrix<T,N,M>>
    TT& scale(const Vector<T, N-1> &dScale) requires is_square_matrix<TT> {
        return ((*this) = Matrix<T,N,M>::makeScale(dScale) ^ (*this));
    }
    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] TT translated(const Vector<T, N-1> &dPos) requires is_square_matrix<TT> {
        return (Matrix<T,N,M>::makeTranslate(dPos) ^ (*this));
    }
    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] TT scaled(const Vector<T, N-1> &dScale) requires is_square_matrix<TT> {
        return (Matrix<T,N,M>::makeScale(dScale) ^ (*this));
    }

    [[nodiscard]] Matrix<T, M, N> transposed() const {
        Matrix<T, M, N> r;
        for(int i = 0; i < M; i++) for(int j = 0; j < N; j++) { r(i, j) = (*this)(j, i); }
        return r;
    }

    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] typename std::enable_if<(N == M), TT>::type inverse() const {
        T det = determinant(N);
        if(det == T{0}) {
            log() << LogLevel::Warning << "Matrix has no inverse (determinant is zero); returning input unchanged";
            log().flush();
            return *this;
        }
        return adjoint() / det;
    }

    Matrix& operator=(const Matrix &o) { m_data = o.m_data; return *this; }

    [[nodiscard]] bool operator==(const Matrix &o) const { for(int i = 0; i < N*M; i++) if(m_data[i] != o.m_data[i]) return false; return true; }
    [[nodiscard]] bool operator!=(const Matrix &o) const { return !(*this == o); }

    [[nodiscard]] ConstColView operator[](int c) const { return ConstColView(&m_data[c*N]); }
    [[nodiscard]] ColView      operator[](int c)       { return ColView(&m_data[c*N]); }
    // Column-major addressing, consistent with operator[](c)[r] and data() (the flat
    // buffer OpenGL expects): element (row r, col c) == m_data[c*N + r].
    [[nodiscard]] const T& operator()(int r, int c) const  { return m_data[c*N + r]; }
    [[nodiscard]] T& operator()(int r, int c)              { return m_data[c*N + r]; }

    Matrix& operator+=(const Matrix &rhs)       { for(int i = 0; i < N*M; i++) m_data[i] += rhs.m_data[i]; return *this; }
    [[nodiscard]] Matrix operator+ (const Matrix &rhs) const { Matrix r(*this); return (r += rhs); }
    Matrix& operator-=(const Matrix &rhs)       { for(int i = 0; i < N*M; i++) m_data[i] -= rhs.m_data[i]; return *this; }
    [[nodiscard]] Matrix operator- (const Matrix &rhs) const { Matrix r(*this); return (r -= rhs); }

    template<typename TT = Matrix<T,N,M>>
    typename std::enable_if<(N == M), TT&>::type operator^=(const TT &rhs) {
        std::array<T, N*M> result;
        // (M*rhs)(r,c) = dot(row r, column c); column-major storage puts (r,c) at c*N + r.
        for(int r = 0; r < N; r++)
            for(int c = 0; c < M; c++)
                { result[c*N + r] = dot(row(r), rhs.col(c)); }
        m_data = result;
        return *this;
    }
    template<typename TT=Matrix<T,N,M>>
    [[nodiscard]] TT operator^(const TT &rhs) const requires is_square_matrix<TT> {
        TT r(*this); return (r ^= rhs);
    }

    [[nodiscard]] Vector<T, M> operator^(const Vector<T, M> &rhs) const {
        Vector<T, M> r;
        for(int i = 0; i < M; i++) { r[i] = dot(row(i), rhs); }
        return r;
    }

    template<typename TT, int NN, int MM>
    friend Vector<TT, NN> operator^(const Vector<TT, NN> &lhs, const Matrix<TT, NN, MM> &rhs);

    template<int NN=N, int MM=M>
    Matrix<T,NN,MM>& operator%=(const Matrix<T,NN,MM> &rhs) requires (is_square_matrix<Matrix<T,NN,MM>> && MM == M) {
        return (*this ^= rhs.inverse());
    }
    template<int NN=N, int MM=M>
    [[nodiscard]] Matrix<T,NN,MM> operator%(const Matrix<T,NN,MM> &rhs) const requires (is_square_matrix<Matrix<T,NN,MM>> && MM == M) {
        Matrix<T,NN,MM> r(*this); r %= rhs; return r;
    }

    Matrix& operator/=(const Matrix &rhs)       { for(int i = 0; i < N*M; i++) m_data[i] /= rhs.m_data[i]; return *this; }
    [[nodiscard]] Matrix operator/ (const Matrix &rhs) const { Matrix r(*this); return (r /= rhs); }
    Matrix& operator*=(const Matrix &rhs)       { for(int i = 0; i < N*M; i++) m_data[i] *= rhs.m_data[i]; return *this; }
    [[nodiscard]] Matrix operator* (const Matrix &rhs) const { Matrix r(*this); return (r *= rhs); }

    Matrix& operator*=(const T &rhs)      { for(int i = 0; i < N*M; i++) m_data[i] *= rhs; return *this; }
    [[nodiscard]] Matrix operator*(const T &rhs) const { Matrix r(*this); return (r *= rhs); }
    Matrix& operator/=(const T &rhs)      { for(int i = 0; i < N*M; i++) m_data[i] /= rhs; return *this; }
    [[nodiscard]] Matrix operator/(const T &rhs) const { Matrix r(*this); return (r /= rhs); }

    // lhs is `const T2&` (matching the namespace-scope definitions below) — NOT
    // `const T&`. With `const T&` the friend is a *different* template than the
    // definition, so `scalar op matrix` saw two equally-good overloads and was
    // ambiguous (never compiled).
    template<typename T2, int N2, int M2> friend Matrix<T2,N2,M2> operator+(const T2 &lhs, const Matrix<T2,N2,M2> &rhs);
    template<typename T2, int N2, int M2> friend Matrix<T2,N2,M2> operator-(const T2 &lhs, const Matrix<T2,N2,M2> &rhs);
    template<typename T2, int N2, int M2> friend Matrix<T2,N2,M2> operator*(const T2 &lhs, const Matrix<T2,N2,M2> &rhs);
    template<typename T2, int N2, int M2> friend Matrix<T2,N2,M2> operator/(const T2 &lhs, const Matrix<T2,N2,M2> &rhs);

    [[nodiscard]] T sum() const {
        T s = T{0};
        for(int i = 0; i < N*M; i++) { s += m_data[i]; }
        return s;
    }

    [[nodiscard]] std::string toString() const { std::stringstream ss; ss << *this; return ss.str(); }
    template<typename T2, int N2, int M2> friend std::ostream& operator<<(std::ostream &os, const Matrix<T2,N2,M2> &mat);
    template<typename T2, int N2, int M2> friend std::istream& operator>>(std::istream &is, Matrix<T2,N2,M2> &mat);
};

template<typename T, int N, int M>
[[nodiscard]] inline Matrix<T,N,M> operator+(const T &lhs, const Matrix<T,N,M> &rhs) {
    Matrix<T,N,M> r = rhs;
    for(int i = 0; i < N*M; i++) { r.m_data[i] = lhs + rhs.m_data[i]; }
    return r;
}
template<typename T, int N, int M>
[[nodiscard]] inline Matrix<T,N,M> operator-(const T &lhs, const Matrix<T,N,M> &rhs) {
    Matrix<T,N,M> r = rhs;
    for(int i = 0; i < N*M; i++) { r.m_data[i] = lhs - rhs.m_data[i]; }
    return r;
}
template<typename T, int N, int M>
[[nodiscard]] inline Matrix<T,N,M> operator*(const T &lhs, const Matrix<T,N,M> &rhs) {
    Matrix<T,N,M> r = rhs;
    for(int i = 0; i < N*M; i++) { r.m_data[i] = lhs * rhs.m_data[i]; }
    return r;
}
template<typename T, int N, int M>
[[nodiscard]] inline Matrix<T,N,M> operator/(const T &lhs, const Matrix<T,N,M> &rhs) {
    Matrix<T,N,M> r = rhs;
    for(int i = 0; i < N*M; i++) { r.m_data[i] = lhs/rhs.m_data[i]; }
    return r;
}

template<typename T, int N, int M>
[[nodiscard]] inline Vector<T, N> operator^(const Vector<T, N> &lhs, const Matrix<T, N, M> &rhs) {
    Vector<T, N> r;
    for(int i = 0; i < N; i++) { r[i] = dot(lhs, rhs.col(i)); }
    return r;
}

template<typename T, int N, int M>
inline std::ostream& operator<<(std::ostream &os, const Matrix<T,N,M> &mat) {
    os << "  ";
    for(int i = 0; i < 8*4+1; i++) { os << "="; }
    os << "\n  |";
    for(int r = 0; r < N; r++)
        for(int c = 0; c < M; c++) {
            os << std::right << std::internal << std::setprecision(5) << std::fixed << std::setw(10) << mat(r,c);
            os << ((r != N-1 || c != M-1) ? ((c == M-1) ? "|\n  |" : (" ")) : "|");
        }
    os << "\n  ";
    for(int i = 0; i < 8*4+1; i++) { os << "="; }
    os << "\n";
    return os;
}

template<typename T, int N, int M>
inline std::istream& operator>>(std::istream &is, Matrix<T,N,M> &mat) {
    // Reads N*M whitespace-separated values in row-major (r,c) order via operator().
    // NOTE: this does not parse operator<<'s decorated box form (that is a human
    // display); use to_json/from_json for exact serialization round-trips.
    for(int r = 0; r < N; r++)
        for(int c = 0; c < M; c++) { is >> mat(r, c); }
    return is;
}

// JSON nests as row-major [[row0...],[row1...]]: js[r][c] == element (row r, col c),
// matching operator()/operator<<. (Index via operator(), not operator[], which is
// column-major and would transpose.)
template<typename T, int N, int M>
inline void to_json(nlohmann::json &js, const Matrix<T,N,M> &mat) {
    js = nlohmann::json::array();
    for(int r = 0; r < N; r++) {
        nlohmann::json row = nlohmann::json::array();
        for(int c = 0; c < M; c++) { row.push_back(mat(r, c)); }
        js.push_back(row);
    }
}
template<typename T, int N, int M>
inline void from_json(const nlohmann::json &js, Matrix<T,N,M> &mat) {
    for(int r = 0; r < N; r++)
        for(int c = 0; c < M; c++)
            { mat(r, c) = js[r][c]; }
}

}
