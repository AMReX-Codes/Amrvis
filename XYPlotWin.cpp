// ---------------------------------------------------------------
// XYPlotWin.cpp
// ---------------------------------------------------------------

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

#include "XYPlotWin.H"
#include "PltApp.H"
#include "PltAppState.H"
#include "GraphicsAttributes.H"

#ifdef BL_USE_NEW_HFILES
#include <iostream>
#include <iomanip>
using std::setw;
#else
#include <iostream.h>
#include <iomanip.h>
#endif

#define STRDUP(xx) (strcpy(new char[strlen(xx)+1], (xx)))
#define MARK (fprintf(stderr, "Mark at file %s, line %d.\n", __FILE__, __LINE__))

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
  {0x00, 0x3e, 0x1c, 0x08, 0x1c, 0x3e, 0x00, 0x00}};

static param_style param_null_style = {STYLE, 0, (char *) 0};

#ifdef BL_USE_NEW_HFILES
#include <cctype>
#include <strstream>
using std::ostrstream;
using std::ends;
using std::endl;
#else
#include <ctype.h>
#include <strstream.h>
#endif

// Some macros for obtaining parameters.
#define PM_INT(name) ((parameters->Get_Parameter(name, &param_temp)) ? \
   param_temp.intv.value : (BL_ASSERT(0), (int) 0))
#define PM_STR(name) ((parameters->Get_Parameter(name, &param_temp)) ? \
   param_temp.strv.value : (BL_ASSERT(0), (char *) 0))
#define PM_COLOR(name) ((parameters->Get_Parameter(name, &param_temp)) ? \
   param_temp.pixv.value : (BL_ASSERT(0), param_null_color))
#define PM_PIXEL(name) ((parameters->Get_Parameter(name, &param_temp)) ? \
   param_temp.pixv.value.pixel : (BL_ASSERT(0), (Pixel) 0))
#define PM_FONT(name) ((parameters->Get_Parameter(name, &param_temp)) ? \
   param_temp.fontv.value : (BL_ASSERT(0), (XFontStruct *) 0))
#define PM_STYLE(name) ((parameters->Get_Parameter(name, &param_temp)) ? \
   param_temp.stylev : (BL_ASSERT(0), param_null_style))
#define PM_BOOL(name) ((parameters->Get_Parameter(name, &param_temp)) ? \
   param_temp.boolv.value : (BL_ASSERT(0), 0))
#define PM_DBL(name) ((parameters->Get_Parameter(name, &param_temp)) ? \
   param_temp.dblv.value : (BL_ASSERT(0), 0.0))


// -------------------------------------------------------------------
XYPlotWin::~XYPlotWin() {
  if(pltParent->GetXYPlotWin(whichType) == this) {
    pltParent->DetachXYPlotWin(whichType);
  }
  CBdoClearData(None, NULL, NULL);
  if(wOptionsDialog != None) XtDestroyWidget(wOptionsDialog);
  if(wExportFileDialog != None) XtDestroyWidget(wExportFileDialog);
  XtDestroyWidget(wXYPlotTopLevel);
  delete GAptr;
  delete Xsegs[0];
  delete Xsegs[1];
  delete XUnitText;
  delete YUnitText;
  delete formatY;
  delete formatX;
}


