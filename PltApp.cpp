
//
// $Id: PltApp.cpp,v 1.95 2001-08-21 20:37:55 vince Exp $
//

// ---------------------------------------------------------------
// PltApp.cpp
// ---------------------------------------------------------------
#include "PltApp.H"
#include "PltAppState.H"
#include "AmrPicture.H"
#include "DataServices.H"
#include "GraphicsAttributes.H"
#include "ProjectionPicture.H"
#include "XYPlotWin.H"

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

#if defined(BL_PARALLELVOLUMERENDER)
#include <PVolRender.H>
#endif

#ifdef BL_USE_NEW_HFILES
#include <cctype>
#include <strstream>
using std::ostrstream;
using std::ends;
using std::cout;
using std::cerr;
using std::endl;
using std::min;
using std::max;

#else
#include <ctype.h>
#include <strstream.h>
#endif

// Hack for window manager calls
#ifndef FALSE
#define FALSE false
#endif

const int MAXSCALE = 32;
#define MARK fprintf(stderr, "Mark at file %s, line %d.\n", __FILE__, __LINE__)

// -------------------------------------------------------------------
PltApp::~PltApp() {
  int np;
#if (BL_SPACEDIM == 3)
  for(np = 0; np != NPLANES; ++np) {
    amrPicturePtrArray[np]->DoStop();
  }
#endif
  if(animating2d) {
    StopAnimation();
  }
  for(np = 0; np != NPLANES; ++np) {
    delete amrPicturePtrArray[np];
  }
#if (BL_SPACEDIM == 3)
  delete projPicturePtr;
#endif
  delete XYplotparameters;
  delete pltPaletteptr;
  delete GAptr;
  delete pltAppState;
  if(datasetShowing) {
    delete datasetPtr;
  }
  XtDestroyWidget(wAmrVisTopLevel);

  // delete all the call back parameter structs
  for(int nSize(0); nSize < cbdPtrs.size(); ++nSize) {
    delete cbdPtrs[nSize];
  }
}


// -------------------------------------------------------------------
PltApp::PltApp(XtAppContext app, Widget w, const aString &filename,
	       const Array<DataServices *> &dataservicesptr, bool isAnim)
  : paletteDrawn(false),
    appContext(app),
    wTopLevel(w),
    fileName(filename),
    dataServicesPtr(dataservicesptr),
    animating2d(isAnim),
    currentFrame(0),
    currentRangeType(GLOBALMINMAX),
    bCartGridSmoothing(false)
{
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  lightingWindowExists = false;
#endif

  if( ! dataservicesptr[0]->AmrDataOk()) {
    return;
  }
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  bFileRangeButtonSet = false;

  int i;

  char header[BUFSIZ];
  ostrstream headerout(header, BUFSIZ);
  int fnl(fileName.length() - 1);
  while(fnl > -1 && fileName[fnl] != '/') {
    --fnl;
  }

  if(animating2d) {
    animFrames = GetFileCount(); 
    BL_ASSERT(dataServicesPtr.size() == animFrames);
    fileNames.resize(animFrames);
    for(i = 0; i < animFrames; ++i) {
      fileNames[i] = GetComlineFilename(i); 
    }

    // If this is an animation, putting the time in the title bar wouldn't
    // make much sense.  Instead, we simply indicate that this is an
    // animation, and indicate the time underneath the file label in the
    // controls window.
    headerout << &fileName[fnl+1] << ", 2D Animation" << ends;
  } else {
    animFrames = 1;
    fileNames.resize(animFrames);
    fileNames[currentFrame] = fileName;

    // Single plot file, so the time is indicated in the title bar.
    headerout << &fileName[fnl+1] << ", T=" << amrData.Time() << ends;
  }

  pltAppState = new PltAppState(animFrames, amrData.NumDeriveFunc());
  pltAppState->SetContourType(RASTERONLY);
  pltAppState->SetCurrentFrame(currentFrame);

  // Set the delete response to DO_NOTHING so that it can be app defined.
  wAmrVisTopLevel = XtVaCreatePopupShell(header, 
			 topLevelShellWidgetClass, wTopLevel,
			 XmNwidth,	    initialWindowWidth,
			 XmNheight,	    initialWindowHeight,
			 XmNx,		    100+placementOffsetX,
			 XmNy,		    200+placementOffsetY,
			 XmNdeleteResponse, XmDO_NOTHING,
			 NULL);

  GAptr = new GraphicsAttributes(wAmrVisTopLevel);
  display = GAptr->PDisplay();
  xgc = GAptr->PGC();
  if(GAptr->PVisual() != XDefaultVisual(display, GAptr->PScreenNumber())) {
    XtVaSetValues(wAmrVisTopLevel, XmNvisual, GAptr->PVisual(),
		  XmNdepth, GAptr->PDepth(), NULL);
  }

  if( ! dataServicesPtr[currentFrame]->CanDerive(PltApp::initialDerived)) {
    cerr << "Bad initial derived:  cannot derive "
	 << PltApp::initialDerived << endl;
    cerr << "defaulting to "
	 << dataServicesPtr[currentFrame]->PlotVarNames()[0] << endl;
    SetInitialDerived(dataServicesPtr[currentFrame]->PlotVarNames()[0]);
  }

  pltAppState->SetCurrentDerived(PltApp::initialDerived,
				 amrData.StateNumber(initialDerived));
  pltAppState->SetShowingBoxes(GetDefaultShowBoxes());
  int finestLevel(amrData.FinestLevel());
  pltAppState->SetFinestLevel(finestLevel);
  int maxlev = DetermineMaxAllowableLevel(amrData.ProbDomain()[finestLevel],
			       finestLevel, MaxPictureSize(), amrData.RefRatio());
  int minAllowableLevel(0);
  pltAppState->SetMinAllowableLevel(minAllowableLevel);
  pltAppState->SetMaxAllowableLevel(maxlev);
  pltAppState->SetMinDrawnLevel(minAllowableLevel);
  pltAppState->SetMaxDrawnLevel(maxlev);
  Box maxDomain(amrData.ProbDomain()[maxlev]);
  unsigned long dataSize(maxDomain.length(XDIR) * maxDomain.length(YDIR));
  if(MaxPictureSize() == 0) {
    maxAllowableScale = 1;
  } else  {
    maxAllowableScale = (int) sqrt((Real) (MaxPictureSize()/dataSize));
  }

  int currentScale(max(1, min(GetInitialScale(), maxAllowableScale)));
  pltAppState->SetCurrentScale(currentScale);
  pltAppState->SetMaxScale(maxAllowableScale);

// ---------------
  pltAppState->SetMinMaxRangeType(GLOBALMINMAX);
  Real rGlobalMin, rGlobalMax;
  rGlobalMin =  AV_BIG_REAL;
  rGlobalMax = -AV_BIG_REAL;
  int coarseLevel(0);
  int iCDerNum(pltAppState->CurrentDerivedNumber());
  aString asCDer(pltAppState->CurrentDerived());
  int fineLevel(pltAppState->MaxAllowableLevel());
  const Array<Box> &onBox(amrData.ProbDomain());
  for(int iFrame(0); iFrame < animFrames; ++iFrame) {
    Real rFileMin, rFileMax;

    FindAndSetMinMax(FILEGLOBALMINMAX, iFrame, asCDer, iCDerNum,
		     onBox, coarseLevel, fineLevel, false);  // dont reset if set
    pltAppState->GetMinMax(FILEGLOBALMINMAX, iFrame, iCDerNum,
			   rFileMin, rFileMax);
    rGlobalMin = min(rFileMin, rGlobalMin);
    rGlobalMax = max(rFileMax, rGlobalMax);

    // also set FILESUBREGIONMINMAXs and FILEUSERMINMAXs to the global values
    pltAppState->SetMinMax(FILESUBREGIONMINMAX, iFrame,
			   pltAppState->CurrentDerivedNumber(),
			   rFileMin, rFileMax);
    pltAppState->SetMinMax(FILEUSERMINMAX, iFrame,
			   pltAppState->CurrentDerivedNumber(),
			   rFileMin, rFileMax);
  }  // end for(iFrame...)

  // now set global values for each file in the animation
  for(int iFrame(0); iFrame < animFrames; ++iFrame) {
    // set GLOBALMINMAXs
    pltAppState->SetMinMax(GLOBALMINMAX, iFrame,
			   pltAppState->CurrentDerivedNumber(),
			   rGlobalMin, rGlobalMax);
    // also set SUBREGIONMINMAXs and USERMINMAXs to the global values
    pltAppState->SetMinMax(SUBREGIONMINMAX, iFrame,
			   pltAppState->CurrentDerivedNumber(),
			   rGlobalMin, rGlobalMax);
    pltAppState->SetMinMax(USERMINMAX, iFrame,
			   pltAppState->CurrentDerivedNumber(),
			   rGlobalMin, rGlobalMax);
  }  // end for(iFrame...)

  if(UseSpecifiedMinMax()) {
    pltAppState->SetMinMaxRangeType(USERMINMAX);
    Real specifiedMin, specifiedMax;
    GetSpecifiedMinMax(specifiedMin, specifiedMax);
    for(int iFrame(0); iFrame < animFrames; ++iFrame) {
      pltAppState->SetMinMax(USERMINMAX, iFrame,
			     pltAppState->CurrentDerivedNumber(),
			     specifiedMin, specifiedMax);
      pltAppState->SetMinMax(FILEUSERMINMAX, iFrame,
			     pltAppState->CurrentDerivedNumber(),
			     specifiedMin, specifiedMax);
    }
  }
// ---------------


  amrPicturePtrArray[ZPLANE] = new AmrPicture(GAptr, this, pltAppState,
					dataServicesPtr[currentFrame],
					bCartGridSmoothing);
#if (BL_SPACEDIM == 3)
  amrPicturePtrArray[YPLANE] = new AmrPicture(YPLANE, GAptr,
					amrData.ProbDomain()[finestLevel],
		                        //amrPicturePtrArray[ZPLANE],
					NULL, this,
					pltAppState,
					bCartGridSmoothing);
  amrPicturePtrArray[XPLANE] = new AmrPicture(XPLANE, GAptr,
					amrData.ProbDomain()[finestLevel],
		                        //amrPicturePtrArray[ZPLANE],
					NULL, this,
					pltAppState,
					bCartGridSmoothing);
#endif

  for(i = 0; i != BL_SPACEDIM; ++i) {
    ivLowOffsetMAL.setVal(i, amrData.ProbDomain()[maxlev].smallEnd(i));
  }

  contourNumString = aString("10");
  pltAppState->SetNumContours(10);

  palFilename = PltApp::defaultPaletteString;
  lightingFilename = PltApp::defaultLightingFilename;
  PltAppInit();
}


// -------------------------------------------------------------------
PltApp::PltApp(XtAppContext app, Widget w, const Box &region,
	       const IntVect &offset,
	       //AmrPicture *parentPtr,
	       PltApp *pltParent, const aString &palfile,
	       bool isAnim, const aString &newderived, const aString &filename)
  : paletteDrawn(false),
    appContext(app),
    wTopLevel(w),
    fileName(filename),
    fileNames(pltParent->fileNames),
    animFrames(pltParent->animFrames),
    dataServicesPtr(pltParent->dataServicesPtr),
    animating2d(isAnim),
    currentFrame(pltParent->currentFrame),
    palFilename(palfile),
    lightingFilename(pltParent->lightingFilename)
{
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  bFileRangeButtonSet = pltParent->bFileRangeButtonSet;

  pltAppState = new PltAppState(animFrames, amrData.NumDeriveFunc());
  *pltAppState = *pltParent->GetPltAppState();

#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  lightingWindowExists = false;
#endif
  contourNumString = pltParent->contourNumString.c_str();

  char header[BUFSIZ];

  bCartGridSmoothing = pltParent->bCartGridSmoothing;
  int finestLevel(amrData.FinestLevel());
  pltAppState->SetFinestLevel(finestLevel);
  int maxlev = DetermineMaxAllowableLevel(region, finestLevel,
					  MaxPictureSize(), amrData.RefRatio());
  int minAllowableLevel = amrData.FinestContainingLevel(region, finestLevel);

  pltAppState->SetMinAllowableLevel(minAllowableLevel);
  pltAppState->SetMaxAllowableLevel(maxlev);

  Box maxDomain(region);
  if(maxlev < finestLevel) {
    maxDomain.coarsen(CRRBetweenLevels(maxlev, finestLevel, amrData.RefRatio()));
  }

  unsigned long dataSize(maxDomain.length(XDIR) * maxDomain.length(YDIR));
  if(MaxPictureSize() / dataSize == 0) {
    maxAllowableScale = 1;
  } else {
    maxAllowableScale = (int) sqrt((Real) (MaxPictureSize()/dataSize));
  }

  int currentScale = min(maxAllowableScale,
			 pltParent->GetPltAppState()->CurrentScale());
  pltAppState->SetCurrentScale(currentScale);
  
// ---------------
  Array<Box> onBox(pltAppState->MaxAllowableLevel() + 1);
  onBox[pltAppState->MaxAllowableLevel()] = maxDomain;
  for(int ilev(pltAppState->MaxAllowableLevel() - 1); ilev >= 0; --ilev) {
    Box tempbox(maxDomain);
    tempbox.coarsen(CRRBetweenLevels(ilev, finestLevel, amrData.RefRatio()));
    onBox[ilev] = tempbox;
  }
  int iCDerNum(pltAppState->CurrentDerivedNumber());
  const aString asCDer(pltAppState->CurrentDerived());
  int coarseLevel(0);
  int fineLevel(pltAppState->MaxAllowableLevel());
  Real rSMin, rSMax;
  rSMin = AV_BIG_REAL;
  rSMax = -AV_BIG_REAL;
  for(int iFrame(0); iFrame < animFrames; ++iFrame) {
    // GLOBALMINMAX is already set to parent's values
    // USERMINMAX is already set to parent's values
    // FILEGLOBALMINMAX is already set to parent's values
    // FILEUSERMINMAX is already set to parent's values
    // these do not change for a subregion

    // set FILESUBREGIONMINMAX
    FindAndSetMinMax(FILESUBREGIONMINMAX, iFrame, asCDer, iCDerNum, onBox,
	             coarseLevel, fineLevel, true);  // reset if already set

    Real rTempMin, rTempMax;
    pltAppState->GetMinMax(FILESUBREGIONMINMAX, iFrame, iCDerNum,
			   rTempMin, rTempMax);
    // collect file values
    rSMin = min(rSMin, rTempMin);
    rSMax = max(rSMax, rTempMax);
  }
  // now set each frame's SUBREGIONMINMAX to the overall subregion minmax
  for(int iFrame(0); iFrame < animFrames; ++iFrame) {
    pltAppState->SetMinMax(FILEUSERMINMAX, iFrame, iCDerNum, rSMin, rSMax);
  }
// ---------------

  int fnl(fileName.length() - 1);
  while (fnl>-1 && fileName[fnl] != '/') {
    --fnl;
  }

  char timestr[20];
  // If animating, do not indicate the time.
  if(animating2d) {
    timestr[0] = '\0';
  } else {
    ostrstream timeOut(timestr, 20);
    timeOut << " T = " << amrData.Time() << ends;
  }

  ostrstream headerout(header, BUFSIZ);
    
  headerout << &fileName[fnl+1] << timestr << "  Subregion:  "
	    << maxDomain << "  on level " << maxlev << ends;
				
  wAmrVisTopLevel = XtVaCreatePopupShell(header, 
					 topLevelShellWidgetClass, wTopLevel,
					 XmNwidth,	    initialWindowWidth,
					 XmNheight,	    initialWindowHeight,
					 XmNx,		    120+placementOffsetX,
					 XmNy,		    220+placementOffsetY,
					 XmNdeleteResponse, XmDO_NOTHING,			 
					 NULL);

  if(Verbose()) {
    cout << "_in PltApp::PltApp:  subregion" << endl;
    pltAppState->PrintSetMap();  cout << endl;
  }

  GAptr = new GraphicsAttributes(wAmrVisTopLevel);
  display = GAptr->PDisplay();
  xgc = GAptr->PGC();

  if(GAptr->PVisual() != XDefaultVisual(display, GAptr->PScreenNumber())) {
      XtVaSetValues(wAmrVisTopLevel, XmNvisual, GAptr->PVisual(),
                    XmNdepth, 8, NULL);
  }
  pltAppState->SetMinDrawnLevel(minAllowableLevel);
  for(int np(0); np < NPLANES; ++np) {
    amrPicturePtrArray[np] = new AmrPicture(np, GAptr, region,
					    //parentPtr,
					    pltParent, this,
					    pltAppState,
					    bCartGridSmoothing);
  }
  
#if (BL_SPACEDIM == 3)
  for(int i(0); i < 3; ++i) {
   amrPicturePtrArray[i]->SetHVLine(pltAppState->CurrentScale());
  }
#endif
  ivLowOffsetMAL = offset;
  PltAppInit();
}


