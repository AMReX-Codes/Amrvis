// ---------------------------------------------------------------
// XYPlotWin.cpp
// ---------------------------------------------------------------
#include <AMReX_ParallelDescriptor.H>

#include <Xm/AtomMgr.h>
#include <Xm/Protocols.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeB.h>
#include <Xm/PushBG.h>
#include <Xm/PushB.h>
#include <Xm/CascadeBG.h>
#include <Xm/Frame.h>
#include <Xm/ScrolledW.h>
#include <Xm/DrawingA.h>
#include <Xm/Text.h>
#include <Xm/DialogS.h>
#include <Xm/LabelG.h>
#include <Xm/ToggleBG.h>
#include <Xm/ToggleB.h>
#include <Xm/FileSB.h>
#include <Xm/MessageB.h>

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <XYPlotWin.H>
#include <AVPApp.H>
#include <PltApp.H>
#include <PltAppState.H>
#include <GraphicsAttributes.H>
#include <AMReX_AmrData.H>
#include <AMReX_DataServices.H>

#include <iostream>
#include <iomanip>
#include <limits>
#include <cmath>
#include <cstdlib>
using std::setw;
using std::cout;
using std::cerr;
using std::endl;

using namespace amrex;

#define MARK (fprintf(stderr, "Mark at file %s, line %d.\n", __FILE__, __LINE__))
#define nlog10(x)      (x == 0.0 ? 0.0 : log10(x) + 1e-15)

// Hack fix for compiler bug for window manager calls
#ifndef FALSE
#define FALSE false
#endif

// Bitmap data for various data mark styles
static char markBits[8][8] = {
  {0x00, 0x00, 0x1c, 0x1c, 0x1c, 0x00, 0x00, 0x00},
  {0x00, 0x3e, 0x22, 0x22, 0x22, 0x3e, 0x00, 0x00},
  {0x00, 0x1c, 0x36, 0x22, 0x36, 0x1c, 0x00, 0x00},
  {0x00, 0x22, 0x14, 0x08, 0x14, 0x22, 0x00, 0x00},
  {0x00, 0x08, 0x14, 0x22, 0x14, 0x08, 0x00, 0x00},
  {0x00, 0x1c, 0x14, 0x1c, 0x14, 0x1c, 0x00, 0x00},
  {0x00, 0x1c, 0x2a, 0x36, 0x2a, 0x1c, 0x00, 0x00},
  {0x00, 0x3e, 0x1c, 0x08, 0x1c, 0x3e, 0x00, 0x00}
};

static param_style param_null_style = {STYLE, 0, (char *) 0};

#include <cctype>

using std::endl;

// Some macros for obtaining parameters.
#define PM_INT(name) ((parameters->Get_Parameter(const_cast<char *>(name), &param_temp)) ? \
   param_temp.intv.value : (BL_ASSERT(0), (int) 0))
#define PM_STRING(name) ((parameters->Get_Parameter(const_cast<char *>(name), &param_temp)) ? \
   param_temp.strv.value : (BL_ASSERT(0), (char *) 0))
#define PM_COLOR(name) ((parameters->Get_Parameter(const_cast<char *>(name), &param_temp)) ? \
   param_temp.pixv.value : (BL_ASSERT(0), param_null_color))
#define PM_FONT(name) ((parameters->Get_Parameter(const_cast<char *>(name), &param_temp)) ? \
   param_temp.fontv.value : (BL_ASSERT(0), (XFontStruct *) 0))
#define PM_STYLE(name) ((parameters->Get_Parameter(const_cast<char *>(name), &param_temp)) ? \
   param_temp.stylev : (BL_ASSERT(0), param_null_style))
#define PM_BOOL(name) ((parameters->Get_Parameter(const_cast<char *>(name), &param_temp)) ? \
   param_temp.boolv.value : (BL_ASSERT(0), 0))
#define PM_DBL(name) ((parameters->Get_Parameter(const_cast<char *>(name), &param_temp)) ? \
   param_temp.dblv.value : (BL_ASSERT(0), 0.0))
#define PM_PIXEL(name) ((parameters->Get_Parameter(const_cast<char *>(name), &param_temp)) ? \
   pal->makePixel(param_temp.pixv.iColorMapSlot) : (BL_ASSERT(0), (Pixel) 0))


// -------------------------------------------------------------------
XYPlotWin::~XYPlotWin() {
  if(pltParent->PaletteCBQ()) {
    XtRemoveAllCallbacks(pltParent->GetPalArea(), XmNinputCallback);
    pltParent->SetPaletteCBQ(false);
  }
  if(pltParent->GetXYPlotWin(whichType) == this) {
    pltParent->DetachXYPlotWin(whichType);
  }
  CBdoClearData(None, NULL, NULL);
  if(wOptionsDialog != None) {
    XtDestroyWidget(wOptionsDialog);
  }
  if(wExportFileDialog != None) {
    XtDestroyWidget(wExportFileDialog);
  }
  XtDestroyWidget(wXYPlotTopLevel);
  delete gaPtr;
  delete [] Xsegs[0];
  delete [] Xsegs[1];
  delete [] XUnitText;
  delete [] YUnitText;
  delete [] formatY;
  delete [] formatX;
  delete [] pltTitle;

  // delete all the call back parameter structs
  int nSize;
  for(nSize = 0; nSize < xycbdPtrs.size(); ++nSize) {
    delete xycbdPtrs[nSize];
  }
  for(nSize = 0; nSize < xymenucbdPtrs.size(); ++nSize) {
    delete xymenucbdPtrs[nSize];
  }
  colorChangeItem = nullptr;
  pltParent = nullptr;
}


