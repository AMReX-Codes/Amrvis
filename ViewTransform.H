//BL_COPYRIGHT_NOTICE

//
// $Id: ViewTransform.H,v 1.10 1998-10-27 18:16:38 lijewski Exp $
//

#ifndef _VIEWTRANSFORM_H_
#define _VIEWTRANSFORM_H_

#ifdef BL_USE_NEW_HFILES
#include <iostream>
using std::iostream;
#else
#include <iostream.h>
#endif

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
    void ViewRenderRotationMat() const;
  
    void SetAspect(Real apct) { vtAspect = apct; }
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
    void SetAdjustments(Real len, int width, int height);
  
    void MakeTranslation(int x1, int y1, int x2, int y2, Real bS);
    void ResetTranslation()
      { boxTransX = renTransX = boxTransY = renTransY = 0.; }
    Real GetTranslationX() { return boxTransX; }
    Real GetTranslationY() { return boxTransY; }
  
    Real InfNorm();
    AmrQuaternion Screen2Quat(int, int, int, int, Real);
    void SetAdjust(Real txA, Real tyA) { txAdjust = txA; tyAdjust = tyA; }

  private:
    Real scale, boxTransX, boxTransY, renTransX, renTransY;
    AmrQuaternion rotation, renderRotation;
    int screenPositionX, screenPositionY;
    Real objCenterX, objCenterY, objCenterZ;
    MatrixFour mRotation, mRenderRotation;
    Real txAdjust, tyAdjust;
    Real vtAspect;
};

ostream& operator << (ostream &, const ViewTransform &);

#endif