// -------------------------------------------------------------------
void PltApp::PltAppInit() {
  int np;
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  int minAllowableLevel(pltAppState->MinAllowableLevel());
  int maxAllowableLevel(pltAppState->MaxAllowableLevel());
  wRangeRadioButton.resize(BNUMBEROFMINMAX);

  // User defined widget destroy callback -- will free all memory used to create
  // window.
  XmAddWMProtocolCallback(wAmrVisTopLevel,
			  XmInternAtom(display,"WM_DELETE_WINDOW", false),
			  (XtCallbackProc) CBQuitPltApp, (XtPointer) this);

  for(np = 0; np != BL_SPACEDIM; ++np) {
    XYplotwin[np] = NULL; // No 1D plot windows initially.

    // For speed (and clarity!) we store the values of the finest value of h of
    // each dimension, as well as the low value of the problem domain in simple
    // arrays.  These are both in problem space.
    finestDx[np] = amrData.DxLevel()[maxAllowableLevel][np];
    gridOffset[np] = amrData.ProbLo()[np];
  }

  placementOffsetX += 20;
  placementOffsetY += 20;
  
  servingButton = 0;
  activeView = ZPLANE;
  int maxDrawnLevel = maxAllowableLevel;
  startX = 0;
  startY = 0;
  endX = 0;
  endY = 0;

  //currentFrame = 0;
  animationIId = 0;
  frameSpeed = 300;

  readyFrames.resize(animFrames, false);
  frameBuffer.resize(animFrames);

  selectionBox.convert(amrData.ProbDomain()[0].type());
  selectionBox.setSmall(IntVect::TheZeroVector());
  selectionBox.setBig(IntVect::TheZeroVector());

  for(np = 0; np != NPLANES; ++np) {
    amrPicturePtrArray[np]->SetFrameSpeed(frameSpeed);
    amrPicturePtrArray[np]->SetRegion(startX, startY, endX, endY);
  }

  char tempFormat[32];
  strcpy(tempFormat, PltApp::initialFormatString.c_str());
  XmString sFormatString = XmStringCreateSimple(tempFormat);
  char *tempchar = new char[LINELENGTH];
  XmStringGetLtoR(sFormatString, XmSTRING_DEFAULT_CHARSET, &tempchar);
  XmStringFree(sFormatString);
  pltAppState->SetFormatString(tempchar);
  delete [] tempchar;

  infoShowing = false;
  contoursShowing = false;
  setRangeShowing = false;
  bSetRangeRedraw = false;
  datasetShowing = false;
  bFormatShowing = false;
  writingRGB = false;
			  
  int palListLength(PALLISTLENGTH);
  int palWidth(PALWIDTH);
  int totalPalWidth(TOTALPALWIDTH);
  int totalPalHeight(TOTALPALHEIGHT);
  pltPaletteptr = new Palette(wTopLevel, palListLength, palWidth,
			      totalPalWidth, totalPalHeight,
			      reserveSystemColors);
  
  // gc for gxxor rubber band line drawing
  rbgc = XCreateGC(display, GAptr->PRoot(), 0, NULL);
  XSetFunction(display, rbgc, GXxor);
  cursor = XCreateFontCursor(display, XC_left_ptr);

  // No need to store these widgets in the class after this function is called.
  Widget wMainArea, wPalFrame, wPlotFrame, wPalForm;

  wMainArea = XtVaCreateManagedWidget("MainArea", xmFormWidgetClass,
				      wAmrVisTopLevel, NULL);

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

  // To look at a subregion of the plot
  label_str = XmStringCreateSimple("Ctrl+S");
  wid =
    XtVaCreateManagedWidget((BL_SPACEDIM != 3) ? "Subregion..." : "Subvolume...",
			    xmPushButtonGadgetClass, wMenuPulldown,
			    XmNmnemonic, 'S',
			    XmNaccelerator, "Ctrl<Key>S",
			    XmNacceleratorText, label_str,
			    NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoSubregion);

  // To change the palette
  wid = XtVaCreateManagedWidget("Palette...", xmPushButtonGadgetClass,
				wMenuPulldown, XmNmnemonic,  'P', NULL);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoPaletteButton);

  // To output to various formats (.ps, .rgb, .fab)
  wCascade = XmCreatePulldownMenu(wMenuPulldown, "outputmenu", NULL, 0);
  XtVaCreateManagedWidget("Export", xmCascadeButtonWidgetClass, wMenuPulldown,
			  XmNmnemonic, 'E', XmNsubMenuId, wCascade, NULL);
  wid = XtVaCreateManagedWidget("PS File...", xmPushButtonGadgetClass,
				wCascade, XmNmnemonic, 'P', NULL);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoOutput, (XtPointer) 0);
  wid = XtVaCreateManagedWidget("RGB File...", xmPushButtonGadgetClass,
				wCascade, XmNmnemonic, 'R', NULL);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoOutput, (XtPointer) 1);
  wid = XtVaCreateManagedWidget("FAB File...", xmPushButtonGadgetClass,
				wCascade, XmNmnemonic, 'F', NULL);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoOutput, (XtPointer) 2);

  // Close
  XtVaCreateManagedWidget(NULL, xmSeparatorGadgetClass, wMenuPulldown, NULL);
  label_str = XmStringCreateSimple("Ctrl+C");
  wid = XtVaCreateManagedWidget("Close", xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'C',
				XmNaccelerator, "Ctrl<Key>C",
				XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  XtAddCallback(wid, XmNactivateCallback, (XtCallbackProc) CBQuitPltApp,
		(XtPointer) this);
  
  // ------------------------------- view menu
  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, "MenuPulldown", NULL, 0);
  XtVaCreateManagedWidget("View", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'V', XmNsubMenuId, wMenuPulldown, NULL);

  // To scale the raster / contour windows
  int maxallow(min(MAXSCALE, maxAllowableScale));
  wCascade = XmCreatePulldownMenu(wMenuPulldown, "scalemenu", NULL, 0);
  XtVaCreateManagedWidget("Scale", xmCascadeButtonWidgetClass, wMenuPulldown,
			  XmNmnemonic, 'S', XmNsubMenuId, wCascade, NULL);
  for(unsigned long scale(1); scale <= maxallow; ++scale) {
    sprintf(selectText, "%ix", scale);
    wid = XtVaCreateManagedWidget(selectText, xmToggleButtonGadgetClass, wCascade,
				  XmNset, false, NULL);
    if(scale <= 10) {
      // scale values <= 10 are shortcutted by pressing the number 1-0
      sprintf(accel, "<Key>%i", scale % 10);
      sprintf(accelText, "%i", scale % 10);
      label_str = XmStringCreateSimple(accelText);
      XtVaSetValues(wid,
		    XmNmnemonic, scale + 'O',
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
    if(scale == pltAppState->CurrentScale()) {
      // Toggle buttons indicate which is the current scale
      XtVaSetValues(wid, XmNset, true, NULL);
      wCurrScale = wid;
    }
    AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::ChangeScale,
		      (XtPointer) scale);
  }

  // Levels button, to view various levels of refinement
  //int numberOfLevels(amrPicturePtrArray[ZPLANE]->NumberOfLevels()-minAllowableLevel);
  wCascade = XmCreatePulldownMenu(wMenuPulldown, "levelmenu", NULL, 0);
  XtVaCreateManagedWidget("Level", xmCascadeButtonWidgetClass, wMenuPulldown,
			  XmNmnemonic, 'L', XmNsubMenuId, wCascade, NULL);
  for(int menuLevel(minAllowableLevel); menuLevel <= maxDrawnLevel; ++menuLevel) {
    sprintf(selectText, "%i/%i", menuLevel, amrData.FinestLevel());
    wid = XtVaCreateManagedWidget(selectText, xmToggleButtonGadgetClass, wCascade,
				  XmNset, false, NULL);
    if(menuLevel <= 10) {
      // Levels <= 10 are shortcutted by holding down the CTRL key and pressing 1-0
      sprintf(accel, "Ctrl<Key>%i", menuLevel % 10);
      sprintf(accelText, "Ctrl+%i", menuLevel % 10);
      label_str = XmStringCreateSimple(accelText);
      XtVaSetValues(wid,
		    XmNmnemonic, menuLevel + '0',
		    XmNaccelerator, accel,
		    XmNacceleratorText, label_str,
		    NULL);			      
      XmStringFree(label_str);
    }
    if(menuLevel - minAllowableLevel == maxDrawnLevel) {
      // Toggle button indicates current level in view
      XtVaSetValues(wid, XmNset, true, NULL);
      wCurrLevel = wid;
    }
    AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::ChangeLevel,
		      (XtPointer)(menuLevel - minAllowableLevel));
  }

  // Button to create (or pop up) a dialog to set contour settings
  label_str = XmStringCreateSimple("C");
  wid = XtVaCreateManagedWidget("Contours...",
				xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'C',
				// XmNaccelerator, "<Key>C",
				// XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoContoursButton);

  // Button to create (or pop up) a dialog to set range
  label_str = XmStringCreateSimple("R");
  wid = XtVaCreateManagedWidget("Range...",
				xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'R',
				// XmNaccelerator, "<Key>R",
				// XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoSetRangeButton);

  // To create (or update and pop up) a dialog indicating the data values
  // of a selected region
  label_str = XmStringCreateSimple("D");
  wid = XtVaCreateManagedWidget("Dataset...",
				xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'D',
				// XmNaccelerator, "<Key>D",
				// XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoDatasetButton);

  // To change the number format string
  label_str = XmStringCreateSimple("F");
  wid = XtVaCreateManagedWidget("Number Format...",
				xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'F',
				// XmNaccelerator, "<Key>F",
				// XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoNumberFormatButton);

  XtVaCreateManagedWidget(NULL, xmSeparatorGadgetClass, wMenuPulldown,
			  NULL);

  // Toggle viewing the boxes
  label_str = XmStringCreateSimple("B");
  wid = XtVaCreateManagedWidget("Boxes",
				xmToggleButtonGadgetClass, wMenuPulldown,
				XmNmnemonic, 'B',
				XmNset, pltAppState->GetShowingBoxes(),
				// XmNaccelerator, "<Key>B",
				// XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::DoBoxesButton);

  // ------------------------------- derived menu
  int maxMenuItems(20);  // arbitrarily
  int numberOfDerived(dataServicesPtr[currentFrame]->NumDeriveFunc());
  const Array<aString> &derivedStrings =
	     dataServicesPtr[currentFrame]->PlotVarNames();

  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, "DerivedPulldown", NULL, 0);
  XtVaSetValues(wMenuPulldown,
		XmNpacking, XmPACK_COLUMN,
		XmNnumColumns, numberOfDerived / maxMenuItems +
		((numberOfDerived % maxMenuItems == 0) ? 0 : 1), NULL);
  XtVaCreateManagedWidget("Variable", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'a', XmNsubMenuId,   wMenuPulldown, NULL);
  //unsigned long cderived(0);
  wCurrDerived = XtVaCreateManagedWidget(derivedStrings[0].c_str(),
					 xmToggleButtonGadgetClass, wMenuPulldown,
					 XmNset, true, NULL);
  AddStaticCallback(wCurrDerived, XmNvalueChangedCallback, &PltApp::ChangeDerived,
		    (XtPointer) 0);
  for(unsigned long derived(1); derived < numberOfDerived; ++derived) {
    wid = XtVaCreateManagedWidget(derivedStrings[derived].c_str(),
				  xmToggleButtonGadgetClass, wMenuPulldown,
				  XmNset, false, NULL);
    if(derivedStrings[derived] == pltAppState->CurrentDerived()) {
      XtVaSetValues(wCurrDerived, XmNset, false, NULL);
      XtVaSetValues(wid, XmNset, true, NULL);
      wCurrDerived = wid;
      //cderived = derived;
    }
    AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::ChangeDerived,
		      (XtPointer) derived);
  }  

#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  lightingModel = true;
  showing3dRender = false;
  preClassify = true;

  // RENDER MENU

  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, "RenderPulldown", NULL, 0);
  XtVaCreateManagedWidget("Render", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'R', XmNsubMenuId, wMenuPulldown, NULL);

  wAutoDraw =
    XtVaCreateManagedWidget("Autodraw", xmToggleButtonGadgetClass, wMenuPulldown,
			    XmNmnemonic, 'A', XmNset, false, NULL);
  AddStaticCallback(wAutoDraw, XmNvalueChangedCallback, &PltApp::DoAutoDraw);
  
  wid = XtVaCreateManagedWidget("Lighting...",
				xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic, 'L', NULL);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoCreateLightingWindow);
  
  wCascade = XmCreatePulldownMenu(wMenuPulldown, "rendermodemenu", NULL, 0);
  XtVaCreateManagedWidget("Render Mode", xmCascadeButtonWidgetClass, wMenuPulldown,
			  XmNmnemonic, 'M', XmNsubMenuId, wCascade, NULL);
  wid = XtVaCreateManagedWidget("Light", xmToggleButtonGadgetClass,
				wCascade, XmNset, true, NULL);
  AddStaticCallback(wid, XmNvalueChangedCallback,
		    &PltApp::DoRenderModeMenu, (XtPointer) 0);
  wCurrentRenderMode = wid;
  wid = XtVaCreateManagedWidget("Value", xmToggleButtonGadgetClass,
				wCascade, XmNset, false, NULL);
  AddStaticCallback(wid, XmNvalueChangedCallback,
		    &PltApp::DoRenderModeMenu, (XtPointer) 1);
  
  wCascade = XmCreatePulldownMenu(wMenuPulldown, "classifymenu", NULL, 0);
  XtVaCreateManagedWidget("Classify", xmCascadeButtonWidgetClass, wMenuPulldown,
			  XmNmnemonic, 'C', XmNsubMenuId, wCascade, NULL);
  wid = XtVaCreateManagedWidget("PC", xmToggleButtonGadgetClass,
				wCascade, XmNset, true, NULL);
  AddStaticCallback(wid, XmNvalueChangedCallback,
		    &PltApp::DoClassifyMenu, (XtPointer) 0);
  wCurrentClassify = wid;

  wid = XtVaCreateManagedWidget("OT", xmToggleButtonGadgetClass,
				wCascade, XmNset, false, NULL);
  AddStaticCallback(wid, XmNvalueChangedCallback,
		    &PltApp::DoClassifyMenu, (XtPointer) 1);

#endif

  // --------------------------------------------------------------- help menu
  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, "MenuPulldown", NULL, 0);
  XtVaCreateManagedWidget("Help", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'H', XmNsubMenuId,   wMenuPulldown, NULL);
  wid = XtVaCreateManagedWidget("Info...", xmPushButtonGadgetClass,
				wMenuPulldown, XmNmnemonic,  'I', NULL);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoInfoButton);
  
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
			    // XmNx,	         690,
			    // XmNy,		 0,
			    XmNwidth,		 150,
			    XmNheight,		 290,
			    NULL);
  AddStaticEventHandler(wPalArea, ExposureMask, &PltApp::DoExposePalette);

  // Indicate the unit type of the palette (legend) area above it.
  strcpy(buffer, pltAppState->CurrentDerived().c_str());
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
			    XmNbackground,       pltPaletteptr->BlackIndex(),
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

  unsigned long wc;
  int wcfWidth = 150, wcfHeight = 260;
  int centerX = wcfWidth/2, centerY = wcfHeight / 2 - 16;
  int controlSize = 16;
  int halfbutton = controlSize/2;
  wControlForm = XtVaCreateManagedWidget("refArea",
			    xmDrawingAreaWidgetClass, wControlsFrame,
			    XmNwidth,	wcfWidth,
			    XmNheight,	wcfHeight,
			    XmNresizePolicy, XmRESIZE_NONE,
			    NULL);
  AddStaticEventHandler(wControlForm, ExposureMask, &PltApp::DoExposeRef);

#if (BL_SPACEDIM == 3)

  wControls[WCSTOP] =
    XtVaCreateManagedWidget("0", xmPushButtonWidgetClass, wControlForm,
			    XmNx, centerX-halfbutton,
			    XmNy, centerY-halfbutton,
			    XmCMarginBottom, 2,
			    NULL);
  XtManageChild(wControls[WCSTOP]);
  
  wControls[WCXNEG] =
    XtVaCreateManagedWidget("wcxneg", xmArrowButtonWidgetClass, wControlForm,
			    XmNarrowDirection,      XmARROW_LEFT,
			    XmNx, centerX+halfbutton,
			    XmNy, centerY-halfbutton,
			    NULL);
  wControls[WCXPOS] =
    XtVaCreateManagedWidget("wcxpos", xmArrowButtonWidgetClass, wControlForm,
			    XmNarrowDirection,      XmARROW_RIGHT,
			    XmNx, centerX+3*halfbutton,
			    XmNy, centerY-halfbutton,
			    NULL);
  wControls[WCYNEG] =
    XtVaCreateManagedWidget("wcyneg", xmArrowButtonWidgetClass, wControlForm,
			    XmNarrowDirection,      XmARROW_DOWN,
			    XmNx, centerX-halfbutton,
			    XmNy, centerY-3*halfbutton,
			    NULL);
  wControls[WCYPOS] =
    XtVaCreateManagedWidget("wcypos", xmArrowButtonWidgetClass, wControlForm,
			    XmNarrowDirection,      XmARROW_UP,
			    XmNx, centerX-halfbutton,
			    XmNy, centerY-5*halfbutton,
			    NULL);
  wControls[WCZNEG] =
    XtVaCreateManagedWidget("wczneg", xmArrowButtonWidgetClass, wControlForm,
			    XmNarrowDirection,      XmARROW_RIGHT,
			    XmNx, centerX-3*halfbutton+1,
			    XmNy, centerY+halfbutton-1,
			    NULL);
  wControls[WCZPOS] =
    XtVaCreateManagedWidget("wczpos", xmArrowButtonWidgetClass, wControlForm,
			    XmNarrowDirection,      XmARROW_LEFT,
			    XmNx, centerX-5*halfbutton+2,
			    XmNy, centerY+3*halfbutton-2,
			    NULL);
  
  for(wc = WCSTOP; wc <= WCZPOS; ++wc) {
    XtVaSetValues(wControls[wc],
		  XmNwidth, controlSize,
		  XmNheight, controlSize,
		  XmNborderWidth, 0,
		  XmNhighlightThickness, 0,
		  NULL); 
    AddStaticCallback(wControls[wc], XmNactivateCallback, &PltApp::ChangePlane,
		      (XtPointer) wc);
  }
  
#endif
  int adjustHeight2D = (BL_SPACEDIM == 2) ? centerY : 0;
  Dimension slw;
  
  if(animating2d) {
    wControls[WCASTOP] =
      XtVaCreateManagedWidget("0", xmPushButtonWidgetClass, wControlForm,
			      XmNx, centerX-halfbutton,
			      XmCMarginBottom, 2,
			      NULL);
    //XtManageChild(wControls[WCASTOP]);
    
    wControls[WCATNEG] =
      XtVaCreateManagedWidget("wcatneg", xmArrowButtonWidgetClass, wControlForm,
			      XmNarrowDirection,      XmARROW_LEFT,
			      XmNx, centerX-3*halfbutton,
			      NULL);
    wControls[WCATPOS] =
      XtVaCreateManagedWidget("wcatpos", xmArrowButtonWidgetClass, wControlForm,
			      XmNarrowDirection,      XmARROW_RIGHT,
			      XmNx, centerX+halfbutton,
			      NULL);
    for(wc = WCATNEG; wc <= WCATPOS; ++wc) {
      XtVaSetValues(wControls[wc], XmNwidth,  controlSize, XmNheight, controlSize,
		    XmNborderWidth, 0, XmNhighlightThickness, 0,
		    XmNy, wcfHeight-adjustHeight2D, NULL); 
      AddStaticCallback(wControls[wc], XmNactivateCallback, &PltApp::ChangePlane,
			(XtPointer) wc);
    }

    wControls[WCARGB] =
      XtVaCreateManagedWidget("rgb >", xmPushButtonWidgetClass, wControlForm,
			      XmCMarginBottom, 2,
			      XmNwidth,  3*controlSize,
			      XmNheight, controlSize,
			      XmNborderWidth, 0,
			      XmNhighlightThickness, 0,
			      XmNx, centerX-3*halfbutton,
			      XmNy, wcfHeight+3*halfbutton-adjustHeight2D,
			      NULL);
    AddStaticCallback(wControls[WCARGB], XmNactivateCallback, &PltApp::ChangePlane,
		      (XtPointer) WCARGB);
    //XtManageChild(wControls[WCARGB]);

    wWhichFileScale =
      XtVaCreateManagedWidget("whichFileScale", xmScaleWidgetClass, wControlForm,
			      XmNminimum,     0,
			      XmNmaximum,     animFrames-1,
			      XmNvalue,	      currentFrame,
			      XmNorientation, XmHORIZONTAL,
			      XmNx,           0,
			      XmNscaleWidth,  wcfWidth * 2 / 3,
			      XmNy,           wcfHeight-7*halfbutton-adjustHeight2D,
			      NULL);
    AddStaticCallback(wWhichFileScale, XmNvalueChangedCallback,
		      &PltApp::DoAnimFileScale);
    
    Widget wAnimLabelFast =
      XtVaCreateManagedWidget("File", xmLabelWidgetClass, wControlForm,	
			      XmNy, wcfHeight-7*halfbutton-adjustHeight2D,
			      NULL);
    XtVaGetValues(wAnimLabelFast, XmNwidth, &slw, NULL);
    XtVaSetValues(wAnimLabelFast, XmNx, wcfWidth-slw, NULL);
    
    int fnl(fileNames[currentFrame].length() - 1);
    while(fnl > -1 && fileNames[currentFrame][fnl] != '/') {
      --fnl;
    }

    XmString fileString = XmStringCreateSimple(&fileNames[currentFrame][fnl+1]);
    wWhichFileLabel = XtVaCreateManagedWidget("whichFileLabel",
			      xmLabelWidgetClass, wControlForm,	
			      XmNx, 0,
			      XmNy, wcfHeight -4*halfbutton-3-adjustHeight2D,
			      XmNlabelString,         fileString,
			      NULL);
    XmStringFree(fileString);

    // We add a which-time label to indicate the time of each plot file.
    char tempTimeName[LINELENGTH];
    ostrstream tempTimeOut(tempTimeName, LINELENGTH);
    tempTimeOut << "T=" << amrData.Time() << ends;
    XmString timeString = XmStringCreateSimple(tempTimeName);
    wWhichTimeLabel = XtVaCreateManagedWidget("whichTimeLabel",
			      xmLabelWidgetClass, wControlForm,
			      XmNx, 0,
			      XmNy, wcfHeight - 2*halfbutton-3-adjustHeight2D,
			      XmNlabelString, timeString,
			      NULL);
    XmStringFree(timeString);
    
  }  // end if(animating2d)

  if(animating2d || BL_SPACEDIM == 3) {   
    Widget wSpeedScale =
      XtVaCreateManagedWidget("speed", xmScaleWidgetClass, wControlForm,
			      XmNminimum,		0, 
			      XmNmaximum,		599, 
			      XmNvalue,		        600 - frameSpeed,
			      XmNorientation,		XmHORIZONTAL,
			      XmNx,                     0,
			      XmNscaleWidth,  		wcfWidth * 2 / 3,
			      XmNy, wcfHeight - 9 * halfbutton - adjustHeight2D,
			      NULL);
    
    AddStaticCallback(wSpeedScale, XmNvalueChangedCallback, &PltApp::DoSpeedScale);
    //XtVaGetValues(wSpeedScale, XmNwidth, &slw, NULL);
    //XtVaSetValues(wSpeedScale, XmNx, (wcfWidth-slw) / 2, NULL);
    Widget wSpeedLabel =
      XtVaCreateManagedWidget("Speed", xmLabelWidgetClass, wControlForm,	
			      XmNy, wcfHeight - 9 * halfbutton - adjustHeight2D,
			      NULL);
    XtVaGetValues(wSpeedLabel, XmNwidth, &slw, NULL);
    XtVaSetValues(wSpeedLabel, XmNx, wcfWidth-slw, NULL);
     
  }

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

  wLocArea = XtVaCreateManagedWidget("locarea",xmDrawingAreaWidgetClass, wPicArea,
			    XmNleftAttachment,   XmATTACH_FORM,
			    XmNrightAttachment,  XmATTACH_FORM,
			    XmNbottomAttachment, XmATTACH_FORM,
			    XmNheight,	         30,
			    NULL);

  wPlotArea = XtVaCreateManagedWidget("plotarea", xmFormWidgetClass, wPicArea,
			    XmNtopAttachment,    XmATTACH_FORM,
			    XmNleftAttachment,   XmATTACH_FORM,
			    XmNrightAttachment,  XmATTACH_FORM,
			    XmNbottomAttachment, XmATTACH_WIDGET,
			    XmNbottomWidget,     wLocArea, NULL);

  wScrollArea[ZPLANE] = XtVaCreateManagedWidget("scrollAreaXY",
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
	
  wPlotPlane[ZPLANE] = XtVaCreateManagedWidget("plotArea",
			    xmDrawingAreaWidgetClass, wScrollArea[ZPLANE],
			    XmNtranslations,  XtParseTranslationTable(trans),
			    XmNwidth, amrPicturePtrArray[ZPLANE]->ImageSizeH() + 1,
			    XmNheight, amrPicturePtrArray[ZPLANE]->ImageSizeV() + 1,
			    NULL);
  XtVaSetValues(wScrollArea[ZPLANE], XmNworkWindow, wPlotPlane[ZPLANE], NULL); 
#if (BL_SPACEDIM == 2)
  XtVaSetValues(wScrollArea[ZPLANE], XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM, NULL);		
#elif (BL_SPACEDIM == 3)
  XtVaSetValues(wAmrVisTopLevel, XmNwidth, 1100, XmNheight, 650, NULL);
  
  XtVaSetValues(wScrollArea[ZPLANE], 
		XmNrightAttachment,	XmATTACH_POSITION,
		XmNrightPosition,	50,	// %
		XmNbottomAttachment,	XmATTACH_POSITION,
		XmNbottomPosition,	50,
		NULL);

// ********************************************************* XZ

  wScrollArea[YPLANE] =
    XtVaCreateManagedWidget("scrollAreaXZ",
			    xmScrolledWindowWidgetClass, wPlotArea,
			    XmNrightAttachment,	 XmATTACH_FORM,
			    XmNleftAttachment,	 XmATTACH_WIDGET,
			    XmNleftWidget,	 wScrollArea[ZPLANE],
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition,	 50,
			    XmNtopAttachment,	 XmATTACH_FORM,
			    XmNtopOffset,	 0,
			    XmNscrollingPolicy,	 XmAUTOMATIC,
			    NULL);
  
  wPlotPlane[YPLANE] = XtVaCreateManagedWidget("plotArea",
		         xmDrawingAreaWidgetClass, wScrollArea[YPLANE],
		         XmNtranslations, XtParseTranslationTable(trans),
		         XmNwidth,  amrPicturePtrArray[YPLANE]->ImageSizeH() + 1,
		         XmNheight, amrPicturePtrArray[YPLANE]->ImageSizeV() + 1,
		         NULL);
  XtVaSetValues(wScrollArea[YPLANE], XmNworkWindow, wPlotPlane[YPLANE], NULL); 
  
  // ********************************************************* YZ
  
  wScrollArea[XPLANE] =
    XtVaCreateManagedWidget("scrollAreaYZ",
			    xmScrolledWindowWidgetClass, wPlotArea,
			    XmNbottomAttachment,	XmATTACH_FORM,
			    XmNleftAttachment,	XmATTACH_FORM,
			    XmNrightAttachment,	XmATTACH_POSITION,
			    XmNrightPosition,	50,	// %
			    XmNtopAttachment,	XmATTACH_POSITION,
			    XmNtopPosition,		50,	// %
			    XmNscrollingPolicy,	XmAUTOMATIC,
			    NULL);
  
  wPlotPlane[XPLANE] = XtVaCreateManagedWidget("plotArea",
			    xmDrawingAreaWidgetClass, wScrollArea[XPLANE],
			    XmNtranslations,  XtParseTranslationTable(trans),
			    XmNwidth,  amrPicturePtrArray[XPLANE]->ImageSizeH() + 1,
			    XmNheight, amrPicturePtrArray[XPLANE]->ImageSizeV() + 1,
			    NULL);
  XtVaSetValues(wScrollArea[XPLANE], XmNworkWindow, wPlotPlane[XPLANE], NULL); 
  
  // ****************************************** Transform Area & buttons
  
  wOrient = XtVaCreateManagedWidget("0",
				    xmPushButtonGadgetClass, wPlotArea,
				    XmNleftAttachment, XmATTACH_WIDGET,
				    XmNleftWidget, wScrollArea[XPLANE],
				    XmNleftOffset, WOFFSET,
				    XmNtopAttachment, XmATTACH_POSITION,
				    XmNtopPosition, 50,
				    NULL);
  AddStaticCallback(wOrient, XmNactivateCallback, &PltApp::DoOrient);
  XtManageChild(wOrient);
  
  int whiteColor(pltPaletteptr->WhiteIndex());
  //XSetForeground(display, xgc, pltPaletteptr->pixelate(whiteColor));
  //XSetForeground(display, xgc, pltPaletteptr->pixelate(100));
  wLabelAxes = XtVaCreateManagedWidget("XYZ",
				       xmPushButtonGadgetClass, wPlotArea,
				       XmNleftAttachment, XmATTACH_WIDGET,
				       XmNleftWidget, wOrient,
				       XmNleftOffset, WOFFSET,
				       XmNtopAttachment, XmATTACH_POSITION,
				       XmNtopPosition, 50,
				       //XmNforeground, 42,
				       //XmNbackground, 100,
				       NULL);
  AddStaticCallback(wLabelAxes, XmNactivateCallback, &PltApp::DoLabelAxes);
  XtManageChild(wLabelAxes);
  
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  wRender = XtVaCreateManagedWidget("Draw",
				    xmPushButtonGadgetClass, wPlotArea,
				    XmNleftAttachment, XmATTACH_WIDGET,
				    XmNleftWidget, wLabelAxes,
				    XmNleftOffset, WOFFSET,
				    XmNtopAttachment, XmATTACH_POSITION,
				    XmNtopPosition, 50, NULL);
  AddStaticCallback(wRender, XmNactivateCallback, &PltApp::DoRender);
  XtManageChild(wRender);
#endif
  
  XmString sDetach = XmStringCreateSimple("Detach");
  wDetach =
    XtVaCreateManagedWidget("detach", xmPushButtonGadgetClass,
			    wPlotArea,
			    XmNlabelString, sDetach,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNrightOffset, WOFFSET,
			    XmNtopAttachment, XmATTACH_POSITION,
			    XmNtopPosition, 50,
			    NULL);
  XmStringFree(sDetach);
  AddStaticCallback(wDetach, XmNactivateCallback, &PltApp::DoDetach);
  XtManageChild(wDetach);
  
  wTransDA = XtVaCreateManagedWidget("transDA", xmDrawingAreaWidgetClass,
			     wPlotArea,
			     XmNtranslations, XtParseTranslationTable(trans),
			     XmNleftAttachment,	        XmATTACH_WIDGET,
			     XmNleftWidget,		wScrollArea[XPLANE],
			     XmNtopAttachment,	        XmATTACH_WIDGET,
			     XmNtopWidget,		wOrient,
			     XmNrightAttachment,	XmATTACH_FORM,
			     XmNbottomAttachment,	XmATTACH_FORM,
			     NULL);

#endif
  
  for(np = 0; np < NPLANES; ++np) {
    AddStaticCallback(wPlotPlane[np], XmNinputCallback,
		      &PltApp::DoRubberBanding, (XtPointer) np);
    AddStaticEventHandler(wPlotPlane[np], ExposureMask, &PltApp::PADoExposePicture,
			  (XtPointer) np);
    AddStaticEventHandler(wPlotPlane[np], PointerMotionMask | LeaveWindowMask,
			  &PltApp::DoDrawPointerLocation, (XtPointer) np);
  }
  
  
  // ***************************************************************** 
  XtManageChild(wPalArea);
  XtManageChild(wPlotArea);
  XtPopup(wAmrVisTopLevel, XtGrabNone);
  
  pltPaletteptr->SetWindow(XtWindow(wPalArea));
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPalArea));
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPlotArea));
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wAmrVisTopLevel));
  for(np = 0; np != NPLANES; ++np) {
    pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPlotPlane[np]));
  }
  
