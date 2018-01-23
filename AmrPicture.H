// ---------------------------------------------------------------
// AmrPicture.H
// ---------------------------------------------------------------
#ifndef _AMRPICTURE_H
#define _AMRPICTURE_H

#include <cstdio>

#include <AMReX_REAL.H>
#include <AMReX_Box.H>
#include <AMReX_AmrvisConstants.H>
#include <GlobalUtilities.H>
#include <GridPicture.H>
#include <AMReX_Vector.H>
#include <AMReX_FArrayBox.H>
#include <AMReX_DataServices.H>

#include <string>
using std::string;

using amrex::Real;

#include <X11/X.h>
#include <X11/Intrinsic.h>

class PltApp;
class PltAppState;
class GraphicsAttributes;
class Palette;

struct GridBoxes {
  int xbox, ybox, wbox, hbox;
};

enum VectorDerived { enVelocity, enMomentum, enNoneFound };

class AmrPicture {
 public:
  AmrPicture(GraphicsAttributes *gaptr,
             PltApp *pltappptr, PltAppState *pltappstateptr,
	     amrex::DataServices *dataservicesptr,
	     bool bcartgridsmoothing);
  // construct an AmrPicture using data from filename
  // 3D: called by first XY view
  
  AmrPicture(int view,
             GraphicsAttributes *gaptr, const amrex::Box &region, 
             PltApp *parentPltAppPtr, PltApp *pltappptr,
	     PltAppState *pltappstateptr,
	     bool bcartgridsmoothing);
  // construct an AmrPicture using pointer to data & box
  
  ~AmrPicture();
  void	DoFrameUpdate();
  void  DoContourSweep();
  void	DoStop();   // removes TimeOutCallback, updates GridPictures 
  void	SetRegion(int, int, int, int);
  void	CreatePicture(Window drawPictureHere, Palette *palptr);
  void	APDraw(int fromLevel, int toLevel);
  void  DrawSlice(int);
  void	DoExposePicture();
  void	APMakeImages(Palette *palptr);
  void	APChangeScale(int newScale, int previousScale);
  void	APChangeLevel();
  XImage *GetPictureXImage(const bool bdrawboxesintoimage = true);
  void	APChangeSlice(int here);
  
  void	SetSubCut(int, int, int, int);
  void	ToggleShowSubCut();
  void	DrawDatasetPoint(int hplot, int vplot, int size);
  void	UnDrawDatasetPoint();
  void	Sweep(amrex::Amrvis::AnimDirection direction);
  void  SetHVLine(int scale);
  void	SetHLine(int hl)    { hLine = hl; }
  void	SetVLine(int vl)    { vLine = vl; }
  int	GetHLine() const    { return hLine; }
  int	GetVLine() const    { return vLine; }
  int	GetSlice() const    { return slice; }
  unsigned int ImageSizeH() const { return imageSizeH; }
  unsigned int ImageSizeV() const { return imageSizeV; }
  void	SetDataServicesPtr(amrex::DataServices *tothis) { dataServicesPtr = tothis; } 
  void	SetHColor(int c)          { hColor = c; }
  void	SetVColor(int c)          { vColor = c; }
  const amrex::Box &GetSliceBox(int level) const { return sliceBox[level]; }
  int	GetMyView() const         { return myView; }
  const amrex::Vector<amrex::Box> &GetSubDomain() const { return subDomain; }
  void	SetFrameSpeed(int toThis) { frameSpeed = toThis; }
  Window PictureWindow()          { return pictureWindow; }
  void  SetCartGridSmoothing(bool tf) { bCartGridSmoothing = tf; }
  void APChangeContour(amrex::Amrvis::ContourType prevCType);
  void GetGridBoxes(amrex::Vector< amrex::Vector<GridBoxes> > &gb,
		    const int minlev, const int maxlev);