// -------------------------------------------------------------------
XYPlotWin::XYPlotWin(char *title, XtAppContext app, Widget w, AVPApp *parent,
		     int type, int curr_frame)
          : appContext(app),
            wTopLevel(w),
            pltParent(parent),
            whichType(type),
            currFrame(curr_frame)
{
  int idx;
  char buffer[Amrvis::BUFSIZE];
  pltTitle = new char[strlen(title) + 1];
  strcpy(pltTitle, title);
  params param_temp;   // temporary parameter grabbing slot
  colorChangeItem = nullptr;

  // Store some local stuff from the parent.
  parameters = pltParent->GetXYPlotParameters();

  wExportFileDialog = None;
  wOptionsDialog = None;

  // Standard flags.
  zoomedInQ = false;
  saveDefaultQ = 0;
  animatingQ = false;
#if (BL_SPACEDIM != 3)
  currFrame = 0;
#endif

  // Create empty dataset list.
  numDrawnItems = 0;
  numItems = 0;
  SetBoundingBox();

  iCurrHint = 1;
  
  char testc[] = "WM_DELETE_WINDOW";
  WM_DELETE_WINDOW = XmInternAtom(XtDisplay(wTopLevel),
				  //"WM_DELETE_WINDOW", false);
				  testc, false);

  // --------------------------------------------------------  main window
  int winOffsetX, winOffsetY;
  int winWidth = PM_INT("InitialWindowWidth");
  int winHeight = PM_INT("InitialWindowHeight");

  if(whichType == Amrvis::XDIR) {
    winOffsetX = PM_INT("InitialXWindowOffsetX");
    winOffsetY = PM_INT("InitialXWindowOffsetY");
  } else if(whichType == Amrvis::YDIR) {
    winOffsetX = PM_INT("InitialYWindowOffsetX");
    winOffsetY = PM_INT("InitialYWindowOffsetY");
  } else {
    winOffsetX = PM_INT("InitialZWindowOffsetX");
    winOffsetY = PM_INT("InitialZWindowOffsetY");
  }

  sprintf(buffer, "%s %c Value 1D plot", pltTitle, whichType + 'X');

  wXYPlotTopLevel =  XtVaCreatePopupShell(buffer, topLevelShellWidgetClass,
					  wTopLevel,
					  XmNscrollingPolicy, XmAUTOMATIC,
					  XmNdeleteResponse, XmDO_NOTHING,
					  XmNx,		winOffsetX,
					  XmNy,		winOffsetY,
					  NULL);
  XmAddWMProtocolCallback(wXYPlotTopLevel, WM_DELETE_WINDOW,
			  (XtCallbackProc) CBcloseXYPlotWin,
			  (XtPointer) this);
  Widget wControlArea, wLegendArea;
  Widget wExportButton, wOptionsButton, wCloseButton;
  Widget wAllButton, wNoneButton, wClearButton;
  XmString label_str1, label_str2, label_str3;

  wControlArea = XtVaCreateManagedWidget("controlArea", xmFormWidgetClass,
			                 wXYPlotTopLevel,
			                 NULL);

  // --------------------------------------------------------  legend
  wLegendArea = XtVaCreateManagedWidget("legendarea", xmFormWidgetClass,
			    wControlArea,
			    XmNtopAttachment,    XmATTACH_FORM,
			    XmNbottomAttachment, XmATTACH_FORM,
			    XmNrightAttachment,  XmATTACH_FORM,
			    NULL);

  wLegendMenu = XtVaCreateManagedWidget("legendmenu", xmFormWidgetClass,
			    wLegendArea,
			    XmNleftAttachment, XmATTACH_FORM,
			    XmNtopAttachment, XmATTACH_FORM,
			    XmNrightAttachment, XmATTACH_FORM,
			    NULL);

  label_str1 = XmStringCreateSimple(const_cast<char *> ("Export"));
  label_str2 = XmStringCreateSimple(const_cast<char *> ("Options"));
  label_str3 = XmStringCreateSimple(const_cast<char *> ("Close"));
  wExportButton = XtVaCreateManagedWidget("Export", xmPushButtonGadgetClass,
			    wLegendMenu,
			    XmNlabelString,     label_str1,
			    XmNleftAttachment,  XmATTACH_FORM,
			    XmNleftOffset,      2,
			    XmNtopAttachment,   XmATTACH_FORM,
			    XmNtopOffset,       2,
			    XmNwidth,           60,
			    NULL);
  AddStaticCallback(wExportButton, XmNactivateCallback,
		    &XYPlotWin::CBdoExportFileDialog, NULL);

  wOptionsButton = XtVaCreateManagedWidget("options", xmPushButtonGadgetClass,
			    wLegendMenu,
			    XmNlabelString,     label_str2,
			    XmNleftAttachment,  XmATTACH_WIDGET,
			    XmNleftWidget,      wExportButton,
			    XmNleftOffset,      2,
			    XmNtopAttachment,   XmATTACH_FORM,
			    XmNtopOffset,       2,
			    XmNwidth,           60,
			    NULL);
  AddStaticCallback(wOptionsButton, XmNactivateCallback,
		    &XYPlotWin::CBdoOptions, NULL);
  wCloseButton = XtVaCreateManagedWidget("close", xmPushButtonGadgetClass,
			    wLegendMenu,
			    XmNlabelString,     label_str3,
			    XmNleftAttachment,  XmATTACH_WIDGET,
			    XmNleftWidget,      wOptionsButton,
			    XmNleftOffset,      2,
			    XmNtopAttachment,   XmATTACH_FORM,
			    XmNtopOffset,       2,
			    XmNwidth,           60,
			    NULL);
  XtAddCallback(wCloseButton, XmNactivateCallback,
		(XtCallbackProc) CBcloseXYPlotWin, (XtPointer) this);

  XmStringFree(label_str1);
  XmStringFree(label_str2);
  XmStringFree(label_str3);

  label_str1 = XmStringCreateSimple(const_cast<char *>("All"));
  label_str2 = XmStringCreateSimple(const_cast<char *>("None"));
  label_str3 = XmStringCreateSimple(const_cast<char *>("Clear"));
  wAllButton = XtVaCreateManagedWidget("All", xmPushButtonGadgetClass,
			    wLegendMenu,
			    XmNlabelString,     label_str1,
			    XmNleftAttachment,  XmATTACH_FORM,
			    XmNleftOffset,      2,
			    XmNtopAttachment,   XmATTACH_WIDGET,
			    XmNtopWidget,       wExportButton,
			    XmNtopOffset,       2,
			    XmNwidth,           60,
			    NULL);
  AddStaticCallback(wAllButton, XmNactivateCallback,
		    &XYPlotWin::CBdoSelectAllData, NULL);
  wNoneButton = XtVaCreateManagedWidget("None", xmPushButtonGadgetClass,
			    wLegendMenu,
			    XmNlabelString,     label_str2,
			    XmNleftAttachment,  XmATTACH_WIDGET,
			    XmNleftWidget,      wAllButton,
			    XmNleftOffset,      2,
			    XmNtopAttachment,   XmATTACH_WIDGET,
			    XmNtopWidget,       wOptionsButton,
			    XmNtopOffset,       2,
			    XmNwidth,           60,
			    NULL);
  AddStaticCallback(wNoneButton, XmNactivateCallback,
		    &XYPlotWin::CBdoDeselectAllData, NULL);
  wClearButton = XtVaCreateManagedWidget("Clear", xmPushButtonGadgetClass,
			    wLegendMenu,
			    XmNlabelString,     label_str3,
			    XmNleftAttachment,  XmATTACH_WIDGET,
			    XmNleftWidget,      wNoneButton,
			    XmNleftOffset,      2,
			    XmNtopAttachment,   XmATTACH_WIDGET,
			    XmNtopWidget,       wCloseButton,
			    XmNtopOffset,       2,
			    XmNwidth,           60,
			    NULL);
  AddStaticCallback(wClearButton, XmNactivateCallback,
		    &XYPlotWin::CBdoClearData, NULL);
  
  XmStringFree(label_str1);
  XmStringFree(label_str2);
  XmStringFree(label_str3);

  wScrollArea = XtVaCreateManagedWidget("scrollArea", xmScrolledWindowWidgetClass,
			    wLegendArea,
			    XmNleftAttachment,	XmATTACH_FORM,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNbottomAttachment, XmATTACH_FORM,
			    XmNtopAttachment,	XmATTACH_WIDGET,
			    XmNtopWidget,       wLegendMenu,
			    XmNtopOffset,	2,
			    XmNscrollingPolicy,	XmAUTOMATIC,
			    NULL);

  wLegendButtons = XtVaCreateManagedWidget("legendbuttons", xmFormWidgetClass,
			    wScrollArea,
			    NULL);
  
  XtVaSetValues(wScrollArea, XmNworkWindow, wLegendButtons, NULL); 

  XtManageChild(wLegendMenu);
  XtManageChild(wLegendButtons);
  XtManageChild(wScrollArea);
  XtManageChild(wLegendArea);

  // PLOT
  wPlotWin = XtVaCreateManagedWidget("plotwin", xmDrawingAreaWidgetClass,
			    wControlArea,
			    XmNtopAttachment,    XmATTACH_FORM,
			    XmNbottomAttachment, XmATTACH_FORM,
			    XmNleftAttachment,   XmATTACH_FORM,
			    XmNrightAttachment,  XmATTACH_WIDGET,
			    XmNrightWidget,      wLegendArea,
			    XmNwidth,            winWidth,
			    XmNheight,           winHeight,
			    NULL);
  
  AddStaticCallback(wPlotWin, XmNexposeCallback,
		    &XYPlotWin::CBdoDrawPlot, NULL);
  AddStaticCallback(wPlotWin, XmNresizeCallback,
		    &XYPlotWin::CBdoRedrawPlot, NULL);
  AddStaticCallback(wPlotWin, XmNinputCallback,
		    &XYPlotWin::CBdoRubberBanding, NULL);
  AddStaticEventHandler(wPlotWin, PointerMotionMask | LeaveWindowMask,
			&XYPlotWin::CBdoDrawLocation, NULL);
  XtManageChild(wPlotWin);
  XtManageChild(wControlArea);

  XtPopup(wXYPlotTopLevel, XtGrabNone);
  pWindow = XtWindow(wPlotWin);
  SetPalette();
  gaPtr = new GraphicsAttributes(wXYPlotTopLevel);
  disp = gaPtr->PDisplay();
  vis  = gaPtr->PVisual();
  if(vis != XDefaultVisual(disp, gaPtr->PScreenNumber())) {
    XtVaSetValues(wXYPlotTopLevel, XmNvisual, vis, XmNdepth, 8, NULL);
  }
  cursor = XCreateFontCursor(disp, XC_left_ptr);
  zoomCursor = XCreateFontCursor(disp, XC_sizing);
  XtVaGetValues(wPlotWin,
		XmNforeground, &foregroundPix,
		XmNbackground, &backgroundPix,
		NULL);
  Palette *pal = pltParent->GetPalettePtr();
  gridPix = PM_PIXEL("GridColor");
  textPix = PM_PIXEL("TextColor");

  labeltextFont = PM_FONT("LabelFont");
  titletextFont = PM_FONT("TitleFont");

  // gc's for labels and titles, rubber banding, segments, and dots.
  XGCValues gcvals;
  gcvals.font = labeltextFont->fid;
  gcvals.foreground = textPix;
  labeltextGC = XCreateGC(disp, pWindow, GCFont | GCForeground, &gcvals);
  gcvals.font = titletextFont->fid;
  titletextGC = XCreateGC(disp, pWindow, GCFont | GCForeground, &gcvals);
  gcvals.function = GXxor;
  rbGC  = XCreateGC(disp, gaPtr->PRoot(), GCFunction, &gcvals);
  segGC = XCreateGC(disp, gaPtr->PRoot(), 0, NULL);
  dotGC = XCreateGC(disp, gaPtr->PRoot(), 0, NULL);

  // Allocate space for XSegment's
  numXsegs = NUM_INIT_XSEGS;
  Xsegs[0] = new XSegment[numXsegs];
  Xsegs[1] = new XSegment[numXsegs];

  // Get parameters and  attributes out of parameters database
  // and initialize bitmaps for line styles
  markQ      = PM_BOOL("Markers");
  tickQ      = PM_BOOL("Ticks");
  axisQ      = PM_BOOL("TickAxis");
  boundBoxQ  = PM_BOOL("BoundBox");
  plotLinesQ = PM_BOOL("PlotLines");
  dispHintsQ = PM_BOOL("DisplayHints");
  gridW      = PM_INT("GridWidth");
  lineW      = PM_INT("LineWidth");
  dotW       = PM_INT("DotWidth");

  char *str;

  if(whichType == Amrvis::XDIR) {
    str = PM_STRING("XUnitTextX");
  } else if(whichType == Amrvis::YDIR) {
    str = PM_STRING("XUnitTextY");
  } else {
    str = PM_STRING("XUnitTextZ");
  }
  XUnitText = new char[strlen(str) + 1];
  strcpy(XUnitText, str);

  str = PM_STRING("YUnitText");
  YUnitText = new char[strlen(str) + 1];
  strcpy(YUnitText, str);

  str = PM_STRING("FormatX");
  formatX = new char[strlen(str) + 1];
  strcpy(formatX, str);

  str = PM_STRING("FormatY");
  formatY = new char[strlen(str) + 1];
  strcpy(formatY, str);

  gridStyle = PM_STYLE("GridStyle");

  for(idx = 0; idx < 8; ++idx) {
    sprintf(buffer, "%d.Style", idx);
    parameters->Get_Parameter(buffer, &param_temp);
    AllAttrs[idx].lineStyleLen = param_temp.stylev.len;
    strncpy(AllAttrs[idx].lineStyle, param_temp.stylev.dash_list,
	    param_temp.stylev.len);
    AllAttrs[idx].markStyle =
      XCreateBitmapFromData(disp, pWindow, markBits[idx], 8, 8);
  }
  for(idx = 0; idx != 8; ++idx) {
    lineFormats[idx] = 0x0;
  }
  
  // Set up the device information structure.
  devInfo.maxSegs  = numXsegs;
  devInfo.areaW = devInfo.areaH = 0; // Set later
  devInfo.bdrPad = BORDER_PADDING;
  devInfo.axisPad = AXIS_PADDING;
  devInfo.tickLen = TICKLENGTH;
  devInfo.axisW = XTextWidth(labeltextFont, "8", 1);
  devInfo.titleW = XTextWidth(titletextFont, "8", 1);
  devInfo.axisH = labeltextFont->max_bounds.ascent + 
                  labeltextFont->max_bounds.descent;
  devInfo.titleH = titletextFont->max_bounds.ascent + 
                  titletextFont->max_bounds.descent;

  xycbdPtrs.reserve(512);      // arbitrarily
  xymenucbdPtrs.reserve(512);  // arbitrarily
}


#if (BL_SPACEDIM != 3)
// -------------------------------------------------------------------
void XYPlotWin::InitializeAnimation(int curr_frame, int num_frames) {
  if(animatingQ) {
    return;
  }
  PltAppState *pas = pltParent->GetPltAppState();
  Real gmin(std::numeric_limits<Real>::max());
  Real gmax(std::numeric_limits<Real>::lowest());
  Real fmin, fmax;
  animatingQ = true;
  currFrame = curr_frame;
  numFrames = num_frames;
  for(list<XYPlotLegendItem *>::iterator ptr = legendList.begin();
      ptr != legendList.end(); ++ptr)
  {
    if((*ptr)->XYPLIlist->CopiedFrom() != NULL) {
      ::XYPlotDataList *tempList = (*ptr)->XYPLIlist;
      (*ptr)->XYPLIlist = pltParent->CreateLinePlot(Amrvis::ZPLANE, whichType,
					    (*ptr)->XYPLIlist->MaxLevel(),
					    (*ptr)->XYPLIlist->Gridline(),
					    &((*ptr)->XYPLIlist->DerivedName()));
      delete tempList;
    }
    (*ptr)->anim_lists = new Vector<::XYPlotDataList *>(numFrames);
    (*ptr)->ready_list = new Vector<char>(numFrames, 0);

// =================
    string sDerName = (*ptr)->XYPLIlist->DerivedName();
    const AmrData &amrData = pltParent->GetDataServicesPtr()->AmrDataRef();
    int snum = amrData.StateNumber(sDerName);
    Amrvis::MinMaxRangeType mmrt = pas->GetMinMaxRangeType();
    for(int iframe(0); iframe < numFrames; ++iframe) {
      pas->GetMinMax(mmrt, iframe, snum, fmin, fmax);
      gmin = std::min(gmin, fmin);
      gmax = std::max(gmax, fmax);
    }
// =================
  }

  lloY = gmin;
  hhiY = gmax;
  if(zoomedInQ == false) {
    SetBoundingBox();
    CBdoRedrawPlot(None, NULL, NULL);
  }
}


// -------------------------------------------------------------------
void XYPlotWin::UpdateFrame(int frame) {
  ::XYPlotDataList *tempList;
  int num_lists_changed(0);
  char buffer[Amrvis::BUFSIZE];
  sprintf(buffer, "%s %c Value 1D plot",
          AVGlobals::StripSlashes(pltParent->GetFileName()).c_str(),
	  whichType + 'X');
  XtVaSetValues(wXYPlotTopLevel, XmNtitle, buffer, NULL);
  if( ! animatingQ && zoomedInQ == false) {
    lloY = std::numeric_limits<Real>::max();
    hhiY = std::numeric_limits<Real>::lowest();
  }
  for(list<XYPlotLegendItem *>::iterator ptr = legendList.begin();
      ptr != legendList.end(); ++ptr)
  {
    if((*ptr)->drawQ == true) {
      ++num_lists_changed;
    }
    if(animatingQ) {
      (*(*ptr)->anim_lists)[currFrame] = (*ptr)->XYPLIlist;
      (*(*ptr)->ready_list)[currFrame] = 1;
      if((*(*ptr)->ready_list)[frame]) {
	(*ptr)->XYPLIlist = (*(*ptr)->anim_lists)[frame];
	continue;
      }
    } else {
      tempList = (*ptr)->XYPLIlist;
    }
    
    int level((*ptr)->XYPLIlist->CurLevel());
    (*ptr)->XYPLIlist = pltParent->CreateLinePlot(Amrvis::ZPLANE, whichType,
				          (*ptr)->XYPLIlist->MaxLevel(),
				          (*ptr)->XYPLIlist->Gridline(),
				          &(*ptr)->XYPLIlist->DerivedName());
    (*ptr)->XYPLIlist->SetLevel(level);
    (*ptr)->XYPLIlist->UpdateStats();
    if((*ptr)->XYPLIlist->NumPoints() > numXsegs) {
      numXsegs = (*ptr)->XYPLIlist->NumPoints() + 5;
      delete [] Xsegs[0];
      Xsegs[0] = new XSegment[numXsegs];
      delete [] Xsegs[1];
      Xsegs[1] = new XSegment[numXsegs];
    }
    if( ! animatingQ) {
      delete tempList;
      if(zoomedInQ == false && (*ptr)->drawQ == true) {
        UpdateBoundingBox((*ptr)->XYPLIlist);
      }
    }
  }
  currFrame = frame;
  if(num_lists_changed == 0) {
    return;
  }
  if(animatingQ) {
    clearData();
    drawGridAndAxis();
    drawData();
  } else {
    if(zoomedInQ == false) {
      SetBoundingBox();
    }
    CBdoRedrawPlot(None, NULL, NULL);
  }
}


