//BL_COPYRIGHT_NOTICE

//
// $Id: PltApp.cpp,v 1.61 2000-06-13 23:19:08 car Exp $
//

// ---------------------------------------------------------------
// PltApp.cpp
// ---------------------------------------------------------------
#include "PltApp.H"

#include <MainW.h>
#include <PushB.h>
#include <PushBG.h>
#include <FileSB.h>
#include <Label.h>
#include <LabelG.h>
#include <RowColumn.h>
#include <Form.h>
#include <DrawingA.h>
#include <Text.h>
#include <ScrolledW.h>
#include <TextF.h>
#include <ToggleB.h>
#include <SelectioB.h>
#include <List.h>
#include <Scale.h>
#include <SeparatoG.h>

#include <cursorfont.h>
#include <ArrowB.h>
#include <CascadeB.h>
#include <Frame.h>

#if defined(BL_PARALLELVOLUMERENDER)
#include <PVolRender.H>
#endif

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

// Hack for window manager calls
#ifndef FALSE
#define FALSE false
#endif

const int MAXSCALE       = 32;
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
  if(datasetShowing) {
    delete datasetPtr;
  }
  XtDestroyWidget(wAmrVisTopLevel);
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
    currentContour(0),
    bCartGridSmoothing(false)
{
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  lightingWindowExists = false;
#endif

  if( ! dataservicesptr[0]->AmrDataOk()) {
    return;
  }
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  int i;

  char header[BUFSIZ];
  ostrstream headerout(header, BUFSIZ);
  int fnl(fileName.length() - 1);
  while(fnl > -1 && fileName[fnl] != '/') {
    --fnl;
  }

  if(animating2d) {
    animFrames = GetFileCount(); 
    BL_ASSERT(dataServicesPtr.length() == animFrames);
    fileNames.resize(animFrames);
    for(i = 0; i < animFrames; i++) {
      fileNames[i] = GetComlineFilename(i); 
    }

    // If this is an animation, putting the time in the title bar wouldn't
    // make much sense.  Instead, we simply indicate that this is an
    // animation, and indicate the time underneath the file label in the
    // controls window.
    headerout << &fileName[fnl+1] << ", 2D Animation" << ends;
  }
  else {
    animFrames = 1;
    fileNames.resize(animFrames);
    fileNames[currentFrame] = fileName;

    // Single plot file, so the time is indicated in the title bar.
    headerout << &fileName[fnl+1] << ", T=" << amrData.Time() << ends;
  }

  // Set the delete response to DO_NOTHING so that it can be app defined.
  wAmrVisTopLevel =
    XtVaCreatePopupShell(header, 
			 topLevelShellWidgetClass, wTopLevel,
			 XmNwidth,	    initialWindowWidth,
			 XmNheight,	    initialWindowHeight,
			 XmNx,		    100+placementOffsetX,
			 XmNy,		    200+placementOffsetY,
			 XmNdeleteResponse, XmDO_NOTHING,
			 NULL);

  GAptr = new GraphicsAttributes(wAmrVisTopLevel);
  if(GAptr->PVisual() != XDefaultVisual(GAptr->PDisplay(), GAptr->PScreenNumber()))
  {
    XtVaSetValues(wAmrVisTopLevel, XmNvisual, GAptr->PVisual(),
		  XmNdepth, GAptr->PDepth(), NULL);
  }

  FileType fileType = GetDefaultFileType();
  BL_ASSERT(fileType != INVALIDTYPE);
  
  if(! (dataServicesPtr[currentFrame]->CanDerive(PltApp::initialDerived)) &&
     ! (fileType == FAB) && ! (fileType == MULTIFAB))
  {
    cerr << "Bad initial derived:  cannot derive "
	 << PltApp::initialDerived <<endl;
    cerr << "defaulting to "
	 << dataServicesPtr[currentFrame]->PlotVarNames()[0] << endl;
    SetInitialDerived(dataServicesPtr[currentFrame]->PlotVarNames()[0]);
  }

  currentDerived = PltApp::initialDerived;
  SetShowBoxes(GetDefaultShowBoxes());
  int finestLevel(amrData.FinestLevel());
  int maxlev = DetermineMaxAllowableLevel(amrData.ProbDomain()[finestLevel],
			       finestLevel, MaxPictureSize(), amrData.RefRatio());
  minAllowableLevel = 0;
  Box maxDomain(amrData.ProbDomain()[maxlev]);
  unsigned long dataSize = maxDomain.length(XDIR) * maxDomain.length(YDIR);
  if(MaxPictureSize() / dataSize == 0) {
    maxAllowableScale = 1;
  } else  {
    maxAllowableScale = (int) sqrt((Real) (MaxPictureSize()/dataSize));
  }

  currentScale = Max(1, Min(GetInitialScale(), maxAllowableScale));
  
  amrPicturePtrArray[ZPLANE] = new AmrPicture(minAllowableLevel, GAptr,
					this, dataServicesPtr[currentFrame],
					bCartGridSmoothing);
#if (BL_SPACEDIM == 3)
  amrPicturePtrArray[YPLANE] = new AmrPicture(YPLANE, minAllowableLevel, GAptr,
					amrData.ProbDomain()[finestLevel],
		                        amrPicturePtrArray[ZPLANE], NULL, this,
					bCartGridSmoothing);
  amrPicturePtrArray[XPLANE] = new AmrPicture(XPLANE, minAllowableLevel, GAptr,
					amrData.ProbDomain()[finestLevel],
		                        amrPicturePtrArray[ZPLANE], NULL, this,
					bCartGridSmoothing);
#endif

  for(i = 0; i != BL_SPACEDIM; i++) {
    ivLowOffsetMAL.setVal(i, amrData.ProbDomain()[maxlev].smallEnd(i));
  }

  numContours = 5;
  contourNumString = aString("5");
  for(i = 0; i != NPLANES; ++i) {
    amrPicturePtrArray[i]->SetContourNumber(5);
  }

  palFilename = PltApp::defaultPaletteString;
  PltAppInit();
}


