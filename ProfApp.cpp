// ---------------------------------------------------------------
// ProfApp.cpp
// ---------------------------------------------------------------

#include <ProfApp.H>
#include <ProfDataServices.H>
#include <AMReX_DataServices.H>
#include <AmrPicture.H>
#include <PltApp.H>
#include <PltAppState.H>
#include <GraphicsAttributes.H>
#include <XYPlotWin.H>
#include <AMReX_ParallelDescriptor.H>
#include <MessageArea.H>
#include <Palette.H>

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

void CollectMProfStats(std::map<std::string, BLProfiler::ProfStats> &mProfStats,
                       const Array<Array<BLProfStats::FuncStat> > &funcStats,
                       const Array<std::string> &fNames,
                       Real runTime, int whichProc);

// -------------------------------------------------------------------
ProfApp::~ProfApp() {
  delete XYplotparameters;
  delete pltPaletteptr;
  delete gaPtr;
  delete profAppStatePtr;

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
  fileNames.resize(1);
  fileNames[0] = fileName;

  std::ostringstream headerout;
  headerout << AVGlobals::StripSlashes(fileNames[0]) << "   bbbbbbbbbb!!!!";

  // Set the delete response to DO_NOTHING so that it can be app defined.
  wAmrVisTopLevel = XtVaCreatePopupShell(headerout.str().c_str(), 
			 topLevelShellWidgetClass, wTopLevel,
			 XmNwidth,	    initialWindowWidth,
			 XmNheight,	    initialWindowHeight,
			 XmNx,		    40,
			 XmNy,		    300,
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

cout << "_here 01:  calling ProfAppInit." << endl;
  ProfAppInit();
//cout << "_here 04:  calling WriteSummary." << endl;
////profDataServicesPtr[0]->WriteSummary(cout, false, 0, false);
//BLProfilerUtils::WriteHeader(cout, 10, 16, true);

std::string regionsFileName("pltRegions");
dspArray.resize(1);
dspArray[0] = new DataServices(regionsFileName, Amrvis::NEWPLT);
PltApp *temp = new PltApp(app, wTopLevel, regionsFileName, dspArray, false);
pltAppList.push_back(temp);

  Array<std::string> funcs;
  std::ostringstream ossSummary;
  profDataServicesPtr[0]->WriteSummary(ossSummary, false, 0, false);
  size_t startPos(0), endPos(0);
  const std::string &ossString = ossSummary.str();
  while(startPos < ossString.length() - 1) {
    endPos = ossString.find('\n', startPos);
    std::string sLine(ossString.substr(startPos, endPos - startPos));
    startPos = endPos + 1;
    funcs.push_back(sLine);
  }
  GenerateFuncList(funcs);
}


// -------------------------------------------------------------------
void ProfApp::ProfAppInit() {
  int np;

  XmAddWMProtocolCallback(wAmrVisTopLevel,
			  XmInternAtom(display,const_cast<String> ("WM_DELETE_WINDOW"), false),
			  (XtCallbackProc) CBQuitProfApp, (XtPointer) this);

  for(np = 0; np != BL_SPACEDIM; ++np) {
    XYplotwin[np] = NULL; // No 1D plot windows initially.
  }

  servingButton = 0;

  infoShowing = false;

  int palListLength(AVPalette::PALLISTLENGTH);
  int palWidth(AVPalette::PALWIDTH);
  int totalPalWidth(AVPalette::TOTALPALWIDTH);
  int totalPalHeight(AVPalette::TOTALPALHEIGHT);
  int reserveSystemColors(0);
  bool bRegions(true);
  if(bRegions) {
    totalPalWidth += 100;
  }
  pltPaletteptr = new Palette(wTopLevel, palListLength, palWidth,
                              totalPalWidth, totalPalHeight,
                              reserveSystemColors);
  pltPaletteptr->ReadSeqPalette("Palette", false);

  // No need to store these widgets in the class after this function is called.
  Widget wMainArea, wPalFrame, wPlotFrame, wPalForm;

  wMainArea = XtVaCreateManagedWidget("MainArea", xmFormWidgetClass,
				      wAmrVisTopLevel,
				      NULL);

  // ------------------------------- menu bar
  Widget wMenuBar, wMenuPulldown, wid;
  XmString label_str;
  XtSetArg(args[0], XmNtopAttachment, XmATTACH_FORM);
  XtSetArg(args[1], XmNleftAttachment, XmATTACH_FORM);
  XtSetArg(args[2], XmNrightAttachment, XmATTACH_FORM);
  wMenuBar = XmCreateMenuBar(wMainArea, const_cast<String> ("menubar"), args, 3);

  // ------------------------------- file menu
  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, const_cast<String> ("Filepulldown"), NULL, 0);
  XtVaCreateManagedWidget("File", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'F', XmNsubMenuId, wMenuPulldown, NULL);

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
  strcpy(buffer, "plotlabel");
  label_str = XmStringCreateSimple(buffer);
  wPlotLabel = XtVaCreateManagedWidget("plotlabel", xmLabelWidgetClass, wPalForm,
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
                            XmNheight,  wcfHeight,
                            XmNresizePolicy, XmRESIZE_NONE,
                            NULL);
  AddStaticEventHandler(wControlForm, ExposureMask, &ProfApp::DoExposeRef);

  wControls =
    XtVaCreateManagedWidget("Generate Function List", xmPushButtonWidgetClass, wControlForm,
                            //XmNx, centerX - halfbutton,
                            XmNy, 64,
                            XmCMarginBottom, 2,
                            NULL);

  AddStaticCallback(wControls, XmNactivateCallback, &ProfApp::DoGenerateFuncList,
                          (XtPointer) 111);

  XtManageChild(wControls);


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
			    XmNheight, 242,
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
	
  wPlotPlane[Amrvis::ZPLANE] = XtVaCreateManagedWidget("plotArea",
			    xmDrawingAreaWidgetClass, wScrollArea[Amrvis::ZPLANE],
			    XmNtranslations,  XtParseTranslationTable(trans),
			    //XmNwidth, amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeH() + 1,
			    //XmNheight, amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeV() + 1,
			    XmNwidth, 700,
			    XmNheight, 500,
			    NULL);
  XtVaSetValues(wScrollArea[Amrvis::ZPLANE], XmNworkWindow, wPlotPlane[Amrvis::ZPLANE], NULL); 
  XtVaSetValues(wScrollArea[Amrvis::ZPLANE], XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM, NULL);		
  

  // ************************** profiled function list area

  Widget wFuncListFrame;
  wFuncListFrame = XtVaCreateManagedWidget("funclistframe",
                            xmFrameWidgetClass, wMainArea,
                            XmNrightAttachment, XmATTACH_WIDGET,
                            XmNrightWidget,       wPalFrame,
			    XmNleftAttachment,    XmATTACH_FORM,
			    XmNbottomAttachment,  XmATTACH_FORM,
                            XmNtopAttachment,   XmATTACH_WIDGET,
                            XmNtopWidget,       wPlotFrame,
                            XmNheight, 600,
			    XmNwidth, 850,
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
                //XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomAttachment, XmATTACH_POSITION,
                XmNbottomPosition, 90,
                XmNheight, 480,
                XmNwidth, 880,
                NULL);

  AddStaticCallback(wFuncList, XmNdefaultActionCallback, &ProfApp::DoFuncList, (XtPointer) 42);
  AddStaticCallback(wFuncList, XmNbrowseSelectionCallback, &ProfApp::DoFuncList, (XtPointer) 43);
  AddStaticCallback(wFuncList, XmNextendedSelectionCallback, &ProfApp::DoFuncList, (XtPointer) 44);
  AddStaticCallback(wFuncList, XmNmultipleSelectionCallback, &ProfApp::DoFuncList, (XtPointer) 45);

  XtManageChild(wFuncList);


  
  // ***************************************************************** 
  XtManageChild(wPalArea);
  XtManageChild(wPlotArea);
  XtPopup(wAmrVisTopLevel, XtGrabNone);

std::string palFilename("Palette");
  pltPaletteptr->SetWindow(XtWindow(wPalArea));
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPalArea), false);
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPlotArea), false);
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wAmrVisTopLevel), false);
  //for(np = 0; np != Amrvis::NPLANES; ++np) {
    //pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPlotPlane[np]), false);
    pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPlotPlane[0]), false);
  //}
    //pltPaletteptr->RedrawPalette();

  
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
/*
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
*/
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
amrex::XYPlotDataList *ProfApp::CreateLinePlot(int V, int sdir, int mal, int ix,
				       const string *derived)
{
/*
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
    //sprintf(intersectStr[lev]+2, profAppState->GetFormatString().c_str(),
	    //gridOffset[dir1] +
	    //(0.5 + trueRegion[lev].smallEnd(dir1))*amrData.DxLevel()[lev][dir1]);
  }
  amrex::XYPlotDataList *newlist = new amrex::XYPlotDataList(*derived,
                                     profAppState->MinDrawnLevel(), mal,
				     ix, amrData.RefRatio(),
		                     XdX, intersectStr, gridOffset[sdir]);

  bool lineOK;
  ProfDataServices::Dispatch(ProfDataServices::LineValuesRequest,
			 profDataServicesPtr[0],
			 mal + 1,
			 (void *) (trueRegion.dataPtr()),
			 sdir,
			 (void *) derived,
			 profAppState->MinAllowableLevel(), mal,
			 (void *) newlist, &lineOK);
  if(lineOK) {
    return newlist;
  }
  delete newlist;
*/
  return NULL;
}


