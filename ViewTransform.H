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
    void SetScale(Real s) { scale = s; }
    Real GetScale(void) { return scale; }
    
    void SetTranslation(Real tx, Real ty)
    { transX = tx;  transY = ty; }
    void GetTranslation(Real &tx, Real &ty) { tx = transX;  ty = transY; }
    
    AmrQuaternion Screen2Quat(int, int, int, int, Real);
private:
    Real scale, transX, transY;
    AmrQuaternion rotation, renderRotation;
    int screenPositionX, screenPositionY;
    Real objCenterX, objCenterY, objCenterZ;
    MatrixFour mRotation, mRenderRotation;
};

ostream& operator << (ostream &, const ViewTransform &);

#endif

