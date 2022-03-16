// ProfApp.cpp
// ---------------------------------------------------------------

#include <ProfApp.H>
#include <AMReX_BLBackTrace.H>
#include <AMReX_DataServices.H>
#include <AmrPicture.H>
#include <PltApp.H>
#include <PltAppState.H>
#include <GraphicsAttributes.H>
#include <XYPlotWin.H>
#include <AMReX_ParallelDescriptor.H>
#include <MessageArea.H>
#include <Palette.H>
#include <RegionPicture.H>

#include <Xm/Protocols.h>
#include <Xm/ToggleBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/FileSB.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/DrawingA.h>
#include <Xm/Text.h>
#include <Xm/ScrolledW.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <Xm/List.h>
#include <Xm/Scale.h>
#include <Xm/SeparatoG.h>
#include <Xm/Frame.h>
#include <Xm/ArrowB.h>
#include <Xm/CascadeB.h>
#include <Xm/SelectioB.h>

#include <X11/cursorfont.h>

#include <cctype>
#include <sstream>
using std::cout;
using std::cerr;
using std::endl;

#ifndef FALSE
#define FALSE false
#endif

cMessageArea profAppMessageText;

const int atiPaletteEntry(-2);
const int notInRegionPaletteEntry(-1);

// ---- widget constants
const int windowOffset(20);
const int regionPalettePad(100);
const int topLevelNx(40);
const int topLevelNy(300);
const int marginBottom(2);
const int infoWidth(400);
const int infoHeight(300);
const int plotAreaHeight(342);
const int funcListHeight(600);
const int funcListWidth(850);

using namespace amrex;

void CollectMProfStats(std::map<std::string, BLProfiler::ProfStats> &mProfStats,
                       const Vector<Vector<BLProfStats::FuncStat> > &funcStats,
                       const Vector<std::string> &fNames,
                       Real runTime, int whichProc);


// -------------------------------------------------------------------
ProfApp::~ProfApp() {
  delete XYplotparameters;
  delete pltPaletteptr;
  delete gaPtr;

  XtDestroyWidget(wAmrVisTopLevel);

  for(int nSize(0); nSize < cbdPtrs.size(); ++nSize) {
    delete cbdPtrs[nSize];
  }
}


// -------------------------------------------------------------------
ProfApp::ProfApp(XtAppContext app, Widget w, const string &filename,
	       const Vector<amrex::DataServices *> &dataservicesptr)
  : wTopLevel(w),
    appContext(app),
    fileName(filename),
    clickHistory()
{
  bool isSubRegion = false;
  int displayProc = 0;
  double initTimer = amrex::ParallelDescriptor::second();
  currentFrame = 0;
  dataServicesPtr = dataservicesptr;
  fileNames.resize(1);
  fileNames[0] = fileName;

  std::ostringstream headerout;
  headerout << AVGlobals::StripSlashes(fileNames[0]) << "   bbbbbbbbbb!!!!";

  // Set the delete response to DO_NOTHING so that it can be app defined.
  wAmrVisTopLevel = XtVaCreatePopupShell(headerout.str().c_str(), 
			 topLevelShellWidgetClass, wTopLevel,
			 XmNwidth,	    initialWindowWidth,
			 XmNheight,	    initialWindowHeight,
			 XmNx,		    topLevelNx + placementOffsetX,
			 XmNy,		    topLevelNy + placementOffsetY,
			 XmNdeleteResponse, XmDO_NOTHING,
			 NULL);

  gaPtr = new GraphicsAttributes(wAmrVisTopLevel);
  display = gaPtr->PDisplay();
  xgc = gaPtr->PGC();
  if(gaPtr->PVisual() != XDefaultVisual(display, gaPtr->PScreenNumber())) {
    XtVaSetValues(wAmrVisTopLevel, XmNvisual, gaPtr->PVisual(),
		  XmNdepth, gaPtr->PDepth(), NULL);
  }

  headerout.str(std::string());
  headerout << AVGlobals::StripSlashes(fileNames[0]) << "   aaaaaaaaaaaaaaaa!!!!";
  XtVaSetValues(wAmrVisTopLevel, XmNtitle, const_cast<char *>(headerout.str().c_str()),
		NULL);

  regionPicturePtr = new RegionPicture(gaPtr, dataServicesPtr[0]);

  ivLowOffset = IntVect::TheZeroVector();
  domainBox = regionPicturePtr->DomainBox();

  ProfAppInit(isSubRegion);

  // Should be the settings for the default constructor, but
  // repeated for consistency & guarantee desired behavior.
  clickHistory.RestartOn();
  clickHistory.SetSubset(false);

  dataServicesPtr[0]->GetRegionsProfStats().FillRegionTimeRanges(dtr, displayProc);
//  rtr = dataServicesPtr[0]->GetRegionsProfStats().GetRegionTimeRanges();

  //const amrex::Vector<amrex::Vector<amrex::Box>> &regionBoxes = regionPicturePtr->RegionBoxes();
  //for(int r(0); r < regionBoxes.size(); ++r) {
    //for(int t(0); t < regionBoxes[r].size(); ++t) {
      //cout << "regionBoxes[" << r << "][" << t << "] = " << regionBoxes[r][t] << endl;
    //}
  //}

////dataServicesPtr[0]->WriteSummary(cout, false, 0, false);
//BLProfilerUtils::WriteHeader(cout, 10, 16, true);

/*
std::string regionsFileName("pltRegions");
dataServicesPtr.resize(1);
dataServicesPtr[0] = new amrex::DataServices(regionsFileName, Amrvis::NEWPLT);
PltApp *temp = new PltApp(app, wTopLevel, regionsFileName, dataServicesPtr, false);
pltAppList.push_back(temp);

const amrex::Vector<amrex::Vector<amrex::Vector<BLProfStats::TimeRange> > > &regionTimeRanges =
        dataServicesPtr[0]->GetRegionsProfStats().GetRegionTimeRanges();
if(dataServicesPtr[0]->RegionDataAvailable()) {
cout << "regionTimeRanges:  size = " << regionTimeRanges.size() << endl;
if(regionTimeRanges.size() > 0) {
  for(int p(0); p < regionTimeRanges.size(); ++p) {
  for(int rnum(0); rnum < regionTimeRanges[p].size(); ++rnum) {
    for(int r(0); r < regionTimeRanges[p][rnum].size(); ++r) {
    //cout << "regionTimeRanges[" << p << "]][" << rnum << "][" << r << "] = "
         //<< regionTimeRanges[p][rnum][r] << endl;
    }
  }
  }
  }
} else {
  cout << "region data not available." << endl;
}
*/

  pltPaletteptr->DrawPalette(atiPaletteEntry, regNames.size() - 3, "%8.2f");
  regionPicturePtr->APDraw(0,0);
  cout << ">>>> rpp.SubTimeRange = " << regionPicturePtr->SubTimeRange() << endl;

  initTimer = amrex::ParallelDescriptor::second() - initTimer;
  cout << "initTimer [sec] = " << initTimer << std::endl; 
}


// -------------------------------------------------------------------
ProfApp::ProfApp(XtAppContext app, Widget w, const amrex::Box &region,
	         const amrex::IntVect &offset,
	         ProfApp *profparent, const string &palfile,
		 const string &filename)
  : wTopLevel(w),
    appContext(app),
    fileName(filename),
    domainBox(profparent->domainBox),
    currentScale(profparent->currentScale),
    maxAllowableScale(profparent->maxAllowableScale),
    clickHistory(profparent->clickHistory),
    dtr(profparent->dtr),
    rtr(profparent->rtr)
{
  bool isSubRegion = true;
//  int displayProc = 0;
  currentFrame = 0;
  palFilename = palfile;
  fileNames.resize(1);
  fileNames[0] = fileName;
  dataServicesPtr = profparent->dataServicesPtr;
/*
  cout << "----------------- rtr:" << endl;
  for(int r(0); r < rtr.size(); ++r) {
    for(int t(0); t < rtr[r].size(); ++t) {
      cout << "rtr[" << r << "][" << t << "] = " << rtr[r][t] << endl;
    }
  }
*/
  std::ostringstream headerout;
  headerout << AVGlobals::StripSlashes(fileNames[0]) << "   s:  bbbbbbbbbb!!!!";

  // Set the delete response to DO_NOTHING so that it can be app defined.
  wAmrVisTopLevel = XtVaCreatePopupShell(headerout.str().c_str(), 
			 topLevelShellWidgetClass, wTopLevel,
			 XmNwidth,	    initialWindowWidth,
			 XmNheight,	    initialWindowHeight,
			 XmNx,		    topLevelNx + placementOffsetX,
			 XmNy,		    topLevelNy + placementOffsetY,
			 XmNdeleteResponse, XmDO_NOTHING,
			 NULL);

  gaPtr = new GraphicsAttributes(wAmrVisTopLevel);
  display = gaPtr->PDisplay();
  xgc = gaPtr->PGC();
  if(gaPtr->PVisual() != XDefaultVisual(display, gaPtr->PScreenNumber())) {
    XtVaSetValues(wAmrVisTopLevel, XmNvisual, gaPtr->PVisual(),
		  XmNdepth, gaPtr->PDepth(), NULL);
  }

  headerout.str(std::string());
  headerout << AVGlobals::StripSlashes(fileNames[0]) << "   s:  aaaaaaaaaaaaaaaa!!!!";
  XtVaSetValues(wAmrVisTopLevel, XmNtitle, const_cast<char *>(headerout.str().c_str()),
		NULL);

  regionPicturePtr = new RegionPicture(gaPtr, region, this);

  ivLowOffset = offset;

  ProfAppInit(isSubRegion);

  // New subregion starts with all on.
  clickHistory.RestartOn();
  clickHistory.SetSubset(true);

  // Trim down the displayed region time ranges based on the subregion selected
  // CalcTimeRange = entire range of data
  // SubTimeRange = selected time range
  BLProfStats::TimeRange subRange(regionPicturePtr->SubTimeRange());
  for (int r(0); r < dtr.size(); ++r) {
    for (int t(0); t < dtr[r].size(); ++t) {

       BLProfStats::TimeRange regionRange = dtr[r][t]; 
       BLProfStats::TimeRange trimmedRange( std::max(subRange.startTime, regionRange.startTime),
                                            std::min(subRange.stopTime, regionRange.stopTime));
       if (trimmedRange.startTime > trimmedRange.stopTime)   // Occurs if region is outside new window.
       {
         dtr[r][t] = BLProfStats::TimeRange(0.0, 0.0);
       }
       else
       {
         dtr[r][t] = trimmedRange;
       }
    }
  }

////dataServicesPtr[0]->WriteSummary(cout, false, 0, false);
//BLProfilerUtils::WriteHeader(cout, 10, 16, true);

/*
std::string regionsFileName("pltRegions");
dataServicesPtr.resize(1);
dataServicesPtr[0] = new DataServices(regionsFileName, Amrvis::NEWPLT);
PltApp *temp = new PltApp(app, wTopLevel, regionsFileName, dataServicesPtr, false);
pltAppList.push_back(temp);

const amrex::Vector<amrex::Vector<amrex::Vector<BLProfStats::TimeRange> > > &regionTimeRanges =
        dataServicesPtr[0]->GetRegionsProfStats().GetRegionTimeRanges();
if(dataServicesPtr[0]->RegionDataAvailable()) {
cout << "regionTimeRanges:  size = " << regionTimeRanges.size() << endl;
if(regionTimeRanges.size() > 0) {
  for(int p(0); p < regionTimeRanges.size(); ++p) {
  for(int rnum(0); rnum < regionTimeRanges[p].size(); ++rnum) {
    for(int r(0); r < regionTimeRanges[p][rnum].size(); ++r) {
    //cout << "regionTimeRanges[" << p << "]][" << rnum << "][" << r << "] = "
         //<< regionTimeRanges[p][rnum][r] << endl;
    }
  }
  }
  }
} else {
  cout << "region data not available." << endl;
}
*/

  pltPaletteptr->DrawPalette(atiPaletteEntry, regNames.size() - 3, "%8.2f");
  regionPicturePtr->APDraw(0,0);
}


