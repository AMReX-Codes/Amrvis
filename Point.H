// ---------------------------------------------------------------
// Point.H
// ---------------------------------------------------------------
#ifndef _AMR_POINT_H_
#define _AMR_POINT_H_

#include <iostream>
#include <cmath>

#include <AMReX_REAL.H>

using amrex::Real;

class AmrQuaternion;
class AmrSpherePoint;

//----------------------------------------------------------------------------
//				CLASS AMRVECTOR
//
//   An unnormalized AmrVector in three dimensions

class AmrVector
{
  Real x, y, z;
public:
  AmrVector();				// all components set to zero
  AmrVector(Real X, Real Y, Real Z);	// specified components
  AmrVector(const AmrSpherePoint &S);
  
  friend Real X(const AmrVector &v);		//  x-component
  friend Real Y(const AmrVector &v);		//  y-component
  friend Real Z(const AmrVector &v);		//  z-component
  
  AmrVector operator +(AmrVector v) const		//  Point addition
  { return AmrVector( x + v.x, y + v.y, z + v.z ); }
  AmrVector operator +=(AmrVector v)
  { x += v.x;  y += v.y; z += v.z;   return *this; }
  AmrVector operator -(AmrVector v) const		//  Point subtraction
  { return AmrVector( x - v.x, y - v.y, z - v.z ); }
  AmrVector operator -=(AmrVector v)
  { x -= v.x;  y -= v.y; z -= v.z;	return *this; }
  AmrVector operator +() const		//  Unary + (no-op)
  { return *this; }
  AmrVector operator -() const		//  Unary - (negation)
  { return AmrVector( -x, -y, -z ); }
    Real operator *(AmrVector v) const		//  Dot product
  { return x*v.x + y*v.y + z*v.z; }
  AmrVector operator %(AmrVector v) const		//  Cross product
  { return AmrVector( y*v.z-z*v.y, z*v.x-x*v.z, x*v.y - y*v.x ); }
  
  AmrVector operator *(Real r) const		//  Scalar multiplication
  { return AmrVector( x*r, y*r, z*r ); }
  AmrVector operator *=(Real r)
  { x *= r;  y *= r;  z *= r;	return *this; }
  friend AmrVector operator *(Real, AmrVector);
  
  AmrVector operator /(Real r) const		//  Scalar division
  { return AmrVector( x/r, y/r, z/r ); }
  AmrVector operator /=(Real r)
  { x /= r;  y /= r;  z /= r;	return *this; }

  AmrVector applyMatrix(Real m[4][4]);

  friend Real mag(const AmrVector &);	//  Length
  friend Real mag2(const AmrVector &);	//  Length squared (no sqrt)
  friend Real dist(const AmrVector &p,const AmrVector &q);
  friend Real dist2(const AmrVector &p,const AmrVector &q);

    //  Triple product:  positive if (p1,p2,p3) are right-handed coord system
    //friend Real triple( AmrVector v1, AmrVector v2, AmrVector v3 );

  friend std::ostream& operator<<(std::ostream&s, const AmrVector &p);
  
  friend class AmrSpherePoint;
};

//----------------------------------------------------------------------------
//				CLASS AMRSPHEREPOINT
//
//  A 3-D point, constrained to be on the surface of the unit sphere.
//  If x=y=z=0 it is the null point, does not exist

class AmrSpherePoint
{
  Real x, y, z;
public:
  AmrSpherePoint();				// null point
  AmrSpherePoint(Real X, Real Y, Real Z);	// specified components
  AmrSpherePoint(const AmrVector &v);		// normalize it

  friend Real X(const AmrSpherePoint &v);	//  x-component
  friend Real Y(const AmrSpherePoint &v);	//  y-component
  friend Real Z(const AmrSpherePoint &v);	//  z-component
  friend int isnull(const AmrSpherePoint &v);

  AmrVector operator -(AmrSpherePoint v) const		//  Spoint subtraction
    { return AmrVector( x - v.x, y - v.y, z - v.z ); }
  AmrSpherePoint operator +() const		//  Unary + (no-op)
    { return *this; }
  AmrSpherePoint operator -() const		//  Unary - (negation)
    { return AmrSpherePoint( -x, -y, -z ); }
  Real operator *(const AmrSpherePoint &v) const	//  Dot product
    { return x*v.x + y*v.y + z*v.z; }
  AmrSpherePoint operator %(const AmrSpherePoint &v) const	//  Normalized cross product
    { return AmrSpherePoint( y*v.z-z*v.y, z*v.x-x*v.z, x*v.y - y*v.x ); }

  AmrSpherePoint applyMatrix(Real m[4][4]);
  AmrSpherePoint rotate( const AmrQuaternion &Q );

  friend Real mag(const AmrSpherePoint &p);	//  1
  friend Real mag2(const AmrSpherePoint &p);
  friend Real dist(const AmrSpherePoint &p,const AmrSpherePoint &q);
  friend Real dist2(const AmrSpherePoint &p,const AmrSpherePoint &q);