// -------------------------------------------------------------------
PltApp::PltApp(XtAppContext app, Widget w, const Box &region,
	       const IntVect &offset, AmrPicture *parentPtr,
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
    //showBoxes(pltParent->showBoxes),
    currentFrame(pltParent->currentFrame),
    numContours(pltParent->numContours),
    palFilename(palfile)
{
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  lightingWindowExists = false;
#endif
  contourNumString = pltParent->contourNumString.c_str();

  char header[BUFSIZ];

  SetShowBoxes(pltParent->GetShowBoxes());

  bCartGridSmoothing =  pltParent->bCartGridSmoothing;
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  int finestLevel(amrData.FinestLevel());
  int maxlev = DetermineMaxAllowableLevel(region, finestLevel,
					  MaxPictureSize(), amrData.RefRatio());
  //minAllowableLevel = 0; //amrData.FinestContainingLevel(region, finestLevel);
  minAllowableLevel = amrData.FinestContainingLevel(region, finestLevel);
  Box maxDomain(region);
  if(maxlev < finestLevel)
    maxDomain.coarsen(CRRBetweenLevels(maxlev, finestLevel,amrData.RefRatio()));

  unsigned long dataSize = maxDomain.length(XDIR) * maxDomain.length(YDIR);
  if(MaxPictureSize() / dataSize == 0) maxAllowableScale = 1;
  else maxAllowableScale = (int) sqrt((Real) (MaxPictureSize()/dataSize));

  currentScale = Min(maxAllowableScale, pltParent->CurrentScale());
  currentContour = pltParent->CurrentContour();
  
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
    timeOut << " T=" << amrData.Time() << ends;
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

  GAptr = new GraphicsAttributes(wAmrVisTopLevel);

  if(GAptr->PVisual() != XDefaultVisual(GAptr->PDisplay(),
					GAptr->PScreenNumber()))
      XtVaSetValues(wAmrVisTopLevel, XmNvisual, GAptr->PVisual(),
                    XmNdepth, 8, NULL);
  for(int np = 0; np < NPLANES; ++np)
    amrPicturePtrArray[np] = new AmrPicture(np, minAllowableLevel, GAptr, region,
					    parentPtr, pltParent, this,
					    bCartGridSmoothing);
  
  for(int j = 0; j < NPLANES; ++j) 
    amrPicturePtrArray[j]->SetContourNumber(numContours);
  
#if (BL_SPACEDIM == 3)
  SetHVLine(amrPicturePtrArray);
#endif
  ivLowOffsetMAL = offset;
  currentDerived = newderived;
  PltAppInit();
}


// -------------------------------------------------------------------
void PltApp::PltAppInit() {
  int np;
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  int maxAllowLev = amrPicturePtrArray[ZPLANE]->MaxAllowableLevel();

  // User defined widget destroy callback -- will free all memory used to create
  // window.
  XmAddWMProtocolCallback(wAmrVisTopLevel,
			  XmInternAtom(GAptr->PDisplay(),"WM_DELETE_WINDOW", false),
			  CBQuitPltApp, (XtPointer) this);

  for(np = 0; np != BL_SPACEDIM; ++np) {
    XYplotwin[np] = NULL; // No 1D plot windows initially.

    // For speed (and clarity!) we store the values of the finest value of h of
    // each dimension, as well as the low value of the problem domain in simple
    // arrays.  These are both in problem space.
    finestDx[np] = amrData.DxLevel()[maxAllowLev][np];
    gridOffset[np] = amrData.ProbLo()[np];
  }

  placementOffsetX += 20;
  placementOffsetY += 20;
  
  servingButton = 0;
  activeView = ZPLANE;
  maxDrawnLevel = maxAllowLev;
  minDrawnLevel = minDataLevel = minAllowableLevel;
  maxDataLevel = amrData.FinestLevel();
  startX = startY = endX = endY = 0;

  currentFrame = 0;
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
  formatString = tempchar;
  delete [] tempchar;

  infoShowing = false;
  contoursShowing = false;
  setRangeShowing = false;
  datasetShowing = false;
  writingRGB = false;
			  
  int palListLength = PALLISTLENGTH;
  int palWidth = PALWIDTH;
  int totalPalWidth = TOTALPALWIDTH;
  int totalPalHeight = TOTALPALHEIGHT;
  pltPaletteptr = new Palette(wTopLevel, palListLength, palWidth,
			      totalPalWidth, totalPalHeight,
			      reserveSystemColors);
  
  // gc for gxxor rubber band line drawing
  rbgc = XCreateGC(GAptr->PDisplay(), GAptr->PRoot(), 0, NULL);
  XSetFunction(GAptr->PDisplay(), rbgc, GXxor);
  cursor = XCreateFontCursor(GAptr->PDisplay(), XC_left_ptr);

  // No need to store these widgets in the class after this function is called.
  Widget wMainArea, wPalFrame, wPlotFrame, wPalForm;

  wMainArea = XtVaCreateManagedWidget("MainArea", xmFormWidgetClass,
				      wAmrVisTopLevel, NULL);

  /////////////////////////////////////////////////
  // MENU BAR
  //  -- Modified 7-99 to give interface a nosejob
  /////////////////////////////////////////////////
  Widget wMenuBar, wMenuPulldown, wid, wCascade;
  char selectText[20], accelText[20], accel[20];
  XmString label_str;
  XtSetArg(args[0], XmNtopAttachment, XmATTACH_FORM);
  XtSetArg(args[1], XmNleftAttachment, XmATTACH_FORM);
  XtSetArg(args[2], XmNrightAttachment, XmATTACH_FORM);
  wMenuBar = XmCreateMenuBar(wMainArea, "menubar", args, 3);

  /////////////////////////////////////////////////
  // FILE MENU
  /////////////////////////////////////////////////
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
				XmNacceleratorText, label_str, NULL);
  XmStringFree(label_str);
  XtAddCallback(wid, XmNactivateCallback, CBQuitPltApp, (XtPointer) this);
  
  /////////////////////////////////////////////////
  // VIEW MENU
  /////////////////////////////////////////////////
  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, "MenuPulldown", NULL, 0);
  XtVaCreateManagedWidget("View", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'V', XmNsubMenuId, wMenuPulldown, NULL);

  // To scale the raster / contour windows
  int maxallow = Min(MAXSCALE, maxAllowableScale);
  wCascade = XmCreatePulldownMenu(wMenuPulldown, "scalemenu", NULL, 0);
  XtVaCreateManagedWidget("Scale", xmCascadeButtonWidgetClass, wMenuPulldown,
			  XmNmnemonic, 'S', XmNsubMenuId, wCascade, NULL);
  for(unsigned long scale = 1; scale <= maxallow; scale++) {
    sprintf(selectText, "%ix", scale);
    wid = XtVaCreateManagedWidget(selectText, xmToggleButtonGadgetClass, wCascade,
				  XmNset, false, NULL);
    if(scale <= 10) {
      // scale values <= 10 are shortcutted by pressing the number 1-0
      sprintf(accel, "<Key>%i", scale % 10);
      sprintf(accelText, "%i", scale % 10);
      label_str = XmStringCreateSimple(accelText);
      XtVaSetValues(wid,  XmNmnemonic, scale + 'O', XmNaccelerator, accel,
		    XmNacceleratorText, label_str, NULL);
      XmStringFree(label_str);
    }
    else if(scale <= 20) {
      // scale values <= 20 can be obtained by holding down ALT and pressing 1-0
      sprintf(accel, "Alt<Key>%i", scale % 10);
      sprintf(accelText, "Alt+%i", scale % 10);
      label_str = XmStringCreateSimple(accelText);
      XtVaSetValues(wid, XmNaccelerator, accel,
		    XmNacceleratorText, label_str, NULL);
      XmStringFree(label_str);
    }      
    if(scale == currentScale){
      // Toggle buttons indicate which is the current scale
      XtVaSetValues(wid, XmNset, true, NULL);
      wCurrScale = wid;
    }
    AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::ChangeScale,
		      (XtPointer) scale);
  }

  // Levels button, to view various levels of refinement
  int numberOfLevels(amrPicturePtrArray[ZPLANE]->NumberOfLevels()-minAllowableLevel);
  wCascade = XmCreatePulldownMenu(wMenuPulldown, "levelmenu", NULL, 0);
  XtVaCreateManagedWidget("Level", xmCascadeButtonWidgetClass, wMenuPulldown,
			  XmNmnemonic, 'L', XmNsubMenuId, wCascade, NULL);
  for(int menuLevel = minAllowableLevel; menuLevel <= maxDrawnLevel; ++menuLevel) {
    sprintf(selectText, "%i/%i", menuLevel, amrData.FinestLevel());
    wid = XtVaCreateManagedWidget(selectText, xmToggleButtonGadgetClass, wCascade,
				  XmNset, false, NULL);
    if(menuLevel <= 10) {
      // Levels <= 10 are shortcutted by holding down the CTRL key and pressing 1-0
      sprintf(accel, "Ctrl<Key>%i", menuLevel % 10);
      sprintf(accelText, "Ctrl+%i", menuLevel % 10);
      label_str = XmStringCreateSimple(accelText);
      XtVaSetValues(wid, XmNmnemonic, menuLevel + '0', XmNaccelerator, accel,
		    XmNacceleratorText, label_str, NULL);			      
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
				XmNaccelerator, "<Key>C",
				XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoContoursButton);

  // Button to create (or pop up) a dialog to set range
  label_str = XmStringCreateSimple("R");
  wid = XtVaCreateManagedWidget("Range...",
				xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'R',
				XmNaccelerator, "<Key>R",
				XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoSetRangeButton);

  // To create (or update and pop up) a dialog indicating the data values
  // of a selected region
  label_str = XmStringCreateSimple("D");
  wid = XtVaCreateManagedWidget("Dataset...",
				xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'D',
				XmNaccelerator, "<Key>D",
				XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoDatasetButton);

  XtVaCreateManagedWidget(NULL, xmSeparatorGadgetClass, wMenuPulldown, NULL);

  // Toggle viewing the boxes
  label_str = XmStringCreateSimple("B");
  wid = XtVaCreateManagedWidget("Boxes",
				xmToggleButtonGadgetClass, wMenuPulldown,
				XmNmnemonic, 'B',
				XmNset, showBoxes,
				XmNaccelerator, "<Key>B",
				XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::DoBoxesButton);

  /////////////////////////////////////////////////
  // DERIVED MENU
  /////////////////////////////////////////////////

  int maxMenuItems = 20;
  int numberOfDerived = dataServicesPtr[currentFrame]->NumDeriveFunc();
  derivedStrings  = dataServicesPtr[currentFrame]->PlotVarNames();

  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, "DerivedPulldown", NULL, 0);
  XtVaSetValues(wMenuPulldown,
		XmNpacking, XmPACK_COLUMN,
		XmNnumColumns, numberOfDerived / maxMenuItems +
		((numberOfDerived % maxMenuItems == 0) ? 0 : 1), NULL);
  XtVaCreateManagedWidget("Variable", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'a', XmNsubMenuId,   wMenuPulldown, NULL);
  unsigned long cderived = 0;
  wCurrDerived = XtVaCreateManagedWidget(derivedStrings[0].c_str(),
					 xmToggleButtonGadgetClass, wMenuPulldown,
					 XmNset, true, NULL);
  AddStaticCallback(wCurrDerived, XmNvalueChangedCallback, &PltApp::ChangeDerived,
		    (XtPointer) 0);
  for(unsigned long derived = 1; derived < numberOfDerived; derived++) {
    wid = XtVaCreateManagedWidget(derivedStrings[derived].c_str(),
				  xmToggleButtonGadgetClass, wMenuPulldown,
				  XmNset, false, NULL);
    if(derivedStrings[derived] == currentDerived) {
      XtVaSetValues(wCurrDerived, XmNset, false, NULL);
      XtVaSetValues(wid, XmNset, true, NULL);
      wCurrDerived = wid;
      cderived = derived;
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
  /////////////////////////////////////////////////
  // HELP MENU
  /////////////////////////////////////////////////

  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, "MenuPulldown", NULL, 0);
  XtVaCreateManagedWidget("Help", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'H', XmNsubMenuId,   wMenuPulldown, NULL);
  wid = XtVaCreateManagedWidget("Info...", xmPushButtonGadgetClass,
				wMenuPulldown, XmNmnemonic,  'I', NULL);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoInfoButton);
  
  XtManageChild(wMenuBar);

  // ****************** Palette frame and drawing area

  wPalFrame =
    XtVaCreateManagedWidget("paletteframe", xmFrameWidgetClass, wMainArea,
			    XmNtopAttachment,   XmATTACH_WIDGET,
			    XmNtopWidget,       wMenuBar,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNrightOffset,     1,
			    XmNshadowType,      XmSHADOW_ETCHED_IN,
			    NULL);
  wPalForm =
    XtVaCreateManagedWidget("paletteform", xmFormWidgetClass, wPalFrame,
			    NULL);

  wPalArea =
    XtVaCreateManagedWidget("palarea", xmDrawingAreaWidgetClass, wPalForm,
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
  strcpy(buffer, derivedStrings[cderived].c_str());
  label_str = XmStringCreateSimple(buffer);
  wPlotLabel = 
    XtVaCreateManagedWidget("plotlabel", xmLabelWidgetClass, wPalForm,
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
  wControlsFrame =
    XtVaCreateManagedWidget("controlsframe", xmFrameWidgetClass, wMainArea,
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
  wControlForm =
    XtVaCreateManagedWidget("refArea", xmDrawingAreaWidgetClass, wControlsFrame,
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
			      XmNvalue,	      0,
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
    
    int fnl(fileName.length() - 1);
    while(fnl > -1 && fileName[fnl] != '/') {
      --fnl;
    }

    XmString fileString = XmStringCreateSimple(&fileName[fnl+1]);
    wWhichFileLabel =
      XtVaCreateManagedWidget("whichFileLabel",
			      xmLabelWidgetClass, wControlForm,	
			      XmNx, 0,
			      XmNy, wcfHeight -4*halfbutton-3-adjustHeight2D,
			      XmNlabelString,         fileString,
			      NULL);
    XmStringFree(fileString);

    // We add a which-time label to indicate the time of each plot file.
    char tempTimeName[100];
    ostrstream tempTimeOut(tempTimeName, 100);
    tempTimeOut << "T=" << amrData.Time() << ends;
    XmString timeString = XmStringCreateSimple(tempTimeName);
    wWhichTimeLabel =
      XtVaCreateManagedWidget("whichTimeLabel",
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
     
  }  // end if(animating2d || BL_SPACEDIM == 3)

  // ************************** Plot frame and area

  wPlotFrame =
    XtVaCreateManagedWidget("plotframe",
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

  Widget wPicArea =
    XtVaCreateManagedWidget("picarea", xmFormWidgetClass, wPlotFrame, NULL);

  wLocArea =
    XtVaCreateManagedWidget("locarea",xmDrawingAreaWidgetClass, wPicArea,
			    XmNleftAttachment,   XmATTACH_FORM,
			    XmNrightAttachment,  XmATTACH_FORM,
			    XmNbottomAttachment, XmATTACH_FORM,
			    XmNheight,	         30,
			    NULL);

  wPlotArea =
    XtVaCreateManagedWidget("plotarea", xmFormWidgetClass, wPicArea,
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
	
  wPlotPlane[ZPLANE] =
    XtVaCreateManagedWidget("plotArea",
			    xmDrawingAreaWidgetClass, wScrollArea[ZPLANE],
			    XmNtranslations,  XtParseTranslationTable(trans),
			    XmNwidth,         amrPicturePtrArray[ZPLANE]->ImageSizeH() + 1,
			    XmNheight,        amrPicturePtrArray[ZPLANE]->ImageSizeV() + 1,
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
  
  wPlotPlane[YPLANE] =
    XtVaCreateManagedWidget("plotArea",
			    xmDrawingAreaWidgetClass, wScrollArea[YPLANE],
			    XmNtranslations, XtParseTranslationTable(trans),
			    XmNwidth,   amrPicturePtrArray[YPLANE]->ImageSizeH() + 1,
			    XmNheight,  amrPicturePtrArray[YPLANE]->ImageSizeV() + 1,
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
  
  wPlotPlane[XPLANE] =
    XtVaCreateManagedWidget("plotArea",
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
  
  wLabelAxes = XtVaCreateManagedWidget("XYZ",
				       xmPushButtonGadgetClass, wPlotArea,
				       XmNleftAttachment, XmATTACH_WIDGET,
				       XmNleftWidget, wOrient,
				       XmNleftOffset, WOFFSET,
				       XmNtopAttachment, XmATTACH_POSITION,
				       XmNtopPosition, 50,
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
				     XmNtranslations,	XtParseTranslationTable(trans),
				     XmNleftAttachment,	XmATTACH_WIDGET,
				     XmNleftWidget,		wScrollArea[XPLANE],
				     XmNtopAttachment,	XmATTACH_WIDGET,
				     XmNtopWidget,		wOrient,
				     XmNrightAttachment,	XmATTACH_FORM,
				     XmNbottomAttachment,	XmATTACH_FORM,
				     NULL);

#endif
  
  for(np = 0; np < NPLANES; np++) {
    AddStaticCallback(wPlotPlane[np], XmNinputCallback,
		      &PltApp::DoRubberBanding, (XtPointer) np);
    AddStaticEventHandler(wPlotPlane[np], ExposureMask, &PltApp::DoExposePicture,
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
  for(np = 0; np != NPLANES; np++)
    pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPlotPlane[np]));
  
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
  XYplotparameters = new XYPlotParameters(this, plottertitle);

#if (BL_SPACEDIM == 2)

  if(animating2d) {
    if(currentDerived == "vol_frac") {
      globalMin = 0.0;
      globalMax = 1.0;
    }
    else {
      aString outbuf("Finding global min & max values for ");
      outbuf += currentDerived;
      outbuf += "...\n";
      strcpy(buffer, outbuf.c_str());
      PrintMessage(buffer);
      
      Real dataMin, dataMax;
      globalMin =  AV_BIG_REAL;
      globalMax = -AV_BIG_REAL;
      for(int iFrame = 0; iFrame < animFrames; ++iFrame) {
	int coarseLevel(0);
	int fineLevel(amrPicturePtrArray[ZPLANE]->NumberOfLevels());
        for(int lev = coarseLevel; lev < fineLevel; ++lev) {
          bool minMaxValid(false);
          DataServices::Dispatch(DataServices::MinMaxRequest,
                                 dataServicesPtr[iFrame],
                                 (void *) &(amrData.ProbDomain()[lev]),
                                 (void *) &(derivedStrings[cderived]),
				 lev, &dataMin, &dataMax, &minMaxValid);
          if(minMaxValid) {
            globalMin = Min(globalMin, dataMin);
            globalMax = Max(globalMax, dataMax);
          }
	}
      }
    } 
  }
#endif

  for(np = 0; np < NPLANES; np++)
    amrPicturePtrArray[np]->CreatePicture(XtWindow(wPlotPlane[np]),
					  pltPaletteptr,
					  derivedStrings[cderived]);

  FileType fileType = dataServicesPtr[currentFrame]->GetFileType();
  BL_ASSERT(fileType != INVALIDTYPE);
  if(fileType == FAB  || fileType == MULTIFAB)
    currentDerived = amrPicturePtrArray[ZPLANE]->CurrentDerived();

#if (BL_SPACEDIM == 3)
  viewTrans.MakeTransform();
  labelAxes = false;
  transDetached = false;
  
  for(np = 0; np < NPLANES; np++) {
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
  
  for(np = 0; np < NPLANES; np++) {
    amrPicturePtrArray[np]->SetDataMin(amrPicturePtrArray[np]->GetRegionMin());
    amrPicturePtrArray[np]->SetDataMax(amrPicturePtrArray[np]->GetRegionMax());
  }  

}	// end of PltAppInit



// -------------------------------------------------------------------
void PltApp::DoExposeRef(Widget, XtPointer, XtPointer) {
  int zPlanePosX = 10, zPlanePosY = 15;
  int whiteColor = pltPaletteptr->WhiteIndex();
  int zpColor = whiteColor;
  char sX[LINELENGTH], sY[LINELENGTH], sZ[LINELENGTH];
  
  XClearWindow(GAptr->PDisplay(), XtWindow(wControlForm));
  
  strcpy(sX, "X");
  strcpy(sY, "Y");
  strcpy(sZ, "Z");
  
  DrawAxes(wControlForm, zPlanePosX, zPlanePosY, 0, sX, sY, zpColor);
#if (BL_SPACEDIM == 3)
  int axisLength = 20;
  int ypColor = whiteColor, xpColor = whiteColor;
  int xyzAxisLength = 50;
  int stringOffsetX = 4, stringOffsetY = 20;
  int yPlanePosX = 80, yPlanePosY = 15;
  int xPlanePosX = 10;
  int xPlanePosY = (int) (axisLength + zPlanePosY + 1.4 * stringOffsetY);
  char temp[LINELENGTH];
  Dimension width, height;
  XtVaGetValues(wControlForm, XmNwidth, &width, XmNheight, &height, NULL);
  int centerX = (int) width/2;
  int centerY = (int) height/2-16;
  
  DrawAxes(wControlForm, yPlanePosX, yPlanePosY, 0, sX, sZ, ypColor);
  DrawAxes(wControlForm, xPlanePosX, xPlanePosY, 0, sY, sZ, xpColor);
  
  sprintf(temp, "Z=%i", amrPicturePtrArray[ZPLANE]->GetSlice());
  //                (amrPicturePtrArray[YPLANE]->
  //		ImageSizeV()-1 - amrPicturePtrArray[YPLANE]->GetHLine())/
  //		currentScale + ivLowOffsetMAL[ZDIR]);
  // FIXME:
  XSetForeground(GAptr->PDisplay(), GAptr->PGC(), zpColor);
  XDrawString(GAptr->PDisplay(), XtWindow(wControlForm), GAptr->PGC(),
	      centerX-xyzAxisLength+12,
	      centerY+xyzAxisLength+4,
	      temp, strlen(temp));
  
  sprintf(temp, "Y=%i", amrPicturePtrArray[YPLANE]->GetSlice());
  //amrPicturePtrArray[XPLANE]->GetVLine()/
  //		currentScale + ivLowOffsetMAL[YDIR]);
  // FIXME:
  XSetForeground(GAptr->PDisplay(), GAptr->PGC(), ypColor);
  XDrawString(GAptr->PDisplay(), XtWindow(wControlForm), GAptr->PGC(),
	      centerX+stringOffsetX,
	      centerY-xyzAxisLength+4,
	      temp, strlen(temp));
  
  sprintf(temp, "X=%i", amrPicturePtrArray[XPLANE]->GetSlice());
  //amrPicturePtrArray[ZPLANE]->GetVLine()/
  //currentScale + ivLowOffsetMAL[XDIR]);
  // FIXME:
  XSetForeground(GAptr->PDisplay(), GAptr->PGC(), xpColor);
  XDrawString(GAptr->PDisplay(), XtWindow(wControlForm), GAptr->PGC(),
	      centerX+4*stringOffsetX,
	      centerY+stringOffsetY+2,
	      temp, strlen(temp));
  
  // FIXME:
  XSetForeground(XtDisplay(wControlForm), GAptr->PGC(), ypColor);
  XDrawLine(GAptr->PDisplay(), XtWindow(wControlForm), GAptr->PGC(),
	    centerX, centerY, centerX, centerY-xyzAxisLength);
  XDrawLine(GAptr->PDisplay(), XtWindow(wControlForm), GAptr->PGC(),
	    centerX, centerY, centerX+xyzAxisLength, centerY);
  XDrawLine(GAptr->PDisplay(), XtWindow(wControlForm), GAptr->PGC(),
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
  // FIXME:
  XSetForeground(XtDisplay(wArea), GAptr->PGC(), color);
  XDrawLine(XtDisplay(wArea), XtWindow(wArea), GAptr->PGC(),
	    xpos+5, ypos, xpos+5, ypos+axisLength);
  XDrawLine(XtDisplay(wArea), XtWindow(wArea), GAptr->PGC(),
	    xpos+5, ypos+axisLength, xpos+5+axisLength, ypos+axisLength);
  XDrawString(XtDisplay(wArea), XtWindow(wArea), GAptr->PGC(),
	      xpos+5+axisLength, ypos+5+axisLength, hLabel, strlen(hLabel));
  XDrawString(XtDisplay(wArea), XtWindow(wArea), GAptr->PGC(),
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
  previousScale = currentScale;
  currentScale  = (unsigned long) newScale;
  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }
  for(int V = 0; V < NPLANES; ++V) {
    if(V == activeView) {
      startX = (int) startX / previousScale * currentScale;
      startY = (int) startY / previousScale * currentScale;
      endX   = (int) endX   / previousScale * currentScale;
      endY   = (int) endY   / previousScale * currentScale;
      amrPicturePtrArray[activeView]->SetRegion(startX, startY, endX, endY);
    }
    startcutX[V]  /= previousScale;
    startcutY[V]  /= previousScale;
    finishcutX[V] /= previousScale;
    finishcutY[V] /= previousScale;
    startcutX[V]  *= currentScale;
    startcutY[V]  *= currentScale;
    finishcutX[V] *= currentScale;
    finishcutY[V] *= currentScale;
    amrPicturePtrArray[V]->SetSubCut(startcutX[V],  startcutY[V],
				     finishcutX[V], finishcutY[V]);
    amrPicturePtrArray[V]->ChangeScale(currentScale);
    XtVaSetValues(wPlotPlane[V], 
		  XmNwidth,	amrPicturePtrArray[V]->ImageSizeH()+1,
		  XmNheight,	amrPicturePtrArray[V]->ImageSizeV()+1,
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
  minDrawnLevel = minAllowableLevel;
  maxDrawnLevel = newLevel + minAllowableLevel;

  for(int V = 0; V < NPLANES; ++V) {
    amrPicturePtrArray[V]->ChangeLevel(minDrawnLevel, maxDrawnLevel);
    
#if (BL_SPACEDIM == 3)
    if(V == ZPLANE) {
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
      if(! XmToggleButtonGetState(wAutoDraw)) {
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
    datasetPtr->Render(trueRegion, 
		       amrPicturePtrArray[activeView],
		       //datasetPtr->GetAmrPicturePtr(),
		       this, hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  }
  DoExposeRef();
}

// -------------------------------------------------------------------
void PltApp::ChangeDerived(Widget w, XtPointer client_data, XtPointer) {
  if(w == wCurrDerived) {
    XtVaSetValues(w, XmNset, true, NULL);
    return;
  }
  XtVaSetValues(wCurrDerived, XmNset, false, NULL);
  wCurrDerived = w;

  maxDrawnLevel = amrPicturePtrArray[ZPLANE]->MaxAllowableLevel();

  unsigned long derivedNumber = (unsigned long) client_data;
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();

#if (BL_SPACEDIM == 3)
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  // ------------------------- swf
  projPicturePtr->GetVolRenderPtr()->InvalidateSWFData();
  projPicturePtr->GetVolRenderPtr()->InvalidateVPData();
#endif
  // ------------------------- end swf
  Clear();
#endif

  writingRGB = false;
  paletteDrawn = false;
  if(animating2d) {
    ResetAnimation();
    DirtyFrames();
    if(rangeType == USEFILE) {
      Real dataMin, dataMax;
      globalMin =  AV_BIG_REAL;
      globalMax = -AV_BIG_REAL;
      int coarseLevel(0);
      int fineLevel(maxDrawnLevel);
      for(int lev = coarseLevel; lev <= fineLevel; lev++) {
	bool minMaxValid(false);
	DataServices::Dispatch(DataServices::MinMaxRequest,
			       dataServicesPtr[currentFrame],
			       (void *) &(amrData.ProbDomain()[lev]),
			       (void *) &(derivedStrings[derivedNumber]),
			       lev, &dataMin, &dataMax, &minMaxValid);
	if(!minMaxValid) continue;
	globalMin = Min(globalMin, dataMin);
	globalMax = Max(globalMax, dataMax);
      }
    }
    else if(strcmp(derivedStrings[derivedNumber].c_str(),"vol_frac") == 0) {
      globalMin = 0.0;
      globalMax = 1.0;
    }
    else {
      aString outbuf("Finding global min & max values for ");
      outbuf += derivedStrings[derivedNumber];
      outbuf += "...\n";
      strcpy(buffer, outbuf.c_str());
      PrintMessage(buffer);
      
      Real dataMin, dataMax;
      globalMin =  AV_BIG_REAL;
      globalMax = -AV_BIG_REAL;
      int coarseLevel(0);
      int fineLevel(maxDrawnLevel);
      for(int iFrame = 0; iFrame < animFrames; ++iFrame) {
	for(int lev = coarseLevel; lev <= fineLevel; ++lev) {
	  bool minMaxValid(false);
	  DataServices::Dispatch(DataServices::MinMaxRequest,
				 dataServicesPtr[iFrame],
				 (void *) &(amrData.ProbDomain()[lev]),
				 (void *) &(derivedStrings[derivedNumber]),
				 lev, &dataMin, &dataMax, &minMaxValid);
	  if(!minMaxValid) continue;
	  globalMin = Min(globalMin, dataMin);
	  globalMax = Max(globalMax, dataMax);
	}
      }
    } 
  }  // end if(animating2d)
  
  currentDerived = derivedStrings[derivedNumber];
  strcpy(buffer, currentDerived.c_str());
  XmString label_str = XmStringCreateSimple(buffer);
  XtVaSetValues(wPlotLabel, XmNlabelString, label_str, NULL);
  XmStringFree(label_str);

  for(int V = 0; V < NPLANES; ++V) {
    amrPicturePtrArray[V]->SetMaxDrawnLevel(maxDrawnLevel);
    amrPicturePtrArray[V]->ChangeDerived(derivedStrings[derivedNumber],
                                         pltPaletteptr);
  }
  
  if(datasetShowing) {
    int hdir, vdir, sdir;
    hdir = (activeView == XPLANE) ? YDIR : XDIR;
    vdir = (activeView == ZPLANE) ? YDIR : ZDIR;
    sdir = (activeView == YPLANE) ? YDIR : ((activeView == XYPLANE) ? XDIR : ZDIR);
    datasetPtr->Render(trueRegion, amrPicturePtrArray[activeView],
		       this, hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  }

  if(UseSpecifiedMinMax()) {
    Real specifiedMin, specifiedMax;
    GetSpecifiedMinMax(specifiedMin, specifiedMax);
    for(int V = 0; V < NPLANES; ++V) {
      amrPicturePtrArray[V]->SetDataMin(specifiedMin);
      amrPicturePtrArray[V]->SetDataMax(specifiedMax);
    }
  }
  else {
    for(int V = 0; V < NPLANES; ++V) {
      amrPicturePtrArray[V]->SetDataMin(amrPicturePtrArray[V]->GetRegionMin());
      amrPicturePtrArray[V]->SetDataMax(amrPicturePtrArray[V]->GetRegionMax());
    }
  }
}

// -------------------------------------------------------------------
void PltApp::ChangeContour(Widget w, XtPointer input_data, XtPointer) {
  unsigned long newContour = (unsigned long) input_data;
  if(newContour == currentContour) return;
  XtVaSetValues(wContourLabel,
		XmNsensitive, (newContour != 0 && newContour != 4),
		NULL);
  XtVaSetValues(wNumberContours,
		XmNsensitive, (newContour != 0 && newContour != 4),
		NULL);
  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }
  currentContour = newContour;
  for(int np = 0; np < NPLANES; ++np) 
    amrPicturePtrArray[np]->ChangeContour((int) newContour);
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
  if(tmpContourNumString.length() >= 3) stringOk = false;
  else for(int i = 0; i < tmpContourNumString.length(); ++i)
    if( ! isdigit(tmpContourNumString[i])) stringOk = false;
  if(stringOk) {
    contourNumString = tmpContourNumString;
    numContours = atoi(contourNumString.c_str());
    SetNumContours();
  }

  XtVaSetValues(w, XmNvalue, contourNumString.c_str(), NULL);
}


// -------------------------------------------------------------------
void PltApp::SetNumContours(bool bRedrawAmrPicture) {
  for(int ii = 0; ii < NPLANES; ++ii) {
    amrPicturePtrArray[ii]->SetContourNumber(numContours);
    if(bRedrawAmrPicture)
      amrPicturePtrArray[ii]->Draw(amrPicturePtrArray[ii]->MinDrawnLevel(), 
				   amrPicturePtrArray[ii]->MaxDrawnLevel());
  }
}
  
  

// -------------------------------------------------------------------
void PltApp::ToggleRange(Widget w, XtPointer client_data, XtPointer call_data) {
  unsigned long r = (unsigned long) client_data;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *) call_data;
  if(state->set == true) rangeType = (Range) r;
}

// -------------------------------------------------------------------
void PltApp::DoSubregion(Widget, XtPointer, XtPointer) {
  Box subregionBox;
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  int finestLevel(amrData.FinestLevel());
  int maxAllowableLevel(amrPicturePtrArray[ZPLANE]->MaxAllowableLevel());
  int newMinAllowableLevel;
  
  // subregionBox is at the maxAllowableLevel
  
#if (BL_SPACEDIM == 2)
  if(selectionBox.bigEnd(XDIR) == 0 || selectionBox.bigEnd(YDIR) == 0) return;
  subregionBox = selectionBox + ivLowOffsetMAL;
#endif
  
#if (BL_SPACEDIM == 3)
  int np;
  for(np = 0; np < NPLANES; np++) {
    startcutX[np]  /= currentScale;
    startcutY[np]  /= currentScale;
    finishcutX[np] /= currentScale;
    finishcutY[np] /= currentScale;
  }
  
  subregionBox.setSmall(XDIR, Min(startcutX[ZPLANE], finishcutX[ZPLANE]));
  subregionBox.setSmall(YDIR, Min(startcutX[XPLANE], finishcutX[XPLANE]));
  subregionBox.setSmall(ZDIR, (amrPicturePtrArray[XPLANE]->ImageSizeV()-1)/
			currentScale - Max(startcutY[XPLANE], finishcutY[XPLANE]));
  subregionBox.setBig(XDIR, Max(startcutX[ZPLANE], finishcutX[ZPLANE]));
  subregionBox.setBig(YDIR, Max(startcutX[XPLANE], finishcutX[XPLANE]));
  subregionBox.setBig(ZDIR, (amrPicturePtrArray[XPLANE]->ImageSizeV()-1)/
		      currentScale - Min(startcutY[XPLANE], finishcutY[XPLANE]));
  
  if(subregionBox.numPts() <= 4) {
    for(np = 0; np < NPLANES; np++) {
      startcutX[np]  *= currentScale;
      startcutY[np]  *= currentScale;
      finishcutX[np] *= currentScale;
      finishcutY[np] *= currentScale;
    }
    return;
  }

  subregionBox += ivLowOffsetMAL;   // shift to true data region
#endif
  
  Box tempRefinedBox = subregionBox;
  tempRefinedBox.refine(CRRBetweenLevels(maxAllowableLevel, finestLevel,
					 amrData.RefRatio()));
  // this puts tempRefinedBox in terms of the finest level
  newMinAllowableLevel = 0;//amrData.FinestContainingLevel(
                           // tempRefinedBox, finestLevel);
  newMinAllowableLevel = Min(newMinAllowableLevel, maxAllowableLevel);
  
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
  SubregionPltApp(wTopLevel, subregionBox, ivOffset, amrPicturePtrArray[ZPLANE],
		  this, palFilename, animating2d, currentDerived, fileName);
  
#if (BL_SPACEDIM == 3)
  for(np = 0; np < NPLANES; np++) {
    startcutX[np]  *= currentScale;
    startcutY[np]  *= currentScale;
    finishcutX[np] *= currentScale;
    finishcutY[np] *= currentScale;
  }
#endif
}  // end DoSubregion
  
// -------------------------------------------------------------------
void PltApp::DoDatasetButton(Widget, XtPointer, XtPointer) {
  if(selectionBox.bigEnd(XDIR) == 0 || selectionBox.bigEnd(YDIR) == 0) return;

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
    datasetPtr->Render(trueRegion, amrPicturePtrArray[activeView], this,
		       hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  } else {
    datasetShowing = true;
    datasetPtr = new Dataset(wAmrVisTopLevel, trueRegion,
			     amrPicturePtrArray[activeView], this,
			     hdir, vdir, sdir);
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
		(XtCallbackProc)XtUnmanageChild, (XtPointer) this);
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
  if(GAptr->PVisual() 
     != XDefaultVisual(GAptr->PDisplay(), GAptr->PScreenNumber()))
    XtVaSetValues(wInfoTopLevel, XmNvisual, GAptr->PVisual(),
		  XmNdepth, 8, NULL);
  
  Widget wInfoForm =
    XtVaCreateManagedWidget("infoform", xmFormWidgetClass, wInfoTopLevel, NULL);
  
  int i = 0;
  XtSetArg(args[i], XmNlistSizePolicy, XmRESIZE_IF_POSSIBLE);   i++;
  Widget wInfoList =
    XmCreateScrolledList(wInfoForm, "infoscrolledlist", args, i);
  
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
  
  for(int j = 0; j<numEntries; j++) entries[j] = new char[BUFSIZ];
  
  i=0;
  char buf[BUFSIZ];
  ostrstream prob(buf, BUFSIZ);
  prob.precision(15);
  strcpy(entries[i], fileName.c_str());i++;
  strcpy(entries[i], amrData.PlotFileVersion().c_str()); i++;
  prob << "time: "<< amrData.Time() << ends;
  strcpy(entries[i], buf); i++;
  sprintf(entries[i], "levels: %d", amrData.FinestLevel()+1); i++;
  sprintf(entries[i], "prob domain"); i++;
  for(int k = 0; k<=amrData.FinestLevel(); k++, i++) {
    ostrstream prob_domain(entries[i], BUFSIZ);
    prob_domain << " level " << k << ": " << amrData.ProbDomain()[k] << ends;
  }

  prob.seekp(0);
  prob << "refratios:";
  for(int k=0; k<amrData.FinestLevel(); k++) prob << " " << amrData.RefRatio()[k];
  prob << ends;
  strcpy(entries[i], buf); i++;
  
  prob.seekp(0);
  prob << "probsize:";
  for(int k=0; k<BL_SPACEDIM; k++) prob << " " << amrData.ProbSize()[k];
  prob << ends;
  strcpy(entries[i], buf); i++;
  
  prob.seekp(0);
  prob << "prob lo:";
  for(int k=0; k<BL_SPACEDIM; k++) prob << " " << amrData.ProbLo()[k];
  prob << ends;
  strcpy(entries[i], buf); i++;
  
  prob.seekp(0);
  prob << "prob hi:";
  for(int k=0; k<BL_SPACEDIM; k++) prob<<" "<< amrData.ProbHi()[k];
  prob << ends;
  strcpy(entries[i], buf); i++;

  XmStringTable str_list = (XmStringTable)XtMalloc(numEntries*sizeof(XmString *));
  for(int j = 0; j<numEntries ; j++) str_list[j] = XmStringCreateSimple(entries[j]);
    
  XtVaSetValues(wInfoList,
		XmNvisibleItemCount, numEntries,
		XmNitemCount, numEntries,
		XmNitems, str_list,
		NULL);
  
  for(int j = 0; j<numEntries; j++) delete [] entries[j];
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
  AddStaticCallback(wInfoCloseButton, XmNactivateCallback, &PltApp::CloseInfoWindow);
  
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
  
  wContoursTopLevel =
    XtVaCreatePopupShell("Set Contours",
			 topLevelShellWidgetClass, wAmrVisTopLevel,
			 XmNwidth,		   170,
			 XmNheight,		   220,
			 XmNx,			   xpos+width/2,
			 XmNy,			   ypos+height/2,
			 NULL);
  
  AddStaticCallback(wContoursTopLevel, XmNdestroyCallback,
		    &PltApp::DestroyContoursWindow);
  
  //set visual in case the default isn't 256 pseudocolor
  if(GAptr->PVisual() != XDefaultVisual(GAptr->PDisplay(), GAptr->PScreenNumber()))
    XtVaSetValues(wContoursTopLevel, XmNvisual, GAptr->PVisual(), XmNdepth, 8, NULL);
        
    
  wContoursForm =
    XtVaCreateManagedWidget("Contoursform",
			    xmFormWidgetClass, wContoursTopLevel,
			    NULL);

  wContourRadio = XmCreateRadioBox(wContoursForm, "contourtype", NULL, 0);
  XtVaSetValues(wContourRadio,
		XmNtopAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM, NULL);		
  
  char *conItems[5] = {"Raster", "Raster & Contours", "Color Contours",
		       "B/W Contours", "Velocity Vectors"};
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  int nItems = (amrData.NComp() < (BL_SPACEDIM + 1)) ? 4 : 5;
  for(int j = 0; j != nItems; j++) {
    wid = XtVaCreateManagedWidget(conItems[j],
				  xmToggleButtonGadgetClass,
				  wContourRadio, NULL);
    if(j == currentContour) XtVaSetValues(wid, XmNset, true, NULL);
    AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::ChangeContour,
		      (XtPointer) j);
  }

  XmString label_str = XmStringCreateSimple("Num contours:");
  wContourLabel =
    XtVaCreateManagedWidget("numcontours",
			    xmLabelWidgetClass, wContoursForm,
			    XmNlabelString,     label_str, 
			    XmNleftAttachment,  XmATTACH_FORM,
			    XmNleftOffset,      WOFFSET,
			    XmNtopAttachment,   XmATTACH_WIDGET,
			    XmNtopWidget,       wContourRadio,
			    XmNtopOffset,       WOFFSET,
			    XmNsensitive,       (currentContour != 0 &&
						 currentContour != 4),
			    NULL);
  XmStringFree(label_str);

  wNumberContours =
    XtVaCreateManagedWidget("contours",
			    xmTextFieldWidgetClass, wContoursForm,
			    XmNtopAttachment,    XmATTACH_WIDGET,
			    XmNtopWidget,        wContourRadio,
			    XmNtopOffset,        WOFFSET,
			    XmNleftAttachment,   XmATTACH_WIDGET,
			    XmNleftWidget,       wContourLabel,
			    XmNleftOffset,       WOFFSET,
			    XmNvalue,            contourNumString.c_str(),
			    XmNcolumns,          4,
			    XmNsensitive,       (currentContour != 0 &&
						 currentContour != 4),
			    NULL);
  AddStaticCallback(wNumberContours, XmNactivateCallback, &PltApp::ReadContourString);
  
  wCloseButton =
    XtVaCreateManagedWidget(" Close ",
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

void PltApp::DestroyContoursWindow(Widget, XtPointer, XtPointer) {
  contoursShowing = false;
}

void PltApp::CloseContoursWindow(Widget, XtPointer, XtPointer) {
  XtPopdown(wContoursTopLevel);
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
  char format[LINELENGTH];
  char fMin[LINELENGTH];
  char fMax[LINELENGTH];
  strcpy(format, formatString.c_str());
  Widget wSetRangeForm, wDoneButton, wCancelButton, wSetRangeRadioBox,
    wRangeRC, wid;

  
  // do these here to set XmNwidth of wSetRangeTopLevel
  sprintf(fMin, format, amrPicturePtrArray[ZPLANE]->GetMin());
  sprintf(fMax, format, amrPicturePtrArray[ZPLANE]->GetMax());
  sprintf (range, "Min: %s  Max: %s", fMin, fMax);
  
  XtVaGetValues(wAmrVisTopLevel, XmNx, &xpos, XmNy, &ypos,
		XmNwidth, &width, XmNheight, &height, NULL);
  
  wSetRangeTopLevel =
    XtVaCreatePopupShell("Set Range",
			 topLevelShellWidgetClass, wAmrVisTopLevel,
			 XmNwidth,		12*(strlen(range)+2),
			 XmNheight,		150,
			 XmNx,			xpos+width/2,
			 XmNy,			ypos,
			 NULL);
  
  AddStaticCallback(wSetRangeTopLevel, XmNdestroyCallback,
		    &PltApp::DestroySetRangeWindow);
  
  //set visual in case the default isn't 256 pseudocolor
  if(GAptr->PVisual() != XDefaultVisual(GAptr->PDisplay(), GAptr->PScreenNumber()))
    XtVaSetValues(wSetRangeTopLevel, XmNvisual, GAptr->PVisual(), XmNdepth, 8, NULL);
        
    
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
  AddStaticCallback(wDoneButton, XmNactivateCallback,
		    &PltApp::DoDoneSetRange);
  
  wCancelButton = XtVaCreateManagedWidget(" Cancel ",
					  xmPushButtonGadgetClass, wSetRangeForm,
					  XmNbottomAttachment, XmATTACH_FORM,
					  XmNbottomOffset, WOFFSET,
					  XmNrightAttachment, XmATTACH_FORM,
					  XmNrightOffset, WOFFSET,
					  NULL);
  AddStaticCallback(wCancelButton, XmNactivateCallback,
		    &PltApp::DoCancelSetRange);
  
  // make the radio box
  wSetRangeRadioBox = XmCreateRadioBox(wSetRangeForm,
				       "setrangeradiobox", NULL, 0);
  
  wRangeRadioButton[USEGLOBAL] =
    XtVaCreateManagedWidget("Global",
			    xmToggleButtonWidgetClass, wSetRangeRadioBox, NULL);
  AddStaticCallback(wRangeRadioButton[USEGLOBAL], XmNvalueChangedCallback,
		    &PltApp::ToggleRange, (XtPointer) USEGLOBAL);
  wRangeRadioButton[USELOCAL] =
    XtVaCreateManagedWidget("Local",
			    xmToggleButtonWidgetClass, wSetRangeRadioBox, NULL);
  AddStaticCallback(wRangeRadioButton[USELOCAL], XmNvalueChangedCallback,
		    &PltApp::ToggleRange, (XtPointer) USELOCAL);
  wRangeRadioButton[USESPECIFIED] =
    XtVaCreateManagedWidget("User",
			    xmToggleButtonWidgetClass, wSetRangeRadioBox, NULL);
  AddStaticCallback(wRangeRadioButton[USESPECIFIED], XmNvalueChangedCallback,
		    &PltApp::ToggleRange, (XtPointer) USESPECIFIED);
  if(animating2d) {
    wRangeRadioButton[USEFILE] =
      XtVaCreateManagedWidget("File",
			      xmToggleButtonWidgetClass, wSetRangeRadioBox, NULL);
    AddStaticCallback(wRangeRadioButton[USEFILE], XmNvalueChangedCallback,
		      &PltApp::ToggleRange, (XtPointer) USEFILE);
  }
    
  rangeType = amrPicturePtrArray[ZPLANE]->GetWhichRange();
  XtVaSetValues(wRangeRadioButton[rangeType], XmNset, true, NULL);
  
  wRangeRC = 
    XtVaCreateManagedWidget("rangeRC", xmRowColumnWidgetClass,
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
  sprintf(fMin, format, amrPicturePtrArray[ZPLANE]->GetMin());
  sprintf(fMax, format, amrPicturePtrArray[ZPLANE]->GetMax());
  sprintf(range, "Min: %s", fMin);
  XtVaCreateManagedWidget(range, xmLabelGadgetClass, wRangeRC, NULL);
  sprintf(range, "Max: %s", fMax);
  XtVaCreateManagedWidget(range, xmLabelGadgetClass, wRangeRC, NULL);

  sprintf(fMin, format, amrPicturePtrArray[ZPLANE]->GetRegionMin());
  sprintf(fMax, format, amrPicturePtrArray[ZPLANE]->GetRegionMax());
  sprintf(range, "Min: %s", fMin);
  XtVaCreateManagedWidget(range, xmLabelGadgetClass, wRangeRC, NULL);
  sprintf(range, "Max: %s", fMin, fMax);
  XtVaCreateManagedWidget(range, xmLabelGadgetClass, wRangeRC, NULL);

  wid =
    XtVaCreateManagedWidget("wid", xmRowColumnWidgetClass, wRangeRC,
			    XmNorientation, XmHORIZONTAL,
			    XmNleftOffset,       0,
			    NULL);
  XtVaCreateManagedWidget("Min:", xmLabelGadgetClass, wid, NULL);
  sprintf(range, format, amrPicturePtrArray[ZPLANE]->GetSpecifiedMin());
  wUserMin =
    XtVaCreateManagedWidget("local range",
			    xmTextFieldWidgetClass, wid,
			    XmNvalue,		range,
			    XmNeditable,	true,
			    XmNcolumns,		strlen(range)+2,
			    NULL);
  AddStaticCallback(wUserMin, XmNactivateCallback, &PltApp::DoUserMin);
  
  wid =
    XtVaCreateManagedWidget("wid", xmRowColumnWidgetClass, wRangeRC,
			    XmNorientation, XmHORIZONTAL,
 			    XmNleftOffset,       0,
			    NULL);
  XtVaCreateManagedWidget("Max:", xmLabelGadgetClass, wid, NULL);
  sprintf(range, format, amrPicturePtrArray[ZPLANE]->GetSpecifiedMax());
  wUserMax =
    XtVaCreateManagedWidget("local range",
			    xmTextFieldWidgetClass, wid,
			    XmNvalue,		range,
			    XmNeditable,	true,
			    XmNcolumns,		strlen(range)+2,
			    NULL);
  AddStaticCallback(wUserMax, XmNactivateCallback, &PltApp::DoUserMax);
  
  XtManageChild(wSetRangeRadioBox);
  XtManageChild(wRangeRC);
  XtManageChild(wDoneButton);
  XtManageChild(wCancelButton);
  XtPopup(wSetRangeTopLevel, XtGrabNone);
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wSetRangeTopLevel));
  setRangeShowing = true;
}

// -------------------------------------------------------------------
void PltApp::DoDoneSetRange(Widget, XtPointer, XtPointer) {
  paletteDrawn = false;

  int np;
  Real umin = atof(XmTextFieldGetString(wUserMin));
  Real umax = atof(XmTextFieldGetString(wUserMax));

  for(np = 0; np < NPLANES; np++) {
    amrPicturePtrArray[np]->SetDataMin(umin);
    amrPicturePtrArray[np]->SetDataMax(umax);
  }

  if(rangeType != amrPicturePtrArray[ZPLANE]->GetWhichRange() ||
     rangeType == USESPECIFIED)
  {
    for(np = 0; np < NPLANES; np++) {
      amrPicturePtrArray[np]->SetWhichRange(rangeType);	
      amrPicturePtrArray[np]->ChangeDerived(currentDerived, pltPaletteptr);
    }
#if (BL_SPACEDIM == 3)
    Clear();
#endif
    if(animating2d) {
      ResetAnimation();
      DirtyFrames(); 
      ShowFrame();
    }
  }
  XtDestroyWidget(wSetRangeTopLevel);

  if(datasetShowing) {
    datasetPtr->DoRaise();
    int hdir, vdir, sdir;
    if(activeView==ZPLANE) {
      hdir = XDIR;
      vdir = YDIR;
      sdir = ZDIR;
    }
    if(activeView==YPLANE) {
      hdir = XDIR;
      vdir = ZDIR;
      sdir = YDIR;
    }
    if(activeView==XPLANE) {
      hdir = YDIR;
      vdir = ZDIR;
      sdir = XDIR;
    }
    datasetPtr->Render(trueRegion, amrPicturePtrArray[activeView], this,
    hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
    }
}


// -------------------------------------------------------------------
void PltApp::DoCancelSetRange(Widget, XtPointer, XtPointer) {
  XtDestroyWidget(wSetRangeTopLevel);
}

void PltApp::DestroySetRangeWindow(Widget, XtPointer, XtPointer) {
  setRangeShowing = false;
}

// -------------------------------------------------------------------
void PltApp::DoUserMin(Widget, XtPointer, XtPointer) {
  if(rangeType != USESPECIFIED)
    XtVaSetValues(wRangeRadioButton[rangeType], XmNset, false, NULL);
  XtVaSetValues(wRangeRadioButton[USESPECIFIED], XmNset, true, NULL);
  rangeType = USESPECIFIED;
  DoDoneSetRange(NULL, NULL, NULL);
}


// -------------------------------------------------------------------
void PltApp::DoUserMax(Widget, XtPointer, XtPointer) {
  if(rangeType != USESPECIFIED) {
    XtVaSetValues(wRangeRadioButton[rangeType], XmNset, false, NULL);
  }
  XtVaSetValues(wRangeRadioButton[USESPECIFIED], XmNset, true, NULL);
  rangeType = USESPECIFIED;
  DoDoneSetRange(NULL, NULL, NULL);
}


// -------------------------------------------------------------------
void PltApp::DoBoxesButton(Widget, XtPointer, XtPointer) {
  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }
  showBoxes = !showBoxes;
  amrPicturePtrArray[ZPLANE]->ToggleBoxes();
#if (BL_SPACEDIM == 3)
    projPicturePtr->MakeBoxes(); 
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
    if(! XmToggleButtonGetState(wAutoDraw)) {
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
      DoExposeTransDA();
    }
#else
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
      DoExposeTransDA();
#endif
    amrPicturePtrArray[YPLANE]->ToggleBoxes();
    amrPicturePtrArray[XPLANE]->ToggleBoxes();
#endif

  // draw a bounding box around the image
  int imageSizeX = amrPicturePtrArray[ZPLANE]->ImageSizeH();
  int imageSizeY = amrPicturePtrArray[ZPLANE]->ImageSizeV();
  XSetForeground(GAptr->PDisplay(), GAptr->PGC(), pltPaletteptr->WhiteIndex());
  XDrawLine(GAptr->PDisplay(), XtWindow(wPlotPlane[ZPLANE]), GAptr->PGC(),
            0, imageSizeY, imageSizeX, imageSizeY);
  XDrawLine(GAptr->PDisplay(), XtWindow(wPlotPlane[ZPLANE]), GAptr->PGC(),
            imageSizeX, 0, imageSizeX, imageSizeY);
}

// -------------------------------------------------------------------
void PltApp::DoOpenPalFile(Widget w, XtPointer, XtPointer call_data) {
  char *palfile;
  if( ! XmStringGetLtoR(((XmFileSelectionBoxCallbackStruct *) call_data)->value,
		        XmSTRING_DEFAULT_CHARSET, &palfile))
  {
    cerr << "CBOpenPalFile : system error" << endl;
    return;
  }
  XtPopdown(XtParent(w));
  pltPaletteptr->ChangeWindowPalette(palfile, XtWindow(wAmrVisTopLevel));
# ifdef BL_VOLUMERENDER
  projPicturePtr->GetVolRenderPtr()->SetTransferProperties();
  projPicturePtr->GetVolRenderPtr()->InvalidateVPData();
  showing3dRender = false;
  // If we could also clear the window...
  if(XmToggleButtonGetState(wAutoDraw)) DoRender(wAutoDraw, NULL, NULL);
# endif
  palFilename = palfile;
  
  XYplotparameters->ResetPalette();
  for(int np = 0; np != BL_SPACEDIM; ++np) {
    if(XYplotwin[np]) XYplotwin[np]->SetPalette();
  }
}


XYPlotDataList* PltApp::CreateLinePlot(int V, int sdir, int mal, int ix,
				       aString *derived) {
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
    if(sdir == XDIR) { tdir = ZDIR; dir1 = YDIR; dir2 = ZDIR; }
    else             { tdir = XDIR; dir1 = XDIR; dir2 = YDIR; }
    break;
  case XPLANE:
    dir1 = XDIR;
    if(sdir == YDIR) tdir = ZDIR; 
    else             tdir = YDIR;
    dir1 = XDIR;
    dir2 = tdir;
    break;
#endif
  }
  Array<Box> trueRegion(mal+1);
  trueRegion[mal] = amrPicturePtrArray[V]->GetSliceBox();
  trueRegion[mal].setSmall(tdir, ivLowOffsetMAL[tdir] + ix);
  trueRegion[mal].setBig(tdir, trueRegion[mal].smallEnd(tdir));
  int lev;
  for(lev = mal - 1; lev >= 0; --lev) {
    trueRegion[lev] = trueRegion[mal];
    trueRegion[lev].coarsen(CRRBetweenLevels(lev, mal, amrData.RefRatio()));
  }
  // Create an array of titles corresponding to the intersected line.
  Array<Real> XdX(mal+1);
  Array<char *> intersectStr(mal+1);
  
#if (BL_SPACEDIM == 3)
  char buffer[100];
  sprintf(buffer, "%s%s %s%s",
	  (dir1 == XDIR) ? "X=" : "Y=", formatString.c_str(),
	  (dir2 == YDIR) ? "Y=" : "Z=", formatString.c_str());
#endif
  for(lev = 0; lev <= mal; ++lev) {
    XdX[lev] = amrData.DxLevel()[lev][sdir];
    intersectStr[lev] = new char[30];
#if (BL_SPACEDIM == 2)
    sprintf(intersectStr[lev], ((dir1 == XDIR) ? "X=" : "Y="));
    sprintf(intersectStr[lev]+2, formatString.c_str(), gridOffset[dir1] +
	    (0.5 + trueRegion[lev].smallEnd(dir1))*amrData.DxLevel()[lev][dir1]);
#elif (BL_SPACEDIM == 3)
    sprintf(intersectStr[lev], buffer,
	    amrData.DxLevel()[lev][dir1] * (0.5 + trueRegion[lev].smallEnd(dir1)) +
	    gridOffset[dir1],
	    amrData.DxLevel()[lev][dir2] * (0.5 + trueRegion[lev].smallEnd(dir2)) +
	    gridOffset[dir2]);
#endif	    
  }
  XYPlotDataList *newlist =
    new XYPlotDataList(*derived, mal, ix, amrData.RefRatio(),
		       XdX, intersectStr, gridOffset[sdir]);
  int lineOK;
  DataServices::Dispatch(DataServices::LineValuesRequest,
			 dataServicesPtr[currentFrame],
			 mal + 1,
			 (void *) (trueRegion.dataPtr()),
			 sdir,
			 (void *) derived,
			 minAllowableLevel, mal,
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

  int scale(currentScale);
  int V((unsigned long) client_data);
  int imageHeight(amrPicturePtrArray[V]->ImageSizeV() - 1);
  int imageWidth(amrPicturePtrArray[V]->ImageSizeH() - 1);
  int oldX(Max(0, Min(imageWidth,  cbs->event->xbutton.x)));
  int oldY(Max(0, Min(imageHeight, cbs->event->xbutton.y)));
  int mal(amrPicturePtrArray[ZPLANE]->MaxAllowableLevel());
  const AmrData &amrData(dataServicesPtr[currentFrame]->AmrDataRef());
  Display *display(GAptr->PDisplay());
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
    int anchorX = oldX;
    int anchorY = oldY;
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
	  XDrawRectangle(display,
			 amrPicturePtrArray[V]->PictureWindow(),
			 rbgc, rStartX, rStartY, rWidth, rHeight);
#if (BL_SPACEDIM == 3)
	  // 3D sub domain selecting
	  switch (V) {
	  case ZPLANE:
	    XDrawRectangle(display,
			   amrPicturePtrArray[YPLANE]->PictureWindow(),
			   rbgc, rStartX, startcutY[YPLANE], rWidth,
			   abs(finishcutY[YPLANE]-startcutY[YPLANE]));
	    rStartPlane = (anchorY < oldY) ? oldY : anchorY;
	    XDrawRectangle(display,
			   amrPicturePtrArray[XPLANE]->PictureWindow(),
			   rbgc, imageHeight-rStartPlane, startcutY[XPLANE],
			   rHeight,
			   abs(finishcutY[XPLANE]-startcutY[XPLANE]));
	    break;
	  case YPLANE:
	    XDrawRectangle(display,
			   amrPicturePtrArray[ZPLANE]->PictureWindow(),
			   rbgc, rStartX, startcutY[ZPLANE], rWidth,
			   abs(finishcutY[ZPLANE]-startcutY[ZPLANE]));
	    XDrawRectangle(display,
			   amrPicturePtrArray[XPLANE]->PictureWindow(),
			   rbgc, startcutX[XPLANE], rStartY,
			   abs(finishcutX[XPLANE]-startcutX[XPLANE]),
			   rHeight);
	    break;
	  default: // XPLANE
	    rStartPlane = (anchorX < oldX) ? oldX : anchorX;
	    XDrawRectangle(display,
			   amrPicturePtrArray[ZPLANE]->PictureWindow(),
			   rbgc, startcutX[ZPLANE], imageWidth-rStartPlane,
			   abs(finishcutX[ZPLANE]-startcutX[ZPLANE]), rWidth);
	    XDrawRectangle(display,
			   amrPicturePtrArray[YPLANE]->PictureWindow(),
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
	
	newX = Max(0, Min(imageWidth,  newX));
	newY = Max(0, Min(imageHeight, newY));
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
	  XDrawRectangle(display,
			 amrPicturePtrArray[YPLANE]->PictureWindow(),
			 rbgc, rStartX, startcutY[YPLANE], rWidth,
			 abs(finishcutY[YPLANE]-startcutY[YPLANE]));
	  rStartPlane = (anchorY < newY) ? newY : anchorY;
	  XDrawRectangle(display,
			 amrPicturePtrArray[XPLANE]->PictureWindow(),
			 rbgc, imageHeight-rStartPlane, startcutY[XPLANE],
			 rHeight,
			 abs(finishcutY[XPLANE]-startcutY[XPLANE]));
	  break;
	case YPLANE:
	  startcutX[ZPLANE] = startcutX[V];
	  finishcutX[ZPLANE] = finishcutX[V];
	  startcutY[XPLANE] = startcutY[V];
	  finishcutY[XPLANE] = finishcutY[V];
	  XDrawRectangle(display,
			 amrPicturePtrArray[ZPLANE]->PictureWindow(),
			 rbgc, rStartX, startcutY[ZPLANE], rWidth,
			 abs(finishcutY[ZPLANE]-startcutY[ZPLANE]));
	  XDrawRectangle(display,
			 amrPicturePtrArray[XPLANE]->PictureWindow(),
			 rbgc, startcutX[XPLANE], rStartY,
			 abs(finishcutX[XPLANE]-startcutX[XPLANE]), rHeight);
	  break;
	default: // XPLANE
	  startcutY[YPLANE] = startcutY[V];
	  finishcutY[YPLANE] = finishcutY[V];
	  startcutY[ZPLANE] = imageWidth - startcutX[V];
	  finishcutY[ZPLANE] = imageWidth - finishcutX[V];
	  rStartPlane = (anchorX < newX) ? newX : anchorX;
	  XDrawRectangle(display,
			 amrPicturePtrArray[ZPLANE]->PictureWindow(),
			 rbgc, startcutX[ZPLANE], imageWidth-rStartPlane,
			 abs(finishcutX[ZPLANE]-startcutX[ZPLANE]), rWidth);
	  XDrawRectangle(display,
			 amrPicturePtrArray[YPLANE]->PictureWindow(),
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
	      z2 = amrPicturePtrArray[ZPLANE]->
		GetSubDomain()[mal].smallEnd(ZDIR);
	    }
	    IntVect ivmin(Min(x1,x2), Min(y1,y2), Min(z1,z2));
	    IntVect ivmax(Max(x1,x2), Max(y1,y2), Max(z1,z2));
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
	
	startX = (Max(0, Min(imageWidth,  anchorX))) / scale;
	startY = (Max(0, Min(imageHeight, anchorY))) / scale;
	endX   = (Max(0, Min(imageWidth,  nextEvent.xbutton.x))) / scale;
	endY   = (Max(0, Min(imageHeight, nextEvent.xbutton.y))) / scale;
	
	// make "aligned" box with correct size, converted to AMR space.
	selectionBox.setSmall(XDIR, Min(startX, endX));
	selectionBox.setSmall(YDIR, ((imageHeight + 1) / scale) -
				     Max(startY, endY) - 1);
	selectionBox.setBig(XDIR, Max(startX, endX));
	selectionBox.setBig(YDIR, ((imageHeight + 1) / scale)  -
				     Min(startY, endY) - 1);
	
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
				 trueRegion.length(),
				 (void *) (trueRegion.dataPtr()),
				 (void *) &currentDerived,
				 minDrawnLevel, maxDrawnLevel,
				 &intersectedLevel, &intersectedGrid,
				 &dataValue, &goodIntersect);
	  char dataValueCharString[LINELENGTH];
	  sprintf(dataValueCharString, formatString.c_str(), dataValue);
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
	}
	else {
	  
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
	tempi = Max(0, Min(imageHeight, nextEvent.xbutton.y));
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
	      ChangeSlice((imageHeight - tempi)/scale + ivLowOffsetMAL[YDIR]);
	  }
	  if(V == YPLANE) {
	    amrPicturePtrArray[ZPLANE]->
	      ChangeSlice((imageHeight - tempi)/scale + ivLowOffsetMAL[ZDIR]);
	  }
	  if(V == XPLANE) {
	    amrPicturePtrArray[ZPLANE]->
	      ChangeSlice((imageHeight - tempi)/scale + ivLowOffsetMAL[ZDIR]);
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
			              &currentDerived);
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
	tempi = Max(0, Min(imageWidth, nextEvent.xbutton.x));
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
	      ChangeSlice((tempi / scale) + ivLowOffsetMAL[XDIR]);
	  }
	  if(V == YPLANE) {
	    amrPicturePtrArray[XPLANE]->
	      ChangeSlice((tempi / scale) + ivLowOffsetMAL[XDIR]);
	  }
	  if(V == XPLANE) {
	    amrPicturePtrArray[YPLANE]->
	      ChangeSlice((tempi / scale) + ivLowOffsetMAL[YDIR]);
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
	  XYPlotDataList *newlist =
	    CreateLinePlot(V, sdir, mal, oldX / scale, &currentDerived);
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
void PltApp::DoExposePicture(Widget w, XtPointer client_data, XtPointer) {
  unsigned long np = (unsigned long) client_data;
  
  amrPicturePtrArray[np]->DoExposePicture();
  // draw bounding box
  int isX = amrPicturePtrArray[np]->ImageSizeH();
  int isY = amrPicturePtrArray[np]->ImageSizeV();
  XSetForeground(GAptr->PDisplay(), GAptr->PGC(),
		 amrPicturePtrArray[np]->GetPalPtr()->WhiteIndex());
  XDrawLine(GAptr->PDisplay(), XtWindow(w), GAptr->PGC(), 0, isY, isX, isY);
  XDrawLine(GAptr->PDisplay(), XtWindow(w), GAptr->PGC(), isX, 0, isX, isY);
}

void PltApp::DoDrawPointerLocation(Widget, XtPointer data, XtPointer cbe) {
  XEvent *event = (XEvent *) cbe;
  unsigned long V = (unsigned long) data;

  if(event->type == LeaveNotify){
    XClearWindow(GAptr->PDisplay(), XtWindow(wLocArea));
    return;
  }

  Window whichRoot, whichChild;
  int rootX, rootY, newX, newY;
  unsigned int inputMask;
  
  XQueryPointer(GAptr->PDisplay(), XtWindow(wPlotPlane[V]),
		&whichRoot, &whichChild,
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

  XSetForeground(GAptr->PDisplay(), GAptr->PGC(),
		 pltPaletteptr->WhiteIndex());
  XClearWindow(GAptr->PDisplay(), XtWindow(wLocArea));
  XDrawString(GAptr->PDisplay(), XtWindow(wLocArea),
	      GAptr->PGC(), 10, 20, locText, strlen(locText));
  
} 

// -------------------------------------------------------------------
void PltApp::DoSpeedScale(Widget, XtPointer, XtPointer call_data) {
  XmScaleCallbackStruct *cbs = (XmScaleCallbackStruct *) call_data;
  frameSpeed = 600 - cbs->value;
# if(BL_SPACEDIM == 3)
  for(int v = 0; v != 3; ++v)
    amrPicturePtrArray[v]->SetFrameSpeed(600 - cbs->value);
# endif
  XSync(GAptr->PDisplay(), false);
}


// -------------------------------------------------------------------
void PltApp::DoBackStep(int plane) {
  switch (plane) {
  case XPLANE:
    if(amrPicturePtrArray[XPLANE]->GetSlice() > amrPicturePtrArray[XPLANE]->
       GetSubDomain()[amrPicturePtrArray[XPLANE]->
		     MaxAllowableLevel()].smallEnd(XDIR)) {
      amrPicturePtrArray[ZPLANE]->SetVLine((amrPicturePtrArray[ZPLANE]->
					    GetVLine()-1)*currentScale);
      amrPicturePtrArray[ZPLANE]->DoExposePicture();
      amrPicturePtrArray[YPLANE]->SetVLine((amrPicturePtrArray[YPLANE]->
					    GetVLine()-1)*currentScale);
      amrPicturePtrArray[YPLANE]->DoExposePicture();
      amrPicturePtrArray[XPLANE]->ChangeSlice(amrPicturePtrArray[XPLANE]->
					      GetSlice()-1);
      break;
    }
    amrPicturePtrArray[ZPLANE]->SetVLine(amrPicturePtrArray[YPLANE]->
					 ImageSizeH()-1);
    amrPicturePtrArray[ZPLANE]->DoExposePicture();
    amrPicturePtrArray[YPLANE]->SetVLine(amrPicturePtrArray[YPLANE]->
					 ImageSizeH()-1);
    amrPicturePtrArray[YPLANE]->DoExposePicture();
    amrPicturePtrArray[XPLANE]->
      ChangeSlice(amrPicturePtrArray[XPLANE]->
		  GetSubDomain()[amrPicturePtrArray[XPLANE]->
				MaxAllowableLevel()].bigEnd(XDIR));
    break;
  case YPLANE:
    if(amrPicturePtrArray[YPLANE]->GetSlice() > amrPicturePtrArray[YPLANE]->
       GetSubDomain()[amrPicturePtrArray[YPLANE]->
		     MaxAllowableLevel()].smallEnd(YDIR)) {
      amrPicturePtrArray[XPLANE]->SetVLine(amrPicturePtrArray[XPLANE]->
					   GetVLine()-1* currentScale);
      amrPicturePtrArray[XPLANE]->DoExposePicture();
      amrPicturePtrArray[ZPLANE]->SetHLine(amrPicturePtrArray[ZPLANE]->
					   GetHLine()+1* currentScale);
      amrPicturePtrArray[ZPLANE]->DoExposePicture();
      amrPicturePtrArray[YPLANE]->ChangeSlice(amrPicturePtrArray[YPLANE]->
					     GetSlice()-1);
      break;
    }
    amrPicturePtrArray[XPLANE]->SetVLine(amrPicturePtrArray[XPLANE]->
					 ImageSizeH()-1);
    amrPicturePtrArray[XPLANE]->DoExposePicture();
    amrPicturePtrArray[ZPLANE]->SetHLine(0);
    amrPicturePtrArray[ZPLANE]->DoExposePicture();
    amrPicturePtrArray[YPLANE]->
      ChangeSlice(amrPicturePtrArray[YPLANE]->
		  GetSubDomain()[amrPicturePtrArray[YPLANE]->
				MaxAllowableLevel()].bigEnd(YDIR));
    break;
  case ZPLANE:
    if(amrPicturePtrArray[ZPLANE]->GetSlice() > amrPicturePtrArray[ZPLANE]->
       GetSubDomain()[amrPicturePtrArray[ZPLANE]->
		     MaxAllowableLevel()].smallEnd(ZDIR)) {
      amrPicturePtrArray[XPLANE]->SetHLine(amrPicturePtrArray[XPLANE]->
					   GetHLine()+1* currentScale);
      amrPicturePtrArray[XPLANE]->DoExposePicture();
      amrPicturePtrArray[YPLANE]->SetHLine(amrPicturePtrArray[YPLANE]->
					   GetHLine()+1* currentScale);
      amrPicturePtrArray[YPLANE]->DoExposePicture();
      amrPicturePtrArray[ZPLANE]->ChangeSlice(amrPicturePtrArray[ZPLANE]->
					      GetSlice()-1);
      break;
    }
    amrPicturePtrArray[XPLANE]->SetHLine(0);
    amrPicturePtrArray[XPLANE]->DoExposePicture();
    amrPicturePtrArray[YPLANE]->SetHLine(0);
    amrPicturePtrArray[YPLANE]->DoExposePicture();
    amrPicturePtrArray[ZPLANE]->
      ChangeSlice(amrPicturePtrArray[ZPLANE]->
		  GetSubDomain()[amrPicturePtrArray[ZPLANE]->
				MaxAllowableLevel()].bigEnd(ZDIR));
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
  switch(plane) {
  case XPLANE:
    if(amrPicturePtrArray[XPLANE]->GetSlice() < amrPicturePtrArray[XPLANE]->
       GetSubDomain()[amrPicturePtrArray[XPLANE]->
		     MaxAllowableLevel()].bigEnd(XDIR)) {
      amrPicturePtrArray[ZPLANE]->SetVLine(amrPicturePtrArray[ZPLANE]->
					   GetVLine()+1* currentScale);
      amrPicturePtrArray[ZPLANE]->DoExposePicture();
      amrPicturePtrArray[YPLANE]->SetVLine(amrPicturePtrArray[YPLANE]->
					   GetVLine()+1* currentScale);
      amrPicturePtrArray[YPLANE]->DoExposePicture();
      amrPicturePtrArray[XPLANE]->ChangeSlice(amrPicturePtrArray[XPLANE]->
					      GetSlice()+1);
      break;
    }
    amrPicturePtrArray[ZPLANE]->SetVLine(0);
    amrPicturePtrArray[ZPLANE]->DoExposePicture();
    amrPicturePtrArray[YPLANE]->SetVLine(0);
    amrPicturePtrArray[YPLANE]->DoExposePicture();
    amrPicturePtrArray[XPLANE]->
      ChangeSlice(amrPicturePtrArray[XPLANE]->
		  GetSubDomain()[amrPicturePtrArray[XPLANE]->
				MaxAllowableLevel()].smallEnd(XDIR));
    break;
  case YPLANE:
    if(amrPicturePtrArray[YPLANE]->GetSlice() < amrPicturePtrArray[YPLANE]->
       GetSubDomain()[amrPicturePtrArray[YPLANE]->
		     MaxAllowableLevel()].bigEnd(YDIR)) {
      amrPicturePtrArray[XPLANE]->SetVLine(amrPicturePtrArray[XPLANE]->
					   GetVLine()+1* currentScale);
      amrPicturePtrArray[XPLANE]->DoExposePicture();
      amrPicturePtrArray[ZPLANE]->SetHLine(amrPicturePtrArray[ZPLANE]->
					   GetHLine()-1* currentScale);
      amrPicturePtrArray[ZPLANE]->DoExposePicture();
      amrPicturePtrArray[YPLANE]->ChangeSlice(amrPicturePtrArray[YPLANE]->
					      GetSlice()+1);
      break;
    }
    amrPicturePtrArray[XPLANE]->SetVLine(0);
    amrPicturePtrArray[XPLANE]->DoExposePicture();
    amrPicturePtrArray[ZPLANE]->SetHLine(amrPicturePtrArray[XPLANE]->
					 ImageSizeV()-1);
    amrPicturePtrArray[ZPLANE]->DoExposePicture();
    amrPicturePtrArray[YPLANE]->
      ChangeSlice(amrPicturePtrArray[YPLANE]->
		  GetSubDomain()[amrPicturePtrArray[YPLANE]->
				MaxAllowableLevel()].smallEnd(YDIR));
    break;
  case ZPLANE:
    if(amrPicturePtrArray[ZPLANE]->GetSlice() < amrPicturePtrArray[ZPLANE]->
       GetSubDomain()[amrPicturePtrArray[ZPLANE]->
		     MaxAllowableLevel()].bigEnd(ZDIR)) {
      amrPicturePtrArray[XPLANE]->SetHLine(amrPicturePtrArray[XPLANE]->
					   GetHLine()-1* currentScale);
      amrPicturePtrArray[XPLANE]->DoExposePicture();
      amrPicturePtrArray[YPLANE]->SetHLine(amrPicturePtrArray[YPLANE]->
					   GetHLine()-1* currentScale);
      amrPicturePtrArray[YPLANE]->DoExposePicture();
      amrPicturePtrArray[ZPLANE]->ChangeSlice(amrPicturePtrArray[ZPLANE]->
					      GetSlice()+1);
      break;
    }
    amrPicturePtrArray[XPLANE]->SetHLine(amrPicturePtrArray[XPLANE]->
					 ImageSizeV()-1);
    amrPicturePtrArray[XPLANE]->DoExposePicture();
    amrPicturePtrArray[YPLANE]->SetHLine(amrPicturePtrArray[YPLANE]->
					 ImageSizeV()-1);
    amrPicturePtrArray[YPLANE]->DoExposePicture();
    amrPicturePtrArray[ZPLANE]->
      ChangeSlice(amrPicturePtrArray[ZPLANE]->
		  GetSubDomain()[amrPicturePtrArray[ZPLANE]->
				MaxAllowableLevel()].smallEnd(ZDIR));
  }
#if (BL_SPACEDIM == 3)
  //what about voume rendering?
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
  currentFrame--;
  if(currentFrame < 0) {
    currentFrame = animFrames-1;
  }
  ShowFrame();
}


// -------------------------------------------------------------------
void PltApp::DoAnimForwardStep() {
  StopAnimation();
  if(writingRGB) {
    DoCreateAnimRGBFile();
    writingRGB = false;
  }
  currentFrame++;
  if(currentFrame==animFrames) {
    currentFrame = 0;
  }
  ShowFrame();
}


// -------------------------------------------------------------------
void PltApp::DoAnimFileScale(Widget, XtPointer, XtPointer cbs) {
  StopAnimation();
  currentFrame = ((XmScaleCallbackStruct *) cbs)->value;
  ShowFrame();
}


// -------------------------------------------------------------------
void PltApp::ResetAnimation() {
  StopAnimation();
  if( ! interfaceReady) {
#   if(BL_SPACEDIM == 2)
    int maxAllowableLevel(amrPicturePtrArray[ZPLANE]->MaxAllowableLevel());
    int newContourNum(amrPicturePtrArray[ZPLANE]->GetContourNumber());
    AmrPicture *tempap = amrPicturePtrArray[ZPLANE];
    Array<Box> domain = tempap->GetSubDomain();
    char tempString[BUFSIZ];
    strcpy(tempString, tempap->CurrentDerived().c_str());
    tempap->SetDataServicesPtr(dataServicesPtr[currentFrame]); 
    
    const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
    Box fineDomain(domain[tempap->MaxAllowableLevel()]);
    fineDomain.refine(CRRBetweenLevels(tempap->MaxAllowableLevel(),
				       amrData.FinestLevel(), amrData.RefRatio()));
    amrPicturePtrArray[ZPLANE] = new AmrPicture(ZPLANE, minAllowableLevel,
						GAptr, fineDomain, 
						tempap, NULL, this,
						bCartGridSmoothing);
    amrPicturePtrArray[ZPLANE]->SetMaxDrawnLevel(maxDrawnLevel);
    SetNumContours(false);
    XtRemoveEventHandler(wPlotPlane[ZPLANE], ExposureMask, false, 
			 (XtEventHandler) &PltApp::StaticEvent,
			 (XtPointer) tempap);
    delete tempap;
    amrPicturePtrArray[ZPLANE]->CreatePicture(XtWindow(wPlotPlane[ZPLANE]),
					      pltPaletteptr, tempString);
    AddStaticEventHandler(wPlotPlane[ZPLANE], ExposureMask,
			  &PltApp::DoExposePicture, (XtPointer) ZPLANE);
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
  for(int dim = 0; dim != 2; ++dim) {
    if(XYplotwin[dim]) {
      XYplotwin[dim]->InitializeAnimation(currentFrame, animFrames);
    }
  }
#endif
}


// -------------------------------------------------------------------
void PltApp::DirtyFrames() {
  paletteDrawn = false;
  for(int i = 0; i < animFrames; i++) {
    if(readyFrames[i]) {
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
  ShowFrame();
  XSync(GAptr->PDisplay(), false);
  animationIId = AddStaticTimeOut(frameSpeed, &PltApp::DoUpdateFrame);
}


// -------------------------------------------------------------------
void PltApp::ShowFrame() {
  interfaceReady = false;
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  if( ! readyFrames[currentFrame] || datasetShowing || rangeType == USEFILE) {
#   if(BL_SPACEDIM == 2)
    AmrPicture *tempap = amrPicturePtrArray[ZPLANE];
    Array<Box> domain = tempap->GetSubDomain();
    char tempString[BUFSIZ];
    strcpy(tempString, tempap->CurrentDerived().c_str());
    
    tempap->SetDataServicesPtr(dataServicesPtr[currentFrame]);
    int newContourNum(tempap->GetContourNumber());
    
    int finestLevel(amrData.FinestLevel());
    int maxlev(DetermineMaxAllowableLevel(amrData.ProbDomain()[finestLevel],
					  finestLevel, MaxPictureSize(), amrData.RefRatio()));
    
    Box fineDomain(domain[tempap->MaxAllowableLevel()]);
    fineDomain.refine(CRRBetweenLevels(tempap->MaxAllowableLevel(),
				       finestLevel, amrData.RefRatio()));
    amrPicturePtrArray[ZPLANE] = new AmrPicture(ZPLANE, minAllowableLevel, 
						GAptr, fineDomain, 
						tempap, NULL, this,
						bCartGridSmoothing);
    XtRemoveEventHandler(wPlotPlane[ZPLANE], ExposureMask, false, 
			 (XtEventHandler) &PltApp::StaticEvent,
			 (XtPointer) tempap);
    SetNumContours(false);
    delete tempap;
    
    amrPicturePtrArray[ZPLANE]->SetMaxDrawnLevel(maxDrawnLevel);
    amrPicturePtrArray[ZPLANE]->CreatePicture(XtWindow(wPlotPlane[ZPLANE]),
					      pltPaletteptr, tempString);
    AddStaticEventHandler(wPlotPlane[ZPLANE], ExposureMask,
			  DoExposePicture, (XtPointer) ZPLANE);
    frameBuffer[currentFrame] = amrPicturePtrArray[ZPLANE]->GetPictureXImage();
#   endif
    readyFrames[currentFrame] = true;
    paletteDrawn = (rangeType == USEFILE) ? false : true;
  }
  
  XPutImage(GAptr->PDisplay(), XtWindow(wPlotPlane[ZPLANE]), GAptr->PGC(),
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
    if(activeView == ZPLANE) {
      hdir = XDIR;
      vdir = YDIR;
      sdir = ZDIR;
    }
    if(activeView == YPLANE) {
      hdir = XDIR;
      vdir = ZDIR;
      sdir = YDIR;
    }
    if(activeView == XPLANE) {
      hdir = YDIR;
      vdir = ZDIR;
      sdir = XDIR;
    }
    datasetPtr->Render(trueRegion, amrPicturePtrArray[activeView], this,
		       hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  }
#if (BL_SPACEDIM == 2)
  for(int dim = 0; dim != 2; ++dim) {
    if(XYplotwin[dim]) {
      XYplotwin[dim]->UpdateFrame(currentFrame, currentDerived);
    }
  }
#endif
}  // end ShowFrame

// -------------------------------------------------------------------
void PltApp::SetGlobalMinMax(Real specifiedMin, Real specifiedMax) {
  BL_ASSERT(specifiedMax > specifiedMin);
  for(int np = 0; np < NPLANES; ++np) {
    amrPicturePtrArray[np]->SetWhichRange(USESPECIFIED);
    amrPicturePtrArray[np]->SetDataMin(specifiedMin);
    amrPicturePtrArray[np]->SetDataMax(specifiedMax);
  }
}

// -------------------------------------------------------------------
void PltApp::SetHVLine(AmrPicture **apArray) {
  for(int i = 0; i < 3; ++i) {
    apArray[i]->SetHVLine();
  }
}


// -------------------------------------------------------------------
void PltApp::AddStaticCallback(Widget w, String cbtype, memberCB cbf, void *d) {
  CBData *cbs = new CBData(this, d, cbf);
  XtAddCallback(w, cbtype, &PltApp::StaticCallback, (XtPointer) cbs);
}


// -------------------------------------------------------------------
void PltApp::AddStaticEventHandler(Widget w, EventMask mask, memberCB cbf, void *d)
{
  CBData *cbs = new CBData(this, d, cbf);
  XtAddEventHandler(w, mask, false, (XtEventHandler) &PltApp::StaticEvent, (XtPointer) cbs);
}
 
 
// -------------------------------------------------------------------
XtIntervalId PltApp::AddStaticTimeOut(int time, memberCB cbf, void *d) {
  CBData *cbs = new CBData(this, d, cbf);
  return XtAppAddTimeOut(appContext, time, &PltApp::StaticTimeOut, (XtPointer) cbs);
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
aString PltApp::initialDerived;
aString PltApp::initialFormatString;

bool  PltApp::PaletteDrawn()          { return PltApp::paletteDrawn;     }
int   PltApp::GetInitialScale()       { return PltApp::initialScale;     }
int   PltApp::GetDefaultShowBoxes()   { return PltApp::defaultShowBoxes; }
aString &PltApp::GetInitialDerived()  { return PltApp::initialDerived;   }
aString &PltApp::GetDefaultPalette()  { return PltApp::defaultPaletteString; }
const aString &PltApp::GetFileName()  { return (fileNames[currentFrame]); }
void  PltApp::PaletteDrawn(bool tOrF) { paletteDrawn = tOrF; }

void PltApp::SetDefaultPalette(const aString &palString) {
  PltApp::defaultPaletteString = palString;
}

void PltApp::SetInitialDerived(const aString &initialderived) {
  PltApp::initialDerived = initialderived;
}

void PltApp::SetInitialScale(int initScale) {
  PltApp::initialScale = initScale;
}

void PltApp::SetInitialFormatString(const aString &formatString) {
  PltApp::initialFormatString = formatString;
}

void PltApp::SetDefaultShowBoxes(int showBoxes) {
  PltApp::defaultShowBoxes = showBoxes;
}

void PltApp::SetInitialWindowHeight(int initWindowHeight) {
  PltApp::initialWindowHeight = initWindowHeight;
}

void PltApp::SetInitialWindowWidth(int initWindowWidth) {
  PltApp::initialWindowWidth = initWindowWidth;
}

void PltApp::SetReserveSystemColors(int reservesystemcolors) {
  reservesystemcolors = Max(0, Min(128, reservesystemcolors));  // arbitrarily
  PltApp::reserveSystemColors = reservesystemcolors;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
