
//
// $Id: Dataset.H,v 1.28 2001-08-22 00:22:32 vince Exp $
//

// ---------------------------------------------------------------
// Dataset.H
// ---------------------------------------------------------------
#ifndef _DATASET_H_
#define _DATASET_H_

#include <Xm/Xm.h>
#undef index

#include "Dataset.H"
#include "GlobalUtilities.H"
#include "GraphicsAttributes.H"

#include "Box.H"
#include "AmrvisConstants.H"

const int MAXSTRINGWIDTH = 64;

class PltApp;
class PltAppState;
class AmrPicture;
class DataServices;
class StringLoc;	

class Dataset {
 public:
  Dataset(const Box &alignedRegion, AmrPicture *apptr,
          PltApp *pltappptr, PltAppState *pltappstateptr,
	  int hdir, int vdir, int sdir);
  
  ~Dataset();
  
  void DatasetRender(const Box &alignedRegion, AmrPicture *apptr,
		     PltApp *pltappptr, PltAppState *pltappstateptr,
		     int hdir, int vdir, int sdir);
  void DrawIndices();
  void DoRaise();
  void DoExpose(int fromExpose);
  static void CBColorButton(Widget, XtPointer, XtPointer);
  static void CBQuitButton(Widget, XtPointer, XtPointer);
  static void CBPixInput(Widget, XtPointer, XtPointer);
  static void CBEndScrolling(Widget, XtPointer, XtPointer);
  static void CBScrolling(Widget, XtPointer, XtPointer);
  static void CBDoExposeDataset(Widget, XtPointer, XEvent *, Boolean *);
  
  void DoColorButton();
  void DoQuitButton();
  void DoPixInput(XmDrawingAreaCallbackStruct *);

  AmrPicture *GetAmrPicturePtr() { return amrPicturePtr; }

 private:
  Widget	wDatasetTopLevel, wDatasetForm;
  Widget	wDatasetTools, wScrollArea, wPixArea;
  Widget        wFormat, wColorButton, wQuitButton, wLevels, wMinValue, wMaxValue;
  Arg		args[MAXARGS];
  Box		*datasetRegion;
  unsigned int	pixSizeX, pixSizeY;
  int		dataItemWidth, dataItemHeight;
  int		maxAllowableLevel, maxDrawnLevel, minDrawnLevel, stringOk;
  int		noData, hDIR, vDIR, sDIR, numStrings, *myStringCount;
  char		buffer[BUFSIZ], dataString[MAXSTRINGWIDTH];
  StringLoc	*dataStringArray, **myDataStringArray;
  bool          bDataStringArrayAllocated;
  AmrPicture	*amrPicturePtr;
  DataServices	*dataServicesPtr;
  PltApp	*pltAppPtr;
  PltAppState	*pltAppStatePtr;
  GraphicsAttributes	*GAptr;
  int 		dragging, drags;
  int		hStringOffset, vStringOffset;
  string 	hAxisString, vAxisString;
  int           indexWidth, indexHeight;
  int		hScrollBarPos, vScrollBarPos;
  int		hIndexAreaHeight, hIndexAreaStart, hIndexAreaEnd;
  int		vIndexAreaWidth,  vIndexAreaStart, vIndexAreaEnd;
  int		xh, yv;
  Dimension	width, height;
  StringLoc    **hIndexArray, **vIndexArray;    
  bool datasetPoint;
  
  Pixel blackIndex, whiteIndex;
  
  void DrawGrid(int startX, int startY, int finishX, int finishY,
                int gridspacingX, int gridspacingY,
                int foregroundIndex, int backgroundIndex);
  void DrawGrid(int startX, int startY, int finishX, int finishY,
                int refRatio,
                int foregroundIndex, int backgroundIndex);
};


class StringLoc {
public:
  StringLoc()  { }
  ~StringLoc() { }
  char ds[MAXSTRINGWIDTH];
  int xloc, yloc, color, olflag, dslen;
};

#endif
// -------------------------------------------------------------------
// -------------------------------------------------------------------