 private:
  Window 		pictureWindow;
  int			numberOfLevels;
  amrex::Vector<unsigned long>  dataSize, dataSizeH, dataSizeV;
  unsigned int   	imageSize, imageSizeH, imageSizeV;
  int			hdspoint, vdspoint, dsBoxSize;
  int			datasetPointShowing, datasetPointColor;
  Pixmap 		pixMap;
  amrex::Vector< amrex::Vector<GridPicture> > gpArray;
  amrex::Vector< amrex::Vector< amrex::Vector<GridPicture> > > frameGrids;
  int			regionX, regionY, region2ndX, region2ndY;
  int			subCutShowing;
  int			subcutX, subcutY, subcut2ndX, subcut2ndY;
  amrex::Vector<amrex::Box> subDomain;
  amrex::Vector<amrex::Box> sliceBox;
  amrex::Vector<XImage *>   frameBuffer;
  bool			framesMade;
  int			frameSpeed;
  amrex::Amrvis::AnimDirection sweepDirection;
  XtIntervalId		pendingTimeOut;
  Palette		*palPtr;
  amrex::Vector<amrex::FArrayBox *> sliceFab;
  amrex::Vector<amrex::FArrayBox *> vfSliceFab;
  amrex::Vector<unsigned char *>  imageData, scaledImageData;
  amrex::Vector<unsigned char> scaledImageDataBodyMask;
  amrex::Vector<XImage *>       xImageArray;
  amrex::Vector<bool>           xImageCreated;
  GraphicsAttributes	*gaPtr;
  PltApp	        *pltAppPtr;
  PltAppState	        *pltAppStatePtr;
  amrex::DataServices   *dataServicesPtr;
  Display		*display;
  GC			xgc;
  int			myView, hLine, vLine;
  int			maxLevelWithGrids;
  int			hColor, vColor, myColor, slice, sliceDir;
  bool                  bCartGridSmoothing;
  bool                  pixMapCreated, isSubDomain, findSubRange;
  amrex::Vector< amrex::Vector<string> > vecNames;
  
  
  // private functions
  void SetSlice(int view, int here);
  void CoarsenSliceBox();
  void ShowFrameImage(int iSlice);
  void CreateFrames(amrex::Amrvis::AnimDirection direction);
  void AmrPictureInit();
  void DrawBoxes(amrex::Vector< amrex::Vector<GridPicture> > &gp, Drawable &drawable);
  void DrawTerrBoxes(int level, bool bIsWindow, bool bIsPixmap);
  void CreateImage(const amrex::FArrayBox &fab, unsigned char *imagedata,
                   int datasizeh, int datasizev,
                   Real globalMin, Real globalMax, Palette *palptr,
		   const amrex::FArrayBox *vfracFab, const Real vfeps);
  void CreateScaledImage(XImage **ximage, int scale,
                         unsigned char *imagedata,
                         unsigned char *scaledimagedata,
                         int datasizeh, int datasizev,
                         int imagesizeh, int imagesizev, int level,
		         bool bCreateMask);
  void DrawContour(amrex::Vector<amrex::FArrayBox *> slicefab, Display *display,
		   Drawable &drawable, const GC &gc);
  bool DrawContour(const amrex::FArrayBox &fab, Real value,
                   bool has_mask, const bool *mask,
                   Display *display, Drawable &drawable, const GC &gc, int FGColor,
                   int xLength, int yLength,
                   Real leftEdge, Real bottomEdge, Real rightEdge, Real topEdge);
  inline bool Between(Real a, Real b, Real c) {
      return ( ((a <= b) && (b <= c)) || ((a >= b) && (b >= c)) );
  }

  void DrawVectorField(Display *display, Drawable &drawable, const GC &gc);
  void DrawVectorField(Display *display, Drawable &drawable, const GC &gc,
		       int hDir, int vDir, int maxLength,
                       const Real *hdat, const Real *vdat,
		       const Real velocityMax,
		       const amrex::Box &dvfSliceBox, int dvfFactor);
  VectorDerived FindVectorDerived(amrex::Vector<string> &aVectorDeriveNames);

  static void CBFrameTimeOut(XtPointer, XtIntervalId *);
  static void CBContourSweep(XtPointer, XtIntervalId *);

};

#endif