// -------------------------------------------------------------------
void ProfApp::ProfAppInit(bool bSubregion) {
  //int np;

  currentScale = 1;
  maxAllowableScale = 8;
  int displayProc = 0;

/*
  filterTimeRanges.resize(dataServicesPtr[0]->GetBLProfStats().GetNProcs());
  for(int i(0); i < filterTimeRanges.size(); ++i) {
    filterTimeRanges[i].push_back(regionPicturePtr->SubTimeRange());
    cout << "FTR::  i STR = " << i << "  " << regionPicturePtr->SubTimeRange() << endl;
  }
  //RegionsProfStats &regionsProfStats = dataServicesPtr[0]->GetRegionsProfStats();
  //regionsProfStats.SetFilterTimeRanges(filterTimeRanges);
  //regionPicturePtr->SetAllOnOff(RegionPicture::RP_ON);
*/


  XmAddWMProtocolCallback(wAmrVisTopLevel,
			  XmInternAtom(display,const_cast<String> ("WM_DELETE_WINDOW"), false),
			  (XtCallbackProc) CBQuitProfApp, (XtPointer) this);

  XYplotwin[0] = nullptr; // No 1D plot windows initially.
  XYplotwin[1] = nullptr;

  placementOffsetX += windowOffset;
  placementOffsetY += windowOffset;

  servingButton = 0;

  infoShowing = false;

  int palListLength(AVPalette::PALLISTLENGTH);
  int palWidth(AVPalette::PALWIDTH);
  int totalPalWidth(AVPalette::TOTALPALWIDTH);
  int totalPalHeight(AVPalette::TOTALPALHEIGHT);
  int reserveSystemColors(0);
  bool bRegions(true);
  if(bRegions) {
    totalPalWidth += regionPalettePad;
  }
  totalPalWidth += PltApp::GetExtraPaletteWidth();

  pltPaletteptr = new Palette(wTopLevel, palListLength, palWidth,
                              totalPalWidth, totalPalHeight,
                              reserveSystemColors);
  pltPaletteptr->SetRegions(true);
  pltPaletteptr->ReadSeqPalette(AVGlobals::GetPaletteName(), false);

  servingButton = 0;
  startX = 0;
  startY = 0;
  endX = 0;
  endY = 0;

  // gc for gxxor rubber band line drawing
  rbgc = XCreateGC(display, gaPtr->PRoot(), 0, NULL);
  XSetFunction(display, rbgc, GXxor);
  cursor = XCreateFontCursor(display, XC_left_ptr);

  // No need to store these widgets in the class after this function is called.
  Widget wMainArea, wPalFrame, wPlotFrame, wPalForm;

  wMainArea = XtVaCreateManagedWidget("MainArea", xmFormWidgetClass,
				      wAmrVisTopLevel,
				      NULL);

  // ------------------------------- menu bar
  Widget wMenuBar, wMenuPulldown, wid, wCascade;
  char selectText[20], accelText[20], accel[20];
  XmString label_str;
  XtSetArg(args[0], XmNtopAttachment, XmATTACH_FORM);
  XtSetArg(args[1], XmNleftAttachment, XmATTACH_FORM);
  XtSetArg(args[2], XmNrightAttachment, XmATTACH_FORM);
  wMenuBar = XmCreateMenuBar(wMainArea, const_cast<String> ("menubar"), args, 3);

  // ------------------------------- file menu
  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, const_cast<String> ("Filepulldown"), NULL, 0);
  XtVaCreateManagedWidget("File", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'F', XmNsubMenuId, wMenuPulldown, NULL);

  // To look at a subregion of the plot
  label_str = XmStringCreateSimple(const_cast<char *>("Ctrl+S"));
  wid =
    XtVaCreateManagedWidget("Subregion...",
                            xmPushButtonGadgetClass, wMenuPulldown,
                            XmNmnemonic, 'S',
                            XmNaccelerator, "Ctrl<Key>S",
                            XmNacceleratorText, label_str,
                            NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNactivateCallback, &ProfApp::DoSubregion);

  // To change the palette
  wid = XtVaCreateManagedWidget("Palette...", xmPushButtonGadgetClass,
                                wMenuPulldown, XmNmnemonic,  'P', NULL);
  //AddStaticCallback(wid, XmNactivateCallback, &ProfApp::DoPaletteButton);

  // To output call traces
  wCascade = XmCreatePulldownMenu(wMenuPulldown, const_cast<char *>("outputmenu"), NULL, 0);
  XtVaCreateManagedWidget("Export Call Traces", xmCascadeButtonWidgetClass, wMenuPulldown,
                          XmNsubMenuId, wCascade, NULL);
  wid = XtVaCreateManagedWidget("HTML File...", xmPushButtonGadgetClass,
                                wCascade, NULL);
  AddStaticCallback(wid, XmNactivateCallback, &ProfApp::DoOutput, (XtPointer) 0);
  wid = XtVaCreateManagedWidget("Text File...", xmPushButtonGadgetClass,
                                wCascade, NULL);
  AddStaticCallback(wid, XmNactivateCallback, &ProfApp::DoOutput, (XtPointer) 1);

  // Quit
  XtVaCreateManagedWidget(NULL, xmSeparatorGadgetClass, wMenuPulldown, NULL);
  label_str = XmStringCreateSimple(const_cast<String> ("Ctrl+Q"));
  wid = XtVaCreateManagedWidget("Quit", xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'Q',
				XmNaccelerator, "Ctrl<Key>Q",
				XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  XtAddCallback(wid, XmNactivateCallback, (XtCallbackProc) CBQuitAll,
		(XtPointer) this);
  
  // Close
  XtVaCreateManagedWidget(NULL, xmSeparatorGadgetClass, wMenuPulldown, NULL);
  label_str = XmStringCreateSimple(const_cast<String> ("Ctrl+C"));
  wid = XtVaCreateManagedWidget("Close", xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'C',
				XmNaccelerator, "Ctrl<Key>C",
				XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  XtAddCallback(wid, XmNactivateCallback, (XtCallbackProc) CBQuitProfApp,
		(XtPointer) this);
  
  // ------------------------------- view menu
  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, const_cast<char *>("MenuPulldown"), NULL, 0);
  XtVaCreateManagedWidget("View", xmCascadeButtonWidgetClass, wMenuBar,
                          XmNmnemonic, 'V', XmNsubMenuId, wMenuPulldown, NULL);

  // To scale the region picture
  wCascade = XmCreatePulldownMenu(wMenuPulldown, const_cast<char *>("scalemenu"), NULL, 0);
  XtVaCreateManagedWidget("Scale", xmCascadeButtonWidgetClass, wMenuPulldown,
                          XmNmnemonic, 'S', XmNsubMenuId, wCascade, NULL);
  for(int scale(1); scale <= maxAllowableScale; ++scale) {
    sprintf(selectText, "%ix", scale);
    wid = XtVaCreateManagedWidget(selectText, xmToggleButtonGadgetClass, wCascade,
                                  XmNset, false, NULL);
    if(scale <= 10) {
      // scale values <= 10 are shortcutted by pressing the number 1-0
      sprintf(accel, "<Key>%i", scale % 10);
      sprintf(accelText, "%i", scale % 10);
      label_str = XmStringCreateSimple(accelText);
      XtVaSetValues(wid, XmNmnemonic, scale + 'O',
                    XmNaccelerator, accel,
                    XmNacceleratorText, label_str,
                    NULL);
      XmStringFree(label_str);
    } else if(scale <= 20) {
      // scale values <= 20 can be obtained by holding down ALT and pressing 1-0
      sprintf(accel, "Alt<Key>%i", scale % 10);
      sprintf(accelText, "Alt+%i", scale % 10);
      label_str = XmStringCreateSimple(accelText);
      XtVaSetValues(wid,
                    XmNaccelerator, accel,
                    XmNacceleratorText, label_str,
                    NULL);
      XmStringFree(label_str);
    }     

    if(scale == currentScale) {
      // Toggle buttons indicate which is the current scale
      XtVaSetValues(wid, XmNset, true, NULL);
      wCurrScale = wid;
    }
    AddStaticCallback(wid, XmNvalueChangedCallback, &ProfApp::ChangeScale,
                      (XtPointer) static_cast<long> (scale));
  }

  // --------------------------------------------------------------- help menu
  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, const_cast<String> ("MenuPulldown"), NULL, 0);
  XtVaCreateManagedWidget("Help", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'H', XmNsubMenuId,   wMenuPulldown, NULL);
  wid = XtVaCreateManagedWidget("Info...", xmPushButtonGadgetClass,
				wMenuPulldown, XmNmnemonic,  'I', NULL);
  AddStaticCallback(wid, XmNactivateCallback, &ProfApp::DoInfoButton);
  
  XtManageChild(wMenuBar);


  // --------------------------------------------Palette frame and drawing area
  wPalFrame = XtVaCreateManagedWidget("paletteframe", xmFrameWidgetClass, wMainArea,
                            XmNtopAttachment,   XmATTACH_WIDGET,
                            XmNtopWidget,       wMenuBar,
                            XmNrightAttachment, XmATTACH_FORM,
                            XmNrightOffset,     1,
                            XmNshadowType,      XmSHADOW_ETCHED_IN,
                            NULL);
  wPalForm = XtVaCreateManagedWidget("paletteform", xmFormWidgetClass, wPalFrame,
                            NULL);

  wPalArea = XtVaCreateManagedWidget("palarea", xmDrawingAreaWidgetClass, wPalForm,
                            XmNtopAttachment,    XmATTACH_FORM,
                            XmNtopOffset,        30,
                            XmNleftAttachment,   XmATTACH_FORM,
                            XmNrightAttachment,  XmATTACH_FORM,
                            XmNbottomAttachment, XmATTACH_FORM,
                            XmNwidth,            totalPalWidth,
                            XmNheight,           AVPalette::TOTALPALHEIGHT,
                            NULL);
  AddStaticEventHandler(wPalArea, ExposureMask, &ProfApp::DoExposePalette);

  // Indicate the unit type of the palette (legend) area above it.
  strcpy(buffer, "regions");
  label_str = XmStringCreateSimple(buffer);
  wPlotLabel = XtVaCreateManagedWidget("regions", xmLabelWidgetClass, wPalForm,
                            XmNtopAttachment,    XmATTACH_FORM,
                            XmNleftAttachment,   XmATTACH_FORM,
                            XmNrightAttachment,  XmATTACH_FORM,
                            XmNbottomAttachment, XmATTACH_WIDGET,
                            XmNbottomWidget,     wPalArea,
                            XmNleftOffset,       0,
                            XmNrightOffset,      0,
                            XmNbottomOffset,     0,
                            XmNbackground,       pltPaletteptr->AVBlackPixel(),
                            XmNforeground,       pltPaletteptr->AVWhitePixel(),
                            XmNlabelString,      label_str,
                            NULL);
  XmStringFree(label_str);


  // ************************************** Controls frame and area
  Widget wControlsFrame;
  wControlsFrame = XtVaCreateManagedWidget("controlsframe",
                            xmFrameWidgetClass, wMainArea,
                            XmNrightAttachment, XmATTACH_FORM,
                            XmNrightOffset,     1,
                            XmNtopAttachment,   XmATTACH_WIDGET,
                            XmNtopWidget,       wPalFrame,
                            XmNshadowType,      XmSHADOW_ETCHED_IN,
                            NULL);

  int wcfWidth(totalPalWidth), wcfHeight(AVPalette::TOTALPALHEIGHT);
  wControlForm = XtVaCreateManagedWidget("refArea",
                            xmDrawingAreaWidgetClass, wControlsFrame,
                            XmNwidth,   wcfWidth,
                            XmNheight,  wcfHeight+100,
                            XmNresizePolicy, XmRESIZE_NONE,
                            NULL);
  AddStaticEventHandler(wControlForm, ExposureMask, &ProfApp::DoExposeRef);

  wFuncListButton = XtVaCreateManagedWidget("Generate Function List",
                            xmPushButtonWidgetClass, wControlForm,
                            XmNy, 100,
                            XmCMarginBottom, marginBottom,
                            NULL);
  AddStaticCallback(wFuncListButton, XmNactivateCallback, &ProfApp::DoGenerateFuncList,
                          (XtPointer) 111);
  XtManageChild(wFuncListButton);

  wAllOnButton = XtVaCreateManagedWidget("All On", xmPushButtonWidgetClass,
                            wControlForm,
                            XmNleftAttachment,   XmATTACH_FORM,
                            XmNy, 140,
                            XmCMarginBottom, marginBottom,
                            NULL);
  AddStaticCallback(wAllOnButton, XmNactivateCallback, &ProfApp::DoAllOnOff,
                          (XtPointer) RegionPicture::RP_ON);
  //XtManageChild(wAllOnButton);

  wAllOffButton = XtVaCreateManagedWidget("All Off", xmPushButtonWidgetClass,
                            wControlForm,
                            XmNx, 60,
                            XmNy, 140,
                            XmCMarginBottom, marginBottom,
                            NULL);
  AddStaticCallback(wAllOffButton, XmNactivateCallback, &ProfApp::DoAllOnOff,
                          (XtPointer) RegionPicture::RP_OFF);
  //XtManageChild(wAllOffButton);

  wTimelineButton = XtVaCreateManagedWidget("Generate Timeline",
                            xmPushButtonWidgetClass, wControlForm,
                            XmNy, 180,
                            XmCMarginBottom, marginBottom,
                            NULL);
  AddStaticCallback(wTimelineButton, XmNactivateCallback, &ProfApp::DoGenerateTimeline,
                          (XtPointer) RegionPicture::RP_OFF);
  //XtManageChild(wGenerateTimelineButton);

  wSendRecvButton = XtVaCreateManagedWidget("Generate Send/Recv List",
                            xmPushButtonWidgetClass, wControlForm,
                            XmNy, 220,
                            XmCMarginBottom, marginBottom,
                            NULL);
  AddStaticCallback(wSendRecvButton, XmNactivateCallback, &ProfApp::DoSendRecvList,
                          (XtPointer) RegionPicture::RP_OFF);
  //XtManageChild(wGenerateTimelineButton);

  wRegionTimePlotButton = XtVaCreateManagedWidget("Generate Region Time Plot",
                            xmPushButtonWidgetClass, wControlForm,
                            XmNy, 260,
                            XmCMarginBottom, marginBottom,
                            NULL);
  AddStaticCallback(wRegionTimePlotButton, XmNactivateCallback, &ProfApp::DoRegionTimePlot,
                          (XtPointer) RegionPicture::RP_OFF);
  //XtManageChild(wGenerateTimelineButton);

  wSendsPlotButton = XtVaCreateManagedWidget("Generate Sends Plotfile",
                       xmPushButtonWidgetClass, wControlForm,
                       XmNy, 300,
                       XmCMarginBottom, marginBottom,
                       NULL);
  AddStaticCallback(wSendsPlotButton, XmNactivateCallback, &ProfApp::DoSendsPlotfile,
                          (XtPointer) RegionPicture::RP_OFF);
 
  // ************************** Plot frame and area
  wPlotFrame = XtVaCreateManagedWidget("plotframe",
			    xmFrameWidgetClass,   wMainArea,
			    XmNrightAttachment,	  XmATTACH_WIDGET,
			    XmNrightWidget,       wPalFrame,
			    XmNleftAttachment,    XmATTACH_FORM,
			    XmNtopAttachment,	  XmATTACH_WIDGET,
			    XmNtopWidget,         wMenuBar,
			    XmNshadowType,	  XmSHADOW_ETCHED_IN,
			    XmNspacing,           0,
			    XmNmarginHeight,      0,
			    NULL);

  Widget wPicArea = XtVaCreateManagedWidget("picarea",
			    xmFormWidgetClass, wPlotFrame,
			    NULL);

  wPlotArea = XtVaCreateManagedWidget("plotarea", xmFormWidgetClass, wPicArea,
			    XmNtopAttachment,    XmATTACH_FORM,
			    XmNleftAttachment,   XmATTACH_FORM,
			    XmNrightAttachment,  XmATTACH_FORM,
			    //XmNbottomAttachment, XmATTACH_FORM,
			    XmNheight, plotAreaHeight,
			    NULL);

  wScrollArea = XtVaCreateManagedWidget("scrollAreaXY",
		xmScrolledWindowWidgetClass,
		wPlotArea,
		XmNleftAttachment,	XmATTACH_FORM,
		XmNleftOffset,		0,
		XmNtopAttachment,	XmATTACH_FORM,
		XmNtopOffset,		0,
		XmNscrollingPolicy,	XmAUTOMATIC,
		NULL);

  trans = const_cast<String> (
	"<Btn1Motion>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn1Down>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn1Up>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn2Motion>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn2Down>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn2Up>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn3Motion>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn3Down>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn3Up>: DrawingAreaInput() ManagerGadgetButtonMotion()" );
	
  wPlotPlane = XtVaCreateManagedWidget("plotArea",
			    xmDrawingAreaWidgetClass, wScrollArea,
			    XmNtranslations,  XtParseTranslationTable(trans),
			    XmNwidth, regionPicturePtr->ImageSizeH() + 1,
			    XmNheight, regionPicturePtr->ImageSizeV() + 1,
			    NULL);
  XtVaSetValues(wScrollArea, XmNworkWindow, wPlotPlane, NULL); 
  XtVaSetValues(wScrollArea, XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM, NULL);		
  
  AddStaticEventHandler(wPlotPlane, ExposureMask, &ProfApp::DoExposePicture);
  AddStaticCallback(wPlotPlane, XmNinputCallback, &ProfApp::DoRubberBanding);

  // ************************** profiled function list area

  Widget wFuncListFrame;
  wFuncListFrame = XtVaCreateManagedWidget("funclistframe",
                            xmFrameWidgetClass, wMainArea,
                            XmNrightAttachment, XmATTACH_WIDGET,
                            XmNrightWidget,     wPalFrame,
			    XmNleftAttachment,  XmATTACH_FORM,
			    XmNbottomAttachment, XmATTACH_FORM,
                            XmNtopAttachment,   XmATTACH_WIDGET,
                            XmNtopWidget,       wPlotFrame,
                            XmNheight,          funcListHeight,
			    XmNwidth,           funcListWidth,
                            XmNshadowType,      XmSHADOW_ETCHED_IN,
                            NULL);


  Widget wFuncForm =
    XtVaCreateManagedWidget("funcform", xmFormWidgetClass, wFuncListFrame, NULL);

  int i(0);
  XtSetArg(args[i++], XmNlistSizePolicy, XmRESIZE_IF_POSSIBLE);

  wFuncList = XmCreateScrolledList(wFuncForm, const_cast<String> ("funclist"), args, i);

  XtVaSetValues(XtParent(wFuncList),
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment, XmATTACH_FORM,
		XmNtopAttachment,   XmATTACH_WIDGET,
		XmNtopWidget,       wPlotFrame,
                XmNtopOffset, Amrvis::WOFFSET,
                XmNbottomAttachment, XmATTACH_POSITION,
                XmNbottomPosition, 90,
                XmNheight, 480,
                XmNwidth, 880,
                NULL);

  AddStaticCallback(wFuncList, XmNdefaultActionCallback,
                    &ProfApp::DoFuncListClick, (XtPointer) 42);
  AddStaticCallback(wFuncList, XmNbrowseSelectionCallback,
                    &ProfApp::DoFuncListClick, (XtPointer) 43);
  AddStaticCallback(wFuncList, XmNextendedSelectionCallback,
                    &ProfApp::DoFuncListClick, (XtPointer) 44);
  AddStaticCallback(wFuncList, XmNmultipleSelectionCallback,
                    &ProfApp::DoFuncListClick, (XtPointer) 45);

  XtManageChild(wFuncList);


  
  // ***************************************************************** 
  XtManageChild(wPalArea);
  XtManageChild(wPlotArea);
  XtPopup(wAmrVisTopLevel, XtGrabNone);

  pltPaletteptr->SetWindow(XtWindow(wPalArea));
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPalArea), false);
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPlotArea), false);
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wAmrVisTopLevel), false);
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPlotPlane), false);

  
  char plottertitle[50];
  sprintf(plottertitle, "XYPlot%dd", BL_SPACEDIM);
  XYplotparameters = new XYPlotParameters(pltPaletteptr, gaPtr, plottertitle);

  regionPicturePtr->CreatePicture(XtWindow(wPlotPlane), pltPaletteptr);

  RegionsProfStats &rProfStats = dataServicesPtr[0]->GetRegionsProfStats();
  regNames.insert(std::make_pair(atiPaletteEntry, "active time intervals"));
  regNames.insert(std::make_pair(notInRegionPaletteEntry, "not in region"));
  for(auto rnames : rProfStats.RegionNames()) {  // ---- swap map first with second
    regNames.insert(std::make_pair(rnames.second, rnames.first));
  }
  pltPaletteptr->SetRegionNames(regNames);

  subdomainBox = regionPicturePtr->DomainBox();

