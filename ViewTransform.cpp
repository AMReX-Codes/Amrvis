// -------------------------------------------------------------------
// ViewTransform.C
// -------------------------------------------------------------------
#include "ViewTransform.H"
#include "Trackball.H"
#include <assert.h>

// -------------------------------------------------------------------
ViewTransform::ViewTransform()
{

  rotation = AmrQuaternion();
  renderRotation = AmrQuaternion();
  screenPositionX = 0;
  screenPositionY = 0;
  objCenterX = 0.0;
  objCenterY = 0.0;
  objCenterZ = 0.0;
  scale = 1.0;
  boxTransX = renTransX = 0.0;
  boxTransY = renTransY = 0.0;
  txAdjust = tyAdjust = 1.;
  MakeTransform();
}


// -------------------------------------------------------------------
ViewTransform::~ViewTransform() {
}


//--------------------------------------------------------------------
AmrQuaternion ViewTransform::Screen2Quat(int start_X, int start_Y, 
                                         int end_X, int end_Y, Real box_size) {
    Real xWorldStart = (Real)(start_X-screenPositionX)/
        (screenPositionX*scale);
    Real yWorldStart = (Real)(start_Y-screenPositionY)/
        (screenPositionY*scale);
    Real xWorldEnd = (Real)(end_X-screenPositionX)/(screenPositionX*scale);
    Real yWorldEnd = (Real)(end_Y-screenPositionY)/(screenPositionY*scale);
    return trackball(xWorldEnd/box_size, yWorldEnd/box_size,
                     xWorldStart/box_size, yWorldStart/box_size);
}

void ViewTransform::MakeTranslation(int start_X, int start_Y, 
                                    int end_X, int end_Y, Real box_size) {
    //norm coord. onto [-1,1] by [-1,1] grid
    Real xWorldStart = (Real)(start_X)/
        (screenPositionX);
    Real yWorldStart = (Real)(start_Y)/
        (screenPositionY);
    Real xWorldEnd = (Real)(end_X)/(screenPositionX);
    Real yWorldEnd = (Real)(end_Y)/(screenPositionY);
    renTransX += (xWorldEnd - xWorldStart);
    renTransY += (yWorldEnd - yWorldStart);
    // that isn't right for the boxes
    boxTransX += (end_X - start_X);
    boxTransY += (start_Y - end_Y);
}

// -------------------------------------------------------------------
void ViewTransform::TransformPoint(Real x, Real y, Real z,
				   Real &pX, Real &pY, Real &pZ)
{
//apply rotation to (x, y, z, 1);
    Real scaleVector[4] = {scale, scale, scale, 1.};
    Real point[4]= {x-objCenterX,y-objCenterY,z-objCenterZ,1};
    Real newPoint[4];
    for(int ii=0; ii<2; ii++) {
//save time -- only the first two numbers, I think
        int temp = 0;
        for(int jj=0; jj<4; jj++) {
            temp += point[jj]*mRotation[ii][jj]*scaleVector[jj];
        }
        newPoint[ii] = temp;
    }
    pX = newPoint[0] + screenPositionX;
    pY = newPoint[1] + screenPositionY;
//    pZ = newPoint[2];
}


// -------------------------------------------------------------------
void ViewTransform::Print()const {
    cout<<"ViewTransform::Print() does nothing now"<<endl;
}



// -------------------------------------------------------------------
void ViewTransform::MakeTransform() {//ugh -- why this constant?
  rotation.tomatrix(mRotation);
  mRotation[0][3] = boxTransX*0.185;
  mRotation[1][3] = boxTransY*0.185;
  renderRotation.tomatrix(mRenderRotation);
  mRenderRotation[0][3] = (renTransX)*txAdjust*0.185;
  mRenderRotation[1][3] = (renTransY)*tyAdjust*0.185;
}

void ViewTransform::SetAdjustments(Real len, int width, int height) {
    if (width < height) {
        txAdjust = len*vtAspect;
        tyAdjust = len;
    } else {
        txAdjust = len;
        tyAdjust = len*vtAspect;
    }
}



// -------------------------------------------------------------------
void ViewTransform::GetRotationMat(MatrixFour m) {
    for(int i = 0; i< 4; i++) {
        for (int j = 0; j<4 ; j++) {
            m[i][j] = mRotation[i][j];
        }
    }
}
    

// -------------------------------------------------------------------
void ViewTransform::GetRenderRotationMat(MatrixFour m) {
    for(int i = 0; i< 4; i++) {
        for (int j = 0; j<4 ; j++) {
            m[i][j] = mRenderRotation[i][j];
        }
    }
}
    

// -------------------------------------------------------------------
void ViewTransform::ViewRotationMat() const {
  int i, j;
  printf("%s\n", "Rotation matrix:");
  for (i=0; i<4; i++) {
    for (j=0; j<4; j++) {
      printf("%7.2f", mRotation[i][j]);
    }
    printf("\n");
  }
}


// -------------------------------------------------------------------
void ViewTransform::ViewRenderRotationMat() const {
  int i, j;
  printf("%s\n", "Render Rotation matrix:");
  for (i=0; i<4; i++) {
    for (j=0; j<4; j++) {
      printf("%7.2f", mRenderRotation[i][j]);
    }
    printf("\n");
  }
}


// -------------------------------------------------------------------
ostream& operator << (ostream &o, const ViewTransform &v) {
  v.Print();
  return o;
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------







