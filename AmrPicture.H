//BL_COPYRIGHT_NOTICE

//
// $Id: AmrPicture.H,v 1.27 1999-10-05 21:53:09 vince Exp $
//

// ---------------------------------------------------------------
// AmrPicture.H
// ---------------------------------------------------------------
#ifndef _AMRPICTURE_H
#define _AMRPICTURE_H

#ifdef BL_USE_NEW_HFILES
#include <cstdio>
#else
#include <stdio.h>
#endif

#include "REAL.H"
#include "Box.H"
#include "AmrvisConstants.H"
#include "GlobalUtilities.H"
#include "GridPicture.H"
#include "Palette.H"
#include "Array.H"
#include "aString.H"
#include "FArrayBox.H"

class PltApp;
class DataServices;
class GraphicsAttributes;

enum VectorDerived { enVelocity, enMomentum, enNoneFound };

class AmrPicture {
 public:
  AmrPicture(int mindrawnlevel, GraphicsAttributes *gaptr,
             PltApp *pltappptr, DataServices *dataservicesptr,
	     bool bcartgridsmoothing);
  // construct an AmrPicture using data from filename
  // 3D: called by first XY view
  
  AmrPicture(int view, int mindrawnlevel, 
             GraphicsAttributes *gaptr, Box region, 
             AmrPicture *parentPicturePtr, 
             PltApp *parentPltAppPtr, PltApp *pltappptr,
	     bool bcartgridsmoothing);
  // construct an AmrPicture using pointer to data & box
  
  ~AmrPicture();
  static void CBFrameTimeOut(XtPointer, XtIntervalId *);
  static void CBContourSweep(XtPointer, XtIntervalId *);
  void	DoFrameUpdate();
  void  DoContourSweep();
  void	DoStop();   // removes TimeOutCallback, updates GridPictures 
  void	SetRegion(int, int, int, int);
  void	CreatePicture(Window drawPictureHere, Palette *palptr,
                      const aString &derived);
  void	Draw(int fromLevel, int toLevel);
  void  DrawSlice(int);
  void	DoExposePicture();
  void	ToggleBoxes();
  void	ChangeDerived(aString derived, Palette *palptr);
  void  ChangeRasterImage();
  void	ChangeScale(int newScale);
  void	ChangeLevel(int lowLevel, int hiLevel);
  XImage *GetPictureXImage();
  void	ChangeSlice(int here);
  