/*
  Vector<std::string> funcs;
  std::ostringstream ossSummary;
  dataServicesPtr[0]->WriteSummary(ossSummary, false, 0, false);
  size_t startPos(0), endPos(0);
  const std::string &ossString = ossSummary.str();
  while(startPos < ossString.length() - 1) {
    endPos = ossString.find('\n', startPos);
    std::string sLine(ossString.substr(startPos, endPos - startPos));
    startPos = endPos + 1;
    funcs.push_back(sLine);
  }
  PopulateFuncList(funcs);

  int whichProc(0);
  bool writeAverage(false), useTrace(false);
  PopulateFuncList(writeAverage, whichProc, useTrace);
*/
  cout << "rpp.SubTimeRange = " << regionPicturePtr->SubTimeRange() << endl;
  cout << "rpp.CalcTimeRange = " << regionPicturePtr->CalcTimeRange() << endl;

  filterTimeRanges.resize(dataServicesPtr[0]->GetBLProfStats().GetNProcs());
  for(int iii(0); iii < filterTimeRanges.size(); ++iii) {
    filterTimeRanges[iii].push_back(regionPicturePtr->SubTimeRange());
//    cout << "FTR::  iii STR = " << iii << "  " << regionPicturePtr->SubTimeRange() << endl;
  }
  dataServicesPtr[0]->GetRegionsProfStats().SetFilterTimeRanges(filterTimeRanges);
  dataServicesPtr[0]->GetCommOutputStats().SetFilterTimeRanges(filterTimeRanges);
  //regionPicturePtr->SetAllOnOff(RegionPicture::RP_ON);

  if (clickHistory.IsInitialized() || !bSubregion)
  {
    // If we have the region data from a previous operation, update with the full summary.
    // Otherwise, this is the initial load, so use the simplifed write summary.
    bool writeAverage(clickHistory.IsInitialized()), useTrace(clickHistory.IsInitialized());
    PopulateFuncList(writeAverage, displayProc, useTrace);
  }
  else // Write a message to the user detailing why there isn't a summary.
  {
    int numEntries(3);
    XmStringTable messageList = (XmStringTable) XtMalloc(numEntries * sizeof(XmString *));

    funcSelectionStrings.resize(numEntries, "");

    amrex::Vector<std::string> message(numEntries, "");
    message[0] = "Function list of a subregion requires trace information.";
    message[1] = "To generate the function list, press the 'Generate Func List' button.";
    message[2] = "WARNING: May take a long time to initialize the required data.";

    for(int j(0); j < message.size(); ++j) {
      messageList[j] = XmStringCreateSimple(const_cast<char *>(message[j].c_str()));
    }

    XtVaSetValues(wFuncList,
                  XmNitemCount, numEntries,
                  XmNitems, messageList,
                  nullptr);

    for(int j(0); j < numEntries; ++j) {
      XmStringFree(messageList[j]);
    }
    XtFree((char *) messageList);
  }

  interfaceReady = true;
}


