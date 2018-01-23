// ---------------------------------------------------------------
// RegionPicture.H
// ---------------------------------------------------------------
#ifndef _REGIONPICTURE_H
#define _REGIONPICTURE_H

#include <cstdio>

#include <AMReX_REAL.H>
#include <AMReX_Box.H>
#include <AMReX_AmrvisConstants.H>
#include <AMReX_DataServices.H>
#include <GlobalUtilities.H>
#include <AMReX_Vector.H>
#include <AMReX_FArrayBox.H>
#include <AMReX_BLProfStats.H>

#include <string>
using std::string;

using amrex::Real;

#include <X11/X.h>
#include <X11/Intrinsic.h>

class GraphicsAttributes;
class Palette;
class DataServices;
class ProfApp;


class RegionPicture {
 public:

  enum { RP_OFF = 0, RP_ON, RP_SPLIT };

  RegionPicture(GraphicsAttributes *gaptr, amrex::DataServices *pdsp);

  // ---- for subregions
  RegionPicture(GraphicsAttributes *gaptr, const amrex::Box &regionBox,
                ProfApp *profAppPtr);
  
  ~RegionPicture();
  void	SetRegion(int, int, int, int);
  void	CreatePicture(Window drawPictureHere, Palette *palptr);
  void	APDraw(int fromLevel, int toLevel);
  void	DoExposePicture();
  void	APMakeImages(Palette *palptr);
  void	APChangeScale(int newScale, int previousScale);
  XImage *GetPictureXImage();
  void	APChangeSlice(int here);
  amrex::Box DomainBox() { return subRegion; }
  const amrex::Vector<amrex::Vector<amrex::Box>> &RegionBoxes() const { return regionBoxes; }
  const BLProfStats::TimeRange &CalcTimeRange() const { return calcTimeRange; }
  const BLProfStats::TimeRange &SubTimeRange()  const { return subTimeRange; }
  Real DataValue(int i, int j, bool &outOfRange);
  void SetRegionOnOff(int regionIndex, int whichRegion, int onoff);
  void SetAllOnOff(int onoff);
  
  unsigned int ImageSizeH() const { return imageSizeH; }
  unsigned int ImageSizeV() const { return imageSizeV; }
  void	SetHColor(int c)          { hColor = c; }
  void	SetVColor(int c)          { vColor = c; }
  Window PictureWindow()          { return pictureWindow; }

 private:
  Window pictureWindow;
  GraphicsAttributes *gaPtr;
  Display *display;
  GC  xgc;
  Pixmap  pixMap;
  XImage *xImage, *atiXImage;
  XImage *xImageDim, *atiXImageDim;
  unsigned long  dataSize, dataSizeH, dataSizeV;
  unsigned long  atiDataSize, atiDataSizeH, atiDataSizeV;
  unsigned int   imageSize, imageSizeH, imageSizeV;
  unsigned int   atiImageSize, atiImageSizeH, atiImageSizeV;
  Palette *palPtr;
  amrex::FArrayBox *sliceFab;
  amrex::Box subRegion;
  unsigned char *imageData;
  unsigned char *scaledImageData, *scaledImageDataDim;
  unsigned char *atiImageData;
  unsigned char *scaledATIImageData, *scaledATIImageDataDim;
  bool xImageCreated;
  amrex::DataServices   *dataServicesPtr;
  int hLine, vLine, hColor, vColor, myColor;
  bool pixMapCreated;
  int currentScale;
  int regionX, regionY, region2ndX, region2ndY;
  int subcutX, subcutY, subcut2ndX, subcut2ndY;
  BLProfStats::TimeRange calcTimeRange, subTimeRange;
  amrex::Vector<amrex::Vector<amrex::Box>> regionBoxes;  // ---- [region][box]
  amrex::Vector<amrex::Vector<int>> regionsOnOff;        // ---- [region][onoff]
  amrex::BoxDomain timeSpanOff;                          // ---- time spans that are off
  
  void SetSlice(int view, int here);
  void CoarsenSliceBox();
  void ShowFrameImage(int iSlice);
  void RegionPictureInit(const amrex::Box &regionBox);
  void CreateImage(const amrex::FArrayBox &fab, unsigned char *imagedata,
                   int datasizeh, int datasizev,
                   Real globalMin, Real globalMax, Palette *palptr);
  void CreateScaledImage(XImage **ximage, int scale,
                         unsigned char *imagedata,
                         unsigned char *scaledimagedata,
                         int datasizeh, int datasizev,
                         int imagesizeh, int imagesizev,
			 bool dim = false);
  RegionPicture();
};

#endif
