//BL_COPYRIGHT_NOTICE

//
// $Id: PltApp.H,v 1.34 2000-04-21 17:54:06 car Exp $
//

// ---------------------------------------------------------------
// PltApp.H
// ---------------------------------------------------------------
#ifndef _PLTAPP_H_
#define _PLTAPP_H_

#include <Xm.h>
#include <Intrinsic.h>
#undef index

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

#include "XYPlotWin.H"
#include "XYPlotParam.H"

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
const int WCARGB  = 10;

const int WCN     = 11;  // number of control widget

const int NFRAMES = 19;
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
         const Array<DataServices *> &dataservicesptr, bool isAnim);
  
  // for subregion
  PltApp(XtAppContext app, Widget w, const Box &region,
         const IntVect &offset,
         AmrPicture *parentPtr, PltApp *pltparent,
         const aString &palfile, bool isAnim,
         const aString &newderived, const aString &file);
  
  ~PltApp();
  
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
  aString GetFormatString() const  { return formatString; }
  void SetFormatString(aString str) { formatString = str; }
  AmrPicture *GetAmrPicturePtr(int plane) { return amrPicturePtrArray[plane]; }
  Palette *GetPalettePtr()         { return pltPaletteptr; }
  aString GetPaletteName()         { return palFilename; }
  DataServices *GetDataServicesPtr() { return dataServicesPtr[currentFrame]; }
  Array<DataServices *> &GetDataServicesPtrArray() { return dataServicesPtr; }
  int GetActiveView() const        { return activeView;    }
  XtAppContext GetAppContext()     { return appContext;    }
  Widget GetStopButtonWidget()     { return wControls[WCSTOP]; }
  bool IsAnim()       const        { return animating2d; }
  bool Animating()    const        { return ((bool) animationIId); }
  int CurrentFrame()  const        { return currentFrame; }
  XImage *CurrentFrameXImage()     { return frameBuffer[currentFrame]; }
  Real GlobalMin()    const        { return globalMin; }
  Real GlobalMax()    const        { return globalMax; }
  int CurrentScale()  const        { return currentScale; }
  int PreviousScale() const        { return previousScale; }
  int MinAllowableLevel() const    { return minAllowableLevel; }
  int MinDrawnLevel() const        { return minDrawnLevel; }
  int MaxDrawnLevel() const        { return maxDrawnLevel; }
  bool GetShowBoxes() const        { return showBoxes; }
  void SetShowBoxes(bool showing)  { showBoxes = showing; }
  GraphicsAttributes * GetGAptr()  { return GAptr; }

  const aString &GetFileName();
  bool  PaletteDrawn();
  bool  PaletteCBQ() { return paletteCBQ; }
  void  SetPaletteCBQ() { paletteCBQ = true; }
  void  ClearPaletteCBQ() { paletteCBQ = false; }
  Widget GetPalArea() { return wPalArea; }
  void  PaletteDrawn(bool trueOrFalse);
  
  XYPlotDataList* CreateLinePlot(int V, int sdir, int mal, int ixY,
				 aString *derived);
  void  SetGlobalMinMax(Real specifiedMin, Real specifiedMax);
  GC GetRbgc() { return rbgc; }

  void QuitDataset();
  void DoBackStep(int);
  void DoForwardStep(int);
  void DoExposeRef(Widget = None, XtPointer = NULL, XtPointer = NULL);

#if (BL_SPACEDIM == 3)
  void DoExposeTransDA(Widget = None, XtPointer = NULL, XtPointer = NULL);
  ProjectionPicture *GetProjPicturePtr() { return projPicturePtr; }
  Widget GetWTransDA() { return wTransDA; }
#endif

  void DetachXYPlotWin(int dir) { XYplotwin[dir] = NULL; }
  class XYPlotParameters *GetXYPlotParameters() { return XYplotparameters; }
  class XYPlotWin * GetXYPlotWin(int dir) { return XYplotwin[dir]; }

private:
  Widget wTopLevel, wAmrVisTopLevel;
  Widget wScrollArea[NPLANES], wPlotPlane[NPLANES];
  Widget wRangeRadioButton[NRANGES];
  Widget wUserMin, wUserMax;
  Widget wWhichFileLabel, wWhichFileScale, wWhichTimeLabel;
  Widget wControlForm;
  Widget wNumberContours;
  Widget wContoursTopLevel, wInfoTopLevel, wSetRangeTopLevel;
  Widget wLocArea, wPlotArea;
  Widget wControls[WCN];
  Widget wCurrScale, wCurrLevel, wCurrDerived;
  Widget wPlotLabel, wContourLabel;
  Widget wPalArea;

