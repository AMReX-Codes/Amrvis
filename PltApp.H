// -------------------------------------------------------------------
// PltApp.H 
// -------------------------------------------------------------------
#ifndef _PLTAPP_H_
#define _PLTAPP_H_

#include <Xm.h>
#include <Intrinsic.h>
#include <ctype.h>
#include <iostream.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strstream.h>

#include "AmrvisConstants.H"
#include "AmrPicture.H"
#include "Box.H"
#include "Dataset.H"
#include "GraphicsAttributes.H"
#include "GlobalUtilities.H"
#include "Palette.H"
#include "List.H"
#include "aString.H"
#if (BL_SPACEDIM==3)
# include "ViewTransform.H"
# include "ProjectionPicture.H"
#endif

#include "DataServices.H"
//class DataServices;

// plane sweep widget controls (WC)
const int WCSTOP = 0;
const int WCXNEG = 1;
const int WCXPOS = 2;
const int WCYNEG = 3;
const int WCYPOS = 4;
const int WCZNEG = 5;
const int WCZPOS = 6;

// file animation widget controls (WCA)
const int WCATNEG = 7;
const int WCASTOP = 8;
const int WCATPOS = 9;

const int WCN     = 10;  // number of control widget

const int NFRAMES = 20;
const int MAXDERIVED = 64;

struct ClientDataStruct {   // for passing information through client_data
  Widget w;
  XtPointer object, data;
  unsigned long tempint1, tempint2;
};

typedef void (PltApp::*memberCB)(Widget, XtPointer, XtPointer);

class PltApp {

private:
	PltApp();  // not defined

public:
	// created when opening a new file
	PltApp(XtAppContext, Widget, const aString &filename, 
	       DataServices *dataservicesptr, bool isAnim);

	// for subregion
	PltApp(XtAppContext app, Widget w, const Box &region,
	       const IntVect &offset,
	       AmrPicture *parentPtr, PltApp *pltparent,
	       const aString &palfile, bool isAnim,
	       const aString &newderived, const aString &file);

	~PltApp();

	static void	CBQuitButton(Widget, XtPointer, XtPointer);
	static void	CBChangeScale(Widget, XtPointer, XtPointer);
	static void	CBChangeLevel(Widget, XtPointer, XtPointer);
	static void	CBChangeDerived(Widget, XtPointer, XtPointer);
	static void	CBSpeedScale(Widget, XtPointer, XtPointer);
	static void	CBUpdateFrame(XtPointer, XtIntervalId *);
	static void	CBToggleRange(Widget, XtPointer, XtPointer);
	static void	CBChangePlane(Widget, XtPointer, XtPointer);

#if (BL_SPACEDIM==3)
	static void	CBTransResize(Widget, XtPointer, XtPointer);
	static void	CBTransInput(Widget, XtPointer, XtPointer);

	void		DoAutoDraw(Widget, XtPointer, XtPointer);
	void		DoExposeTransDA();
	void		DoReadTransferFile(Widget, XtPointer, XtPointer);
	void		DoOrient(Widget, XtPointer, XtPointer);
	void		DoRender(Widget, XtPointer, XtPointer);
	void		DoLabelAxes(Widget, XtPointer, XtPointer);
	void		DoTransInput(Widget, XtPointer, XtPointer);
	void		DoTransResize(Widget, XtPointer, XtPointer);
	void		DoAttach(Widget, XtPointer, XtPointer);
	void		DoDetach(Widget, XtPointer, XtPointer);
	void		Clear();
#endif

	void		DoUpdateFrame();
	void		DoSpeedScale(XmScaleCallbackStruct *);
	void		DoQuitButton(Widget, XtPointer, XtPointer);
	void		DoChangeScale(int view);
	void		DoChangeLevel(int, Widget, XtPointer, XtPointer);
	void		DoChangeDerived(int, Widget, XtPointer, XtPointer);
	void		DoDatasetButton(Widget, XtPointer, XtPointer);
	void		DoPaletteButton(Widget, XtPointer, XtPointer);
	void		DoSetRangeButton(Widget, XtPointer, XtPointer);
	void		DoDoneSetRange(Widget, XtPointer, XtPointer);
	void		DoCancelSetRange(Widget, XtPointer, XtPointer);
	void		DoUserMin(Widget, XtPointer, XtPointer);
	void		DoUserMax(Widget, XtPointer, XtPointer);
	void		DoToggleRange(Widget, Range,
					XmToggleButtonCallbackStruct *);
	void		DoOpenPalFile(Widget, XtPointer, XtPointer);
	void		DoSubregion(Widget, XtPointer, XtPointer);
	void		DoOutput(Widget, XtPointer, XtPointer);
	void		DoCreatePSFile(Widget, XtPointer, XtPointer);
	void		DoCreateRGBFile(Widget, XtPointer, XtPointer);
	void		DoCreateFABFile(Widget, XtPointer, XtPointer);
	void		QuitDataset();
	void		DoExposeRef(); // RefArea (axis pictures) 

