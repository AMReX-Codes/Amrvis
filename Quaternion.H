// ---------------------------------------------------------------
// Quaternion.H
// ---------------------------------------------------------------
#ifndef _AMR_QUATERNION_H_
#define _AMR_QUATERNION_H_

#include <iostream>

#include <AMReX_REAL.H>
#include <Point.H>

using amrex::Real;

/***************************************************************************

A quaternion represents a rotation of a rigid body in Euclidean
three-dimensional space.  (These quaternions all are normalized
so that the sum of the squares of the components is one.)

If the rotation is counterclockwise by an angle alpha about axis n,
then the corresponding quaternion is

    [  cos(alpha/2),   sin(alpha/2) n  ]

Multiplication of normalized quaternions corresponds to composition of
these rotations:  q * r  means do rotation r first, then q.

The operator  *=  does right multiplication by the argument, so  q *= r
means  q = q*r .  This seems to be more natural for the usual applications
where r is an increment which is successively applied to a position q.

***************************************************************************/

class AmrQuaternion {
  private:
    Real w, x, y, z;

  public:
    AmrQuaternion()  { w=1.;  x=y=z=0.; }		    // identity rotation
    AmrQuaternion(Real W, Real X, Real Y, Real Z)  { w=W; x=X; y=Y; z=Z; }
    AmrQuaternion(Real, Real, Real, Real, Real, Real);
    //returns a quaternion that rotates p1 to p2
    AmrQuaternion(const AmrSpherePoint &p1, const AmrSpherePoint &p2);
    AmrQuaternion operator*(const AmrQuaternion &q) const;  // right-mult by q
    AmrQuaternion operator/(const AmrQuaternion &q) const;  // right-mult by q^-1
    AmrQuaternion operator*=(const AmrQuaternion &q);	    // right-mult by q
    AmrQuaternion operator/=(const AmrQuaternion &q);	    // right-mult by q^-1
    friend AmrQuaternion inverse(const AmrQuaternion &q);   // inverse
  
    Real InfNorm() const;
    void tomatrix( Real m[4][4] ) const;

    Real QW() const { return w; }
    Real QX() const { return x; }
    Real QY() const { return y; }
    Real QZ() const { return z; }
  
    friend std::ostream& operator<<(std::ostream &s, const AmrQuaternion &q);
};

inline AmrQuaternion AmrQuaternion::operator*(const AmrQuaternion &q) const {
  return AmrQuaternion(w*q.w - x*q.x - y*q.y - z*q.z,
		       x*q.w + w*q.x + y*q.z - z*q.y,
		       y*q.w + w*q.y + z*q.x - x*q.z,
		       z*q.w + w*q.z + x*q.y - y*q.x);
}

inline AmrQuaternion AmrQuaternion::operator/(const AmrQuaternion &q) const {
  return AmrQuaternion(w*q.w + x*q.x + y*q.y + z*q.z,
		       x*q.w - w*q.x - y*q.z + z*q.y,
		       y*q.w - w*q.y - z*q.x + x*q.z,
		       z*q.w - w*q.z - x*q.y + y*q.x);
}

inline AmrQuaternion AmrQuaternion::operator*=(const AmrQuaternion &q) {
  Real ww(w*q.w - x*q.x - y*q.y - z*q.z);
  Real xx(x*q.w + w*q.x + y*q.z - z*q.y);
  Real yy(y*q.w + w*q.y + z*q.x - x*q.z);
  Real zz(z*q.w + w*q.z + x*q.y - y*q.x);
  w = ww;  x = xx;  y = yy;  z = zz;
  return *this;
}

inline AmrQuaternion AmrQuaternion::operator/=(const AmrQuaternion &q) {
  Real ww(w*q.w + x*q.x + y*q.y + z*q.z);
  Real xx(x*q.w - w*q.x - y*q.z + z*q.y);
  Real yy(y*q.w - w*q.y - z*q.x + x*q.z);
  Real zz(z*q.w - w*q.z - x*q.y + y*q.x);
  w = ww;  x = xx;  y = yy;  z = zz;
  return *this;
}

inline AmrQuaternion inverse(const AmrQuaternion &q) {
  return AmrQuaternion( q.w, -q.x, -q.y, -q.z );
}

inline std::ostream& operator<<(std::ostream &s, const AmrQuaternion &q) {
  return s << "AmrQuaternion( " << q.w << " , " << q.x << " , "
           << q.y << " , " << q.z << " )";
}

#endif  //  ndef _AMR_QUATERNION_H_

