// -------------------------------------------------------------------
// AmrPicture.H
// -------------------------------------------------------------------
#ifndef _AMRPICTURE_H
#define _AMRPICTURE_H

#include <fcntl.h>
#include <iostream.h>
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

	AmrPicture(int view, int mindrawnlevel, 
                   GraphicsAttributes *gaptr, Box region, 
                   AmrPicture *parentPicturePtr, 
                   PltApp *parentPltAppPtr, PltApp *pltappptr);
	// construct an AmrPicture using pointer to data & box

	~AmrPicture();
	static void CBFrameTimeOut(XtPointer, XtIntervalId *);
	void	DoFrameUpdate();
	void	DoStop();   // removes TimeOutCallback, updates GridPictures 
	void	SetRegion(int, int, int, int);
	void	CreatePicture(Window drawPictureHere, Palette *palptr,
				const aString &derived);
	void	Draw(int fromLevel, int toLevel);
        void    DrawSlice(int);
        void    UndrawSlice(int);
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
	void	DrawDatasetPoint(int hplot, int vplot, int size);
	void	UnDrawDatasetPoint();
	void	Sweep(AnimDirection direction);
        void    SetHVLine();
	void	SetHLine(int hl)          { hLine = hl; }
	void	SetVLine(int vl)          { vLine = vl; }
	int	GetHLine() const          { return hLine; }
	int	GetVLine() const          { return vLine; }
	int	GetSlice() const          { return slice; }
	const aString	&CurrentDerived() const   { return currentDerived; }
	int	NumberOfLevels()  const   { return numberOfLevels; }
	int	FinestLevel()     const   { return numberOfLevels - 1; }
	int	MaxAllowableLevel() const { return maxAllowableLevel; }
	bool	ShowingBoxes() const      { return showBoxes; }
        void    SetShowBoxes(bool b)      { showBoxes = b; }
	unsigned int ImageSizeH() const   { return imageSizeH; }
	unsigned int ImageSizeV() const   { return imageSizeV; }
	DataServices *GetDataServicesPtr()    { return dataServicesPtr; } 
	void	SetDataServicesPtr(DataServices *dsp) { dataServicesPtr = dsp; } 
	void	SetDataMin(Real min) { dataMinSpecified = min; }
	void	SetDataMax(Real max) { dataMaxSpecified = max; }
	Real	GetSpecifiedMin() const   { return dataMinSpecified; }
	Real	GetSpecifiedMax() const   { return dataMaxSpecified; }
	Real	GetMin()          const   { return dataMinAllGrids; }
	Real	GetMax()          const   { return dataMaxAllGrids; }
	Real	GetRegionMin()    const   { return dataMinRegion; }
	Real	GetRegionMax()    const   { return dataMaxRegion; }
	void	SetWhichRange(Range which);
	Range	GetWhichRange()   const   { return whichRange; }
	Real	GetWhichMin();
	Real	GetWhichMax();
	void	SetHColor(int c)               { hColor = c; }
	void	SetVColor(int c)               { vColor = c; }
	const Box  &GetSliceBox()  const       { return sliceBox[finestLevel]; }
	int	GetMyView()        const       { return myView; }
	const Array<Box> &GetSubDomain() const       { return subDomain; }
	bool	FramesMade()       const       { return framesMade; }
	void	SetFrameSpeed(int toThis)      { frameSpeed = toThis; }
	int	GetFrameSpeed()    const       { return frameSpeed; }
	GraphicsAttributes *GetGAPtr() const   { return GAptr; }
	Palette *GetPalPtr()           const   { return palPtr; }
	Real	MinUsing()             const   { return minUsing; }
	Real	MaxUsing()             const   { return maxUsing; }
	Window	PictureWindow()        const   { return pictureWindow; }
	void	SetMaxDrawnLevel(int lev)      { maxDrawnLevel = lev; }
        PltApp *GetPltAppPtr()         const   {return pltAppPtr; }

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
	Array<unsigned long>   	dataSize, dataSizeH, dataSizeV;
	unsigned int   		imageSize, imageSizeH, imageSizeV;
        aString			currentDerived;
        bool			showBoxes;
        int			minDrawnLevel, maxDrawnLevel, maxAllowableLevel;
	int			maxLevelWithGrids, maxsFound;
	int			hdspoint, vdspoint, dsBoxSize;
	int			datasetPointShowing, datasetPointColor;
	Pixmap 			pixMap;
	char			buffer[BUFSIZ];
	bool			pixMapCreated, isSubDomain, findSubRange;
	Array< Array<GridPicture> > gpArray;
	Array< Array< Array<GridPicture> > > frameGrids;
	PltApp			*pltAppPtr;
	DataServices		*dataServicesPtr;
	int			regionX, regionY, region2ndX, region2ndY;
	int			subCutShowing;
	int			subcutX, subcutY, subcut2ndX, subcut2ndY;
	Array<Box>		subDomain, sliceBox;
	Array<XImage *>	        frameBuffer;
	bool			framesMade;
	int			frameSpeed;
	AnimDirection		sweepDirection;
	XtIntervalId		pendingTimeOut;
	GraphicsAttributes	*GAptr;
	Palette			*palPtr;
	Array<FArrayBox *>	sliceFab;
	Array<unsigned char *>  imageData, scaledImageData;
	Array<XImage *>         xImageArray;
	Array<bool >            xImageCreated;


	// private functions
	void SetSlice(int view, int here);
	void CoarsenSliceBox();
	void ShowFrameImage(int iSlice);
        void RubberBandSlice(int iSlice);
	void CreateFrames(AnimDirection direction);
	void AmrPictureInit();
	void DrawBoxes(Array< Array<GridPicture> > &gp, Drawable &drawable);
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