#if (BL_SPACEDIM == 3)
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wTransDA));

  Dimension width, height;
  XtVaGetValues(wTransDA, XmNwidth, &width, XmNheight, &height, NULL);
  daWidth  = width;
  daHeight = height;
  
  projPicturePtr = new ProjectionPicture(this, &viewTrans, pltPaletteptr,
					 wTransDA, daWidth, daHeight);
  AddStaticCallback(wTransDA, XmNinputCallback, &PltApp::DoTransInput);
  AddStaticCallback(wTransDA, XmNresizeCallback, &PltApp::DoTransResize);
  AddStaticEventHandler(wTransDA, ExposureMask, &PltApp::DoExposeTransDA);
  DoTransResize(wTransDA, NULL, NULL);
  
#endif

  char plottertitle[50];
  sprintf(plottertitle, "XYPlot%dd", BL_SPACEDIM);
  XYplotparameters = new XYPlotParameters(pltPaletteptr, GAptr, plottertitle);

  for(np = 0; np < NPLANES; ++np) {
    amrPicturePtrArray[np]->CreatePicture(XtWindow(wPlotPlane[np]),
					  pltPaletteptr);
  }

#if (BL_SPACEDIM == 3)
  viewTrans.MakeTransform();
  labelAxes = false;
  transDetached = false;
  
  for(np = 0; np < NPLANES; ++np) {
    startcutX[np] = 0;
    startcutY[np] = amrPicturePtrArray[np]->GetHLine();
    finishcutX[np] = 0;
    finishcutY[np] = amrPicturePtrArray[np]->GetHLine();
    amrPicturePtrArray[np]->ToggleShowSubCut();
  }

  projPicturePtr->ToggleShowSubCut(); 
  projPicturePtr->MakeBoxes(); 
#endif

  interfaceReady = true;

  cbdPtrs.reserve(512);  // arbitrarily
  
}  // end PltAppInit()


// -------------------------------------------------------------------
void PltApp::FindAndSetMinMax(const MinMaxRangeType mmrangetype,
			      const int framenumber,
		              const aString &currentderived,
			      const int derivednumber,
		              const Array<Box> &onBox,
		              const int coarselevel, const int finelevel,
		              const bool resetIfSet)
{
  Real rMin, rMax, levMin, levMax;
  bool isSet(pltAppState->IsSet(mmrangetype, framenumber, derivednumber));
  if(isSet == false || resetIfSet) {  // find and set the mins and maxes
    rMin =  AV_BIG_REAL;
    rMax = -AV_BIG_REAL;
    for(int lev(coarselevel); lev <= finelevel; ++lev) {
      bool minMaxValid(false);
      DataServices::Dispatch(DataServices::MinMaxRequest,
                             dataServicesPtr[framenumber],
                             (void *) &(onBox[lev]),
                             (void *) &(currentderived),
                             lev, &levMin, &levMax, &minMaxValid);
      if(minMaxValid) {
        rMin = min(rMin, levMin);
        rMax = max(rMax, levMax);
      }
    }
    pltAppState->SetMinMax(mmrangetype, framenumber, derivednumber, rMin, rMax);
  }
}


// -------------------------------------------------------------------
void PltApp::DoExposeRef(Widget, XtPointer, XtPointer) {
  int zPlanePosX(10), zPlanePosY(15);
  int whiteColor(pltPaletteptr->WhiteIndex());
  int zpColor(whiteColor);
  char sX[LINELENGTH], sY[LINELENGTH], sZ[LINELENGTH];
  
  XClearWindow(display, XtWindow(wControlForm));
  
  strcpy(sX, "X");
  strcpy(sY, "Y");
  strcpy(sZ, "Z");
  
  DrawAxes(wControlForm, zPlanePosX, zPlanePosY, 0, sX, sY, zpColor);
#if (BL_SPACEDIM == 3)
  int axisLength(20);
  int ypColor(whiteColor), xpColor(whiteColor);
  int xyzAxisLength(50);
  int stringOffsetX(4), stringOffsetY(20);
  int yPlanePosX(80), yPlanePosY(15);
  int xPlanePosX(10);
  int xPlanePosY((int) (axisLength + zPlanePosY + 1.4 * stringOffsetY));
  char temp[LINELENGTH];
  Dimension width, height;
  XtVaGetValues(wControlForm, XmNwidth, &width, XmNheight, &height, NULL);
  int centerX((int) width / 2);
  int centerY((int) height / 2 - 16);
  
  DrawAxes(wControlForm, yPlanePosX, yPlanePosY, 0, sX, sZ, ypColor);
  DrawAxes(wControlForm, xPlanePosX, xPlanePosY, 0, sY, sZ, xpColor);
  
  sprintf(temp, "Z=%i", amrPicturePtrArray[ZPLANE]->GetSlice());
  //                (amrPicturePtrArray[YPLANE]->
  //		ImageSizeV()-1 - amrPicturePtrArray[YPLANE]->GetHLine())/
  //		currentScale + ivLowOffsetMAL[ZDIR]);
  XSetForeground(display, xgc, pltPaletteptr->pixelate(zpColor));
  XDrawString(display, XtWindow(wControlForm), xgc,
	      centerX-xyzAxisLength+12, centerY+xyzAxisLength+4,
	      temp, strlen(temp));
  
  sprintf(temp, "Y=%i", amrPicturePtrArray[YPLANE]->GetSlice());
  //amrPicturePtrArray[XPLANE]->GetVLine()/
  //		currentScale + ivLowOffsetMAL[YDIR]);
  XSetForeground(display, xgc, pltPaletteptr->pixelate(ypColor));
  XDrawString(display, XtWindow(wControlForm), xgc,
	      centerX+stringOffsetX, centerY-xyzAxisLength+4,
	      temp, strlen(temp));
  
  sprintf(temp, "X=%i", amrPicturePtrArray[XPLANE]->GetSlice());
  //amrPicturePtrArray[ZPLANE]->GetVLine()/
  //currentScale + ivLowOffsetMAL[XDIR]);
  XSetForeground(display, xgc, pltPaletteptr->pixelate(xpColor));
  XDrawString(display, XtWindow(wControlForm), xgc,
	      centerX+4*stringOffsetX, centerY+stringOffsetY+2,
	      temp, strlen(temp));
  
  XSetForeground(XtDisplay(wControlForm), xgc,
		 pltPaletteptr->pixelate(ypColor));
  XDrawLine(display, XtWindow(wControlForm), xgc,
	    centerX, centerY, centerX, centerY-xyzAxisLength);
  XDrawLine(display, XtWindow(wControlForm), xgc,
	    centerX, centerY, centerX+xyzAxisLength, centerY);
  XDrawLine(display, XtWindow(wControlForm), xgc,
	    centerX, centerY,
	    (int) (centerX - 0.9 * xyzAxisLength),
	    (int) (centerY + 0.9 * xyzAxisLength));
#endif
}


// -------------------------------------------------------------------
void PltApp::DrawAxes(Widget wArea, int xpos, int ypos, int /* orientation */ ,
		      char *hlabel, char *vlabel, int color)
{
  int axisLength=20;
  char hLabel[LINELENGTH], vLabel[LINELENGTH];
  strcpy(hLabel, hlabel);
  strcpy(vLabel, vlabel);
  XSetForeground(XtDisplay(wArea), xgc, pltPaletteptr->pixelate(color));
  XDrawLine(XtDisplay(wArea), XtWindow(wArea), xgc,
	    xpos+5, ypos, xpos+5, ypos+axisLength);
  XDrawLine(XtDisplay(wArea), XtWindow(wArea), xgc,
	    xpos+5, ypos+axisLength, xpos+5+axisLength, ypos+axisLength);
  XDrawString(XtDisplay(wArea), XtWindow(wArea), xgc,
	      xpos+5+axisLength, ypos+5+axisLength, hLabel, strlen(hLabel));
  XDrawString(XtDisplay(wArea), XtWindow(wArea), xgc,
	      xpos, ypos, vLabel, strlen(vLabel));
}  



// -------------------------------------------------------------------
void PltApp::ChangeScale(Widget w, XtPointer client_data, XtPointer) {
  if(w == wCurrScale) {
    XtVaSetValues(w, XmNset, true, NULL);
    return;
  }
  unsigned long newScale = (unsigned long) client_data;
  XtVaSetValues(wCurrScale, XmNset, false, NULL);
  wCurrScale = w;
  int previousScale(pltAppState->CurrentScale());
  int currentScale(newScale);
  pltAppState->SetCurrentScale(currentScale);
  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }
  for(int whichView(0); whichView < NPLANES; ++whichView) {
    if(whichView == activeView) {
      startX = (int) startX / previousScale * currentScale;
      startY = (int) startY / previousScale * currentScale;
      endX   = (int) endX   / previousScale * currentScale;
      endY   = (int) endY   / previousScale * currentScale;
      amrPicturePtrArray[activeView]->SetRegion(startX, startY, endX, endY);
    }
    startcutX[whichView]  /= previousScale;
    startcutY[whichView]  /= previousScale;
    finishcutX[whichView] /= previousScale;
    finishcutY[whichView] /= previousScale;
    startcutX[whichView]  *= currentScale;
    startcutY[whichView]  *= currentScale;
    finishcutX[whichView] *= currentScale;
    finishcutY[whichView] *= currentScale;
    amrPicturePtrArray[whichView]->SetSubCut(startcutX[whichView],
					     startcutY[whichView],
				             finishcutX[whichView],
					     finishcutY[whichView]);
    amrPicturePtrArray[whichView]->APChangeScale(currentScale, previousScale);
    XtVaSetValues(wPlotPlane[whichView], 
		  XmNwidth,  amrPicturePtrArray[whichView]->ImageSizeH() + 1,
		  XmNheight, amrPicturePtrArray[whichView]->ImageSizeV() + 1,
		  NULL);
  }
  DoExposeRef();
  writingRGB = false;
}

// -------------------------------------------------------------------
void PltApp::ChangeLevel(Widget w, XtPointer client_data, XtPointer) {

  if(w == wCurrLevel) {
    XtVaSetValues(w, XmNset, true, NULL);
    return;
  }
  XtVaSetValues(wCurrLevel, XmNset, false, NULL);
  wCurrLevel = w;

  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }

  unsigned long newLevel = (unsigned long) client_data;
  int minDrawnLevel(pltAppState->MinAllowableLevel());
  int maxDrawnLevel(newLevel + pltAppState->MinAllowableLevel());

  pltAppState->SetMinDrawnLevel(minDrawnLevel);
  pltAppState->SetMaxDrawnLevel(maxDrawnLevel);
  for(int whichView(0); whichView < NPLANES; ++whichView) {
    amrPicturePtrArray[whichView]->APChangeLevel();
    
#if (BL_SPACEDIM == 3)
    if(whichView == ZPLANE) {
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
      if( ! XmToggleButtonGetState(wAutoDraw)) {
	projPicturePtr->MakeBoxes(); 
	XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
	DoExposeTransDA();
      }
#else
      projPicturePtr->MakeBoxes(); 
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
      DoExposeTransDA();
#endif
    }
#endif
  }

  if(datasetShowing) {
    int hdir, vdir, sdir;
    if(activeView==ZPLANE) { hdir = XDIR; vdir = YDIR; sdir = ZDIR; }
    if(activeView==YPLANE) { hdir = XDIR; vdir = ZDIR; sdir = YDIR; }
    if(activeView==XPLANE) { hdir = YDIR; vdir = ZDIR; sdir = XDIR; }
    datasetPtr->DatasetRender(trueRegion, amrPicturePtrArray[activeView],
		              this, pltAppState, hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  }
  DoExposeRef();
}

// -------------------------------------------------------------------
void PltApp::ChangeDerived(Widget w, XtPointer client_data, XtPointer) {
  if(w == wCurrDerived) {
    XtVaSetValues(w, XmNset, true, NULL);
    if(Verbose()) {
      cout << "--------------------- unchanged derived." << endl;
    }
    return;
  }
  XtVaSetValues(wCurrDerived, XmNset, false, NULL);
  wCurrDerived = w;
  unsigned long derivedNumber = (unsigned long) client_data;
  pltAppState->SetCurrentDerived(dataServicesPtr[currentFrame]->
				   PlotVarNames()[derivedNumber], derivedNumber);

  int maxDrawnLevel = pltAppState->MaxDrawnLevel();

  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();

  // possibly set all six minmax types here
  const Array<Box> &onSubregionBox = amrPicturePtrArray[ZPLANE]->GetSubDomain();
  const Array<Box> &onBox(amrData.ProbDomain());
  int iCDerNum(pltAppState->CurrentDerivedNumber());
  int coarseLevel(pltAppState->MinAllowableLevel());
  int fineLevel(pltAppState->MaxAllowableLevel());
  Real rGlobalMin, rGlobalMax;
  Real rSubregionMin, rSubregionMax;
  rGlobalMin    =  AV_BIG_REAL;
  rGlobalMax    = -AV_BIG_REAL;
  rSubregionMin =  AV_BIG_REAL;
  rSubregionMax = -AV_BIG_REAL;
  const aString asCDer(pltAppState->CurrentDerived());
  for(int iFrame(0); iFrame < animFrames; ++iFrame) {
    // set FILEGLOBALMINMAX  dont reset if already set
    FindAndSetMinMax(FILEGLOBALMINMAX, iFrame, asCDer, iCDerNum, onBox,
	             coarseLevel, fineLevel, false);

    // set FILESUBREGIONMINMAX  dont reset if already set
    FindAndSetMinMax(FILESUBREGIONMINMAX, iFrame, asCDer, iCDerNum, onSubregionBox,
	             coarseLevel, fineLevel, false);

    // collect file values
    Real rTempMin, rTempMax;
	       
    pltAppState->GetMinMax(FILEGLOBALMINMAX, iFrame, iCDerNum,
			   rTempMin, rTempMax);
    rGlobalMin = min(rGlobalMin, rTempMin);
    rGlobalMax = max(rGlobalMax, rTempMax);

    pltAppState->GetMinMax(FILESUBREGIONMINMAX, iFrame, iCDerNum,
			   rTempMin, rTempMax);
    rSubregionMin = min(rSubregionMin, rTempMin);
    rSubregionMax = max(rSubregionMax, rTempMax);

    // set FILEUSERMINMAX  dont reset if already set
    // this sets values to FILESUBREGIONMINMAX values if the user has not set them
    bool isSet(pltAppState->IsSet(FILEUSERMINMAX, iFrame, iCDerNum));
    if( ! isSet) {
      pltAppState->SetMinMax(FILEUSERMINMAX, iFrame, iCDerNum, rTempMin, rTempMax);
    }
  }

  for(int iFrame(0); iFrame < animFrames; ++iFrame) {
    pltAppState->SetMinMax(GLOBALMINMAX, iFrame, iCDerNum, rGlobalMin, rGlobalMax);
    pltAppState->SetMinMax(SUBREGIONMINMAX, iFrame, iCDerNum,
			   rSubregionMin, rSubregionMax);
    // set FILEUSERMINMAX  dont reset if already set
    // this sets values to FILESUBREGIONMINMAX values if the user has not set them
    bool isSet(pltAppState->IsSet(USERMINMAX, iFrame, iCDerNum));
    if( ! isSet) {
      pltAppState->SetMinMax(USERMINMAX, iFrame, iCDerNum,
			     rSubregionMin, rSubregionMax);
    }
  }
  if(Verbose()) {
    cout << "_in PltApp::ChangeDerived" << endl;
    pltAppState->PrintSetMap();  cout << endl;
  }


#if (BL_SPACEDIM == 3)
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  projPicturePtr->GetVolRenderPtr()->InvalidateSWFData();
  projPicturePtr->GetVolRenderPtr()->InvalidateVPData();
#endif
  Clear();
#endif

  writingRGB = false;
  paletteDrawn = false;
  if(animating2d) {
    ResetAnimation();
    DirtyFrames();
    if(UsingFileRange(currentRangeType)) {
      Real dataMin, dataMax;
      int coarseLevel(0);
      int fineLevel(maxDrawnLevel);
      for(int lev(coarseLevel); lev <= fineLevel; ++lev) {
	bool minMaxValid(false);
	DataServices::Dispatch(DataServices::MinMaxRequest,
			       dataServicesPtr[currentFrame],
			       (void *) &(amrData.ProbDomain()[lev]),
			       (void *) &(pltAppState->CurrentDerived()),
			       lev, &dataMin, &dataMax, &minMaxValid);
	if( ! minMaxValid) {
	  continue;
	}
      }
    } else if(strcmp(pltAppState->CurrentDerived().c_str(),"vol_frac") == 0) {
    } else {
      aString outbuf("Finding global min & max values for ");
      outbuf += pltAppState->CurrentDerived();
      outbuf += "...\n";
      strcpy(buffer, outbuf.c_str());
      PrintMessage(buffer);
      
      Real dataMin, dataMax;
      int coarseLevel(0);
      int fineLevel(maxDrawnLevel);
      for(int iFrame(0); iFrame < animFrames; ++iFrame) {
	for(int lev(coarseLevel); lev <= fineLevel; ++lev) {
	  bool minMaxValid(false);
	  DataServices::Dispatch(DataServices::MinMaxRequest,
				 dataServicesPtr[iFrame],
				 (void *) &(amrData.ProbDomain()[lev]),
				 (void *) &(pltAppState->CurrentDerived()),
				 lev, &dataMin, &dataMax, &minMaxValid);
	  if( ! minMaxValid) {
	    continue;
	  }
	}
      }
    } 
  }  // end if(animating2d)
  
  strcpy(buffer, pltAppState->CurrentDerived().c_str());
  XmString label_str = XmStringCreateSimple(buffer);
  XtVaSetValues(wPlotLabel, XmNlabelString, label_str, NULL);
  XmStringFree(label_str);

  pltAppState->SetMaxDrawnLevel(maxDrawnLevel);
  for(int iv(0); iv < NPLANES; ++iv) {
    amrPicturePtrArray[iv]->APMakeImages(pltPaletteptr);
  }
  
  if(datasetShowing) {
    int hdir, vdir, sdir;
    hdir = (activeView == XPLANE) ? YDIR : XDIR;
    vdir = (activeView == ZPLANE) ? YDIR : ZDIR;
    sdir = (activeView == YPLANE) ? YDIR : ((activeView == XYPLANE) ? XDIR : ZDIR);
    datasetPtr->DatasetRender(trueRegion, amrPicturePtrArray[activeView],
		              this, pltAppState, hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  }

/*
  if(UseSpecifiedMinMax()) {
    Real specifiedMin, specifiedMax;
    GetSpecifiedMinMax(specifiedMin, specifiedMax);
    for(int iFrame(0); iFrame < animFrames; ++iFrame) {
      pltAppState->SetMinMax(USERMINMAX, iFrame,
			     pltAppState->CurrentDerivedNumber(),
			     specifiedMin, specifiedMax);
    }
  }
*/
}  // end ChangeDerived(...)


// -------------------------------------------------------------------
void PltApp::ChangeContour(Widget w, XtPointer input_data, XtPointer) {
  ContourType prevCType(pltAppState->GetContourType());
  ContourType newCType = ContourType((unsigned long) input_data);
  if(newCType == prevCType) {
    return;
  }
  pltAppState->SetContourType(newCType);
  XtVaSetValues(wContourLabel,
		XmNsensitive, (newCType != RASTERONLY),
		NULL);
  XtVaSetValues(wNumberContours,
	        XmNsensitive, (newCType != RASTERONLY),
		NULL);
  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }
  for(int np(0); np < NPLANES; ++np) {
    amrPicturePtrArray[np]->APChangeContour(prevCType);
  }
}