  void	SyncPicture();
  void	SetSubCut(int, int, int, int);
  void	ToggleShowSubCut();
  void	DrawDatasetPoint(int hplot, int vplot, int size);
  void	UnDrawDatasetPoint();
  void	Sweep(AnimDirection direction);
  void  SetHVLine();
  void	SetHLine(int hl)    { hLine = hl; }
  void	SetVLine(int vl)    { vLine = vl; }
  int	GetHLine() const    { return hLine; }
  int	GetVLine() const    { return vLine; }
  int	GetSlice() const    { return slice; }
  const aString &CurrentDerived() const { return currentDerived; }
  int	NumberOfLevels()    const { return numberOfLevels; }
  int	FinestLevel()       const { return numberOfLevels - 1; }
  int	MaxAllowableLevel() const { return maxAllowableLevel; }
  bool	ShowingBoxes()      const { return showBoxes; }
  void  SetShowBoxes(bool b) { showBoxes = b; }
  unsigned int ImageSizeH() const { return imageSizeH; }
  unsigned int ImageSizeV() const { return imageSizeV; }
  DataServices *GetDataServicesPtr() const { return dataServicesPtr; } 
  void	SetDataServicesPtr(DataServices *tothis) { dataServicesPtr = tothis; } 
  void	SetDataMin(Real min) { dataMinSpecified = min; }
  void	SetDataMax(Real max) { dataMaxSpecified = max; }
  Real	GetSpecifiedMin() const { return dataMinSpecified; }
  Real	GetSpecifiedMax() const { return dataMaxSpecified; }
  Real	GetMin() const       { return dataMinAllGrids; }
  Real	GetMax() const       { return dataMaxAllGrids; }
  Real	GetRegionMin() const { return dataMinRegion; }
  Real	GetRegionMax() const { return dataMaxRegion; }
  void	SetWhichRange(Range which);
  void  SetContourNumber(int);
  int   GetContourNumber() const { return numberOfContours; }
  int   MinDrawnLevel() const { return minDrawnLevel; }
  int   MaxDrawnLevel() const { return maxDrawnLevel; }
  Range	GetWhichRange() const   { return whichRange; }
  Real	GetWhichMin();
  Real	GetWhichMax();
  void	SetHColor(int c)          { hColor = c; }
  void	SetVColor(int c)          { vColor = c; }
  const Box &GetSliceBox() const  { return sliceBox[finestLevel]; }
  int	GetMyView() const         { return myView; }
  const Array<Box> &GetSubDomain() const { return subDomain; }
  bool	FramesMade() const        { return framesMade; }
  void	SetFrameSpeed(int toThis) { frameSpeed = toThis; }
  int	GetFrameSpeed() const     { return frameSpeed; }
  GraphicsAttributes *GetGAPtr()  { return GAptr; }
  Palette *GetPalPtr()            { return palPtr; }
  Real	MinUsing() const          { return minUsing; }
  Real	MaxUsing() const          { return maxUsing; }
  Window	PictureWindow()   { return pictureWindow; }
  void	SetMaxDrawnLevel(int lev) { maxDrawnLevel = lev; }
  void  SetCartGridSmoothing(bool tf);
  PltApp *GetPltAppPtr()          { return pltAppPtr; }
  void ChangeContour(int);
  bool Contours() const     { return contours; }
  bool Raster()   const     { return raster; }
  bool ColorContour() const { return colContour; }
  bool VectorField()  const { return vectorField; }
  VectorDerived FindVectorDerived(Array<aString> &aVectorDeriveNames);

 private:
  Window 		pictureWindow;
  int			numberOfLevels, finestLevel;
  int			numberOfDerived, numberOfContours;
  Real			dataMinAllGrids, dataMaxAllGrids;
  Real			dataMinRegion, dataMaxRegion;
  Real			dataMinSpecified, dataMaxSpecified;
  Real			dataMinFile, dataMaxFile;
  Real			minUsing, maxUsing;
  Range			whichRange;
  int			myView, hLine, vLine;
  int			hColor, vColor, myColor, slice, sliceDir;
  Array<unsigned long>  dataSize, dataSizeH, dataSizeV;
  unsigned int   	imageSize, imageSizeH, imageSizeV;
  aString		currentDerived;
  bool			showBoxes;
  int			minDrawnLevel, maxDrawnLevel, maxAllowableLevel;
  int			maxLevelWithGrids, maxsFound;
  int			hdspoint, vdspoint, dsBoxSize;
  int			datasetPointShowing, datasetPointColor;
  Pixmap 		pixMap;
  char			buffer[BUFSIZ];
  bool			pixMapCreated, isSubDomain, findSubRange;
  Array< Array<GridPicture> > gpArray;
  Array< Array< Array<GridPicture> > > frameGrids;
  PltApp	        *pltAppPtr;
  DataServices		*dataServicesPtr;
  int			regionX, regionY, region2ndX, region2ndY;
  int			subCutShowing;
  int			subcutX, subcutY, subcut2ndX, subcut2ndY;
  Array<Box>		subDomain, sliceBox;
  Array<XImage *>	frameBuffer;
  bool			framesMade;
  int			frameSpeed;
  AnimDirection		sweepDirection;
  XtIntervalId		pendingTimeOut;
  GraphicsAttributes	*GAptr;
  Palette		*palPtr;
  Array<FArrayBox *>	sliceFab;
  Array<unsigned char *>  imageData, scaledImageData;
  Array<XImage *>       xImageArray;
  Array<bool >           xImageCreated;
  bool contours, raster, colContour, vectorField, bCartGridSmoothing;
  
  
  // private functions
  void SetRasterOnly();
  void SetRasterContour();
  void SetColorContour();
  void SetBWContour();
  void SetVectorField();
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
};

#endif
