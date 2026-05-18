#pragma once

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <type_traits>

#include <nlohmann/json.hpp>

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

private:
    union {
        std::array<T, N*M> m_data;
        std::array<ColVector, M> m_columns;
    };

    template<typename TT = Matrix<T,N,M>>
    void cofactor(Matrix<T,N,M> &result, int row, int col, int dim) const requires is_square_matrix<TT> {
        int i = 0; int j = 0;
        for(int x = 0; x < dim; x++)
            for(int y = 0; y < dim; y++)
                if(x != col && y != row) {
                    result[j++][i] = m_data[y*M + x];
                    if(j == dim - 1) { j = 0; i++; }
                }
    }
    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] T determinant(int dim) const requires is_square_matrix<TT> {
        if(dim == 1) { return m_columns[0][0]; }
        T det = T{0}; T sign = T{1}; Matrix<T,N,M> temp;
        for(int c = 0; c < dim; c++) {
            cofactor(temp, 0, c, dim);
            det += sign*m_data[0*M + c] * temp.determinant(dim-1);
            sign = -sign;
        }
        return det;
    }
    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] TT adjoint() const requires is_square_matrix<TT> {
        T sign = T{1}; Matrix<T,N,M> result, temp;
        if(N == 1) { result[0][0] = T{1}; return result; }
        for(int c = 0; c < N; c++)
            for(int r = 0; r < N; r++) {
                cofactor(temp, r, c, N);
                sign = ((r+c) % 2 == 0) ? T{1} : T{-1};
                result[r][c] = sign*temp.determinant(N-1);
            }
        return result;
    }

