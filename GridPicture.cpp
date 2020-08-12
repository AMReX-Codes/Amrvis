// -------------------------------------------------------------------
// GridPicture.cpp
// -------------------------------------------------------------------
#include <GridPicture.H>
#include <GlobalUtilities.H>
#include <climits>
#include <cfloat>

using namespace amrex;


// -------------------------------------------------------------------
GridPicture::GridPicture() {
}


// -------------------------------------------------------------------
void GridPicture::GridPictureInit(int /*level*/,
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

  BL_ASSERT(overlapBox.sameSize(boxWithData));

  imageBox = overlapBox; 
  imageBox.refine(refRatio * currentScale);

  if(sliceDir == Amrvis::ZDIR) {
    dataSizeH = boxWithData.length(Amrvis::XDIR);
#if (BL_SPACEDIM == 1)
    dataSizeV = 1;
#else
    dataSizeV = boxWithData.length(Amrvis::YDIR);
#endif
  } else if(sliceDir == Amrvis::YDIR) {
    dataSizeV = boxWithData.length(Amrvis::ZDIR);
    dataSizeH = boxWithData.length(Amrvis::XDIR);
  } else {
    dataSizeH = boxWithData.length(Amrvis::YDIR);
    dataSizeV = boxWithData.length(Amrvis::ZDIR);
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
  if(sliceDir == Amrvis::ZDIR) {
    endLoc = imageBox.smallEnd()[Amrvis::XDIR];
  } else if(sliceDir == Amrvis::YDIR) {
    endLoc = imageBox.smallEnd()[Amrvis::XDIR];
  } else {
    endLoc = imageBox.smallEnd()[Amrvis::YDIR];
  }
  return (endLoc - ((int) imageSizeH % refRatio));
}


// -------------------------------------------------------------------
int GridPicture::VPositionInPicture() {
  int endLoc, nodeAdjustment;
  if(sliceDir == Amrvis::ZDIR) {
#if (BL_SPACEDIM == 1)
    endLoc = pictureSizeV - 1;
    nodeAdjustment = 0;
#else
    endLoc = imageBox.bigEnd()[Amrvis::YDIR];
    nodeAdjustment = imageBox.type()[Amrvis::YDIR] * (currentScale - 1) * refRatio;
#endif
  } else if(sliceDir == Amrvis::YDIR) {
    endLoc = imageBox.bigEnd()[Amrvis::ZDIR];
    nodeAdjustment = imageBox.type()[Amrvis::ZDIR] * (currentScale - 1) * refRatio;
  } else {
    endLoc = imageBox.bigEnd()[Amrvis::ZDIR];
    nodeAdjustment = imageBox.type()[Amrvis::ZDIR] * (currentScale - 1) * refRatio;
  }
  return ((pictureSizeV-1) - endLoc - ((int) imageSizeV % refRatio) - 
	  nodeAdjustment);
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
