// -------------------------------------------------------------------
// ViewTransform.C
// -------------------------------------------------------------------
#include "ViewTransform.H"

// ------------------------------------------------- utility functions
// -------------------------------------------------------------------
Real DegToRad(Real angle) {
  return (angle * M_PI/180.0);
}

// -------------------------------------------------------------------
Real RadToDeg(Real angle) {
  return (angle * 180.0/M_PI);
}

// -------------------------------------------------------------------
void multM4(MatrixFour &mAB, MatrixFour &mA, MatrixFour &mB) {  // 4x4
  int i,j;
  for(i=0; i<4; i++) {
    for(j=0; j<4; j++) {
      mAB[i][j] = mA[i][0] * mB[0][j] + mA[i][1] * mB[1][j] +
                  mA[i][2] * mB[2][j] + mA[i][3] * mB[3][j];
    }
  }
}


// -------------------------------------------------------------------
// -------------------------------------------------------------------
ViewTransform::ViewTransform() {
  ViewTransformInit(0.0, 0.0, 0.0, 0.0, 0.0);
}


// -------------------------------------------------------------------
ViewTransform::ViewTransform(Real rHo, Real tHeta, Real pHi, Real yr,
				Real dist)
{
  ViewTransformInit(rHo, tHeta, pHi, yr, dist);
}


// -------------------------------------------------------------------
void ViewTransform::ViewTransformInit(Real rHo, Real tHeta, Real pHi,
				      Real yr, Real dist)
{
  int i, j;

  for (i=0; i<4; i++) {
    for (j=0; j<4; j++) {
      if(i==j) {
        mScale[i][j]  = 1.0;
        mTrans[i][j]  = 1.0;
        mRx[i][j] = 1.0;
        mRy[i][j] = 1.0;
        mRz[i][j] = 1.0;
      } else {
        mScale[i][j]  = 0.0;
        mTrans[i][j]  = 0.0;
        mRx[i][j] = 0.0;
        mRy[i][j] = 0.0;
        mRz[i][j] = 0.0;
      }
      mTemp1[i][j] = 0.0;
      mTemp2[i][j] = 0.0;
      mTemp3[i][j] = 0.0;
    }
  }
  rho = rHo;
  theta = tHeta;
  phi = pHi;
  screenPositionX = 0;
  screenPositionY = 0;
  objCenterX = 0.0;
  objCenterY = 0.0;
  objCenterZ = 0.0;
  scaleX = 1.0;
  scaleY = 1.0;
  scaleZ = 1.0;
  transX = 0.0;
  transY = 0.0;
  transZ = 0.0;
  MakeTransform();
}


// -------------------------------------------------------------------
ViewTransform::~ViewTransform() {
}


// -------------------------------------------------------------------
void ViewTransform::TransformPoint(Real x, Real y, Real z,
				   Real &pX, Real &pY, Real &pZ)
{
  for(int ii=0; ii<4; ii++) {
    for(int jj=0; jj<4; jj++) {
      mTemp0[ii][jj] = 0.0;
    }
  }
  mTemp0[0][0] = x - objCenterX;
  mTemp0[0][1] = y - objCenterY;
  mTemp0[0][2] = z - objCenterZ;
  mTemp0[0][3] = 1.0;

  multM4(mTemp1, mRx, mRy);
  multM4(mTemp2, mTemp1, mRz);
  multM4(mTemp3, mTemp2, mScale);

  multM4(mTemp2, mTemp0, mTemp3);

  pX = mTemp2[0][0] + screenPositionX;
  pY = mTemp2[0][1] + screenPositionY;
  pZ = mTemp2[0][2];

}


// -------------------------------------------------------------------
void ViewTransform::MakeTransform() {
  mScale[0][0] = scaleX;
  mScale[1][1] = scaleY;
  mScale[2][2] = scaleZ;

  mRx[1][1] = cos(rho);
  mRx[2][2] = cos(rho);
  mRx[1][2] = sin(rho);
  mRx[2][1] =-sin(rho);

  mRy[0][0] = cos(theta);
  mRy[2][2] = cos(theta);
  mRy[0][2] = sin(theta);
  mRy[2][0] =-sin(theta);

  mRz[0][0] = cos(phi);
  mRz[1][1] = cos(phi);
  mRz[0][1] = sin(phi);
  mRz[1][0] =-sin(phi);
}


// -------------------------------------------------------------------
void ViewTransform::Print() const {
  int i, j;
  printf("%s\n", "transform matrix");
  for (i=0; i<4; i++) {
    for (j=0; j<4; j++) {
      //printf("%7.2f", transform[i][j]);
    }
    //printf("\n");
  }
}


// -------------------------------------------------------------------
ostream& operator << (ostream &o, const ViewTransform &v) {
  v.Print();
  return o;
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
