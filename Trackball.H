
//
// $Id: Trackball.H,v 1.4 2000-10-02 20:53:10 lijewski Exp $
//

// ---------------------------------------------------------------
// TrackBall.H
// ---------------------------------------------------------------
/*
 * Pass the x and y coordinates of the last and current positions of
 * the mouse, scaled so they are from (-1.0 ... 1.0).
 *
 * The resulting rotation is returned as a quaternion rotation 
 */

extern AmrQuaternion trackball( Real p1x, Real p1y, Real p2x, Real p2y);