// -------------------------------------------------------------------
void ProfApp::DestroyInfoWindow(Widget, XtPointer /*xp*/, XtPointer) {
  infoShowing = false;
}


// -------------------------------------------------------------------
void ProfApp::CloseInfoWindow(Widget, XtPointer, XtPointer) {
  XtPopdown(wInfoTopLevel);
  infoShowing = false;
}


// -------------------------------------------------------------------
void ProfApp::DoInfoButton(Widget, XtPointer, XtPointer) {
  if(infoShowing) {
    XtPopup(wInfoTopLevel, XtGrabNone);
    XMapRaised(XtDisplay(wInfoTopLevel), XtWindow(wInfoTopLevel));
    return;
  }

  infoShowing = true;
  Dimension width, height;
  Position  xpos, ypos;
  XtVaGetValues(wAmrVisTopLevel, XmNx, &xpos, XmNy, &ypos,
		XmNwidth, &width, XmNheight, &height, NULL);
  
  wInfoTopLevel = 
    XtVaCreatePopupShell("Info",
			 topLevelShellWidgetClass, wAmrVisTopLevel,
			 XmNwidth,	infoWidth,
			 XmNheight,	infoHeight,
			 XmNx,		50+xpos+width/2,
			 XmNy,		ypos-10,
			 NULL);
  
  AddStaticCallback(wInfoTopLevel, XmNdestroyCallback, &ProfApp::DestroyInfoWindow);
  
  // set visual in case the default isn't 256 pseudocolor
  if(gaPtr->PVisual() != XDefaultVisual(display, gaPtr->PScreenNumber())) {
    XtVaSetValues(wInfoTopLevel, XmNvisual, gaPtr->PVisual(), XmNdepth, 8, NULL);
  }
  
  Widget wInfoForm =
    XtVaCreateManagedWidget("infoform", xmFormWidgetClass, wInfoTopLevel, NULL);
  
  int i(0);
  XtSetArg(args[i], XmNeditable, false);       ++i;
  XtSetArg(args[i], XmNeditMode, XmMULTI_LINE_EDIT);       ++i;
  XtSetArg(args[i], XmNwordWrap, true);       ++i;
  XtSetArg(args[i], XmNblinkRate, 0);       ++i;
  XtSetArg(args[i], XmNautoShowCursorPosition, true);       ++i;
  XtSetArg(args[i], XmNcursorPositionVisible,  false);      ++i;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);      ++i;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);      ++i;
  XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);      ++i;
  XtSetArg(args[i], XmNbottomAttachment, XmATTACH_POSITION);      ++i;
  XtSetArg(args[i], XmNbottomPosition, 80);      ++i;

  Widget wInfoList = XmCreateScrolledText(wInfoForm,
                                          const_cast<char *> ("infoscrolledlist"),
                                          args, i);

  Widget wInfoCloseButton =
    XtVaCreateManagedWidget("Close",
			    xmPushButtonGadgetClass, wInfoForm,
			    XmNtopAttachment, XmATTACH_POSITION,
			    XmNtopPosition, 85,
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition, 95,
			    XmNrightAttachment, XmATTACH_POSITION,
			    XmNrightPosition, 75,
			    XmNleftAttachment, XmATTACH_POSITION,
			    XmNleftPosition, 25,
			    NULL);
  AddStaticCallback(wInfoCloseButton, XmNactivateCallback,
		    &ProfApp::CloseInfoWindow);
  
  XtManageChild(wInfoList);
  XtManageChild(wInfoCloseButton);

  XtPopup(wInfoTopLevel, XtGrabNone);

  profAppMessageText.Init(wInfoList);
  
  std::ostringstream prob;
  prob.precision(15);

  prob << "Profiling database :  " << fileNames[0] << '\n';
  prob << "ProfDataAvailable  :  "
       << (dataServicesPtr[0]->ProfDataAvailable()   ? "true" : "false") << '\n';
  prob << "RegionDataAvailable:  "
       << (dataServicesPtr[0]->RegionDataAvailable() ? "true" : "false") << '\n';
  prob << "TraceDataAvailable :  "
       << (dataServicesPtr[0]->TraceDataAvailable()  ? "true" : "false") << '\n';
  prob << "CommDataAvailable  :  "
       << (dataServicesPtr[0]->CommDataAvailable()   ? "true" : "false") << '\n';
  if(dataServicesPtr[0]->ProfDataAvailable()) {
    prob << "Data NProcs        :  "
         << dataServicesPtr[0]->GetBLProfStats().GetNProcs() << '\n';
  }

  profAppMessageText.PrintText(prob.str().c_str());
}


