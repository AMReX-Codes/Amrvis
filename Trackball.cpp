// ---------------------------------------------------------------
// Trackball.cpp
// ---------------------------------------------------------------
#include <Quaternion.H>
#include <Point.H>
#include <AMReX_REAL.H>

// ---------------------------------------------------------------
static Real tb_project(Real x, Real y) {
    Real r2(x * x + y * y);
    return 1.0 / ( 1.0 + 0.5 *( r2 + 0.75 * r2 ) );
}

// ---------------------------------------------------------------
// Compute a quaternion rotation for the given two points,
// assumed to be in the interval -1 to 1
// ---------------------------------------------------------------
AmrQuaternion trackball(Real p1x, Real p1y, Real p2x, Real p2y) {
  AmrSpherePoint s1(p1x, p1y, tb_project(p1x, p1y));
  AmrSpherePoint s2(p2x, p2y, tb_project(p2x, p2y));
  return AmrQuaternion(s2,s1);
}
// ---------------------------------------------------------------
// ---------------------------------------------------------------
