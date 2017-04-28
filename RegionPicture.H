// ---------------------------------------------------------------
// RegionPicture.H
// ---------------------------------------------------------------
#ifndef _REGIONPICTURE_H
#define _REGIONPICTURE_H

#include <cstdio>

#include <AMReX_REAL.H>
#include <AMReX_Box.H>
#include <AMReX_AmrvisConstants.H>
#include <GlobalUtilities.H>
#include <GridPicture.H>
#include <AMReX_Array.H>
#include <AMReX_FArrayBox.H>

#include <string>
using std::string;

using amrex::Real;

#include <X11/X.h>
#include <X11/Intrinsic.h>

class GraphicsAttributes;
class Palette;

struct GridBoxes {
  int xbox, ybox, wbox, hbox;
};


class RegionPicture {
 public:
  RegionPicture(GraphicsAttributes *gaptr);
  
  ~RegionPicture();
  void	SetRegion(int, int, int, int);
  void	CreatePicture(Window drawPictureHere, Palette *palptr);
  void	APDraw(int fromLevel, int toLevel);
  void	DoExposePicture();
  void	APMakeImages(Palette *palptr);
  void	APChangeScale(int newScale, int previousScale);
  XImage *GetPictureXImage(const bool bdrawboxesintoimage = true);
  void	APChangeSlice(int here);
  
  unsigned int ImageSizeH() const { return imageSizeH; }
  unsigned int ImageSizeV() const { return imageSizeV; }
  void	SetHColor(int c)          { hColor = c; }
  void	SetVColor(int c)          { vColor = c; }
  const amrex::Box &GetSliceBox(int level) const { return sliceBox; }
  Window PictureWindow()          { return pictureWindow; }

 private:
  Window 		pictureWindow;
  unsigned long  dataSize, dataSizeH, dataSizeV;
  unsigned int   	imageSize, imageSizeH, imageSizeV;
  Pixmap 		pixMap;
  Palette		*palPtr;
  amrex::FArrayBox *sliceFab;
  unsigned char *imageData, *scaledImageData;
  XImage *xImage;
  bool           xImageCreated;
  GraphicsAttributes	*gaPtr;
  Display		*display;
  GC			xgc;
  int			myView, hLine, vLine;
  int			hColor, vColor, myColor;
  bool                  pixMapCreated;
  int currentScale;
  
  
  // private functions
  void SetSlice(int view, int here);
  void CoarsenSliceBox();
  void ShowFrameImage(int iSlice);
  void RegionPictureInit();
  void CreateImage(const amrex::FArrayBox &fab, unsigned char *imagedata,
                   int datasizeh, int datasizev,
                   Real globalMin, Real globalMax, Palette *palptr,
		   const amrex::FArrayBox *vfracFab, const Real vfeps);
  void CreateScaledImage(XImage **ximage, int scale,
                         unsigned char *imagedata,
                         unsigned char *scaledimagedata,
                         int datasizeh, int datasizev,
                         int imagesizeh, int imagesizev);
};

#endif