// -------------------------------------------------------------------
amrex::XYPlotDataList *ProfApp::CreateLinePlot(const string &derived,
                                               int dIndex)
{
  // Create an array of boxes corresponding to the intersected line.
  Box trueRegion(IntVect(0,0), IntVect(aFuncStats[dIndex].size() - 1,0));
  FArrayBox dataFab(trueRegion, 1);
  for(int i(0); i < aFuncStats[dIndex].size(); ++i) {
    Real *dp = dataFab.dataPtr();
    const BLProfStats::FuncStat &fs = aFuncStats[dIndex][i];
    dp[i] = fs.totalTime;
  }

  // Create an array of titles corresponding to the intersected line.
  Vector<Real> XdX(1);
  XdX[0] = 1.0;
  Vector<int> refR(1);
  refR[0] = 1;
  Vector<char *> intersectStr(1);
  intersectStr[0] = new char[128];
  sprintf(intersectStr[0], "lineplot");

  amrex::XYPlotDataList *newlist = new amrex::XYPlotDataList(derived,
                                     0, 0, 0, refR, XdX, intersectStr, 0.0);

  newlist->AddFArrayBox(dataFab, 0, 0);

  return newlist;
}


// -------------------------------------------------------------------
void ProfApp::DoExposePalette(Widget, XtPointer, XtPointer) {
  pltPaletteptr->ExposePalette();
}


// -------------------------------------------------------------------
void ProfApp::DoExposePicture(Widget /*w*/, XtPointer, XtPointer) {
  regionPicturePtr->DoExposePicture();
}


// -------------------------------------------------------------------
void ProfApp::DoExposeRef(Widget, XtPointer, XtPointer) {
  XClearWindow(display, XtWindow(wControlForm));
  int color(pltPaletteptr->WhiteIndex());
  XSetForeground(XtDisplay(wControlForm), xgc, pltPaletteptr->makePixel(color));

  axisLengthX = 138;
  axisLengthY = 32;
  Real dLength(domainBox.length(0));
  Real sdXL(subdomainBox.smallEnd(0));
  Real sdXH(subdomainBox.smallEnd(0) + subdomainBox.length(0));
  sdLineXL = domainBox.smallEnd(0) + (static_cast<int>(axisLengthX * sdXL / dLength));
  sdLineXH = domainBox.smallEnd(0) + (static_cast<int>(axisLengthX * sdXH / dLength));
  Real subTimeRangeStart(regionPicturePtr->SubTimeRange().startTime);
  Real subTimeRangeStop(regionPicturePtr->SubTimeRange().stopTime);
  DrawTimeRange(wControlForm, sdLineXL, sdLineXH, axisLengthX, axisLengthY,
                subTimeRangeStart, subTimeRangeStop, "region");
}


// -------------------------------------------------------------------
void ProfApp::DoFuncListClick(Widget w, XtPointer /*client_data*/, XtPointer call_data)
{
  XmListCallbackStruct *cbs = (XmListCallbackStruct *) call_data;

  String selection;
  XmStringGetLtoR(cbs->item, XmSTRING_DEFAULT_CHARSET, &selection);

  int itemCount(-1), selectedItemCount(-2), *positions;
  XtVaGetValues(w, XmNitemCount, &itemCount,
                   XmNselectedItemCount, &selectedItemCount,
                   XmNselectedPositions, &positions,
		   NULL);


  if(selectedItemCount > 0)
  {
    int fSSPosition(positions[0] - 1);  // ---- the xwindow list starts at 1 not 0
    cout << "selected function = " << funcSelectionStrings[fSSPosition] << endl;
    if(funcSelectionStrings[fSSPosition].size() == 0) {
      //XmListDeselectPos(w, positions[0]);
      XmListDeselectAllItems(w);
    } else {
      int aFSIndex = funcNameIndex[funcSelectionStrings[fSSPosition]];
      RegionsProfStats &regionsProfStats = dataServicesPtr[0]->GetRegionsProfStats();
      ReplayClickHistory();
      if(aFuncStats.size() == 0) {
        regionsProfStats.SetFilterTimeRanges(filterTimeRanges);
        regionsProfStats.CollectFuncStats(aFuncStats);
      }

       const amrex::Vector<std::string> &numbersToFNames =
                                          regionsProfStats.NumbersToFName();

    int whichFuncNameInt(-1);
    string whichFuncName = funcSelectionStrings[fSSPosition];
    for(int i(0); i < numbersToFNames.size(); ++i) {
      if(numbersToFNames[i] == whichFuncName) {
        whichFuncNameInt = i;
      }
    }

      aFSIndex = whichFuncNameInt;

      XYPlotDataList *newlist = CreateLinePlot(funcSelectionStrings[fSSPosition],
    	                                           aFSIndex);
      if(newlist) {
        newlist->SetLevel(0);
        if(XYplotwin[0] == nullptr) {
          XYplotwin[0] = new XYPlotWin(const_cast<char *>("Function Runtime per Rank  -  "), appContext, wAmrVisTopLevel,
                                       this, 0, 0);
        }
        XYplotwin[0]->AddDataList(newlist);
      }
    }
  }
}
// -------------------------------------------------------------------
void ProfApp::DoSendRecvList(Widget /*w*/, XtPointer /*client_data*/,
                                 XtPointer /*call_data*/)
{

  // Test for whether this already exists. (Put timeline in bl_prof?)
  cout << " Generating Send/Recv List. Please wait. " << std::endl;

  ReplayClickHistory();
  dataServicesPtr[0]->RunSendRecvList();

  cout << " Send/Recv List completed. " << std::endl;

}

// -------------------------------------------------------------------
void ProfApp::DoGenerateTimeline(Widget /*w*/, XtPointer /*client_data*/,
                                 XtPointer /*call_data*/)
{
  std::map<int,string> mpiFuncNames;
  int maxSmallImageLength(800), refRatioAll(4), nTimeSlots(25600);
  bool statsCollected(false);
  BLProfStats::TimeRange subTimeRange(regionPicturePtr->SubTimeRange());
  std::string plotfileName("buttonPltFile");

  // Test for whether this already exists. (Put timeline in bl_prof?)
  cout << " Generating Timeline for range:  " << subTimeRange << "\nPlease wait. " << std::endl;

  amrex::DataServices::Dispatch(amrex::DataServices::RunTimelinePFRequest,
                                dataServicesPtr[0],
                                (void *) &(mpiFuncNames),
                                (void *) &(plotfileName), 
                                (void *) &(subTimeRange),
                                maxSmallImageLength,
                                refRatioAll,
                                nTimeSlots,
                                &statsCollected);

  cout << " Timeline completed. " << std::endl;

/*
   ?? open timelinepf ??
*/
}
// -------------------------------------------------------------------
void ProfApp::DoRegionTimePlot(Widget /*w*/, XtPointer /*client_data*/,
                                 XtPointer /*call_data*/)
{

  ReplayClickHistory();

  // If there are no selected regions, write that and quit.
  // Note: all ranks should have the same number of regions, so 
  //       check is currently only done on rank 0. Need to change
  //       when ProfApp is fully scaled.
  if (filterTimeRanges[0].size() == 0)
  {
    cout << "*** Cannot generate RegionTimePlot: No regions selected." << endl;
    return;
  }
  else
  {

    string title("");
    // Generate title for the plot.
    if(XYplotwin[1] == nullptr) {
      title = "Plot #1";
    }
    else
    {
      std::ostringstream titlestream;
      titlestream << "Plot #" << XYplotwin[1]->NumItems() + 1;
      title = titlestream.str(); 
    }

    // Create an array of boxes corresponding to the intersected line.
    Box plotData(IntVect(0,0), IntVect(filterTimeRanges.size() - 1, 0));
    FArrayBox dataFab(plotData, 1);
    Real *dp = dataFab.dataPtr();
    for (int i(0); i<filterTimeRanges.size(); ++i)
    {
      Real regionTime = 0.0;
      std::list<BLProfStats::TimeRange>::iterator it;
      for (it = filterTimeRanges[i].begin(); it != filterTimeRanges[i].end(); ++it)
      {
        BLProfStats::TimeRange tr = *it;
        regionTime += tr.stopTime - tr.startTime;
      }
      dp[i] = regionTime; 
    }

    // Create an array of titles corresponding to the intersected line.
    Vector<Real> XdX(1, 1.0);
    Vector<int> refR(1, 1);
    Vector<char *> intersectStr(1, new char[128]);
    sprintf(intersectStr[0], "lineplot"); 

    amrex::XYPlotDataList *newlist = new amrex::XYPlotDataList(title,
                                      0, 0, 0, refR, XdX, intersectStr, 0.0);

    newlist->AddFArrayBox(dataFab, 0, 0);

    if(newlist) 
    {
      newlist->SetLevel(0);
      if(XYplotwin[1] == nullptr) {
        XYplotwin[1] = new XYPlotWin(const_cast<char *>("Selected Region(s) Time per Rank  -  "), appContext, wAmrVisTopLevel,
                                     this, 1, 0);
      }
      XYplotwin[1]->AddDataList(newlist);
    }
  }
}
// -------------------------------------------------------------------
void ProfApp::DoSendsPlotfile(Widget /*w*/, XtPointer /*client_data*/,
                                 XtPointer /*call_data*/)
{
  ReplayClickHistory();
  std::string plotfileName("pltTSP2P_Button");
  int maxSmallImageLength = 800;
  bool proxMap = false;
  int refRatioAll = 4;

  for(int i(0); i < filterTimeRanges.size(); ++i) {
    int jcount = 0;
    for (list<BLProfStats::TimeRange>::iterator j=filterTimeRanges[i].begin(); j != filterTimeRanges[i].end(); ++j)
    {
      BLProfStats::TimeRange ttr = *j;
      cout << "filterTimeRanges[ " << i << "][" << jcount++ << "] = " << ttr << endl;
    }
  }

  cout << " Generating Sends Plotfile. Please wait. " << std::endl;

  amrex::DataServices::Dispatch(amrex::DataServices::RunSendsPFRequest,
                                dataServicesPtr[0],
                                (void *) &(plotfileName),
                                maxSmallImageLength, 
                                &proxMap,
                                refRatioAll);

  cout << " Sends completed. " << std::endl;

}
// -------------------------------------------------------------------
void ProfApp::DoGenerateFuncList(Widget /*w*/, XtPointer client_data,
                                 XtPointer /*call_data*/)
{
  unsigned long r = (unsigned long) client_data;
  cout << "_in ProfApp::DoGenerateFuncList:  r = " << r << endl;
  ReplayClickHistory();
  RegionsProfStats &regionsProfStats = dataServicesPtr[0]->GetRegionsProfStats();
  regionsProfStats.SetFilterTimeRanges(filterTimeRanges);
  // All procs should have the same size, especially in this case. So only test 1.
  if(filterTimeRanges[0].size() == 0) {
    if(ParallelDescriptor::IOProcessor()) { cout << "*****Cannot generate a function list: No regions are selected" << endl; }
    return;
  }
  for(int i(0); i < filterTimeRanges.size(); ++i) {
    int jcount = 0;
    for (list<BLProfStats::TimeRange>::iterator j=filterTimeRanges[i].begin(); j != filterTimeRanges[i].end(); ++j)
    {
      BLProfStats::TimeRange ttr = *j;
      cout << "filterTimeRanges[ " << i << "][" << jcount++ << "] = " << ttr << endl;
    }
  }

  aFuncStats.clear();
  regionsProfStats.CollectFuncStats(aFuncStats);
  std::map<std::string, BLProfiler::ProfStats> mProfStats;  // [fname, pstats]
  const Vector<string> &blpFNames = regionsProfStats.BLPFNames();

  Real calcRunTime(1.0);
  int whichProc(0);
  amrex::CollectMProfStats(mProfStats, aFuncStats, blpFNames, calcRunTime, whichProc);
  cout << "||||::::  mProfStats.size() = " << mProfStats.size() << endl;
  for(int i(0); i < blpFNames.size(); ++i) {
    cout << "blpFNames[" << i << "] = " << blpFNames[i] << endl;
    funcNameIndex[blpFNames[i]] = i;
  }
  for(auto mps : mProfStats) {
    Real percent(100.0);
    const int colWidth(10);
    const int maxlen(64);
    bool bwriteavg(true);
    cout << "mps = " << mps.first << "  " << mps.second.totalTime << endl;
    BLProfilerUtils::WriteRow(cout, mps.first, mps.second, percent, colWidth, maxlen, bwriteavg);
  }
  cout << "||||::::" << endl;

  bool writeAverage(true), useTrace(true);
  PopulateFuncList(writeAverage, whichProc, useTrace);

  regionPicturePtr->APDraw(0,0);
  pltPaletteptr->DrawPalette(atiPaletteEntry, regNames.size() - 3, "%8.2f");
}

