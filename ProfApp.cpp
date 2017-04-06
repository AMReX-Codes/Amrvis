// ---------------------------------------------------------------
// ProfApp.cpp
// ---------------------------------------------------------------
#include <winstd.H>

#include <ProfApp.H>
#include <ProfDataServices.H>
#include <GraphicsAttributes.H>
#include <XYPlotWin.H>
#include <ParallelDescriptor.H>
#include <MessageArea.H>

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

#include <X11/cursorfont.h>

#include <cctype>
#include <sstream>
using std::cout;
using std::cerr;
using std::endl;
using std::min;
using std::max;

// Hack for window manager calls
#ifndef FALSE
#define FALSE false
#endif

cMessageArea profAppMessageText;
char cbuff[100];


// -------------------------------------------------------------------
ProfApp::~ProfApp() {
  delete XYplotparameters;
  delete gaPtr;

  XtDestroyWidget(wAmrVisTopLevel);

  for(int nSize(0); nSize < cbdPtrs.size(); ++nSize) {
    delete cbdPtrs[nSize];
  }
}


// -------------------------------------------------------------------
ProfApp::ProfApp(XtAppContext app, Widget w, const string &filename,
	       const Array<ProfDataServices *> &profdataservicesptr)
  : wTopLevel(w),
    appContext(app),
    fileName(filename),
    profDataServicesPtr(profdataservicesptr)
{
  int i;

  fileNames.resize(1);
  fileNames[0] = fileName;

  std::ostringstream headerout;
  headerout << AVGlobals::StripSlashes(fileNames[0]) << "   bbbbbbbbbb!!!!";

  // Set the delete response to DO_NOTHING so that it can be app defined.
  wAmrVisTopLevel = XtVaCreatePopupShell(headerout.str().c_str(), 
			 topLevelShellWidgetClass, wTopLevel,
			 XmNwidth,	    initialWindowWidth,
			 XmNheight,	    initialWindowHeight,
			 XmNx,		    100,
			 XmNy,		    200,
			 XmNdeleteResponse, XmDO_NOTHING,
			 NULL);

  gaPtr = new GraphicsAttributes(wAmrVisTopLevel);
  display = gaPtr->PDisplay();
  xgc = gaPtr->PGC();
  if(gaPtr->PVisual() != XDefaultVisual(display, gaPtr->PScreenNumber())) {
    XtVaSetValues(wAmrVisTopLevel, XmNvisual, gaPtr->PVisual(),
		  XmNdepth, gaPtr->PDepth(), NULL);
  }

  //const string pfVersion(amrData.PlotFileVersion());

  headerout.str(std::string());
  headerout << AVGlobals::StripSlashes(fileNames[0])
            << "   aaaaaaaaaaaaaaaa!!!!";
  XtVaSetValues(wAmrVisTopLevel, XmNtitle, const_cast<char *>(headerout.str().c_str()),
		NULL);

  ProfAppInit();
}


