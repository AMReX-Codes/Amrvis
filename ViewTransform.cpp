// ---------------------------------------------------------------
// ViewTransform.cpp
// ---------------------------------------------------------------
#include <ViewTransform.H>
#include <Trackball.H>

#include <iostream>
#include <cmath>
using std::endl;
using std::cout;

// -------------------------------------------------------------------
ViewTransform::ViewTransform() {
  rotation = AmrQuaternion();
  renderRotation = AmrQuaternion();
  screenPositionX = 0;
  screenPositionY = 0;
  objCenterX = 0.0;
  objCenterY = 0.0;
  objCenterZ = 0.0;
  scale      = 1.0;
  boxTransX  = 0.0;
  boxTransY  = 0.0;
  renTransX  = 0.0;
  renTransY  = 0.0;
  txAdjust   = 1.0;
  tyAdjust   = 1.0;
  MakeTransform();
}


// -------------------------------------------------------------------
ViewTransform::~ViewTransform() {
}


//--------------------------------------------------------------------
AmrQuaternion ViewTransform::Screen2Quat(int startX, int startY, 
                                         int endX, int endY, Real boxSize)
{
  Real xWorldStart((Real)(startX - screenPositionX) / (screenPositionX * scale));
  Real yWorldStart((Real)(startY - screenPositionY) / (screenPositionY * scale));
  Real xWorldEnd((Real)(endX     - screenPositionX) / (screenPositionX * scale));
  Real yWorldEnd((Real)(endY     - screenPositionY) / (screenPositionY * scale));
  Real oobs(1.0 / boxSize);
  return trackball(xWorldStart * oobs, yWorldStart * oobs,
                   xWorldEnd   * oobs, yWorldEnd   * oobs);
}

//--------------------------------------------------------------------
void ViewTransform::MakeTranslation(int start_X, int start_Y, 
                                    int end_X, int end_Y, Real /*box_size*/)
{
  // norm coord. onto [-1,1] by [-1,1] grid
  Real xWorldStart((Real)(start_X) / (screenPositionX));
  Real yWorldStart((Real)(start_Y) / (screenPositionY));
  Real xWorldEnd((Real)(end_X) / (screenPositionX));
  Real yWorldEnd((Real)(end_Y) / (screenPositionY));
  renTransX += (xWorldEnd - xWorldStart);
  renTransY += (yWorldEnd - yWorldStart);

  // that isn't right for the boxes
  boxTransX += (end_X - start_X);
  boxTransY += (start_Y - end_Y);
}

// -------------------------------------------------------------------
void ViewTransform::TransformPoint(Real x, Real y, Real z,
				   Real &pX, Real &pY, Real &/*pZ*/)
{
  //apply rotation to (x, y, z, 1);
  Real scaleVector[4] = {scale, scale, scale, 1.};
  Real point[4] = {x - objCenterX, y - objCenterY, z - objCenterZ, 1};
  Real newPoint[4];
  for(int ii(0); ii < 2; ++ii) {
    int temp(0);
    for(int jj(0); jj < 4; ++jj) {
      temp += (int) (point[jj] * mRotation[ii][jj] * scaleVector[jj]);
    }
    newPoint[ii] = temp;
  }
  pX = newPoint[0] + screenPositionX;
  pY = newPoint[1] + screenPositionY;
  //pZ = newPoint[2];
}


// -------------------------------------------------------------------
void ViewTransform::Print() const {
    cout << "ViewTransform::Print() does nothing now." << endl;
}


// -------------------------------------------------------------------
void ViewTransform::MakeTransform() {
  rotation.tomatrix(mRotation);
  mRotation[0][3] = boxTransX * 0.185;
  mRotation[1][3] = boxTransY * 0.185;
  renderRotation.tomatrix(mRenderRotation);
  mRenderRotation[0][3] = (renTransX) * txAdjust * 0.185;
  mRenderRotation[1][3] = (renTransY) * tyAdjust * 0.185;
}


// -------------------------------------------------------------------
void ViewTransform::SetAdjustments(Real len, int width, int height) {
  if(width < height) {
    txAdjust = len * vtAspect;
    tyAdjust = len;
  } else {
    txAdjust = len;
    tyAdjust = len * vtAspect;
  }
}


// -------------------------------------------------------------------
Real ViewTransform::InfNorm() {
  Real mat[4][4];
  rotation.tomatrix(mat);
  // compute the L-inf matrix norm:
  Real sum(0.0);
  Real max(0.0);
  for(int i(0); i < 3; ++i) {
    sum = fabs(mat[i][0]) + fabs(mat[i][1]) + fabs(mat[i][2]);
    max = (sum > max ? sum : max);
  }
  return max;
}

// -------------------------------------------------------------------
void ViewTransform::GetRotationMat(MatrixFour m) {
  for(int i(0); i < 4; ++i) {
    for(int j(0); j < 4; ++j) {
      m[i][j] = mRotation[i][j];
    }
  }
}
    

// -------------------------------------------------------------------
void ViewTransform::GetRenderRotationMat(MatrixFour m) {
  for(int i(0); i < 4; ++i) {
    for(int j(0); j < 4; ++j) {
      m[i][j] = mRenderRotation[i][j];
    }
  }
}
    

// -------------------------------------------------------------------
void ViewTransform::ViewRotationMat() const {
  cout << "Rotation matrix:" << endl;
  for(int i(0); i < 4; ++i) {
    for(int j(0); j < 4; ++j) {
      cout << mRotation[i][j] << "  ";
    }
    cout << endl;
  }
}


// -------------------------------------------------------------------
void ViewTransform::ViewRenderRotationMat() const {
  cout << "Render Rotation matrix:" << endl;
  for(int i(0); i < 4; ++i) {
    for(int j(0); j < 4; ++j) {
      cout << mRenderRotation[i][j] << "  ";
    }
    cout << endl;
  }
}


// -------------------------------------------------------------------
std::ostream& operator << (std::ostream &o, const ViewTransform &v) {
  v.Print();
  return o;
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
