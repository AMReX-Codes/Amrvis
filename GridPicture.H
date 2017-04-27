// ---------------------------------------------------------------
// GridPicture.H
// ---------------------------------------------------------------
#ifndef _GRIDPICTURE_H
#define _GRIDPICTURE_H

#include <AMReX_AmrvisConstants.H>
#include <AMReX_Box.H>


class GridPicture {

 public:

  GridPicture();
  ~GridPicture();
  void GridPictureInit(int level, int rratio, int scale,
    		       int picSizeH, int picSizeV,
		       const amrex::Box &overlapbox,
		       const amrex::Box &boxWithData,
		       int slicedir);
  void ChangeScale(int newScale, int picSizeH, int picSizeV);
  int HPositionInPicture();
  int VPositionInPicture();

  int ImageSizeH() const { return imageSizeH; }
  int ImageSizeV() const { return imageSizeV; }


 private:

  amrex::Box overlapBox;
  unsigned int dataSizeH, dataSizeV;
  unsigned int imageSizeH, imageSizeV;
  unsigned int pictureSizeH, pictureSizeV;
  int currentScale, refRatio, sliceDir;
  amrex::Box imageBox;
};

#endif
