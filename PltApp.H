// ---------------------------------------------------------------
// PltApp.H
// ---------------------------------------------------------------
#ifndef _PLTAPP_H_
#define _PLTAPP_H_

#include <ParallelDescriptor.H>

#include <Xm/Xm.h>
#include <X11/Intrinsic.h>
#undef index

#include <AmrvisConstants.H>
#include <Box.H>
#include <Dataset.H>
#include <GlobalUtilities.H>
#include <Palette.H>
#if (BL_SPACEDIM==3)
# include <ViewTransform.H>
class ProjectionPicture;
#endif

#include <vector>
#include <string>
using std::vector;
using std::string;

class DataServices;
class AmrPicture;
class Dataset;
class GraphicsAttributes;
class PltAppState;
class XYPlotParameters;
class XYPlotWin;
class XYPlotDataList;


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

const int OXY(0);
const int OYZ(1);
const int OXZ(2);

class PltApp;
typedef void (PltApp::*memberCB)(Widget, XtPointer, XtPointer);

class PltApp {
    
private:
  PltApp();  // not defined
  
public:
  // created when opening a new file
  PltApp(XtAppContext, Widget, const string &filename, 
         const Array<DataServices *> &dataservicesptr, bool isAnim);
  
  // for subregion
  PltApp(XtAppContext app, Widget w, const Box &region,
         const IntVect &offset,
         //AmrPicture *parentPtr,
	 PltApp *pltparent,
         const string &palfile, bool isAnim,
         const string &newderived, const string &file);
  
  ~PltApp();
  
  static void SetDefaultPalette(const string &palString);
  static void SetDefaultLightingFile(const string &lightFileString);
  static void SetInitialDerived(const string &initialderived);
  static void SetInitialScale(int initScale);
  static void SetInitialMaxMenuItems(int initmaxitems);
  static int  GetInitialScale();
  static void SetInitialFormatString(const string &formatString);
  static void SetDefaultShowBoxes(int showBoxes);
  static int  GetDefaultShowBoxes();
  static void SetInitialWindowHeight(int initWindowHeight);
  static void SetInitialWindowWidth(int initWindowWidth);
  static void SetReserveSystemColors(int reservesystemcolors);
  
  Widget WId()                     { return wAmrVisTopLevel; }
  void SetNewFormatString(const string &newformatstring);
  AmrPicture *GetAmrPicturePtr(int plane) { return amrPicturePtrArray[plane]; }
  Palette *GetPalettePtr()         { return pltPaletteptr; }
  string GetPaletteName()         { return palFilename; }
  string GetLightingFileName()         { return lightingFilename; }
  DataServices *GetDataServicesPtr() { return dataServicesPtr[currentFrame]; }
  Array<DataServices *> &GetDataServicesPtrArray() { return dataServicesPtr; }
  int GetActiveView() const        { return activeView;    }
  XtAppContext GetAppContext()     { return appContext;    }
  Widget GetStopButtonWidget()     { return wControls[WCSTOP]; }
  bool Animating()    const        { return ((bool) animationIId); }
  int CurrentFrame()  const        { return currentFrame; }
  XImage *CurrentFrameXImage()     { return frameBuffer[currentFrame]; }
  GraphicsAttributes *GetGAptr() const  { return gaPtr; }
  PltAppState *GetPltAppState() const { return pltAppState; }

  const string &GetFileName();
  bool  PaletteDrawn();
  bool  PaletteCBQ() const { return paletteCBQ; }
  void  SetPaletteCBQ() { paletteCBQ = true; }
  void  ClearPaletteCBQ() { paletteCBQ = false; }
  Widget GetPalArea() const { return wPalArea; }
  void  PaletteDrawn(bool trueOrFalse);
  
  XYPlotDataList *CreateLinePlot(int V, int sdir, int mal, int ixY,
				 const string *derived);
  GC GetRbgc() const { return rbgc; }

  void QuitDataset();
  void DoBackStep(int);
  void DoForwardStep(int);
  void DoExposeRef(Widget = None, XtPointer = NULL, XtPointer = NULL);

  string GetMPIFName(Real r);
  string GetRegionName(Real r);
  bool IsTimeline() const { return bTimeline; }
  bool IsRegions() const  { return bRegions; }

#if (BL_SPACEDIM == 3)
  void DoExposeTransDA(Widget = None, XtPointer = NULL, XtPointer = NULL);
  ProjectionPicture *GetProjPicturePtr() const { return projPicturePtr; }
  Widget GetWTransDA() const { return wTransDA; }
#endif