#if (BL_SPACEDIM == 3)
  Widget wTransDA, wRender, wLabelAxes, wOrient, wDetach;
  Widget wLWTopLevel, wLWForm;
  Widget wAttach, wDetachTopLevel;
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  Widget wAutoDraw;
  Widget wLWambient, wLWdiffuse, wLWspecular, wLWshiny;
  Widget wLWminOpacity, wLWmaxOpacity;
  Widget wCurrentRenderMode, wCurrentClassify;
#endif
#endif

  class XYPlotParameters *XYplotparameters;
  class XYPlotWin *XYplotwin[BL_SPACEDIM];

  XtAppContext appContext;
  Arg args[MAXARGS];
  Palette *pltPaletteptr;
  AmrPicture *amrPicturePtrArray[NPLANES];
  IntVect ivLowOffsetMAL;  // at maxAllowableLevel
  Range rangeType;
  bool animating2d, setRangeShowing, contoursShowing, infoShowing,
    datasetShowing, paletteDrawn, showBoxes,  writingRGB, interfaceReady,
    paletteCBQ;
  int servingButton;
  int numContours;
  int minDrawnLevel, maxDrawnLevel, minDataLevel, maxDataLevel;
  int currentFrame, animFrames, frameSpeed;
  int currentContour;
  int activeView;
  int startX, startY, endX, endY;
  int currentScale, previousScale, maxAllowableScale;
  int minAllowableLevel;
  bool bCartGridSmoothing;
  aString contourNumString, formatString, fileName, palFilename, currentDerived;
  char buffer[BUFSIZ];
  Box	trueRegion, selectionBox;
  Dataset *datasetPtr;
  GraphicsAttributes	*GAptr;
  Array<DataServices *> dataServicesPtr;
  Array<aString> fileNames, derivedStrings;
  Array<XImage *> frameBuffer;
  Array<bool>	readyFrames;
  // Array<XYPlotDataList *> 
  AnimDirection	animDirection;
  XtIntervalId	animationIId, multiclickIId;
  Real		globalMin, globalMax;
  Real finestDx[BL_SPACEDIM],
    gridOffset[BL_SPACEDIM];
  String trans;
  int startcutX[3], startcutY[3], finishcutX[3], finishcutY[3];
  PltApp *pltParent;
  
  // baggage for fast rubber banding
  GC		rbgc;
  XEvent	nextEvent;
  int 		rWidth, rHeight, rStartX, rStartY;
  Cursor	cursor;
    
  static aString defaultPaletteString,
    initialDerived,
    initialFormatString;
  static int initialScale,
    initialWindowHeight,
    initialWindowWidth,
    defaultShowBoxes,
    placementOffsetX,
    placementOffsetY,
    reserveSystemColors;
  
#if (BL_SPACEDIM==3)
  int acc, labelAxes, transDetached;
  int daWidth, daHeight;
  ProjectionPicture	*projPicturePtr;
  ViewTransform	viewTrans;
//#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  bool lightingModel, showing3dRender, preClassify, lightingWindowExists;
//#endif

