#include <stdlib.h>
#include <stdio.h>
#include <iostream.h>
#include <math.h>

//#include "point.h"
#include "Quaternion.H"

/*
 *  These constructors reduce smoothly to the identity when the input
 *  points are the same, but will generate Realing-point errors if
 *  they are diametrically opposite and the rotation axis is ill-defined.
 */

/*
AmrQuaternion::AmrQuaternion(const spoint &s)
{
    Real c = Z(s);			//  cosine of rotation angle
    Real ch = sqrt(0.5*(1.+c));	//  cosine of half angle

    w = ch;
    x = -0.5*Y(s)/ch;    y = 0.5*X(s)/ch;    z = 0.;
}

AmrQuaternion::AmrQuaternion(const spoint &axis, Real sin_half_angle)
{
    w = sqrt( 1. - sin_half_angle*sin_half_angle );
    x = sin_half_angle*X(axis);
    y = sin_half_angle*Y(axis);
    z = sin_half_angle*Z(axis);
}
AmrQuaternion::AmrQuaternion(const spoint &s1, const spoint &s2)
{
    Real c = s1*s2;			//  cosine of rotation angle
    Real xn = Y(s1)*Z(s2) - Z(s1)*Y(s2);   //  unnormalized axis of rotation
    Real yn = Z(s1)*X(s2) - X(s1)*Z(s2);
    Real zn = X(s1)*Y(s2) - Y(s1)*X(s2);
    Real ch = sqrt(0.5*(1.+c));	//  cosine of half rotation angle

    w = ch;
    x = 0.5*xn/ch;    y = 0.5*yn/ch;    z = 0.5*zn/ch;
}


void AmrQuaternion::tomatrix( Real m[3][3] ) const
{
    Real wx=w*x, wy=w*y, wz=w*z, xz=x*z, yz=y*z, xy=x*y;
    Real ww=w*w, xx=x*x, yy=y*y, zz=z*z;

    m[0][0] = 1.-2.*(yy+zz);  m[0][1] = 2.*(xy-wz);    m[0][2] = 2.*(xz+wy);
    m[1][0] = 2.*(xy+wz);     m[1][1]= 1.-2.*(zz+xx);  m[1][2] = 2.*(yz-wx);
    m[2][0] = 2.*(xz-wy);     m[2][1] = 2.*(yz+wx);    m[2][2] = 1.-2.*(yy+xx);
}
*/

//defines the quaternion that rotates (x1,y1,z1) to (x2,y2,z2)
AmrQuaternion::AmrQuaternion(Real x1,Real y1,Real z1,Real x2,Real y2,Real z2)
{
    Real c = x1*x2+y1*y2+z1*z2;			//  cosine of rotation angle
    Real xn = y1*z2 - z1*y2;   //  unnormalized axis of rotation
    Real yn = z1*x2 - x1*z2;
    Real zn = x1*y2 - y1*x2;
    Real ch = sqrt(0.5*(1.+c));	//  cosine of half rotation angle

    w = ch;
    x = 0.5*xn/ch;    y = 0.5*yn/ch;    z = 0.5*zn/ch;
}


void AmrQuaternion::tomatrix( Real m[4][4] ) const
{
    Real wx=w*x, wy=w*y, wz=w*z, xz=x*z, yz=y*z, xy=x*y;
    Real ww=w*w, xx=x*x, yy=y*y, zz=z*z;

    m[0][0] = 1.-2.*(yy+zz);  m[0][1] = 2.*(xy-wz);    m[0][2] = 2.*(xz+wy);
    m[1][0] = 2.*(xy+wz);     m[1][1]= 1.-2.*(zz+xx);  m[1][2] = 2.*(yz-wx);
    m[2][0] = 2.*(xz-wy);     m[2][1] = 2.*(yz+wx);    m[2][2] = 1.-2.*(yy+xx);

    m[0][3] = m[1][3] = m[2][3] = 0.;
    m[3][0] = m[3][1] = m[3][2] = 0.;
    m[3][3] = 1.;
}

