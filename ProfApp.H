// ---------------------------------------------------------------
// ProftApp.H
// ---------------------------------------------------------------
#ifndef _PROFAPP_H_
#define _PROFAPP_H_

#include <AMReX_ParallelDescriptor.H>

#include <Xm/Xm.h>
#include <X11/Intrinsic.h>
#undef index

#include <AVPApp.H>
#include <AMReX_AmrvisConstants.H>
#include <AMReX_DataServices.H>
#include <GlobalUtilities.H>
#include <BLProfStats.H>

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
class XYPlotWin;


class ProfApp;
typedef void (ProfApp::*profMemberCB)(Widget, XtPointer, XtPointer);

class ProfApp : public AVPApp {
    
private:
  ProfApp();  // not defined
  
public:
  ProfApp(XtAppContext app, Widget w, const string &filename, 
         const amrex::Array<ProfDataServices *> &profdataservicesptr);
  
  // ---- for subregions
  ProfApp(XtAppContext app, Widget w, const amrex::Box &region,
          const amrex::IntVect &offset,
          ProfApp *profparent, const string &palfile,
          const string &filename);

  virtual ~ProfApp();
  
  static void SetInitialWindowHeight(int initWindowHeight);
  static void SetInitialWindowWidth(int initWindowWidth);
  
  ProfDataServices *GetProfDataServicesPtr() { return profDataServicesPtr[0]; }
  amrex::Array<ProfDataServices *> &GetProfDataServicesPtrArray() { return profDataServicesPtr; }
  amrex::DataServices *GetDataServicesPtr() { return dspArray[0]; }
  amrex::Array<amrex::DataServices *> &GetDataServicesPtrArray() { return dspArray; }
  XtAppContext GetAppContext()     { return appContext; }
  GraphicsAttributes *GetGAptr() const  { return gaPtr; }
  int GetCurrentScale() const      { return currentScale; }
  const string &GetFileName();
  
  amrex::XYPlotDataList *CreateLinePlot(const string &derived, int dIndex);
  void DoExposeRef(Widget = None, XtPointer = nullptr, XtPointer = nullptr);
  void DoExposePalette(Widget, XtPointer, XtPointer);
  void DoExposePicture(Widget, XtPointer, XtPointer);
  void DoFuncListClick(Widget, XtPointer, XtPointer);
  void DoGenerateFuncList(Widget, XtPointer, XtPointer);
  void PopulateFuncList(bool bWriteAverage, int whichProc, bool bUseTrace);
  void DoAllOnOff(Widget, XtPointer, XtPointer);
  void ChangeScale(Widget, XtPointer, XtPointer);
  void DoRubberBanding(Widget, XtPointer, XtPointer);
  void DoOutput(Widget, XtPointer, XtPointer);
  void DoCreateHTMLTrace(Widget, XtPointer, XtPointer);
  void DoCreateTextTrace(Widget, XtPointer, XtPointer);
  void DoSubregion(Widget, XtPointer, XtPointer);

  string GetRegionName(Real r);
  int FindRegionTimeRangeIndex(int whichRegion, Real time);

private:
  Widget wTopLevel;
  Widget wScrollArea, wPlotPlane;
  Widget wControlForm;
  Widget wInfoTopLevel;
  Widget wPlotArea;
  Widget wPlotLabel;
  Widget wFuncListButton, wAllOnButton, wAllOffButton;
  Widget wFuncListScrollArea;
  Widget wFuncList, wCurrScale;

  XtAppContext appContext;
  Arg args[amrex::Amrvis::MAXARGS];
  amrex::IntVect ivLowOffset;
  bool infoShowing;
  bool interfaceReady;
  int servingButton;
  string fileName, headerSuffix;
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
  amrex::Array<amrex::Array<BLProfStats::TimeRange>> rtr;
  amrex::Array<std::list<BLProfStats::TimeRange>> filterTimeRanges;
  amrex::Array<std::string> funcSelectionStrings;
  amrex::Array<amrex::Array<BLProfStats::FuncStat>> aFuncStats;
  map<string, int> funcNameIndex;
    
  // ---- baggage for fast rubber banding
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

  void AddStaticCallback(Widget w, String whichcb, profMemberCB whichf, void *d = nullptr);
  void AddStaticEventHandler(Widget w, EventMask mask, profMemberCB cbf, void *d = nullptr);
  XtIntervalId AddStaticTimeOut(int time, profMemberCB whichf, void *d = nullptr);
  static void StaticCallback(Widget, XtPointer, XtPointer);
  static void StaticEvent(Widget w, XtPointer client_data, XEvent *event, char*);
  static void StaticTimeOut(XtPointer client_data, XtIntervalId *);
  
  class CBData {  // ---- callback data
    public:
      CBData(ProfApp *p, XtPointer d, profMemberCB mcb)
        : instance(p), data(d), cbFunc(mcb) { }
      ~CBData() { }
      ProfApp *instance;
      XtPointer data;
      profMemberCB cbFunc;
  };
  amrex::Array<CBData *> cbdPtrs;  // ---- save these so we can delete them later
};


void SubregionProfApp(Widget toplevel, const amrex::Box &region,
                      const amrex::IntVect &offset,
                      ProfApp *pltparent, const string &palfile,
                      const string &filename);
void CBQuitProfApp(Widget, XtPointer, XtPointer);
void CBQuitAll(Widget, XtPointer, XtPointer);
void PrintMessage(const char *cpmsg);

#endif