	static void DoChangePlane(XtPointer, XtIntervalId *);
	static void SetDefaultPalette(const aString &palString);
	static void SetInitialDerived(const aString &initialderived);
	static aString &GetInitialDerived();
	static aString &GetDefaultPalette();
	static void SetInitialScale(int initScale);
	static int  GetInitialScale();
	static void SetInitialFormatString(const aString &formatString);
	static void SetDefaultShowBoxes(int showBoxes);
	static int  GetDefaultShowBoxes();
	static void SetInitialWindowHeight(int initWindowHeight);
	static void SetInitialWindowWidth(int initWindowWidth);
	static void SetReserveSystemColors(int reservesystemcolors);

	Widget WId()                     { return wAmrVisTopLevel; }
	aString GetFormatString()          { return formatString; }
	void SetFormatString(aString str)  { formatString = str; }
	AmrPicture *GetAmrPicturePtr(int plane)
					 { return amrPicturePtrArray[plane]; }
	Palette *GetPalettePtr()         { return pltPaletteptr; }
	DataServices *GetDataServicesPtr()         { return dataServicesPtr; }
	int GetActiveView()              { return activeView;    }
	XtAppContext GetAppContext()     { return appContext;    }
	Widget GetStopButtonWidget()     { return wControls[WCSTOP]; }
	int IsAnim()                     { return anim; }
	int Animating()                  { return ((int) animating); }
	int CurrentFrame()               { return currentFrame; }
	XImage *CurrentFrameXImage()     { return frameBuffer[currentFrame]; }
	Real GlobalMin()                 { return globalMin; }
	Real GlobalMax()                 { return globalMax; }
	int CurrentScale()               { return currentScale; }
	int PreviousScale()              { return previousScale; }
	int MinAllowableLevel()          { return minAllowableLevel; }

	const aString &GetFileName();
	bool  PaletteDrawn();
	void  PaletteDrawn(bool trueOrFalse);

	void  SetGlobalMinMax(Real specifiedMin, Real specifiedMax);

private:
	Widget 		wTopLevel, wAmrVisTopLevel, wAmrVisMenu;
	Widget 		wScaleMenu, wScaleOptions;
	Widget 		wLevelMenu, wLevelOptions;
	Widget		wDerivedMenu, wDerivedOptions;
	Widget 		wQuitButton, wSubregionButton;
	Widget		wPicArea, wPalArea, wPlotArea;
	Widget		wScrollArea[NPLANES], wPlotPlane[NPLANES];
	Widget		wBoxesButton, wPaletteButton, wDatasetButton;
	Widget		wCGSmoothButton, wControlArea, wSetRangeButton;
	Widget		wRangeRadioButton[NRANGES];
        Widget		wOutputMenuBar, wOutputMenu, wOutputButton;
	Widget		wPSFileButton, wFABFileButton, wRGBFileButton;
	Widget		wSetRangeTopLevel, wAxesPic;
	Widget		wSetRangeForm, wDoneButton, wCancelButton;
	Widget		wSetRangeRadioBox, wGlobalRange, wLocalRange;
	Widget		wUserMin, wUserMax, wMin, wMax;
	Widget		wSpeedScale, wLabelSlow, wLabelFast, wDetach;
	Widget		wAnimSpeedScale, wAnimLabelSlow, wAnimLabelFast;
	Widget		wWhichFileLabel, wWhichFileScale;
	Widget		wAnimControl, wControlForm, wControls[WCN];
	Widget		wFrames[NFRAMES];
	Array<Widget>	wLevelItems, wScaleItems, wDerivedItems;