// -------------------------------------------------------------------
void ProfApp::DoExposePalette(Widget, XtPointer, XtPointer) {
  pltPaletteptr->ExposePalette();
}


// -------------------------------------------------------------------
//void ProfApp::PADoExposePicture(Widget w, XtPointer client_data, XtPointer) {
  //unsigned long np = (unsigned long) client_data;
//}


// -------------------------------------------------------------------
void ProfApp::DoExposeRef(Widget, XtPointer, XtPointer) {
  int xpos(10), ypos(15);
  int color(pltPaletteptr->WhiteIndex());
  char sX[Amrvis::LINELENGTH];
  strcpy(sX, "X");

  XClearWindow(display, XtWindow(wControlForm));


  int axisLength(20);
  char hLabel[Amrvis::LINELENGTH], vLabel[Amrvis::LINELENGTH];
  strcpy(hLabel, "uuu");
  strcpy(vLabel, "ddd");
  XSetForeground(XtDisplay(wControlForm), xgc, pltPaletteptr->makePixel(color));
  XDrawLine(XtDisplay(wControlForm), XtWindow(wControlForm), xgc,
            xpos+5, ypos, xpos+5, ypos+axisLength);
  XDrawLine(XtDisplay(wControlForm), XtWindow(wControlForm), xgc,
            xpos+5, ypos+axisLength, xpos+5+axisLength, ypos+axisLength);
  XDrawString(XtDisplay(wControlForm), XtWindow(wControlForm), xgc,
              xpos+5+axisLength, ypos+5+axisLength, hLabel, strlen(hLabel));
  XDrawString(XtDisplay(wControlForm), XtWindow(wControlForm), xgc,
              xpos, ypos, vLabel, strlen(vLabel));
}


