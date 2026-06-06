#include "imtest.hpp"

#include <imtool/common/matrix.hpp>

using namespace imtool;

int main() {
  IMTEST_SUITE("matrix");

  // default-constructed square matrix is the identity
  Mat3f I;
  CHECK(I(0, 0) == 1.0f && I(1, 1) == 1.0f && I(2, 2) == 1.0f);
  CHECK(I(0, 1) == 0.0f && I(1, 0) == 0.0f && I(2, 0) == 0.0f);

  // === Finding #3: operator()(r,c) and operator[c][r] must address the SAME
  // element. With a row-major flat accessor over column-major storage they
  // disagree. (r,c) = (row, col); [c][r] = (column c, then row r) = (r,c). ===
  {
    Mat3f m;
    m(0, 1) = 5.0f;         // row 0, col 1
    CHECK(m[1][0] == 5.0f); // column 1, row 0  == element (0,1)
    m(2, 0) = 9.0f;         // row 2, col 0
    CHECK(m[0][2] == 9.0f); // column 0, row 2  == element (2,0)
  }

  // === Finding #13: operator^ (matmul) must be a real matrix product, not its
  // transpose. A=[[1,2],[3,4]], B=[[5,6],[7,8]] => A*B = [[19,22],[43,50]]. ===
  {
    Mat2f A;
    A(0, 0) = 1;
    A(0, 1) = 2;
    A(1, 0) = 3;
    A(1, 1) = 4;
    Mat2f B;
    B(0, 0) = 5;
    B(0, 1) = 6;
    B(1, 0) = 7;
    B(1, 1) = 8;
    Mat2f C = A ^ B;
    CHECK_NEAR(C(0, 0), 19, 1e-4);
    CHECK_NEAR(C(0, 1), 22, 1e-4);
    CHECK_NEAR(C(1, 0), 43, 1e-4);
    CHECK_NEAR(C(1, 1), 50, 1e-4);

    // identity is the multiplicative unit (catches transpose on asymmetric A)
    Mat2f AI = A ^ Mat2f();
    CHECK_NEAR(AI(0, 0), 1, 1e-4);
    CHECK_NEAR(AI(0, 1), 2, 1e-4);
    CHECK_NEAR(AI(1, 0), 3, 1e-4);
    CHECK_NEAR(AI(1, 1), 4, 1e-4);

    // matrix * column-vector
    Vec2f Av = A ^ Vec2f(5, 6); // (1*5+2*6, 3*5+4*6) = (17,39)
    CHECK_NEAR(Av.x, 17, 1e-4);
    CHECK_NEAR(Av.y, 39, 1e-4);
  }

  // === Finding #12: determinant()/inverse() (via cofactor) must be correct for
  // matrices larger than 2x2. Test the defining property M ^ M^-1 == I. ===
  {
    Mat2f M;
    M(0, 0) = 4;
    M(0, 1) = 7;
    M(1, 0) = 2;
    M(1, 1) = 6; // det = 10
    Mat2f Minv = M.inverse();
    CHECK_NEAR(Minv(0, 0), 0.6f, 1e-4);
    CHECK_NEAR(Minv(0, 1), -0.7f, 1e-4);
    CHECK_NEAR(Minv(1, 0), -0.2f, 1e-4);
    CHECK_NEAR(Minv(1, 1), 0.4f, 1e-4);
    Mat2f Mid = M ^ Minv;
    CHECK_NEAR(Mid(0, 0), 1, 1e-4);
    CHECK_NEAR(Mid(0, 1), 0, 1e-4);
    CHECK_NEAR(Mid(1, 0), 0, 1e-4);
    CHECK_NEAR(Mid(1, 1), 1, 1e-4);

    Mat3f N;
    N(0, 0) = 1;
    N(0, 1) = 2;
    N(0, 2) = 3;
    N(1, 0) = 0;
    N(1, 1) = 1;
    N(1, 2) = 4;
    N(2, 0) = 5;
    N(2, 1) = 6;
    N(2, 2) = 0; // det = 1
    Mat3f Nid = N ^ N.inverse();
    for (int r = 0; r < 3; r++)
      for (int c = 0; c < 3; c++)
        CHECK_NEAR(Nid(r, c), (r == c ? 1.0f : 0.0f), 1e-3);
  }

  // transpose, including a non-square matrix
  {
    Matrix<float, 2, 3> P;
    P(0, 0) = 1;
    P(0, 1) = 2;
    P(0, 2) = 3;
    P(1, 0) = 4;
    P(1, 1) = 5;
    P(1, 2) = 6;
    Matrix<float, 3, 2> Pt = P.transposed();
    for (int r = 0; r < 2; r++)
      for (int c = 0; c < 3; c++)
        CHECK(Pt(c, r) == P(r, c));
  }

  // makeTranslate: translation lives in the last column (M * v convention),
  // and applying it to a homogeneous point offsets it.
  {
    Mat4f T = Mat4f::makeTranslate(Vec3f(1, 2, 3));
    CHECK_NEAR(T(0, 3), 1, 1e-4);
    CHECK_NEAR(T(1, 3), 2, 1e-4);
    CHECK_NEAR(T(2, 3), 3, 1e-4);
    CHECK_NEAR(T(3, 3), 1, 1e-4);
    CHECK_NEAR(T(0, 0), 1, 1e-4);
    Vec4f tp = T ^ Vec4f(10, 20, 30, 1); // (11,22,33,1)
    CHECK_NEAR(tp.x, 11, 1e-4);
    CHECK_NEAR(tp.y, 22, 1e-4);
    CHECK_NEAR(tp.z, 33, 1e-4);
    CHECK_NEAR(tp.w, 1, 1e-4);
  }

  // === Finding #3 (cont.): data() must be a column-major buffer (OpenGL
  // expects column-major), i.e. data()[c*N + r] == M(r,c). ===
  {
    Mat3f G;
    G(0, 1) = 7.0f; // row 0, col 1
    const float *d = G.data();
    CHECK(d[1 * 3 + 0] == 7.0f); // column-major flat index of (0,1)
  }

  // === Findings #2 / #7: scalar `op matrix` free operators must not recurse
  // into themselves (they did: `return lhs + rhs;`). Element-wise semantics.
  // ===
  {
    Mat2f Z;
    Z(0, 0) = 1;
    Z(0, 1) = 2;
    Z(1, 0) = 3;
    Z(1, 1) = 4;
    Mat2f sumS = 10.0f + Z; // would stack-overflow on the old code
    CHECK_NEAR(sumS(0, 0), 11, 1e-4);
    CHECK_NEAR(sumS(1, 1), 14, 1e-4);
    Mat2f mulS = 2.0f * Z;
    CHECK_NEAR(mulS(0, 0), 2, 1e-4);
    CHECK_NEAR(mulS(1, 1), 8, 1e-4);
  }

  return imtest::report();
}
