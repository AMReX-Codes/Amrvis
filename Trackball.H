
//
// $Id: Trackball.H,v 1.5 2002-02-07 23:59:02 vince Exp $
//

// ---------------------------------------------------------------
// TrackBall.H
// ---------------------------------------------------------------

// ---------------------------------------------------------------
// Pass the x and y coordinates of the last and current positions of
// the mouse, scaled so they are from (-1.0 ... 1.0).
//
// The resulting rotation is returned as a quaternion rotation 
// ---------------------------------------------------------------

extern AmrQuaternion trackball( Real p1x, Real p1y, Real p2x, Real p2y);

// ---------------------------------------------------------------
// ---------------------------------------------------------------
