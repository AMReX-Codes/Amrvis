
//
// $Id: Dataset.H,v 1.32 2010-12-15 23:10:27 vince Exp $
//

// ---------------------------------------------------------------
// Dataset.H
// ---------------------------------------------------------------
#ifndef _DATASET_H_
#define _DATASET_H_

#include "ParallelDescriptor.H"

#include <Xm/Xm.h>
#undef index

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
  GraphicsAttributes	*gaPtr;
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
  Atom          WM_DELETE_WINDOW;

  
  Pixel blackIndex, whiteIndex;
  
  void DrawGrid(int startX, int startY, int finishX, int finishY,
                int gridspacingX, int gridspacingY,
                Pixel foreground, Pixel background);
  void DrawGrid(int startX, int startY, int finishX, int finishY,
                int refRatio,
                Pixel foreground, Pixel background);
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