  void DetachXYPlotWin(int dir) { XYplotwin[dir] = NULL; }
  XYPlotParameters *GetXYPlotParameters() const { return XYplotparameters; }
  XYPlotWin *GetXYPlotWin(int dir) const { return XYplotwin[dir]; }

private:
  Widget wTopLevel, wAmrVisTopLevel;
  Widget wScrollArea[Amrvis::NPLANES], wPlotPlane[Amrvis::NPLANES];
  Array<Widget> wRangeRadioButton;
  Widget wFileRangeCheckBox;
  Widget wUserMin, wUserMax;
  Widget wWhichFileLabel, wWhichFileScale, wWhichTimeLabel;
  Widget wControlForm;
  Widget wNumberContours;
  Widget wContoursTopLevel, wInfoTopLevel;
  Widget wSetRangeTopLevel, wNumberFormatTopLevel;
  Widget wFormat;
  Widget wLocArea, wPlotArea;
  Widget wControls[WCN];
  Widget wCurrScale, wCurrLevel, wCurrDerived;
  Widget wPlotLabel, wContourLabel;
  Widget wPalArea;

#if (BL_SPACEDIM == 3)
  Widget wTransDA, wRender, wLabelAxes, wDetach;
  Widget wOrientXY, wOrientYZ, wOrientXZ;
  Widget wLWTopLevel, wLWForm;
  Widget wAttach, wDetachTopLevel;
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  Widget wAutoDraw;
  Widget wLWambient, wLWdiffuse, wLWspecular, wLWshiny;
  Widget wLWminOpacity, wLWmaxOpacity;
  Widget wCurrentRenderMode, wCurrentClassify;
#endif
#endif

  XYPlotParameters *XYplotparameters;
  XYPlotWin *XYplotwin[BL_SPACEDIM];

  XtAppContext appContext;
  Arg args[Amrvis::MAXARGS];
  Palette *pltPaletteptr;
  AmrPicture *amrPicturePtrArray[Amrvis::NPLANES];
  IntVect ivLowOffsetMAL;  // at maxAllowableLevel
  Amrvis::MinMaxRangeType currentRangeType;
  bool animating2d, setRangeShowing, contoursShowing, infoShowing, bFormatShowing;
  bool bSetRangeRedraw;
  bool datasetShowing, paletteDrawn, writingRGB, interfaceReady;
  bool paletteCBQ;
  bool bTimeline, bRegions;
  int  timelineMin, timelineMax;
  int  regionsMin, regionsMax;
  map<int, string> mpiFNames;
  map<int, string> regNames;
  vector<string> nameTagNames;
  Real nameTagMultiplier;
  int servingButton;
  int currentFrame, animFrames, frameSpeed;
  int activeView;
  int startX, startY, endX, endY;
  int maxAllowableScale;
  bool bCartGridSmoothing;
  string contourNumString, fileName, palFilename, lightingFilename, headerSuffix;
  char buffer[BUFSIZ];
  Box trueRegion, selectionBox;
  Dataset *datasetPtr;
  GraphicsAttributes	*gaPtr;
  Array<DataServices *> dataServicesPtr;
  Array<string> fileNames;
  Array<XImage *> frameBuffer;
  vector<bool>	readyFrames;
  Amrvis::AnimDirection	animDirection;
  XtIntervalId	animationIId, multiclickIId;
  Real finestDx[BL_SPACEDIM], gridOffset[BL_SPACEDIM];
  String trans;
  int startcutX[3], startcutY[3], finishcutX[3], finishcutY[3];
  PltApp *pltParent;
  PltAppState *pltAppState;
  Display *display;
  GC xgc;
  bool bFileRangeButtonSet, bSyncFrame;
  void DoToggleFileRangeButton(Widget, XtPointer, XtPointer);
  
  // baggage for fast rubber banding
  GC		rbgc;
  XEvent	nextEvent;
  int 		rWidth, rHeight, rStartX, rStartY;
  Cursor	cursor;
    
  static string defaultPaletteString, initialDerived, initialFormatString;
  static string defaultLightingFilename;
  static int initialMaxMenuItems;
  static int initialScale, initialWindowHeight, initialWindowWidth;
  static int defaultShowBoxes, placementOffsetX, placementOffsetY;
  static int reserveSystemColors;
  
#if (BL_SPACEDIM==3)
  int acc, labelAxes, transDetached;
  int daWidth, daHeight;
  ProjectionPicture *projPicturePtr;
  ViewTransform	viewTrans;
//#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  bool lightingModel, showing3dRender, preClassify, lightingWindowExists;
//#endif

#endif
  