// -------------------------------------------------------------------
void XYPlotWin::StopAnimation() {
  if( ! animatingQ) {
    return;
  }
  animatingQ = false;
  for(list<XYPlotLegendItem *>::iterator ptr = legendList.begin();
      ptr != legendList.end(); ++ptr)
  {
    for(int ii(0); ii != numFrames; ++ii) {
      if((*(*ptr)->ready_list)[ii] &&
         (*(*ptr)->anim_lists)[ii] != (*ptr)->XYPLIlist)
      {
	delete (*(*ptr)->anim_lists)[ii];
      }
    }
    delete (*ptr)->ready_list;
    delete (*ptr)->anim_lists;
  }
  lloY = std::numeric_limits<Real>::max();
  hhiY = std::numeric_limits<Real>::lowest();
  for(list<XYPlotLegendItem *>::iterator ptr = legendList.begin();
      ptr != legendList.end(); ++ptr)
  {
    if((*ptr)->drawQ == true) {
      UpdateBoundingBox((*ptr)->XYPLIlist);
    }
  }
  if(zoomedInQ == false) {
    SetBoundingBox();
    CBdoRedrawPlot(None, NULL, NULL);
  }
}
#endif



// -------------------------------------------------------------------
#define TRANX(xval) (((double) ((xval) - iXOrgX)) * dXUnitsPerPixel + dUsrOrgX)
#define TRANY(yval) (dUsrOppY - (((double) ((yval) - iXOrgY)) * dYUnitsPerPixel))
#define SCREENX(userX) \
    (((int) (((userX) - dUsrOrgX)/dXUnitsPerPixel + 0.5)) + iXOrgX)
#define SCREENY(userY) \
    (iXOppY - ((int) (((userY) - dUsrOrgY)/dYUnitsPerPixel + 0.5)))
//    (iXOppY - ((int) (log10(((userY) - dUsrOrgY)/dYUnitsPerPixel + 0.5))))


