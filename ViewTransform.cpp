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


//--------------------------------------------------------------------
AmrQuaternion ViewTransform::Screen2Quat(int start_X, int start_Y, 
                                         int end_X, int end_Y, Real box_size) {
    Real xWorldStart = (Real)(start_X-screenPositionX)/
        (screenPositionX*scaleX);
    Real yWorldStart = (Real)(start_Y-screenPositionY)/
        (screenPositionY*scaleX);
    Real xWorldEnd = (Real)(end_X-screenPositionX)/(screenPositionX*scaleX);
    Real yWorldEnd = (Real)(end_Y-screenPositionY)/(screenPositionY*scaleX);
    return trackball(xWorldEnd/box_size, yWorldEnd/box_size,
                     xWorldStart/box_size, yWorldStart/box_size);
}



// -------------------------------------------------------------------
void ViewTransform::TransformPoint(Real x, Real y, Real z,
				   Real &pX, Real &pY, Real &pZ)
{
//apply rotation to (x, y, z, 1);
    Real scale[4] = {scaleX, scaleY, scaleZ, 1.};
    Real point[4]= {x-objCenterX,y-objCenterY,z-objCenterZ,1};
    Real newPoint[4];
    for(int ii=0; ii<2; ii++) {
//save time -- only the first two numbers, I think
        int temp = 0;
        for(int jj=0; jj<4; jj++) {
            temp += point[jj]*mRotation[ii][jj]*scale[jj];
        }
        newPoint[ii] = temp;
    }
    pX = newPoint[0] + screenPositionX;
    pY = newPoint[1] + screenPositionY;
//    pZ = newPoint[2];
}

void ViewTransform::Print()const {
    cout<<"ViewTransform::Print() does nothing now"<<endl;
}

// -------------------------------------------------------------------
void ViewTransform::MakeTransform() {
  rotation.tomatrix(mRotation);
  renderRotation.tomatrix(mRenderRotation);
}

// -------------------------------------------------------------------
void ViewTransform::GetRotationMat(MatrixFour m) {
    for(int i = 0; i< 4; i++) {
        for (int j = 0; j<4 ; j++) {
            m[i][j] = mRotation[i][j];
        }
    }
}
    
void ViewTransform::GetRenderRotationMat(MatrixFour m) {
    for(int i = 0; i< 4; i++) {
        for (int j = 0; j<4 ; j++) {
            m[i][j] = mRenderRotation[i][j];
        }
    }
}
    

void ViewTransform::ViewRotationMat() const {
  int i, j;
  printf("%s\n", "Rotation matrix");
  for (i=0; i<4; i++) {
    for (j=0; j<4; j++) {
      printf("%7.2f", mRotation[i][j]);
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







