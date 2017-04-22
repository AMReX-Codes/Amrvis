// ---------------------------------------------------------------
// TrackBall.H
// ---------------------------------------------------------------

using amrex::Real;

// ---------------------------------------------------------------
// Pass the x and y coordinates of the last and current positions of
// the mouse, scaled so they are from (-1.0 ... 1.0).
//
// The resulting rotation is returned as a quaternion rotation 
// ---------------------------------------------------------------

extern AmrQuaternion trackball( Real p1x, Real p1y, Real p2x, Real p2y);

// ---------------------------------------------------------------
// ---------------------------------------------------------------