// -------------------------------------------------------------------
void ProfApp::ProfAppInit() {
  int np;

  XmAddWMProtocolCallback(wAmrVisTopLevel,
			  XmInternAtom(display,"WM_DELETE_WINDOW", false),
			  (XtCallbackProc) CBQuitProfApp, (XtPointer) this);

  for(np = 0; np != BL_SPACEDIM; ++np) {
    XYplotwin[np] = NULL; // No 1D plot windows initially.
  }

  servingButton = 0;
  startX = 0;
  startY = 0;
  endX = 0;
  endY = 0;

  infoShowing     = false;
			  
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
  wMenuBar = XmCreateMenuBar(wMainArea, "menubar", args, 3);

  // ------------------------------- file menu
  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, "Filepulldown", NULL, 0);
  XtVaCreateManagedWidget("File", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'F', XmNsubMenuId, wMenuPulldown, NULL);

  // Quit
  XtVaCreateManagedWidget(NULL, xmSeparatorGadgetClass, wMenuPulldown, NULL);
  label_str = XmStringCreateSimple("Ctrl+Q");
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
  label_str = XmStringCreateSimple("Ctrl+C");
  wid = XtVaCreateManagedWidget("Close", xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'C',
				XmNaccelerator, "Ctrl<Key>C",
				XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  XtAddCallback(wid, XmNactivateCallback, (XtCallbackProc) CBQuitProfApp,
		(XtPointer) this);
  
  Widget wTempDrawLevel = wid;

  // --------------------------------------------------------------- help menu
  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, "MenuPulldown", NULL, 0);
  XtVaCreateManagedWidget("Help", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'H', XmNsubMenuId,   wMenuPulldown, NULL);
  wid = XtVaCreateManagedWidget("Info...", xmPushButtonGadgetClass,
				wMenuPulldown, XmNmnemonic,  'I', NULL);
  AddStaticCallback(wid, XmNactivateCallback, &ProfApp::DoInfoButton);
  
  XtManageChild(wMenuBar);


  // ************************** Plot frame and area
  wPlotFrame = XtVaCreateManagedWidget("plotframe",
			    xmFrameWidgetClass,   wMainArea,
			    XmNrightAttachment,	  XmATTACH_WIDGET,
			    XmNrightWidget,	  wPalFrame,
			    XmNleftAttachment,    XmATTACH_FORM,
			    XmNbottomAttachment,  XmATTACH_FORM,
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
			    XmNbottomAttachment, XmATTACH_FORM,
			    NULL);

  wScrollArea[Amrvis::ZPLANE] = XtVaCreateManagedWidget("scrollAreaXY",
		xmScrolledWindowWidgetClass,
		wPlotArea,
		XmNleftAttachment,	XmATTACH_FORM,
		XmNleftOffset,		0,
		XmNtopAttachment,	XmATTACH_FORM,
		XmNtopOffset,		0,
		XmNscrollingPolicy,	XmAUTOMATIC,
		NULL);

  trans =
	"<Btn1Motion>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn1Down>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn1Up>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn2Motion>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn2Down>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn2Up>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn3Motion>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn3Down>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn3Up>: DrawingAreaInput() ManagerGadgetButtonMotion()";
	
  wPlotPlane[Amrvis::ZPLANE] = XtVaCreateManagedWidget("plotArea",
			    xmDrawingAreaWidgetClass, wScrollArea[Amrvis::ZPLANE],
			    XmNtranslations,  XtParseTranslationTable(trans),
			    XmNwidth, amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeH() + 1,
			    XmNheight, amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeV() + 1,
			    NULL);
  XtVaSetValues(wScrollArea[Amrvis::ZPLANE], XmNworkWindow, wPlotPlane[Amrvis::ZPLANE], NULL); 
  XtVaSetValues(wScrollArea[Amrvis::ZPLANE], XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM, NULL);		
  
  
  
  // ***************************************************************** 
  XtManageChild(wPlotArea);
  XtPopup(wAmrVisTopLevel, XtGrabNone);
  
  char plottertitle[50];
  sprintf(plottertitle, "XYPlot%dd", BL_SPACEDIM);
  XYplotparameters = new XYPlotParameters(pltPaletteptr, gaPtr, plottertitle);

  interfaceReady = true;

}  // end ProfAppInit()


// -------------------------------------------------------------------
void ProfApp::DestroyInfoWindow(Widget, XtPointer xp, XtPointer) {
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
			 XmNwidth,	400,
			 XmNheight,	300,
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

  Widget wInfoList = XmCreateScrolledText(wInfoForm, "infoscrolledlist", args, i);

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
  ProfData &profData = profDataServicesPtr[0]->AmrDataRef();
  
  std::ostringstream prob;
  prob.precision(15);

  prob << fileNames[0] << '\n';
  prob << "prob domain:" << '\n';

  profAppMessageText.PrintText(prob.str().c_str());
}

/*
// -------------------------------------------------------------------
void ProfApp::DoInfoButtonScrolledList(Widget, XtPointer, XtPointer) {
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
			 XmNwidth,	400,
			 XmNheight,	300,
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
  XtSetArg(args[i++], XmNlistSizePolicy, XmRESIZE_IF_POSSIBLE);
  Widget wInfoList = XmCreateScrolledList(wInfoForm, "infoscrolledlist", args, i);
  
  XtVaSetValues(XtParent(wInfoList), 
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNtopOffset, Amrvis::WOFFSET,
		XmNbottomAttachment, XmATTACH_POSITION,
		XmNbottomPosition, 80,
		NULL);
  
  AmrData &amrData = profDataServicesPtr[0]->AmrDataRef();
  
  int numEntries(9 + amrData.FinestLevel() + 1);
  XmStringTable strList = (XmStringTable) XtMalloc(numEntries*sizeof(XmString *));
  
  i = 0;
  std::ostringstream prob;
  prob.precision(15);

  prob << fileNames[0];
  strList[i++] = XmStringCreateSimple(const_cast<char *>(prob.str().c_str()));
  prob.str(std::string());  // clear prob

  prob << amrData.PlotFileVersion().c_str();
  strList[i++] = XmStringCreateSimple(const_cast<char *>(prob.str().c_str()));
  prob.str(std::string());  // clear prob

  XtVaSetValues(wInfoList,
		XmNvisibleItemCount, numEntries,
		XmNitemCount, numEntries,
		XmNitems, strList,
		NULL);

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
}
*/


