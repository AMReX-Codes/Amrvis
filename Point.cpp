// ---------------------------------------------------------------
// Point.cpp
// ---------------------------------------------------------------
#include <cstdlib>
#include <cmath>

#include <Point.H>

Real scale = 1.0;


// ---------------------------------------------------------------
AmrVector::AmrVector(Real X, Real Y, Real Z)
          : x(X), y(Y), z(Z)
{
}


// ---------------------------------------------------------------
AmrVector::AmrVector(const AmrSpherePoint &S)
          : x(S.x), y(S.y), z(S.z)
{
}


// ---------------------------------------------------------------
AmrSpherePoint::AmrSpherePoint(Real X, Real Y, Real Z) {
    Real m(X * X + Y * Y + Z * Z);
    if(m == 0.0) {
      x = y = z = 0.0;
      return;
    }
    Real oneOverSqrtM(1.0 / sqrt(m));
    x = X * oneOverSqrtM;
    y = Y * oneOverSqrtM;
    z = Z * oneOverSqrtM;
}


// ---------------------------------------------------------------
AmrSpherePoint::AmrSpherePoint(const AmrVector &v) {
    Real m(v.x * v.x + v.y * v.y + v.z * v.z);
    if(m == 0.0) {
      x = y = z = 0.0;
      return;
    }
    Real oneOverSqrtM(1.0 / sqrt(m));
    x = v.x * oneOverSqrtM;
    y = v.y * oneOverSqrtM;
    z = v.z * oneOverSqrtM;
}


// ---------------------------------------------------------------
AmrVector AmrVector::applyMatrix(Real m[4][4]) {
  Real X(x * m[0][0] + m[1][0] * y + z * m[2][0]);
  Real Y(x * m[0][1] + m[1][1] * y + z * m[2][1]);
  Real Z(x * m[0][2] + m[1][2] * y + z * m[2][2]);

  return AmrVector(X, Y, Z);
}


// ---------------------------------------------------------------
AmrSpherePoint AmrSpherePoint::applyMatrix(Real m[4][4]) {
  Real X(x * m[0][0] + m[1][0] * y + z * m[2][0]);
  Real Y(x * m[0][1] + m[1][1] * y + z * m[2][1]);
  Real Z(x * m[0][2] + m[1][2] * y + z * m[2][2]);

  return AmrSpherePoint(X, Y, Z);
}
// ---------------------------------------------------------------
// ---------------------------------------------------------------