// -------------------------------------------------------------------
void PltApp::ReadContourString(Widget w, XtPointer, XtPointer) {
  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }

  char temp[64];
  strcpy(temp, XmTextFieldGetString(w));
  aString tmpContourNumString(temp);

  // unexhaustive string check to prevent errors 
  bool stringOk(true);

  //check to make sure the input is an integer, and its length < 3.
  if(tmpContourNumString.length() >= 3) {
    stringOk = false;
    cerr << "Bad contour number string, must be less than 100." << endl;
  } else {
    for(int i(0); i < tmpContourNumString.length(); ++i) {
      if( ! isdigit(tmpContourNumString[i])) {
        stringOk = false;
      }
    }
  }

  if(stringOk) {
    contourNumString = tmpContourNumString;
    pltAppState->SetNumContours(atoi(contourNumString.c_str()));
    for(int ii(0); ii < NPLANES; ++ii) {
        amrPicturePtrArray[ii]->APDraw(pltAppState->MinDrawnLevel(), 
				       pltAppState->MaxDrawnLevel());
    }
  }

  XtVaSetValues(w, XmNvalue, contourNumString.c_str(), NULL);
}


// -------------------------------------------------------------------
void PltApp::ToggleRange(Widget w, XtPointer client_data, XtPointer call_data) {
  unsigned long r = (unsigned long) client_data;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *) call_data;
  if(state->set == true) {
    currentRangeType = (MinMaxRangeType) r;
    if(bFileRangeButtonSet) {
      if(currentRangeType == GLOBALMINMAX) {
        currentRangeType = FILEGLOBALMINMAX;
      } else if(currentRangeType == SUBREGIONMINMAX) {
        currentRangeType = FILESUBREGIONMINMAX;
      } else if(currentRangeType == USERMINMAX) {
        currentRangeType = FILEUSERMINMAX;
      }
    }
  }
}


// -------------------------------------------------------------------
void PltApp::DoSubregion(Widget, XtPointer, XtPointer) {
  Box subregionBox;
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  int finestLevel(amrData.FinestLevel());
  int maxAllowableLevel(pltAppState->MaxAllowableLevel());
  int newMinAllowableLevel;
  
  // subregionBox is at the maxAllowableLevel
  
#if (BL_SPACEDIM == 2)
  if(selectionBox.bigEnd(XDIR) == 0 || selectionBox.bigEnd(YDIR) == 0) {
    return;
  }
  subregionBox = selectionBox + ivLowOffsetMAL;
#endif
  
#if (BL_SPACEDIM == 3)
  int np;
  int currentScale(pltAppState->CurrentScale());
  for(np = 0; np < NPLANES; ++np) {
    startcutX[np]  /= currentScale;
    startcutY[np]  /= currentScale;
    finishcutX[np] /= currentScale;
    finishcutY[np] /= currentScale;
  }
  
  subregionBox.setSmall(XDIR, min(startcutX[ZPLANE], finishcutX[ZPLANE]));
  subregionBox.setSmall(YDIR, min(startcutX[XPLANE], finishcutX[XPLANE]));
  subregionBox.setSmall(ZDIR, (amrPicturePtrArray[XPLANE]->ImageSizeV()-1)/
			currentScale - max(startcutY[XPLANE], finishcutY[XPLANE]));
  subregionBox.setBig(XDIR, max(startcutX[ZPLANE], finishcutX[ZPLANE]));
  subregionBox.setBig(YDIR, max(startcutX[XPLANE], finishcutX[XPLANE]));
  subregionBox.setBig(ZDIR, (amrPicturePtrArray[XPLANE]->ImageSizeV()-1)/
		      currentScale - min(startcutY[XPLANE], finishcutY[XPLANE]));
  
  if(subregionBox.numPts() <= 4) {
    for(np = 0; np < NPLANES; ++np) {
      startcutX[np]  *= currentScale;
      startcutY[np]  *= currentScale;
      finishcutX[np] *= currentScale;
      finishcutY[np] *= currentScale;
    }
    return;
  }

  subregionBox += ivLowOffsetMAL;   // shift to true data region
#endif
  
  Box tempRefinedBox(subregionBox);
  tempRefinedBox.refine(CRRBetweenLevels(maxAllowableLevel, finestLevel,
					 amrData.RefRatio()));
  // this puts tempRefinedBox in terms of the finest level
  newMinAllowableLevel = 0;//amrData.FinestContainingLevel(
                           // tempRefinedBox, finestLevel);
  newMinAllowableLevel = min(newMinAllowableLevel, maxAllowableLevel);
  
  // coarsen to the newMinAllowableLevel to align grids
  subregionBox.coarsen(CRRBetweenLevels(newMinAllowableLevel,
					maxAllowableLevel, amrData.RefRatio()));
  
  Box subregionBoxMAL(subregionBox);
  
  // refine to the finestLevel
  subregionBox.refine(CRRBetweenLevels(newMinAllowableLevel, finestLevel,
				       amrData.RefRatio()));
  
  maxAllowableLevel = DetermineMaxAllowableLevel(subregionBox, finestLevel,
						 MaxPictureSize(), amrData.RefRatio());
  subregionBoxMAL.refine(CRRBetweenLevels(newMinAllowableLevel,
					  maxAllowableLevel, amrData.RefRatio()));
  
  IntVect ivOffset(subregionBoxMAL.smallEnd());
  
  // get the old slices and check if they will be within the new subregion.
  // if not -- choose the limit
  
  // then pass the slices to the subregion constructor below...
  SubregionPltApp(wTopLevel, subregionBox, ivOffset,
		  //amrPicturePtrArray[ZPLANE],
		  this, palFilename, animating2d, pltAppState->CurrentDerived(),
		  fileName);
  
#if (BL_SPACEDIM == 3)
  for(np = 0; np < NPLANES; ++np) {
    startcutX[np]  *= currentScale;
    startcutY[np]  *= currentScale;
    finishcutX[np] *= currentScale;
    finishcutY[np] *= currentScale;
  }
#endif
}  // end DoSubregion

  
// -------------------------------------------------------------------
void PltApp::DoDatasetButton(Widget, XtPointer, XtPointer) {
  if(selectionBox.bigEnd(XDIR) == 0 || selectionBox.bigEnd(YDIR) == 0) {
    return;
  }

  int hdir, vdir, sdir;
  
  trueRegion = selectionBox;
  
#if (BL_SPACEDIM == 3)
  trueRegion.setSmall(ZDIR, amrPicturePtrArray[ZPLANE]->GetSlice()); 
  trueRegion.setBig(ZDIR,   amrPicturePtrArray[ZPLANE]->GetSlice()); 
#endif

  hdir = (activeView == XPLANE) ? YDIR : XDIR;
  vdir = (activeView == ZPLANE) ? YDIR : ZDIR;
  switch (activeView) {
  case ZPLANE: // orient box to view and shift
    trueRegion.shift(XDIR, ivLowOffsetMAL[XDIR]);
    trueRegion.shift(YDIR, ivLowOffsetMAL[YDIR]);
    sdir = ZDIR;
    break;
  case YPLANE:
    trueRegion.setSmall(ZDIR, trueRegion.smallEnd(YDIR)); 
    trueRegion.setBig(ZDIR, trueRegion.bigEnd(YDIR)); 
    trueRegion.setSmall(YDIR, amrPicturePtrArray[YPLANE]->GetSlice()); 
    trueRegion.setBig(YDIR, amrPicturePtrArray[YPLANE]->GetSlice()); 
    trueRegion.shift(XDIR, ivLowOffsetMAL[XDIR]);
    trueRegion.shift(ZDIR, ivLowOffsetMAL[ZDIR]);
    sdir = YDIR;
    break;
  case XPLANE:
    trueRegion.setSmall(ZDIR, trueRegion.smallEnd(YDIR)); 
    trueRegion.setBig(ZDIR, trueRegion.bigEnd(YDIR)); 
    trueRegion.setSmall(YDIR, trueRegion.smallEnd(XDIR)); 
    trueRegion.setBig(YDIR, trueRegion.bigEnd(XDIR)); 
    trueRegion.setSmall(XDIR, amrPicturePtrArray[XPLANE]->GetSlice()); 
    trueRegion.setBig(XDIR, amrPicturePtrArray[XPLANE]->GetSlice()); 
    trueRegion.shift(YDIR, ivLowOffsetMAL[YDIR]);
    trueRegion.shift(ZDIR, ivLowOffsetMAL[ZDIR]);
    sdir = XDIR;
  }
  
  if(datasetShowing) {
    datasetPtr->DoRaise();
    datasetPtr->DatasetRender(trueRegion, amrPicturePtrArray[activeView], this,
		              pltAppState, hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  } else {
    datasetShowing = true;
    datasetPtr = new Dataset(trueRegion, amrPicturePtrArray[activeView], this,
			     pltAppState, hdir, vdir, sdir);
  }
}  // end DoDatasetButton


// -------------------------------------------------------------------
void PltApp::QuitDataset() {
  datasetShowing = false;
}


// -------------------------------------------------------------------
void PltApp::DoPaletteButton(Widget, XtPointer, XtPointer) {
  static Widget wPalDialog;
  wPalDialog = XmCreateFileSelectionDialog(wAmrVisTopLevel,
				"Choose Palette", NULL, 0);

  AddStaticCallback(wPalDialog, XmNokCallback, &PltApp::DoOpenPalFile);
  XtAddCallback(wPalDialog, XmNcancelCallback,
		(XtCallbackProc) XtUnmanageChild, (XtPointer) this);
  XtManageChild(wPalDialog);
  XtPopup(XtParent(wPalDialog), XtGrabExclusive);
}


// -------------------------------------------------------------------
void PltApp::DestroyInfoWindow(Widget, XtPointer xp, XtPointer) {
  infoShowing = false;
}


// -------------------------------------------------------------------
void PltApp::CloseInfoWindow(Widget, XtPointer, XtPointer) {
  XtPopdown(wInfoTopLevel);
  infoShowing = false;
}


// -------------------------------------------------------------------
void PltApp::DoInfoButton(Widget, XtPointer, XtPointer) {
  if(infoShowing) {
    XtPopup(wInfoTopLevel, XtGrabNone);
    XMapRaised(XtDisplay(wInfoTopLevel), XtWindow(wInfoTopLevel));
    return;
  }

  infoShowing = true;
  int xpos, ypos, width, height;
  XtVaGetValues(wAmrVisTopLevel, XmNx, &xpos, XmNy, &ypos,
		XmNwidth, &width, XmNheight, &height, NULL);
  
  wInfoTopLevel = 
    XtVaCreatePopupShell("Info",
			 topLevelShellWidgetClass, wAmrVisTopLevel,
			 XmNwidth,		400,
			 XmNheight,		300,
			 XmNx,		50+xpos+width/2,
			 XmNy,		ypos-10,
			 NULL);
  
  AddStaticCallback(wInfoTopLevel, XmNdestroyCallback, &PltApp::DestroyInfoWindow);
  
  //set visual in case the default isn't 256 pseudocolor
  if(GAptr->PVisual() != XDefaultVisual(display, GAptr->PScreenNumber())) {
    XtVaSetValues(wInfoTopLevel, XmNvisual, GAptr->PVisual(), XmNdepth, 8, NULL);
  }
  
  Widget wInfoForm =
    XtVaCreateManagedWidget("infoform", xmFormWidgetClass, wInfoTopLevel, NULL);
  
  int i(0);
  XtSetArg(args[i], XmNlistSizePolicy, XmRESIZE_IF_POSSIBLE);   ++i;
  Widget wInfoList = XmCreateScrolledList(wInfoForm, "infoscrolledlist", args, i);
  
  XtVaSetValues(XtParent(wInfoList), 
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNtopOffset, WOFFSET,
		XmNbottomAttachment, XmATTACH_POSITION,
		XmNbottomPosition, 80,
		NULL);
  
  AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  
  int numEntries = 9+amrData.FinestLevel()+1;
  char **entries = new char *[numEntries];
  
  for(int j(0); j < numEntries; ++j) {
    entries[j] = new char[BUFSIZ];
  }
  
  i=0;
  char buf[BUFSIZ];
  ostrstream prob(buf, BUFSIZ);
  prob.precision(15);
  strcpy(entries[i], fileNames[currentFrame].c_str()); ++i;
  strcpy(entries[i], amrData.PlotFileVersion().c_str()); ++i;
  prob << "time: "<< amrData.Time() << ends;
  strcpy(entries[i], buf); ++i;
  sprintf(entries[i], "levels: %d", amrData.FinestLevel()+1); ++i;
  sprintf(entries[i], "prob domain"); ++i;
  for(int k(0); k <= amrData.FinestLevel(); ++k, ++i) {
    ostrstream prob_domain(entries[i], BUFSIZ);
    prob_domain << " level " << k << ": " << amrData.ProbDomain()[k] << ends;
  }

  prob.seekp(0);
  prob << "refratios:";
  for(int k(0); k < amrData.FinestLevel(); ++k) {
    prob << " " << amrData.RefRatio()[k];
  }
  prob << ends;
  strcpy(entries[i], buf); ++i;
  
  prob.seekp(0);
  prob << "probsize:";
  for(int k(0); k < BL_SPACEDIM; ++k) {
    prob << " " << amrData.ProbSize()[k];
  }
  prob << ends;
  strcpy(entries[i], buf); ++i;
  
  prob.seekp(0);
  prob << "prob lo:";
  for(int k(0); k < BL_SPACEDIM; ++k) {
    prob << " " << amrData.ProbLo()[k];
  }
  prob << ends;
  strcpy(entries[i], buf); ++i;
  
  prob.seekp(0);
  prob << "prob hi:";
  for(int k(0); k < BL_SPACEDIM; ++k) {
    prob<<" "<< amrData.ProbHi()[k];
  }
  prob << ends;
  strcpy(entries[i], buf); ++i;

  XmStringTable str_list = (XmStringTable)XtMalloc(numEntries*sizeof(XmString *));
  for(int j(0); j < numEntries; ++j) {
    str_list[j] = XmStringCreateSimple(entries[j]);
  }
    
  XtVaSetValues(wInfoList,
		XmNvisibleItemCount, numEntries,
		XmNitemCount, numEntries,
		XmNitems, str_list,
		NULL);
  
  for(int j(0); j < numEntries; ++j) {
    delete [] entries[j];
  }
  delete [] entries;
  
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
		    &PltApp::CloseInfoWindow);
  
  XtManageChild(wInfoList);
  XtManageChild(wInfoCloseButton);
  XtPopup(wInfoTopLevel, XtGrabNone);
}

// -------------------------------------------------------------------
void PltApp::DoContoursButton(Widget, XtPointer, XtPointer) {
  Position xpos, ypos;
  Dimension width, height;

  if(contoursShowing) {
    XtPopup(wContoursTopLevel, XtGrabNone);
    XMapRaised(XtDisplay(wContoursTopLevel), XtWindow(wContoursTopLevel));
    return;
  }

  Widget wContoursForm, wCloseButton, wContourRadio, wid;
  
  XtVaGetValues(wAmrVisTopLevel, XmNx, &xpos, XmNy, &ypos,
		XmNwidth, &width, XmNheight, &height, NULL);
  
  wContoursTopLevel = XtVaCreatePopupShell("Set Contours",
			 topLevelShellWidgetClass, wAmrVisTopLevel,
			 XmNwidth,		   170,
			 XmNheight,		   220,
			 XmNx,			   xpos+width/2,
			 XmNy,			   ypos+height/2,
			 NULL);
  
  AddStaticCallback(wContoursTopLevel, XmNdestroyCallback,
		    &PltApp::DestroyContoursWindow);
  
  //set visual in case the default isn't 256 pseudocolor
  if(GAptr->PVisual() != XDefaultVisual(display, GAptr->PScreenNumber())) {
    XtVaSetValues(wContoursTopLevel, XmNvisual, GAptr->PVisual(), XmNdepth, 8,NULL);
  }
        
    
  wContoursForm = XtVaCreateManagedWidget("Contoursform",
			    xmFormWidgetClass, wContoursTopLevel,
			    NULL);

  wContourRadio = XmCreateRadioBox(wContoursForm, "contourtype", NULL, 0);
  XtVaSetValues(wContourRadio,
		XmNtopAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM, NULL);		
  
  char *conItems[NCONTOPTIONS] = {"Raster", "Raster & Contours", "Color Contours",
		                  "B/W Contours", "Velocity Vectors"};
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  int nItems = (amrData.NComp() < (BL_SPACEDIM + 1)) ?
				    (NCONTOPTIONS - 1) : NCONTOPTIONS;
  for(int j(0); j < nItems; ++j) {
    wid = XtVaCreateManagedWidget(conItems[j], xmToggleButtonGadgetClass,
				  wContourRadio, NULL);
    if(j == (int) pltAppState->GetContourType()) {
      XtVaSetValues(wid, XmNset, true, NULL);
    }
    AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::ChangeContour,
		      (XtPointer) j);
  }

  XmString label_str = XmStringCreateSimple("Number of lines:");
  wContourLabel = XtVaCreateManagedWidget("numcontours",
			    xmLabelWidgetClass, wContoursForm,
			    XmNlabelString,     label_str, 
			    XmNleftAttachment,  XmATTACH_FORM,
			    XmNleftOffset,      WOFFSET,
			    XmNtopAttachment,   XmATTACH_WIDGET,
			    XmNtopWidget,       wContourRadio,
			    XmNtopOffset,       WOFFSET,
			    XmNsensitive,
				  pltAppState->GetContourType() != RASTERONLY,
			    NULL);
  XmStringFree(label_str);

  wNumberContours = XtVaCreateManagedWidget("contours",
			    xmTextFieldWidgetClass, wContoursForm,
			    XmNtopAttachment,    XmATTACH_WIDGET,
			    XmNtopWidget,        wContourRadio,
			    XmNtopOffset,        WOFFSET,
			    XmNleftAttachment,   XmATTACH_WIDGET,
			    XmNleftWidget,       wContourLabel,
			    XmNleftOffset,       WOFFSET,
			    XmNvalue,            contourNumString.c_str(),
			    XmNcolumns,          4,
			    XmNsensitive,
				  pltAppState->GetContourType() != RASTERONLY,
			    NULL);
  AddStaticCallback(wNumberContours, XmNactivateCallback,
		    &PltApp::ReadContourString);
  
  wCloseButton = XtVaCreateManagedWidget(" Close ",
			    xmPushButtonGadgetClass, wContoursForm,
			    XmNbottomAttachment, XmATTACH_FORM,
			    XmNbottomOffset, WOFFSET,
			    XmNleftAttachment, XmATTACH_POSITION,
			    XmNleftPosition, 33,
			    XmNrightAttachment, XmATTACH_POSITION,
			    XmNrightPosition, 66,
			    NULL);
  AddStaticCallback(wCloseButton, XmNactivateCallback,
		    &PltApp::CloseContoursWindow);
  
  XtManageChild(wContourRadio);
  XtManageChild(wCloseButton);
  XtPopup(wContoursTopLevel, XtGrabNone);
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wContoursTopLevel));
  contoursShowing = true;
}