// -------------------------------------------------------------------
void ProfApp::DoFuncList(Widget w, XtPointer client_data, XtPointer call_data) {
  cout << "_in ProfApp::DoFuncList" << endl;
  unsigned long r = (unsigned long) client_data;
  XmListCallbackStruct *cbs = (XmListCallbackStruct *) call_data;

  String selection;
  XmStringGetLtoR(cbs->item, XmSTRING_DEFAULT_CHARSET, &selection);
  cout << "r selection = " << r << "  " << selection << endl;
}


// -------------------------------------------------------------------
void ProfApp::DoGenerateFuncList(Widget w, XtPointer client_data, XtPointer call_data) {
  unsigned long r = (unsigned long) client_data;
  cout << "_in ProfApp::DoGenerateFuncList:  r = " << r << endl;

  BLProfStats &blProfStats = profDataServicesPtr[0]->GetBLProfStats();
  Array<Array<BLProfStats::FuncStat>> aFuncStats;
  blProfStats.CollectFuncStats(aFuncStats);
  cout << "aFuncStats.size() = " << aFuncStats.size() << endl;
  std::map<std::string, BLProfiler::ProfStats> mProfStats;  // [fname, pstats]
  const Array<string> &blpFNames = blProfStats.BLPFNames();
  Real calcRunTime(1.0);
  int whichProc(0);
  CollectMProfStats(mProfStats, aFuncStats, blpFNames, calcRunTime, whichProc);
  cout << "||||::::" << endl;
  for(auto mps : mProfStats) {
    Real percent(100.0);
    const int colWidth(10);
    const int maxlen(64);
    bool bwriteavg(true);
    BLProfilerUtils::WriteRow(cout, mps.first, mps.second, percent, colWidth, maxlen, bwriteavg);
  }
  cout << "||||::::" << endl;

  Array<std::string> funcs;
  std::ostringstream ossSummary;
  profDataServicesPtr[0]->WriteSummary(ossSummary, false, 0, false);
  size_t startPos(0), endPos(0);
  const std::string &ossString = ossSummary.str();
  while(startPos < ossString.length() - 1) {
    endPos = ossString.find('\n', startPos);
    std::string sLine(ossString.substr(startPos, endPos - startPos));
    startPos = endPos + 1;
    funcs.push_back(sLine);
  }
  GenerateFuncList(funcs);
}


