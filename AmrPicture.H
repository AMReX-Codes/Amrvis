// -------------------------------------------------------------------
// AmrPicture.H
// -------------------------------------------------------------------
#ifndef _AMRPICTURE_H
#define _AMRPICTURE_H

#include <fcntl.h>
#include <iostream.h>
#include <stream.h>
#include <fstream.h>
#include <stdio.h>

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

class AmrPicture {
  public:
	AmrPicture(int mindrawnlevel, GraphicsAttributes *gaptr,
		   PltApp *pltappptr, DataServices *dataservicesptr);
	// construct an AmrPicture using data from filename
	// 3D: called by first XY view

	AmrPicture(int view, int mindrawnlevel, GraphicsAttributes *gaptr,
		   Box region, AmrPicture *parentPicturePtr, PltApp *pltappptr);
	// construct an AmrPicture using pointer to data & box

	~AmrPicture();
	static void CBFrameTimeOut(XtPointer, XtIntervalId *);
	void	DoFrameUpdate();
	void	DoStop();   // removes TimeOutCallback, updates GridPictures 
	void	SetWindow(Window drawPictureHere);
	void	SetRegion(int, int, int, int);
	void	CreatePicture(Window drawPictureHere, Palette *palptr,
				const aString &derived);
	void	Draw(int fromLevel, int toLevel);
	void	DrawBoxes();
	void	DoExposePicture();
	void	ToggleBoxes();
	void	ChangeDerived(aString derived, Palette *palptr);
	void	ChangeScale(int newScale);
	void	ChangeLevel(int lowLevel, int hiLevel);
	XImage	*GetPictureXImage();
	void	ChangeSlice(int here);

	void	SyncPicture();
	void	SetSubCut(int, int, int, int);
	void	ToggleShowSubCut();
	void	SetDatasetPoint(int hplot, int vplot, int color);
	void	Sweep(AnimDirection direction);
	void	DatasetPointOff()   { datasetPointShowing = false; }
	void	SetHLine(int hl)    { hLine = hl; }
	void	SetVLine(int vl)    { vLine = vl; }
	int	GetHLine()          { return hLine; }
	int	GetVLine()          { return vLine; }
	int	GetSlice()          { return slice; }
	aString	&CurrentDerived()   { return currentDerived; }
	int	NumberOfLevels()    { return numberOfLevels; }
	int	FinestLevel()       { return numberOfLevels - 1; }
	int	MaxAllowableLevel() { return maxAllowableLevel; }
	int	ShowingBoxes()      { return showBoxes; }
	unsigned int ImageSizeH()   { return imageSizeH; }
	unsigned int ImageSizeV()   { return imageSizeV; }
	DataServices *GetDataServicesPtr()    { return dataServicesPtr; } 
	void	SetDataServicesPtr(DataServices *tothis) { dataServicesPtr = tothis; } 
	void	SetDataMin(Real min) { dataMinSpecified = min; }
	void	SetDataMax(Real max) { dataMaxSpecified = max; }
	Real	GetSpecifiedMin()    { return dataMinSpecified; }
	Real	GetSpecifiedMax()    { return dataMaxSpecified; }
	Real	GetMin()             { return dataMinAllGrids; }
	Real	GetMax()             { return dataMaxAllGrids; }
	Real	GetRegionMin()       { return dataMinRegion; }
	Real	GetRegionMax()       { return dataMaxRegion; }
	void	SetWhichRange(Range which);
	Range	GetWhichRange()         { return whichRange; }
	Real	GetWhichMin();
	Real	GetWhichMax();
	void	SetHColor(int c)          { hColor = c; }
	void	SetVColor(int c)          { vColor = c; }
	unsigned long	GetDataSize()     { return dataSize; }
	Box	&GetSliceBox()             { return sliceBox[finestLevel]; }
	int	GetMyView()               { return myView; }
	Array<Box> &GetSubDomain()        { return subDomain; }
	int	FramesMade()              { return framesMade; }
	void	SetFrameSpeed(int toThis) { frameSpeed = toThis; }
	int	GetFrameSpeed()           { return frameSpeed; }
	GraphicsAttributes *GetGAPtr()    { return GAptr; }
	Palette *GetPalPtr()              { return palPtr; }
	Real	MinUsing()                { return minUsing; }
	Real	MaxUsing()                { return maxUsing; }
	Window	PictureWindow()           { return pictureWindow; }
	void	SetMaxDrawnLevel(int lev) { maxDrawnLevel = lev; }

private:
	Window 			pictureWindow;
	int	  		numberOfLevels, finestLevel, numberOfDerived;
	Real			dataMinAllGrids, dataMaxAllGrids;
	Real			dataMinRegion, dataMaxRegion;
	Real			dataMinSpecified, dataMaxSpecified;
	Real			dataMinFile, dataMaxFile;
	Real			minUsing, maxUsing;
	Range			whichRange;
	int			myView, hLine, vLine;
	int			hColor, vColor, myColor, slice, sliceDir;
	unsigned long   	dataSize, dataSizeH, dataSizeV;
	unsigned int   		imageSize, imageSizeH, imageSizeV;
	int			currentLevel;
	aString			currentDerived;
	int			showBoxes;
	int			minDrawnLevel, maxDrawnLevel, maxAllowableLevel;
	int			maxLevelWithGrids, maxsFound;
	int			hdspoint, vdspoint;
	int			datasetPointShowing, datasetPointColor;
	Pixmap 			pixMap;
	char			buffer[BUFSIZ];
	bool			pixMapCreated, isSubDomain, findSubRange;
	Array< Array<GridPicture> > gpArray;
	PltApp			*pltAppPtr;
	DataServices		*dataServicesPtr;
	int			regionX, regionY, region2ndX, region2ndY;
	int			subCutShowing;
	int			subcutX, subcutY, subcut2ndX, subcut2ndY;
	Array<Box>		subDomain, sliceBox;
	XImage			**frameBuffer;
	int			framesMade, frameSpeed;
	AnimDirection		sweepDirection;
	XtIntervalId		pendingTimeOut;
	GraphicsAttributes	*GAptr;
	Palette			*palPtr;
	FArrayBox		sliceFab;
	unsigned char           *imageData, *scaledImageData;
	XImage                  *image;



	// private functions
	void SetSlice(int view, int here);
	void CoarsenSliceBox();
	void ShowFrameImage();
	void CreateFrames(AnimDirection direction);
	void AmrPictureInit();
	void CreateImage(const FArrayBox &fab, unsigned char *imagedata,
			 int datasizeh, int datasizev,
			 Real globalMin, Real globalMax, Palette *palptr);
	void CreateScaledImage(XImage **ximage, int scale,
                               unsigned char *imagedata,
                               unsigned char *scaledimagedata,
                               int datasizeh, int datasizev,
                               int imagesizeh, int imagesizev);
};

#endif