	Arg		args[MAXARGS];
	Palette	       *pltPaletteptr;
	DataServices   *dataServicesPtr;
	AmrPicture     *amrPicturePtrArray[NPLANES];
	IntVect		ivLowOffsetMAL;  // at maxAllowableLevel
	Range		rangeType;
	bool		anim, setRangeShowing, datasetShowing, paletteDrawn;
	int		minDrawnLevel, maxDrawnLevel;
	int		minDataLevel,  maxDataLevel;
	aString		fileName, palFilename;
	Array<aString>	fileNames, derivedStrings;
	char		buffer[BUFSIZ];
	aString		formatString;
	int		numberOfDerived;
	int		XYyz, XYxz, XZxy, XZyz, YZxz, YZxy;
	int		servingButton, activeView;
	int		startX, startY, endX, endY;
	int		startcutX[3], startcutY[3], finishcutX[3], finishcutY[3];
	Box		trueRegion, selectionBox;
	Dataset	       *datasetPtr;
	ClientDataStruct	clientData;
	GraphicsAttributes	*GAptr;
	Array<XImage *>	frameBuffer;
	int		currentFrame, animFrames, frameSpeed;
	AnimDirection	animDirection;
	bool		interfaceReady;
	Array<bool>	readyFrames;
	XtIntervalId	animating, multiclickIId;
	Real		globalMin, globalMax;
	aString		currentDerived;
	String		trans;
	int		multiclickInterval;
	int		currentScale, previousScale, maxAllowableScale;
	int		minAllowableLevel;

	// baggage for fast rubber banding
	GC		rbgc;
	XEvent		nextEvent;
        int 		rWidth, rHeight, rStartX, rStartY;
	int		anchorX, anchorY, rectDrawn;
        int		oldX, oldY, newX, newY;
        int		rootX, rootY;  // place holders for x call
        unsigned int	inputMask;
        Window		whichRoot, whichChild;
	Cursor		cursor;
	bool		cgSmoothing;


	static aString	defaultPaletteString;
	static aString	initialDerived;
	static aString	initialFormatString;
	static int	initialScale;
	static int	initialWindowHeight;
	static int	initialWindowWidth;
	static int	defaultShowBoxes;
	static int	placementOffsetX;
	static int	placementOffsetY;
	static int	reserveSystemColors;

#if (BL_SPACEDIM==3)
	ProjectionPicture	*projPicturePtr;
	ViewTransform	viewTrans;
#endif
	Widget		wTransDA, wRender, wLabelAxes, wOrient;
	Widget		wDetachTopLevel, wDetachForm, wDetachWindowForm;
	Widget		wAttach, wDRender, wDLabelAxes, wDOrient;
	Widget		wAutoDraw, wDAutoDraw;
	Widget		wReadTransfer, wDReadTransfer;
	int		daWidth, daHeight;
	int		acc, autorender, labelAxes, transDetached;
	XtAppContext	appContext;


	void	PltAppInit();           // called by constructors
	void	DrawAxes(Widget, int, int, int, char *, char *, int);
	void	ResetAnimation();
	void	StopAnimation();
	void	Animate(AnimDirection direction);
	void	ShowFrame();
	void	DirtyFrames();
	void	DoCGSmooth(Widget, XtPointer, XtPointer);
	void	DoRubberBanding(Widget, XtPointer, XtPointer);
	void	DoBoxesButton(Widget, XtPointer, XtPointer);
	void	DoBackStep(int);
	void	DoForwardStep(int);
	void	DoAnimBackStep();
	void	DoAnimForwardStep();
	void	DoAnimFileScale(Widget, XtPointer, XtPointer);


	// these replace inidvidual static indirection callbacks
        void    AddStaticCallback(Widget w, String whichcb, memberCB whichf);
        static void     StaticCallback(Widget, XtPointer, XtPointer);

        class CBData {  // callback data
          public:
            CBData(PltApp *p, memberCB mcb): instance(p), cbFunc(mcb) { }
            PltApp *instance;
            memberCB cbFunc;
        };
};

void	SubregionPltApp(Widget toplevel, const Box &region, const IntVect &offset,
			AmrPicture *app, PltApp *pltparent, const aString &palfile,
			int isanim, const aString &derived,
			const aString &filename);
void	CBQuitPltApp(Widget, XtPointer, XtPointer);
void	CBDoExposePalette(Widget, XtPointer, XExposeEvent);
void	CBDoExposePicture(Widget, XtPointer, XExposeEvent);
void	CBDoExposeRef(Widget, XtPointer, XExposeEvent);
void	CBExposeTransDA(Widget, XtPointer, XExposeEvent);
void	PrintMessage(char *);

#endif