// -------------------------------------------------------------------
XYPlotDataList *ProfApp::CreateLinePlot(int V, int sdir, int mal, int ix,
				       const string *derived)
{
  const AmrData &amrData(profDataServicesPtr[0]->AmrDataRef());
  
  // Create an array of boxes corresponding to the intersected line.
  int tdir(-1), dir1(-1);
  switch (V) {
  case Amrvis::ZPLANE:
    tdir = (sdir == Amrvis::XDIR) ? Amrvis::YDIR : Amrvis::XDIR;
    dir1 = tdir;
  }
  Array<Box> trueRegion(mal + 1);
  trueRegion[mal] = amrPicturePtrArray[V]->GetSliceBox(mal);
  trueRegion[mal].setSmall(tdir, ivLowOffsetMAL[tdir] + ix);
  trueRegion[mal].setBig(tdir, trueRegion[mal].smallEnd(tdir));
  int lev;
  for(lev = mal - 1; lev >= 0; --lev) {
    trueRegion[lev] = trueRegion[mal];
    trueRegion[lev].coarsen(AVGlobals::CRRBetweenLevels(lev, mal,
                            amrData.RefRatio()));
  }
  // Create an array of titles corresponding to the intersected line.
  Array<Real> XdX(mal + 1);
  Array<char *> intersectStr(mal + 1);
  
  for(lev = 0; lev <= mal; ++lev) {
    XdX[lev] = amrData.DxLevel()[lev][sdir];
    intersectStr[lev] = new char[128];
    sprintf(intersectStr[lev], ((dir1 == Amrvis::XDIR) ? "X=" : "Y="));
    //sprintf(intersectStr[lev]+2, pltAppState->GetFormatString().c_str(),
	    //gridOffset[dir1] +
	    //(0.5 + trueRegion[lev].smallEnd(dir1))*amrData.DxLevel()[lev][dir1]);
  }
  XYPlotDataList *newlist = new XYPlotDataList(*derived,
                                     pltAppState->MinDrawnLevel(), mal,
				     ix, amrData.RefRatio(),
		                     XdX, intersectStr, gridOffset[sdir]);

  bool lineOK;
  ProfDataServices::Dispatch(ProfDataServices::LineValuesRequest,
			 profDataServicesPtr[0],
			 mal + 1,
			 (void *) (trueRegion.dataPtr()),
			 sdir,
			 (void *) derived,
			 pltAppState->MinAllowableLevel(), mal,
			 (void *) newlist, &lineOK);
  if(lineOK) {
    return newlist;
  }
  delete newlist;
  return NULL;
}


// -------------------------------------------------------------------
void ProfApp::PADoExposePicture(Widget w, XtPointer client_data, XtPointer) {
  unsigned long np = (unsigned long) client_data;
}


// -------------------------------------------------------------------
void ProfApp::AddStaticCallback(Widget w, String cbtype, memberCB cbf, void *d) {
  CBData *cbs = new CBData(this, d, cbf);

  // cbdPtrs.push_back(cbs)
  int nSize(cbdPtrs.size());
  cbdPtrs.resize(nSize + 1);
  cbdPtrs[nSize] = cbs;

  XtAddCallback(w, cbtype, (XtCallbackProc ) &ProfApp::StaticCallback,
		(XtPointer) cbs);
}


// -------------------------------------------------------------------
void ProfApp::AddStaticEventHandler(Widget w, EventMask mask, memberCB cbf, void *d)
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
XtIntervalId ProfApp::AddStaticTimeOut(int time, memberCB cbf, void *d) {
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

const string &ProfApp::GetFileName()  { return (fileNames[0]); }

void ProfApp::SetInitialWindowHeight(int initWindowHeight) {
  ProfApp::initialWindowHeight = initWindowHeight;
}

void ProfApp::SetInitialWindowWidth(int initWindowWidth) {
  ProfApp::initialWindowWidth = initWindowWidth;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