  void PltAppInit(bool bSubVolume = false);           // called by constructors

  void DoDrawPointerLocation(Widget, XtPointer, XtPointer);
  void ChangeScale(Widget, XtPointer, XtPointer);
  void ChangeLevel(Widget, XtPointer, XtPointer);
  void ChangeDerived(Widget, XtPointer, XtPointer);
  void ChangeContour(Widget, XtPointer, XtPointer);
  void ChangePlane(Widget, XtPointer, XtPointer);

  void DrawAxes(Widget, int, int, int, char *, char *, int);
  void ResetAnimation();
  void StopAnimation();
  void Animate(Amrvis::AnimDirection direction);
  void ShowFrame();
  void DirtyFrames();
  void DoRubberBanding(Widget, XtPointer, XtPointer);
  void DoBoxesButton(Widget, XtPointer, XtPointer);
  void DoAnimBackStep();
  void DoAnimForwardStep();
  void DoAnimFileScale(Widget, XtPointer, XtPointer);
    
  void DoReadContourString(Widget, XmSelectionBoxCallbackStruct *unused,
			   bool bRedrawAmrPicture = true);
  
  void DoUpdateFrame(Widget, XtPointer, XtPointer);
  void DoSpeedScale(Widget, XtPointer, XtPointer);
  void DoDatasetButton(Widget, XtPointer, XtPointer);
  void DoNumberFormatButton(Widget, XtPointer, XtPointer);
  void DoPaletteButton(Widget, XtPointer, XtPointer);
  void DoSetRangeButton(Widget, XtPointer, XtPointer);
  void DoContoursButton(Widget, XtPointer, XtPointer);
  void DoDoneNumberFormat(Widget, XtPointer, XtPointer);
  void DoCancelNumberFormat(Widget, XtPointer, XtPointer);
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
  void DoOpenFileLightingWindow(Widget w, XtPointer, XtPointer call_data);
  void DoOpenLightingFile(Widget w, XtPointer, XtPointer call_data);

  void ToggleRange(Widget, XtPointer, XtPointer);
  void ReadContourString(Widget, XtPointer, XtPointer);
  void DestroyNumberFormatWindow(Widget, XtPointer, XtPointer);
  void DestroySetRangeWindow(Widget, XtPointer, XtPointer);
  void DestroyContoursWindow(Widget, XtPointer, XtPointer);
  void DestroyInfoWindow(Widget, XtPointer, XtPointer);
  void CloseInfoWindow(Widget, XtPointer, XtPointer);
  void CloseContoursWindow(Widget, XtPointer, XtPointer);
  void DoExposePalette(Widget, XtPointer, XtPointer);
  void PADoExposePicture(Widget, XtPointer, XtPointer);

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

  void FindAndSetMinMax(const Amrvis::MinMaxRangeType mmrangetype, const int framenumber,
		        const string &currentderived, const int derivednumber,
		        const Array<Box> &onBox,
		        const int coarselevel, const int finelevel,
		        const bool resetIfSet);

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
      ~CBData() { }
      PltApp *instance;
      XtPointer data;
      memberCB cbFunc;
  };
  Array<CBData *> cbdPtrs;  // save these so we can delete them later
};


// -------------------------------------------------------------------
class AVXGrab {
  public:
    AVXGrab(Display *display) : bIsGrabbed(true), cachedDisplay(display) {
      XSync(cachedDisplay, False);
      XGrabServer(display);
      XSync(cachedDisplay, False);
    }

    ~AVXGrab() {
      if(bIsGrabbed) {
        std::cout << "_______in ~AVXGrab:  implicit ungrab." << std::endl;
      }
      XUngrabServer(cachedDisplay);
      XSync(cachedDisplay, False);
      bIsGrabbed = false;
    }

    void ExplicitUngrab() {
      if(bIsGrabbed == false) {
        std::cout << "_______in AVXGrab::ExplicitUngrab:  server not grabbed."
                  << std::endl;
      }
      XUngrabServer(cachedDisplay);
      XSync(cachedDisplay, False);
      bIsGrabbed = false;
    }

  private:
    AVXGrab();  // not implemented
    bool bIsGrabbed;
    Display *cachedDisplay;
};
// -------------------------------------------------------------------


void	SubregionPltApp(Widget toplevel, const Box &region, const IntVect &offset,
			PltApp *pltparent, const string &palfile,
			int isanim, const string &derived,
			const string &filename);
void	CBQuitPltApp(Widget, XtPointer, XtPointer);
void	CBQuitAll(Widget, XtPointer, XtPointer);
void	PrintMessage(char *cpmsg);

#endif