// -------------------------------------------------------------------
void ProfApp::GenerateFuncList(const Array<std::string> &funcs) {
  static int t(1000);
  cout << "_in ProfApp::DoGenerateFuncList:  t = " << t << endl;
  int numEntries(funcs.size());
  XmStringTable strList = (XmStringTable) XtMalloc(numEntries*sizeof(XmString *));

  for(int i(0); i < funcs.size(); ++i) {
    strList[i] = XmStringCreateSimple(const_cast<char *>(funcs[i].c_str()));
  }

  XtVaSetValues(wFuncList,
                XmNvisibleItemCount, numEntries,
                XmNitemCount, numEntries,
                XmNitems, strList,
                NULL);

  for(int i(0); i < funcs.size(); ++i) {
    XmStringFree(strList[i]);
  }
  XtFree((char *) strList);

  t += 1000;
}


// -------------------------------------------------------------------
void ProfApp::AddStaticCallback(Widget w, String cbtype, profMemberCB cbf, void *d) {
  CBData *cbs = new CBData(this, d, cbf);

  // cbdPtrs.push_back(cbs)
  int nSize(cbdPtrs.size());
  cbdPtrs.resize(nSize + 1);
  cbdPtrs[nSize] = cbs;

  XtAddCallback(w, cbtype, (XtCallbackProc ) &ProfApp::StaticCallback,
		(XtPointer) cbs);
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

const string &ProfApp::GetFileName()  { return (fileNames[0]); }

void ProfApp::SetInitialWindowHeight(int initWindowHeight) {
  ProfApp::initialWindowHeight = initWindowHeight;
}

void ProfApp::SetInitialWindowWidth(int initWindowWidth) {
  ProfApp::initialWindowWidth = initWindowWidth;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