// -------------------------------------------------------------------
void PltApp::DestroyContoursWindow(Widget, XtPointer, XtPointer) {
  contoursShowing = false;
}


// -------------------------------------------------------------------
void PltApp::CloseContoursWindow(Widget, XtPointer, XtPointer) {
  XtPopdown(wContoursTopLevel);
}


// -------------------------------------------------------------------
void PltApp::DoToggleFileRangeButton(Widget w, XtPointer client_data,
				     XtPointer call_data)
{
  bFileRangeButtonSet = XmToggleButtonGetState(wFileRangeCheckBox);
  if(bFileRangeButtonSet) {
    if(currentRangeType == GLOBALMINMAX) {
      currentRangeType = FILEGLOBALMINMAX;
    } else if(currentRangeType == SUBREGIONMINMAX) {
      currentRangeType = FILESUBREGIONMINMAX;
    } else if(currentRangeType == USERMINMAX) {
      currentRangeType = FILEUSERMINMAX;
    }
  } else {
    if(currentRangeType == FILEGLOBALMINMAX) {
      currentRangeType = GLOBALMINMAX;
    } else if(currentRangeType == FILESUBREGIONMINMAX) {
      currentRangeType = SUBREGIONMINMAX;
    } else if(currentRangeType == FILEUSERMINMAX) {
      currentRangeType = USERMINMAX;
    }
  }
}


// -------------------------------------------------------------------
void PltApp::DoSetRangeButton(Widget, XtPointer, XtPointer) {
  Position xpos, ypos;
  Dimension width, height;

  if(setRangeShowing) {
    XtPopup(wSetRangeTopLevel, XtGrabNone);
    XMapRaised(XtDisplay(wSetRangeTopLevel), XtWindow(wSetRangeTopLevel));
    return;
  }

  char range[LINELENGTH];
  char saveRangeString[LINELENGTH];
  char format[LINELENGTH];
  char fMin[LINELENGTH];
  char fMax[LINELENGTH];
  strcpy(format, pltAppState->GetFormatString().c_str());
  Widget wSetRangeForm, wDoneButton, wApplyButton, wCancelButton;
  Widget wSetRangeRadioBox, wRangeRC, wid;

  
  // do these here to set XmNwidth of wSetRangeTopLevel
  Real rtMin, rtMax;
  pltAppState->GetMinMax(GLOBALMINMAX, currentFrame,
			 pltAppState->CurrentDerivedNumber(),
			 rtMin, rtMax);
  sprintf(fMin, format, rtMin);
  sprintf(fMax, format, rtMax);
  sprintf(range, "Min: %s  Max: %s", fMin, fMax);
  strcpy(saveRangeString, range);
  
  XtVaGetValues(wAmrVisTopLevel,
		XmNx, &xpos,
		XmNy, &ypos,
		XmNwidth, &width,
		XmNheight, &height,
		NULL);
  
  wSetRangeTopLevel = XtVaCreatePopupShell("Set Range",
			 topLevelShellWidgetClass, wAmrVisTopLevel,
			 XmNwidth,		250,
			 XmNheight,		200,
			 XmNx,			xpos+width/2,
			 XmNy,			ypos,
			 NULL);
  
  AddStaticCallback(wSetRangeTopLevel, XmNdestroyCallback,
		    &PltApp::DestroySetRangeWindow);
  
  //set visual in case the default isn't 256 pseudocolor
  if(GAptr->PVisual() != XDefaultVisual(display, GAptr->PScreenNumber())) {
    XtVaSetValues(wSetRangeTopLevel, XmNvisual, GAptr->PVisual(),
		  XmNdepth, 8, NULL);
  }
        
    
  wSetRangeForm = XtVaCreateManagedWidget("setrangeform",
					  xmFormWidgetClass, wSetRangeTopLevel,
					  NULL);
  
  // make the buttons
  wDoneButton = XtVaCreateManagedWidget(" Ok ",
					xmPushButtonGadgetClass, wSetRangeForm,
					XmNbottomAttachment, XmATTACH_FORM,
					XmNbottomOffset, WOFFSET,
					XmNleftAttachment, XmATTACH_FORM,
					XmNleftOffset, WOFFSET,
					NULL);
  bool bKillSRWindow(true);
  AddStaticCallback(wDoneButton, XmNactivateCallback, &PltApp::DoDoneSetRange,
		    (XtPointer) bKillSRWindow);
  
  wApplyButton = XtVaCreateManagedWidget(" Apply ",
					  xmPushButtonGadgetClass, wSetRangeForm,
					  XmNbottomAttachment, XmATTACH_FORM,
					  XmNbottomOffset, WOFFSET,
					  XmNleftAttachment, XmATTACH_WIDGET,
					  XmNleftWidget, wDoneButton,
					  XmNleftOffset, WOFFSET,
					  NULL);
  bKillSRWindow = false;
  AddStaticCallback(wApplyButton, XmNactivateCallback, &PltApp::DoDoneSetRange,
		    (XtPointer) bKillSRWindow);
  
  wCancelButton = XtVaCreateManagedWidget(" Cancel ",
					  xmPushButtonGadgetClass, wSetRangeForm,
					  XmNbottomAttachment, XmATTACH_FORM,
					  XmNbottomOffset, WOFFSET,
					  XmNrightAttachment, XmATTACH_FORM,
					  XmNrightOffset, WOFFSET,
					  NULL);
  AddStaticCallback(wCancelButton, XmNactivateCallback, &PltApp::DoCancelSetRange);
  
  // make the radio box
  wSetRangeRadioBox = XmCreateRadioBox(wSetRangeForm, "setrangeradiobox",
				       NULL, 0);
  XtVaSetValues(wSetRangeRadioBox, XmNmarginHeight, 8, NULL);
  XtVaSetValues(wSetRangeRadioBox, XmNmarginWidth, 4, NULL);
  XtVaSetValues(wSetRangeRadioBox, XmNspacing, 14, NULL);
  
  wRangeRadioButton[BGLOBALMINMAX] = XtVaCreateManagedWidget("Global",
			    xmToggleButtonWidgetClass, wSetRangeRadioBox, NULL);
  AddStaticCallback(wRangeRadioButton[BGLOBALMINMAX], XmNvalueChangedCallback,
		    &PltApp::ToggleRange, (XtPointer) BGLOBALMINMAX);
  wRangeRadioButton[BSUBREGIONMINMAX] = XtVaCreateManagedWidget("Local",
			    xmToggleButtonWidgetClass, wSetRangeRadioBox,
			    NULL);
  AddStaticCallback(wRangeRadioButton[BSUBREGIONMINMAX], XmNvalueChangedCallback,
		    &PltApp::ToggleRange, (XtPointer) BSUBREGIONMINMAX);
  wRangeRadioButton[BUSERMINMAX] = XtVaCreateManagedWidget("User",
			    xmToggleButtonWidgetClass, wSetRangeRadioBox, NULL);
  AddStaticCallback(wRangeRadioButton[BUSERMINMAX], XmNvalueChangedCallback,
		    &PltApp::ToggleRange, (XtPointer) BUSERMINMAX);
  if(animating2d) {
    int i(0);
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);  ++i;
    XtSetArg(args[i], XmNleftOffset, WOFFSET);            ++i;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET); ++i;
    XtSetArg(args[i], XmNtopWidget, wSetRangeRadioBox); ++i;
    XtSetArg(args[i], XmNtopOffset, WOFFSET); ++i;
    XtSetArg(args[i], XmNset, bFileRangeButtonSet); ++i;
    wFileRangeCheckBox = XmCreateToggleButton(wSetRangeForm, "File_Range",
			      args, i);
    AddStaticCallback(wFileRangeCheckBox, XmNvalueChangedCallback,
		      &PltApp::DoToggleFileRangeButton, NULL);
  }
    
  currentRangeType = pltAppState->GetMinMaxRangeType();
  MinMaxRangeTypeForButtons mmButton(BINVALIDMINMAX);
  if(currentRangeType == GLOBALMINMAX || currentRangeType == FILEGLOBALMINMAX) {
    mmButton = BGLOBALMINMAX;
  } else if(currentRangeType == SUBREGIONMINMAX ||
	    currentRangeType == FILESUBREGIONMINMAX)
  {
    mmButton = BSUBREGIONMINMAX;
  } else if(currentRangeType == USERMINMAX || currentRangeType == FILEUSERMINMAX) {
    mmButton = BUSERMINMAX;
  } else {
    BoxLib::Abort("Bad button range type.");
  }
  XtVaSetValues(wRangeRadioButton[mmButton], XmNset, true, NULL);
  
  wRangeRC = XtVaCreateManagedWidget("rangeRC", xmRowColumnWidgetClass,
			    wSetRangeForm,
			    XmNtopAttachment,   XmATTACH_FORM,
			    XmNtopOffset,       0,
			    XmNleftAttachment,  XmATTACH_WIDGET,
			    XmNleftWidget,      wSetRangeRadioBox,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNpacking,         XmPACK_COLUMN,
			    XmNnumColumns,      3,
			    XmNorientation,     XmHORIZONTAL,
			    NULL);
  // make the strings representing data min and maxes
  pltAppState->GetMinMax(GLOBALMINMAX, currentFrame,
			 pltAppState->CurrentDerivedNumber(), rtMin, rtMax);
  sprintf(fMin, format, rtMin);
  sprintf(fMax, format, rtMax);
  sprintf(range, "Min: %s", fMin);
  XtVaCreateManagedWidget(range, xmLabelGadgetClass, wRangeRC, NULL);
  //XtVaSetValues(wid, XmNleftOffset, 20, NULL);
  sprintf(range, "Max: %s", fMax);
  XtVaCreateManagedWidget(range, xmLabelGadgetClass, wRangeRC, NULL);

  pltAppState->GetMinMax(SUBREGIONMINMAX, currentFrame,
			 pltAppState->CurrentDerivedNumber(),
			 rtMin, rtMax);
  sprintf(fMin, format, rtMin);
  sprintf(fMax, format, rtMax);
  sprintf(range, "Min: %s", fMin);
  XtVaCreateManagedWidget(range, xmLabelGadgetClass, wRangeRC, NULL);
  sprintf(range, "Max: %s", fMax);
  XtVaCreateManagedWidget(range, xmLabelGadgetClass, wRangeRC, NULL);

  pltAppState->GetMinMax(USERMINMAX, currentFrame,
			 pltAppState->CurrentDerivedNumber(),
			 rtMin, rtMax);
  wid = XtVaCreateManagedWidget("wid", xmRowColumnWidgetClass, wRangeRC,
			    XmNorientation, XmHORIZONTAL,
			    XmNleftOffset,       0,
			    //XmNborderWidth,      0,
			    NULL);
  XtVaCreateManagedWidget("Min:", xmLabelGadgetClass, wid, NULL);
  //XtVaSetValues(wid, XmNmarginWidth, 0, NULL);
  sprintf(range, format, rtMin);
  wUserMin = XtVaCreateManagedWidget("local range",
			    xmTextFieldWidgetClass, wid,
			    XmNvalue,		range,
			    XmNeditable,	true,
			    XmNcolumns,		strlen(range)+2,
			    NULL);
  AddStaticCallback(wUserMin, XmNactivateCallback, &PltApp::DoUserMin);
  
  wid = XtVaCreateManagedWidget("wid", xmRowColumnWidgetClass, wRangeRC,
			    XmNorientation, XmHORIZONTAL,
 			    XmNleftOffset,       0,
			    //XmNborderWidth,      0,
			    NULL);
  XtVaCreateManagedWidget("Max:", xmLabelGadgetClass, wid, NULL);
  sprintf(range, format, rtMax);
  wUserMax = XtVaCreateManagedWidget("local range",
			    xmTextFieldWidgetClass, wid,
			    XmNvalue,		range,
			    XmNeditable,	true,
			    XmNcolumns,		strlen(range)+2,
			    NULL);
  AddStaticCallback(wUserMax, XmNactivateCallback, &PltApp::DoUserMax);
  
  XtManageChild(wSetRangeRadioBox);
  XtManageChild(wRangeRC);
  XtManageChild(wDoneButton);
  XtManageChild(wApplyButton);
  XtManageChild(wCancelButton);
  if(animating2d) {
    XtManageChild(wFileRangeCheckBox);
  }
  XtPopup(wSetRangeTopLevel, XtGrabNone);
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wSetRangeTopLevel));
  setRangeShowing = true;

  int isrw, iwidw;
  XtVaGetValues(wSetRangeRadioBox, XmNwidth, &isrw, NULL);
  XtVaGetValues(wid, XmNwidth, &iwidw, NULL);
  XtVaSetValues(wSetRangeTopLevel, XmNwidth, isrw + (2 * iwidw) + 10, NULL);
}


// -------------------------------------------------------------------
void PltApp::SetNewFormatString(const aString &newformatstring) {
  pltAppState->SetFormatString(newformatstring);
  pltPaletteptr->SetFormat(newformatstring);
  pltPaletteptr->Redraw();
  if(datasetShowing) {
    int hdir, vdir, sdir;
    if(activeView==ZPLANE) { hdir = XDIR; vdir = YDIR; sdir = ZDIR; }
    if(activeView==YPLANE) { hdir = XDIR; vdir = ZDIR; sdir = YDIR; }
    if(activeView==XPLANE) { hdir = YDIR; vdir = ZDIR; sdir = XDIR; }
    datasetPtr->DatasetRender(trueRegion, amrPicturePtrArray[activeView],
		              this, pltAppState, hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  }
  if(setRangeShowing) {
    bSetRangeRedraw = true;
    XtDestroyWidget(wSetRangeTopLevel);
    setRangeShowing = false;
    DoSetRangeButton(NULL, NULL, NULL);
    setRangeShowing = true;
  }
}


// -------------------------------------------------------------------
void PltApp::DoNumberFormatButton(Widget, XtPointer, XtPointer) {
  Position xpos, ypos;
  Dimension width, height;

  if(bFormatShowing) {
    XtPopup(wNumberFormatTopLevel, XtGrabNone);
    XMapRaised(XtDisplay(wNumberFormatTopLevel), XtWindow(wNumberFormatTopLevel));
    return;
  }

  char format[LINELENGTH];
  strcpy(format, pltAppState->GetFormatString().c_str());
  Widget wNumberFormatForm, wDoneButton, wApplyButton, wCancelButton, wid;

  
  XtVaGetValues(wAmrVisTopLevel,
		XmNx, &xpos,
		XmNy, &ypos,
		XmNwidth, &width,
		XmNheight, &height,
		NULL);
  
  wNumberFormatTopLevel = XtVaCreatePopupShell("Number Format",
			 topLevelShellWidgetClass, wAmrVisTopLevel,
			 XmNwidth,		200,
			 XmNheight,		100,
			 XmNx,			xpos+width/2,
			 XmNy,			ypos,
			 NULL);
  
  AddStaticCallback(wNumberFormatTopLevel, XmNdestroyCallback,
		    &PltApp::DestroyNumberFormatWindow);
  
  //set visual in case the default isn't 256 pseudocolor
  if(GAptr->PVisual() != XDefaultVisual(display, GAptr->PScreenNumber())) {
    XtVaSetValues(wNumberFormatTopLevel, XmNvisual, GAptr->PVisual(),
		  XmNdepth, 8, NULL);
  }
        
    
  wNumberFormatForm = XtVaCreateManagedWidget("setrangeform",
				  xmFormWidgetClass, wNumberFormatTopLevel,
				  NULL);
  
  // make the buttons
  wDoneButton = XtVaCreateManagedWidget(" Ok ",
					xmPushButtonGadgetClass, wNumberFormatForm,
					XmNbottomAttachment, XmATTACH_FORM,
					XmNbottomOffset, WOFFSET,
					XmNleftAttachment, XmATTACH_FORM,
					XmNleftOffset, WOFFSET,
					NULL);
  bool bKillNFWindow(true);
  AddStaticCallback(wDoneButton, XmNactivateCallback, &PltApp::DoDoneNumberFormat,
		    (XtPointer) bKillNFWindow);
  
  wApplyButton = XtVaCreateManagedWidget(" Apply ",
				  xmPushButtonGadgetClass, wNumberFormatForm,
				  XmNbottomAttachment, XmATTACH_FORM,
				  XmNbottomOffset, WOFFSET,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, wDoneButton,
				  XmNleftOffset, WOFFSET,
				  NULL);
  bKillNFWindow = false;
  AddStaticCallback(wApplyButton, XmNactivateCallback, &PltApp::DoDoneNumberFormat,
		    (XtPointer) bKillNFWindow);
  
  wCancelButton = XtVaCreateManagedWidget(" Cancel ",
				  xmPushButtonGadgetClass, wNumberFormatForm,
				  XmNbottomAttachment, XmATTACH_FORM,
				  XmNbottomOffset, WOFFSET,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNrightOffset, WOFFSET,
				  NULL);
  AddStaticCallback(wCancelButton, XmNactivateCallback,
		    &PltApp::DoCancelNumberFormat);
  
  wid = XtVaCreateManagedWidget("Format:",
			    xmLabelGadgetClass, wNumberFormatForm,
			    XmNtopAttachment, XmATTACH_FORM,
			    XmNtopOffset, WOFFSET,
			    XmNleftAttachment, XmATTACH_FORM,
			    XmNleftOffset, WOFFSET,
			    NULL);
  wFormat = XtVaCreateManagedWidget("format",
			    xmTextFieldWidgetClass, wNumberFormatForm,
			    XmNvalue,		format,
			    XmNeditable,	true,
			    XmNcolumns,		strlen(format) + 4,
			    XmNtopAttachment, XmATTACH_FORM,
			    XmNtopOffset, WOFFSET,
			    XmNleftAttachment, XmATTACH_WIDGET,
			    XmNleftWidget, wid,
			    XmNleftOffset, WOFFSET,
			    NULL);
  AddStaticCallback(wFormat, XmNactivateCallback, &PltApp::DoDoneNumberFormat,
		    (XtPointer) bKillNFWindow);
  
  XtManageChild(wDoneButton);
  XtManageChild(wApplyButton);
  XtManageChild(wCancelButton);

  XtPopup(wNumberFormatTopLevel, XtGrabNone);
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wNumberFormatTopLevel));
  bFormatShowing = true;
}


// -------------------------------------------------------------------
void PltApp::DoDoneNumberFormat(Widget, XtPointer client_data, XtPointer) {
  bool bKillNFWindow = (bool) client_data;
  paletteDrawn = false;

  aString tempFormatString;

  char temp[64];
  strcpy(temp, XmTextFieldGetString(wFormat));
  if(temp[0] != '%') {
    tempFormatString  = "%";
    tempFormatString += temp;
  } else {
    tempFormatString = temp;
  }
  // unexhaustive string check to prevent errors
  bool stringOk(true);
  for(int i(0); i < tempFormatString.length(); ++i) {
    if(tempFormatString[i] == 's' || tempFormatString[i] == 'u'
        || tempFormatString[i] == 'p')
    {
      stringOk = false;
    }
  }
  if(stringOk) {
    SetNewFormatString(tempFormatString);
  } else {
    cerr << "Error in format string." << endl;
  }

  if(bKillNFWindow) {
    XtDestroyWidget(wNumberFormatTopLevel);
  }

}


// -------------------------------------------------------------------
void PltApp::DoCancelNumberFormat(Widget, XtPointer, XtPointer) {
  XtDestroyWidget(wNumberFormatTopLevel);
}


// -------------------------------------------------------------------
void PltApp::DestroyNumberFormatWindow(Widget, XtPointer, XtPointer) {
  bFormatShowing = false;
}