// -------------------------------------------------------------------
void ProfApp::PopulateFuncList(bool bWriteAverage, int whichProc, bool bUseTrace) {
  amrex::ignore_unused(whichProc);
  Vector<std::string> funcs;
  std::ostringstream ossSummary;
  dataServicesPtr[0]->WriteSummary(ossSummary, bWriteAverage, 0, bUseTrace, false);
  size_t startPos(0), endPos(0);
  const std::string &ossString = ossSummary.str();
  while(startPos < ossString.length() - 1) {
    endPos = ossString.find('\n', startPos);
    std::string sLine(ossString.substr(startPos, endPos - startPos));
    startPos = endPos + 1;
    funcs.push_back(sLine);
  }


  RegionsProfStats &regionsProfStats = dataServicesPtr[0]->GetRegionsProfStats();
  const Vector<string> &blpFNames = regionsProfStats.BLPFNames();

  funcSelectionStrings.resize(funcs.size(), "");
  for(int i(0); i < funcs.size(); ++i) {
    const std::vector<std::string>& tokens = amrex::Tokenize(funcs[i], " ");
    if(tokens.size() > 0) {
      if(std::find(blpFNames.begin(), blpFNames.end(), tokens[0]) != blpFNames.end()) {
        funcSelectionStrings[i] = tokens[0];
      }
    }
  }

  int numEntries(funcs.size());
  XmStringTable strList = (XmStringTable) XtMalloc(numEntries*sizeof(XmString *));

  for(int i(0); i < funcs.size(); ++i) {
    strList[i] = XmStringCreateSimple(const_cast<char *>(funcs[i].c_str()));
  }

  XtVaSetValues(wFuncList,
                XmNitemCount, numEntries,
                XmNitems, strList,
                nullptr);

  for(int i(0); i < funcs.size(); ++i) {
    XmStringFree(strList[i]);
  }
  XtFree((char *) strList);
}


// -------------------------------------------------------------------
void ProfApp::DoAllOnOff(Widget /*w*/, XtPointer client_data, XtPointer /*call_data*/)
{
  unsigned long v = (unsigned long) client_data;
  regionPicturePtr->SetAllOnOff(v);
  for(int i(0); i < filterTimeRanges.size(); ++i) {
    filterTimeRanges[i].clear();
    if(v == RegionPicture::RP_ON) {
      filterTimeRanges[i].push_back(regionPicturePtr->SubTimeRange());
      clickHistory.RestartOn();
    }
    else
    {
      clickHistory.RestartOff();
    }
  }
}


