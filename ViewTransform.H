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
#include "Quaternion.H"

typedef Real MatrixFour[4][4];


class ViewTransform {
public:
    ViewTransform();
    ~ViewTransform();
    void MakeTransform();
    // should be called after adjusting parameters
    void TransformPoint(Real x, Real y, Real z,
                        Real &pX, Real &pY, Real &pZ);

    void Print() const;

    void GetRotationMat(MatrixFour m);
    void ViewRotationMat() const;
    void GetRenderRotationMat(MatrixFour m);
    
    AmrQuaternion GetRotation() { return rotation; }
    void SetRotation(AmrQuaternion quat) { rotation = quat; }
    AmrQuaternion GetRenderRotation() { return renderRotation; }
    void SetRenderRotation(AmrQuaternion quat) { renderRotation = quat; }

    void SetObjCenter(Real x, Real y, Real z) 
    { objCenterX = x; objCenterY = y; objCenterZ = z; }
    void SetScreenPosition(int x, int y) 
    { screenPositionX = x; screenPositionY = y; }
    void SetScale(Real sx, Real sy, Real sz)
    { scaleX = sx;  scaleY = sy;  scaleZ = sz; }
    void GetScale(Real &sx, Real &sy, Real &sz)
    { sx = scaleX;  sy = scaleY;  sz = scaleZ; }
    
    void SetTranslation(Real tx, Real ty, Real tz)
    { transX = tx;  transY = ty;  transZ = tz; }
    void GetTranslation(Real &tx, Real &ty, Real &tz)
			{ tx = transX;  ty = transY;  tz = transZ; }
    
    AmrQuaternion Screen2Quat(int, int, int, int);
private:
    Real scaleX, scaleY, scaleZ, transX, transY, transZ;
    AmrQuaternion rotation, renderRotation;
    int screenPositionX, screenPositionY;
    Real objCenterX, objCenterY, objCenterZ;
    MatrixFour mRotation, mRenderRotation;
};

ostream& operator << (ostream &, const ViewTransform &);

#endif
