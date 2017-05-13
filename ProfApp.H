// ---------------------------------------------------------------
// ProftApp.H
// ---------------------------------------------------------------
#ifndef _PROFAPP_H_
#define _PROFAPP_H_

#include <AMReX_ParallelDescriptor.H>

#include <Xm/Xm.h>
#include <X11/Intrinsic.h>
#undef index

#include <AMReX_AmrvisConstants.H>
#include <AMReX_DataServices.H>
#include <GlobalUtilities.H>

#include <vector>
#include <string>
#include <map>
using std::vector;
using std::string;
using std::map;

class ProfDataServices;
class RegionPicture;
class GraphicsAttributes;
class Palette;
class PltApp;
class PltAppState;
class XYPlotParameters;
class XYPlotWin;


class ProfApp;
typedef void (ProfApp::*profMemberCB)(Widget, XtPointer, XtPointer);

class ProfApp {
    
private:
  ProfApp();  // not defined
  
public:
  ProfApp(XtAppContext app, Widget w, const string &filename, 
         const amrex::Array<ProfDataServices *> &profdataservicesptr);
  
  // for subregion
  ProfApp(XtAppContext app, Widget w, const amrex::Box &region,
          const amrex::IntVect &offset,
          ProfApp *profparent, const string &palfile,
          const string &filename);

  ~ProfApp();
  
  static void SetInitialWindowHeight(int initWindowHeight);
  static void SetInitialWindowWidth(int initWindowWidth);
  
  Widget WId()                     { return wAmrVisTopLevel; }
  Palette *GetPalettePtr()         { return pltPaletteptr; }
  ProfDataServices *GetProfDataServicesPtr() { return profDataServicesPtr[0]; }
  amrex::Array<ProfDataServices *> &GetProfDataServicesPtrArray() { return profDataServicesPtr; }
  amrex::DataServices *GetDataServicesPtr() { return dspArray[0]; }
  amrex::Array<amrex::DataServices *> &GetDataServicesPtrArray() { return dspArray; }
  XtAppContext GetAppContext()     { return appContext;    }
  GraphicsAttributes *GetGAptr() const  { return gaPtr; }
  int GetCurrentScale() const        { return currentScale;    }

  const string &GetFileName();
  
  amrex::XYPlotDataList *CreateLinePlot(int V, int sdir, int mal, int ixY,
				 const string *derived);
  void DoExposeRef(Widget = None, XtPointer = NULL, XtPointer = NULL);
  void DoExposePalette(Widget, XtPointer, XtPointer);
  void DoExposePicture(Widget, XtPointer, XtPointer);
  void DoFuncList(Widget, XtPointer, XtPointer);
  void DoGenerateFuncList(Widget, XtPointer, XtPointer);
  void GenerateFuncList(const amrex::Array<std::string> &funcs);
  void ChangeScale(Widget, XtPointer, XtPointer);
  void DoRubberBanding(Widget, XtPointer, XtPointer);
  void DoOutput(Widget, XtPointer, XtPointer);
  void DoCreateHTMLTrace(Widget, XtPointer, XtPointer);
  void DoCreateTextTrace(Widget, XtPointer, XtPointer);
  void DoSubregion(Widget, XtPointer, XtPointer);

  void DetachXYPlotWin(int dir) { XYplotwin[dir] = NULL; }
  XYPlotParameters *GetXYPlotParameters() const { return XYplotparameters; }
  XYPlotWin *GetXYPlotWin(int dir) const { return XYplotwin[dir]; }

private:
  Widget wTopLevel, wAmrVisTopLevel;
  Widget wScrollArea, wPlotPlane;
  Widget wControlForm;
  Widget wInfoTopLevel;
  Widget wPlotArea;
  Widget wPlotLabel;
  Widget wPalArea, wControls;
  Widget wFuncListScrollArea;
  Widget wFuncList, wCurrScale;

  XYPlotParameters *XYplotparameters;
  XYPlotWin *XYplotwin[BL_SPACEDIM];

  XtAppContext appContext;
  Arg args[amrex::Amrvis::MAXARGS];
  Palette *pltPaletteptr;
  amrex::IntVect ivLowOffset;
  bool infoShowing;
  bool interfaceReady;
  int servingButton;
  string fileName, palFilename, headerSuffix;
  char buffer[BUFSIZ];
  amrex::Box selectionBox, domainBox, subdomainBox;
  GraphicsAttributes	*gaPtr;
  amrex::Array<ProfDataServices *> profDataServicesPtr;
  amrex::Array<string> fileNames;
  String trans;
  Display *display;
  GC xgc;
  std::list<PltApp *> pltAppList;
  amrex::Array<amrex::DataServices *> dspArray;
  RegionPicture *regionPicturePtr;
  map<int, string> regNames;
  int currentScale, maxAllowableScale;
  int startX, startY, endX, endY;
  int sdLineXL, sdLineXH, axisLengthX, axisLengthY;
    
  // baggage for fast rubber banding
  GC            rbgc;
  XEvent        nextEvent;
  int           rWidth, rHeight, rStartX, rStartY;
  Cursor cursor;

  static int initialWindowHeight, initialWindowWidth;
  static int placementOffsetX, placementOffsetY;
  
  void ProfAppInit(bool bSubregion);
  void DoInfoButton(Widget, XtPointer, XtPointer);
  void DestroyInfoWindow(Widget, XtPointer, XtPointer);
  void CloseInfoWindow(Widget, XtPointer, XtPointer);

  void AddStaticCallback(Widget w, String whichcb, profMemberCB whichf, void *d = NULL);
  void AddStaticEventHandler(Widget w, EventMask mask, profMemberCB cbf, void *d = NULL);
  XtIntervalId AddStaticTimeOut(int time, profMemberCB whichf, void *d = NULL);
  static void StaticCallback(Widget, XtPointer, XtPointer);
  static void StaticEvent(Widget w, XtPointer client_data, XEvent *event, char*);
  static void StaticTimeOut(XtPointer client_data, XtIntervalId *);
  
  class CBData {  // callback data
    public:
      CBData(ProfApp *p, XtPointer d, profMemberCB mcb)
        : instance(p), data(d), cbFunc(mcb) { }
      ~CBData() { }
      ProfApp *instance;
      XtPointer data;
      profMemberCB cbFunc;
  };
  amrex::Array<CBData *> cbdPtrs;  // save these so we can delete them later
};


void    SubregionProfApp(Widget toplevel, const amrex::Box &region,
                         const amrex::IntVect &offset,
                         ProfApp *pltparent, const string &palfile,
                         const string &filename);
void	CBQuitProfApp(Widget, XtPointer, XtPointer);
void	CBQuitAll(Widget, XtPointer, XtPointer);
void	PrintMessage(const char *cpmsg);

#endif