// -------------------------------------------------------------------
void ProfApp::DoRubberBanding(Widget, XtPointer client_data, XtPointer call_data)
{
  amrex::ignore_unused(client_data);

  XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct *) call_data;

  if(cbs->event->xany.type != ButtonPress) {
    return;
  }

  int scale(currentScale);
  int imageHeight(regionPicturePtr->ImageSizeV() - 1);
  int imageWidth(regionPicturePtr->ImageSizeH() - 1);
  int oldX(max(0, min(imageWidth,  cbs->event->xbutton.x)));
  int oldY(max(0, min(imageHeight, cbs->event->xbutton.y)));
  int saveOldX(oldX), saveOldY(oldY);  // ---- for click with control down
  int rootX, rootY;
  unsigned int inputMask;
  Window whichRoot, whichChild;
  bool bShiftDown(cbs->event->xbutton.state & ShiftMask);
  bool bControlDown(cbs->event->xbutton.state & ControlMask);
  bControlDown = true;

  servingButton = cbs->event->xbutton.button;

  XSetForeground(display, rbgc, pltPaletteptr->makePixel(120));
  XChangeActivePointerGrab(display, PointerMotionHintMask |
                           ButtonMotionMask | ButtonReleaseMask |
                           OwnerGrabButtonMask, cursor, CurrentTime);
  AVXGrab avxGrab(display);

  if(servingButton == 1) {
    if(bShiftDown) {
      oldX = 0;
    }
    if(bControlDown) {
      oldY = 0;
    }
    int rectDrawn(false);
    int anchorX(oldX);
    int anchorY(oldY);
    int newX, newY;

    while(true) {
      XNextEvent(display, &nextEvent);

      switch(nextEvent.type) {

      case MotionNotify:

        if(rectDrawn) {   // undraw the old rectangle(s)
          rWidth  = std::abs(oldX-anchorX);
          rHeight = std::abs(oldY-anchorY);
          rStartX = (anchorX < oldX) ? anchorX : oldX;
          rStartY = (anchorY < oldY) ? anchorY : oldY;
          XDrawRectangle(display, regionPicturePtr->PictureWindow(),
                         rbgc, rStartX, rStartY, rWidth, rHeight);
        }

        while(XCheckTypedEvent(display, MotionNotify, &nextEvent)) {
          ;  // do nothing
        }

        XQueryPointer(display, regionPicturePtr->PictureWindow(),
                      &whichRoot, &whichChild,
                      &rootX, &rootY, &newX, &newY, &inputMask);

        if(bShiftDown) {
          newX = imageWidth;
        }
        if(bControlDown) {
          newY = imageHeight;
        }
        newX = max(0, min(imageWidth,  newX));
        newY = max(0, min(imageHeight, newY));
        rWidth  = std::abs(newX-anchorX);   // draw the new rectangle
        rHeight = std::abs(newY-anchorY);
        rStartX = (anchorX < newX) ? anchorX : newX;
        rStartY = (anchorY < newY) ? anchorY : newY;
        XDrawRectangle(display, regionPicturePtr->PictureWindow(),
                       rbgc, rStartX, rStartY, rWidth, rHeight);
        rectDrawn = true;

        oldX = newX;
        oldY = newY;

        break;

      case ButtonRelease: {
        avxGrab.ExplicitUngrab();

        startX = (max(0, min(imageWidth,  anchorX))) / scale;
        startY = (max(0, min(imageHeight, anchorY))) / scale;
        endX   = (max(0, min(imageWidth,  nextEvent.xbutton.x))) / scale;
        endY   = (max(0, min(imageHeight, nextEvent.xbutton.y))) / scale;

        if(bShiftDown) {
          startX = 0;
          endX   = imageWidth  / scale;
        }
        if(bControlDown) {
          startY = 0;
          endY   = imageHeight / scale;
        }

        selectionBox.setSmall(Amrvis::XDIR, min(startX, endX));
        selectionBox.setSmall(Amrvis::YDIR, ((imageHeight + 1) / scale) -
                                     max(startY, endY) - 1);
        selectionBox.setBig(Amrvis::XDIR, max(startX, endX));
        selectionBox.setBig(Amrvis::YDIR, ((imageHeight + 1) / scale)  -
                                     min(startY, endY) - 1);

        if(saveOldX == nextEvent.xbutton.x && saveOldY == nextEvent.xbutton.y) {
          // ---- data at click
	  int dpX((saveOldX / scale) + ivLowOffset[Amrvis::XDIR]);
	  int dpY((imageHeight - 1 - saveOldY) / scale);
	  bool outOfRange;
	  Real dataValue(regionPicturePtr->DataValue(dpX, dpY, outOfRange));
	  int dataValueIndex(static_cast<int>(dataValue));
	  BLProfStats::TimeRange calcTimeRange(regionPicturePtr->CalcTimeRange());
	  Real calcTime(calcTimeRange.stopTime - calcTimeRange.startTime);
	  Real clickTime(calcTime * (static_cast<Real>(dpX) /
	                 static_cast<Real>(domainBox.length(Amrvis::XDIR) - 1)));
	  int rtri(FindRegionTimeRangeIndex(dataValueIndex, clickTime));

          std::ostringstream buffout;
          buffout << "click at " << saveOldX << "  " << saveOldY << "  !" << endl;
          buffout << "dpX dpY = " << dpX << "  " << dpY << endl;
          buffout << "ivLowOffset = " << ivLowOffset << endl;
          buffout << "calcTimeRange calcTime clickTime = "
	          << calcTimeRange << "  " << calcTime << "  " << clickTime << endl;
          buffout << "dataValueIndex regName rtri timerange = "
                  << dataValueIndex << "  " << GetRegionName(dataValue) << "  "
		  << rtri << "  ";
	  if(rtri < 0 || rtri >= dtr[dataValueIndex].size()) {
            buffout << "Not in a region."  << endl;
	  } else {
            buffout << dtr[dataValueIndex][rtri] << endl;
	  }

          PrintMessage(const_cast<char *>(buffout.str().c_str()));

        } else {         // ---- tell the regionpicture about the box
          if(startX < endX) { // box in scaled pixmap space
            startX = selectionBox.smallEnd(Amrvis::XDIR) * scale;
            endX   = selectionBox.bigEnd(Amrvis::XDIR)   * scale;
          } else {
            startX = selectionBox.bigEnd(Amrvis::XDIR)   * scale;
            endX   = selectionBox.smallEnd(Amrvis::XDIR) * scale;
          }

          if(startY < endY) {
            startY = imageHeight - selectionBox.bigEnd(Amrvis::YDIR)   * scale;
            endY   = imageHeight - selectionBox.smallEnd(Amrvis::YDIR) * scale;
          } else {
            startY = imageHeight - selectionBox.smallEnd(Amrvis::YDIR) * scale;
            endY   = imageHeight - selectionBox.bigEnd(Amrvis::YDIR)   * scale;
          }

          int nodeAdjustment = (scale - 1) * selectionBox.type()[Amrvis::YDIR];
          startY -= nodeAdjustment;
          endY   -= nodeAdjustment;

          regionPicturePtr->SetRegion(startX, startY, endX, endY);
          regionPicturePtr->DoExposePicture();
        }
        return;
      }
      break;

      case NoExpose:
      break;

      default:
        break;
      }  // end switch
    }  // end while(true)
  }


  if(servingButton == 2) {

    while(true) {
      XNextEvent(display, &nextEvent);

      switch(nextEvent.type) {

      case MotionNotify:
      break;

      case ButtonRelease:
        avxGrab.ExplicitUngrab();

        if(saveOldX == nextEvent.xbutton.x && saveOldY == nextEvent.xbutton.y) {
          // ---- turn region off
	  int dpX((saveOldX / scale) + ivLowOffset[Amrvis::XDIR]);
	  int dpY((imageHeight - 1 - saveOldY) / scale);
	  bool outOfRange;
	  Real dataValue(regionPicturePtr->DataValue(dpX, dpY, outOfRange));
	  int dataValueIndex(static_cast<int>(dataValue));
	  BLProfStats::TimeRange calcTimeRange(regionPicturePtr->CalcTimeRange());
	  Real calcTime(calcTimeRange.stopTime - calcTimeRange.startTime);
	  Real clickTime(calcTime * (static_cast<Real>(dpX) /
	                 static_cast<Real>(domainBox.length(Amrvis::XDIR) - 1)));
	  int rtri(FindRegionTimeRangeIndex(dataValueIndex, clickTime));

          std::ostringstream buffout;
          buffout << "click at " << saveOldX << "  " << saveOldY << "  !" << endl;
          buffout << "dpX dpY = " << dpX << "  " << dpY << endl;
          buffout << "ivLowOffset = " << ivLowOffset << endl;
          buffout << "calcTimeRange calcTime clickTime = "
	          << calcTimeRange << "  " << calcTime << "  " << clickTime << endl;
          buffout << "dataValueIndex regName rtri timerange = "
                  << dataValueIndex << "  " << GetRegionName(dataValue) << "  "
		  << rtri << "  ";
	  if(rtri < 0 || rtri >= dtr[dataValueIndex].size()) {
            buffout << "Not in a region."  << endl;
	  } else {
            buffout << dtr[dataValueIndex][rtri] << endl;
	  }

          PrintMessage(const_cast<char *>(buffout.str().c_str()));
	  regionPicturePtr->SetRegionOnOff(dataValueIndex, rtri, RegionPicture::RP_OFF);
	  if(rtri < 0 || rtri >= dtr[dataValueIndex].size()) {
	  } else {
	    for(int i(0); i < filterTimeRanges.size(); ++i) {
	      BLProfStats::RemovePiece(filterTimeRanges[i], dtr[dataValueIndex][rtri]);
	    }
            clickHistory.Store(dataValueIndex, rtri, false);
	  }
          regionPicturePtr->DoExposePicture();

        }

        return;

      default:
        break;
      }  // end switch
    }  // end while(true)
  }




  if(servingButton == 3) {
    while(true) {
      XNextEvent(display, &nextEvent);

      switch(nextEvent.type) {

      case MotionNotify:
      break;

      case ButtonRelease:
        avxGrab.ExplicitUngrab();

        if(saveOldX == nextEvent.xbutton.x && saveOldY == nextEvent.xbutton.y) {
          // ---- turn region on
	  int dpX((saveOldX / scale) + ivLowOffset[Amrvis::XDIR]);
	  int dpY((imageHeight - 1 - saveOldY) / scale);
	  bool outOfRange;
	  Real dataValue(regionPicturePtr->DataValue(dpX, dpY, outOfRange));
	  int dataValueIndex(static_cast<int>(dataValue));
	  BLProfStats::TimeRange calcTimeRange(regionPicturePtr->CalcTimeRange());
	  Real calcTime(calcTimeRange.stopTime - calcTimeRange.startTime);
	  Real clickTime(calcTime * (static_cast<Real>(dpX) /
	                 static_cast<Real>(domainBox.length(Amrvis::XDIR) - 1)));
	  int rtri(FindRegionTimeRangeIndex(dataValueIndex, clickTime));

          std::ostringstream buffout;
          buffout << "click at " << saveOldX << "  " << saveOldY << "  !" << endl;
          buffout << "dpX dpY = " << dpX << "  " << dpY << endl;
          buffout << "ivLowOffset = " << ivLowOffset << endl;
          buffout << "calcTimeRange calcTime clickTime = "
	          << calcTimeRange << "  " << calcTime << "  " << clickTime << endl;
          buffout << "dataValueIndex regName rtri timerange = "
                  << dataValueIndex << "  " << GetRegionName(dataValue) << "  "
		  << rtri << "  ";
	  if(rtri < 0 || rtri >= dtr[dataValueIndex].size()) {
            buffout << "Not in a region."  << endl;
	  } else {
            buffout << dtr[dataValueIndex][rtri] << endl;
	  }

          PrintMessage(const_cast<char *>(buffout.str().c_str()));
	  regionPicturePtr->SetRegionOnOff(dataValueIndex, rtri, RegionPicture::RP_ON);
	  if(rtri < 0 || rtri >= dtr[dataValueIndex].size()) {
	  } else {
	    for(int i(0); i < filterTimeRanges.size(); ++i) {
	      BLProfStats::AddPiece(filterTimeRanges[i], dtr[dataValueIndex][rtri]);
	    }
            clickHistory.Store(dataValueIndex, rtri, true);
	  }
          regionPicturePtr->DoExposePicture();

        }

        return;

      default:
        break;
      }  // end switch
    }  // end while(true)
  }


}  // end DoRubberBanding


// -------------------------------------------------------------------
void ProfApp::DoOutput(Widget /*w*/, XtPointer data, XtPointer) {
  int i;
  static Widget wGetFileName;
  XmString sMessage;
  sMessage = XmStringCreateSimple(const_cast<char *>("Please enter a filename base:"));

  i=0;
  XtSetArg(args[i], XmNselectionLabelString, sMessage); ++i;
  XtSetArg(args[i], XmNautoUnmanage, false); ++i;
  XtSetArg(args[i], XmNkeyboardFocusPolicy, XmPOINTER); ++i;
  wGetFileName = XmCreatePromptDialog(wAmrVisTopLevel, const_cast<char *>("Save as"), args, i);
  XmStringFree(sMessage);

  unsigned long which = (unsigned long) data;
  switch(which) {
    case 0:
      AddStaticCallback(wGetFileName, XmNokCallback, &ProfApp::DoCreateHTMLTrace);
    break;
    case 1:
      AddStaticCallback(wGetFileName, XmNokCallback, &ProfApp::DoCreateTextTrace);
    break;
    default:
      cerr << "Error in ProfApp::DoOutput:  bad selection = " << data << endl;
      return;
  }

  XtAddCallback(wGetFileName, XmNcancelCallback, (XtCallbackProc) XtDestroyWidget, NULL);
  XtSetSensitive(XmSelectionBoxGetChild(wGetFileName, XmDIALOG_HELP_BUTTON), false);

  string tempfilename("");
  switch(which) {
    case 0:
      tempfilename = "HTMLCallTrace";  // ---- filename base
      break;
    case 1:
      tempfilename = "CallTrace";
      break;
  }
   
  XmTextSetString(XmSelectionBoxGetChild(wGetFileName, XmDIALOG_TEXT),
                  const_cast<char *> (tempfilename.c_str()));
  XtManageChild(wGetFileName);
  XtPopup(XtParent(wGetFileName), XtGrabNone);
}