// -------------------------------------------------------------------
void PltApp::DoDoneSetRange(Widget, XtPointer client_data, XtPointer) {
  bool bKillSRWindow = (bool) client_data;
  paletteDrawn = false;

  int np;
  Real umin(atof(XmTextFieldGetString(wUserMin)));
  Real umax(atof(XmTextFieldGetString(wUserMax)));

  for(int iFrame(0); iFrame < animFrames; ++iFrame) {
    pltAppState->SetMinMax(USERMINMAX, iFrame,
			   pltAppState->CurrentDerivedNumber(),
			   umin, umax);
  }

  if(currentRangeType != pltAppState->GetMinMaxRangeType() ||
     currentRangeType == USERMINMAX || currentRangeType == FILEUSERMINMAX)
  {
    pltAppState->SetMinMaxRangeType(currentRangeType);	
    for(np = 0; np < NPLANES; ++np) {
      amrPicturePtrArray[np]->APMakeImages(pltPaletteptr);
    }
#if (BL_SPACEDIM == 3)
    //Clear();
#endif
    if(animating2d) {
      ResetAnimation();
      DirtyFrames(); 
      ShowFrame();
    }
#if (BL_SPACEDIM == 3)
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
    projPicturePtr->GetVolRenderPtr()->InvalidateSWFData();
    projPicturePtr->GetVolRenderPtr()->InvalidateVPData();
    XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
    if(XmToggleButtonGetState(wAutoDraw) || showing3dRender) {
      DoRender();
    }
#endif
#endif
  }
  if(bKillSRWindow) {
    XtDestroyWidget(wSetRangeTopLevel);
  }

  if(datasetShowing) {
    datasetPtr->DoRaise();
    int hdir, vdir, sdir;
    if(activeView==ZPLANE) { hdir = XDIR; vdir = YDIR; sdir = ZDIR; }
    if(activeView==YPLANE) { hdir = XDIR; vdir = ZDIR; sdir = YDIR; }
    if(activeView==XPLANE) { hdir = YDIR; vdir = ZDIR; sdir = XDIR; }
    datasetPtr->DatasetRender(trueRegion, amrPicturePtrArray[activeView], this,
                              pltAppState, hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  }
}


// -------------------------------------------------------------------
void PltApp::DoCancelSetRange(Widget, XtPointer, XtPointer) {
  XtDestroyWidget(wSetRangeTopLevel);
  setRangeShowing = false;
}


// -------------------------------------------------------------------
void PltApp::DestroySetRangeWindow(Widget, XtPointer, XtPointer) {
  if(bSetRangeRedraw == false) {
    setRangeShowing = false;
  }
  bSetRangeRedraw = false;
}


// -------------------------------------------------------------------
void PltApp::DoUserMin(Widget, XtPointer, XtPointer) {
  if(currentRangeType == GLOBALMINMAX || currentRangeType == FILEGLOBALMINMAX) {
    XtVaSetValues(wRangeRadioButton[BGLOBALMINMAX], XmNset, false, NULL);
  }
  if(currentRangeType == SUBREGIONMINMAX ||
     currentRangeType == FILESUBREGIONMINMAX)
  {
    XtVaSetValues(wRangeRadioButton[BSUBREGIONMINMAX], XmNset, false, NULL);
  }
  XtVaSetValues(wRangeRadioButton[BUSERMINMAX], XmNset, true, NULL);
  if(bFileRangeButtonSet) {
    currentRangeType = FILEUSERMINMAX;
  } else {
    currentRangeType = USERMINMAX;
  }
  bool bKillSRWindow(true);
  DoDoneSetRange(NULL, (XtPointer) bKillSRWindow, NULL);
}


// -------------------------------------------------------------------
void PltApp::DoUserMax(Widget, XtPointer, XtPointer) {
  if(currentRangeType == GLOBALMINMAX || currentRangeType == FILEGLOBALMINMAX) {
    XtVaSetValues(wRangeRadioButton[BGLOBALMINMAX], XmNset, false, NULL);
  }
  if(currentRangeType == SUBREGIONMINMAX ||
     currentRangeType == FILESUBREGIONMINMAX)
  {
    XtVaSetValues(wRangeRadioButton[BSUBREGIONMINMAX], XmNset, false, NULL);
  }
  XtVaSetValues(wRangeRadioButton[BUSERMINMAX], XmNset, true, NULL);
  if(bFileRangeButtonSet) {
    currentRangeType = FILEUSERMINMAX;
  } else {
    currentRangeType = USERMINMAX;
  }
  bool bKillSRWindow(true);
  DoDoneSetRange(NULL, (XtPointer) bKillSRWindow, NULL);
}


// -------------------------------------------------------------------
void PltApp::DoBoxesButton(Widget, XtPointer, XtPointer) {
  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }
  pltAppState->SetShowingBoxes( ! pltAppState->GetShowingBoxes());
  for(int np(0); np < NPLANES; ++np) {
    amrPicturePtrArray[np]->DoExposePicture();
  }
#if (BL_SPACEDIM == 3)
    projPicturePtr->MakeBoxes(); 
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
    if( ! XmToggleButtonGetState(wAutoDraw)) {
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
      DoExposeTransDA();
    }
#else
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
      DoExposeTransDA();
#endif
#endif

  // draw a bounding box around the image
  /*
  int imageSizeX = amrPicturePtrArray[ZPLANE]->ImageSizeH();
  int imageSizeY = amrPicturePtrArray[ZPLANE]->ImageSizeV();
  XSetForeground(display, xgc, pltPaletteptr->WhiteIndex());
  XDrawLine(display, XtWindow(wPlotPlane[ZPLANE]), xgc,
            0, imageSizeY, imageSizeX, imageSizeY);
  XDrawLine(display, XtWindow(wPlotPlane[ZPLANE]), xgc,
            imageSizeX, 0, imageSizeX, imageSizeY);
  */
}


// -------------------------------------------------------------------
void PltApp::DoOpenPalFile(Widget w, XtPointer, XtPointer call_data) {
  char *palfile;
  if( ! XmStringGetLtoR(((XmFileSelectionBoxCallbackStruct *) call_data)->value,
		        XmSTRING_DEFAULT_CHARSET, &palfile))
  {
    cerr << "PltApp::DoOpenPalFile : system error" << endl;
    return;
  }
  XtPopdown(XtParent(w));
  pltPaletteptr->ChangeWindowPalette(palfile, XtWindow(wAmrVisTopLevel));
# ifdef BL_VOLUMERENDER
  projPicturePtr->GetVolRenderPtr()->SetTransferProperties();
  projPicturePtr->GetVolRenderPtr()->InvalidateVPData();
  showing3dRender = false;
  if(XmToggleButtonGetState(wAutoDraw)) {
    DoRender(wAutoDraw, NULL, NULL);
  }
# endif
  palFilename = palfile;
  XtFree(palfile);
  
  XYplotparameters->ResetPalette(pltPaletteptr);
  for(int np(0); np != BL_SPACEDIM; ++np) {
    if(XYplotwin[np]) {
      XYplotwin[np]->SetPalette();
    }
  }
}


// -------------------------------------------------------------------
XYPlotDataList *PltApp::CreateLinePlot(int V, int sdir, int mal, int ix,
				       const aString *derived)
{
  const AmrData &amrData(dataServicesPtr[currentFrame]->AmrDataRef());
  
  // Create an array of boxes corresponding to the intersected line.
  int tdir, dir1;
#if (BL_SPACEDIM == 3)
  int dir2;
#endif
  switch (V) {
  case ZPLANE:
    tdir = (sdir == XDIR) ? YDIR : XDIR;
    dir1 = tdir;
#if (BL_SPACEDIM == 3)
    dir2 = ZDIR;
    break;
  case YPLANE:
    if(sdir == XDIR) {
      tdir = ZDIR;
      dir1 = YDIR;
      dir2 = ZDIR;
    } else {
      tdir = XDIR;
      dir1 = XDIR;
      dir2 = YDIR;
    }
    break;
  case XPLANE:
    dir1 = XDIR;
    if(sdir == YDIR) {
      tdir = ZDIR; 
    } else {
      tdir = YDIR;
    }
    dir1 = XDIR;
    dir2 = tdir;
    break;
#endif
  }
  Array<Box> trueRegion(mal + 1);
  trueRegion[mal] = amrPicturePtrArray[V]->GetSliceBox(pltAppState->FinestLevel());
  trueRegion[mal].setSmall(tdir, ivLowOffsetMAL[tdir] + ix);
  trueRegion[mal].setBig(tdir, trueRegion[mal].smallEnd(tdir));
  int lev;
  for(lev = mal - 1; lev >= 0; --lev) {
    trueRegion[lev] = trueRegion[mal];
    trueRegion[lev].coarsen(CRRBetweenLevels(lev, mal, amrData.RefRatio()));
  }
  // Create an array of titles corresponding to the intersected line.
  Array<Real> XdX(mal+1);
  Array<char *> intersectStr(mal + 1);
  
#if (BL_SPACEDIM == 3)
  char buffer[128];
  sprintf(buffer, "%s%s %s%s",
	  (dir1 == XDIR) ? "X=" : "Y=", pltAppState->GetFormatString().c_str(),
	  (dir2 == YDIR) ? "Y=" : "Z=", pltAppState->GetFormatString().c_str());
#endif
  for(lev = 0; lev <= mal; ++lev) {
    XdX[lev] = amrData.DxLevel()[lev][sdir];
    intersectStr[lev] = new char[128];
#if (BL_SPACEDIM == 2)
    sprintf(intersectStr[lev], ((dir1 == XDIR) ? "X=" : "Y="));
    sprintf(intersectStr[lev]+2, pltAppState->GetFormatString().c_str(),
	    gridOffset[dir1] +
	    (0.5 + trueRegion[lev].smallEnd(dir1))*amrData.DxLevel()[lev][dir1]);
#elif (BL_SPACEDIM == 3)
    sprintf(intersectStr[lev], buffer,
	    amrData.DxLevel()[lev][dir1] * (0.5 + trueRegion[lev].smallEnd(dir1)) +
	    gridOffset[dir1],
	    amrData.DxLevel()[lev][dir2] * (0.5 + trueRegion[lev].smallEnd(dir2)) +
	    gridOffset[dir2]);
#endif	    
  }
  XYPlotDataList *newlist = new XYPlotDataList(*derived, mal, ix,
				     amrData.RefRatio(),
		                     XdX, intersectStr, gridOffset[sdir]);
  bool lineOK;
  DataServices::Dispatch(DataServices::LineValuesRequest,
			 dataServicesPtr[currentFrame],
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
void PltApp::DoRubberBanding(Widget, XtPointer client_data, XtPointer call_data) {

  XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct *) call_data;

  if(cbs->event->xany.type != ButtonPress) {
    return;
  }

  int scale(pltAppState->CurrentScale());
  int V((unsigned long) client_data);
  int imageHeight(amrPicturePtrArray[V]->ImageSizeV() - 1);
  int imageWidth(amrPicturePtrArray[V]->ImageSizeH() - 1);
  int oldX(max(0, min(imageWidth,  cbs->event->xbutton.x)));
  int oldY(max(0, min(imageHeight, cbs->event->xbutton.y)));
  int mal(pltAppState->MaxAllowableLevel());
  int minDrawnLevel(pltAppState->MinDrawnLevel());
  int maxDrawnLevel(pltAppState->MaxDrawnLevel());
  const AmrData &amrData(dataServicesPtr[currentFrame]->AmrDataRef());
  int rootX, rootY;
  unsigned int inputMask;
  Window whichRoot, whichChild;
#if (BL_SPACEDIM == 3)
  int x1, y1, z1, x2, y2, z2, rStartPlane;
  for(int np = 0; np != BL_SPACEDIM; ++np) {
    amrPicturePtrArray[np]->DoStop();
  }
#endif

  servingButton = cbs->event->xbutton.button;
  if(animating2d) {
    ResetAnimation();
  }

  XSetForeground(display, rbgc, 120);
  XChangeActivePointerGrab(display, PointerMotionHintMask |
			   ButtonMotionMask | ButtonReleaseMask |
			   OwnerGrabButtonMask, cursor, CurrentTime);
  XGrabServer(display);

  if(servingButton == 1) {
    int rectDrawn(false);
    int anchorX(oldX);
    int anchorY(oldY);
    int newX, newY;

    while(true) {
      XNextEvent(display, &nextEvent);
      
      switch(nextEvent.type) {
	
      case MotionNotify:
	
	if(rectDrawn) {   // undraw the old rectangle(s)
	  rWidth  = abs(oldX-anchorX);
	  rHeight = abs(oldY-anchorY);
	  rStartX = (anchorX < oldX) ? anchorX : oldX;
	  rStartY = (anchorY < oldY) ? anchorY : oldY;
	  XDrawRectangle(display, amrPicturePtrArray[V]->PictureWindow(),
			 rbgc, rStartX, rStartY, rWidth, rHeight);
#if (BL_SPACEDIM == 3)
	  // 3D sub domain selecting
	  switch (V) {
	  case ZPLANE:
	    XDrawRectangle(display, amrPicturePtrArray[YPLANE]->PictureWindow(),
			   rbgc, rStartX, startcutY[YPLANE], rWidth,
			   abs(finishcutY[YPLANE]-startcutY[YPLANE]));
	    rStartPlane = (anchorY < oldY) ? oldY : anchorY;
	    XDrawRectangle(display, amrPicturePtrArray[XPLANE]->PictureWindow(),
			   rbgc, imageHeight-rStartPlane, startcutY[XPLANE],
			   rHeight,
			   abs(finishcutY[XPLANE]-startcutY[XPLANE]));
	    break;
	  case YPLANE:
	    XDrawRectangle(display, amrPicturePtrArray[ZPLANE]->PictureWindow(),
			   rbgc, rStartX, startcutY[ZPLANE], rWidth,
			   abs(finishcutY[ZPLANE]-startcutY[ZPLANE]));
	    XDrawRectangle(display, amrPicturePtrArray[XPLANE]->PictureWindow(),
			   rbgc, startcutX[XPLANE], rStartY,
			   abs(finishcutX[XPLANE]-startcutX[XPLANE]),
			   rHeight);
	    break;
	  default: // XPLANE
	    rStartPlane = (anchorX < oldX) ? oldX : anchorX;
	    XDrawRectangle(display, amrPicturePtrArray[ZPLANE]->PictureWindow(),
			   rbgc, startcutX[ZPLANE], imageWidth-rStartPlane,
			   abs(finishcutX[ZPLANE]-startcutX[ZPLANE]), rWidth);
	    XDrawRectangle(display, amrPicturePtrArray[YPLANE]->PictureWindow(),
			   rbgc, startcutX[YPLANE], rStartY,
			   abs(finishcutX[YPLANE]-startcutX[YPLANE]),
			   rHeight);
	  }
#endif
	}

	DoDrawPointerLocation(None, (XtPointer) V, &nextEvent);

	while(XCheckTypedEvent(display, MotionNotify, &nextEvent)) {
	  ;  // do nothing
	}
	
	XQueryPointer(display, amrPicturePtrArray[V]->PictureWindow(),
		      &whichRoot, &whichChild,
		      &rootX, &rootY, &newX, &newY, &inputMask);
	
	newX = max(0, min(imageWidth,  newX));
	newY = max(0, min(imageHeight, newY));
	rWidth  = abs(newX-anchorX);   // draw the new rectangle
	rHeight = abs(newY-anchorY);
	rStartX = (anchorX < newX) ? anchorX : newX;
	rStartY = (anchorY < newY) ? anchorY : newY;
	XDrawRectangle(display, amrPicturePtrArray[V]->PictureWindow(),
		       rbgc, rStartX, rStartY, rWidth, rHeight);
	rectDrawn = true;
	
	oldX = newX;
	oldY = newY;
	
#if (BL_SPACEDIM == 3)
	// 3D sub domain selecting
	startcutX[V]  = rStartX;
	startcutY[V]  = rStartY;
	finishcutX[V] = rStartX + rWidth;
	finishcutY[V] = rStartY + rHeight;
	switch (V) {
	case ZPLANE:
	  startcutX[YPLANE] = startcutX[V];
	  finishcutX[YPLANE] = finishcutX[V];
	  startcutX[XPLANE] = imageHeight - startcutY[V];
	  finishcutX[XPLANE] = imageHeight - finishcutY[V];
	  // draw in other planes
	  XDrawRectangle(display, amrPicturePtrArray[YPLANE]->PictureWindow(),
			 rbgc, rStartX, startcutY[YPLANE], rWidth,
			 abs(finishcutY[YPLANE]-startcutY[YPLANE]));
	  rStartPlane = (anchorY < newY) ? newY : anchorY;
	  XDrawRectangle(display, amrPicturePtrArray[XPLANE]->PictureWindow(),
			 rbgc, imageHeight-rStartPlane, startcutY[XPLANE],
			 rHeight,
			 abs(finishcutY[XPLANE]-startcutY[XPLANE]));
	  break;
	case YPLANE:
	  startcutX[ZPLANE] = startcutX[V];
	  finishcutX[ZPLANE] = finishcutX[V];
	  startcutY[XPLANE] = startcutY[V];
	  finishcutY[XPLANE] = finishcutY[V];
	  XDrawRectangle(display, amrPicturePtrArray[ZPLANE]->PictureWindow(),
			 rbgc, rStartX, startcutY[ZPLANE], rWidth,
			 abs(finishcutY[ZPLANE]-startcutY[ZPLANE]));
	  XDrawRectangle(display, amrPicturePtrArray[XPLANE]->PictureWindow(),
			 rbgc, startcutX[XPLANE], rStartY,
			 abs(finishcutX[XPLANE]-startcutX[XPLANE]), rHeight);
	  break;
	default: // XPLANE
	  startcutY[YPLANE] = startcutY[V];
	  finishcutY[YPLANE] = finishcutY[V];
	  startcutY[ZPLANE] = imageWidth - startcutX[V];
	  finishcutY[ZPLANE] = imageWidth - finishcutX[V];
	  rStartPlane = (anchorX < newX) ? newX : anchorX;
	  XDrawRectangle(display, amrPicturePtrArray[ZPLANE]->PictureWindow(),
			 rbgc, startcutX[ZPLANE], imageWidth-rStartPlane,
			 abs(finishcutX[ZPLANE]-startcutX[ZPLANE]), rWidth);
	  XDrawRectangle(display, amrPicturePtrArray[YPLANE]->PictureWindow(),
			 rbgc, startcutX[YPLANE], rStartY,
			 abs(finishcutX[YPLANE]-startcutX[YPLANE]), rHeight);
	}
	
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
	if( ! XmToggleButtonGetState(wAutoDraw))
#endif
	  {
	    x1 = startcutX[ZPLANE]/scale + ivLowOffsetMAL[XDIR];
	    y1 = startcutX[XPLANE]/scale + ivLowOffsetMAL[YDIR];
	    z1 = (amrPicturePtrArray[YPLANE]->ImageSizeV()-1 -
		  startcutY[YPLANE])/scale + ivLowOffsetMAL[ZDIR];
	    x2 = finishcutX[ZPLANE]/scale + ivLowOffsetMAL[XDIR];
	    y2 = finishcutX[XPLANE]/scale + ivLowOffsetMAL[YDIR];
	    z2 = (amrPicturePtrArray[YPLANE]->ImageSizeV()-1 -
		  finishcutY[YPLANE])/scale + ivLowOffsetMAL[ZDIR];
	    
	    if(z2 > 65536) {    // fix bad -z values
	      z2 = amrPicturePtrArray[ZPLANE]->GetSubDomain()[mal].smallEnd(ZDIR);
	    }
	    IntVect ivmin(min(x1,x2), min(y1,y2), min(z1,z2));
	    IntVect ivmax(max(x1,x2), max(y1,y2), max(z1,z2));
	    Box xyz12(ivmin, ivmax);
	    xyz12 &= amrPicturePtrArray[ZPLANE]->GetSubDomain()[mal];
	    
	    projPicturePtr->SetSubCut(xyz12); 
	    //projPicturePtr->MakeBoxes();
	    XClearWindow(display, XtWindow(wTransDA));
	    DoExposeTransDA();
	  }
#endif
	
	break;
	
      case ButtonRelease: {
	XUngrabServer(display);  // giveitawaynow
	
	startX = (max(0, min(imageWidth,  anchorX))) / scale;
	startY = (max(0, min(imageHeight, anchorY))) / scale;
	endX   = (max(0, min(imageWidth,  nextEvent.xbutton.x))) / scale;
	endY   = (max(0, min(imageHeight, nextEvent.xbutton.y))) / scale;
	
	// make "aligned" box with correct size, converted to AMR space.
	selectionBox.setSmall(XDIR, min(startX, endX));
	selectionBox.setSmall(YDIR, ((imageHeight + 1) / scale) -
				     max(startY, endY) - 1);
	selectionBox.setBig(XDIR, max(startX, endX));
	selectionBox.setBig(YDIR, ((imageHeight + 1) / scale)  -
				     min(startY, endY) - 1);
	
	// selectionBox is now at the maxAllowableLevel because
	// it is defined on the pixmap ( / scale)
	
	if(anchorX == nextEvent.xbutton.x && anchorY == nextEvent.xbutton.y) {
	  // data at click
	  int y, intersectedLevel(-1);
	  Box temp, intersectedGrid;
	  Array<Box> trueRegion(mal+1);
	  int plane(amrPicturePtrArray[V]->GetSlice());
	  
	  trueRegion[mal] = selectionBox;
	  
	  // convert to point box
	  trueRegion[mal].setBig(XDIR, trueRegion[mal].smallEnd(XDIR));
	  trueRegion[mal].setBig(YDIR, trueRegion[mal].smallEnd(YDIR));
	  
	  if(V == ZPLANE) {
	    trueRegion[mal].shift(XDIR, ivLowOffsetMAL[XDIR]);
	    trueRegion[mal].shift(YDIR, ivLowOffsetMAL[YDIR]);
	    if(BL_SPACEDIM == 3) {
	      trueRegion[mal].setSmall(ZDIR, plane);
	      trueRegion[mal].setBig(ZDIR, plane);
	    }	
	  }	
	  if(V == YPLANE) {
	    trueRegion[mal].setSmall(ZDIR, trueRegion[mal].smallEnd(YDIR));
	    trueRegion[mal].setBig(ZDIR, trueRegion[mal].bigEnd(YDIR));
	    trueRegion[mal].setSmall(YDIR, plane);
	    trueRegion[mal].setBig(YDIR, plane);
	    trueRegion[mal].shift(XDIR, ivLowOffsetMAL[XDIR]);
	    trueRegion[mal].shift(ZDIR, ivLowOffsetMAL[ZDIR]);
	  }
	  if(V == XPLANE) {
	    trueRegion[mal].setSmall(ZDIR, trueRegion[mal].smallEnd(YDIR));
	    trueRegion[mal].setBig(ZDIR, trueRegion[mal].bigEnd(YDIR));
	    trueRegion[mal].setSmall(YDIR, trueRegion[mal].smallEnd(XDIR));
	    trueRegion[mal].setBig(YDIR, trueRegion[mal].bigEnd(XDIR));
	    trueRegion[mal].setSmall(XDIR, plane);
	    trueRegion[mal].setBig(XDIR, plane);
	    trueRegion[mal].shift(YDIR, ivLowOffsetMAL[YDIR]);
	    trueRegion[mal].shift(ZDIR, ivLowOffsetMAL[ZDIR]);
	  }
	  
	  for(y = mal - 1; y >= 0; --y) {
	    trueRegion[y] = trueRegion[mal];
	    trueRegion[y].coarsen(CRRBetweenLevels(y, mal, amrData.RefRatio()));
	    trueRegion[y].setBig(XDIR, trueRegion[y].smallEnd(XDIR));
	    trueRegion[y].setBig(YDIR, trueRegion[y].smallEnd(YDIR));
	  }
	  
	  bool goodIntersect;
	  Real dataValue;
	  DataServices::Dispatch(DataServices::PointValueRequest,
				 dataServicesPtr[currentFrame],
				 trueRegion.size(),
				 (void *) (trueRegion.dataPtr()),
				 (void *) &pltAppState->CurrentDerived(),
				 minDrawnLevel, maxDrawnLevel,
				 &intersectedLevel, &intersectedGrid,
				 &dataValue, &goodIntersect);
	  char dataValueCharString[LINELENGTH];
	  sprintf(dataValueCharString, pltAppState->GetFormatString().c_str(), dataValue);
	  aString dataValueString(dataValueCharString);
	  ostrstream buffout(buffer, BUFSIZ);
	  if(goodIntersect) {
	    buffout << '\n';
	    buffout << "level = " << intersectedLevel << '\n';
	    buffout << "point = " << trueRegion[intersectedLevel].smallEnd()
		    << '\n';
	    buffout << "grid  = " << intersectedGrid << '\n';
	    buffout << "loc   = (";
	    for(int idx = 0; idx != BL_SPACEDIM; ++idx) {
	      if(idx != 0) {
	        buffout << ", ";
	      }
	      buffout << gridOffset[idx] +
		(0.5 + trueRegion[intersectedLevel].smallEnd()[idx]) *
		amrData.DxLevel()[intersectedLevel][idx];
	    }
	    buffout << ")\n";
	    buffout << "value = " << dataValueString << '\n';
	  } else buffout << "Bad point at mouse click" << '\n';
	  
	  buffout << ends;  // end the string
	  PrintMessage(buffer);

	} else {
	  
	  // tell the amrpicture about the box
	  activeView = V;
	  if(startX < endX) { // box in scaled pixmap space
	    startX = selectionBox.smallEnd(XDIR) * scale;
	    endX   = selectionBox.bigEnd(XDIR)   * scale;
	  } else {	
	    startX = selectionBox.bigEnd(XDIR)   * scale;
	    endX   = selectionBox.smallEnd(XDIR) * scale;
	  }
	  
	  if(startY < endY) {
	    startY = imageHeight - selectionBox.bigEnd(YDIR)   * scale;
	    endY   = imageHeight - selectionBox.smallEnd(YDIR) * scale;
	  } else {
	    startY = imageHeight - selectionBox.smallEnd(YDIR) * scale;
	    endY   = imageHeight - selectionBox.bigEnd(YDIR)   * scale;
	  }
	  int nodeAdjustment = (scale - 1) * selectionBox.type()[YDIR];
	  startY -= nodeAdjustment;
	  endY   -= nodeAdjustment;
	  
	  amrPicturePtrArray[V]->SetRegion(startX, startY, endX, endY);
	  
#if (BL_SPACEDIM == 3)
	  amrPicturePtrArray[V]->SetSubCut(startcutX[V], startcutY[V],
					   finishcutX[V], finishcutY[V]);
	  if(V == ZPLANE) {
	    amrPicturePtrArray[YPLANE]->SetSubCut(startcutX[V], -1,
						  finishcutX[V], -1);
	    amrPicturePtrArray[XPLANE]->SetSubCut(imageHeight-startcutY[V],-1,
						  imageHeight-finishcutY[V],-1);
	  }
	  if(V == YPLANE) {
	    amrPicturePtrArray[ZPLANE]->SetSubCut(startcutX[V], -1,
						  finishcutX[V], -1);
	    amrPicturePtrArray[XPLANE]->SetSubCut(-1, startcutY[V], 
						  -1, finishcutY[V]);
	  }
	  if(V == XPLANE) {
	    amrPicturePtrArray[ZPLANE]->SetSubCut(-1, imageWidth-startcutX[V],
						  -1, imageWidth-finishcutX[V]);
	    amrPicturePtrArray[YPLANE]->SetSubCut(-1, startcutY[V], 
						  -1, finishcutY[V]);
	  }
#endif
	  
	  for(int np(0); np < NPLANES; ++np) {
	    amrPicturePtrArray[np]->DoExposePicture();
	  }
	}
      }
      default:
	return;
      }  // end switch
    }  // end while(true)
  }
  if(servingButton == 2) {
    XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
	      rbgc, 0, oldY, imageWidth, oldY);
    
#if (BL_SPACEDIM == 3)
    switch (V) {
    case ZPLANE:
      XDrawLine(display, amrPicturePtrArray[XPLANE]->PictureWindow(),
		rbgc, imageHeight-oldY, 0, imageHeight-oldY,
		amrPicturePtrArray[XPLANE]->ImageSizeV());
      break;
    case YPLANE:
      XDrawLine(display, amrPicturePtrArray[XPLANE]->PictureWindow(), rbgc,
		0, oldY, amrPicturePtrArray[XPLANE]->ImageSizeH(), oldY);
      break;
    default: // XPLANE
      XDrawLine(display, amrPicturePtrArray[YPLANE]->PictureWindow(), rbgc,
		0, oldY, amrPicturePtrArray[YPLANE]->ImageSizeH(), oldY);
    }
#endif
    int tempi;
    while(true) {
      XNextEvent(display, &nextEvent);
      
      switch(nextEvent.type) {
	
      case MotionNotify:
	
	XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
		  rbgc, 0, oldY, imageWidth, oldY);
#if (BL_SPACEDIM == 3)
	// undraw in other planes
	if(V == ZPLANE) {
	  XDrawLine(display, amrPicturePtrArray[XPLANE]->PictureWindow(),
		    rbgc, imageHeight-oldY, 0, imageHeight-oldY,
		    amrPicturePtrArray[XPLANE]->ImageSizeV());
	} else if(V == YPLANE) {
	  XDrawLine(display, amrPicturePtrArray[XPLANE]->PictureWindow(), rbgc,
		    0, oldY, amrPicturePtrArray[XPLANE]->ImageSizeH(), oldY);
	} else if(V == XPLANE) {
	  XDrawLine(display, amrPicturePtrArray[YPLANE]->PictureWindow(), rbgc,
		    0, oldY, amrPicturePtrArray[YPLANE]->ImageSizeH(), oldY);
	}
#endif

	DoDrawPointerLocation(None, (XtPointer) V, &nextEvent);
	while(XCheckTypedEvent(display, MotionNotify, &nextEvent)) {
	  ;  // do nothing
	}
	XQueryPointer(display, amrPicturePtrArray[V]->PictureWindow(),
		      &whichRoot, &whichChild,
		      &rootX, &rootY, &oldX, &oldY, &inputMask);
	
	// draw the new line
	XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
		  rbgc, 0, oldY, imageWidth, oldY);
#if (BL_SPACEDIM == 3)
	switch (V) {
	case ZPLANE:
	  XDrawLine(display, amrPicturePtrArray[XPLANE]->PictureWindow(),
		    rbgc, imageHeight-oldY, 0, imageHeight-oldY,
		    amrPicturePtrArray[XPLANE]->ImageSizeV());
	  break;
	case YPLANE:
	  XDrawLine(display, amrPicturePtrArray[XPLANE]->PictureWindow(), rbgc,
		    0, oldY, amrPicturePtrArray[XPLANE]->ImageSizeH(), oldY);
	  break;
	default: // XPLANE
	  XDrawLine(display, amrPicturePtrArray[YPLANE]->PictureWindow(), rbgc,
		    0, oldY, amrPicturePtrArray[YPLANE]->ImageSizeH(), oldY);
	}
#endif
	
	break;
	
      case ButtonRelease:
	XUngrabServer(display);
	tempi = max(0, min(imageHeight, nextEvent.xbutton.y));
	amrPicturePtrArray[V]->SetHLine(tempi);
#if (BL_SPACEDIM == 3)
	if(V == ZPLANE) {
	  amrPicturePtrArray[XPLANE]->SetVLine(amrPicturePtrArray[XPLANE]->
				                       ImageSizeH()-1 - tempi);
	}
	if(V == YPLANE) {
	  amrPicturePtrArray[XPLANE]->SetHLine(tempi);
	}
	if(V == XPLANE) {
	  amrPicturePtrArray[YPLANE]->SetHLine(tempi);
	}

	if( ! (cbs->event->xbutton.state & ShiftMask)) {
	  if(V == ZPLANE) {
	    amrPicturePtrArray[YPLANE]->
	      APChangeSlice((imageHeight - tempi)/scale + ivLowOffsetMAL[YDIR]);
	  }
	  if(V == YPLANE) {
	    amrPicturePtrArray[ZPLANE]->
	      APChangeSlice((imageHeight - tempi)/scale + ivLowOffsetMAL[ZDIR]);
	  }
	  if(V == XPLANE) {
	    amrPicturePtrArray[ZPLANE]->
	      APChangeSlice((imageHeight - tempi)/scale + ivLowOffsetMAL[ZDIR]);
	  }
	  for(int np(0); np < NPLANES; ++np) {
	    amrPicturePtrArray[np]->DoExposePicture();
	    projPicturePtr->ChangeSlice(np, amrPicturePtrArray[np]->GetSlice());
	  }

	  projPicturePtr->MakeSlices();
	  XClearWindow(display, XtWindow(wTransDA));
	  DoExposeTransDA();
	  DoExposeRef();
	  return;
	}
#endif

	for(int np(0); np != NPLANES; ++np) {
	  amrPicturePtrArray[np]->DoExposePicture();
	}

	if(oldY >= 0 && oldY <= imageHeight) {
	  int sdir;
	  switch (V) {
	    case ZPLANE:
	      sdir = XDIR;
	    break;
	    case YPLANE:
	      sdir = XDIR;
	    break;
	    case XPLANE:
	      sdir = YDIR;
	    break;
	  }
	  XYPlotDataList *newlist = CreateLinePlot(V, sdir, mal,
				      (imageHeight + 1) / scale - 1 - oldY / scale,
			              &pltAppState->CurrentDerived());
	  if(newlist) {
	    newlist->SetLevel(maxDrawnLevel);
	    if(XYplotwin[sdir] == NULL) {
	      int fnl(fileNames[0].length() - 1);
	      while(fnl > -1 && fileNames[0][fnl] != '/') {
	        --fnl;
	      }
	      XYplotwin[sdir] = new XYPlotWin(&fileNames[0][fnl+1],
					      appContext, wAmrVisTopLevel,
					      this, sdir, currentFrame);
	    }
	    XYplotwin[sdir]->AddDataList(newlist);
	  }
	}
	
      default:
	return;
      }  // end switch
    }  // end while(true)
  }
  if(servingButton == 3) {
    int tempi;
    XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
	      rbgc, oldX, 0, oldX, imageHeight);
