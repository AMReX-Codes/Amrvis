// ---------------------------------------------------------------
// Dataset.H
// ---------------------------------------------------------------
#ifndef _DATASET_H_
#define _DATASET_H_

#include <AMReX_ParallelDescriptor.H>

#include <Xm/Xm.h>
#undef index

#include <GlobalUtilities.H>
#include <GraphicsAttributes.H>

#include <AMReX_Box.H>
#include <AMReX_AmrvisConstants.H>
#include <AMReX_DataServices.H>

namespace {
  const int MAXSTRINGWIDTH(64);
}

class PltApp;
class PltAppState;
class AmrPicture;
class StringLoc;	

class Dataset {
 public:
  Dataset(const amrex::Box &alignedRegion, AmrPicture *apptr,
          PltApp *pltappptr, PltAppState *pltappstateptr,
	  int hdir, int vdir, int sdir);
  
  ~Dataset();
  
  void DatasetRender(const amrex::Box &alignedRegion, AmrPicture *apptr,
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

  void SetTimeline(bool b) { bTimeline = b; }
  void SetRegions(bool b)  { bRegions  = b; }

  AmrPicture *GetAmrPicturePtr() { return amrPicturePtr; }

  static bool GetInitialColor();
  static void SetInitialColor(const bool bColor);

 private:
  Widget	wDatasetTopLevel, wDatasetForm;
  Widget	wDatasetTools, wScrollArea, wPixArea;
  Widget        wFormat, wColorButton, wQuitButton, wLevels, wMinValue, wMaxValue;
  Arg		args[amrex::Amrvis::MAXARGS];
  amrex::Box		*datasetRegion;
  unsigned int	pixSizeX, pixSizeY;
  int		dataItemWidth, dataItemHeight;
  int		maxAllowableLevel, maxDrawnLevel, minDrawnLevel, stringOk;
  int		noData, hDIR, vDIR, sDIR, numStrings, *myStringCount;
  char		buffer[BUFSIZ], dataString[MAXSTRINGWIDTH];
  StringLoc	*dataStringArray, **myDataStringArray;
  bool          bDataStringArrayAllocated;
  AmrPicture	*amrPicturePtr;
  amrex::DataServices	*dataServicesPtr;
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
  StringLoc     **hIndexArray, **vIndexArray;    
  bool          datasetPoint;
  Atom          WM_DELETE_WINDOW;
  bool          bTimeline, bRegions;

  static bool   initialColor;
  
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
