// -------------------------------------------------------------------
// Dataset.H
// -------------------------------------------------------------------
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

#include <iostream.h>
#include <strstream.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

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

  private:
	Widget		wAmrVisTopLevel, wDatasetTopLevel, wDatasetForm;
	Widget		wDatasetTools, wScrollArea, wPixArea;
	Widget		wFormat, wColorButton, wQuitButton;
	Arg		args[MAXARGS];
	Box		*datasetRegion;
	unsigned int	pixSizeX, pixSizeY;
	int		finestLevel, dataItemWidth;
	int		maxAllowableLevel, stringOk;
	int		noData, showColor, hDIR, vDIR, sDIR, numStrings;
	char		buffer[BUFSIZ], dataString[MAXSTRINGWIDTH];
	aString		formatString;
	StringLoc	*dataStringArray;
	AmrPicture	*amrPicturePtr;
	DataServices	*dataServicesPtr;
	PltApp		*pltAppPtr;
	GraphicsAttributes	*GAptr;
	int 		dragging, drags;

	void DrawGrid(int start_x, int start_y,
			int finish_x, int finish_y, int refRatio);
};


class StringLoc {
  public:
    StringLoc()  { }
    ~StringLoc() { }
    char ds[MAXSTRINGWIDTH];
    int xloc, yloc, color, olflag, dslen;
};

#endif