// -------------------------------------------------------------------
XYPlotWin::XYPlotWin(char *title, XtAppContext app, Widget w, PltApp *parent,
		     int type, int curr_frame)
  : appContext(app),
    wTopLevel(w),
    pltParent(parent),
    whichType(type),
    currFrame(curr_frame)
{

  int idx;
  char buffer[BUFSIZE];
  pltTitle = STRDUP(title);
  params param_temp;   // temporary parameter grabbing slot

  // Store some local stuff from the parent.
  parameters = pltParent->GetXYPlotParameters();

  wExportFileDialog = wOptionsDialog = None;

  // Standard flags.
  zoomedInQ = 0;
  saveDefaultQ = 0;
  animatingQ = 0;
#if (BL_SPACEDIM == 2)
  currFrame = 0;
#endif

  // Create empty dataset list.
  legendHead = legendTail = NULL;
  numDrawnItems = 0;
  numItems = 0;
  setBoundingBox();

  currHint = 1;
  
  WM_DELETE_WINDOW = XmInternAtom(XtDisplay(wTopLevel),
				  "WM_DELETE_WINDOW", false);

  // --------------------------------------------------------  main window
  int winOffsetX, winOffsetY;
  int winWidth = PM_INT("InitialWindowWidth");
  int winHeight = PM_INT("InitialWindowHeight");

  if(whichType == XDIR) {
    winOffsetX = PM_INT("InitialXWindowOffsetX");
    winOffsetY = PM_INT("InitialXWindowOffsetY");
  } else if(whichType == YDIR) {
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
  Widget wControlArea, wScrollArea, wLegendArea,
    wExportButton, wOptionsButton, wCloseButton,
    wAllButton, wNoneButton, wClearButton;
  XmString label_str1, label_str2, label_str3;

  wControlArea =
    XtVaCreateManagedWidget("controlArea", xmFormWidgetClass,
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

  label_str1 = XmStringCreateSimple("Export");
  label_str2 = XmStringCreateSimple("Options");
  label_str3 = XmStringCreateSimple("Close");
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

  label_str1 = XmStringCreateSimple("All");
  label_str2 = XmStringCreateSimple("None");
  label_str3 = XmStringCreateSimple("Clear");
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
  GAptr = new GraphicsAttributes(wXYPlotTopLevel);
  disp = GAptr->PDisplay();
  vis  = GAptr->PVisual();
  if(vis != XDefaultVisual(disp, GAptr->PScreenNumber())) {
    XtVaSetValues(wXYPlotTopLevel, XmNvisual, vis, XmNdepth, 8, NULL);
  }
  cursor = XCreateFontCursor(disp, XC_left_ptr);
  zoomCursor = XCreateFontCursor(disp, XC_sizing);
  XtVaGetValues(wPlotWin,
		XmNforeground, &foregroundPix,
		XmNbackground, &backgroundPix,
		NULL);
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
  rbGC  = XCreateGC(disp, GAptr->PRoot(), GCFunction, &gcvals);
  segGC = XCreateGC(disp, GAptr->PRoot(), 0, NULL);
  dotGC = XCreateGC(disp, GAptr->PRoot(), 0, NULL);

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

  if(whichType == XDIR) {
    str = PM_STR("XUnitTextX");
  } else if(whichType == YDIR) {
    str = PM_STR("XUnitTextY");
  } else {
    str = PM_STR("XUnitTextZ");
  }
  XUnitText = STRDUP(str);

  str       = PM_STR("YUnitText");
  YUnitText = STRDUP(str);  
  str       = PM_STR("FormatX");
  formatX   = STRDUP(str);
  str       = PM_STR("FormatY");
  formatY   = STRDUP(str);
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
}



#if (BL_SPACEDIM == 2)

// -------------------------------------------------------------------
void XYPlotWin::InitializeAnimation(int curr_frame, int num_frames) {
  if(animatingQ) {
    return;
  }
  animatingQ = true;
  currFrame = curr_frame;
  numFrames = num_frames;
  for(XYPlotLegendItem *ptr = legendHead; ptr; ptr = ptr->next) {
    if(ptr->list->copied_from != NULL) {
      XYPlotDataList *tempList = ptr->list;
      ptr->list = pltParent->CreateLinePlot(ZPLANE, whichType,
					    ptr->list->maxLevel,
					    ptr->list->gridline,
					    &ptr->list->derived);
      delete tempList;
    }
    ptr->anim_lists = new Array<XYPlotDataList *>(numFrames);
    ptr->ready_list = new Array<char>(numFrames, 0);
  }
  pltParent->GetPltAppState()->GetMinMax(lloY, hhiY);
  if( ! zoomedInQ) {
    setBoundingBox();
    CBdoRedrawPlot(None, NULL, NULL);
  }
}


// -------------------------------------------------------------------
void XYPlotWin::UpdateFrame(int frame) {
  XYPlotDataList *tempList;
  int num_lists_changed(0);
  if( ! animatingQ && ! zoomedInQ) {
    lloY =  DBL_MAX;
    hhiY = -DBL_MAX;
  }
  for(XYPlotLegendItem *ptr = legendHead; ptr; ptr = ptr->next) {
    if(ptr->drawQ) {
      ++num_lists_changed;
    }
    if(animatingQ) {
      (*ptr->anim_lists)[currFrame] = ptr->list;
      (*ptr->ready_list)[currFrame] = 1;
      if((*ptr->ready_list)[frame]) {
	ptr->list = (*ptr->anim_lists)[frame];
	continue;
      }
    } else {
      tempList = ptr->list;
    }
    
    int level(ptr->list->curLevel);
    ptr->list = pltParent->CreateLinePlot(ZPLANE, whichType,
				          ptr->list->maxLevel,
				          ptr->list->gridline,
				          &ptr->list->derived);
    ptr->list->SetLevel(level);
    ptr->list->UpdateStats();
    if(ptr->list->numPoints > numXsegs) {
      numXsegs = ptr->list->numPoints + 5;
      delete Xsegs[0]; Xsegs[0] = new XSegment[numXsegs];
      delete Xsegs[1]; Xsegs[1] = new XSegment[numXsegs];
    }
    if( ! animatingQ) {
      delete tempList;
      if( ! zoomedInQ && ptr->drawQ) {
        updateBoundingBox(ptr->list);
      }
    }
  }
  currFrame = frame;
  if(num_lists_changed == 0) {
    return;
  }
  if(animatingQ) {
    clearData();
    drawData();
  } else {
    if( ! zoomedInQ) {
      setBoundingBox();
    }
    CBdoRedrawPlot(None, NULL, NULL);
  }
}


// -------------------------------------------------------------------
void XYPlotWin::StopAnimation(void) {
  if(!animatingQ) return;
  animatingQ = false;
  for(XYPlotLegendItem *ptr = legendHead; ptr; ptr = ptr->next) {
    for(int ii = 0; ii != numFrames; ++ii) {
      if((*ptr->ready_list)[ii] && (*ptr->anim_lists)[ii] != ptr->list) {
	delete (*ptr->anim_lists)[ii];
      }
    }
    delete ptr->ready_list;
    delete ptr->anim_lists;
  }
  lloY =  DBL_MAX;
  hhiY = -DBL_MAX;
  for(XYPlotLegendItem *ptr = legendHead; ptr; ptr = ptr->next) {
    if(ptr->drawQ) {
      updateBoundingBox(ptr->list);
    }
  }
  if( ! zoomedInQ) {
    setBoundingBox();
    CBdoRedrawPlot(None, NULL, NULL);
  }
}

#endif



#define TRANX(xval) (((double) ((xval) - XOrgX)) * XUnitsPerPixel + UsrOrgX)
#define TRANY(yval) (UsrOppY - (((double) ((yval) - XOrgY)) * YUnitsPerPixel))
#define SCREENX(userX) \
    (((int) (((userX) - UsrOrgX)/XUnitsPerPixel + 0.5)) + XOrgX)
#define SCREENY(userY) \
    (XOppY - ((int) (((userY) - UsrOrgY)/YUnitsPerPixel + 0.5)))


// -------------------------------------------------------------------
void XYPlotWin::setBoundingBox (double lowX, double lowY,
				double highX, double highY) {

  double pad;
  if(highX > lowX) {
    loX = lowX;
    hiX = highX;
  } else {
    if(numDrawnItems == 0) {
      loX = loY = -1.0;
      hiX = hiY = 1.0;
      lloX = lloY = DBL_MAX;
      hhiX = hhiY = -DBL_MAX;
      return;
    }
    loX = lloX;
    hiX = hhiX;
  }
  if(highY > lowY) {
    loY = lowY;
    hiY = highY;
  } else {
    loY = lloY;
    hiY = hhiY;
  }
  
  // Increase the padding for aesthetics
  if(hiX - loX == 0.0) {
    pad = Max(0.5, fabs(hiX / 2.0));
    hiX += pad;
    loX -= pad;
  }
  if(hiY - loY == 0.0) {
    pad = Max(0.5, fabs(hiY / 2.0));
    hiY += pad;
    loY -= pad;
  }

  if( ! zoomedInQ) {
    // Add 10% padding to bounding box (div by 20 yeilds 5%)
    pad = (hiX - loX) / 20.0;
    loX -= pad;
    hiX += pad;
    pad = (hiY - loY) / 20.0;
    loY -= pad;
    hiY += pad;
  }
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoRedrawPlot(Widget w, XtPointer, XtPointer) {
  XClearWindow(disp, pWindow);
  CBdoDrawPlot(None, NULL, NULL);
}


// -------------------------------------------------------------------
void XYPlotWin::CalculateBox(void) {
  XWindowAttributes win_attr;
  XGetWindowAttributes(disp, pWindow, &win_attr);

  devInfo.areaW = win_attr.width;
  devInfo.areaH = win_attr.height;

  // Figure out the transformation constants.  Draw only if valid.
  // First,  we figure out the origin in the X window.  Above the space we
  // have the title and the Y axis unit label. To the left of the space we
  // have the Y axis grid labels.

  // Here we make an arbitrary label to find out how big an offset we need
  char buff[40];
  sprintf(buff, formatY, -200.0);
  XCharStruct bb;
  int dir, ascent, descent;
  XTextExtents(labeltextFont, buff, strlen(buff), &dir, &ascent, &descent, &bb);

  XOrgX = 2 * devInfo.bdrPad + bb.rbearing - bb.lbearing;
  if(dispHintsQ) {
    XOrgY = devInfo.bdrPad + (5 * devInfo.axisH) / 2;
  } else {
    XOrgY = devInfo.bdrPad + (3 * devInfo.axisH) / 2;
  }
  
  // Now we find the lower right corner.  Below the space we have the X axis
  // grid labels.  To the right of the space we have the X axis unit label
  // and the legend.  We assume the worst case size for the unit label.  
  XOppX = devInfo.areaW - devInfo.bdrPad - devInfo.axisW;
  XOppY = devInfo.areaH - devInfo.bdrPad - (2 * devInfo.axisH);

  XLocWinX = devInfo.bdrPad + (30 * devInfo.axisW);
  XLocWinY = devInfo.areaH - devInfo.bdrPad - devInfo.axisH;
  
  // Is the drawing area too small?
  if((XOrgX >= XOppX) || (XOrgY >= XOppY)) {
    return;
  }
  
  // We now have a bounding box for the drawing region. Figure out the units
  // per pixel using the data set bounding box.
  XUnitsPerPixel = (hiX - loX) / ((double) (XOppX - XOrgX));
  YUnitsPerPixel = (hiY - loY) / ((double) (XOppY - XOrgY));
  
  // Find origin in user coordinate space.  We keep the center of the
  // original bounding box in the same place.
  double bbCenX = (loX + hiX) / 2.0;
  double bbCenY = (loY + hiY) / 2.0;
  double bbHalfWidth = ((double) (XOppX - XOrgX)) / 2.0 * XUnitsPerPixel;
  double bbHalfHeight = ((double) (XOppY - XOrgY)) / 2.0 * YUnitsPerPixel;
  UsrOrgX = bbCenX - bbHalfWidth;
  UsrOrgY = bbCenY - bbHalfHeight;
  UsrOppX = bbCenX + bbHalfWidth;
  UsrOppY = bbCenY + bbHalfHeight;
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
void XYPlotWin::AddDataList(XYPlotDataList *new_list,
			    XYPlotLegendItem *insert_after) {
  if(++numItems > 64) {
    // Too many data lists to assign unique color/style.  Delete.
    PrintMessage("Too many lines in plotter!\n");
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

//j = 254;
//cout << "_here 00:  j = " << j << endl;
//j = 4;

  XYPlotLegendItem *new_item = new XYPlotLegendItem;

  new_item->list = new_list;
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
			    XmNtopAttachment,     XmATTACH_WIDGET,
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
  char buffer[10];
  new_item->menu = XmCreatePopupMenu(new_item->wid, "popup", NULL, 0);
  if(new_list->maxLevel != 0) {
    XmString label_str = XmStringCreateSimple("Level");
    levelmenu = XmCreatePulldownMenu(new_item->menu, "pulldown", NULL, 0);
    XtVaCreateManagedWidget("Level", xmCascadeButtonGadgetClass,
			    new_item->menu,
			    XmNsubMenuId, levelmenu,
			    XmNlabelString, label_str,
			    XmNmnemonic, 'L',
			    NULL);
    XmStringFree(label_str);
    
    for(int ii = 0; ii <= new_list->maxLevel; ++ii) {
      sprintf(buffer, "%d/%d", ii, new_list->maxLevel);
      wid = XtVaCreateManagedWidget(buffer, xmPushButtonGadgetClass,
				    levelmenu, NULL);
      if(ii < 10) {
        XtVaSetValues(wid, XmNmnemonic, ii + '0', NULL);
      }
      AddStaticCallback(wid, XmNactivateCallback, &XYPlotWin::CBdoSetListLevel,
			new XYMenuCBData(new_item, ii));
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

  wid = XtVaCreateManagedWidget("Choose color", xmPushButtonGadgetClass,
				new_item->menu,
				XmNmnemonic, 'o',
				NULL);
  AddStaticCallback(wid, XmNactivateCallback,
		    &XYPlotWin::CBdoInitializeListColorChange, new_item);
  if(insert_after == NULL) {
    new_item->drawQ = 1;          // Default to draw.
    ++numDrawnItems;
    new_list->UpdateStats();      // Find extremes, number of points.
    updateBoundingBox(new_list);
    if( ! zoomedInQ) {
      setBoundingBox();
    }
    if(new_list->numPoints > numXsegs) {
      numXsegs = new_list->numPoints + 5;
      delete Xsegs[0]; Xsegs[0] = new XSegment[numXsegs];
      delete Xsegs[1]; Xsegs[1] = new XSegment[numXsegs];
    }
    new_item->next = NULL;
    if((new_item->prev = legendTail) != NULL) {
      legendTail = legendTail->next = new_item;
      XtVaSetValues(new_item->frame, XmNtopWidget, new_item->prev->frame, NULL);
    } else {
      legendTail = legendHead = new_item;
      XtVaSetValues(new_item->frame, XmNtopWidget, wLegendMenu, NULL);
    }
  } else {
    new_item->drawQ = insert_after->drawQ;
    if(new_item->drawQ) {
      ++numDrawnItems;
    } else {
      XtVaSetValues(new_item->frame,
		       XmNtopShadowColor, backgroundPix,
		       XmNbottomShadowColor, backgroundPix,
		       NULL);
    }

    new_item->prev = insert_after;
    XtVaSetValues(new_item->frame, XmNtopWidget, insert_after->frame, NULL);

    if((new_item->next = insert_after->next) != NULL) {
      new_item->next->prev = new_item;
      XtVaSetValues(new_item->next->frame, XmNtopWidget, new_item->frame, NULL);
    } else {
      legendTail = new_item;
    }
    insert_after->next = new_item;
  }

  AddStaticCallback(new_item->wid, XmNinputCallback,
		    &XYPlotWin::CBdoSelectDataList, new_item);
  AddStaticCallback(new_item->wid, XmNexposeCallback,
		    &XYPlotWin::CBdoDrawLegendItem, new_item);

  XtManageChild(new_item->frame);
  if(new_item->drawQ) {
    CBdoRedrawPlot(None, NULL, NULL);
  }
}


// -------------------------------------------------------------------
void XYPlotWin::updateBoundingBox(XYPlotDataList *list) {
  if(list->startX < lloX) {
    lloX = list->startX;
  }
  if(list->endX > hhiX) {
    hhiX = list->endX;
  }
  if(list->lloY[list->curLevel] < lloY) {
    lloY = list->lloY[list->curLevel];
  }
  if(list->hhiY[list->curLevel] > hhiY) {
    hhiY = list->hhiY[list->curLevel];
  }
}



#define nlog10(x)      (x == 0.0 ? 0.0 : log10(x) + 1e-15)


// -------------------------------------------------------------------
double XYPlotWin::initGrid(double low, double high, double step) {
  
  // Hack fix for graphs of large constant graphs.  Sometimes the
  // step is too small in comparison to the size of the grid itself,
  // and rounding error takes its toll.  We "fix" this by multiplying
  // the step by an arbitrary number > 1 (1.2) when this happens.
  double gridHigh;
  while(1) {
    gridStep = roundUp(step);
    gridHigh = (ceil(high / gridStep) + 1.0) * gridStep;
    if(gridHigh + gridStep != gridHigh) {
      break;
    }
    if(step < DBL_EPSILON) {
      step = DBL_EPSILON;
    }
    step *= 1.2;
  }

  gridBase = (floor(low / gridStep)+1.0) * gridStep;
  return gridBase;
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
      val /= 10.0;
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
      val /= 10.0;
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
  int idx;
  if(expv < 0) {
    for(idx = expv; idx < 0; ++idx) {
      val *= 10.0;
    }
  } else {
    for(idx = expv; idx > 0; --idx) {
      val /= 10.0;
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
   if((xval) < UsrOrgX) rtn = LEFT_CODE; \
   else if((xval) > UsrOppX) rtn = RIGHT_CODE; \
   if((yval) < UsrOrgY) rtn |= BOTTOM_CODE; \
   else if((yval) > UsrOppY) rtn |= TOP_CODE



// -------------------------------------------------------------------
void XYPlotWin::drawGridAndAxis(void) {
  int expX, expY; // Engineering powers
  int Yspot, Xspot;
  char value[10], final[BUFSIZE + 10];
  double Xincr, Yincr, Xstart, Ystart,
    Yindex, Xindex, larger;
  XSegment segs[2];
  
  // Grid display powers are computed by taking the log of the largest
  // numbers and rounding down to the nearest multiple of 3.
  if(fabs(UsrOrgX) > fabs(UsrOppX)) larger = fabs(UsrOrgX);
  else larger = fabs(UsrOppX);
  expX = ((int) floor(nlog10(larger) / 3.0)) * 3;
  if(fabs(UsrOrgY) > fabs(UsrOppY)) larger = fabs(UsrOrgY);
  else larger = fabs(UsrOppY);
  expY = ((int) floor(nlog10(larger) / 3.0)) * 3;
  
  // With the powers computed,  we can draw the axis labels.
  Xspot = devInfo.bdrPad + (2 * devInfo.axisW);
  Yspot = 2 * devInfo.bdrPad;
  if(expY != 0) {
    (void) sprintf(final, "%s x 10^%d", YUnitText, expY);
    textX(wPlotWin, Xspot, Yspot, final, T_LEFT, T_AXIS);
  }
  else textX(wPlotWin, Xspot, Yspot, YUnitText, T_LEFT, T_AXIS);
  
  Xspot = devInfo.areaW - devInfo.bdrPad;
  Yspot = devInfo.areaH - (2*devInfo.bdrPad);
  if(expX != 0) {
    (void) sprintf(final, "%s x 10^%d", XUnitText, expX);
    textX(wPlotWin, Xspot, Yspot, final, T_RIGHT, T_AXIS);
  }
  else textX(wPlotWin, Xspot, Yspot, XUnitText, T_RIGHT, T_AXIS);
  
  // First,  the grid line labels
  Yincr = (devInfo.axisPad + devInfo.axisH) * YUnitsPerPixel;
  Ystart = initGrid(UsrOrgY, UsrOppY, Yincr);
  for(Yindex = Ystart; Yindex < UsrOppY; Yindex += gridStep) {
    Yspot = SCREENY(Yindex);
    // Write the axis label
    writeValue(value, formatY, Yindex, expY);
    textX(wPlotWin, XOrgX - devInfo.bdrPad,
	   Yspot, value, T_RIGHT, T_AXIS);
  }
  
  Xincr = (devInfo.axisPad + (devInfo.axisW * 7)) * XUnitsPerPixel;
  Xstart = initGrid(UsrOrgX, UsrOppX, Xincr);
  
  for(Xindex = Xstart; Xindex < UsrOppX; Xindex += gridStep) {
    Xspot = SCREENX(Xindex);
    // Write the axis label
    writeValue(value, formatX, Xindex, expX);
    textX(wPlotWin, Xspot, devInfo.areaH-devInfo.bdrPad-devInfo.axisH,
	  value, T_BOTTOM, T_AXIS);
  }
  
  // Now,  the grid lines or tick marks
  Yincr = (devInfo.axisPad + devInfo.axisH) * YUnitsPerPixel;
  Ystart = initGrid(UsrOrgY, UsrOppY, Yincr);
  for(Yindex = Ystart; Yindex < UsrOppY; Yindex += gridStep) {
    Yspot = SCREENY(Yindex);
    // Draw the grid line or tick mark
    if(tickQ && !(axisQ && Yindex == Ystart)) {
      segs[0].x1 = XOrgX;
      segs[0].x2 = XOrgX + devInfo.tickLen;
      segs[1].x1 = XOppX - devInfo.tickLen;
      segs[1].x2 = XOppX;
      segs[0].y1 = segs[0].y2 = segs[1].y1 = segs[1].y2 = Yspot;
      segX(wPlotWin, 1, &(segs[1]), gridW, L_AXIS, 0, 0);
    }
    else {
      segs[0].x1 = XOrgX;
      segs[0].x2 = XOppX;
      segs[0].y1 = segs[0].y2 = Yspot;
    }
    segX(wPlotWin, 1, segs, gridW, L_AXIS, 0, 0);
  }
  
  Xincr = (devInfo.axisPad + (devInfo.axisW * 7)) * XUnitsPerPixel;
  Xstart = initGrid(UsrOrgX, UsrOppX, Xincr);
  for(Xindex = Xstart; Xindex < UsrOppX; Xindex += gridStep) {
    Xspot = SCREENX(Xindex);
    // Draw the grid line or tick marks
    if(tickQ && !(axisQ && Xindex == Xstart)) {
      segs[0].x1 = segs[0].x2 = segs[1].x1 = segs[1].x2 = Xspot;
      segs[0].y1 = XOrgY;
      segs[0].y2 = XOrgY + devInfo.tickLen;
      segs[1].y1 = XOppY - devInfo.tickLen;
      segs[1].y2 = XOppY;
    }
    else {
      segs[0].x1 = segs[0].x2 = Xspot;
      segs[0].y1 = XOrgY;
      segs[0].y2 = XOppY;
    }
    segX(wPlotWin, 1, segs, gridW, L_AXIS, 0, 0);
    if(tickQ) segX(wPlotWin, 1, &(segs[1]), gridW, L_AXIS, 0, 0);
  }

  if(boundBoxQ) { // draw bounding box
    XSegment bb[4];
    bb[0].x1 = bb[0].x2 = bb[1].x1 = bb[3].x2 = XOrgX;
    bb[0].y1 = bb[2].y2 = bb[3].y1 = bb[3].y2 = XOrgY;
    bb[1].x2 = bb[2].x1 = bb[2].x2 = bb[3].x1 = XOppX;
    bb[0].y2 = bb[1].y1 = bb[1].y2 = bb[2].y1 = XOppY;
    segX(wPlotWin, 4, bb, gridW, L_AXIS, 0, 0);
  }
}


// -------------------------------------------------------------------
void XYPlotWin::clearData(void) {
  XClearArea(disp, pWindow, XOrgX+1, XOrgY+1,
	     (XOppX - XOrgX - 2),
	     (XOppY - XOrgY - 2), false);
}



// -------------------------------------------------------------------
void XYPlotWin::drawData(void){

  double sx1, sy1, sx2, sy2, ssx1, ssx2, ssy1, ssy2, tx(0.0), ty(0.0);
  int code1, code2, cd, mark_inside;
  unsigned int X_idx;
  XSegment *ptr;
  int style;
  Pixel color;
  XFlush(disp);
  XYPlotLegendItem *item;

  for(item = legendHead; item != NULL ; item = item->next) {
    if( ! item->drawQ) {
      continue;
    }
    XYPlotDataListIterator li(item->list);
    if( ! li) {
      continue;
    }
    X_idx = 0;
    style = item->style;
    color = item->pixel;
    sx1 = li.xval;
    sy1 = li.yval;
//cout << endl << "_here 1:  about to draw segments." << endl;
//cout << "  maxSegs = " << devInfo.maxSegs << endl;
    while(++li) {
      sx2 = li.xval;
      sy2 = li.yval;

      ssx1 = sx1;
      ssx2 = sx2;
      ssy1 = sy1;
      ssy2 = sy2;

//bool bDiag(X_idx >= 79 && X_idx <= 81);
//bool bDiag(false);
//bool bDiagLines(true);
//if(bDiag) {
//if(bDiagLines) {
  //cout << "[" << X_idx << "]:  sx1 sx2 = " << sx1 << "  " << sx2 << endl;
//}
      // Put segment in (ssx1,ssy1) (ssx2,ssy2), clip to window boundary
      C_CODE(ssx1, ssy1, code1);
      C_CODE(ssx2, ssy2, code2);
//if(bDiag) { cout << "      code1 code2 = " << code1 << "  " << code2 << endl; }
      mark_inside = (code1 == 0);
//if(bDiag) { cout << "      mark_inside = " << mark_inside << endl; }
      while(code1 || code2) {
	if(code1 & code2) {
	  break;
	}
	cd = (code1 ? code1 : code2);
	if(cd & LEFT_CODE) {	     // Crosses left edge
//if(bDiag) { cout << "      _here 2.LEFT" << endl; }
	  ty = ssy1 + (ssy2 - ssy1) * (UsrOrgX - ssx1) / (ssx2 - ssx1);
	  tx = UsrOrgX;
	} else if(cd & RIGHT_CODE) {  // Crosses right edge
//if(bDiag) { cout << "      _here 2.RIGHT" << endl; }
	  ty = ssy1 + (ssy2 - ssy1) * (UsrOppX - ssx1) / (ssx2 - ssx1);
	  tx = UsrOppX;
	} else if(cd & BOTTOM_CODE) { // Crosses bottom edge
//if(bDiag) { cout << "      _here 2.BOTTOM" << endl; }
	  tx = ssx1 + (ssx2 - ssx1) * (UsrOrgY - ssy1) / (ssy2 - ssy1);
	  ty = UsrOrgY;
	} else if(cd & TOP_CODE) {    // Crosses top edge
//if(bDiag) { cout << "      _here 2.TOP" << endl; }
	  tx = ssx1 + (ssx2 - ssx1) * (UsrOppY - ssy1) / (ssy2 - ssy1);
	  ty = UsrOppY;
	}
	if(cd == code1) {
//if(bDiag) { cout << "      _here 2.code1" << endl; }
	  ssx1 = tx;
	  ssy1 = ty;
	  C_CODE(ssx1, ssy1, code1);
	} else {
//if(bDiag) { cout << "      _here 2.code2" << endl; }
	  ssx2 = tx;
	  ssy2 = ty;
	  C_CODE(ssx2, ssy2, code2);
	}
      }
//if(bDiag) { cout << "      code1 code2 = " << code1 << "  " << code2 << endl; }
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
/*
short xx1, xx2, yy1, yy2;
xx1 =  Xsegs[1][X_idx].x1;
xx2 =  Xsegs[1][X_idx].x2;
yy1 =  Xsegs[1][X_idx].y1;
yy2 =  Xsegs[1][X_idx].y2;
if((Xsegs[0][X_idx].x1 + Xsegs[0][X_idx].y1 +
    Xsegs[0][X_idx].x2 + Xsegs[0][X_idx].y2) != 0)
{
  if(xx1 > xx2) {
    cout << "[" << setw(3) << X_idx << "]" << ":  x1 y1 x2 y2 = " << setw(3)
         << xx1 << "  " << setw(3) << yy1 << "  " << setw(3) << xx2
         << "  " << setw(3) << yy2 << endl;
  }
}
*/
	++X_idx;
      }
      sx1 = sx2;
      sy1 = sy2;
      // Draw markers if requested and they are in drawing region
      if(markQ && mark_inside) {
	dotX(wPlotWin, Xsegs[1][X_idx - 1].x1, Xsegs[1][X_idx - 1].y1,
	     style, color);
      }
    }

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
      //if(X_idx <= 91) {
      segX(wPlotWin, X_idx, ptr, lineW, L_VAR, style, color);
      //}
    }
  }
}


// -------------------------------------------------------------------
void XYPlotWin::textX (Widget win, int x, int y, char *text,
		       int just, int style) {
  XCharStruct bb;
  int rx = 0, ry = 0, dir;
  int ascent, descent;
  XFontStruct *font;
  int len = strlen(text);
  GC textGC;

  if(style == T_TITLE){ font = titletextFont; textGC = titletextGC; }
  else {                font = labeltextFont; textGC = labeltextGC; }

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
#if (BL_SPACEDIM == 2)
  if(animatingQ) StopAnimation();
#endif
  XYPlotLegendItem *next;
  for(XYPlotLegendItem *ptr = legendHead; ptr != NULL; ptr = next) {
    next = ptr->next;
    XtDestroyWidget(ptr->frame);
    delete ptr->list;
    delete ptr;
  }

  for(int idx = 0; idx != 8; ++idx) {
    lineFormats[idx] = 0x0;
  }
  legendHead = legendTail = colorChangeItem = NULL;
  numItems = numDrawnItems = 0;
  zoomedInQ = 0;
  setBoundingBox();

  CBdoRedrawPlot(None, NULL, NULL);

}


// -------------------------------------------------------------------
void XYPlotWin::CBdoExportFileDialog(Widget, XtPointer, XtPointer) {
  if(wExportFileDialog == None) {
    Widget wExportDumpFSBox =
      XmCreateFileSelectionDialog(wXYPlotTopLevel,
				  "Choose a file to dump ASCII Data",
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
  XmFileSelectionBoxCallbackStruct *cbs =
    (XmFileSelectionBoxCallbackStruct *) data;
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
    Widget wid = XmCreateErrorDialog(wXYPlotTopLevel, "error", args, 1);
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
    label_str = XmStringCreateSimple("Overwrite existing file?");
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
void XYPlotWin::CBdoASCIIDump(Widget, XtPointer client_data, XtPointer data) {
  char *filename = (char *) client_data;
  FILE *fs = fopen(filename, "w");

  if(fs == NULL) {
    XmString label_str;
    Arg args[1];
    label_str = XmStringCreateSimple("Access denied.");
    XtSetArg(args[0], XmNmessageString, label_str);
    Widget wid = XmCreateErrorDialog(wXYPlotTopLevel, filename, args, 1);
    XmStringFree(label_str);
    XtUnmanageChild(XmMessageBoxGetChild(wid, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(wid, XmDIALOG_HELP_BUTTON));    
    XtManageChild(wid);
    XtFree(filename);
    return;
  }
  XtFree(filename);
  
  fprintf(fs, "TitleText: %s\n", pltTitle);
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

  for(XYPlotLegendItem *item = legendHead; item; item = item->next) {
    if( ! item->drawQ) {
      continue;
    }
    XYPlotDataList *list = item->list;
    fprintf(fs, "\"%s %lf Level %d/%d\n",
	    list->derived.c_str(),
	    list->intersectPoint[list->curLevel],
	    list->curLevel, list->maxLevel);
    for(XYPlotDataListIterator li(list); li; ++li) {
      fprintf(fs, "%lf %lf\n", li.xval, li.yval);
    }
    fprintf(fs, "\n");
  }
  fclose(fs);

  XtPopdown(wExportFileDialog);
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoOptions(Widget, XtPointer, XtPointer) {
  if(wOptionsDialog == None) {
    // Build options dialog.
    char *firstColumnText[] = {
      "Data Markers", "Ticks (vs. grid)", "Axis Lines", "Bounding Box",
      "Plot Lines", "Display Hints"
    };
    char *secondColumnText[] = {
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
    
    int ii;
    Widget wid;
    char buffer[20], *str;
    for(ii = 0; ii != 6; ++ii) {
      switch(ii) {
        case 0: sprintf(buffer, "%d", gridW); str = buffer; break;
        case 1: sprintf(buffer, "%d", lineW); str = buffer; break;
        case 2: str = XUnitText; break;
        case 3: str = YUnitText; break;
        case 4: str = formatX; break;
        case 5: str = formatY; break;
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
    
    Widget wOptionsButtons =
      XtVaCreateManagedWidget("optionsbuttons", xmFormWidgetClass,
			      wOptionsForm,
			      XmNtopAttachment,    XmATTACH_WIDGET,
			      XmNtopWidget,        wOptionsRC,
			      XmNtopOffset,        5,
			      XmNbottomAttachment, XmATTACH_FORM,
			      XmNbottomOffset,     10,
			      XmNleftAttachment,   XmATTACH_FORM,
			      XmNrightAttachment,  XmATTACH_FORM,
			      XmNfractionBase, 7,
			      NULL);
    
    Widget wOkButton =
      XtVaCreateManagedWidget("Ok", xmPushButtonGadgetClass,
			      wOptionsButtons,
			      XmNtopAttachment,    XmATTACH_FORM,
			      XmNbottomAttachment, XmATTACH_FORM,
			      XmNleftAttachment,   XmATTACH_POSITION,
			      XmNleftPosition,     1,
			      XmNrightAttachment,  XmATTACH_POSITION,
			      XmNrightPosition,    2,
			      NULL);
    AddStaticCallback(wOkButton, XmNactivateCallback,
		      &XYPlotWin::CBdoOptionsOKButton, (XtPointer) 1);
    
    Widget wCancelButton =
      XtVaCreateManagedWidget("Cancel", xmPushButtonGadgetClass,
			      wOptionsButtons,
			      XmNtopAttachment,    XmATTACH_FORM,
			      XmNbottomAttachment, XmATTACH_FORM,
			      XmNleftAttachment,   XmATTACH_POSITION,
			      XmNleftPosition,     3,
			      XmNrightAttachment,  XmATTACH_POSITION,
			      XmNrightPosition,    4,
			      NULL);
    AddStaticCallback(wCancelButton, XmNactivateCallback,
		      &XYPlotWin::CBdoOptionsOKButton, (XtPointer) 0);
    
    wid = XtVaCreateManagedWidget("Save as Default",
			      xmToggleButtonGadgetClass,
			      wOptionsButtons,
			      XmNset,     tbool[6],
			      XmNleftAttachment,   XmATTACH_POSITION,
			      XmNleftPosition,     5,
			      XmNrightAttachment,  XmATTACH_FORM,
			      XmNbottomAttachment, XmATTACH_FORM,
			      XmNbottomOffset,     5,
			      NULL);
    wOptionsWidgets[6] = wid;
			      
    AddStaticCallback(wid, XmNvalueChangedCallback,
		      &XYPlotWin::CBdoOptionsToggleButton,
		      (XtPointer) 6);

    XtManageChild(wOptionsButtons);
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
    delete XUnitText;
    delete YUnitText;
    delete formatX;
    delete formatY;
    XUnitText = XmTextGetString(wOptionsWidgets[9]);
    YUnitText = XmTextGetString(wOptionsWidgets[10]);
    formatX   = XmTextGetString(wOptionsWidgets[11]);
    formatY   = XmTextGetString(wOptionsWidgets[12]);

    if(whichType == XDIR)
      parameters->Set_Parameter("XUnitTextX", STR, XUnitText);
    else if(whichType == YDIR)
      parameters->Set_Parameter("XUnitTextY", STR, XUnitText);
    else parameters->Set_Parameter("XUnitTextZ", STR, XUnitText);

    parameters->Set_Parameter("YUnitText", STR, YUnitText);
    parameters->Set_Parameter("FormatX", STR, formatX);
    parameters->Set_Parameter("FormatY", STR, formatY);

    CBdoRedrawPlot(None, NULL, NULL);
    for(XYPlotLegendItem *ptr = legendHead; ptr; ptr = ptr->next) {
      XClearWindow(disp, XtWindow(ptr->wid));
      CBdoDrawLegendItem(None, ptr, NULL);
    }

    if(saveDefaultQ){
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
      if(whichType == XDIR) {
	parameters->Set_Parameter("InitialXWindowOffsetX", INT, buf);
	parameters->Set_Parameter("InitialXWindowOffsetY", INT, buf2);
      } else if(whichType == YDIR){
	parameters->Set_Parameter("InitialYWindowOffsetX", INT, buf);
	parameters->Set_Parameter("InitialYWindowOffsetY", INT, buf2);
      } else{
	parameters->Set_Parameter("InitialZWindowOffsetX", INT, buf);
	parameters->Set_Parameter("InitialZWindowOffsetY", INT, buf2);
      }

      parameters->WriteToFile(".XYPlot.Defaults");
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

  XtPopdown(wOptionsDialog);
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
    if(item->drawQ) {
      item->drawQ = 0;
      --numDrawnItems;
      XtVaSetValues(item->frame,
		    XmNtopShadowColor, backgroundPix,
		    XmNbottomShadowColor, backgroundPix,
		    NULL);
      if( ! animatingQ) {
	lloX = lloY =  DBL_MAX;
	hhiX = hhiY = -DBL_MAX;
	for(XYPlotLegendItem *ptr = legendHead; ptr; ptr = ptr->next) {
	  if(ptr->drawQ) {
	    updateBoundingBox(ptr->list);
	  }
	}
      }
    } else {
      item->drawQ = 1;
      ++numDrawnItems;
      XtVaSetValues(item->frame,
		    XmNtopShadowColor, foregroundPix,
		    XmNbottomShadowColor, foregroundPix,
		    NULL);
      if( ! animatingQ) {
        updateBoundingBox(item->list);
      }
    }
    if( ! animatingQ) {
      if( ! zoomedInQ) {
        setBoundingBox();
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
				   XtPointer call_data) {
  if(animatingQ) {
    return;
  }
  XYPlotLegendItem *item = (XYPlotLegendItem *) client_data;
  XYPlotLegendItem *ptr;

  if(item == colorChangeItem) {
    colorChangeItem = NULL;
  }
  if((ptr = item->next) != NULL) {
    if((ptr->prev = item->prev) != NULL) {
      item->prev->next = ptr;
      XtVaSetValues(ptr->frame, XmNtopWidget, item->prev->frame, NULL);
    } else {
      legendHead = ptr;
      XtVaSetValues(ptr->frame, XmNtopWidget, wLegendMenu, NULL);
    }
    if(item->list->copied_from == NULL && ptr->list->copied_from == item->list) {
      ptr->list->copied_from = NULL;
      item->list->copied_from = ptr->list;
      for(XYPlotLegendItem *ptr2 = ptr->next;
	  ptr2 && ptr2->list->copied_from == item->list;
	  ptr2 = ptr2->next)
      {
	ptr2->list->copied_from = ptr->list;
      }
    }
  } else if((legendTail = item->prev) != NULL) {
    item->prev->next = NULL;
  } else {
    legendHead = NULL;
  }

  char mask = 0x1 << item->color;
  lineFormats[item->style] &= ~mask;
  --numItems;

  if(item->drawQ) {
    --numDrawnItems;
    lloX = lloY =  DBL_MAX;
    hhiX = hhiY = -DBL_MAX;
    for(ptr = legendHead; ptr; ptr = ptr->next) {
      if(ptr->drawQ) {
        updateBoundingBox(ptr->list);
      }
    }
    if( ! zoomedInQ) {
      setBoundingBox();
    }
    CBdoRedrawPlot(None, NULL, NULL);
  }

  XtDestroyWidget(item->frame);
  delete item->list;
  delete item;
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoCopyDataList(Widget, XtPointer data, XtPointer) {
  if(animatingQ) {
    return;
  }
  XYPlotLegendItem *item = (XYPlotLegendItem *) data;
  AddDataList(new XYPlotDataList(item->list), item);
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoSetListLevel(Widget, XtPointer data, XtPointer) {
  if(animatingQ) {
    return;
  }
  XYMenuCBData *cbd = (XYMenuCBData *) data;
  XYPlotLegendItem *item = cbd->item;

  item->list->SetLevel(cbd->which);
  XClearWindow(disp, XtWindow(item->wid));
  CBdoDrawLegendItem(None, item, NULL);
  if(item->drawQ) {
    lloX = lloY = DBL_MAX;
    hhiX = hhiY = -DBL_MAX;
    for(item = legendHead; item; item = item->next) {
      if(item->drawQ) {
        updateBoundingBox(item->list);
      }
    }
    if( ! zoomedInQ) {
      setBoundingBox();
    }
    CBdoRedrawPlot(None, NULL, NULL);
  }
}


// -------------------------------------------------------------------
void XYPlotWin::SetPalette(void) {
  Palette *pal = pltParent->GetPalettePtr();
  pal->SetWindowPalette(pltParent->GetPaletteName(), XtWindow(wXYPlotTopLevel));
  pal->SetWindowPalette(pltParent->GetPaletteName(), pWindow);
  char buffer[20];
  params param_temp;   // temporary parameter grabbing slot
  for(int idx(0); idx < 8; ++idx) {
    sprintf(buffer, "%d.Color", idx);
    AllAttrs[idx].pixelValue = pal->GetColorCells()[PM_INT(buffer)].pixel;
  }

  for(XYPlotLegendItem *item = legendHead; item; item = item->next) {
    pal->SetWindowPalette(pltParent->GetPaletteName(), XtWindow(item->wid));
  }
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoInitializeListColorChange(Widget, XtPointer data, XtPointer) {
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
    }
  }
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoSelectAllData(Widget, XtPointer, XtPointer) {
  numDrawnItems = numItems;
  for(XYPlotLegendItem *ptr = legendHead; ptr; ptr = ptr->next) {
    if( ! ptr->drawQ) {
      ptr->drawQ = 1;
      XtVaSetValues(ptr->frame,
		    XmNtopShadowColor, foregroundPix,
		    XmNbottomShadowColor, foregroundPix,
		    NULL);
      if( ! animatingQ) {
        updateBoundingBox(ptr->list);
      }
    }
  }
  if( ! animatingQ) {
    if( ! zoomedInQ) {
      setBoundingBox();
    }
  }
  CBdoRedrawPlot(None, NULL, NULL);
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoDeselectAllData(Widget, XtPointer, XtPointer) {
  numDrawnItems = 0;
  for(XYPlotLegendItem *ptr = legendHead; ptr; ptr = ptr->next) {
    if(ptr->drawQ) {
      ptr->drawQ = 0;
      XtVaSetValues(ptr->frame,
		    XmNtopShadowColor, backgroundPix,
		    XmNbottomShadowColor, backgroundPix,
		    NULL);
    }
  }
  if( ! animatingQ) {
    lloX = lloY =  DBL_MAX;
    hhiX = hhiY = -DBL_MAX;
  }
  CBdoRedrawPlot(None, NULL, NULL);

}


// -------------------------------------------------------------------
void XYPlotWin::drawHint(void) {
  if(devInfo.areaW < 50 * devInfo.axisW) {
    return;
  }
  XClearArea(disp, pWindow, 20 * devInfo.axisW,
	     0, 0, (5 * devInfo.axisH) / 2, false);
  switch(currHint) {
    case 0:
      textX(wPlotWin, devInfo.areaW - 5, devInfo.bdrPad,
	    "Left click to zoom in.", T_UPPERRIGHT, T_AXIS);

      if(zoomedInQ) {
        textX(wPlotWin, devInfo.areaW - 5, devInfo.bdrPad + devInfo.axisH,
	      "Right click to zoom out.", T_UPPERRIGHT, T_AXIS);
      }
    break;
    case 1:
      textX(wPlotWin, devInfo.areaW - 5, devInfo.bdrPad,
	    "Left click on legend items to toggle draw.", T_UPPERRIGHT, T_AXIS);
      textX(wPlotWin, devInfo.areaW - 5, devInfo.bdrPad + devInfo.axisH,
	    "Right click to select options.", T_UPPERRIGHT, T_AXIS);

    break;
  }
}


// -------------------------------------------------------------------
void XYPlotWin::CBdoDrawLocation(Widget, XtPointer, XtPointer data) {
  if(devInfo.areaW < XLocWinX + 15 * devInfo.axisW) {
    return;
  }
  XEvent *event = (XEvent *) data;


  if(event->type == LeaveNotify) {
    if(dispHintsQ) {
      currHint = 1;
      drawHint();
    }
    XClearArea(disp, pWindow, 0, XLocWinY, XLocWinX, 0, false);
  } else {
    if(dispHintsQ && currHint != 0) {
      currHint = 0;
      drawHint();
    }
    Window whichRoot, whichChild;
    int rootX, rootY, newX, newY;
    unsigned int inputMask;
    
    XQueryPointer(disp, pWindow, &whichRoot, &whichChild,
		  &rootX, &rootY, &newX, &newY, &inputMask);
    if(newX <= XOppX && newX >= XOrgX && newY <= XOppY && newY >= XOrgY) {
      char locText[40];
      sprintf(locText, "(%.4E, %.4E)", TRANX(newX), TRANY(newY));
      XClearArea(disp, pWindow, 0, XLocWinY, XLocWinX, 0, false);
      textX(wPlotWin, devInfo.bdrPad, devInfo.areaH - devInfo.bdrPad,
	    locText, T_LOWERLEFT, T_AXIS);
    } else {
      XClearArea(disp, pWindow, 0, XLocWinY, XLocWinX, 0, false);
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

  // FIXME:
  XSetForeground(disp, rbGC, 120);

  if(cbs->event->xany.type == ButtonPress) {
    servingButton = cbs->event->xbutton.button;
    if(servingButton != 1) {
      if(zoomedInQ) {
	zoomedInQ = 0;
	setBoundingBox();
	CBdoRedrawPlot(None, NULL, NULL);
      }
      return;
    }

    oldX = newX = anchorX = Max(0, cbs->event->xbutton.x);
    oldY = newY = anchorY = Max(0, cbs->event->xbutton.y);
    rectDrawn = false;
    // grab server and draw box(es)
    XChangeActivePointerGrab(disp,
			     ButtonMotionMask | OwnerGrabButtonMask |
			     ButtonPressMask | ButtonReleaseMask |
			     PointerMotionMask | PointerMotionHintMask,
			     zoomCursor, CurrentTime);
    XGrabServer(disp);
    lowX = TRANX(anchorX);
    lowY = TRANY(anchorY);
    
    if(devInfo.areaW >= 2 * XLocWinX + 15 * devInfo.axisW) {
      sprintf(locText, "(%.4E, %.4E)", lowX, lowY);
      XClearArea(disp, pWindow, XLocWinX, XLocWinY, XLocWinX, 0, false);
      textX(wPlotWin, XLocWinX + devInfo.bdrPad,
	    devInfo.areaH - devInfo.bdrPad,
	    locText, T_LOWERLEFT, T_AXIS);
    }
    
    while(true) {
      if(devInfo.areaW >= XLocWinX + 15 * devInfo.axisW) {
	sprintf(locText, "(%.4E, %.4E)", TRANX(newX), TRANX(newY));
	XClearArea(disp, pWindow, 0, XLocWinY, XLocWinX, 0, false);
	textX(wPlotWin, devInfo.bdrPad, devInfo.areaH - devInfo.bdrPad,
	      locText, T_LOWERLEFT, T_AXIS);
      }
      
      XNextEvent(disp, &nextEvent);
      
      switch(nextEvent.type) {
	
      case MotionNotify:
	
	if(rectDrawn) {   // undraw the old rectangle(s)
	  rWidth  = abs(oldX-anchorX);
	  rHeight = abs(oldY-anchorY);
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

	rWidth  = abs(newX-anchorX);   // draw the new rectangle
	rHeight = abs(newY-anchorY);
	rStartX = (anchorX < newX) ? anchorX : newX;
	rStartY = (anchorY < newY) ? anchorY : newY;
	XDrawRectangle(disp, pWindow, rbGC, rStartX, rStartY,
		       rWidth, rHeight);
	rectDrawn = true;
	
	oldX = newX;
	oldY = newY;
	
	break;
	
      case ButtonRelease:
	XUngrabServer(disp);  // giveitawaynow
	
	// undraw rectangle
	rWidth  = abs(oldX-anchorX);
	rHeight = abs(oldY-anchorY);
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
	  zoomedInQ = 1;
	  setBoundingBox(lowX, lowY, highX, highY);
	  CBdoRedrawPlot(None, NULL, NULL);
	} else if((anchorX - newX == 0) && (anchorY - newY == 0)) {
	  if(newX >= XOrgX && newX <= XOppX && newY >= XOrgY && newY <= XOppY) {
	    highX = (loX + hiX) / 2 - highX;
	    highY = (loY + hiY) / 2 - highY;
	    loX -= highX;
	    hiX -= highX;
	    loY -= highY;
	    hiY -= highY;
	    zoomedInQ = 1;
	    CBdoRedrawPlot(None, NULL, NULL);
	  }
	}
	if(devInfo.areaW > XLocWinX + 15 * devInfo.axisW) {
	  if(devInfo.areaW > 2*XLocWinX + 15 * devInfo.axisW) {
	    XClearArea(disp, pWindow, 0, XLocWinY, 2*XLocWinX, 0, false);
	  } else {
	    XClearArea(disp, pWindow, 0, XLocWinY, XLocWinX, 0, false);
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
  XYPlotDataList *dataList = item->list;
  XSegment legLine;
  char legendText[1024];

#if (BL_SPACEDIM == 3)
  sprintf(legendText, "%d/%d %s", dataList->curLevel, dataList->maxLevel,
	  dataList->derived.c_str());
  textX(item->wid, 5, 10, legendText, T_UPPERLEFT, T_AXIS);
  textX(item->wid, 5, 10 + devInfo.axisH,
	dataList->intersectPoint[dataList->curLevel], T_UPPERLEFT, T_AXIS);
#else
  sprintf(legendText, "%d/%d %s %s", dataList->curLevel, dataList->maxLevel,
	  dataList->derived.c_str(), dataList->intersectPoint[dataList->curLevel]);
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
  XtAddCallback(w, cbtype, (XtCallbackProc) &XYPlotWin::StaticCallback,
		(XtPointer) cbs);

}
 

// -------------------------------------------------------------------
void XYPlotWin::AddStaticWMCallback(Widget w, Atom cbtype, memberXYCB cbf,
				     void *data)
{
  XYCBData *cbs = new XYCBData(this, data, cbf);
  XmAddWMProtocolCallback(w, cbtype, (XtCallbackProc) &XYPlotWin::StaticCallback,
			  (XtPointer) cbs);

}
 

// -------------------------------------------------------------------
void XYPlotWin::AddStaticEventHandler(Widget w, EventMask mask,
				      memberXYCB cbf, void *data)
{
  XYCBData *cbs = new XYCBData(this, data, cbf);
  XtAddEventHandler(w, mask, false, (XtEventHandler) &XYPlotWin::StaticEvent,
		    (XtPointer) cbs);
}


// -------------------------------------------------------------------
void XYPlotWin::StaticCallback(Widget w, XtPointer client_data,
				  XtPointer call_data)
{
  XYCBData *cbs = (XYCBData *) client_data;
  XYPlotWin *obj = cbs->instance;
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
void CBcloseXYPlotWin(Widget w, XtPointer client_data, XtPointer) {
  XYPlotWin *win = (XYPlotWin *) client_data;
  delete win;
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
