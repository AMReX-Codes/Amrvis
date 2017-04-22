// ---------------------------------------------------------------
// ViewTransform.H
// ---------------------------------------------------------------
#ifndef _VIEWTRANSFORM_H_
#define _VIEWTRANSFORM_H_

#include <iostream>

#include <AMReX_REAL.H>
#include <Quaternion.H>

using amrex::Real;

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
    AmrQuaternion GetRotation() const { return rotation; }
    void SetRotation(AmrQuaternion quat) { rotation = quat; }
    AmrQuaternion GetRenderRotation() { return renderRotation; }
    void SetRenderRotation(AmrQuaternion quat) { renderRotation = quat; }
  
    void SetObjCenter(const Real x, const Real y, const Real z) 
      { objCenterX = x; objCenterY = y; objCenterZ = z; }
    void SetScreenPosition(int x, int y) 
      { screenPositionX = x; screenPositionY = y; }
    void SetScale(Real s) { scale = s; }
    Real GetScale(void) { return scale; }
    void SetAdjustments(Real len, int width, int height);
  
    void MakeTranslation(int x1, int y1, int x2, int y2, Real bS);
    void ResetTranslation() { boxTransX = renTransX = boxTransY = renTransY = 0.0; }
    Real GetRenTransX() const { return renTransX; }
    Real GetRenTransY() const { return renTransY; }
    void SetRenTransX(const Real newRenTransX) { renTransX = newRenTransX; }
    void SetRenTransY(const Real newRenTransY) { renTransY = newRenTransY; }
  
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

std::ostream& operator << (std::ostream &, const ViewTransform &);

#endif