public:
    Matrix() { if constexpr(N == M) { identity(); } else { zero(); } }
    Matrix(const std::array<ColVector, M> &cols) : m_columns(cols) {}
    Matrix(const std::array<T, N*M>       &flat) : m_data(flat) {}
    Matrix(const Matrix &o) : m_data(o.m_data) {}

    template<typename U>
    Matrix(const Matrix<U,N,M> &o) {
        for(int i = 0; i < N; i++)
            for(int j = 0; j < M; j++)
                { m_data[i*M + j] = static_cast<T>(o.m_data[i*M + j]); }
    }

    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] static TT makeIdentity() requires is_square_matrix<TT> { return Matrix<T,N,M>(); }

    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] static TT makeTranslate(const Vector<T, N-1> &dPos) requires is_square_matrix<TT> {
        Matrix<T,N,M> r = makeIdentity();
        for(int i = 0; i < N-1; i++) { r[N-1][i] = dPos[i]; }
        return r;
    }
    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] static TT makeScale(const Vector<T, N-1> &dScale) requires is_square_matrix<TT> {
        Matrix<T,N,M> r = makeIdentity();
        for(int i = 0; i < N-1; i++) { r[i][i] = dScale[i]; }
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
    [[nodiscard]] ColVector col(int c) const { return m_columns[c]; }
    [[nodiscard]] RowVector row(int r) const { RowVector r2; for(int c = 0; c < M; c++) { r2[c] = m_columns[c][r]; } return r2; }

    template<typename TT = Matrix<T,N,M>>
    TT& identity() requires is_square_matrix<TT> {
        for(int c = 0; c < M; c++)
            for(int r = 0; r < N; r++)
                { m_data[r*M + c] = static_cast<T>(r == c ? 1 : 0); }
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
        for(int x = 0; x < M; x++) for(int y = 0; y < N; y++) { r[x][y] = m_data[y*M + x]; }
        return r;
    }

    template<typename TT = Matrix<T,N,M>>
    [[nodiscard]] typename std::enable_if<(N == M), TT>::type inverse() const {
        T det = determinant(N);
        if(det == T{0}) { std::cout << "====> WARNING: Matrix doesn't have an inverse!\n"; return *this; }
        return adjoint() / det;
    }

    Matrix& operator=(const Matrix &o) { m_data = o.m_data; return *this; }

    [[nodiscard]] bool operator==(const Matrix &o) const { for(int i = 0; i < N*M; i++) if(m_data[i] != o.m_data[i]) return false; return true; }
    [[nodiscard]] bool operator!=(const Matrix &o) const { return !(*this == o); }

    [[nodiscard]] const ColVector& operator[](int c) const { return m_columns[c]; }
    [[nodiscard]] ColVector& operator[](int c)             { return m_columns[c]; }
    [[nodiscard]] const T& operator()(int r, int c) const  { return m_data[r*M + c]; }
    [[nodiscard]] T& operator()(int r, int c)              { return m_data[r*M + c]; }

    Matrix& operator+=(const Matrix &rhs)       { for(int i = 0; i < N*M; i++) m_data[i] += rhs.m_data[i]; return *this; }
    [[nodiscard]] Matrix operator+ (const Matrix &rhs) const { Matrix r(*this); return (r += rhs); }
    Matrix& operator-=(const Matrix &rhs)       { for(int i = 0; i < N*M; i++) m_data[i] -= rhs.m_data[i]; return *this; }
    [[nodiscard]] Matrix operator- (const Matrix &rhs) const { Matrix r(*this); return (r -= rhs); }

    template<typename TT = Matrix<T,N,M>>
    typename std::enable_if<(N == M), TT&>::type operator^=(const TT &rhs) {
        std::array<Vector<T, M>, N> result;
        for(int r = 0; r < N; r++)
            for(int c = 0; c < M; c++)
                { result[r][c] = dot(row(r), rhs.col(c)); }
        m_columns = result;
        return *this;
    }
    template<typename TT=Matrix<T,N,M>>
    [[nodiscard]] TT operator^(const TT &rhs) const requires is_square_matrix<TT> {
        TT r(*this); return (r ^= rhs);
    }

    template<int NN=N, int MM=M, typename TT=Matrix<T,NN,MM>>
    [[nodiscard]] TT operator^(const TT &rhs) const requires is_square_matrix<TT> {
        Matrix<T, N, MM> r;
        for(int rr = 0; rr < N; rr++) for(int c = 0; c < MM; c++) { r[rr][c] = dot(row(rr), rhs.col(c)); }
        return r;
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

    template<typename T2, int N2, int M2> friend Matrix<T2,N2,M2> operator+(const T &lhs, const Matrix<T2,N2,M2> &rhs);
    template<typename T2, int N2, int M2> friend Matrix<T2,N2,M2> operator-(const T &lhs, const Matrix<T2,N2,M2> &rhs);
    template<typename T2, int N2, int M2> friend Matrix<T2,N2,M2> operator*(const T &lhs, const Matrix<T2,N2,M2> &rhs);
    template<typename T2, int N2, int M2> friend Matrix<T2,N2,M2> operator/(const T &lhs, const Matrix<T2,N2,M2> &rhs);

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
[[nodiscard]] inline Matrix<T,N,M> operator+(const T &lhs, const Matrix<T,N,M> &rhs) { return lhs + rhs; }
template<typename T, int N, int M>
[[nodiscard]] inline Matrix<T,N,M> operator-(const T &lhs, const Matrix<T,N,M> &rhs) {
    Matrix<T,N,M> r = rhs;
    for(int i = 0; i < N*M; i++) { r.m_data[i] = lhs - rhs.m_data[i]; }
    return r;
}
template<typename T, int N, int M>
[[nodiscard]] inline Matrix<T,N,M> operator*(const T &lhs, const Matrix<T,N,M> &rhs) { return lhs * rhs; }
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
    for(int r = 0; r < N; r++)
        for(int c = 0; c < M; c++) {
            is >> mat[r][c];
            is.ignore((r != N-1 || c != M-1) ? ((c == M-1) ? 5 : 1) : 1);
        }
    return is;
}

template<typename T, int N, int M>
inline void to_json(nlohmann::json &js, const Matrix<T,N,M> &mat) {
    js = nlohmann::json::array();
    for(int i = 0; i < N; i++) {
        nlohmann::json row = nlohmann::json::array();
        for(int j = 0; j < M; j++) { row.push_back(mat[i][j]); }
        js.push_back(row);
    }
}
template<typename T, int N, int M>
inline void from_json(const nlohmann::json &js, Matrix<T,N,M> &mat) {
    for(int i = 0; i < N; i++)
        for(int j = 0; j < M; j++)
            { mat[i][j] = js[i][j]; }
}

}
