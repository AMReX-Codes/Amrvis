// -------------------------------------------------------------------
// ViewTransform.H
// -------------------------------------------------------------------
#ifndef _VIEWTRANSFORM_H_
#define _VIEWTRANSFORM_H_

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iostream.h>
#include "REAL.H" 

typedef Real MatrixFour[4][4];


class ViewTransform {
  public:
	ViewTransform();
	ViewTransform(Real rHo, Real tHeta, Real pHi, Real yr, Real dist);
	void ViewTransformInit(Real rHo, Real tHeta, Real pHi,
			       Real yr, Real dist);
	~ViewTransform();
	void MakeTransform();
	// should be called after adjusting parameters
	void TransformPoint(Real x, Real y, Real z,
			    Real &pX, Real &pY, Real &pZ);
	void Print() const;

	Real GetRho()   { return rho; }
	Real GetTheta() { return theta; }
	Real GetPhi()   { return phi; }

	void SetRho(Real r)   { rho = r; }
	void SetTheta(Real t) { theta = t; }
	void SetPhi(Real p)   { phi = p; }

	void SetObjCenter(Real x, Real y, Real z) { objCenterX = x;
						    objCenterY = y;
						    objCenterZ = z; }
	void SetScreenPosition(int x, int y) { screenPositionX = x;
					       screenPositionY = y; }
	void SetScale(Real sx, Real sy, Real sz)
			{ scaleX = sx;  scaleY = sy;  scaleZ = sz; }
	void GetScale(Real &sx, Real &sy, Real &sz)
			{ sx = scaleX;  sy = scaleY;  sz = scaleZ; }

	void SetTranslation(Real tx, Real ty, Real tz)
			{ transX = tx;  transY = ty;  transZ = tz; }
	void GetTranslation(Real &tx, Real &ty, Real &tz)
			{ tx = transX;  ty = transY;  tz = transZ; }

  private:
	Real scaleX, scaleY, scaleZ, transX, transY, transZ;
	Real rho, theta, phi;
	int screenPositionX, screenPositionY;
	Real objCenterX, objCenterY, objCenterZ;
	MatrixFour mTrans, mScale, mRx, mRy, mRz;
	MatrixFour mTemp0, mTemp1, mTemp2, mTemp3;
};

ostream& operator << (ostream &, const ViewTransform &);

#endif