// -------------------------------------------------------------------
void ProfApp::DoCreateHTMLTrace(Widget w, XtPointer, XtPointer call_data) {
  XmSelectionBoxCallbackStruct *cbs = (XmSelectionBoxCallbackStruct *) call_data;
  char htmlfilename[Amrvis::BUFSIZE];
  char *fileNameBase;
  XmStringGetLtoR(cbs->value, XmSTRING_DEFAULT_CHARSET, &fileNameBase);
  sprintf(htmlfilename, "%s.html", fileNameBase);
  string htmlFileName(htmlfilename);

  //DataServices::Dispatch(DataServices::WriteFabOneVar,
                         //dataServicesPtr[currentFrame],
                         //(void *) &fabFileName,
                         //(void *) &(bx[maxDrawnLevel]),
                         //maxDrawnLevel,
                         //(void *) &derivedQuantity);
  XtFree(fileNameBase);
  XtDestroyWidget(w);
}


// -------------------------------------------------------------------
void ProfApp::DoCreateTextTrace(Widget w, XtPointer, XtPointer call_data) {
  XmSelectionBoxCallbackStruct *cbs = (XmSelectionBoxCallbackStruct *) call_data;
  char textfilename[Amrvis::BUFSIZE];
  char *fileNameBase;
  XmStringGetLtoR(cbs->value, XmSTRING_DEFAULT_CHARSET, &fileNameBase);
  sprintf(textfilename, "%s.txt", fileNameBase);
  string textFileName(textfilename);

  //DataServices::Dispatch(DataServices::WriteFabOneVar,
                         //dataServicesPtr[currentFrame],
                         //(void *) &textFileName,
                         //(void *) &(bx[maxDrawnLevel]),
                         //maxDrawnLevel,
                         //(void *) &derivedQuantity);
  XtFree(fileNameBase);
  XtDestroyWidget(w);
}


// -------------------------------------------------------------------
void ProfApp::ChangeScale(Widget w, XtPointer client_data, XtPointer) {
  if(w == wCurrScale) {
    XtVaSetValues(w, XmNset, true, NULL);
    return;
  }
  unsigned long newScale = (unsigned long) client_data;
  XtVaSetValues(wCurrScale, XmNset, false, NULL);
  wCurrScale = w;
  int previousScale(currentScale);
  currentScale = newScale;

  regionPicturePtr->APChangeScale(currentScale, previousScale);
  XtVaSetValues(wPlotPlane,
                XmNwidth,  regionPicturePtr->ImageSizeH() + 1,
                XmNheight, regionPicturePtr->ImageSizeV() + 1,
                NULL);
  DoExposeRef();
}


// -------------------------------------------------------------------
void ProfApp::DoSubregion(Widget, XtPointer, XtPointer) {
  if(selectionBox.bigEnd(Amrvis::XDIR) == 0 || selectionBox.bigEnd(Amrvis::YDIR) == 0) {
    return;
  }
  Box subregionBox(selectionBox + ivLowOffset);
  IntVect ivOffset(subregionBox.smallEnd());

  SubregionProfApp(wTopLevel, subregionBox, ivOffset,
                   this, palFilename, fileName);

}


// -------------------------------------------------------------------
void ProfApp::AddStaticCallback(Widget w, String cbtype, profMemberCB cbf, void *d) {
  CBData *cbs = new CBData(this, d, cbf);

  // cbdPtrs.push_back(cbs)
  int nSize(cbdPtrs.size());
  cbdPtrs.resize(nSize + 1);
  cbdPtrs[nSize] = cbs;

  XtAddCallback(w, cbtype, (XtCallbackProc) &ProfApp::StaticCallback,
		(XtPointer) cbs);
}


// -------------------------------------------------------------------
string ProfApp::GetRegionName(Real r) {
  int i = int(r);
  std::map<int, std::string>::iterator regIter = regNames.find(i);
  if(regIter != regNames.end()) {
    return(regIter->second);
  } else {
    return("Bad regName value.");
  }
}


// -------------------------------------------------------------------
int ProfApp::FindRegionTimeRangeIndex(int whichRegion, Real time) {
  if(whichRegion < 0 || whichRegion >= dtr.size()) {
    amrex::Print() << "**** Error in ProfApp::FindRegionTimeRangeIndex:  "
                   << "whichRegion out of range: " << endl;
  } else {
    for(int it(0); it < dtr[whichRegion].size(); ++it) {
      if(dtr[whichRegion][it].Contains(time)) {
        return it;
      }
    }
  }
  return -42;  // ---- bad index
}
// -------------------------------------------------------------------
void ProfApp::ReplayClickHistory()
{

  // RegionTimeRanges initialization is expensive. 
  // Only do it when/if needed.
  if (!clickHistory.IsInitialized())
  {
    amrex::DataServices::Dispatch(amrex::DataServices::InitTimeRanges, dataServicesPtr[0]); 
    rtr = dataServicesPtr[0]->GetRegionsProfStats().GetRegionTimeRanges();
    clickHistory.SetInit(true);
  }

  // If subset since last adjustment, change trim rtr to current window.
  if (clickHistory.WasSubset())
  {
    clickHistory.SetSubset(false);
    // Trim down the region time ranges based on the subregion selected
    // CalcTimeRange = entire range of data
    // SubTimeRange = selected time range
    BLProfStats::TimeRange subRange(regionPicturePtr->SubTimeRange());
    for (int n(0); n < rtr.size(); ++n) {             // proc #
      for (int r(0); r < rtr[n].size(); ++r) {        // named region
        for (int t(0); t < rtr[n][r].size(); ++t) {   // instance of the region

           BLProfStats::TimeRange regionRange = rtr[n][r][t]; 
           BLProfStats::TimeRange trimmedRange( std::max(subRange.startTime, regionRange.startTime),
                                                std::min(subRange.stopTime, regionRange.stopTime));
           if (trimmedRange.startTime > trimmedRange.stopTime)   // Occurs if region is outside new window.
           {
             rtr[n][r][t] = BLProfStats::TimeRange(0.0, 0.0);
           }
           else
           {
             rtr[n][r][t] = trimmedRange;
           }
        }
      }
    }
    clickHistory.SetSubset(false);
  }

  // If reset to AllOn or AllOff since last replay, begin by resetting.
  if (clickHistory.IsReset())
  {
    std::cout << endl << "ClickHistory: reset" << endl;
    for(int i(0); i < filterTimeRanges.size(); ++i) {
      filterTimeRanges[i].clear();
      if (clickHistory.IsOn())
      {
         filterTimeRanges[i].push_back(regionPicturePtr->SubTimeRange());
      }
    }
  }

  // Replay the current click history to get the properly filter for all procs.
  int dvi, rtri; 
  bool add;
  while( clickHistory.Replay(dvi, rtri, add) )
  {
    for (int i(0); i< filterTimeRanges.size(); ++i)
      if (add)
      {
         if (i == 0)
           { std::cout << endl << "ClickHistory: add" << endl; }
         BLProfStats::AddPiece(filterTimeRanges[i], rtr[i][dvi][rtri]);
      }
      else
      { 
         if (i == 0)
           { std::cout << endl << "ClickHistory: remove" << endl; }
         BLProfStats::RemovePiece(filterTimeRanges[i], rtr[i][dvi][rtri]);
      }
  }

  dataServicesPtr[0]->GetCommOutputStats().SetFilterTimeRanges(filterTimeRanges);

}

// -------------------------------------------------------------------
void ProfApp::AddStaticEventHandler(Widget w, EventMask mask, profMemberCB cbf, void *d)
{
  CBData *cbs = new CBData(this, d, cbf);

  // cbdPtrs.push_back(cbs)
  int nSize(cbdPtrs.size());
  cbdPtrs.resize(nSize + 1);
  cbdPtrs[nSize] = cbs;

  XtAddEventHandler(w, mask, false, (XtEventHandler) &ProfApp::StaticEvent,
		    (XtPointer) cbs);
}
 
 
// -------------------------------------------------------------------
XtIntervalId ProfApp::AddStaticTimeOut(int time, profMemberCB cbf, void *d) {
  CBData *cbs = new CBData(this, d, cbf);

  // cbdPtrs.push_back(cbs)
  int nSize(cbdPtrs.size());
  cbdPtrs.resize(nSize + 1);
  cbdPtrs[nSize] = cbs;

  return XtAppAddTimeOut(appContext, time,
			 (XtTimerCallbackProc) &ProfApp::StaticTimeOut,
			 (XtPointer) cbs);
}


// -------------------------------------------------------------------
void ProfApp::StaticCallback(Widget w, XtPointer client_data, XtPointer call_data) {
  CBData *cbs = (CBData *) client_data;
  ProfApp *obj = cbs->instance;
  (obj->*(cbs->cbFunc))(w, (XtPointer) cbs->data, call_data);
}


// -------------------------------------------------------------------
void ProfApp::StaticEvent(Widget w, XtPointer client_data, XEvent *event, char*) {
  CBData *cbs = (CBData *) client_data;
  ProfApp *obj = cbs->instance;
  (obj->*(cbs->cbFunc))(w, (XtPointer) cbs->data, (XtPointer) event);
}


// -------------------------------------------------------------------
void ProfApp::StaticTimeOut(XtPointer client_data, XtIntervalId * call_data) {
  CBData *cbs = (CBData *) client_data;
  ProfApp *obj = cbs->instance;
  (obj->*(cbs->cbFunc))(None, (XtPointer) cbs->data, (XtPointer) call_data);
}


// -------------------------------------------------------------------
int  ProfApp::initialWindowHeight;
int  ProfApp::initialWindowWidth;
int  ProfApp::placementOffsetX = 0;
int  ProfApp::placementOffsetY = 0;

void ProfApp::SetInitialWindowHeight(int initWindowHeight) {
  ProfApp::initialWindowHeight = initWindowHeight;
}

void ProfApp::SetInitialWindowWidth(int initWindowWidth) {
  ProfApp::initialWindowWidth = initWindowWidth;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
