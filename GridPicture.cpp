// -------------------------------------------------------------------
// GridPicture.C
// -------------------------------------------------------------------
#include "GridPicture.H"
#include "GlobalUtilities.H"
#include <limits.h>
#include <float.h>

// -------------------------------------------------------------------
GridPicture::GridPicture() {
}


// -------------------------------------------------------------------
void GridPicture::GridPictureInit(int level,
			int rratio, int scale, int picSizeH, int picSizeV,
			const Box &overlapbox,
			const Box &boxWithData, int slicedir)
{
  sliceDir = slicedir;
  refRatio = rratio;
  currentScale = scale; 
  pictureSizeH = picSizeH;
  pictureSizeV = picSizeV;
  overlapBox = overlapbox;

  assert(overlapBox.sameSize(boxWithData));

  imageBox = overlapBox; 
  imageBox.refine(refRatio * currentScale);

  if(sliceDir == ZDIR) {
    dataSizeH = boxWithData.length(XDIR);
    dataSizeV = boxWithData.length(YDIR);
  } else if(sliceDir == YDIR) {
    dataSizeV = boxWithData.length(ZDIR);
    dataSizeH = boxWithData.length(XDIR);
  } else {
    dataSizeH = boxWithData.length(YDIR);
    dataSizeV = boxWithData.length(ZDIR);
  }
  imageSizeH = dataSizeH * refRatio * currentScale;
  imageSizeV = dataSizeV * refRatio * currentScale;

}  // end GridPictureInit


// -------------------------------------------------------------------
GridPicture::~GridPicture() {
}


// -------------------------------------------------------------------
void  GridPicture::ChangeScale(int newScale, int picSizeH, int picSizeV) {
  currentScale = newScale;
  imageBox = overlapBox; 
  imageBox.refine(refRatio * currentScale); 

  // use datasize instead of imageBox.length() for node centered boxes
  imageSizeH = dataSizeH * refRatio * currentScale;
  imageSizeV = dataSizeV * refRatio * currentScale;
  pictureSizeH = picSizeH;
  pictureSizeV = picSizeV;
}


// -------------------------------------------------------------------
int GridPicture::HPositionInPicture() {
  int endLoc;
  if(sliceDir == ZDIR) {
    endLoc = imageBox.smallEnd()[XDIR];
  } else if(sliceDir == YDIR) {
    endLoc = imageBox.smallEnd()[XDIR];
  } else {
    endLoc = imageBox.smallEnd()[YDIR];
  }
  return (endLoc - ((int) imageSizeH % refRatio));
}


// -------------------------------------------------------------------
int GridPicture::VPositionInPicture() {
  int endLoc, nodeAdjustment;
  if(sliceDir == ZDIR) {
    endLoc = imageBox.bigEnd()[YDIR];
    nodeAdjustment = imageBox.type()[YDIR] * (currentScale - 1) * refRatio;
  } else if(sliceDir == YDIR) {
    endLoc = imageBox.bigEnd()[ZDIR];
    nodeAdjustment = imageBox.type()[ZDIR] * (currentScale - 1) * refRatio;
  } else {
    endLoc = imageBox.bigEnd()[ZDIR];
    nodeAdjustment = imageBox.type()[ZDIR] * (currentScale - 1) * refRatio;
  }
  return ((pictureSizeV-1) - endLoc - ((int) imageSizeV % refRatio) - 
	  nodeAdjustment);
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