  friend AmrSpherePoint midpt(const AmrSpherePoint &P, const AmrSpherePoint &Q);
  friend AmrSpherePoint midpt(const AmrSpherePoint &P, const AmrSpherePoint &Q, const AmrSpherePoint &R);
/*
    //  Intersection of two lines with poles p1, p2
  AmrSpherePoint( const AmrSpherePoint &p1, const AmrSpherePoint &p2, int hand=0 );
    //  Intersection of line with segment p1-p2
  AmrSpherePoint( const AmrSpherePoint &pole, const AmrSpherePoint &p1, const AmrSpherePoint &p2 );
    //  Intersection of line (pole=p) with circle (ctr,c,s)
  AmrSpherePoint( const AmrSpherePoint &p, const AmrSpherePoint &ctr, Real c, Real s, int hand=0 );
    //  Intersection of segment p1-p2 with segment p3-p4
  AmrSpherePoint(const AmrSpherePoint &p1,const AmrSpherePoint &p2,const AmrSpherePoint &p3,const AmrSpherePoint &p4);
    //  Intersection of segment p1-p2 with circle (ctr,c,s)
  AmrSpherePoint( const AmrSpherePoint &p1, const AmrSpherePoint &p2,
	const AmrSpherePoint &ctr, Real c, Real s, int hand=0 );
    //  Intersection of two circles
  AmrSpherePoint( const AmrSpherePoint &ctr1, Real c1, Real s1,
	    const AmrSpherePoint &ctr2, Real c2, Real s2, int hand=0 );

    //  We trust the interpolation routines
  friend int seginterp( const AmrSpherePoint &p1, const AmrSpherePoint &p2, Real h, AmrSpherePoint p[]);
  friend int lininterp( const AmrSpherePoint &pole, Real h, AmrSpherePoint p[] );
  friend int circinterp(const AmrSpherePoint &ctr, Real c, Real s, Real h, AmrSpherePoint p[]);
  */

  friend class AmrVector;

    //void draw() const;
    //void draw(Real s ) const;
  

  friend std::ostream& operator<<(std::ostream&s, const AmrSpherePoint &p);
};

//----------------------------------------------------------------------------

inline Real hypot2(Real x, Real y, Real z)	{ return x*x + y*y + z*z; }
inline Real hypot(Real x, Real y, Real z)	{ return sqrt(hypot2(x,y,z)); }

inline AmrVector::AmrVector()	{ x = y = z = 0.; }
inline AmrSpherePoint::AmrSpherePoint()	{ x = y = z = 0.; }

inline Real X(const AmrVector &v)	{ return v.x; }
inline Real Y(const AmrVector &v)	{ return v.y; }
inline Real Z(const AmrVector &v)	{ return v.z; }
inline Real X(const AmrSpherePoint &P)	{ return P.x; }
inline Real Y(const AmrSpherePoint &P)	{ return P.y; }
inline Real Z(const AmrSpherePoint &P)	{ return P.z; }
inline int isnull(const AmrSpherePoint &P)  { return P.x==0. && P.y==0. && P.z==0.; }

inline AmrVector operator *(Real r, AmrVector v)  { return AmrVector( r*v.x, r*v.y, r*v.z );}

inline Real mag(const AmrVector &v)	{ return hypot(v.x,v.y,v.z); }
inline Real mag2(const AmrVector &v)	{ return hypot2(v.x,v.y,v.z); }
inline Real mag(const AmrSpherePoint & /*v*/)	{ return 1.; }
inline Real mag2(const AmrSpherePoint & /*v*/)	{ return 1.; }

inline Real dist(const AmrVector &p, const AmrVector &q)
	{ return hypot(p.x-q.x,p.y-q.y,p.z-q.z); }
inline Real dist2(const AmrVector &p, const AmrVector &q)
	{ return hypot2(p.x-q.x,p.y-q.y,p.z-q.z); }
inline Real dist(const AmrSpherePoint &p, const AmrSpherePoint &q)
	{ return hypot(p.x-q.x,p.y-q.y,p.z-q.z); }
inline Real dist2(const AmrSpherePoint &p, const AmrSpherePoint &q)
	{ return hypot2(p.x-q.x,p.y-q.y,p.z-q.z); }

inline AmrVector midpt( const AmrVector &P, const AmrVector &Q, Real r=0.5 )
{   return P + r*(Q-P); }
inline AmrSpherePoint midpt( const AmrSpherePoint &P, const AmrSpherePoint &Q )
{   return AmrSpherePoint( P.x+Q.x, P.y+Q.y, P.z+Q.z ); }
inline AmrSpherePoint midpt( const AmrSpherePoint &P, const AmrSpherePoint &Q, const AmrSpherePoint &R  )
{   return AmrSpherePoint( P.x+Q.x+R.x, P.y+Q.y+R.y, P.z+Q.z+R.z ); }

/*inline void AmrSpherePoint::draw() const
{  
  glNormal3f( x, y, z );	
  glVertex3f( scale*x, scale*y, scale*z ); }
inline void AmrSpherePoint::draw(Real s ) const
{   glNormal3f( x, y, z );	
    s *= scale; 
    glVertex3f( s*x, s*y, s*z ); }
*/
inline std::ostream& operator<<( std::ostream&s, const AmrVector &p )
  { return s << "AmrVector(" << p.x << "," << p.y << "," << p.z << ")"; }
inline std::ostream& operator<<( std::ostream&s, const AmrSpherePoint &p)
  { return s << "AmrSpherePoint( " << p.x << " , " << p.y << " , " << p.z << " )"; }

#endif

