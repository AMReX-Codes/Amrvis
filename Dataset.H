//BL_COPYRIGHT_NOTICE

//
// $Id: Dataset.H,v 1.18 1999-06-08 20:46:28 vince Exp $
//

// ---------------------------------------------------------------
// Dataset.H
// ---------------------------------------------------------------
#ifndef _DATASET_H_
#define _DATASET_H_

#include <Xm.h>
#include <MainW.h>
#include <RowColumn.h>
#include <DrawingA.h>
#include <PushB.h>
#include <PushBG.h>
#include <ToggleB.h>
#include <ToggleBG.h>
#include <SelectioB.h>
#include <ScrollBar.h>
#include <ScrolledW.h>
#include <Text.h>
#include <TextF.h>
#include <DialogS.h>
#include <Form.h>
#include <Label.h>

#include "GraphicsAttributes.H"
#include "AmrPicture.H"
#include "Box.H"
#include "AmrvisConstants.H"

const int MAXSTRINGWIDTH = 64;

class PltApp;
class DataServices;
class StringLoc;	

class Dataset {
 public:
  Dataset(Widget top, const Box &alignedRegion, AmrPicture *apptr,
          PltApp *pltappptr, int hdir, int vdir, int sdir);
  
  ~Dataset();
  
  void Render(const Box &alignedRegion, AmrPicture *apptr, PltApp *pltappptr,
              int hdir, int vdir, int sdir);
  void DrawIndices();
  void DoRaise();
  void DoExpose(int fromExpose);
  static void CBColorButton(Widget, XtPointer, XtPointer);
  static void CBQuitButton(Widget, XtPointer, XtPointer);
  static void CBReadString(Widget, XtPointer, XtPointer);
  static void CBPixInput(Widget, XtPointer, XtPointer);
  static void CBEndScrolling(Widget, XtPointer, XtPointer);
  static void CBScrolling(Widget, XtPointer, XtPointer);
  static void CBDoExposeDataset(Widget, XtPointer, XEvent *, Boolean *);
  
  void DoColorButton();
  void DoQuitButton();
  void DoReadString(Widget, XmSelectionBoxCallbackStruct *);
  void DoPixInput(XmDrawingAreaCallbackStruct *);

  AmrPicture *GetAmrPicturePtr() { return amrPicturePtr; }

 private:
  Widget	wAmrVisTopLevel, wDatasetTopLevel, wDatasetForm;
  Widget	wDatasetTools, wScrollArea, wPixArea;
  Widget wFormat, wColorButton, wQuitButton, wLevels, wMinValue, wMaxValue;
  Arg		args[MAXARGS];
  Box		*datasetRegion;
  unsigned int	pixSizeX, pixSizeY;
  int		finestLevel, dataItemWidth, dataItemHeight;
  int		maxAllowableLevel, maxDrawnLevel, minDrawnLevel, stringOk;
  int		noData, hDIR, vDIR, sDIR, numStrings, *myStringCount;
  char		buffer[BUFSIZ], dataString[MAXSTRINGWIDTH];
  aString	formatString;
  StringLoc	*dataStringArray, **myDataStringArray;
  bool          bDataStringArrayAllocated;
  AmrPicture	*amrPicturePtr;
  DataServices	*dataServicesPtr;
  PltApp	*pltAppPtr;
  GraphicsAttributes	*GAptr;
  int 		dragging, drags;
  int		 hStringOffset, vStringOffset;
  aString	 hAxisString, vAxisString;
  int            indexWidth, indexHeight;
  int		 hScrollBarPos, vScrollBarPos;
  int		 hIndexAreaHeight, hIndexAreaStart, hIndexAreaEnd;
  int		 vIndexAreaWidth,  vIndexAreaStart, vIndexAreaEnd;
  int		 xh, yv;
  Dimension	 width, height;
  StringLoc    **hIndexArray, **vIndexArray;    
  bool datasetPoint;
  
  int            blackIndex, whiteIndex;
  
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