#if (BL_SPACEDIM == 3)
    if(V == ZPLANE) {
      XDrawLine(display, amrPicturePtrArray[YPLANE]->PictureWindow(), rbgc,
		oldX, 0, oldX, amrPicturePtrArray[YPLANE]->ImageSizeV());
    }
    if(V == YPLANE) {
      XDrawLine(display, amrPicturePtrArray[ZPLANE]->PictureWindow(),rbgc,
		oldX, 0, oldX, amrPicturePtrArray[ZPLANE]->ImageSizeV());
    }
    if(V == XPLANE) {
      XDrawLine(display, amrPicturePtrArray[ZPLANE]->PictureWindow(), rbgc,
		0, imageWidth - oldX, amrPicturePtrArray[ZPLANE]->ImageSizeH(),
		imageWidth - oldX);
    }
#endif
    while(true) {
      XNextEvent(display, &nextEvent);
      
      switch(nextEvent.type) {
	
      case MotionNotify:
	
	// undraw the old line
	XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
		  rbgc, oldX, 0, oldX, imageHeight);
#if (BL_SPACEDIM == 3)
	// undraw in other planes
	if(V == ZPLANE) {
	  XDrawLine(display, amrPicturePtrArray[YPLANE]->PictureWindow(), rbgc,
		    oldX, 0, oldX, amrPicturePtrArray[YPLANE]->ImageSizeV());
	}
	if(V == YPLANE) {
	  XDrawLine(display, amrPicturePtrArray[ZPLANE]->PictureWindow(), rbgc,
		    oldX, 0, oldX, amrPicturePtrArray[ZPLANE]->ImageSizeV());
	}
	if(V == XPLANE) {
	  XDrawLine(display, amrPicturePtrArray[ZPLANE]->PictureWindow(), rbgc,
		    0, imageWidth-oldX, amrPicturePtrArray[ZPLANE]->ImageSizeH(),
		    imageWidth-oldX);
	}
#endif

	DoDrawPointerLocation(None, (XtPointer) V, &nextEvent);
	while(XCheckTypedEvent(display, MotionNotify, &nextEvent)) {
	  ;  // do nothing
	}
	XQueryPointer(display, amrPicturePtrArray[V]->PictureWindow(),
		      &whichRoot, &whichChild,
		      &rootX, &rootY, &oldX, &oldY, &inputMask);
	
	// draw the new line
	XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
		  rbgc, oldX, 0, oldX, imageHeight);
#if (BL_SPACEDIM == 3)
	if(V == ZPLANE) {
	  XDrawLine(display, amrPicturePtrArray[YPLANE]->PictureWindow(), rbgc,
		    oldX, 0, oldX, amrPicturePtrArray[YPLANE]->ImageSizeV());
	}
	if(V == YPLANE) {
	  XDrawLine(display, amrPicturePtrArray[ZPLANE]->PictureWindow(), rbgc,
		    oldX, 0, oldX, amrPicturePtrArray[ZPLANE]->ImageSizeV());
	}
	if(V == XPLANE) {
	  XDrawLine(display, amrPicturePtrArray[ZPLANE]->PictureWindow(), rbgc,
		    0, imageWidth - oldX, amrPicturePtrArray[ZPLANE]->ImageSizeH(),
		    imageWidth - oldX);
	}
#endif
	
	break;
	
      case ButtonRelease:
	XUngrabServer(display);
	tempi = max(0, min(imageWidth, nextEvent.xbutton.x));
	amrPicturePtrArray[V]->SetVLine(tempi);
#if (BL_SPACEDIM == 3)
	if(V == ZPLANE) {
	  amrPicturePtrArray[YPLANE]->SetVLine(tempi);
	}
	if(V == YPLANE) {
	  amrPicturePtrArray[ZPLANE]->SetVLine(tempi);
	}
	if(V == XPLANE) {
	  amrPicturePtrArray[ZPLANE]->SetHLine(amrPicturePtrArray[ZPLANE]->
				                        ImageSizeV()-1 - tempi);
	}
	if( ! (cbs->event->xbutton.state & ShiftMask)) {
	  if(V == ZPLANE) {
	    amrPicturePtrArray[XPLANE]->
	      APChangeSlice((tempi / scale) + ivLowOffsetMAL[XDIR]);
	  }
	  if(V == YPLANE) {
	    amrPicturePtrArray[XPLANE]->
	      APChangeSlice((tempi / scale) + ivLowOffsetMAL[XDIR]);
	  }
	  if(V == XPLANE) {
	    amrPicturePtrArray[YPLANE]->
	      APChangeSlice((tempi / scale) + ivLowOffsetMAL[YDIR]);
	  }
	  
	  for(int np(0); np < NPLANES; ++np) {
	    amrPicturePtrArray[np]->DoExposePicture();
	    projPicturePtr->ChangeSlice(np, amrPicturePtrArray[np]->GetSlice());
	  }
	  
	  projPicturePtr->MakeSlices();
	  XClearWindow(display, XtWindow(wTransDA));
	  DoExposeTransDA();
	  DoExposeRef();
	  return;
	}
#endif

	for(int np(0); np != NPLANES; ++np) {
	  amrPicturePtrArray[np]->DoExposePicture();
	}

	if(oldX >= 0 && oldX <= imageWidth) {
	  int sdir;
	  switch (V) {
	    case ZPLANE:
	      sdir = YDIR;
	    break;
	    case YPLANE:
	      sdir = ZDIR;
	    break;
	    case XPLANE:
	      sdir = ZDIR;
	    break;
	  }
	  XYPlotDataList *newlist = CreateLinePlot(V, sdir, mal, oldX / scale,
	                                           &pltAppState->CurrentDerived());
	  if(newlist) {
	    newlist->SetLevel(maxDrawnLevel);
	    if(XYplotwin[sdir] == NULL) {
	      int fnl(fileNames[0].length() - 1);
	      while(fnl > -1 && fileNames[0][fnl] != '/') {
	        --fnl;
	      }
	      XYplotwin[sdir] = new XYPlotWin(&fileNames[0][fnl+1],
					      appContext, wAmrVisTopLevel,
					      this, sdir, currentFrame);
	    }
	    XYplotwin[sdir]->AddDataList(newlist);
	  }
	}
	
	return;
	//break;
	
      default:
	;  // do nothing
	break;
      }  // end switch
    }  // end while(true)
  }
}  // end DoRubberBanding


// -------------------------------------------------------------------
void PltApp::DoExposePalette(Widget, XtPointer, XtPointer) {
  pltPaletteptr->ExposePalette();
}


// -------------------------------------------------------------------
void PltApp::PADoExposePicture(Widget w, XtPointer client_data, XtPointer) {
  unsigned long np = (unsigned long) client_data;
  
  amrPicturePtrArray[np]->DoExposePicture();
  // draw bounding box
  /*
  int isX = amrPicturePtrArray[np]->ImageSizeH();
  int isY = amrPicturePtrArray[np]->ImageSizeV();
  XSetForeground(display, xgc, pltPaletteptr->WhiteIndex());
  XDrawLine(display, XtWindow(w), xgc, 0, isY, isX, isY);
  XDrawLine(display, XtWindow(w), xgc, isX, 0, isX, isY);
  */
}


// -------------------------------------------------------------------
void PltApp::DoDrawPointerLocation(Widget, XtPointer data, XtPointer cbe) {
  XEvent *event = (XEvent *) cbe;
  unsigned long V = (unsigned long) data;

  if(event->type == LeaveNotify) {
    XClearWindow(display, XtWindow(wLocArea));
    return;
  }

  Window whichRoot, whichChild;
  int rootX, rootY, newX, newY;
  unsigned int inputMask;
  int currentScale(pltAppState->CurrentScale());
  
  XQueryPointer(display, XtWindow(wPlotPlane[V]), &whichRoot, &whichChild,
		&rootX, &rootY, &newX, &newY, &inputMask);

  int Yloci = ((amrPicturePtrArray[V]->ImageSizeV())/currentScale) -
              1 - (newY / currentScale) + ivLowOffsetMAL[YDIR];
  int Xloci = newX / currentScale + ivLowOffsetMAL[XDIR];
  char locText[40];

#if (BL_SPACEDIM == 3)
  int Zloci = amrPicturePtrArray[V]->GetSlice();

  double Xloc = gridOffset[XDIR];
  double Yloc = gridOffset[YDIR];
  double Zloc = gridOffset[ZDIR];
  switch(V) {
  case ZPLANE:
    Xloc += (0.5 + Xloci) * finestDx[XDIR];
    Yloc += (0.5 + Yloci) * finestDx[YDIR];
    Zloc += (0.5 + Zloci) * finestDx[ZDIR];
    break;
  case YPLANE:
    Xloc += (0.5 + Xloci) * finestDx[XDIR];
    Yloc += (0.5 + Zloci) * finestDx[YDIR];
    Zloc += (0.5 + Yloci) * finestDx[ZDIR];
    break;
  case XPLANE:
    Xloc += (0.5 + Zloci) * finestDx[XDIR];
    Yloc += (0.5 + Xloci) * finestDx[YDIR];
    Zloc += (0.5 + Yloci) * finestDx[ZDIR];
  }
  sprintf(locText, "(%.4E, %.4E, %.4E)", Xloc, Yloc, Zloc);
#elif (BL_SPACEDIM == 2)
  double Xloc = gridOffset[XDIR] + (0.5 + Xloci) * finestDx[XDIR];
  double Yloc = gridOffset[YDIR] + (0.5 + Yloci) * finestDx[YDIR];
  sprintf(locText, "(%.4E, %.4E)", Xloc, Yloc);
#endif

  XSetForeground(display, xgc, pltPaletteptr->WhiteIndex());
  XClearWindow(display, XtWindow(wLocArea));
  XDrawString(display, XtWindow(wLocArea),
	      xgc, 10, 20, locText, strlen(locText));
  
} 

// -------------------------------------------------------------------
void PltApp::DoSpeedScale(Widget, XtPointer, XtPointer call_data) {
  XmScaleCallbackStruct *cbs = (XmScaleCallbackStruct *) call_data;
  frameSpeed = 600 - cbs->value;
# if(BL_SPACEDIM == 3)
  for(int v = 0; v != 3; ++v) {
    amrPicturePtrArray[v]->SetFrameSpeed(600 - cbs->value);
  }
# endif
  XSync(display, false);
}