#endif
  
  void PltAppInit();           // called by constructors

  void DoDrawPointerLocation(Widget, XtPointer, XtPointer);
  void ChangeScale(Widget, XtPointer, XtPointer);
  void ChangeLevel(Widget, XtPointer, XtPointer);
  void ChangeDerived(Widget, XtPointer, XtPointer);
  void ChangeContour(Widget, XtPointer, XtPointer);
  void ChangePlane(Widget, XtPointer, XtPointer);
  void SetNumContours(bool bRedrawAmrPicture = true);

  void DrawAxes(Widget, int, int, int, char *, char *, int);
  void ResetAnimation();
  void StopAnimation();
  void Animate(AnimDirection direction);
  void ShowFrame();
  void DirtyFrames();
  void DoRubberBanding(Widget, XtPointer, XtPointer);
  void DoBoxesButton(Widget, XtPointer, XtPointer);
  void DoAnimBackStep();
  void DoAnimForwardStep();
  void DoAnimFileScale(Widget, XtPointer, XtPointer);
    
  void DoReadContourString(Widget, XmSelectionBoxCallbackStruct *unused,
			   bool bRedrawAmrPicture = true);
  int CurrentContour() { return currentContour; }
  void SetCurrentContour(int newContour) { currentContour = newContour; }
  
  void DoUpdateFrame(Widget, XtPointer, XtPointer);
  void DoSpeedScale(Widget, XtPointer, XtPointer);
  void DoDatasetButton(Widget, XtPointer, XtPointer);
  void DoPaletteButton(Widget, XtPointer, XtPointer);
  void DoSetRangeButton(Widget, XtPointer, XtPointer);
  void DoContoursButton(Widget, XtPointer, XtPointer);
  void DoDoneSetRange(Widget, XtPointer, XtPointer);
  void DoCancelSetRange(Widget, XtPointer, XtPointer);
  void DoUserMin(Widget, XtPointer, XtPointer);
  void DoUserMax(Widget, XtPointer, XtPointer);
  void DoOpenPalFile(Widget, XtPointer, XtPointer);
  void DoSubregion(Widget, XtPointer, XtPointer);
  void DoInfoButton(Widget, XtPointer, XtPointer);
  void DoOutput(Widget, XtPointer, XtPointer);
  void DoCreatePSFile(Widget, XtPointer, XtPointer);
  void DoCreateRGBFile(Widget, XtPointer, XtPointer);
  void DoCreateFABFile(Widget, XtPointer, XtPointer);
  void DoCreateAnimRGBFile();

  void ToggleRange(Widget, XtPointer, XtPointer);
  void ReadContourString(Widget, XtPointer, XtPointer);
  void DestroySetRangeWindow(Widget, XtPointer, XtPointer);
  void DestroyContoursWindow(Widget, XtPointer, XtPointer);
  void DestroyInfoWindow(Widget, XtPointer, XtPointer);
  void CloseInfoWindow(Widget, XtPointer, XtPointer);
  void CloseContoursWindow(Widget, XtPointer, XtPointer);
  void DoExposePalette(Widget, XtPointer, XtPointer);
  void DoExposePicture(Widget, XtPointer, XtPointer);

#if (BL_SPACEDIM==3)
  void DoOrient(Widget, XtPointer, XtPointer);
  void DoLabelAxes(Widget, XtPointer, XtPointer);
  void DoTransInput(Widget, XtPointer, XtPointer);
  void DoTransResize(Widget, XtPointer, XtPointer);
  void DoAttach(Widget, XtPointer, XtPointer);
  void DoDetach(Widget, XtPointer, XtPointer);
#if defined(BL_VOLUMERENDER)
  void DoRender(Widget = None, XtPointer = NULL, XtPointer = NULL);
  void DoRenderModeMenu(Widget, XtPointer, XtPointer);
  void DoClassifyMenu(Widget, XtPointer, XtPointer);
  void DoAutoDraw(Widget, XtPointer, XtPointer);
  void DoCreateLightingWindow(Widget, XtPointer, XtPointer);
  void DoDoneLightingWindow(Widget, XtPointer, XtPointer);
  void DoApplyLightingWindow(Widget, XtPointer, XtPointer);
  void DoCancelLightingWindow(Widget, XtPointer, XtPointer);
  void DestroyLightingWindow(Widget, XtPointer, XtPointer);
#endif
  void Clear();
#endif

  // RefArea (axis pictures) 

  void SetHVLine(AmrPicture **);

  void AddStaticCallback(Widget w, String whichcb, memberCB whichf, void *d = NULL);
  void AddStaticEventHandler(Widget w, EventMask mask, memberCB cbf, void *d = NULL);
  XtIntervalId AddStaticTimeOut(int time, memberCB whichf, void *d = NULL);
  static void StaticCallback(Widget, XtPointer, XtPointer);
  static void StaticEvent(Widget w, XtPointer client_data, XEvent *event, char*);
  static void StaticTimeOut(XtPointer client_data, XtIntervalId *);
  
  class CBData {  // callback data
  public:
    CBData(PltApp *p, XtPointer d, memberCB mcb)
      : instance(p), data(d), cbFunc(mcb) { }
    PltApp *instance;
    XtPointer data;
    memberCB cbFunc;
  };
};

void	SubregionPltApp(Widget toplevel, const Box &region, const IntVect &offset,
			AmrPicture *app, PltApp *pltparent, const aString &palfile,
			int isanim, const aString &derived,
			const aString &filename);
void	CBQuitPltApp(Widget, XtPointer, XtPointer);
void	PrintMessage(char *);

#endif