// -------------------------------------------------------------------
void XYPlotWin::SetBoundingBox(double lowXIn,  double lowYIn,
			       double highXIn, double highYIn)
{
  //cout << "?????????????? >>>>>> _in XYPlotWin::SetBoundingBox" << endl;
  //cout << "lowXIn lowYIn highXIn highYIn = " << lowXIn << "  " << lowYIn << "  " << highXIn << "  " << highYIn << endl;
  //cout << "lloX lloY hhiX hhiY = " << lloX << "  " << lloY << "  " << hhiX << "  " << hhiY << endl;
  double pad;
  if(highXIn > lowXIn) {
    loX = lowXIn;
    hiX = highXIn;
  } else {
    if(numDrawnItems == 0) {
      loX = -1.0;
      loY = -1.0;
      hiX =  1.0;
      hiY =  1.0;
      lloX =  std::numeric_limits<Real>::max();
      lloY =  std::numeric_limits<Real>::max();
      hhiX =  std::numeric_limits<Real>::lowest();
      hhiY =  std::numeric_limits<Real>::lowest();
      return;
    }
    loX = lloX;
    hiX = hhiX;
  }
  if(highYIn > lowYIn) {
    loY = lowYIn;
    hiY = highYIn;
  } else {
    loY = lloY;
    hiY = hhiY;
  }
  
  // Increase the padding for aesthetics
  if(hiX - loX == 0.0) {
    pad = std::max(0.5, std::fabs(hiX * 0.5));
    hiX += pad;
    loX -= pad;
  }
  if(hiY - loY == 0.0) {
    pad = std::max(0.5, std::fabs(hiY * 0.5));
    hiY += pad;
    loY -= pad;
  }

  if(zoomedInQ == false) {
    // Add 10% padding to bounding box
    pad = (hiX - loX) * 0.05;
    loX -= pad;
    hiX += pad;
    pad = (hiY - loY) * 0.05;
    loY -= pad;
    hiY += pad;
  }
  //cout << "loX loY hiX hiY = " << loX << "  " << loY << "  " << hiX << "  " << hiY << endl;
  //cout << "?????????????? <<<<<< _out XYPlotWin::SetBoundingBox" << endl;
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoRedrawPlot(Widget /*w*/, XtPointer, XtPointer) {
  XClearWindow(disp, pWindow);
  CBdoDrawPlot(None, NULL, NULL);
}


// -------------------------------------------------------------------
void XYPlotWin::CalculateBox() {
  XWindowAttributes win_attr;
  XGetWindowAttributes(disp, pWindow, &win_attr);

  devInfo.areaW = win_attr.width;
  devInfo.areaH = win_attr.height;

  // Figure out the transformation constants.  Draw only if valid.
  // First,  we figure out the origin in the X window.  Above the space we
  // have the title and the Y axis unit label. To the left of the space we
  // have the Y axis grid labels.

  // Here we make an arbitrary label to find out how big an offset we need
  char buff[128];
  sprintf(buff, formatY, -200.0);
  XCharStruct bb;
  int dir, ascent, descent;
  XTextExtents(labeltextFont, buff, strlen(buff), &dir, &ascent, &descent, &bb);

  iXOrgX = 2 * devInfo.bdrPad + bb.rbearing - bb.lbearing;
  if(dispHintsQ) {
    iXOrgY = devInfo.bdrPad + (5 * devInfo.axisH) / 2;
  } else {
    iXOrgY = devInfo.bdrPad + (3 * devInfo.axisH) / 2;
  }
  
  // Now we find the lower right corner.  Below the space we have the X axis
  // grid labels.  To the right of the space we have the X axis unit label
  // and the legend.  We assume the worst case size for the unit label.  
  iXOppX = devInfo.areaW - devInfo.bdrPad - devInfo.axisW;
  iXOppY = devInfo.areaH - devInfo.bdrPad - (2 * devInfo.axisH);

  iXLocWinX = devInfo.bdrPad + (30 * devInfo.axisW);
  iXLocWinY = devInfo.areaH - devInfo.bdrPad - devInfo.axisH;
  
  // Is the drawing area too small?
  if((iXOrgX >= iXOppX) || (iXOrgY >= iXOppY)) {
    return;
  }
  
  // We now have a bounding box for the drawing region. Figure out the units
  // per pixel using the data set bounding box.
  dXUnitsPerPixel = (hiX - loX) / ((double) (iXOppX - iXOrgX));
  dYUnitsPerPixel = (hiY - loY) / ((double) (iXOppY - iXOrgY));
  
  // Find origin in user coordinate space.  We keep the center of the
  // original bounding box in the same place.
  double bbCenX((loX + hiX) * 0.5);
  double bbCenY((loY + hiY) * 0.5);
  double bbHalfWidth(((double) (iXOppX - iXOrgX))  * 0.5 * dXUnitsPerPixel);
  double bbHalfHeight(((double) (iXOppY - iXOrgY)) * 0.5 * dYUnitsPerPixel);
  dUsrOrgX = bbCenX - bbHalfWidth;
  dUsrOrgY = bbCenY - bbHalfHeight;
  dUsrOppX = bbCenX + bbHalfWidth;
  dUsrOppY = bbCenY + bbHalfHeight;
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoDrawPlot(Widget, XtPointer, XtPointer) {
  CalculateBox();
  // Everything is defined so we can now use the SCREENX and SCREENY
  // transformations.
  drawGridAndAxis();
  drawData();
  if(dispHintsQ) {
    drawHint();
  }
  XFlush(disp);
}


// -------------------------------------------------------------------
void XYPlotWin::AddDataList(::XYPlotDataList *new_list,
			    XYPlotLegendItem *insert_after)
{
  if(++numItems > 64) {
    // Too many data lists to assign unique color/style.  Delete.
    PrintMessage(const_cast<char *>("Too many lines in plotter!\n"));
    numItems = 64;
    delete new_list;
    return;
  }

  // Find a unique color and style.
  int i, j;
  char mask;
  for(i = 0; lineFormats[i] == 0xff; ++i) {
    ;  // do nothing
  }
  for(j = 0, mask = 0x1; lineFormats[i] & mask; ++j, mask = mask << 1) {
    ;  // do nothing
  }

  XYPlotLegendItem *new_item = new XYPlotLegendItem;

  new_item->XYPLIlist = new_list;
  lineFormats[i] |= mask;
  new_item->style = i;
  new_item->color = j;
  new_item->pixel = AllAttrs[j].pixelValue;

  // Append this new list to our data set list.
  new_item->frame =
    XtVaCreateManagedWidget("frame", xmFrameWidgetClass,
			    wLegendButtons,
			    XmNshadowType,        XmSHADOW_ETCHED_IN,
			    XmNhighlightPixmap,   NULL,
			    XmNtopShadowColor,    foregroundPix,
			    XmNbottomShadowColor, foregroundPix,
			    //XmNtopAttachment,     XmATTACH_WIDGET,
			    XmNtopOffset,         0,
			    NULL);

  new_item->wid = XtVaCreateManagedWidget("button", xmDrawingAreaWidgetClass,
			    new_item->frame,
			    XmNwidth,             155,
			    XmNheight,	          ((BL_SPACEDIM == 3) ?
						   (15 + 2 * devInfo.axisH) :
						   (15 + devInfo.axisH)),
			    NULL);
  pltParent->GetPalettePtr()->SetWindowPalette(pltParent->GetPaletteName(),
					       XtWindow(new_item->wid));
  Widget wid, levelmenu;
  char buffer[Amrvis::BUFSIZE];
  new_item->menu = XmCreatePopupMenu(new_item->wid, const_cast<char *>("popup"), NULL, 0);
  if(new_list->MaxLevel() != 0) {
    XmString label_str = XmStringCreateSimple(const_cast<char *>("Level"));
    levelmenu = XmCreatePulldownMenu(new_item->menu, const_cast<char *>("pulldown"), NULL, 0);
    XtVaCreateManagedWidget("Level", xmCascadeButtonGadgetClass,
			    new_item->menu,
			    XmNsubMenuId, levelmenu,
			    XmNlabelString, label_str,
			    XmNmnemonic, 'L',
			    NULL);
    XmStringFree(label_str);
    
    for(int ii(0); ii <= new_list->MaxLevel(); ++ii) {
      sprintf(buffer, "%d/%d", ii, new_list->MaxLevel());
      wid = XtVaCreateManagedWidget(buffer, xmPushButtonGadgetClass,
				    levelmenu, NULL);
      if(ii < 10) {
        XtVaSetValues(wid, XmNmnemonic, ii + '0', NULL);
      }
      XYMenuCBData *xymenucb = new XYMenuCBData(new_item, ii);
      int nSize(xymenucbdPtrs.size());
      xymenucbdPtrs.resize(nSize + 1);
      xymenucbdPtrs[nSize] = xymenucb;

      AddStaticCallback(wid, XmNactivateCallback, &XYPlotWin::CBdoSetListLevel,
			xymenucb);
    }
  }

  wid = XtVaCreateManagedWidget("Copy", xmPushButtonGadgetClass,
				new_item->menu,
				XmNmnemonic, 'C',
				NULL);
  AddStaticCallback(wid, XmNactivateCallback, &XYPlotWin::CBdoCopyDataList,
		    new_item);
  wid = XtVaCreateManagedWidget("Delete", xmPushButtonGadgetClass,
				new_item->menu,
				XmNmnemonic, 'D',
				NULL);
  AddStaticCallback(wid, XmNactivateCallback,
		    &XYPlotWin::CBdoRemoveDataList, new_item);

  wChooseColor = XtVaCreateManagedWidget("Choose color", xmPushButtonGadgetClass,
				new_item->menu,
				XmNmnemonic, 'o',
				NULL);
  AddStaticCallback(wChooseColor, XmNactivateCallback,
		    &XYPlotWin::CBdoInitializeListColorChange, new_item);


  if(insert_after == NULL) {
    new_item->drawQ = true;          // Default to draw.
    ++numDrawnItems;
    new_list->UpdateStats();      // Find extremes, number of points.
    UpdateBoundingBox(new_list);
    if(zoomedInQ == false) {
      SetBoundingBox();
    }
    if(new_list->NumPoints() > numXsegs) {
      numXsegs = new_list->NumPoints() + 5;
      delete [] Xsegs[0];
      Xsegs[0] = new XSegment[numXsegs];
      delete [] Xsegs[1];
      Xsegs[1] = new XSegment[numXsegs];
    }
    legendList.push_back(new_item);
  } else {
    new_item->drawQ = insert_after->drawQ;
    if(new_item->drawQ == true) {
      ++numDrawnItems;
    } else {
      XtVaSetValues(new_item->frame,
		    XmNtopShadowColor, backgroundPix,
		    XmNbottomShadowColor, backgroundPix,
		    NULL);
    }
    // find the item
    for(list<XYPlotLegendItem *>::iterator ptr = legendList.begin();
        ptr != legendList.end(); ++ptr)
    {
      if((*ptr) == insert_after) {
        ++ptr;  // so we can insert before
	legendList.insert(ptr, new_item);
	break;
      }
    }

  }
  ReattachLegendFrames();

  AddStaticCallback(new_item->wid, XmNinputCallback,
		    &XYPlotWin::CBdoSelectDataList, new_item);
  AddStaticCallback(new_item->wid, XmNexposeCallback,
		    &XYPlotWin::CBdoDrawLegendItem, new_item);

  XtManageChild(new_item->frame);
  if(new_item->drawQ == true) {
    CBdoRedrawPlot(None, NULL, NULL);
  }
}


// -------------------------------------------------------------------
void XYPlotWin::ReattachLegendFrames() {
  if(legendList.empty()) {
    return;
  }
  list<XYPlotLegendItem *>::iterator liitem = legendList.begin();
  XtVaSetValues((*liitem)->frame,
		XmNtopAttachment, XmATTACH_FORM,
		NULL);
  list<XYPlotLegendItem *>::iterator liprevitem = liitem;
  ++liitem;
  while(liitem != legendList.end()) {
    XtVaSetValues((*liitem)->frame,
		  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, (*liprevitem)->frame,
                  NULL);
    ++liprevitem;
    ++liitem;
  }
}


// -------------------------------------------------------------------
void XYPlotWin::UpdateBoundingBox(::XYPlotDataList *xypdl) {
  //cout << "???????????????????? _in XYPlotWin::UpdateBoundingBox" << endl;
  if(xypdl->StartX() < lloX) {
    lloX = xypdl->StartX();
  }
  if(xypdl->EndX() > hhiX) {
    hhiX = xypdl->EndX();
  }
  if(xypdl->XYPDLLoY(xypdl->CurLevel()) < lloY) {
    lloY = xypdl->XYPDLLoY(xypdl->CurLevel());
  }
  if(xypdl->XYPDLHiY(xypdl->CurLevel()) > hhiY) {
    hhiY = xypdl->XYPDLHiY(xypdl->CurLevel());
  }
}


// -------------------------------------------------------------------
double XYPlotWin::InitGrid(double low, double high, double step) {
  
  // Hack fix for graphs of large constant graphs.  Sometimes the
  // step is too small in comparison to the size of the grid itself,
  // and rounding error takes its toll.  We "fix" this by multiplying
  // the step by an arbitrary number > 1 (1.2) when this happens.
  double gridHigh;
  int iLoopCheck(0);
  while(true) {
    dGridStep = roundUp(step);
    gridHigh = (ceil(high / dGridStep) + 1.0) * dGridStep;
    if(gridHigh + dGridStep != gridHigh) {
      break;
    }
    if(step < DBL_EPSILON) {
      step = DBL_EPSILON;
    }
    step *= 1.2;
    ++iLoopCheck;
    if(iLoopCheck > 1000) {
      break;
    }
  }
  dGridBase = (floor(low / dGridStep) + 1.0) * dGridStep;
  return dGridBase;
}


// -------------------------------------------------------------------
double XYPlotWin::roundUp(double val) {
  int idx;
  int exponent((int) floor(nlog10(val)));
  if(exponent < 0) {
    for(idx = exponent; idx < 0; ++idx) {
      val *= 10.0;
    }
  } else {
    for(idx = 0; idx < exponent; ++idx) {
      val *= 0.10;
    }
  }
  if(val > 5.0) {
    val = 10.0;
  } else if(val > 2.0) {
    val = 5.0;
  } else if(val > 1.0) {
    val = 2.0;
  } else {
    val = 1.0;
  }
  if(exponent < 0) {
    for(idx = exponent; idx < 0; ++idx) {
      val *= 0.10;
    }
  } else {
    for(idx = 0; idx < exponent; ++idx) {
      val *= 10.0;
    }
  }
  return val;
}


// -------------------------------------------------------------------
void XYPlotWin::writeValue(char *str, char *fmt, double val, int expv) {
  if(expv < 0) {
    for(int idx(expv); idx < 0; ++idx) {
      val *= 10.0;
    }
  } else {
    for(int idx(expv); idx > 0; --idx) {
      val *= 0.10;
    }
  }
  if(strchr(fmt, 'd') || strchr(fmt, 'x')) {
    sprintf(str, fmt, (int) val);
  } else {
    sprintf(str, fmt, val);
  }
}


// -------------------------------------------------------------------
// Clipping algorithm from Neumann and Sproull by Cohen and Sutherland
#define C_CODE(xval, yval, rtn) \
   rtn = 0; \
   if((xval) < dUsrOrgX) rtn = LEFT_CODE; \
   else if((xval) > dUsrOppX) rtn = RIGHT_CODE; \
   if((yval) < dUsrOrgY) rtn |= BOTTOM_CODE; \
   else if((yval) > dUsrOppY) rtn |= TOP_CODE



// -------------------------------------------------------------------
void XYPlotWin::drawGridAndAxis() {
  int expX, expY; // Engineering powers
  int Yspot, Xspot;
  char value[10], final[Amrvis::BUFSIZE + 10];
  double dXIncr, dYIncr, dXStart, dYStart, dYIndex, dXIndex, dLarger;
  XSegment segs[2];
  
  // Grid display powers are computed by taking the log of the largest
  // numbers and rounding down to the nearest multiple of 3.
  if(std::fabs(dUsrOrgX) > std::fabs(dUsrOppX)) {
    dLarger = std::fabs(dUsrOrgX);
  } else {
    dLarger = std::fabs(dUsrOppX);
  }
  expX = ((int) floor(nlog10(dLarger) / 3.0)) * 3;
  if(std::fabs(dUsrOrgY) > std::fabs(dUsrOppY)) {
    dLarger = std::fabs(dUsrOrgY);
  } else {
    dLarger = std::fabs(dUsrOppY);
  }
  expY = ((int) floor(nlog10(dLarger) / 3.0)) * 3;
  
  // With the powers computed,  we can draw the axis labels.
  Xspot = devInfo.bdrPad + (2 * devInfo.axisW);
  Yspot = 2 * devInfo.bdrPad;
  if(expY != 0) {
    sprintf(final, "%s x 10^%d", YUnitText, expY);
    textX(wPlotWin, Xspot, Yspot, final, T_LEFT, T_AXIS);
  } else {
    textX(wPlotWin, Xspot, Yspot, YUnitText, T_LEFT, T_AXIS);
  }
  
  Xspot = devInfo.areaW - devInfo.bdrPad;
  Yspot = devInfo.areaH - (2*devInfo.bdrPad);
  if(expX != 0) {
    sprintf(final, "%s x 10^%d", XUnitText, expX);
    textX(wPlotWin, Xspot, Yspot, final, T_RIGHT, T_AXIS);
  } else {
    textX(wPlotWin, Xspot, Yspot, XUnitText, T_RIGHT, T_AXIS);
  }
  
  // First,  the grid line labels
  dYIncr = (devInfo.axisPad + devInfo.axisH) * dYUnitsPerPixel;
  dYStart = InitGrid(dUsrOrgY, dUsrOppY, dYIncr);
  int iLoopCheck(0);
  for(dYIndex = 0.0; dYIndex < (dUsrOppY - dYStart); dYIndex += dGridStep) {
    //Yspot = SCREENY(dYIndex + dYStart);
Yspot = (iXOppY - ((int) (((dYIndex + dYStart) - dUsrOrgY)/dYUnitsPerPixel + 0.5)));
/*
VSHOWVAL(iXOppY);
VSHOWVAL(dYIndex);
VSHOWVAL(dYStart);
VSHOWVAL(dUsrOrgY);
VSHOWVAL(dYUnitsPerPixel);
VSHOWVAL(Yspot);
cout << endl;
*/
    // Write the axis label
    writeValue(value, formatY, (dYIndex + dYStart), expY);
    textX(wPlotWin, iXOrgX - devInfo.bdrPad, Yspot, value, T_RIGHT, T_AXIS);
    ++iLoopCheck;
    if(iLoopCheck > ((int) ((dUsrOppY - dYStart)/dGridStep))) {
      break;
    }
  }
  
  dXIncr = (devInfo.axisPad + (devInfo.axisW * 7)) * dXUnitsPerPixel;
  dXStart = InitGrid(dUsrOrgX, dUsrOppX, dXIncr);
  iLoopCheck = 0;
  for(dXIndex = 0.0; dXIndex < (dUsrOppX - dXStart); dXIndex += dGridStep) {
    Xspot = SCREENX(dXIndex + dXStart);
    // Write the axis label
    writeValue(value, formatX, (dXIndex + dXStart), expX);
    textX(wPlotWin, Xspot, devInfo.areaH-devInfo.bdrPad-devInfo.axisH,
	  value, T_BOTTOM, T_AXIS);
    ++iLoopCheck;
    if(iLoopCheck > ((int) ((dUsrOppX - dXStart)/dGridStep))) {
      break;
    }
  }
  
  // Now,  the grid lines or tick marks
  dYIncr = (devInfo.axisPad + devInfo.axisH) * dYUnitsPerPixel;
  dYStart = InitGrid(dUsrOrgY, dUsrOppY, dYIncr);
  iLoopCheck = 0;
  for(dYIndex = 0.0; dYIndex < (dUsrOppY - dYStart); dYIndex += dGridStep) {
    Yspot = SCREENY(dYIndex + dYStart);
    // Draw the grid line or tick mark
    if(tickQ && ! (axisQ && dYIndex == 0.0)) {
      segs[0].x1 = iXOrgX;
      segs[0].x2 = iXOrgX + devInfo.tickLen;
      segs[1].x1 = iXOppX - devInfo.tickLen;
      segs[1].x2 = iXOppX;
      segs[0].y1 = segs[0].y2 = segs[1].y1 = segs[1].y2 = Yspot;
      segX(wPlotWin, 1, &(segs[1]), gridW, L_AXIS, 0, 0);
    } else {
      segs[0].x1 = iXOrgX;
      segs[0].x2 = iXOppX;
      segs[0].y1 = segs[0].y2 = Yspot;
    }
    segX(wPlotWin, 1, segs, gridW, L_AXIS, 0, 0);
    ++iLoopCheck;
    if(iLoopCheck > ((int) ((dUsrOppY - dYStart)/dGridStep))) {
      break;
    }
  }
  
  dXIncr = (devInfo.axisPad + (devInfo.axisW * 7)) * dXUnitsPerPixel;
  dXStart = InitGrid(dUsrOrgX, dUsrOppX, dXIncr);
  iLoopCheck = 0;
  for(dXIndex = 0.0; dXIndex < (dUsrOppX - dXStart); dXIndex += dGridStep) {
    Xspot = SCREENX(dXIndex + dXStart);
    // Draw the grid line or tick marks
    if(tickQ && !(axisQ && dXIndex == 0.0)) {
      segs[0].x1 = segs[0].x2 = segs[1].x1 = segs[1].x2 = Xspot;
      segs[0].y1 = iXOrgY;
      segs[0].y2 = iXOrgY + devInfo.tickLen;
      segs[1].y1 = iXOppY - devInfo.tickLen;
      segs[1].y2 = iXOppY;
    } else {
      segs[0].x1 = segs[0].x2 = Xspot;
      segs[0].y1 = iXOrgY;
      segs[0].y2 = iXOppY;
    }
    segX(wPlotWin, 1, segs, gridW, L_AXIS, 0, 0);
    if(tickQ) {
      segX(wPlotWin, 1, &(segs[1]), gridW, L_AXIS, 0, 0);
    }
    ++iLoopCheck;
    if(iLoopCheck > ((int) ((dUsrOppX - dXStart)/dGridStep))) {
      break;
    }
  }

  if(boundBoxQ) { // draw bounding box
    XSegment bb[4];
    bb[0].x1 = bb[0].x2 = bb[1].x1 = bb[3].x2 = iXOrgX;
    bb[0].y1 = bb[2].y2 = bb[3].y1 = bb[3].y2 = iXOrgY;
    bb[1].x2 = bb[2].x1 = bb[2].x2 = bb[3].x1 = iXOppX;
    bb[0].y2 = bb[1].y1 = bb[1].y2 = bb[2].y1 = iXOppY;
    segX(wPlotWin, 4, bb, gridW, L_AXIS, 0, 0);
  }
}


// -------------------------------------------------------------------
void XYPlotWin::clearData() {
  XClearArea(disp, pWindow, iXOrgX + 1, iXOrgY + 1,
	     (iXOppX - iXOrgX - 2), (iXOppY - iXOrgY - 2), false);
}


// -------------------------------------------------------------------
void XYPlotWin::drawData() {
  double sx1, sy1, sx2, sy2, ssx1, ssx2, ssy1, ssy2, tx(0.0), ty(0.0);
  int code1, code2, cd, mark_inside;
  unsigned int X_idx;
  XSegment *ptr;
  int style;
  Pixel color;
  XFlush(disp);

  for(list<XYPlotLegendItem *>::iterator item = legendList.begin();
      item != legendList.end(); ++item)
  {
    if((*item)->drawQ == false) {
      continue;
    }
    if((*item)->XYPLIlist->NumPoints() == 0) {
      continue;
    }
    Vector<double> &xvals = (*item)->XYPLIlist->XVal((*item)->XYPLIlist->CurLevel());
    Vector<double> &yvals = (*item)->XYPLIlist->YVal((*item)->XYPLIlist->CurLevel());
    X_idx = 0;
    style = (*item)->style;
    color = (*item)->pixel;
    sx1 = xvals[0];
    sy1 = yvals[0];
    for(int ili(1); ili < (*item)->XYPLIlist->NumPoints(); ++ili) {
      sx2 = xvals[ili];
      sy2 = yvals[ili];

      ssx1 = sx1;
      ssx2 = sx2;
      ssy1 = sy1;
      ssy2 = sy2;

      // Put segment in (ssx1,ssy1) (ssx2,ssy2), clip to window boundary
      C_CODE(ssx1, ssy1, code1);
      C_CODE(ssx2, ssy2, code2);
      mark_inside = (code1 == 0);
      while(code1 || code2) {
	if(code1 & code2) {
	  break;
	}
	cd = (code1 ? code1 : code2);
	if(cd & LEFT_CODE) {	     // Crosses left edge
	  ty = ssy1 + (ssy2 - ssy1) * (dUsrOrgX - ssx1) / (ssx2 - ssx1);
	  tx = dUsrOrgX;
	} else if(cd & RIGHT_CODE) {  // Crosses right edge
	  ty = ssy1 + (ssy2 - ssy1) * (dUsrOppX - ssx1) / (ssx2 - ssx1);
	  tx = dUsrOppX;
	} else if(cd & BOTTOM_CODE) { // Crosses bottom edge
	  tx = ssx1 + (ssx2 - ssx1) * (dUsrOrgY - ssy1) / (ssy2 - ssy1);
	  ty = dUsrOrgY;
	} else if(cd & TOP_CODE) {    // Crosses top edge
	  tx = ssx1 + (ssx2 - ssx1) * (dUsrOppY - ssy1) / (ssy2 - ssy1);
	  ty = dUsrOppY;
	}
	if(cd == code1) {
	  ssx1 = tx;
	  ssy1 = ty;
	  C_CODE(ssx1, ssy1, code1);
	} else {
	  ssx2 = tx;
	  ssy2 = ty;
	  C_CODE(ssx2, ssy2, code2);
	}
      }
      if( ! code1 && ! code2) {
	// Add segment to list
	Xsegs[0][X_idx].x1 = Xsegs[1][X_idx].x1;
	Xsegs[0][X_idx].y1 = Xsegs[1][X_idx].y1;
	Xsegs[0][X_idx].x2 = Xsegs[1][X_idx].x2;
	Xsegs[0][X_idx].y2 = Xsegs[1][X_idx].y2;
	Xsegs[1][X_idx].x1 = SCREENX(ssx1);
	Xsegs[1][X_idx].y1 = SCREENY(ssy1);
	Xsegs[1][X_idx].x2 = SCREENX(ssx2);
	Xsegs[1][X_idx].y2 = SCREENY(ssy2);
	++X_idx;
      }
      sx1 = sx2;
      sy1 = sy2;
      // Draw markers if requested and they are in drawing region
      if(markQ && mark_inside) {
	dotX(wPlotWin, Xsegs[1][X_idx - 1].x1, Xsegs[1][X_idx - 1].y1,
	     style, color);
      }
    }  // end while(...li)

    // Handle last marker
    if(markQ) {
      C_CODE(sx1, sy1, mark_inside);
      if(mark_inside == 0) {
	dotX(wPlotWin, Xsegs[1][X_idx - 1].x1, Xsegs[1][X_idx - 1].y1,
	     style, color);
      }
    }

    // Draw segments
    if(plotLinesQ && (X_idx > 0)) {
      ptr = Xsegs[1];
      while(X_idx > devInfo.maxSegs) {
	segX(wPlotWin, devInfo.maxSegs, ptr, lineW, L_VAR, style, color);
	ptr += devInfo.maxSegs;
	X_idx -= devInfo.maxSegs;
      }
      segX(wPlotWin, X_idx, ptr, lineW, L_VAR, style, color);
    }
  }
}


// -------------------------------------------------------------------
void XYPlotWin::textX (Widget win, int x, int y, char *text,
		       int just, int style) {
  XCharStruct bb;
  int rx(0), ry(0), dir;
  int ascent, descent;
  XFontStruct *font;
  int len(strlen(text));
  GC textGC;

  if(style == T_TITLE) {
    font   = titletextFont;
    textGC = titletextGC;
  } else {
    font   = labeltextFont;
    textGC = labeltextGC;
  }

  XTextExtents(font, text, len, &dir, &ascent, &descent, &bb);
  int width = bb.rbearing - bb.lbearing;
  int height = bb.ascent + bb.descent;
  
  switch(just) {
    case T_CENTER:     rx = x - (width / 2); ry = y - (height / 2); break;
    case T_LEFT:       rx = x;               ry = y - (height / 2); break;
    case T_UPPERLEFT:  rx = x;               ry = y;                break;
    case T_TOP:        rx = x - (width / 2); ry = y;                break;
    case T_UPPERRIGHT: rx = x - width;       ry = y;                break;
    case T_RIGHT:      rx = x - width;       ry = y - (height / 2); break;
    case T_LOWERRIGHT: rx = x - width;       ry = y - height;       break;
    case T_BOTTOM:     rx = x - (width / 2); ry = y - height;       break;
    case T_LOWERLEFT:  rx = x;               ry = y - height;       break;
  }
  XDrawString(disp, XtWindow(win), textGC, rx, ry + bb.ascent, text, len);
}


// -------------------------------------------------------------------
#define SEGGC(l_fg, l_style, l_width) \
              gcvals.foreground = l_fg; \
              gcvals.line_style = l_style; \
              gcvals.line_width = l_width; \
              XChangeGC(disp,segGC,GCForeground|GCLineStyle|GCLineWidth, &gcvals);

#define DASH(l_chars, l_len) XSetDashes(disp, segGC, 0, l_chars, l_len);


// -------------------------------------------------------------------
void XYPlotWin::segX(Widget win, int ns, XSegment *segs, int width,
		     int style, int lappr, Pixel color) {  

  XGCValues gcvals;
  if(style == L_AXIS) {
    if(gridStyle.len < 2) {
      SEGGC(gridPix, LineSolid, gridW);
    } else {
      SEGGC(gridPix, LineOnOffDash, gridW);
      DASH(gridStyle.dash_list, gridStyle.len);
    }
  } else {  // Color and line style vary
    if(lappr == 0) {
      SEGGC(color, LineSolid, width);
    } else if(lappr == 16) {
      SEGGC(backgroundPix, LineSolid, width);
    } else {
      SEGGC(color, LineOnOffDash, width);
      DASH(AllAttrs[lappr].lineStyle, AllAttrs[lappr].lineStyleLen);
    }
  }
  XDrawSegments(disp, XtWindow(win), segGC, segs, ns);
}


// -------------------------------------------------------------------
void XYPlotWin::dotX(Widget win, int x, int y, int style, int color) {
  XGCValues gcvals;
  gcvals.foreground = color;
  gcvals.clip_mask = AllAttrs[style].markStyle;
  gcvals.clip_x_origin = (int) (x - (dotW >> 1));
  gcvals.clip_y_origin = (int) (y - (dotW >> 1));
  XChangeGC(disp, dotGC,
            GCForeground | GCClipMask | GCClipXOrigin | GCClipYOrigin,
            &gcvals);
  XFillRectangle(disp, XtWindow(win), dotGC, (int) (x - (dotW >> 1)),
		 (int) (y - (dotW >> 1)), dotW, dotW);
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoClearData(Widget, XtPointer, XtPointer) {
#if (BL_SPACEDIM != 3)
  if(animatingQ) {
    StopAnimation();
  }
#endif
  for(list<XYPlotLegendItem *>::iterator item = legendList.begin();
      item != legendList.end(); ++item)
  {
    XtDestroyWidget((*item)->frame);
    delete (*item)->XYPLIlist;
    delete (*item);
  }
  legendList.clear();

  for(int idx(0); idx != 8; ++idx) {
    lineFormats[idx] = 0x0;
  }
  colorChangeItem = nullptr;
  numItems = 0;
  numDrawnItems = 0;
  zoomedInQ = false;
  SetBoundingBox();

  CBdoRedrawPlot(None, NULL, NULL);
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoExportFileDialog(Widget, XtPointer, XtPointer) {
  if(wExportFileDialog == None) {
    Widget wExportDumpFSBox =
      XmCreateFileSelectionDialog(wXYPlotTopLevel,
				  const_cast<char *>("Choose a file to dump ASCII Data"),
				  NULL, 0);
    XtUnmanageChild(XmFileSelectionBoxGetChild(wExportDumpFSBox,
					       XmDIALOG_HELP_BUTTON));

    AddStaticCallback(wExportDumpFSBox, XmNokCallback,
		      &XYPlotWin::CBdoExportFile, (XtPointer) 1);
    AddStaticCallback(wExportDumpFSBox, XmNcancelCallback,
		      &XYPlotWin::CBdoExportFile, (XtPointer) 0);
    XtManageChild(wExportDumpFSBox);
    wExportFileDialog = XtParent(wExportDumpFSBox);
  } else {
    XtPopup(wExportFileDialog, XtGrabNone);
  }
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoExportFile(Widget, XtPointer client_data, XtPointer data) {
  XmFileSelectionBoxCallbackStruct *cbs = (XmFileSelectionBoxCallbackStruct *) data;
  long which = (long) client_data;
  FILE *fs;

  if(which != 1) {
    XtPopdown(wExportFileDialog);
    return;
  }

  char *filename;
  if(animatingQ ||
     ! XmStringGetLtoR(cbs->value, XmSTRING_DEFAULT_CHARSET, &filename) ||
     ! (*filename))
  {
    XmString label_str;
    Arg args[1];
    label_str = XmStringCreateSimple((char *)
				     (animatingQ ?
				      "Cannot create export file while animating." :
				      "Invalid file name."));
    XtSetArg(args[0], XmNmessageString, label_str);
    Widget wid = XmCreateErrorDialog(wXYPlotTopLevel, const_cast<char *>("error"), args, 1);
    XmStringFree(label_str);
    XtUnmanageChild(XmMessageBoxGetChild(wid, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(wid, XmDIALOG_HELP_BUTTON));    
    XtManageChild(wid);
    return;
  }
  if((fs = fopen(filename, "r")) != NULL) {
    fclose(fs);
    XmString label_str;
    Arg args[1];
    label_str = XmStringCreateSimple(const_cast<char *>("Overwrite existing file?"));
    XtSetArg(args[0], XmNmessageString, label_str);
    Widget wid = XmCreateWarningDialog(wXYPlotTopLevel, filename, args, 1);
    XmStringFree(label_str);
    XtUnmanageChild(XmMessageBoxGetChild(wid, XmDIALOG_HELP_BUTTON));    
    AddStaticCallback(wid, XmNokCallback,
		      &XYPlotWin::CBdoASCIIDump, (XtPointer) filename);
    XtManageChild(wid);
    return;
  }
  CBdoASCIIDump(None, filename, NULL);
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoASCIIDump(Widget, XtPointer client_data, XtPointer /*data*/) {
  char *filename = (char *) client_data;
  FILE *fs = fopen(filename, "w");

  if(fs == NULL) {
    XmString label_str;
    Arg args[1];
    label_str = XmStringCreateSimple(const_cast<char *>("Access denied."));
    XtSetArg(args[0], XmNmessageString, label_str);
    Widget wid = XmCreateErrorDialog(wXYPlotTopLevel, filename, args, 1);
    XmStringFree(label_str);
    XtUnmanageChild(XmMessageBoxGetChild(wid, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(wid, XmDIALOG_HELP_BUTTON));
    XtManageChild(wid);
    XtFree(filename);
    return;
  }
 
  DoASCIIDump(fs, AVGlobals::StripSlashes(filename).c_str());
  fclose(fs);
  XtFree(filename);

  XtPopdown(wExportFileDialog);
}


// -------------------------------------------------------------------
void XYPlotWin::DoASCIIDump(FILE *fs, const char *plotname) {
  if(fs == NULL) {
    cerr << "*** Error in XYPlotWin::DoASCIIDump:  fs == NULL" << endl;
    return;
  }
  PltAppState *pas = pltParent->GetPltAppState();
  char format[Amrvis::LINELENGTH];
  sprintf(format, "%s %s\n", pas->GetFormatString().c_str(),
                             pas->GetFormatString().c_str());
  fprintf(fs, "TitleText: %s\n", plotname);
  fprintf(fs, "YUnitText: %s\n", YUnitText);
  fprintf(fs, "XUnitText: %s\n", XUnitText);
  if( ! plotLinesQ) {
    fprintf(fs, "NoLines: true\n");
  }
  if(boundBoxQ) {
    fprintf(fs, "BoundBox: on\n");
  }
  if(tickQ) {
    fprintf(fs, "Ticks: on\n");
  }
  if(markQ) {
    fprintf(fs, "Markers: on\n");
  }
  if(axisQ) {
    fprintf(fs, "TickAxis: on\n");
  }

  for(list<XYPlotLegendItem *>::iterator item = legendList.begin();
      item != legendList.end(); ++item)
  {
    if((*item)->drawQ == false) {
      continue;
    }
    ::XYPlotDataList *xypdList = (*item)->XYPLIlist;
    fprintf(fs, "\"%s %s Level %d/%d\n",
	    xypdList->DerivedName().c_str(),
	    xypdList->IntersectPoint(xypdList->CurLevel()),
	    xypdList->CurLevel(), xypdList->MaxLevel());
    Vector<double> &xvals = xypdList->XVal(xypdList->CurLevel());
    Vector<double> &yvals = xypdList->YVal(xypdList->CurLevel());
    for(int ii(0); ii < xvals.size(); ++ii) {
      fprintf(fs, format, xvals[ii], yvals[ii]);
    }
    fprintf(fs, "\n");
  }
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoOptions(Widget, XtPointer, XtPointer) {
  if(wOptionsDialog == None) {
    // Build options dialog.
    const char *firstColumnText[] = {
      "Data Markers", "Ticks (vs. grid)", "Axis Lines", "Bounding Box",
      "Plot Lines", "Display Hints"
    };
    const char *secondColumnText[] = {
      "Grid Line Width", "Plot Line Width",
      "X Unit Text", "Y Unit Text",
      "Grid Label Format X", "Grid Label Format Y"
    };
    
    tbool[0] = markQ;
    tbool[1] = tickQ;
    tbool[2] = axisQ;
    tbool[3] = boundBoxQ;
    tbool[4] = plotLinesQ;
    tbool[5] = dispHintsQ;
    tbool[6] = saveDefaultQ;

    wOptionsDialog =
      XtVaCreatePopupShell("optionsdialog", xmDialogShellWidgetClass,
			   wXYPlotTopLevel,
			   XmNtitle, "Plot Options",
			   XmNdeleteResponse, XmDO_NOTHING,
			   NULL);
    AddStaticWMCallback(wOptionsDialog, WM_DELETE_WINDOW, 
			&XYPlotWin::CBdoOptionsOKButton, (XtPointer) 0);
    
    Widget wOptionsForm =
      XtVaCreateManagedWidget("optionsform", xmFormWidgetClass,
			      wOptionsDialog,
			      XmNheight,          290,
			      XmNwidth,           550,
			      NULL);
    Widget wOptionsRC =
      XtVaCreateManagedWidget("optionsrc", xmRowColumnWidgetClass,
			      wOptionsForm,
			      XmNtopAttachment,   XmATTACH_FORM,
			      XmNtopOffset,       10,
			      XmNleftAttachment,  XmATTACH_FORM,
			      XmNrightAttachment, XmATTACH_FORM,
			      XmNpacking,         XmPACK_COLUMN,
			      XmNnumColumns,      6,
			      XmNorientation,     XmHORIZONTAL,
			      NULL);
    
    long ii;
    Widget wid;
    char buffer[32], *str = 0;
    for(ii = 0; ii < 6; ++ii) {
      switch(ii) {
        case 0:
	  sprintf(buffer, "%d", gridW);
	  str = buffer;
	break;

        case 1:
	  sprintf(buffer, "%d", lineW);
	  str = buffer;
	break;

        case 2:
	  str = XUnitText;
	break;

        case 3:
	  str = YUnitText;
	break;

        case 4:
	  str = formatX;
	break;

        case 5:
	  str = formatY;
	break;
      }
      wOptionsWidgets[ii] = wid = 
	XtVaCreateManagedWidget(firstColumnText[ii],
				xmToggleButtonGadgetClass,
				wOptionsRC,
				XmNset,     tbool[ii],
				NULL);
      
      AddStaticCallback(wid, XmNvalueChangedCallback,
			&XYPlotWin::CBdoOptionsToggleButton,
			(XtPointer) ii);
      
      XtVaCreateManagedWidget(secondColumnText[ii], xmLabelGadgetClass,
			      wOptionsRC, NULL);
      wid = XtVaCreateManagedWidget(NULL, xmTextWidgetClass, wOptionsRC,
				XmNeditMode, XmSINGLE_LINE_EDIT,
				XmNvalue, str,
				NULL);
      wOptionsWidgets[7+ii] = wid;
    }
    
    XtManageChild(wOptionsRC);
    
    Widget wOkButton =
      XtVaCreateManagedWidget("  Ok  ", xmPushButtonGadgetClass,
			      wOptionsForm,
			      XmNbottomAttachment, XmATTACH_FORM,
			      XmNbottomOffset,     Amrvis::WOFFSET,
			      XmNleftAttachment,   XmATTACH_FORM,
			      XmNleftOffset,       Amrvis::WOFFSET,
			      NULL);
    AddStaticCallback(wOkButton, XmNactivateCallback,
		      &XYPlotWin::CBdoOptionsOKButton, (XtPointer) 1);
    
    Widget wApplyButton =
      XtVaCreateManagedWidget(" Apply ", xmPushButtonGadgetClass,
			      wOptionsForm,
			      XmNbottomAttachment, XmATTACH_FORM,
			      XmNbottomOffset,     Amrvis::WOFFSET,
			      XmNleftAttachment,   XmATTACH_WIDGET,
			      XmNleftWidget,       wOkButton,
			      XmNleftOffset,       Amrvis::WOFFSET,
			      NULL);
    AddStaticCallback(wApplyButton, XmNactivateCallback,
		      &XYPlotWin::CBdoOptionsOKButton, (XtPointer) 1);
    
    Widget wCancelButton =
      XtVaCreateManagedWidget(" Cancel ", xmPushButtonGadgetClass,
			      wOptionsForm,
			      XmNbottomAttachment, XmATTACH_FORM,
			      XmNbottomOffset,     Amrvis::WOFFSET,
			      XmNleftAttachment,   XmATTACH_WIDGET,
			      XmNleftWidget,       wApplyButton,
			      XmNleftOffset,       Amrvis::WOFFSET,
			      NULL);
    AddStaticCallback(wCancelButton, XmNactivateCallback,
		      &XYPlotWin::CBdoOptionsOKButton, (XtPointer) 0);
    
    wid = XtVaCreateManagedWidget("Save as Default",
			      xmToggleButtonGadgetClass,
			      wOptionsForm,
			      XmNset,     tbool[6],
			      XmNbottomAttachment, XmATTACH_FORM,
			      XmNbottomOffset,     Amrvis::WOFFSET + 4,
			      XmNleftAttachment,   XmATTACH_WIDGET,
			      XmNleftWidget,       wCancelButton,
			      XmNleftOffset,       2 * Amrvis::WOFFSET,
			      NULL);
    wOptionsWidgets[6] = wid;
			      
    AddStaticCallback(wid, XmNvalueChangedCallback,
		      &XYPlotWin::CBdoOptionsToggleButton,
		      (XtPointer) 6);

    XtManageChild(wOptionsForm);
  }
  XtPopup(wOptionsDialog, XtGrabNone);
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoOptionsToggleButton(Widget, XtPointer data, XtPointer) {
  long which = (long) data;
  tbool[which] = 1 - tbool[which];
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoOptionsOKButton(Widget, XtPointer data, XtPointer) {
  long which = (long) data;

  if(which == 1) {
    markQ      = tbool[0];
    tickQ      = tbool[1];
    axisQ      = tbool[2];
    boundBoxQ  = tbool[3];
    plotLinesQ = tbool[4];
    dispHintsQ = tbool[5];
    saveDefaultQ = tbool[6];

    parameters->Set_Parameter("Markers", BOOL, (markQ ? "on" : "off"));
    parameters->Set_Parameter("Ticks", BOOL, (tickQ ? "on" : "off"));
    parameters->Set_Parameter("TickAxis", BOOL, (axisQ ? "on" : "off"));
    parameters->Set_Parameter("BoundBox", BOOL, (boundBoxQ ? "on" : "off"));
    parameters->Set_Parameter("PlotLines", BOOL, (plotLinesQ ? "on" : "off"));
    parameters->Set_Parameter("DisplayHints", BOOL,
			      (dispHintsQ ? "on" : "off"));

    char *input;
    int temp;
    input = XmTextGetString(wOptionsWidgets[7]);
    if(sscanf(input, "%d", &temp) != 0 && temp >= 0 && temp <= 10) {
      parameters->Set_Parameter("GridWidth", INT, input);
      gridW = temp;
    }
    XtFree(input);

    input = XmTextGetString(wOptionsWidgets[8]);
    if(sscanf(input, "%d", &temp) != 0 && temp >= 0 && temp <= 10) {
      parameters->Set_Parameter("LineWidth", INT, input);
      lineW = temp;
    }

    XtFree(input);
    delete [] XUnitText;
    delete [] YUnitText;
    delete [] formatX;
    delete [] formatY;

    input = XmTextGetString(wOptionsWidgets[9]);
    XUnitText = new char[strlen(input) + 1];
    strcpy(XUnitText, input);
    XtFree(input);

    input = XmTextGetString(wOptionsWidgets[10]);
    YUnitText = new char[strlen(input) + 1];
    strcpy(YUnitText, input);
    XtFree(input);

    input = XmTextGetString(wOptionsWidgets[11]);
    formatX = new char[strlen(input) + 1];
    strcpy(formatX, input);
    XtFree(input);

    input = XmTextGetString(wOptionsWidgets[12]);
    formatY = new char[strlen(input) + 1];
    strcpy(formatY, input);
    XtFree(input);

    if(whichType == Amrvis::XDIR) {
      parameters->Set_Parameter("XUnitTextX", STR, XUnitText);
    } else if(whichType == Amrvis::YDIR) {
      parameters->Set_Parameter("XUnitTextY", STR, XUnitText);
    } else {
      parameters->Set_Parameter("XUnitTextZ", STR, XUnitText);
    }

    parameters->Set_Parameter("YUnitText", STR, YUnitText);
    parameters->Set_Parameter("FormatX", STR, formatX);
    parameters->Set_Parameter("FormatY", STR, formatY);

    CBdoRedrawPlot(None, NULL, NULL);
    for(list<XYPlotLegendItem *>::iterator item = legendList.begin();
        item != legendList.end(); ++item)
    {
      XClearWindow(disp, XtWindow((*item)->wid));
      CBdoDrawLegendItem(None, (*item), NULL);
    }

    if(saveDefaultQ) {
      int winX, winY;
      XtVaGetValues(wPlotWin,
		    XmNwidth,   &winX,
		    XmNheight,  &winY,
		    NULL);
      char buf[20], buf2[20];
      sprintf(buf, "%d", winX);
      parameters->Set_Parameter("InitialWindowWidth", INT, buf);
      sprintf(buf, "%d", winY);
      parameters->Set_Parameter("InitialWindowHeight", INT, buf);
      XtVaGetValues(wXYPlotTopLevel,
		    XmNx,       &winX,
		    XmNy,       &winY,
		    NULL);
      sprintf(buf, "%d", winX);
      sprintf(buf2, "%d", winY);
      if(whichType == Amrvis::XDIR) {
	parameters->Set_Parameter("InitialXWindowOffsetX", INT, buf);
	parameters->Set_Parameter("InitialXWindowOffsetY", INT, buf2);
      } else if(whichType == Amrvis::YDIR){
	parameters->Set_Parameter("InitialYWindowOffsetX", INT, buf);
	parameters->Set_Parameter("InitialYWindowOffsetY", INT, buf2);
      } else{
	parameters->Set_Parameter("InitialZWindowOffsetX", INT, buf);
	parameters->Set_Parameter("InitialZWindowOffsetY", INT, buf2);
      }

      parameters->WriteToFile(const_cast<char *>(".XYPlot.Defaults"));
    }
  } else {
    tbool[0] = markQ;
    tbool[1] = tickQ;
    tbool[2] = axisQ;
    tbool[3] = boundBoxQ;
    tbool[4] = plotLinesQ;
    tbool[5] = dispHintsQ;
    tbool[6] = saveDefaultQ;

    for(int ii(0); ii != 7; ++ii) {
      XmToggleButtonSetState(wOptionsWidgets[ii], tbool[ii], false);
    }
    XmTextSetString(wOptionsWidgets[8], XUnitText);
    XmTextSetString(wOptionsWidgets[9], YUnitText);
    XmTextSetString(wOptionsWidgets[10], formatX);
    XmTextSetString(wOptionsWidgets[11], formatY);
  }
  char buffer[20];
  sprintf(buffer, "%d", gridW);
  XmTextSetString(wOptionsWidgets[7], buffer);
  sprintf(buffer, "%d", lineW);
  XmTextSetString(wOptionsWidgets[8], buffer);

  XtDestroyWidget(wOptionsDialog);
  wOptionsDialog = None;
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoSelectDataList(Widget, XtPointer data,
				   XtPointer call_data) {
  XYPlotLegendItem *item = (XYPlotLegendItem *) data;
  XmDrawingAreaCallbackStruct *cbs =
    (XmDrawingAreaCallbackStruct *) call_data;
  
  if(cbs->event->xany.type == ButtonRelease &&
     cbs->event->xbutton.button == 1)
  {
    if(item->drawQ == true) {
      item->drawQ = false;
      --numDrawnItems;
      XtVaSetValues(item->frame,
		    XmNtopShadowColor, backgroundPix,
		    XmNbottomShadowColor, backgroundPix,
		    NULL);
      if( ! animatingQ) {
	lloX =  std::numeric_limits<Real>::max();
	lloY =  std::numeric_limits<Real>::max();
	hhiX =  std::numeric_limits<Real>::lowest();
	hhiY =  std::numeric_limits<Real>::lowest();
        for(list<XYPlotLegendItem *>::iterator liitem = legendList.begin();
            liitem != legendList.end(); ++liitem)
        {
	  if((*liitem)->drawQ == true) {
	    UpdateBoundingBox((*liitem)->XYPLIlist);
	  }
	}
      }
    } else {
      item->drawQ = true;
      ++numDrawnItems;
      XtVaSetValues(item->frame,
		    XmNtopShadowColor, foregroundPix,
		    XmNbottomShadowColor, foregroundPix,
		    NULL);
      if( ! animatingQ) {
        UpdateBoundingBox(item->XYPLIlist);
      }
    }
    if( ! animatingQ) {
      if(zoomedInQ == false) {
        SetBoundingBox();
      }
      CBdoRedrawPlot(None, NULL, NULL);
    }
    return;
  }
  if(animatingQ) {
    return;
  }
  if(cbs->event->xany.type == ButtonPress && cbs->event->xbutton.button != 1) {
    XmMenuPosition(item->menu, &cbs->event->xbutton);
    XtManageChild(item->menu);
    return;
  }
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoRemoveDataList(Widget, XtPointer client_data,
				   XtPointer /*call_data*/)
{
  if(animatingQ) {
    return;
  }
  XYPlotLegendItem *item = (XYPlotLegendItem *) client_data;

  // find the item to remove
  list<XYPlotLegendItem *>::iterator liitem, linextitem, liprevitem, linextitem2;
  for(liitem = legendList.begin(); liitem != legendList.end(); ++liitem) {
    if((*liitem) == item) {
      liprevitem = liitem;
      linextitem = liitem;
      --liprevitem;
      ++linextitem;
      break;
    }
  }
  BL_ASSERT(liitem != legendList.end());

  if(item == colorChangeItem) {
    colorChangeItem = nullptr;
  }
  if(linextitem != legendList.end()) {
    if(item->XYPLIlist->CopiedFrom() == NULL &&
       (*linextitem)->XYPLIlist->CopiedFrom() == item->XYPLIlist)
    {
      (*linextitem)->XYPLIlist->SetCopiedFrom(NULL);
      item->XYPLIlist->SetCopiedFrom((*linextitem)->XYPLIlist);
      linextitem2 = linextitem;
      ++linextitem2;
      for( ;
	  linextitem2 != legendList.end() &&
	  (*linextitem2)->XYPLIlist->CopiedFrom() == item->XYPLIlist;
	  ++linextitem2)
      {
	(*linextitem2)->XYPLIlist->SetCopiedFrom((*linextitem)->XYPLIlist);
      }
    }
  }

  char mask = 0x1 << item->color;
  lineFormats[item->style] &= ~mask;
  --numItems;

  if(item->drawQ == true) {
    --numDrawnItems;
    lloX =  std::numeric_limits<Real>::max();
    lloY =  std::numeric_limits<Real>::max();
    hhiX =  std::numeric_limits<Real>::lowest();
    hhiY =  std::numeric_limits<Real>::lowest();
    for(list<XYPlotLegendItem *>::iterator liptr = legendList.begin();
        liptr != legendList.end(); ++liptr)
    {
      if((*liptr)->drawQ == true) {
        UpdateBoundingBox((*liptr)->XYPLIlist);
      }
    }
    if(zoomedInQ == false) {
      SetBoundingBox();
    }
  }

  XtDestroyWidget(item->frame);
  delete item->XYPLIlist;
  delete item;
  legendList.erase(liitem);
  ReattachLegendFrames();
  CBdoRedrawPlot(None, NULL, NULL);
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoCopyDataList(Widget, XtPointer data, XtPointer) {
  if(animatingQ) {
    return;
  }
  XYPlotLegendItem *item = (XYPlotLegendItem *) data;
  AddDataList(new ::XYPlotDataList(item->XYPLIlist), item);
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoSetListLevel(Widget, XtPointer data, XtPointer) {
  if(animatingQ) {
    return;
  }
  XYMenuCBData *cbd = (XYMenuCBData *) data;
  XYPlotLegendItem *item = cbd->item;

  item->XYPLIlist->SetLevel(cbd->which);
  XClearWindow(disp, XtWindow(item->wid));
  CBdoDrawLegendItem(None, item, NULL);
  if(item->drawQ == true) {
    lloX =  std::numeric_limits<Real>::max();
    lloY =  std::numeric_limits<Real>::max();
    hhiX =  std::numeric_limits<Real>::lowest();
    hhiY =  std::numeric_limits<Real>::lowest();
    for(list<XYPlotLegendItem *>::iterator liitem = legendList.begin();
        liitem != legendList.end(); ++liitem)
    {
      if((*liitem)->drawQ == true) {
        UpdateBoundingBox((*liitem)->XYPLIlist);
      }
    }
    if(zoomedInQ == false) {
      SetBoundingBox();
    }
    CBdoRedrawPlot(None, NULL, NULL);
  }
}


// -------------------------------------------------------------------
void XYPlotWin::SetPalette() {
  Palette *pal = pltParent->GetPalettePtr();
  pal->SetWindowPalette(pltParent->GetPaletteName(), XtWindow(wXYPlotTopLevel));
  pal->SetWindowPalette(pltParent->GetPaletteName(), pWindow);
  char buffer[20];
  const int palOffset(24);
  params param_temp;   // temporary parameter grabbing slot
  for(int idx(0); idx < 8; ++idx) {
    sprintf(buffer, "%d.Color", idx);
    int icTemp(PM_INT(buffer));
    long icTempL(icTemp);
    const Vector<XColor> &cCells = pal->GetColorCells();
    long ccsm1(cCells.size() - 1);
    icTempL += palOffset;
    icTemp = std::max(0L, std::min(icTempL, ccsm1));
    AllAttrs[idx].pixelValue = cCells[icTemp].pixel;
  }

  for(list<XYPlotLegendItem *>::iterator liitem = legendList.begin();
      liitem != legendList.end(); ++liitem)
  {
    pal->SetWindowPalette(pltParent->GetPaletteName(), XtWindow((*liitem)->wid));
  }
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoInitializeListColorChange(Widget /*w*/, XtPointer data, XtPointer)
{
  XYPlotLegendItem *item = (XYPlotLegendItem *) data;
  Widget wPalArea = pltParent->GetPalArea();
  if(pltParent->PaletteCBQ()) {
    XtRemoveCallback(wPalArea, XmNinputCallback,
		     (XtCallbackProc) XYPlotWin::StaticCallback, NULL);
  }
  pltParent->SetPaletteCBQ();
  colorChangeItem = item;
  AddStaticCallback(wPalArea, XmNinputCallback, &XYPlotWin::CBdoSetListColor);
  XtPopup(pltParent->WId(), XtGrabNone);
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoSetListColor(Widget, XtPointer, XtPointer call_data) {

  if(colorChangeItem && ! animatingQ) {
    XmDrawingAreaCallbackStruct *cbs =
      (XmDrawingAreaCallbackStruct *) call_data;
    if(cbs->event->xany.type == ButtonPress) {
      int totalColorSlots = pltParent->GetPalettePtr()->PaletteSize();
      int cy = ((totalColorSlots - 1) - cbs->event->xbutton.y) + 14;
      if(cy >= 0 && cy <= (totalColorSlots - 1)) {
	colorChangeItem->pixel = AllAttrs[colorChangeItem->color].pixelValue =
	        pltParent->GetPalettePtr()->GetColorCells()[cy].pixel;
	XtPopup(wXYPlotTopLevel, XtGrabNone);
	CBdoRedrawPlot(None, NULL, NULL);
	CBdoDrawLegendItem(None, (void *) colorChangeItem, NULL);
      }
      XtRemoveAllCallbacks(pltParent->GetPalArea(), XmNinputCallback);
      pltParent->SetPaletteCBQ(false);
      colorChangeItem = nullptr;
    }
  }
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoSelectAllData(Widget, XtPointer, XtPointer) {
  numDrawnItems = numItems;
  for(list<XYPlotLegendItem *>::iterator ptr = legendList.begin();
      ptr != legendList.end(); ++ptr)
  {
    if((*ptr)->drawQ == false) {
      (*ptr)->drawQ = true;
      XtVaSetValues((*ptr)->frame,
		    XmNtopShadowColor, foregroundPix,
		    XmNbottomShadowColor, foregroundPix,
		    NULL);
      if( ! animatingQ) {
        UpdateBoundingBox((*ptr)->XYPLIlist);
      }
    }
  }
  if( ! animatingQ) {
    if(zoomedInQ == false) {
      SetBoundingBox();
    }
  }
  CBdoRedrawPlot(None, NULL, NULL);
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoDeselectAllData(Widget, XtPointer, XtPointer) {
  numDrawnItems = 0;
  for(list<XYPlotLegendItem *>::iterator ptr = legendList.begin();
      ptr != legendList.end(); ++ptr)
  {
    if((*ptr)->drawQ == true) {
      (*ptr)->drawQ = false;
      XtVaSetValues((*ptr)->frame,
		    XmNtopShadowColor, backgroundPix,
		    XmNbottomShadowColor, backgroundPix,
		    NULL);
    }
  }
  if( ! animatingQ) {
    lloX =  std::numeric_limits<Real>::max();
    lloY =  std::numeric_limits<Real>::max();
    hhiX =  std::numeric_limits<Real>::lowest();
    hhiY =  std::numeric_limits<Real>::lowest();
  }
  CBdoRedrawPlot(None, NULL, NULL);

}


// -------------------------------------------------------------------
void XYPlotWin::drawHint() {
  if(devInfo.areaW < 50 * devInfo.axisW) {
    return;
  }
  XClearArea(disp, pWindow, 20 * devInfo.axisW,
	     0, 0, (5 * devInfo.axisH) / 2, false);
  switch(iCurrHint) {
    case 0:
      textX(wPlotWin, devInfo.areaW - 5, devInfo.bdrPad,
	    const_cast<char *>("Left click to zoom in."),
	    T_UPPERRIGHT, T_AXIS);

      if(zoomedInQ == true) {
        textX(wPlotWin, devInfo.areaW - 5, devInfo.bdrPad + devInfo.axisH,
	      const_cast<char *>("Right click to zoom out."),
	      T_UPPERRIGHT, T_AXIS);
      }
    break;
    case 1:
      textX(wPlotWin, devInfo.areaW - 5, devInfo.bdrPad,
	    const_cast<char *>("Left click on legend items to toggle draw."),
	    T_UPPERRIGHT, T_AXIS);
      textX(wPlotWin, devInfo.areaW - 5, devInfo.bdrPad + devInfo.axisH,
	    const_cast<char *>("Right click to select options."),
	    T_UPPERRIGHT, T_AXIS);

    break;
  }
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoDrawLocation(Widget, XtPointer, XtPointer data) {
  XEvent *event = (XEvent *) data;

  if(event->type == LeaveNotify) {
    if(dispHintsQ) {
      iCurrHint = 1;
      drawHint();
    }
    XClearArea(disp, pWindow, 0, iXLocWinY, iXLocWinX, 0, false);
  } else {
    if(dispHintsQ && iCurrHint != 0) {
      iCurrHint = 0;
      drawHint();
    }
    Window whichRoot, whichChild;
    int rootX, rootY, newX, newY;
    unsigned int inputMask;
    
    XQueryPointer(disp, pWindow, &whichRoot, &whichChild,
		  &rootX, &rootY, &newX, &newY, &inputMask);
    if(newX <= iXOppX && newX >= iXOrgX && newY <= iXOppY && newY >= iXOrgY) {
      char locText[40];
      sprintf(locText, "(%.4E, %.4E)", TRANX(newX), TRANY(newY));
      XClearArea(disp, pWindow, 0, iXLocWinY, iXLocWinX, 0, false);
      textX(wPlotWin, devInfo.bdrPad, devInfo.areaH - devInfo.bdrPad,
	    locText, T_LOWERLEFT, T_AXIS);
    } else {
      XClearArea(disp, pWindow, 0, iXLocWinY, iXLocWinX, 0, false);
    }
  }
} 


// -------------------------------------------------------------------
void XYPlotWin::CBdoRubberBanding(Widget, XtPointer, XtPointer call_data) {
  int rStartX, oldX, anchorX, newX, rWidth, rootX;
  int rStartY, oldY, anchorY, newY, rHeight, rootY;
  double lowX, lowY, highX, highY;
  XEvent nextEvent;
  int servingButton;
  bool rectDrawn;
  Window whichRoot, whichChild;
  char locText[30];
  unsigned int inputMask;
  // find which view from the widget w
  XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct *) call_data;

  Palette *pal = pltParent->GetPalettePtr();
  XSetForeground(disp, rbGC, pal->makePixel(120));

  if(cbs->event->xany.type == ButtonPress) {
    servingButton = cbs->event->xbutton.button;
    if(servingButton != 1) {
      if(zoomedInQ == true) {
	zoomedInQ = false;
	SetBoundingBox();
	CBdoRedrawPlot(None, NULL, NULL);
      }
      return;
    }

    oldX = newX = anchorX = std::max(0, cbs->event->xbutton.x);
    oldY = newY = anchorY = std::max(0, cbs->event->xbutton.y);
    rectDrawn = false;
    // grab server and draw box(es)
    XChangeActivePointerGrab(disp,
			     ButtonMotionMask | OwnerGrabButtonMask |
			     ButtonPressMask | ButtonReleaseMask |
			     PointerMotionMask | PointerMotionHintMask,
			     zoomCursor, CurrentTime);
    AVXGrab avxGrab(disp);
    lowX = TRANX(anchorX);
    lowY = TRANY(anchorY);
    
    if(devInfo.areaW >= 2 * iXLocWinX + 15 * devInfo.axisW) {
      sprintf(locText, "(%.4E, %.4E)", lowX, lowY);
      XClearArea(disp, pWindow, iXLocWinX, iXLocWinY, iXLocWinX, 0, false);
      textX(wPlotWin, iXLocWinX + devInfo.bdrPad,
	    devInfo.areaH - devInfo.bdrPad,
	    locText, T_LOWERLEFT, T_AXIS);
    }
    
    while(true) {
      if(devInfo.areaW >= iXLocWinX + 15 * devInfo.axisW) {
	sprintf(locText, "(%.4E, %.4E)", TRANX(newX), TRANX(newY));
	XClearArea(disp, pWindow, 0, iXLocWinY, iXLocWinX, 0, false);
	textX(wPlotWin, devInfo.bdrPad, devInfo.areaH - devInfo.bdrPad,
	      locText, T_LOWERLEFT, T_AXIS);
      }
      
      XNextEvent(disp, &nextEvent);
      
      switch(nextEvent.type) {
	
      case MotionNotify:
	
	if(rectDrawn) {   // undraw the old rectangle(s)
	  rWidth  = std::abs(oldX-anchorX);
	  rHeight = std::abs(oldY-anchorY);
	  rStartX = (anchorX < oldX) ? anchorX : oldX;
	  rStartY = (anchorY < oldY) ? anchorY : oldY;
	  XDrawRectangle(disp, pWindow, rbGC, rStartX, rStartY,
			 rWidth, rHeight);
	}
	
	// get rid of those pesky extra MotionNotify events
	while(XCheckTypedEvent(disp, MotionNotify, &nextEvent)) {
	  ;  // do nothing
	}
	
	XQueryPointer(disp, pWindow, &whichRoot, &whichChild,
		      &rootX, &rootY, &newX, &newY, &inputMask);

	rWidth  = std::abs(newX-anchorX);   // draw the new rectangle
	rHeight = std::abs(newY-anchorY);
	rStartX = (anchorX < newX) ? anchorX : newX;
	rStartY = (anchorY < newY) ? anchorY : newY;
	XDrawRectangle(disp, pWindow, rbGC, rStartX, rStartY,
		       rWidth, rHeight);
	rectDrawn = true;
	
	oldX = newX;
	oldY = newY;
	
	break;
	
      case ButtonRelease:
	avxGrab.ExplicitUngrab();  // giveitawaynow
	
	// undraw rectangle
	rWidth  = std::abs(oldX-anchorX);
	rHeight = std::abs(oldY-anchorY);
	rStartX = (anchorX < oldX) ? anchorX : oldX;
	rStartY = (anchorY < oldY) ? anchorY : oldY;
	XDrawRectangle(disp, pWindow, rbGC, rStartX, rStartY,
		       rWidth, rHeight);
	
	highX = TRANX(newX);
	highY = TRANY(newY);
	if((anchorX - newX != 0) && (anchorY - newY != 0)) {
	  // Figure out relative bounding box
	  if(lowX > highX) {
	    double temp;	  
	    temp = highX;
	    highX = lowX;
	    lowX = temp;
	  }
	  if(lowY > highY) {
	    double temp;
	    temp = highY;
	    highY = lowY;
	    lowY = temp;
	  }
	  zoomedInQ = true;
	  SetBoundingBox(lowX, lowY, highX, highY);
	  CBdoRedrawPlot(None, NULL, NULL);
	} else if((anchorX - newX == 0) && (anchorY - newY == 0)) {
	  if(newX >= iXOrgX && newX <= iXOppX && newY >= iXOrgY && newY <= iXOppY) {
	    highX = (loX + hiX) / 2 - highX;
	    highY = (loY + hiY) / 2 - highY;
	    loX -= highX;
	    hiX -= highX;
	    loY -= highY;
	    hiY -= highY;
	    zoomedInQ = true;
	    CBdoRedrawPlot(None, NULL, NULL);
	  }
	}
	if(devInfo.areaW > iXLocWinX + 15 * devInfo.axisW) {
	  if(devInfo.areaW > 2*iXLocWinX + 15 * devInfo.axisW) {
	    XClearArea(disp, pWindow, 0, iXLocWinY, 2*iXLocWinX, 0, false);
	  } else {
	    XClearArea(disp, pWindow, 0, iXLocWinY, iXLocWinX, 0, false);
	  }
	  sprintf(locText, "(%.4E, %.4E)", TRANX(newX), TRANY(newY));	    
	  textX(wPlotWin, devInfo.bdrPad, devInfo.areaH - devInfo.bdrPad,
		locText, T_LOWERLEFT, T_AXIS);
	}
	return;
	
      default:
	;  // do nothing
	break;
      }  // end switch
    }  // end while(true)
  }
}  // end CBdoRubberBanding


// -------------------------------------------------------------------
void XYPlotWin::CBdoDrawLegendItem(Widget, XtPointer data, XtPointer) {
  XYPlotLegendItem *item = (XYPlotLegendItem *) data;
  ::XYPlotDataList *dataList = item->XYPLIlist;
  XSegment legLine;
  char legendText[1024];

#if (BL_SPACEDIM == 3)
  sprintf(legendText, "%d/%d %s", dataList->CurLevel(), dataList->MaxLevel(),
	  dataList->DerivedName().c_str());
  textX(item->wid, 5, 10, legendText, T_UPPERLEFT, T_AXIS);
  textX(item->wid, 5, 10 + devInfo.axisH,
	dataList->IntersectPoint(dataList->CurLevel()), T_UPPERLEFT, T_AXIS);
#else
  sprintf(legendText, "%d/%d %s %s", dataList->CurLevel(), dataList->MaxLevel(),
	  dataList->DerivedName().c_str(),
	  dataList->IntersectPoint(dataList->CurLevel()));
  textX(item->wid, 5, 10, legendText, T_UPPERLEFT, T_AXIS);
#endif
  
  legLine.x1 = 5;
  legLine.x2 = 85;
  legLine.y1 = legLine.y2 = 5;
  if(plotLinesQ) {
    segX(item->wid, 1, &legLine, lineW, L_VAR, item->style, item->pixel);
  }

  if(markQ) {
    dotX(item->wid, 45, 5, item->style, item->pixel);
  }

}


// -------------------------------------------------------------------
void XYPlotWin::AddStaticCallback(Widget w, String cbtype, memberXYCB cbf,
				  void *data)
{
  XYCBData *cbs = new XYCBData(this, data, cbf);
  int nSize(xycbdPtrs.size());
  xycbdPtrs.resize(nSize + 1);
  xycbdPtrs[nSize] = cbs;

  XtAddCallback(w, cbtype, (XtCallbackProc) &XYPlotWin::StaticCallback,
		(XtPointer) cbs);

}
 

// -------------------------------------------------------------------
void XYPlotWin::AddStaticWMCallback(Widget w, Atom cbtype, memberXYCB cbf,
				     void *data)
{
  XYCBData *cbs = new XYCBData(this, data, cbf);
  int nSize(xycbdPtrs.size());
  xycbdPtrs.resize(nSize + 1);
  xycbdPtrs[nSize] = cbs;

  XmAddWMProtocolCallback(w, cbtype, (XtCallbackProc) &XYPlotWin::StaticCallback,
			  (XtPointer) cbs);

}
 

// -------------------------------------------------------------------
void XYPlotWin::AddStaticEventHandler(Widget w, EventMask mask,
				      memberXYCB cbf, void *data)
{
  XYCBData *cbs = new XYCBData(this, data, cbf);
  int nSize(xycbdPtrs.size());
  xycbdPtrs.resize(nSize + 1);
  xycbdPtrs[nSize] = cbs;

  XtAddEventHandler(w, mask, false, (XtEventHandler) &XYPlotWin::StaticEvent,
		    (XtPointer) cbs);
}


// -------------------------------------------------------------------
void XYPlotWin::StaticCallback(Widget w, XtPointer client_data,
				  XtPointer call_data)
{
  XYCBData *cbs = (XYCBData *) client_data;
  XYPlotWin *obj = cbs->instance;
  bool badPtr(false);
  if(cbs == nullptr) {
    amrex::Print() << "**** XYPlotWin::StaticCallback:  cbs ptr == nullptr" << std::endl;
    badPtr = true;
  }
  if(obj == nullptr) {
    amrex::Print() << "**** XYPlotWin::StaticCallback:  obj ptr == nullptr" << std::endl;
    badPtr = true;
  }
  if(call_data == nullptr) {
    amrex::Print() << "**** XYPlotWin::StaticCallback:  call_data == nullptr" << std::endl;
    badPtr = true;
  }
  if(badPtr) {
    return;
  }
  (obj->*(cbs->cbFunc))(w, (XtPointer) cbs->data, call_data);
}


// -------------------------------------------------------------------
void XYPlotWin::StaticEvent(Widget w, XtPointer client_data,
			    XEvent *event, char *)
{
  XYCBData *cbs = (XYCBData *) client_data;
  XYPlotWin *obj = cbs->instance;
  (obj->*(cbs->cbFunc))(w, (XtPointer) cbs->data, (XtPointer) event);
}


// -------------------------------------------------------------------
void CBcloseXYPlotWin(Widget /*w*/, XtPointer client_data, XtPointer) {
  XYPlotWin *win = (XYPlotWin *) client_data;
  delete win;
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
