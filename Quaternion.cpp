// ---------------------------------------------------------------
// Quaternion.cpp
// ---------------------------------------------------------------
#include <cstdlib>
#include <cmath>

#include <Quaternion.H>

// ---------------------------------------------------------------
//  These constructors reduce smoothly to the identity when the input
//  points are the same, but will generate Realing-point errors if
//  they are diametrically opposite and the rotation axis is ill-defined.
// ---------------------------------------------------------------


// ---------------------------------------------------------------
AmrQuaternion::AmrQuaternion(const AmrSpherePoint &s1, 
                             const AmrSpherePoint &s2)
{
  Real c(s1 * s2);			   // cosine of rotation angle
  Real xn(Y(s1) * Z(s2) - Z(s1) * Y(s2));  // unnormalized axis of rotation
  Real yn(Z(s1) * X(s2) - X(s1) * Z(s2));
  Real zn(X(s1) * Y(s2) - Y(s1) * X(s2));
  Real ch(sqrt(0.5 * (1.0 + c)));	   // cosine of half rotation angle
  Real chTemp(0.5 / ch);
  
  w = ch;
  x = xn * chTemp;
  y = yn * chTemp;
  z = zn * chTemp;
}


// ---------------------------------------------------------------
// defines the quaternion that rotates (x1,y1,z1) to (x2,y2,z2)
// ---------------------------------------------------------------
AmrQuaternion::AmrQuaternion(Real x1,Real y1,Real z1,Real x2,Real y2,Real z2) {
  Real c(x1 * x2 + y1 * y2 + z1 * z2);	// cosine of rotation angle
  Real xn(y1 * z2 - z1 * y2);           // unnormalized axis of rotation
  Real yn(z1 * x2 - x1 * z2);
  Real zn(x1 * y2 - y1 * x2);
  Real ch(sqrt(0.5 * (1.0 + c)));	// cosine of half rotation angle
  Real chTemp(0.5 / ch);
  
  w = ch;
  x = xn * chTemp;
  y = yn * chTemp;
  z = zn * chTemp;
}


// ---------------------------------------------------------------
void AmrQuaternion::tomatrix( Real m[4][4] ) const {
  Real wx(w * x), wy(w * y), wz(w * z);
  Real xz(x * z), yz(y * z), xy(x * y);
  Real xx(x * x), yy(y * y), zz(z * z);
  
  m[0][0] = 1.-2.*(yy+zz);  m[0][1] = 2.*(xy-wz);    m[0][2] = 2.*(xz+wy);
  m[1][0] = 2.*(xy+wz);     m[1][1]= 1.-2.*(zz+xx);  m[1][2] = 2.*(yz-wx);
  m[2][0] = 2.*(xz-wy);     m[2][1] = 2.*(yz+wx);    m[2][2] = 1.-2.*(yy+xx);
  
  m[0][3] = m[1][3] = m[2][3] = 0.0;
  m[3][0] = m[3][1] = m[3][2] = 0.0;
  m[3][3] = 1.0;
}


// ---------------------------------------------------------------
Real AmrQuaternion::InfNorm() const {
  Real mat[4][4];
  this->tomatrix(mat);
  Real max(0.0);
  Real sum(0.0);
  for(int i(0); i < 3; ++i) {
    sum = mat[i][0] + mat[i][1] + mat[i][2];
    max = (sum > max ? sum : max );
  }
  return max;
}
// ---------------------------------------------------------------
// ---------------------------------------------------------------