// -------------------------------------------------------------------
void PltApp::DoBackStep(int plane) {
  int currentScale(pltAppState->CurrentScale());
  int maLev(pltAppState->MaxAllowableLevel());
  AmrPicture *appX = amrPicturePtrArray[XPLANE];
  AmrPicture *appY = amrPicturePtrArray[YPLANE];
  AmrPicture *appZ = amrPicturePtrArray[ZPLANE];
  switch(plane) {
   case XPLANE:
    if(appX->GetSlice() > appX->GetSubDomain()[maLev].smallEnd(XDIR)) {
      appZ->SetVLine((appZ->GetVLine() - 1) * currentScale);
      appZ->DoExposePicture();
      appY->SetVLine((appY->GetVLine() - 1) * currentScale);
      appY->DoExposePicture();
      appX->APChangeSlice(appX->GetSlice()-1);
      break;
    }
    appZ->SetVLine(appY->ImageSizeH() - 1);
    appZ->DoExposePicture();
    appY->SetVLine(appY->ImageSizeH() - 1);
    appY->DoExposePicture();
    appX-> APChangeSlice(appX->GetSubDomain()[maLev].bigEnd(XDIR));
   break;

   case YPLANE:
    if(appY->GetSlice() > appY->GetSubDomain()[maLev].smallEnd(YDIR))
    {
      appX->SetVLine((appX->GetVLine() - 1) * currentScale);
      appX->DoExposePicture();
      appZ->SetHLine((appZ->GetHLine() + 1) * currentScale);
      appZ->DoExposePicture();
      appY->APChangeSlice(appY->GetSlice() - 1);
      break;
    }
    appX->SetVLine(appX->ImageSizeH() - 1);
    appX->DoExposePicture();
    appZ->SetHLine(0);
    appZ->DoExposePicture();
    appY->APChangeSlice(appY->GetSubDomain()[maLev].bigEnd(YDIR));
   break;

   case ZPLANE:
    if(appZ->GetSlice() > appZ->GetSubDomain()[maLev].smallEnd(ZDIR)) {
      appX->SetHLine((appX->GetHLine() + 1) * currentScale);
      appX->DoExposePicture();
      appY->SetHLine((appY->GetHLine() + 1) * currentScale);
      appY->DoExposePicture();
      appZ->APChangeSlice(appZ->GetSlice() - 1);
      break;
    }
    appX->SetHLine(0);
    appX->DoExposePicture();
    appY->SetHLine(0);
    appY->DoExposePicture();
    appZ->APChangeSlice(appZ->GetSubDomain()[maLev].bigEnd(ZDIR));
  }

#if (BL_SPACEDIM == 3)
  projPicturePtr->ChangeSlice(plane, amrPicturePtrArray[plane]->GetSlice());
  projPicturePtr->MakeSlices();
  XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
  DoExposeTransDA();
#endif
  DoExposeRef();
}


// -------------------------------------------------------------------
void PltApp::DoForwardStep(int plane) {
  int currentScale(pltAppState->CurrentScale());
  int maLev(pltAppState->MaxAllowableLevel());
  AmrPicture *appX = amrPicturePtrArray[XPLANE];
  AmrPicture *appY = amrPicturePtrArray[YPLANE];
  AmrPicture *appZ = amrPicturePtrArray[ZPLANE];
  switch(plane) {
   case XPLANE:
    if(appX->GetSlice() < appX->GetSubDomain()[maLev].bigEnd(XDIR)) {
      appZ->SetVLine((appZ->GetVLine() + 1) * currentScale);
      appZ->DoExposePicture();
      appY->SetVLine((appY->GetVLine() + 1) * currentScale);
      appY->DoExposePicture();
      appX->APChangeSlice(appX->GetSlice() + 1);
      break;
    }
    appZ->SetVLine(0);
    appZ->DoExposePicture();
    appY->SetVLine(0);
    appY->DoExposePicture();
    appX->APChangeSlice(appX->GetSubDomain()[maLev].smallEnd(XDIR));
   break;

   case YPLANE:
    if(appY->GetSlice() < appY->GetSubDomain()[maLev].bigEnd(YDIR)) {
      appX->SetVLine((appX->GetVLine() + 1) * currentScale);
      appX->DoExposePicture();
      appZ->SetHLine((appZ->GetHLine() - 1) * currentScale);
      appZ->DoExposePicture();
      appY->APChangeSlice(appY->GetSlice() + 1);
      break;
    }
    appX->SetVLine(0);
    appX->DoExposePicture();
    appZ->SetHLine(appX->ImageSizeV() - 1);
    appZ->DoExposePicture();
    appY->APChangeSlice(appY->GetSubDomain()[maLev].smallEnd(YDIR));
   break;

   case ZPLANE:
    if(appZ->GetSlice() < appZ->GetSubDomain()[maLev].bigEnd(ZDIR)) {
      appX->SetHLine((appX->GetHLine() - 1) * currentScale);
      appX->DoExposePicture();
      appY->SetHLine((appY->GetHLine() - 1) * currentScale);
      appY->DoExposePicture();
      appZ->APChangeSlice(appZ->GetSlice() + 1);
      break;
    }
    appX->SetHLine(appX->ImageSizeV() - 1);
    appX->DoExposePicture();
    appY->SetHLine(appY->ImageSizeV() - 1);
    appY->DoExposePicture();
    appZ->APChangeSlice(appZ->GetSubDomain()[maLev].smallEnd(ZDIR));
  }
#if (BL_SPACEDIM == 3)
  projPicturePtr->ChangeSlice(plane, amrPicturePtrArray[plane]->GetSlice());
  projPicturePtr->MakeSlices();
  XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
  DoExposeTransDA();
#endif
  DoExposeRef();
}


// -------------------------------------------------------------------
void PltApp::ChangePlane(Widget, XtPointer data, XtPointer cbs) {

  unsigned long which = (unsigned long) data;

#if (BL_SPACEDIM == 3)
  if(which == WCSTOP) {
    amrPicturePtrArray[XPLANE]->DoStop();
    amrPicturePtrArray[YPLANE]->DoStop();
    amrPicturePtrArray[ZPLANE]->DoStop();
    return;
  }
#endif

  if(which == WCASTOP) {
    writingRGB = false;
    StopAnimation();
    return;
  }
  
  XmPushButtonCallbackStruct *cbstr = (XmPushButtonCallbackStruct *) cbs;
  if(cbstr->click_count > 1) {
    switch(which) {
#if (BL_SPACEDIM == 3)
    case WCXNEG: amrPicturePtrArray[XPLANE]->Sweep(ANIMNEGDIR); return;
    case WCXPOS: amrPicturePtrArray[XPLANE]->Sweep(ANIMPOSDIR); return;
    case WCYNEG: amrPicturePtrArray[YPLANE]->Sweep(ANIMNEGDIR); return;
    case WCYPOS: amrPicturePtrArray[YPLANE]->Sweep(ANIMPOSDIR); return;
    case WCZNEG: amrPicturePtrArray[ZPLANE]->Sweep(ANIMNEGDIR); return;
    case WCZPOS: amrPicturePtrArray[ZPLANE]->Sweep(ANIMPOSDIR); return;
#endif
    case WCATNEG: Animate(ANIMNEGDIR); return;
    case WCATPOS: Animate(ANIMPOSDIR); return;
    case WCARGB: writingRGB = true; Animate(ANIMPOSDIR); return;
    default: return;
    }
  }
  switch (which) {
#if (BL_SPACEDIM == 3)
  case WCXNEG: DoBackStep(XPLANE);    return;
  case WCXPOS: DoForwardStep(XPLANE); return;
  case WCYNEG: DoBackStep(YPLANE);    return;
  case WCYPOS: DoForwardStep(YPLANE); return;
  case WCZNEG: DoBackStep(ZPLANE);    return;
  case WCZPOS: DoForwardStep(ZPLANE); return;
#endif
  case WCATNEG: DoAnimBackStep();     return;
  case WCATPOS: DoAnimForwardStep();  return;
  case WCARGB: writingRGB = true; DoAnimForwardStep(); return;
  }
}

// -------------------------------------------------------------------
void PltApp::DoAnimBackStep() {
  StopAnimation();
  --currentFrame;
  if(currentFrame < 0) {
    currentFrame = animFrames - 1;
  }
  pltAppState->SetCurrentFrame(currentFrame);
  ShowFrame();
}


// -------------------------------------------------------------------
void PltApp::DoAnimForwardStep() {
  StopAnimation();
  if(writingRGB) {
    DoCreateAnimRGBFile();
    writingRGB = false;
  }
  ++currentFrame;
  if(currentFrame == animFrames) {
    currentFrame = 0;
  }
  pltAppState->SetCurrentFrame(currentFrame);
  ShowFrame();
}


// -------------------------------------------------------------------
void PltApp::DoAnimFileScale(Widget, XtPointer, XtPointer cbs) {
  StopAnimation();
  currentFrame = ((XmScaleCallbackStruct *) cbs)->value;
  pltAppState->SetCurrentFrame(currentFrame);
  ShowFrame();
}


// -------------------------------------------------------------------
void PltApp::ResetAnimation() {
  StopAnimation();
  if( ! interfaceReady) {
#   if(BL_SPACEDIM == 2)
    int maLev(pltAppState->MaxAllowableLevel());
    //int newContourNum(amrPicturePtrArray[ZPLANE]->GetContourNumber());
    AmrPicture *Tempap = amrPicturePtrArray[ZPLANE];
    XtRemoveEventHandler(wPlotPlane[ZPLANE], ExposureMask, false, 
			 (XtEventHandler) &PltApp::StaticEvent,
			 (XtPointer) Tempap);
    //Array<Box> domain = amrPicturePtrArray[ZPLANE]->GetSubDomain();
    Box fineDomain(amrPicturePtrArray[ZPLANE]->GetSubDomain()[maLev]);
    delete Tempap;
    //amrPicturePtrArray[ZPLANE]->SetDataServicesPtr(dataServicesPtr[currentFrame]); 
    
    const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
    fineDomain.refine(CRRBetweenLevels(maLev, amrData.FinestLevel(),
                                       amrData.RefRatio()));
    amrPicturePtrArray[ZPLANE] = new AmrPicture(ZPLANE, GAptr, fineDomain, 
						NULL, this,
						pltAppState,
						bCartGridSmoothing);
    amrPicturePtrArray[ZPLANE]->SetRegion(startX, startY, endX, endY);
    //pltAppState->SetMaxDrawnLevel(maxDrawnLevel);
    //SetNumContours(false);
    //XtRemoveEventHandler(wPlotPlane[ZPLANE], ExposureMask, false, 
			 //(XtEventHandler) &PltApp::StaticEvent,
			 //(XtPointer) Tempap);
    //delete Tempap;
    amrPicturePtrArray[ZPLANE]->CreatePicture(XtWindow(wPlotPlane[ZPLANE]),
					      pltPaletteptr);
    AddStaticEventHandler(wPlotPlane[ZPLANE], ExposureMask,
			  &PltApp::PADoExposePicture, (XtPointer) ZPLANE);
    interfaceReady = true;
#   endif
  }
}


// -------------------------------------------------------------------
void PltApp::StopAnimation() {
  if(animationIId) {
    XtRemoveTimeOut(animationIId);
    animationIId = 0;
  }
#if (BL_SPACEDIM == 2)
  for(int dim = 0; dim != 2; ++dim) {
    if(XYplotwin[dim]) {
      XYplotwin[dim]->StopAnimation();
    }
  }
#endif
}


// -------------------------------------------------------------------
void PltApp::Animate(AnimDirection direction) {
  StopAnimation();
  animationIId = AddStaticTimeOut(frameSpeed, &PltApp::DoUpdateFrame);
  animDirection = direction;
#if (BL_SPACEDIM == 2)
  for(int dim(0); dim != 2; ++dim) {
    if(XYplotwin[dim]) {
      XYplotwin[dim]->InitializeAnimation(currentFrame, animFrames);
    }
  }
#endif
}


// -------------------------------------------------------------------
void PltApp::DirtyFrames() {
  paletteDrawn = false;
  for(int i(0); i < animFrames; ++i) {
    if(readyFrames[i] == true) {
      XDestroyImage(frameBuffer[i]);
    }
    readyFrames[i] = false;
  }
}


// -------------------------------------------------------------------
void PltApp::DoUpdateFrame(Widget, XtPointer, XtPointer) {
  if(animDirection == ANIMPOSDIR) {
    if(writingRGB) {
      DoCreateAnimRGBFile();
    }
    ++currentFrame;
    if(currentFrame == animFrames) {
      currentFrame = 0;
    }
  } else {
    --currentFrame;
    if(currentFrame < 0) {
      currentFrame = animFrames - 1;
    }
  }
  pltAppState->SetCurrentFrame(currentFrame);
  ShowFrame();
  XSync(display, false);
  animationIId = AddStaticTimeOut(frameSpeed, &PltApp::DoUpdateFrame);
}


// -------------------------------------------------------------------
void PltApp::ShowFrame() {
  interfaceReady = false;
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  if( ! readyFrames[currentFrame] || datasetShowing ||
      UsingFileRange(currentRangeType))
  {
#   if(BL_SPACEDIM == 2)
    AmrPicture *tempapSF = amrPicturePtrArray[ZPLANE];
    Array<Box> domain = amrPicturePtrArray[ZPLANE]->GetSubDomain();
    XtRemoveEventHandler(wPlotPlane[ZPLANE], ExposureMask, false, 
			 (XtEventHandler) &PltApp::StaticEvent,
			 (XtPointer) tempapSF);
    delete tempapSF;
    
    int finestLevel(amrData.FinestLevel());
    int maxlev(DetermineMaxAllowableLevel(amrData.ProbDomain()[finestLevel],
					  finestLevel, MaxPictureSize(),
					  amrData.RefRatio()));
    
    pltAppState->SetMaxAllowableLevel(maxlev);
    int minAllowableLevel(pltAppState->MinAllowableLevel());
    Box fineDomain(domain[pltAppState->MaxAllowableLevel()]);
    fineDomain.refine(CRRBetweenLevels(pltAppState->MaxAllowableLevel(),
				       finestLevel, amrData.RefRatio()));
    amrPicturePtrArray[ZPLANE] = new AmrPicture(ZPLANE, GAptr, fineDomain, 
						NULL, this,
						pltAppState,
						bCartGridSmoothing);
    amrPicturePtrArray[ZPLANE]->SetRegion(startX, startY, endX, endY);
    //XtRemoveEventHandler(wPlotPlane[ZPLANE], ExposureMask, false, 
			 //(XtEventHandler) &PltApp::StaticEvent,
			 //(XtPointer) tempapSF);
    //SetNumContours(false);
    //delete tempapSF;
    
    //pltAppState->SetMaxDrawnLevel(maxDrawnLevel);
    amrPicturePtrArray[ZPLANE]->CreatePicture(XtWindow(wPlotPlane[ZPLANE]),
					      pltPaletteptr);
    AddStaticEventHandler(wPlotPlane[ZPLANE], ExposureMask,
			  &PltApp::PADoExposePicture, (XtPointer) ZPLANE);
    frameBuffer[currentFrame] = amrPicturePtrArray[ZPLANE]->GetPictureXImage();
#   endif
    readyFrames[currentFrame] = true;
    paletteDrawn = ! UsingFileRange(currentRangeType);
  }
  
  XPutImage(display, XtWindow(wPlotPlane[ZPLANE]), xgc,
	    frameBuffer[currentFrame], 0, 0, 0, 0,
	    amrPicturePtrArray[ZPLANE]->ImageSizeH(),
	    amrPicturePtrArray[ZPLANE]->ImageSizeV());
  
  aString fileName(fileNames[currentFrame]);
  int fnl(fileName.length() - 1);
  while(fnl > -1 && fileName[fnl] != '/') {
    --fnl;
  }

  XmString fileString = XmStringCreateSimple(&fileName[fnl+1]);
  XtVaSetValues(wWhichFileLabel, XmNlabelString, fileString, NULL);
  XmStringFree(fileString);
  
  char tempTimeName[100];
  ostrstream tempTimeOut(tempTimeName, 100);
  tempTimeOut << "T=" << amrData.Time() << ends;
  XmString timeString = XmStringCreateSimple(tempTimeName);
  XtVaSetValues(wWhichTimeLabel, XmNlabelString, timeString, NULL);
  XmStringFree(timeString);

  XmScaleSetValue(wWhichFileScale, currentFrame);
  
  if(datasetShowing) {
    int hdir, vdir, sdir;
    if(activeView == ZPLANE) { hdir = XDIR; vdir = YDIR; sdir = ZDIR; }
    if(activeView == YPLANE) { hdir = XDIR; vdir = ZDIR; sdir = YDIR; }
    if(activeView == XPLANE) { hdir = YDIR; vdir = ZDIR; sdir = XDIR; }
    datasetPtr->DatasetRender(trueRegion, amrPicturePtrArray[activeView], this,
		              pltAppState, hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  }
#if (BL_SPACEDIM == 2)
  for(int dim(0); dim < 2; ++dim) {
    if(XYplotwin[dim]) {
      XYplotwin[dim]->UpdateFrame(currentFrame);
    }
  }
#endif
}  // end ShowFrame


// -------------------------------------------------------------------
void PltApp::AddStaticCallback(Widget w, String cbtype, memberCB cbf, void *d) {
  CBData *cbs = new CBData(this, d, cbf);

  // cbdPtrs.push_back(cbs)
  int nSize(cbdPtrs.size());
  cbdPtrs.resize(nSize + 1);
  cbdPtrs[nSize] = cbs;

  XtAddCallback(w, cbtype, (XtCallbackProc ) &PltApp::StaticCallback,
		(XtPointer) cbs);
}


// -------------------------------------------------------------------
void PltApp::AddStaticEventHandler(Widget w, EventMask mask, memberCB cbf, void *d)
{
  CBData *cbs = new CBData(this, d, cbf);

  // cbdPtrs.push_back(cbs)
  int nSize(cbdPtrs.size());
  cbdPtrs.resize(nSize + 1);
  cbdPtrs[nSize] = cbs;

  XtAddEventHandler(w, mask, false, (XtEventHandler) &PltApp::StaticEvent,
		    (XtPointer) cbs);
}
 
 
// -------------------------------------------------------------------
XtIntervalId PltApp::AddStaticTimeOut(int time, memberCB cbf, void *d) {
  CBData *cbs = new CBData(this, d, cbf);

  // cbdPtrs.push_back(cbs)
  int nSize(cbdPtrs.size());
  cbdPtrs.resize(nSize + 1);
  cbdPtrs[nSize] = cbs;

  return XtAppAddTimeOut(appContext, time,
			 (XtTimerCallbackProc) &PltApp::StaticTimeOut,
			 (XtPointer) cbs);
}


// -------------------------------------------------------------------
void PltApp::StaticCallback(Widget w, XtPointer client_data, XtPointer call_data) {
  CBData *cbs = (CBData *) client_data;
  PltApp *obj = cbs->instance;
  (obj->*(cbs->cbFunc))(w, (XtPointer) cbs->data, call_data);
}


// -------------------------------------------------------------------
void PltApp::StaticEvent(Widget w, XtPointer client_data, XEvent *event, char*) {
  CBData *cbs = (CBData *) client_data;
  PltApp *obj = cbs->instance;
  (obj->*(cbs->cbFunc))(w, (XtPointer) cbs->data, (XtPointer) event);
}


// -------------------------------------------------------------------
void PltApp::StaticTimeOut(XtPointer client_data, XtIntervalId * call_data) {
  CBData *cbs = (CBData *) client_data;
  PltApp *obj = cbs->instance;
  (obj->*(cbs->cbFunc))(None, (XtPointer) cbs->data, (XtPointer) call_data);
}


// -------------------------------------------------------------------
int  PltApp::initialScale;
int  PltApp::defaultShowBoxes;
int  PltApp::initialWindowHeight;
int  PltApp::initialWindowWidth;
int  PltApp::placementOffsetX    = 0;
int  PltApp::placementOffsetY    = 0;
int  PltApp::reserveSystemColors = 50;
aString PltApp::defaultPaletteString;
aString PltApp::defaultLightingFilename;
aString PltApp::initialDerived;
aString PltApp::initialFormatString;

bool  PltApp::PaletteDrawn()          { return PltApp::paletteDrawn;     }
int   PltApp::GetInitialScale()       { return PltApp::initialScale;     }
int   PltApp::GetDefaultShowBoxes()   { return PltApp::defaultShowBoxes; }
aString &PltApp::GetInitialDerived()  { return PltApp::initialDerived;   }
aString &PltApp::GetDefaultPalette()  { return PltApp::defaultPaletteString; }
aString &PltApp::GetDefaultLightingFile() { return PltApp::defaultLightingFilename; }
const aString &PltApp::GetFileName()  { return (fileNames[currentFrame]); }
void  PltApp::PaletteDrawn(bool tOrF) { paletteDrawn = tOrF; }

void PltApp::SetDefaultPalette(const aString &palString) {
  PltApp::defaultPaletteString = palString;
}

void PltApp::SetDefaultLightingFile(const aString &lightFileString) {
  PltApp::defaultLightingFilename = lightFileString;
}

void PltApp::SetInitialDerived(const aString &initialderived) {
  PltApp::initialDerived = initialderived;
}

void PltApp::SetInitialScale(int initScale) {
  PltApp::initialScale = initScale;
}

void PltApp::SetInitialFormatString(const aString &formatstring) {
  PltApp::initialFormatString = formatstring;
}

void PltApp::SetDefaultShowBoxes(int showboxes) {
  PltApp::defaultShowBoxes = showboxes;
}

void PltApp::SetInitialWindowHeight(int initWindowHeight) {
  PltApp::initialWindowHeight = initWindowHeight;
}

void PltApp::SetInitialWindowWidth(int initWindowWidth) {
  PltApp::initialWindowWidth = initWindowWidth;
}

void PltApp::SetReserveSystemColors(int reservesystemcolors) {
  reservesystemcolors = max(0, min(128, reservesystemcolors));  // arbitrarily
  PltApp::reserveSystemColors = reservesystemcolors;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
