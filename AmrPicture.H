
//
// $Id: AmrPicture.H,v 1.43 2002-02-07 23:59:02 vince Exp $
//

// ---------------------------------------------------------------
// AmrPicture.H
// ---------------------------------------------------------------
#ifndef _AMRPICTURE_H
#define _AMRPICTURE_H

#include <cstdio>

#include "REAL.H"
#include "Box.H"
#include "AmrvisConstants.H"
#include "GlobalUtilities.H"
#include "GridPicture.H"
#include "Array.H"
#include "FArrayBox.H"

#include <string>
using std::string;

#include <X11/X.h>
#include <X11/Intrinsic.h>

class PltApp;
class PltAppState;
class DataServices;
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
	     DataServices *dataservicesptr,
	     bool bcartgridsmoothing);
  // construct an AmrPicture using data from filename
  // 3D: called by first XY view
  
  AmrPicture(int view,
             GraphicsAttributes *gaptr, const Box &region, 
             //AmrPicture *parentPicturePtr, 
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
  void	Sweep(AnimDirection direction);
  void  SetHVLine(int scale);
  void	SetHLine(int hl)    { hLine = hl; }
  void	SetVLine(int vl)    { vLine = vl; }
  int	GetHLine() const    { return hLine; }
  int	GetVLine() const    { return vLine; }
  int	GetSlice() const    { return slice; }
  unsigned int ImageSizeH() const { return imageSizeH; }
  unsigned int ImageSizeV() const { return imageSizeV; }
  void	SetDataServicesPtr(DataServices *tothis) { dataServicesPtr = tothis; } 
  void	SetHColor(int c)          { hColor = c; }
  void	SetVColor(int c)          { vColor = c; }
  const Box &GetSliceBox(int level) const { return sliceBox[level]; }
  int	GetMyView() const         { return myView; }
  const Array<Box> &GetSubDomain() const { return subDomain; }
  void	SetFrameSpeed(int toThis) { frameSpeed = toThis; }
  Window PictureWindow()          { return pictureWindow; }
  void  SetCartGridSmoothing(bool tf) { bCartGridSmoothing = tf; }
  void APChangeContour(ContourType prevCType);
  void GetGridBoxes(Array< Array<GridBoxes> > &gb,
		    const int minlev, const int maxlev);

 private:
  Window 		pictureWindow;
  int			numberOfLevels;
  Array<unsigned long>  dataSize, dataSizeH, dataSizeV;
  unsigned int   	imageSize, imageSizeH, imageSizeV;
  int			hdspoint, vdspoint, dsBoxSize;
  int			datasetPointShowing, datasetPointColor;
  Pixmap 		pixMap;
  Array< Array<GridPicture> > gpArray;
  Array< Array< Array<GridPicture> > > frameGrids;
  int			regionX, regionY, region2ndX, region2ndY;
  int			subCutShowing;
  int			subcutX, subcutY, subcut2ndX, subcut2ndY;
  Array<Box>		subDomain;
  Array<Box>		sliceBox;
  Array<XImage *>	frameBuffer;
  bool			framesMade;
  int			frameSpeed;
  AnimDirection		sweepDirection;
  XtIntervalId		pendingTimeOut;
  Palette		*palPtr;
  Array<FArrayBox *>	sliceFab;
  Array<unsigned char *>  imageData, scaledImageData;
  Array<XImage *>       xImageArray;
  Array<bool>           xImageCreated;
  GraphicsAttributes	*gaPtr;
  PltApp	        *pltAppPtr;
  PltAppState	        *pltAppStatePtr;
  DataServices		*dataServicesPtr;
  Display		*display;
  GC			xgc;
  int			myView, hLine, vLine;
  int			maxLevelWithGrids;
  int			hColor, vColor, myColor, slice, sliceDir;
  bool                  bCartGridSmoothing;
  bool                  pixMapCreated, isSubDomain, findSubRange;
  
  
  // private functions
  void SetSlice(int view, int here);
  void CoarsenSliceBox();
  void ShowFrameImage(int iSlice);
  void CreateFrames(AnimDirection direction);
  void AmrPictureInit();
  void DrawBoxes(Array< Array<GridPicture> > &gp, Drawable &drawable);
  void DrawTerrBoxes(int level, bool bIsWindow, bool bIsPixmap);
  void CreateImage(const FArrayBox &fab, unsigned char *imagedata,
                   int datasizeh, int datasizev,
                   Real globalMin, Real globalMax, Palette *palptr);
  void CreateScaledImage(XImage **ximage, int scale,
                         unsigned char *imagedata,
                         unsigned char *scaledimagedata,
                         int datasizeh, int datasizev,
                         int imagesizeh, int imagesizev);
  void DrawContour(Array<FArrayBox *> slicefab, Display *display,
		   Drawable &drawable, const GC &gc);
  bool DrawContour(const FArrayBox &fab, Real value,
                   bool has_mask, const bool *mask,
                   Display *display, Drawable &drawable, const GC &gc, int FGColor,
                   int xLength, int yLength,
                   Real leftEdge, Real bottomEdge, Real rightEdge, Real topEdge);
  inline bool Between(Real a, Real b, Real c) {
      return ( (a <= b) && (b <= c) || (a >= b) && (b >= c) );
  }

  void DrawVectorField(Display *display, Drawable &drawable, const GC &gc);
  void DrawVectorField(Display *display, Drawable &drawable, const GC &gc,
		       int hDir, int vDir, int maxLength,
                       const Real *hdat, const Real *vdat,
		       const Real velocityMax,
		       const Box &dvfSliceBox, int dvfFactor);
  VectorDerived FindVectorDerived(Array<string> &aVectorDeriveNames);

  static void CBFrameTimeOut(XtPointer, XtIntervalId *);
  static void CBContourSweep(XtPointer, XtIntervalId *);

};

#endif
