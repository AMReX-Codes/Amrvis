// ---------------------------------------------------------------
// ProfApp.H
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
#include <AMReX_BLProfStats.H>

#include <vector>
#include <string>
#include <map>
using std::vector;
using std::string;
using std::map;

class DataServices;
class RegionPicture;
class GraphicsAttributes;
class Palette;
class PltApp;
class PltAppState;
class XYPlotWin;


class ProfApp;
typedef void (ProfApp::*profMemberCB)(Widget, XtPointer, XtPointer);

class ProfApp : public AVPApp {

public:

//------------------------------------------------------------
  // ProfApp class to store the click history for replay.
  // Stores region clicked on and whether added or removed.
  // Can be replayed when corresponding feature is implmented to
  // generate appropriate TimeRanges.
  class ClickHistory {

     // Private struct that stores the clicks done by the user.
     // Stores region clicked on and whether it is removed or added. 
     struct Click {
       Click()
         : dataValueIndex(-1), rtri(-1), doAdd(1)
       { }
       Click(int dvi, int ri, bool add)
         : dataValueIndex(dvi), rtri(ri), doAdd(add)
       { }

       int dataValueIndex;    // Which region type? (Y on region plot)
       int rtri;              // Which time through that region? (X on region plot, RegionTimeRangeIndex)
       bool doAdd;            // AddPiece if true, RemovePiece if false.
     };

    public:

    // Default constructor starts with "allOn" to match behavior of a new GUI window.
    ClickHistory()
      : timeRangesInit(false), reset(true), replayFromAllOn(true), wasSubset(false)
    { guiHistory.clear(); }

    // Copy constructor used for subregions. 
    ClickHistory(const ClickHistory& ch)
      : timeRangesInit(ch.timeRangesInit), reset(ch.reset), replayFromAllOn(ch.replayFromAllOn), wasSubset(ch.wasSubset)
    { guiHistory.clear(); }

    // Functions to store click data
    // ----------------------------
    // Used with "allOn" and "allOff" buttons.
    void SetInit( bool init ) { timeRangesInit = init; }
    void SetSubset( bool subset ) { wasSubset = subset; }
    void RestartOn()  { guiHistory.clear(); reset = true; replayFromAllOn = true; }
    void RestartOff() { guiHistory.clear(); reset = true; replayFromAllOn = false; }
    // Store a button click in the list. 
    void Store(int region, int regionNum, bool bStartOn)
    { guiHistory.push_back( Click(region, regionNum, bStartOn) ); }

    // Functions to replay history
    // ----------------------------
    bool IsInitialized() { return timeRangesInit; }
    bool IsReset() { return reset; }
    bool IsOn() { return replayFromAllOn; }
    bool WasSubset() { return wasSubset; }
    // Replay the top click from the list.
    bool Replay(int &dvi, int &ri, bool &add)
    {
      reset = false;           // If ever replaying, assume continue from this state.

      if (guiHistory.empty()) { return 0; }  // Signals replay is complete (or empty).

      dvi = guiHistory.front().dataValueIndex;
      ri  = guiHistory.front().rtri;
      add = guiHistory.front().doAdd;
      guiHistory.pop_front();
      return 1;
    }

    private:
    bool timeRangesInit;         // True if timeRanges have been read and initialized.
    bool reset;                  // True if GUI was reset to AllOn or AllOff,
                                 // false if continuing from previous work.
    bool replayFromAllOn;        // True if replay starts with all regions on.
                                 // False if replay starts with all regions off.
    bool wasSubset;              // True if subset since the last replay.
    std::list<Click> guiHistory; // List of clicks performed on the GUI. 
  };
//------------------------------------------------------------
 
private:
  ProfApp();  // not defined
  
public:
  ProfApp(XtAppContext app, Widget w, const string &filename, 
         const amrex::Vector<amrex::DataServices *> &profdataservicesptr);
  
  // ---- for subregions
  ProfApp(XtAppContext app, Widget w, const amrex::Box &region,
          const amrex::IntVect &offset,
          ProfApp *profparent, const string &palfile,
          const string &filename);

  virtual ~ProfApp();
  
  static void SetInitialWindowHeight(int initWindowHeight);
  static void SetInitialWindowWidth(int initWindowWidth);
  
  Palette *GetPalettePtr()         { return pltPaletteptr; }
  XtAppContext GetAppContext()     { return appContext; }
  GraphicsAttributes *GetGAptr() const  { return gaPtr; }
  int GetCurrentScale() const      { return currentScale; }
 
  amrex::XYPlotDataList *CreateLinePlot(int /*V*/, int /*sdir*/, int /*mal*/,
                                        int /*ixY*/, const std::string * /*derived*/)
                                        { amrex::Abort("AVPApp not implemented."); 
                                          return nullptr; }
 
  amrex::XYPlotDataList *CreateLinePlot(const string &derived, int dIndex);
  void DoExposeRef(Widget = None, XtPointer = nullptr, XtPointer = nullptr);
  void DoExposePalette(Widget, XtPointer, XtPointer);
  void DoExposePicture(Widget, XtPointer, XtPointer);
  void DoFuncListClick(Widget, XtPointer, XtPointer);
  void DoGenerateFuncList(Widget, XtPointer, XtPointer);
  void DoGenerateTimeline(Widget, XtPointer, XtPointer);
  void DoSendRecvList(Widget, XtPointer, XtPointer);
  void DoRegionTimePlot(Widget, XtPointer, XtPointer);
  void DoSendsPlotfile(Widget, XtPointer, XtPointer);
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
  void ReplayClickHistory();

private:
  Widget wTopLevel;
  Widget wScrollArea, wPlotPlane;
  Widget wControlForm;
  Widget wInfoTopLevel;
  Widget wPlotArea;
  Widget wPlotLabel;
  Widget wFuncListButton, wAllOnButton, wAllOffButton;
  Widget wTimelineButton, wSendRecvButton, wRegionTimePlotButton;
  Widget wSendsPlotButton;
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
  String trans;
  Display *display;
  //GC xgc;
  std::list<PltApp *> pltAppList;
  RegionPicture *regionPicturePtr;
  map<int, string> regNames;
  int currentScale, maxAllowableScale;
  int startX, startY, endX, endY;
  int sdLineXL, sdLineXH, axisLengthX, axisLengthY;
  ClickHistory clickHistory;
  amrex::Vector<amrex::Vector<BLProfStats::TimeRange>> dtr;
//  amrex::Vector<std::list<BLProfStats::TimeRange>> compareTR;
  amrex::Vector<amrex::Vector<amrex::Vector<BLProfStats::TimeRange>>> rtr;    
  amrex::Vector<std::list<BLProfStats::TimeRange>> filterTimeRanges;
  amrex::Vector<std::string> funcSelectionStrings;
  amrex::Vector<amrex::Vector<BLProfStats::FuncStat>> aFuncStats;
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
  amrex::Vector<CBData *> cbdPtrs;  // ---- save these so we can delete them later
};


void SubregionProfApp(Widget toplevel, const amrex::Box &region,
                      const amrex::IntVect &offset,
                      ProfApp *pltparent, const string &palfile,
                      const string &filename);
void CBQuitProfApp(Widget, XtPointer, XtPointer);
void CBQuitAll(Widget, XtPointer, XtPointer);
void PrintMessage(const char *cpmsg);

#endif
