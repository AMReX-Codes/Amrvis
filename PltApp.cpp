// -------------------------------------------------------------------
// PltApp.C 
// -------------------------------------------------------------------
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
#include <TextF.h>
#include <ToggleB.h>
#include <SelectioB.h>
#include <ScrolledW.h>
#include <Scale.h>

#include <cursorfont.h>
#include <ArrowB.h>
#include <CascadeB.h>
#include <Frame.h>

const int MAXSCALE       = 32;
const int MENUFRAME      = 0;
const int PALETTEFRAME   = 1;
const int SETRANGEFRAME  = 2;
const int PLOTFRAME      = 3;
const int CONTROLSFRAME  = 4;

// -------------------------------------------------------------------
PltApp::~PltApp() {

#if (BL_SPACEDIM == 3)
  amrPicturePtrArray[XPLANE]->DoStop();;
  amrPicturePtrArray[YPLANE]->DoStop();;
  amrPicturePtrArray[ZPLANE]->DoStop();;
#endif

  XtRemoveEventHandler(wPlotPlane[ZPLANE], ExposureMask, false, 
  	               (XtEventHandler) CBDoExposePicture,
	               (XtPointer) amrPicturePtrArray[ZPLANE]);

  for(int np = 0; np < NPLANES; np++) {
    delete amrPicturePtrArray[np];
  }

#if (BL_SPACEDIM == 3)
  delete projPicturePtr;
#endif
  delete pltPaletteptr;
  delete GAptr;
  if(datasetShowing) {
    delete datasetPtr;
  }
  if(anim) {
    StopAnimation();
  //for(int i = 1; i < animFrames; i++) {
  //}
  }
  XtDestroyWidget(wAmrVisTopLevel);
}


// -------------------------------------------------------------------
PltApp::PltApp(XtAppContext app, Widget w, const aString &filename,
	       DataServices *dataservicesptr, bool isAnim)
{
    if (! dataservicesptr->AmrDataOk() ) // checks if the Dataservices 
        return;                          // are open and working
  int i;
  anim = isAnim;
  paletteDrawn = false;
  appContext = app;
  wTopLevel = w; 
  fileName = filename;
  if(anim) {
    animFrames = GetFileCount(); 
    fileNames.resize(animFrames);
  } else {
    animFrames = 1;
    fileNames.resize(1);
    currentFrame = 0;
    fileNames[currentFrame] = fileName;
  }
  char shortfilename[BUFSIZ];
  int fnl = fileName.length() - 1;
  while(fnl > -1 && fileName[fnl] != '/') {
    fnl--;
  }
  strcpy(shortfilename, &fileName[fnl+1]);

  wAmrVisTopLevel = XtVaCreatePopupShell(shortfilename, 
			topLevelShellWidgetClass, wTopLevel,
			XmNwidth,	initialWindowWidth,
			XmNheight,	initialWindowHeight,
		 	XmNx,		100+placementOffsetX,
			XmNy,		200+placementOffsetY,
		  	NULL);

  GAptr = new GraphicsAttributes(wAmrVisTopLevel);
  
  FileType fileType = GetDefaultFileType();
  assert(fileType != INVALIDTYPE);
  dataServicesPtr = dataservicesptr;

  if( ! (dataServicesPtr->CanDerive(PltApp::initialDerived)) &&
      ! (fileType == FAB)  && ! (fileType == MULTIFAB))
  {
    cerr << "Bad initial derived:  cannot derive "
	 << PltApp::initialDerived <<endl;
    cerr << "defaulting to "
         << dataServicesPtr->PlotVarNames()[0] << endl;
    SetInitialDerived(dataServicesPtr->PlotVarNames()[0]);
  }
  currentDerived = PltApp::initialDerived;
 
  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  int finestLevel(amrData.FinestLevel());
  int maxlev = DetermineMaxAllowableLevel(amrData.ProbDomain()[finestLevel],
                            finestLevel, MaxPictureSize(), amrData.RefRatio());
  minAllowableLevel = 0;
  Box maxDomain(amrData.ProbDomain()[maxlev]);
  unsigned long dataSize = maxDomain.length(XDIR) * maxDomain.length(YDIR);
  if(MaxPictureSize() / dataSize == 0) {
    maxAllowableScale = 1;
  } else {
    maxAllowableScale = (int) sqrt((Real) (MaxPictureSize()/dataSize));
  }
  currentScale = Max(1, Min(GetInitialScale(), maxAllowableScale));

  amrPicturePtrArray[ZPLANE] = new AmrPicture(minAllowableLevel, GAptr,
                                              this, dataServicesPtr);
#if (BL_SPACEDIM == 3)
    amrPicturePtrArray[YPLANE] = new AmrPicture(YPLANE, minAllowableLevel, GAptr,
    			amrData.ProbDomain()[finestLevel],
			amrPicturePtrArray[ZPLANE], this);
    amrPicturePtrArray[XPLANE] = new AmrPicture(XPLANE, minAllowableLevel, GAptr,
    			amrData.ProbDomain()[finestLevel],
			amrPicturePtrArray[ZPLANE], this);
#endif
    for(i = 0; i < BL_SPACEDIM; i++) {
    ivLowOffsetMAL.setVal(i, amrData.ProbDomain()[maxlev].smallEnd(i));
  }
  palFilename = PltApp::defaultPaletteString;
  PltAppInit();
}


// -------------------------------------------------------------------
PltApp::PltApp(XtAppContext app, Widget w, const Box &region,
	       const IntVect &offset, AmrPicture *parentPtr,
	       PltApp *pltParent, const aString &palfile,
	       bool isAnim, const aString &newderived, const aString &file)
{
  dataServicesPtr = pltParent->dataServicesPtr;
  anim = isAnim;
  paletteDrawn = false;
  appContext = app;
  wTopLevel = w; 
  fileName = file;
  palFilename = palfile;
  char header[BUFSIZ];

  SetDefaultShowBoxes(pltParent->GetAmrPicturePtr(0)->ShowingBoxes());

  if(anim) {
    animFrames = GetFileCount(); 
    fileNames.resize(animFrames);
  } else {
    animFrames = 1;
    fileNames.resize(1);
    currentFrame = 0;
    fileNames[currentFrame] = fileName;
  }

  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  int finestLevel(amrData.FinestLevel());
  int maxlev = DetermineMaxAllowableLevel(region, finestLevel,
				    MaxPictureSize(), amrData.RefRatio());
  minAllowableLevel = amrData.FinestContainingLevel(region, finestLevel);
  Box maxDomain = region;
  if(maxlev < finestLevel) {
    maxDomain.coarsen(CRRBetweenLevels(maxlev, finestLevel,amrData.RefRatio()));
  }
  unsigned long dataSize = maxDomain.length(XDIR) * maxDomain.length(YDIR);
  if(MaxPictureSize() / dataSize == 0) {
    maxAllowableScale = 1;
  } else {
    maxAllowableScale = (int) sqrt((Real) (MaxPictureSize()/dataSize));
  }
  currentScale = Min(maxAllowableScale, pltParent->CurrentScale());

  char shortfilename[BUFSIZ];
  int fnl = fileName.length() - 1;
  while (fnl>-1 && fileName[fnl] != '/') {
    fnl--;
  }
  strcpy(shortfilename, &fileName[fnl+1]);

  ostrstream headerout(header, BUFSIZ);
  headerout << shortfilename << "  Subregion:  " << maxDomain
	    << "  on level " << maxlev << ends;
				
  wAmrVisTopLevel = XtVaCreatePopupShell(header, 
			topLevelShellWidgetClass, wTopLevel,
			XmNwidth,	initialWindowWidth,
			XmNheight,	initialWindowHeight,
		 	XmNx,		120+placementOffsetX,
			XmNy,		220+placementOffsetY,
		  	NULL);

  GAptr = new GraphicsAttributes(wAmrVisTopLevel);

  int np;
  for(np = 0; np < NPLANES; np++) {
    amrPicturePtrArray[np] = new AmrPicture(np, minAllowableLevel, GAptr, region,
					    parentPtr, this);
  }

  ivLowOffsetMAL = offset;
  currentDerived = newderived;
  PltAppInit();
}


// -------------------------------------------------------------------
void PltApp::PltAppInit() {

  placementOffsetX += 4;
  placementOffsetY += 8;

  int i, np;
  servingButton = 0;
  XYyz = 0;
  XYxz = 0;
  XZxy = 0;
  XZyz = 0;
  YZxz = 0;
  YZxy = 0;

  activeView = ZPLANE;

  const AmrData &amrData = dataServicesPtr->AmrDataRef();

  int maxAllowLev = amrPicturePtrArray[ZPLANE]->MaxAllowableLevel();
  maxDrawnLevel = maxAllowLev;
  minDrawnLevel = minAllowableLevel;
  maxDataLevel = amrData.FinestLevel();
  minDataLevel = minAllowableLevel;
  startX = 0;
  startY = 0;
  endX   = 0;
  endY   = 0;

  selectionBox.convert(amrData.ProbDomain()[0].type());
  selectionBox.setSmall(IntVect::TheZeroVector());
  selectionBox.setBig(IntVect::TheZeroVector());

  for(np = 0; np < NPLANES; np++) {
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

  setRangeShowing = false;
  datasetShowing = false;
  
  wControlArea = XtVaCreateManagedWidget("controlarea", xmFormWidgetClass,
		        wAmrVisTopLevel,
		        NULL);

  wFrames[MENUFRAME] = XtVaCreateManagedWidget("menuframe", xmFrameWidgetClass,
		      wControlArea,
		      XmNtopAttachment,   XmATTACH_FORM,
		      XmNleftAttachment,  XmATTACH_FORM,
		      XmNrightAttachment, XmATTACH_FORM,
		      XmNheight,	  40,
		      XmNshadowType,      XmSHADOW_ETCHED_IN,
		      NULL);
  wAmrVisMenu = XtVaCreateManagedWidget("amrvismenu", xmFormWidgetClass,
		wFrames[MENUFRAME],
		NULL);

// ****************************************** Scale Menu
  unsigned long scale;
  char scaleItem[LINELENGTH];
  XmStringTable scaleStringList;
  int maxallow = Min(MAXSCALE, maxAllowableScale);
  scaleStringList = (XmStringTable) XtMalloc(maxallow * sizeof(XmString *));
  wScaleItems.resize(maxallow);

  i=0;
  XtSetArg(args[i], XmNborderWidth, 0);      i++;
  XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNbottomOffset, WOFFSET);      i++;
  wScaleOptions = XmCreatePulldownMenu(wAmrVisMenu, "scaleoptions", args, i);
  XtVaSetValues(wScaleOptions,
		XmNuserData, this,
		NULL);

  for(scale = 0; scale < maxallow; scale++) {
    sprintf(scaleItem, "Scale %ix", scale + 1);
    scaleStringList[scale] = XmStringCreateSimple(scaleItem);
    i=0;
    XtSetArg(args[i], XmNlabelString, scaleStringList[scale]);  i++;
    wScaleItems[scale] = XmCreatePushButtonGadget(wScaleOptions,
			 scaleItem, args, i);
    XtAddCallback(wScaleItems[scale], XmNactivateCallback,
		  &PltApp::CBChangeScale, (XtPointer)scale);
  }

  XtManageChildren(wScaleItems.dataPtr(), maxallow);

  i=0;
  XtSetArg(args[i], XmNsubMenuId, wScaleOptions);            i++;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);        i++;
  XtSetArg(args[i], XmNtopOffset, 2);           i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);       i++;
  XtSetArg(args[i], XmNleftOffset, 0);          i++;
  XtSetArg(args[i], XmNborderWidth, 0);      i++;
  XtSetArg(args[i], XmNmenuHistory, wScaleItems[currentScale - 1]);  i++;
  wScaleMenu = XmCreateOptionMenu(wAmrVisMenu, "ScaleMenu", args, i);

  XmString newOString = XmStringCreateSimple(""); // set this to "" for cray
  XtVaSetValues(XmOptionLabelGadget(wScaleMenu),
                XmNlabelString, newOString,
                NULL);
  XmStringFree(newOString);

  
// ****************************************** Level Menu
  char levelItem[LINELENGTH];
  XmStringTable levelStringList;
  int numberOfLevels = amrPicturePtrArray[ZPLANE]->NumberOfLevels() - minDrawnLevel;
  levelStringList = (XmStringTable) XtMalloc(numberOfLevels * sizeof(XmString *));
  wLevelItems.resize(numberOfLevels);

  i=0;
  wLevelOptions = XmCreatePulldownMenu(wAmrVisMenu, "leveloptions", args, i);
  XtVaSetValues(wLevelOptions, XmNuserData, this, NULL);

  for(int menuLevel = minDrawnLevel;
      menuLevel < amrPicturePtrArray[ZPLANE]->NumberOfLevels(); ++menuLevel)
  {
    sprintf(levelItem, "Level %i/%i", menuLevel, amrData.FinestLevel());
    levelStringList[menuLevel - minDrawnLevel] = XmStringCreateSimple(levelItem);
    i=0;
    XtSetArg(args[i], XmNlabelString, levelStringList[menuLevel - minDrawnLevel]);
    i++;
    wLevelItems[menuLevel - minDrawnLevel] = XmCreatePushButtonGadget(wLevelOptions,
			 levelItem, args, i);
    XtAddCallback(wLevelItems[menuLevel - minDrawnLevel], XmNactivateCallback,
		  &PltApp::CBChangeLevel, (XtPointer)(menuLevel - minDrawnLevel));
  }
  XtManageChildren(wLevelItems.dataPtr(), numberOfLevels);


  i=0;
  XtSetArg(args[i], XmNsubMenuId, wLevelOptions); i++;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNtopOffset, 2);      i++;
  XtSetArg(args[i], XmNleftWidget, wScaleMenu);      i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);      i++;
  XtSetArg(args[i], XmNleftOffset, 0);      i++;
  XtSetArg(args[i], XmNmenuHistory, wLevelItems[maxAllowLev - minDrawnLevel]);  i++;
  wLevelMenu = XmCreateOptionMenu(wAmrVisMenu, "LevelMenu", args, i);

  // set this string to "" for cray
  XmString newLString = XmStringCreateSimple("");
  XtVaSetValues(XmOptionLabelGadget(wLevelMenu),
                XmNlabelString, newLString,
                NULL);
  XmStringFree(newLString);


// ****************************************** Derive Menu

  int derived;
  numberOfDerived = dataServicesPtr->NumDeriveFunc();
  derivedStrings  = dataServicesPtr->PlotVarNames();

  XmStringTable derivedStringList;
  derivedStringList = (XmStringTable)
			XtMalloc(numberOfDerived * sizeof (XmString *));

  i=0;
  wDerivedOptions = XmCreatePulldownMenu(wAmrVisMenu, "derivedoptions", args, i);
  XtVaSetValues(wDerivedOptions, XmNuserData, this, NULL);

  wDerivedItems.resize(numberOfDerived);
  int cderived = 0;
  for(derived=0; derived<numberOfDerived; derived++) {
    if(derivedStrings[derived] == currentDerived) {
      cderived = derived;
    }
    char tempDerived[256];
    strcpy(tempDerived, derivedStrings[derived].c_str());
    derivedStringList[derived] = XmStringCreateSimple(tempDerived);
    i=0;
    XtSetArg(args[i], XmNlabelString, derivedStringList[derived]);  i++;
    wDerivedItems[derived] = XmCreatePushButtonGadget(wDerivedOptions,
			                  tempDerived, args, i);
    XtAddCallback(wDerivedItems[derived], XmNactivateCallback,
		      &PltApp::CBChangeDerived, (XtPointer) derived);
  }
  XtManageChildren(wDerivedItems.dataPtr(), numberOfDerived);

  i=0;
  XtSetArg(args[i], XmNsubMenuId, wDerivedOptions); i++;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNtopOffset, 2);      i++;
  XtSetArg(args[i], XmNleftWidget, wLevelMenu);      i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);      i++;
  XtSetArg(args[i], XmNleftOffset, 0);      i++;
  XtSetArg(args[i], XmNmenuHistory, wDerivedItems[cderived]);  i++;
  wDerivedMenu = XmCreateOptionMenu(wAmrVisMenu, "DerivedMenu", args, i);

  // set this string to "" for cray
  XmString newDString = XmStringCreateSimple("");
  XtVaSetValues(XmOptionLabelGadget(wDerivedMenu),
                XmNlabelString, newDString,
                NULL);
  XmStringFree(newDString);

// ****************************************** Dataset Button 
  i=0;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNtopOffset, WOFFSET);      i++;
  XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNbottomOffset, WOFFSET);      i++;
  XtSetArg(args[i], XmNleftWidget, wDerivedMenu);      i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);      i++;
  XtSetArg(args[i], XmNleftOffset, WOFFSET);      i++;
  XmString sDataset = XmStringCreateSimple("Dataset");
  XtSetArg(args[i], XmNlabelString, sDataset);    i++;
  wDatasetButton = XmCreatePushButton(wAmrVisMenu, "dataset", args, i);
  XmStringFree(sDataset);

  AddStaticCallback(wDatasetButton, XmNactivateCallback,
		      &PltApp::DoDatasetButton);

// ****************************************** Subregioning Button 
  i=0;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNtopOffset, WOFFSET);      i++;
  XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNbottomOffset, WOFFSET);      i++;
  XtSetArg(args[i], XmNleftWidget, wDatasetButton);      i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);      i++;
  XtSetArg(args[i], XmNleftOffset, WOFFSET);      i++;
  XmString sSubregion;
# if (BL_SPACEDIM == 3)
    sSubregion = XmStringCreateSimple("SubVolume");
# else
    sSubregion = XmStringCreateSimple("SubRegion");
# endif
  XtSetArg(args[i], XmNlabelString, sSubregion);       i++;
  wSubregionButton = XmCreatePushButton(wAmrVisMenu, "Subregion", args, i);
  XmStringFree(sSubregion);

  AddStaticCallback(wSubregionButton, XmNactivateCallback, &PltApp::DoSubregion);

// ****************************************** Palette Button 
  i=0;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNtopOffset, WOFFSET);      i++;
  XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNbottomOffset, WOFFSET);      i++;
  XtSetArg(args[i], XmNleftWidget, wSubregionButton);      i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);      i++;
  XtSetArg(args[i], XmNleftOffset, WOFFSET);      i++;
  XmString sPalette = XmStringCreateSimple("Palette");
  XtSetArg(args[i], XmNlabelString, sPalette);    i++;
  wPaletteButton = XmCreatePushButton(wAmrVisMenu, "palette", args, i);
  XmStringFree(sPalette);

  AddStaticCallback(wPaletteButton, XmNactivateCallback,
                    &PltApp::DoPaletteButton);

// ****************************************** Boxes Button 
  i=0;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNtopOffset, WOFFSET);      i++;
  XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNbottomOffset, WOFFSET);      i++;
  XtSetArg(args[i], XmNleftWidget, wPaletteButton);      i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);      i++;
  XtSetArg(args[i], XmNleftOffset, WOFFSET);      i++;
  XmString sBoxes = XmStringCreateSimple("Boxes");
  XtSetArg(args[i], XmNlabelString, sBoxes);      i++;
  wBoxesButton = XmCreateToggleButton(wAmrVisMenu, "boxes", args, i);
  XmStringFree(sBoxes);
  AddStaticCallback(wBoxesButton, XmNvalueChangedCallback, &PltApp::DoBoxesButton);
  XmToggleButtonSetState(wBoxesButton, GetDefaultShowBoxes(), false);
  

// ****************************************** Output Menu 
  i=0;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNtopOffset, WOFFSET);      i++;
  XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNbottomOffset, WOFFSET);      i++;
  XtSetArg(args[i], XmNleftWidget, wBoxesButton);      i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);      i++;
  XtSetArg(args[i], XmNleftOffset, WOFFSET);      i++;
  wOutputMenuBar = XmCreateMenuBar(wAmrVisMenu, "outputmenubar", args, i);

  i=0;
  wOutputMenu = XmCreatePulldownMenu(wOutputMenuBar, "outputmenu", args, i);

  XmString sOutput = XmStringCreateSimple("Output");
  wOutputButton = XtVaCreateManagedWidget("outputbutton",
				  xmCascadeButtonWidgetClass, wOutputMenuBar,
				  XmNlabelString, sOutput,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, wBoxesButton,
				  XmNleftOffset, WOFFSET,
				  XmNsubMenuId, wOutputMenu,
				  NULL);
  XmStringFree(sOutput);

  i=0;
  wPSFileButton = XtCreateManagedWidget("PS File", xmPushButtonGadgetClass,
					wOutputMenu, args, i);
  AddStaticCallback(wPSFileButton, XmNactivateCallback, &PltApp::DoOutput);

  i=0;
  wRGBFileButton = XtCreateManagedWidget("RGB File", xmPushButtonGadgetClass,
					wOutputMenu, args, i);
  AddStaticCallback(wRGBFileButton, XmNactivateCallback, &PltApp::DoOutput);

  i=0;
  wFABFileButton = XtCreateManagedWidget("FAB File", xmPushButtonGadgetClass,
					wOutputMenu, args, i);
  AddStaticCallback(wFABFileButton, XmNactivateCallback, &PltApp::DoOutput);

  XtManageChild(wOutputMenuBar);


// ****************************************** wPicArea

  wPicArea = XtVaCreateWidget("picarea", xmFormWidgetClass, wControlArea,
		XmNtopAttachment,	XmATTACH_WIDGET,
		//XmNtopWidget,		wAmrVisMenu,
		XmNtopWidget,		wFrames[MENUFRAME],
		XmNleftAttachment,	XmATTACH_FORM,
		XmNrightAttachment,	XmATTACH_FORM,
		XmNbottomAttachment,	XmATTACH_FORM,
		NULL);			

  wFrames[PALETTEFRAME] = XtVaCreateManagedWidget("paletteframe",
			     xmFrameWidgetClass, wPicArea,
		             XmNrightAttachment, XmATTACH_FORM,
		             XmNrightOffset,     1,
		             XmNshadowType,      XmSHADOW_ETCHED_IN,
		             NULL);
  wPalArea = XtVaCreateWidget("palarea",xmDrawingAreaWidgetClass,
		wFrames[PALETTEFRAME],
		XmNx,			690,
		XmNy,			0,
		XmNwidth,		150,
		XmNheight,		320,
		NULL);

// ****************************************** Close Button 
  i=0;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNtopOffset, WOFFSET);         i++;
  XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);   i++;
  XtSetArg(args[i], XmNbottomOffset, WOFFSET);      i++;
  XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);    i++;
  XtSetArg(args[i], XmNrightOffset, 16);      i++;
  XmString sQuit = XmStringCreateSimple("Close");
  XtSetArg(args[i], XmNlabelString, sQuit);       i++;
  wQuitButton = XmCreatePushButton(wAmrVisMenu, "quit", args, i);
  XmStringFree(sQuit);
  XtAddCallback(wQuitButton, XmNactivateCallback, CBQuitPltApp, 
		(XtPointer) this);

// ****************************************** SetRange Button 
  wFrames[SETRANGEFRAME] = XtVaCreateManagedWidget("setrangeframe",
			     xmFrameWidgetClass, wPicArea,
		             XmNrightAttachment, XmATTACH_FORM,
		             XmNrightOffset,     1,
		             XmNtopAttachment,   XmATTACH_WIDGET,
		             XmNtopWidget,       wFrames[PALETTEFRAME],
		             XmNtopOffset,       0,
		             XmNshadowType,      XmSHADOW_ETCHED_IN,
		             NULL);
  i=0;
  XtSetArg(args[i], XmNwidth, 150);                          i++;
  XmString sSetRange = XmStringCreateSimple("Set Range");
  XtSetArg(args[i], XmNlabelString, sSetRange);              i++;
  wSetRangeButton = XmCreatePushButton(wFrames[SETRANGEFRAME], "setrange",
				       args, i);
  XmStringFree(sSetRange);

  AddStaticCallback(wSetRangeButton, XmNactivateCallback,
		&PltApp::DoSetRangeButton);

  currentFrame = 0;
  animating = 0;
  frameSpeed = 1;

  readyFrames.resize(animFrames);
  frameBuffer.resize(animFrames);
  for(i = 0; i < animFrames; i++) {
    readyFrames[i] = false;
  }
  if(anim) {
    for(i = 0; i < animFrames; i++) {
      fileNames[i] = GetComlineFilename(i); 
    }
  }


  wFrames[PLOTFRAME] = XtVaCreateManagedWidget("plotframe", xmFrameWidgetClass,
		         wPicArea,
		         XmNrightAttachment,	XmATTACH_WIDGET,
		         XmNrightWidget,	wFrames[SETRANGEFRAME],
		         XmNleftAttachment,	XmATTACH_FORM,
		         XmNbottomAttachment,	XmATTACH_FORM,
		         XmNtopAttachment,	XmATTACH_FORM,
		         XmNshadowType,		XmSHADOW_ETCHED_IN,
		         NULL);
  wPlotArea = XtVaCreateWidget("plotarea", xmFormWidgetClass,
		               wFrames[PLOTFRAME],
		               NULL);
// ******************************************************* XY

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
	          XmNwidth,         amrPicturePtrArray[ZPLANE]->ImageSizeH() + 1,
	          XmNheight,        amrPicturePtrArray[ZPLANE]->ImageSizeV() + 1,
	          NULL);
  AddStaticCallback(wPlotPlane[ZPLANE], XmNinputCallback,
		    &PltApp::DoRubberBanding);
  XtVaSetValues(wScrollArea[ZPLANE], XmNworkWindow, wPlotPlane[ZPLANE], NULL); 

#if (BL_SPACEDIM == 3)
    XtVaSetValues(wAmrVisTopLevel, XmNwidth, 1100, XmNheight, 650, NULL);

    XtVaSetValues(wScrollArea[ZPLANE], 
			XmNrightAttachment,	XmATTACH_POSITION,
			XmNrightPosition,	50,	// %
			XmNbottomAttachment,	XmATTACH_POSITION,
			XmNbottomPosition,	50,
			XmNleftAttachment,	XmATTACH_FORM,
			XmNleftOffset,		0,
			XmNtopAttachment,	XmATTACH_FORM,
			XmNtopOffset,		0,
			NULL);

// ********************************************************* XZ

    wScrollArea[YPLANE] = XtVaCreateManagedWidget("scrollAreaXZ",
		xmScrolledWindowWidgetClass, wPlotArea,
		XmNrightAttachment,	XmATTACH_FORM,
    		XmNleftAttachment,	XmATTACH_WIDGET,
		XmNleftWidget,		wScrollArea[ZPLANE],
	        XmNbottomAttachment,	XmATTACH_POSITION,
	        XmNbottomPosition,	50,
		XmNtopAttachment,	XmATTACH_FORM,
		XmNtopOffset,		0,
		XmNscrollingPolicy,	XmAUTOMATIC,
		NULL);

    wPlotPlane[YPLANE] = XtVaCreateManagedWidget("plotArea",
		xmDrawingAreaWidgetClass, wScrollArea[YPLANE],
		XmNtranslations, XtParseTranslationTable(trans),
		XmNwidth,        amrPicturePtrArray[YPLANE]->ImageSizeH() + 1,
		XmNheight,	 amrPicturePtrArray[YPLANE]->ImageSizeV() + 1,
		NULL);
    AddStaticCallback(wPlotPlane[YPLANE], XmNinputCallback,
		      &PltApp::DoRubberBanding);
    XtVaSetValues(wScrollArea[YPLANE], XmNworkWindow, wPlotPlane[YPLANE], NULL); 

// ********************************************************* YZ

    wScrollArea[XPLANE] = XtVaCreateManagedWidget("scrollAreaYZ",
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
		XmNwidth,         amrPicturePtrArray[XPLANE]->ImageSizeH() + 1,
		XmNheight,        amrPicturePtrArray[XPLANE]->ImageSizeV() + 1,
		NULL);
    AddStaticCallback(wPlotPlane[XPLANE], XmNinputCallback,
		      &PltApp::DoRubberBanding);
    XtVaSetValues(wScrollArea[XPLANE], XmNworkWindow, wPlotPlane[XPLANE], NULL); 

  // ****************************************** Transform Area & buttons

    i=0;
    XmString sOrient = XmStringCreateSimple("0");
    XtSetArg(args[i], XmNlabelString, sOrient); i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET); i++;
    XtSetArg(args[i], XmNleftWidget, wScrollArea[XPLANE]); i++;
    XtSetArg(args[i], XmNleftOffset, WOFFSET); i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_POSITION); i++;
    XtSetArg(args[i], XmNtopPosition, 50); i++;
    wOrient = XmCreatePushButton(wPlotArea, "orient", args, i);
    XmStringFree(sOrient);

    AddStaticCallback(wOrient, XmNactivateCallback, &PltApp::DoOrient);
    XtManageChild(wOrient);

    i=0;
    XmString sLabel = XmStringCreateSimple("XYZ");
    XtSetArg(args[i], XmNlabelString, sLabel); i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET); i++;
    XtSetArg(args[i], XmNleftWidget, wOrient); i++;
    XtSetArg(args[i], XmNleftOffset, WOFFSET); i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_POSITION); i++;
    XtSetArg(args[i], XmNtopPosition, 50); i++;
    wLabelAxes = XmCreatePushButton(wPlotArea, "label", args, i);
    XmStringFree(sLabel);
    AddStaticCallback(wLabelAxes, XmNactivateCallback, &PltApp::DoLabelAxes);
    XtManageChild(wLabelAxes);

#ifdef BL_VOLUMERENDER
    i=0;
    XmString sRender = XmStringCreateSimple("Draw");
    XtSetArg(args[i], XmNlabelString, sRender); i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET); i++;
    XtSetArg(args[i], XmNleftWidget, wLabelAxes); i++;
    XtSetArg(args[i], XmNleftOffset, WOFFSET); i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_POSITION); i++;
    XtSetArg(args[i], XmNtopPosition, 50); i++;
    wRender = XmCreatePushButton(wPlotArea, "render", args, i);
    XmStringFree(sRender);

    AddStaticCallback(wRender, XmNactivateCallback, &PltApp::DoRender);
    XtManageChild(wRender);

    i=0;
    XmString sAutoDraw;
    sAutoDraw = XmStringCreateSimple("Autodraw");
    XtSetArg(args[i], XmNlabelString, sAutoDraw); i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET); i++;
    XtSetArg(args[i], XmNleftWidget, wRender); i++;
    XtSetArg(args[i], XmNleftOffset, WOFFSET); i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_POSITION); i++;
    XtSetArg(args[i], XmNtopPosition, 50); i++;
    wAutoDraw = XmCreateToggleButton(wPlotArea, "autodraw", args, i);
    XmStringFree(sAutoDraw);
    AddStaticCallback(wAutoDraw, XmNvalueChangedCallback, &PltApp::DoAutoDraw);
    XtManageChild(wAutoDraw);

    i=0;
    XmString sReadTrans = XmStringCreateSimple("Trans");
    XtSetArg(args[i], XmNlabelString, sReadTrans); i++;
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET); i++;
    XtSetArg(args[i], XmNleftWidget, wAutoDraw); i++;
    XtSetArg(args[i], XmNleftOffset, WOFFSET); i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_POSITION); i++;
    XtSetArg(args[i], XmNtopPosition, 50); i++;
    wReadTransfer = XmCreatePushButton(wPlotArea, "readtrans", args, i);
    XmStringFree(sReadTrans);
    AddStaticCallback(wReadTransfer, XmNactivateCallback,
		 &PltApp::DoReadTransferFile);
    XtManageChild(wReadTransfer);
#endif

    i=0;
    XmString sDetach = XmStringCreateSimple("Detach");
    XtSetArg(args[i], XmNlabelString, sDetach); i++;
    XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM); i++;
    XtSetArg(args[i], XmNrightOffset, WOFFSET); i++;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_POSITION); i++;
    XtSetArg(args[i], XmNtopPosition, 50); i++;
    wDetach = XmCreatePushButton(wPlotArea, "detach", args, i);
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
    XtAddCallback(wTransDA, XmNinputCallback, &PltApp::CBTransInput,
		(XtPointer) this);

#endif
  // *************************************** Axis and animation control area


    int wc, wcfWidth = 150, wcfHeight = 260;
    int centerX = wcfWidth/2, centerY = wcfHeight / 2 - 16;
  wFrames[CONTROLSFRAME] = XtVaCreateManagedWidget("controlsframe",
			     xmFrameWidgetClass, 	wPicArea,
		             XmNrightAttachment,	XmATTACH_FORM,
		             XmNrightOffset, 		1,
		             XmNtopAttachment,		XmATTACH_WIDGET,
		             XmNtopWidget,		wFrames[SETRANGEFRAME],
		             XmNshadowType, 		XmSHADOW_ETCHED_IN,
		             NULL);
    wControlForm = XtVaCreateManagedWidget("refArea", xmDrawingAreaWidgetClass,
		wFrames[CONTROLSFRAME],
		XmNwidth,	wcfWidth,
		XmNheight,	wcfHeight,
		NULL);

    int controlSize = 16;
    int halfbutton = controlSize/2;

#if (BL_SPACEDIM == 3)

    i=0;
    XmString sControls0 = XmStringCreateSimple("0");
    XtSetArg(args[i], XmNlabelString, sControls0); i++;
    XtSetArg(args[i], XmNx, centerX-halfbutton); i++;
    XtSetArg(args[i], XmNy, centerY-halfbutton); i++;
    XtSetArg(args[i], XmCMarginBottom, 2); i++;
    wControls[WCSTOP] = XmCreatePushButton(wControlForm, "wcstop", args, i);
    XmStringFree(sControls0);

    XtManageChild(wControls[WCSTOP]);

    wControls[WCXNEG] = XtVaCreateManagedWidget("wcxneg",
		   xmArrowButtonWidgetClass, wControlForm,
		   XmNarrowDirection,      XmARROW_LEFT,
		   XmNx, centerX+halfbutton,
		   XmNy, centerY-halfbutton,
		   NULL);
    wControls[WCXPOS] = XtVaCreateManagedWidget("wcxpos",
		   xmArrowButtonWidgetClass, wControlForm,
		   XmNarrowDirection,      XmARROW_RIGHT,
		   XmNx, centerX+3*halfbutton,
		   XmNy, centerY-halfbutton,
		   NULL);
    wControls[WCYNEG] = XtVaCreateManagedWidget("wcyneg",
		   xmArrowButtonWidgetClass, wControlForm,
		   XmNarrowDirection,      XmARROW_DOWN,
		   XmNx, centerX-halfbutton,
		   XmNy, centerY-3*halfbutton,
		   NULL);
    wControls[WCYPOS] = XtVaCreateManagedWidget("wcypos",
		   xmArrowButtonWidgetClass, wControlForm,
		   XmNarrowDirection,      XmARROW_UP,
		   XmNx, centerX-halfbutton,
		   XmNy, centerY-5*halfbutton,
		   NULL);
    wControls[WCZNEG] = XtVaCreateManagedWidget("wczneg",
		   xmArrowButtonWidgetClass, wControlForm,
		   XmNarrowDirection,      XmARROW_RIGHT,
		   XmNx, centerX-3*halfbutton+1,
		   XmNy, centerY+halfbutton-1,
		   NULL);
    wControls[WCZPOS] = XtVaCreateManagedWidget("wczpos",
		   xmArrowButtonWidgetClass, wControlForm,
		   XmNarrowDirection,      XmARROW_LEFT,
		   XmNx, centerX-5*halfbutton+2,
		   XmNy, centerY+3*halfbutton-2,
		   NULL);

    for(wc=WCSTOP; wc<=WCZPOS; wc++) {
      XtVaSetValues(wControls[wc],
		    XmNwidth, controlSize,
		    XmNheight, controlSize,
		    XmNborderWidth, 0,
		    XmNhighlightThickness, 0,
		    NULL); 
    }
#endif

    int adjustHeight2D;
    if(BL_SPACEDIM == 2) {
      adjustHeight2D = centerY;
    } else {
      adjustHeight2D = 0;
    }
    if(anim) {
      i=0;
      XmString sControlswcastop = XmStringCreateSimple("0");
      XtSetArg(args[i], XmNlabelString, sControlswcastop); i++;
      XtSetArg(args[i], XmNx, centerX-halfbutton); i++;
      XtSetArg(args[i], XmCMarginBottom, 2); i++;
      wControls[WCASTOP] = XmCreatePushButton(wControlForm, "wcastop", args, i);
      XmStringFree(sControlswcastop);
      XtManageChild(wControls[WCASTOP]);

      wControls[WCATNEG] = XtVaCreateManagedWidget("wcatneg",
		     xmArrowButtonWidgetClass, wControlForm,
		     XmNarrowDirection,      XmARROW_LEFT,
		     XmNx, centerX-3*halfbutton,
		     NULL);
      wControls[WCATPOS] = XtVaCreateManagedWidget("wcatpos",
		     xmArrowButtonWidgetClass, wControlForm,
		     XmNarrowDirection,      XmARROW_RIGHT,
		     XmNx, centerX+halfbutton,
		     NULL);
      for(wc=WCATNEG; wc<=WCATPOS; wc++) {
        XtVaSetValues(wControls[wc],
		      XmNwidth,  controlSize,
		      XmNheight, controlSize,
		      XmNborderWidth, 0,
		      XmNhighlightThickness, 0,
		      XmNy, wcfHeight-2*halfbutton-adjustHeight2D,
		      NULL); 
      }
    }  // end if(anim)

#if (BL_SPACEDIM == 3)

    for(wc=WCSTOP; wc<=WCZPOS; wc++) {
      XtAddCallback(wControls[wc], XmNactivateCallback,
		  &PltApp::CBChangePlane, (XtPointer) this);
    }

#endif

    Dimension slw, ssw;
     if(anim || BL_SPACEDIM == 3) {

    wLabelFast = XtVaCreateManagedWidget("Speed", xmLabelWidgetClass,
	wControlForm,	
        XmNy, wcfHeight-9*halfbutton-adjustHeight2D,
	NULL);
    XtVaGetValues(wLabelFast, XmNwidth, &slw, NULL);
    XtVaSetValues(wLabelFast, XmNx, wcfWidth-slw, NULL);

    XtVaGetValues(wLabelFast, XmNwidth, &ssw, NULL);
    wSpeedScale = XtVaCreateManagedWidget("speed", xmScaleWidgetClass,
	              wControlForm,
	              XmNminimum,		0, 
	              XmNmaximum,		599, 
	              XmNvalue,		500,
	              XmNorientation,		XmHORIZONTAL,
                      XmNx, -5,
                      XmNy, wcfHeight - 9 * halfbutton - adjustHeight2D,
	              NULL);
      XtAddCallback(wSpeedScale, XmNvalueChangedCallback,
    	  &PltApp::CBSpeedScale, (XtPointer) this);

     }  // end if(anim || BL_SPACEDIM == 3)

     if(anim) {
      wWhichFileScale = XtVaCreateManagedWidget("whichFileScale",
	                    xmScaleWidgetClass, wControlForm,
	                    XmNminimum,		0,
	                    XmNmaximum,		animFrames-1,
	                    XmNvalue,		0,
	                    XmNorientation,		XmHORIZONTAL,
                            XmNx, -5,
                            XmNy, wcfHeight - 7 * halfbutton - adjustHeight2D,
	                    NULL);

      AddStaticCallback(wWhichFileScale, XmNvalueChangedCallback,
		        &PltApp::DoAnimFileScale);

      wAnimLabelFast = XtVaCreateManagedWidget("File", xmLabelWidgetClass,
	                   wControlForm,	
                           XmNy, wcfHeight - 7 * halfbutton - adjustHeight2D,
	                   NULL);
      XtVaGetValues(wAnimLabelFast, XmNwidth, &slw, NULL);
      XtVaSetValues(wAnimLabelFast, XmNx, wcfWidth-slw, NULL);

      char tempFileName[1024];
      strcpy(tempFileName, fileName.c_str());
      XmString fileString = XmStringCreateSimple(tempFileName);
      wWhichFileLabel = XtVaCreateManagedWidget("whichFileLabel",
	                    xmLabelWidgetClass, wControlForm,	
	                    XmNx, 0,
	                    XmNy, wcfHeight - 4 * halfbutton - 3 - adjustHeight2D,
	                    XmNlabelString,         fileString,
	                    NULL);
      XmStringFree(fileString);

      for(wc=WCATNEG; wc<=WCATPOS; wc++) {
        XtAddCallback(wControls[wc], XmNactivateCallback,
		      &PltApp::CBChangePlane, (XtPointer) this);
      }

   }  // end if(anim)

#if (BL_SPACEDIM == 2)
    XtVaSetValues(wScrollArea[ZPLANE], 
		XmNrightAttachment,	XmATTACH_FORM,
		XmNbottomAttachment,	XmATTACH_FORM,
		XmNbottomOffset,	WOFFSET,
		NULL);
#endif


// ***************************************************************** 

  XtManageChild(wScaleMenu);
  XtManageChild(wLevelMenu);
  XtManageChild(wDerivedMenu);
  XtManageChild(wQuitButton);
  XtManageChild(wSubregionButton);
  XtManageChild(wDatasetButton);
  XtManageChild(wPaletteButton);
  XtManageChild(wSetRangeButton);
  XtManageChild(wBoxesButton);
  XtManageChild(wPicArea);
  XtManageChild(wPalArea);
  XtManageChild(wPlotArea);
  XtPopup(wAmrVisTopLevel, XtGrabNone);

  int palListLength = PALLISTLENGTH;
  int palWidth = PALWIDTH;
  int palHeight = PALHEIGHT;
  int totalPalWidth = TOTALPALWIDTH;
  int totalPalHeight = TOTALPALHEIGHT;

  pltPaletteptr = new Palette(wTopLevel, palListLength, palWidth, palHeight,
			      totalPalWidth, totalPalHeight, 
			      reserveSystemColors);

  pltPaletteptr->SetWindow(XtWindow(wPalArea));

  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPlotArea));
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wAmrVisTopLevel));
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPalArea));
  for(np = 0; np < NPLANES; np++) {
    pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPlotPlane[np]));
  }
#if (BL_SPACEDIM == 3)
    pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wTransDA));
#endif

  if(anim) {
  ParallelDescriptor::Abort("PltApp::anim not yet supported.");
  /*
    if(strcmp(derivedStrings[cderived].c_str(), "vol_frac") == 0) {
      globalMin = 0.0;
      globalMax = 1.0;
    } else {
      aString outbuf = "Finding global min & max values for ";
      outbuf += derivedStrings[cderived];
      outbuf += "...\n";
      strcpy(buffer, outbuf.c_str());
      PrintMessage(buffer);

      AmrPlot *temp;
      int lev;
      Real dataMin, dataMax;
      globalMin =  AV_BIG_REAL;
      globalMax = -AV_BIG_REAL;
      for(i=0; i<animFrames; i++) {
        temp = new AmrPlot;
        FileType fileType = GetDefaultFileType();
        assert(fileType != INVALIDTYPE);
        if(fileType == FAB || fileType == MULTIFAB) {
          temp->ReadNonPlotFile(GetComlineFilename(i), fileType);
        } else {
          temp->ReadPlotFile(GetComlineFilename(i));
        }
	int coarseLevel = 0;
	int fineLevel   = amrPicturePtrArray[ZPLANE]->NumberOfLevels();
        for(lev = coarseLevel; lev < fineLevel; lev++) {
          temp->MinMax(amrData.ProbDomain()[lev], derivedStrings[cderived],
		       lev, dataMin, dataMax);
          globalMin = Min(globalMin, dataMin);
          globalMax = Max(globalMax, dataMax);
	}
	delete temp;
      }
    } 
  */
  }
#if (BL_SPACEDIM == 3)
    Dimension width, height;
    XtVaGetValues(wTransDA, XmNwidth, &width, XmNheight, &height, NULL);
    daWidth  = width;
    daHeight = height;
    projPicturePtr = new ProjectionPicture(this, &viewTrans,
				   wTransDA, daWidth, daHeight);
#endif
    for(np = 0; np < NPLANES; np++) {
        amrPicturePtrArray[np]->CreatePicture(XtWindow(wPlotPlane[np]),
                                              pltPaletteptr, derivedStrings[cderived]);
  }
  FileType fileType = dataServicesPtr->GetFileType();
  assert(fileType != INVALIDTYPE);
  if(fileType == FAB  || fileType == MULTIFAB) {
    currentDerived = amrPicturePtrArray[ZPLANE]->CurrentDerived();
  }
#if (BL_SPACEDIM == 3)
    viewTrans.MakeTransform();
    XmToggleButtonSetState(wAutoDraw, false, false);
    labelAxes = false;
    transDetached = false;

    for(np = 0; np < NPLANES; np++) {
      startcutX[np] = 0;
      startcutY[np] = amrPicturePtrArray[np]->GetHLine();
      finishcutX[np] = 0;
      finishcutY[np] = amrPicturePtrArray[np]->GetHLine();
    }
#endif
  for(np = 0; np < NPLANES; np++) {
    XtAddEventHandler(wPlotPlane[np], ExposureMask, false, 
  	  (XtEventHandler) CBDoExposePicture,
	  (XtPointer) amrPicturePtrArray[np]);
  }
#if (BL_SPACEDIM == 3)
    XtAddEventHandler(wControlForm, ExposureMask, false,
  	              (XtEventHandler) CBDoExposeRef, (XtPointer) this);
    XtAddEventHandler(wTransDA, ExposureMask, false,
  	              (XtEventHandler) CBExposeTransDA, (XtPointer) this);
    XtAddCallback(wTransDA, XmNresizeCallback, &PltApp::CBTransResize,
		(XtPointer) this);
    DoTransResize(wTransDA, NULL, NULL);
#endif

  XtAddEventHandler(wPalArea, ExposureMask, false,
	(XtEventHandler) CBDoExposePalette, (XtPointer) pltPaletteptr);

  XSetWindowColormap(GAptr->PDisplay(), XtWindow(wAmrVisTopLevel),
        pltPaletteptr->GetColormap());

  interfaceReady = true;

    // gc for gxxor rubber band line drawing
    rbgc = XCreateGC(GAptr->PDisplay(), GAptr->PRoot(), 0, NULL);
    cursor = XCreateFontCursor(GAptr->PDisplay(), XC_left_ptr);
    XSetFunction(GAptr->PDisplay(), rbgc, GXxor);
#if (BL_SPACEDIM == 3)
  for(np = 0; np < NPLANES; np++) {
    amrPicturePtrArray[np]->ToggleShowSubCut(); 
  }
  projPicturePtr->ToggleShowSubCut(); 
  projPicturePtr->MakeBoxes(); 
#endif

  for(np = 0; np < NPLANES; np++) {
    amrPicturePtrArray[np]->SetDataMin(amrPicturePtrArray[np]->GetRegionMin());
    amrPicturePtrArray[np]->SetDataMax(amrPicturePtrArray[np]->GetRegionMax());
  }  

}	// end of PltAppInit



// -------------------------------------------------------------------
void PltApp::DoExposeRef() {
	int zPlanePosX = 10, zPlanePosY = 15;
	int whiteColor = pltPaletteptr->WhiteIndex();
	int zpColor = whiteColor;
  	char sX[LINELENGTH], sY[LINELENGTH], sZ[LINELENGTH];

        XClearWindow(GAptr->PDisplay(), XtWindow(wControlForm));

  	strcpy(sX, "X");
  	strcpy(sY, "Y");
  	strcpy(sZ, "Z");

	DrawAxes(wControlForm, zPlanePosX, zPlanePosY, 0, sX, sY, zpColor);
#     if (BL_SPACEDIM == 3)
	int axisLength = 20;
	int ypColor = whiteColor, xpColor = whiteColor;
	int xyzAxisLength = 50;
	int stringOffsetX = 4, stringOffsetY = 20;
	int yPlanePosX = 80, yPlanePosY = 15;
	int xPlanePosX = 10;
	int xPlanePosY = (int) axisLength+zPlanePosY+1.4*stringOffsetY;
        char temp[LINELENGTH];
	Dimension width, height;
        XtVaGetValues(wControlForm, XmNwidth, &width, XmNheight, &height, NULL);
	int centerX = (int) width/2;
	int centerY = (int) height/2-16;

	DrawAxes(wControlForm, yPlanePosX, yPlanePosY, 0, sX, sZ, ypColor);
	DrawAxes(wControlForm, xPlanePosX, xPlanePosY, 0, sY, sZ, xpColor);

	sprintf(temp, "Z=%i", (amrPicturePtrArray[YPLANE]->
		ImageSizeV()-1 - amrPicturePtrArray[YPLANE]->GetHLine())/
		currentScale + ivLowOffsetMAL[ZDIR]);
        XSetForeground(GAptr->PDisplay(), GAptr->PGC(), zpColor);
	XDrawString(GAptr->PDisplay(), XtWindow(wControlForm), GAptr->PGC(),
		    centerX-xyzAxisLength+12,
		    centerY+xyzAxisLength+4,
		    temp, strlen(temp));

	sprintf(temp, "Y=%i", amrPicturePtrArray[XPLANE]->GetVLine()/
		currentScale + ivLowOffsetMAL[YDIR]);
        XSetForeground(GAptr->PDisplay(), GAptr->PGC(), ypColor);
	XDrawString(GAptr->PDisplay(), XtWindow(wControlForm), GAptr->PGC(),
		    centerX+stringOffsetX,
		    centerY-xyzAxisLength+4,
		    temp, strlen(temp));

	sprintf(temp, "X=%i", amrPicturePtrArray[ZPLANE]->GetVLine()/
		currentScale + ivLowOffsetMAL[XDIR]);
        XSetForeground(GAptr->PDisplay(), GAptr->PGC(), xpColor);
	XDrawString(GAptr->PDisplay(), XtWindow(wControlForm), GAptr->PGC(),
		    centerX+4*stringOffsetX,
		    centerY+stringOffsetY+2,
		    temp, strlen(temp));

        XSetForeground(XtDisplay(wControlForm), GAptr->PGC(), ypColor);
	XDrawLine(GAptr->PDisplay(), XtWindow(wControlForm), GAptr->PGC(),
		  centerX, centerY, centerX, centerY-xyzAxisLength);
	XDrawLine(GAptr->PDisplay(), XtWindow(wControlForm), GAptr->PGC(),
		  centerX, centerY, centerX+xyzAxisLength, centerY);
	XDrawLine(GAptr->PDisplay(), XtWindow(wControlForm), GAptr->PGC(),
		  centerX, centerY,
		  (int) centerX-0.9*xyzAxisLength,
		  (int) centerY+0.9*xyzAxisLength);
#     endif
}


// -------------------------------------------------------------------
void PltApp::DrawAxes(Widget wArea, int xpos, int ypos, int /* orientation */ ,
			char *hlabel, char *vlabel, int color)
{
  int axisLength=20;
  char hLabel[LINELENGTH], vLabel[LINELENGTH];
  strcpy(hLabel, hlabel);
  strcpy(vLabel, vlabel);
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
void PltApp::CBChangeScale(Widget w, XtPointer client_data, XtPointer call_data) {
  int np;
  unsigned long getobj;
  XtVaGetValues(XtParent(w), XmNuserData, &getobj, NULL);
  PltApp *obj = (PltApp *) getobj;
  unsigned long newScale = (unsigned long) client_data + 1;
  if(obj->currentScale != newScale) {
    obj->previousScale = obj->currentScale;
    obj->currentScale  = newScale;
    for(np = 0; np < NPLANES; np++) {
      obj->DoChangeScale(np);
    }
  }
}


// -------------------------------------------------------------------
void PltApp::CBChangeLevel(Widget w, XtPointer client_data,
				XtPointer call_data)
{
  unsigned long getobj;
  XtVaGetValues(XtParent(w), XmNuserData, &getobj, NULL);
  PltApp *obj = (PltApp *) getobj;
  for(int np = 0; np < NPLANES; ++np) {
    obj->DoChangeLevel(np, w, client_data, call_data);
  }
}


// -------------------------------------------------------------------
void PltApp::CBChangeDerived(Widget w, XtPointer client_data,
				XtPointer call_data)
{
  unsigned long getobj;
  XtVaGetValues(XtParent(w), XmNuserData, &getobj, NULL);
  PltApp *obj = (PltApp *) getobj;
#if (BL_SPACEDIM == 3)
  // ------------------------- swf
  obj->projPicturePtr->GetVolRenderPtr()->InvalidateSWFData();
  obj->projPicturePtr->GetVolRenderPtr()->InvalidateVPData();
#ifdef BL_VOLUMERENDER
  XmString sAutoDraw = XmStringCreateSimple("Autodraw");
  if(obj->transDetached) {
    XtVaSetValues(obj->wDAutoDraw, XmNlabelString, sAutoDraw, NULL);
  } else {
    XtVaSetValues(obj->wAutoDraw, XmNlabelString, sAutoDraw, NULL);
  }
  XmStringFree(sAutoDraw);
#endif
  // ------------------------- end swf
  obj->Clear();
#endif
  obj->paletteDrawn = false;
  for(int np = 0; np < NPLANES; ++np) {
    obj->DoChangeDerived(np, w, client_data, call_data);
  }
}


// -------------------------------------------------------------------
void PltApp::AddStaticCallback(Widget w, String cbtype, memberCB cbf) {
  CBData *cbsp = new CBData(this, cbf);
  XtAddCallback(w, cbtype, &PltApp::StaticCallback, (XtPointer) cbsp);
}

 
// -------------------------------------------------------------------
void PltApp::StaticCallback(Widget w, XtPointer client_data, XtPointer call_data) {
  CBData *cbs = (CBData *) client_data;
  PltApp *obj = cbs->instance;
  (obj->*(cbs->cbFunc))(w, client_data, call_data);
}


// -------------------------------------------------------------------
void PltApp::CBToggleRange(Widget w, XtPointer client_data, XtPointer call_data) {
  unsigned long getobj;
  unsigned long r = (unsigned long) client_data;
  Range range = (Range) r;
  XtVaGetValues(XtParent(w), XmNuserData, &getobj, NULL);
  PltApp *obj = (PltApp *) getobj;
  obj->DoToggleRange(w, range, (XmToggleButtonCallbackStruct *) call_data);
}


// -------------------------------------------------------------------
void PltApp::DoSubregion(Widget, XtPointer, XtPointer) {
  Box subregionBox;
  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  int finestLevel(amrData.FinestLevel());
  int maxAllowableLevel(amrPicturePtrArray[ZPLANE]->MaxAllowableLevel());
  int newMinAllowableLevel;

  // subregionBox is at the maxAllowableLevel

#if (BL_SPACEDIM == 2)
  if(selectionBox.bigEnd(XDIR) != 0 && selectionBox.bigEnd(YDIR) != 0) {
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

  if(subregionBox.numPts() > 4) {
    subregionBox += ivLowOffsetMAL;   // shift to true data region
#endif

    Box tempRefinedBox = subregionBox;
    tempRefinedBox.refine(CRRBetweenLevels(maxAllowableLevel, finestLevel,
		           amrData.RefRatio()));
    // this puts tempRefinedBox in terms of the finest level
    newMinAllowableLevel = amrData.FinestContainingLevel(
                                     tempRefinedBox, finestLevel);
    newMinAllowableLevel = Min(newMinAllowableLevel, maxAllowableLevel);

    // coarsen to the newMinAllowableLevel to align grids
    subregionBox.coarsen(CRRBetweenLevels(newMinAllowableLevel,
			 maxAllowableLevel, amrData.RefRatio()));

    Box subregionBoxMAL = subregionBox;

    // refine to the finestLevel
    subregionBox.refine(CRRBetweenLevels(newMinAllowableLevel, finestLevel,
		        amrData.RefRatio()));

    maxAllowableLevel = DetermineMaxAllowableLevel(subregionBox, finestLevel,
			       MaxPictureSize(), amrData.RefRatio());
    subregionBoxMAL.refine(CRRBetweenLevels(newMinAllowableLevel,
			   maxAllowableLevel, amrData.RefRatio()));

    IntVect ivOffset = subregionBoxMAL.smallEnd();

    SubregionPltApp(wTopLevel, subregionBox, ivOffset, amrPicturePtrArray[ZPLANE],
		    this, palFilename, anim, currentDerived, fileName);
  }

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
void PltApp::DoChangeScale(int V) {
  if(anim) {
    ResetAnimation();
    DirtyFrames(); 
  }
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


// -------------------------------------------------------------------
void PltApp::DoChangeLevel(int V, Widget, XtPointer client_data, XtPointer) {
  unsigned long newLevel((unsigned long) client_data);

  if(anim) {
    ResetAnimation();
    DirtyFrames(); 
  }

  minDrawnLevel = minAllowableLevel;
  maxDrawnLevel = newLevel + minDrawnLevel;

  amrPicturePtrArray[V]->ChangeLevel(minDrawnLevel, maxDrawnLevel);

  if(V == ZPLANE) {
    if(! XmToggleButtonGetState(wAutoDraw)) {
      projPicturePtr->MakeBoxes(); 
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
      DoExposeTransDA();
    }
  }
}


// -------------------------------------------------------------------
void PltApp::DoChangeDerived(int V, Widget, XtPointer client_data, XtPointer) {
  int np;
  unsigned long derivedNumber = (unsigned long) client_data;
  if(derivedStrings[derivedNumber] != amrPicturePtrArray[V]->CurrentDerived()) {
    // reset the level menu to the max drawn level
    maxDrawnLevel = amrPicturePtrArray[ZPLANE]->MaxAllowableLevel();
    XtVaSetValues(wLevelMenu,
                  XmNmenuHistory, wLevelItems[maxDrawnLevel - minDrawnLevel],
                  NULL);

    for(np = 0; np < NPLANES; np++) {
      amrPicturePtrArray[np]->SetMaxDrawnLevel(maxDrawnLevel);
    }

    currentDerived = derivedStrings[derivedNumber];

    if(anim) {
      ResetAnimation();
      DirtyFrames();
    }
    if(anim) {
    /*
      if(rangeType==USEFILE) {
        AmrPlot *temp;
        int lev;
        Real dataMin, dataMax;

        globalMin =  AV_BIG_REAL;
        globalMax = -AV_BIG_REAL;
	i = currentFrame;
          temp = new AmrPlot;
          FileType fileType = GetDefaultFileType();
          assert(fileType != INVALIDTYPE);
          if(fileType == BOXDATA || fileType == BOXCHAR  || fileType == FAB) {
            temp->ReadNonPlotFile(GetComlineFilename(i), fileType);
          } else {
            temp->ReadPlotFile(GetComlineFilename(i));
          }
          int coarseLevel = 0;
          int fineLevel   = maxDrawnLevel;
          for(lev = coarseLevel; lev <= fineLevel; lev++) {
            temp->MinMax(amrData.ProbDomain()[lev],
                         derivedStrings[derivedNumber], lev, dataMin, dataMax);
            globalMin = Min(globalMin, dataMin);
            globalMax = Max(globalMax, dataMax);
          }
          delete temp;
      } else if(strcmp(derivedStrings[derivedNumber].c_str(),"vol_frac") == 0) {
        globalMin = 0.0;
        globalMax = 1.0;
      } else {
        aString outbuf = "Finding global min & max values for ";
        outbuf += derivedStrings[derivedNumber];
        outbuf += "...\n";
        strcpy(buffer, outbuf.c_str());
        PrintMessage(buffer);

        AmrPlot *temp;
        int lev;
        Real dataMin, dataMax;

        globalMin =  AV_BIG_REAL;
        globalMax = -AV_BIG_REAL;
        for(i = 0; i < animFrames; i++) {
          temp = new AmrPlot;
          FileType fileType = GetDefaultFileType();
          assert(fileType != INVALIDTYPE);
          if(fileType == BOXDATA || fileType == BOXCHAR  || fileType == FAB) {
            temp->ReadNonPlotFile(GetComlineFilename(i), fileType);
          } else {
            temp->ReadPlotFile(GetComlineFilename(i));
          }
          int coarseLevel = 0;
          int fineLevel   = maxDrawnLevel;
          for(lev = coarseLevel; lev <= fineLevel; lev++) {
            temp->MinMax(amrData.ProbDomain()[lev],
                         derivedStrings[derivedNumber], lev, dataMin, dataMax);
            globalMin = Min(globalMin, dataMin);
            globalMax = Max(globalMax, dataMax);
          }
          delete temp;
        }
      } 
    */
    }
    amrPicturePtrArray[V]->ChangeDerived(derivedStrings[derivedNumber],
                                         pltPaletteptr);
  }

  if(datasetShowing) {
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
    datasetPtr->Render(trueRegion, amrPicturePtrArray[V], this, hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  }

  if(UseSpecifiedMinMax()) {
    Real specifiedMin, specifiedMax;
    GetSpecifiedMinMax(specifiedMin, specifiedMax);
    amrPicturePtrArray[V]->SetDataMin(specifiedMin);
    amrPicturePtrArray[V]->SetDataMax(specifiedMax);
  } else {
    amrPicturePtrArray[V]->SetDataMin(amrPicturePtrArray[V]->GetRegionMin());
    amrPicturePtrArray[V]->SetDataMax(amrPicturePtrArray[V]->GetRegionMax());
  }

}  // end DoChangeDerived


// -------------------------------------------------------------------
void PltApp::DoDatasetButton(Widget, XtPointer, XtPointer) {
  if(selectionBox.bigEnd(XDIR) != 0 && selectionBox.bigEnd(YDIR) != 0) {
	int hdir, vdir, sdir;

	trueRegion = selectionBox;
	
#       if (BL_SPACEDIM == 3)
    	  trueRegion.setSmall(ZDIR, amrPicturePtrArray[ZPLANE]->GetSlice()); 
    	  trueRegion.setBig(ZDIR,   amrPicturePtrArray[ZPLANE]->GetSlice()); 
#       endif

	if(activeView==ZPLANE) {         // orient box to view and shift
	  trueRegion.shift(XDIR, ivLowOffsetMAL[XDIR]);
	  trueRegion.shift(YDIR, ivLowOffsetMAL[YDIR]);
	  hdir = XDIR;
	  vdir = YDIR;
	  sdir = ZDIR;
	}
	if(activeView==YPLANE) {
    	  trueRegion.setSmall(ZDIR, trueRegion.smallEnd(YDIR)); 
    	  trueRegion.setBig(ZDIR, trueRegion.bigEnd(YDIR)); 
    	  trueRegion.setSmall(YDIR, amrPicturePtrArray[YPLANE]->GetSlice()); 
    	  trueRegion.setBig(YDIR, amrPicturePtrArray[YPLANE]->GetSlice()); 
	  trueRegion.shift(XDIR, ivLowOffsetMAL[XDIR]);
	  trueRegion.shift(ZDIR, ivLowOffsetMAL[ZDIR]);
	  hdir = XDIR;
	  vdir = ZDIR;
	  sdir = YDIR;
	}
	if(activeView==XPLANE) {
    	  trueRegion.setSmall(ZDIR, trueRegion.smallEnd(YDIR)); 
    	  trueRegion.setBig(ZDIR, trueRegion.bigEnd(YDIR)); 
    	  trueRegion.setSmall(YDIR, trueRegion.smallEnd(XDIR)); 
    	  trueRegion.setBig(YDIR, trueRegion.bigEnd(XDIR)); 
    	  trueRegion.setSmall(XDIR, amrPicturePtrArray[XPLANE]->GetSlice()); 
    	  trueRegion.setBig(XDIR, amrPicturePtrArray[XPLANE]->GetSlice()); 
	  trueRegion.shift(YDIR, ivLowOffsetMAL[YDIR]);
	  trueRegion.shift(ZDIR, ivLowOffsetMAL[ZDIR]);
	  hdir = YDIR;
	  vdir = ZDIR;
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
void PltApp::DoSetRangeButton(Widget, XtPointer, XtPointer) {
  int i;
  Position xpos, ypos;
  Dimension width, height;

/*
  if(anim) {
    ResetAnimation();
    DirtyFrames(); 
  }
*/

  if(setRangeShowing) {
	XtPopup(wSetRangeTopLevel, XtGrabNone);
	XMapRaised(XtDisplay(wSetRangeTopLevel), XtWindow(wSetRangeTopLevel));
  } else {
	char range[LINELENGTH];
	char format[LINELENGTH];
	char fMin[LINELENGTH];
	char fMax[LINELENGTH];
	strcpy(format, formatString.c_str());

	// do these here to set XmNwidth of wSetRangeTopLevel
	sprintf(fMin, format, amrPicturePtrArray[ZPLANE]->GetMin());
	sprintf(fMax, format, amrPicturePtrArray[ZPLANE]->GetMax());
	sprintf (range, "Min: %s  Max: %s", fMin, fMax);


	XtVaGetValues(wAmrVisTopLevel, XmNx, &xpos, XmNy, &ypos,
		      XmNwidth, &width, XmNheight, &height, NULL);

	wSetRangeTopLevel = XtVaCreatePopupShell("Set Range",
			topLevelShellWidgetClass, wAmrVisTopLevel,
			XmNwidth,		12*(strlen(range)+12),
			XmNheight,		180,
			XmNx,			xpos+width/2,
			XmNy,			ypos,
			NULL);

	wSetRangeForm = XtVaCreateManagedWidget("setrangeform",
			    xmFormWidgetClass, wSetRangeTopLevel,
			    NULL);

	// make the buttons
	i=0;
	XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);   i++;
	XtSetArg(args[i], XmNbottomOffset, WOFFSET);    i++;
	XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);      i++;
	XtSetArg(args[i], XmNleftOffset, WOFFSET);      i++;
	XmString sDoneSet = XmStringCreateSimple(" Ok ");
	XtSetArg(args[i], XmNlabelString, sDoneSet);    i++;
	wDoneButton = XmCreatePushButton(wSetRangeForm, "doneset", args, i);
	XmStringFree(sDoneSet);
	AddStaticCallback(wDoneButton, XmNactivateCallback,
		&PltApp::DoDoneSetRange);

	i=0;
	XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);   i++;
	XtSetArg(args[i], XmNbottomOffset, WOFFSET);    i++;
	XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);      i++;
	XtSetArg(args[i], XmNrightOffset, WOFFSET);      i++;
	XmString sCancelSet = XmStringCreateSimple("Cancel");
	XtSetArg(args[i], XmNlabelString, sCancelSet);    i++;
	wCancelButton = XmCreatePushButton(wSetRangeForm, "cancelset", args, i);
	XmStringFree(sCancelSet);

	AddStaticCallback(wCancelButton, XmNactivateCallback,
		&PltApp::DoCancelSetRange);

	// make the radio box
	wSetRangeRadioBox = XmCreateRadioBox(wSetRangeForm,
			 "setrangeradiobox", NULL, 0);

	wRangeRadioButton[USEGLOBAL] = XtVaCreateManagedWidget("Global",
					  xmToggleButtonWidgetClass,
					  wSetRangeRadioBox, NULL);
	XtAddCallback(wRangeRadioButton[USEGLOBAL], XmNvalueChangedCallback,
		      CBToggleRange, (XtPointer)USEGLOBAL);
	wRangeRadioButton[USELOCAL] = XtVaCreateManagedWidget("Local",
			  xmToggleButtonWidgetClass, wSetRangeRadioBox, NULL);
	XtAddCallback(wRangeRadioButton[USELOCAL], XmNvalueChangedCallback,
		      CBToggleRange, (XtPointer)USELOCAL);
	wRangeRadioButton[USESPECIFIED] = XtVaCreateManagedWidget("User",
			  xmToggleButtonWidgetClass, wSetRangeRadioBox, NULL);
	XtAddCallback(wRangeRadioButton[USESPECIFIED], XmNvalueChangedCallback,
		      CBToggleRange, (XtPointer)USESPECIFIED);
	if(anim) {
	  wRangeRadioButton[USEFILE] = XtVaCreateManagedWidget("File",
					  xmToggleButtonWidgetClass,
					  wSetRangeRadioBox, NULL);
	  XtAddCallback(wRangeRadioButton[USEFILE], XmNvalueChangedCallback,
		        CBToggleRange, (XtPointer)USEFILE);
	}
	
	XtVaSetValues(wSetRangeRadioBox, XmNuserData, this, NULL);

	rangeType = amrPicturePtrArray[ZPLANE]->GetWhichRange();
	XtVaSetValues(wRangeRadioButton[rangeType], XmNset, true, NULL);


	// make the strings representing data min and maxes
	sprintf(fMin, format, amrPicturePtrArray[ZPLANE]->GetMin());
	sprintf(fMax, format, amrPicturePtrArray[ZPLANE]->GetMax());
	sprintf(range, "Min: %s  Max: %s", fMin, fMax);
	wGlobalRange = XtVaCreateManagedWidget(range,
		xmLabelGadgetClass,	wSetRangeForm,
		XmNtopAttachment,	XmATTACH_FORM,
		XmNtopOffset,		12,
		XmNleftWidget,		wSetRangeRadioBox,
		XmNleftAttachment,	XmATTACH_WIDGET,
		XmNleftOffset,		WOFFSET,
		NULL);
	sprintf(fMin, format, amrPicturePtrArray[ZPLANE]->GetRegionMin());
	sprintf(fMax, format, amrPicturePtrArray[ZPLANE]->GetRegionMax());
	sprintf(range, "Min: %s  Max: %s", fMin, fMax);
	wLocalRange = XtVaCreateManagedWidget(range,
		xmLabelGadgetClass,	wSetRangeForm,
		XmNtopWidget,		wGlobalRange,
		XmNtopAttachment,	XmATTACH_WIDGET,
		XmNtopOffset,		10,
		XmNleftWidget,		wSetRangeRadioBox,
		XmNleftAttachment,	XmATTACH_WIDGET,
		XmNleftOffset,		WOFFSET,
		NULL);
	sprintf(range, "Min:");
	wMin = XtVaCreateManagedWidget(range, xmLabelGadgetClass, wSetRangeForm,
		XmNtopWidget,		wLocalRange,
		XmNtopAttachment,	XmATTACH_WIDGET,
		XmNtopOffset,		10,
		XmNleftWidget,		wSetRangeRadioBox,
		XmNleftAttachment,	XmATTACH_WIDGET,
		XmNleftOffset,		WOFFSET,
		NULL);
	sprintf(range, format, amrPicturePtrArray[ZPLANE]->GetSpecifiedMin());
	wUserMin = XtVaCreateManagedWidget("local range",
		xmTextFieldWidgetClass, wSetRangeForm,
		XmNvalue,		range,
		XmNtopWidget,		wLocalRange,
		XmNtopAttachment,	XmATTACH_WIDGET,
		XmNtopOffset,		3,
		XmNleftWidget,		wMin,
		XmNleftAttachment,	XmATTACH_WIDGET,
		XmNleftOffset,		WOFFSET,
		XmNeditable,		true,
		XmNcolumns,		strlen(range)+2,
		NULL);
	AddStaticCallback(wUserMin, XmNactivateCallback, &PltApp::DoUserMin);

	sprintf(range, "Max:");
	wMax = XtVaCreateManagedWidget(range, xmLabelGadgetClass, wSetRangeForm,
		XmNtopWidget,		wLocalRange,
		XmNtopAttachment,	XmATTACH_WIDGET,
		XmNtopOffset,		10,
		XmNleftWidget,		wUserMin,
		XmNleftAttachment,	XmATTACH_WIDGET,
		XmNleftOffset,		WOFFSET,
		NULL);
	sprintf(range, format, amrPicturePtrArray[ZPLANE]->GetSpecifiedMax());
	wUserMax = XtVaCreateManagedWidget("local range",
		xmTextFieldWidgetClass, wSetRangeForm,
		XmNvalue,		range,
		XmNtopWidget,		wLocalRange,
		XmNtopAttachment,	XmATTACH_WIDGET,
		XmNtopOffset,		3,
		XmNleftWidget,		wMax,
		XmNleftAttachment,	XmATTACH_WIDGET,
		XmNleftOffset,		WOFFSET,
		XmNeditable,		true,
		XmNcolumns,		strlen(range)+2,
		NULL);

	AddStaticCallback(wUserMax, XmNactivateCallback, &PltApp::DoUserMax);

	XtManageChild(wSetRangeRadioBox);
	XtManageChild(wDoneButton);
	XtManageChild(wCancelButton);
	XtPopup(wSetRangeTopLevel, XtGrabNone);
	setRangeShowing = true;
  }
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
    if(anim) {
      ResetAnimation();
      DirtyFrames(); 
      ShowFrame();
    }
  }
  XtDestroyWidget(wSetRangeTopLevel);
  setRangeShowing = false;

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
  setRangeShowing = false;
}


// -------------------------------------------------------------------
void PltApp::DoToggleRange(Widget, Range which, XmToggleButtonCallbackStruct *state)
{
  if(state->set == true) {
    rangeType = which;
  }
}


// -------------------------------------------------------------------
void PltApp::DoUserMin(Widget, XtPointer, XtPointer) {
  if(rangeType != USESPECIFIED) {
    XtVaSetValues(wRangeRadioButton[rangeType], XmNset, false, NULL);
  }
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
  if(anim) {
    ResetAnimation();
    DirtyFrames(); 
  }
  amrPicturePtrArray[ZPLANE]->ToggleBoxes();
# if (BL_SPACEDIM == 3)
    projPicturePtr->MakeBoxes(); 
    if(! XmToggleButtonGetState(wAutoDraw)) {
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
      DoExposeTransDA();
    }
    amrPicturePtrArray[YPLANE]->ToggleBoxes();
    amrPicturePtrArray[XPLANE]->ToggleBoxes();
# endif

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
  palFilename = palfile;
}


// -------------------------------------------------------------------
void PltApp::DoRubberBanding(Widget w, XtPointer, XtPointer call_data) {
  int np, V, scale, imageHeight, imageWidth, loX, loY, hiX, hiY;
  int maxAllowableLevel(amrPicturePtrArray[ZPLANE]->MaxAllowableLevel());
  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  int finestLevel(amrData.FinestLevel());
  int dataAtClickThreshold(0);  // how close to previous point
  Box subdomain(amrPicturePtrArray[ZPLANE]->GetSubDomain()[maxAllowableLevel]);
  Display *display = GAptr->PDisplay();
#if (BL_SPACEDIM == 3)
  int x1, y1, z1, x2, y2, z2, rStartPlane;
#endif

  // find which view from the widget w
  XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct *) call_data;
  if(w == wPlotPlane[ZPLANE]) {
    V = ZPLANE;
  } else if(w == wPlotPlane[YPLANE]) {
    V = YPLANE;
  } else if(w == wPlotPlane[XPLANE]) {
    V = XPLANE;
  } else {
    cerr << "Error in DoRubberBanding:  bad plot area widget" << endl;
    return;
  }

  imageHeight = amrPicturePtrArray[V]->ImageSizeV() - 1;
  imageWidth  = amrPicturePtrArray[V]->ImageSizeH() - 1;
  scale       = currentScale;

  if(anim) {
    ResetAnimation();
  }
  if(cbs->event->xany.type == ButtonPress) {
    servingButton = cbs->event->xbutton.button;
  }

  XSetForeground(display, rbgc, 120);

  if(servingButton == 1 && cbs->event->xany.type == ButtonPress) {
      anchorX = Max(0, Min(imageWidth,   cbs->event->xbutton.x));
      anchorY = Max(0, Min(imageHeight, cbs->event->xbutton.y));
      oldX = anchorX;
      oldY = anchorY;
      rectDrawn = false;
      // grab server and draw box(es)
      XChangeActivePointerGrab(display, PointerMotionHintMask |
			       ButtonMotionMask | ButtonReleaseMask |
			       OwnerGrabButtonMask, cursor, CurrentTime);
      XGrabServer(display);
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
#           if (BL_SPACEDIM == 3)
                // 3D sub domain selecting
	        if(V==ZPLANE) {
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
		}
	        if(V==YPLANE) {
                  XDrawRectangle(display,
			         amrPicturePtrArray[ZPLANE]->PictureWindow(),
		                 rbgc, rStartX, startcutY[ZPLANE], rWidth,
			         abs(finishcutY[ZPLANE]-startcutY[ZPLANE]));
                  XDrawRectangle(display,
			     amrPicturePtrArray[XPLANE]->PictureWindow(),
		             rbgc, startcutX[XPLANE], rStartY,
			     abs(finishcutX[XPLANE]-startcutX[XPLANE]), rHeight);
		}
	        if(V==XPLANE) {
                  rStartPlane = (anchorX < oldX) ? oldX : anchorX;
                  XDrawRectangle(display,
			     amrPicturePtrArray[ZPLANE]->PictureWindow(),
		             rbgc, startcutX[ZPLANE], imageWidth-rStartPlane,
			     abs(finishcutX[ZPLANE]-startcutX[ZPLANE]), rWidth);
                  XDrawRectangle(display,
			     amrPicturePtrArray[YPLANE]->PictureWindow(),
		             rbgc, startcutX[YPLANE], rStartY,
			     abs(finishcutX[YPLANE]-startcutX[YPLANE]), rHeight);
		}
#           endif
	    }

	    // get rid of those pesky extra MotionNotify events
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

#           if (BL_SPACEDIM == 3)
                // 3D sub domain selecting
	        startcutX[V]  = rStartX;
	        startcutY[V]  = rStartY;
	        finishcutX[V] = rStartX + rWidth;
	        finishcutY[V] = rStartY + rHeight;
	        if(V==ZPLANE) {
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
	        }
	        if(V==YPLANE) {
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
	        }
	        if(V==XPLANE) {
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

                if( ! XmToggleButtonGetState(wAutoDraw)) {
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
			 GetSubDomain()[maxAllowableLevel].smallEnd(ZDIR);
		  }
		  IntVect ivmin(Min(x1,x2), Min(y1,y2), Min(z1,z2));
		  IntVect ivmax(Max(x1,x2), Max(y1,y2), Max(z1,z2));
	          Box xyz12(ivmin, ivmax);
	          xyz12 &= subdomain;

	          projPicturePtr->SetSubCut(xyz12); 
                  //projPicturePtr->MakeBoxes();
                  XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
    	          DoExposeTransDA();
                }
#           endif

	  break;

	  case ButtonRelease:
            XUngrabServer(display);  // giveitawaynow

	    startX = (Max(0, Min(imageWidth,  anchorX))) / scale;
	    startY = (Max(0, Min(imageHeight, anchorY))) / scale;
	    endX   = (Max(0, Min(imageWidth,  nextEvent.xbutton.x))) / scale;
	    endY   = (Max(0, Min(imageHeight, nextEvent.xbutton.y))) / scale;

	    // convert to amr space
	    loX = Min(startX, endX);
	    loY = ((imageHeight + 1) / scale) - Max(startY, endY) - 1;
	    hiX = Max(startX, endX);
	    hiY = ((imageHeight + 1) / scale)  - Min(startY, endY) - 1;

	    // make "aligned" box with correct size
	    selectionBox.setSmall(XDIR, loX);
	    selectionBox.setSmall(YDIR, loY);
	    selectionBox.setBig(XDIR, hiX);
	    selectionBox.setBig(YDIR, hiY);

	    // selectionBox is now at the maxAllowableLevel because
	    // it is defined on the pixmap ( / scale)

	    if(abs(anchorX - nextEvent.xbutton.x) <= dataAtClickThreshold &&
	       abs(anchorY - nextEvent.xbutton.y) <= dataAtClickThreshold)
	    {
	      // data at click
	      int y, intersectedLevel(-1), mal(maxAllowableLevel);
	      Box temp, intersectedGrid;
	      Array<Box> trueRegion(mal+1);
	      int plane(amrPicturePtrArray[V]->GetSlice());

	      trueRegion[mal] = selectionBox;

	      // convert to point box
	      trueRegion[mal].setBig(XDIR, trueRegion[mal].smallEnd(XDIR));
	      trueRegion[mal].setBig(YDIR, trueRegion[mal].smallEnd(YDIR));

	      if(V==ZPLANE) {
	        trueRegion[mal].shift(XDIR, ivLowOffsetMAL[XDIR]);
	        trueRegion[mal].shift(YDIR, ivLowOffsetMAL[YDIR]);
	        if(BL_SPACEDIM == 3) {
	          trueRegion[mal].setSmall(ZDIR, plane);
	          trueRegion[mal].setBig(ZDIR, plane);
	        }	
	      }	
	      if(V==YPLANE) {
	        trueRegion[mal].setSmall(ZDIR, trueRegion[mal].smallEnd(YDIR));
	        trueRegion[mal].setBig(ZDIR, trueRegion[mal].bigEnd(YDIR));
	        trueRegion[mal].setSmall(YDIR, plane);
	        trueRegion[mal].setBig(YDIR, plane);
	        trueRegion[mal].shift(XDIR, ivLowOffsetMAL[XDIR]);
	        trueRegion[mal].shift(ZDIR, ivLowOffsetMAL[ZDIR]);
              }
              if(V==XPLANE) {
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
	        trueRegion[y].coarsen(CRRBetweenLevels(y, mal,
			              amrData.RefRatio()));
	        trueRegion[y].setBig(XDIR, trueRegion[y].smallEnd(XDIR));
	        trueRegion[y].setBig(YDIR, trueRegion[y].smallEnd(YDIR));
	      }

	      bool goodIntersect;
	      Real dataValue;
	      //goodIntersect = dataServicesPtr->PointValue(trueRegion,
				       //intersectedLevel,
				       //intersectedGrid, dataValue, 0, mal,
				       //currentDerived, formatString);
	      //cerr << endl;
	      //cerr << "Error:  PointValueRequest not yet implemented." << endl;
	      //cerr << endl;
	      //goodIntersect = false;
	      DataServices::Dispatch(DataServices::PointValueRequest,
				     dataServicesPtr,
				     trueRegion.length(), trueRegion.dataPtr(),
				     currentDerived,
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
	        buffout << "value = " << dataValueString << '\n';
	      } else {
		buffout << "Bad point at mouse click" << '\n';
	      }
	      buffout << ends;  // end the string
	      PrintMessage(buffer);


	    } else {    // tell the amrpicture about the box
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

#             if (BL_SPACEDIM == 3)
	        if(V == ZPLANE) {
                  amrPicturePtrArray[V]->SetSubCut(startcutX[V], startcutY[V],
					finishcutX[V], finishcutY[V]);
                  amrPicturePtrArray[YPLANE]->SetSubCut(startcutX[V], -1,
					finishcutX[V], -1);
                  amrPicturePtrArray[XPLANE]->SetSubCut(imageHeight -
			 startcutY[V], -1, imageHeight - finishcutY[V], -1);
	        }
	        if(V == YPLANE) {
                  amrPicturePtrArray[V]->SetSubCut(startcutX[V], startcutY[V],
					finishcutX[V], finishcutY[V]);
                  amrPicturePtrArray[ZPLANE]->SetSubCut(startcutX[V], -1,
					finishcutX[V], -1);
                  amrPicturePtrArray[XPLANE]->SetSubCut(-1, startcutY[V], 
					-1, finishcutY[V]);
	        }
	        if(V == XPLANE) {
                  amrPicturePtrArray[V]->SetSubCut(startcutX[V], startcutY[V],
					finishcutX[V], finishcutY[V]);
                  amrPicturePtrArray[ZPLANE]->SetSubCut(-1, imageWidth -
			 startcutX[V], -1, imageWidth - finishcutX[V]);
                  amrPicturePtrArray[YPLANE]->SetSubCut(-1, startcutY[V], 
					-1, finishcutY[V]);
	        }
#             endif

              for(np = 0; np < NPLANES; np++) {
                amrPicturePtrArray[np]->DoExposePicture();
              }
	    }
	    return;

	  default:
	    ;  // do nothing
	  break;
	}  // end switch
      }  // end while(true)

  } else if(servingButton == 2 && cbs->event->xany.type == ButtonPress) {
      bool lineDrawnH;                     // draw horizontal line in view
      oldX = cbs->event->xbutton.x;
      oldY = cbs->event->xbutton.y;
      XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
		rbgc, 0, oldY, imageWidth, oldY);
      lineDrawnH = true;

      // grab server and draw line(s)
      XChangeActivePointerGrab(display, PointerMotionHintMask |
			       ButtonMotionMask | ButtonReleaseMask |
			       OwnerGrabButtonMask, cursor, CurrentTime);
      XGrabServer(display);
      while(true) {
        XNextEvent(display, &nextEvent);

	switch(nextEvent.type) {

	  case MotionNotify:

	    if(lineDrawnH) {   // undraw the old line
              XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
		        rbgc, 0, oldY, imageWidth, oldY);
#             if (BL_SPACEDIM == 3)
	      if(V==ZPLANE) {  // undraw in other planes
                XDrawLine(display, amrPicturePtrArray[XPLANE]->PictureWindow(),
		          rbgc, imageHeight-oldY, 0, imageHeight-oldY,
			  amrPicturePtrArray[XPLANE]->ImageSizeV() - 1);
	      }
	      if(V==YPLANE) {
                XDrawLine(display, amrPicturePtrArray[XPLANE]->PictureWindow(),
		          rbgc, 0, oldY,
			  amrPicturePtrArray[XPLANE]->ImageSizeH() - 1, oldY);
	      }
	      if(V==XPLANE) {
                XDrawLine(display, amrPicturePtrArray[YPLANE]->PictureWindow(),
		          rbgc, 0, oldY,
			  amrPicturePtrArray[YPLANE]->ImageSizeH() - 1, oldY);
	      }
#             endif
	    }

	    while(XCheckTypedEvent(display, MotionNotify, &nextEvent)) {
	      ;  // do nothing
	    }

	    XQueryPointer(display, amrPicturePtrArray[V]->PictureWindow(),
			  &whichRoot, &whichChild,
			  &rootX, &rootY, &newX, &newY, &inputMask);

	    // draw the new line
            XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
		      rbgc, 0, newY, imageWidth, newY);
#             if (BL_SPACEDIM == 3)
	      if(V==ZPLANE) {
                XDrawLine(display, amrPicturePtrArray[XPLANE]->PictureWindow(),
		          rbgc, imageHeight-newY, 0, imageHeight-newY,
			  amrPicturePtrArray[XPLANE]->ImageSizeV() - 1);
	      }
	      if(V==YPLANE) {
                XDrawLine(display, amrPicturePtrArray[XPLANE]->PictureWindow(),
		          rbgc, 0, newY,
			  amrPicturePtrArray[XPLANE]->ImageSizeH() - 1, newY);
	      }
	      if(V==XPLANE) {
                XDrawLine(display, amrPicturePtrArray[YPLANE]->PictureWindow(),
		          rbgc, 0, newY,
			  amrPicturePtrArray[YPLANE]->ImageSizeH() - 1, newY);
	      }
#             endif
	    lineDrawnH = true;

	    oldX = newX;
	    oldY = newY;

	  break;

	  case ButtonRelease:
            XUngrabServer(display);

	    if(V==ZPLANE) {
	      XYxz = Max(0, Min(imageHeight, nextEvent.xbutton.y));
	      amrPicturePtrArray[V]->SetHLine(XYxz);
#             if (BL_SPACEDIM == 3)
	      YZxz = amrPicturePtrArray[XPLANE]->ImageSizeH()-1 - XYxz; 
	      amrPicturePtrArray[XPLANE]->SetVLine(YZxz);
	      amrPicturePtrArray[YPLANE]->
		  ChangeSlice((imageHeight - XYxz)/scale + ivLowOffsetMAL[YDIR]);
#             endif
	    }
	    if(V==YPLANE) {
	      XZxy = Max(0, Min(imageHeight, nextEvent.xbutton.y));
	      YZxy = XZxy;
	      amrPicturePtrArray[V]->SetHLine(XZxy);
	      amrPicturePtrArray[XPLANE]->SetHLine(YZxy);
	      amrPicturePtrArray[ZPLANE]->
		  ChangeSlice((imageHeight - XZxy)/scale + ivLowOffsetMAL[ZDIR]);
	    }
	    if(V==XPLANE) {
	      YZxy = Max(0, Min(imageHeight, nextEvent.xbutton.y));
	      XZxy = YZxy;
	      amrPicturePtrArray[V]->SetHLine(YZxy);
	      amrPicturePtrArray[YPLANE]->SetHLine(XZxy);
	      amrPicturePtrArray[ZPLANE]->
		  ChangeSlice((imageHeight - YZxy)/scale + ivLowOffsetMAL[ZDIR]);
	    }
            for(np = 0; np < NPLANES; np++) {
              amrPicturePtrArray[np]->DoExposePicture();
            }

	    DoExposeRef();

	    return;
	  //break;

	  default:
	    ;  // do nothing
	  break;
	}  // end switch
      }  // end while(true)

  } else if(servingButton == 3 && cbs->event->xany.type == ButtonPress) {
      bool lineDrawnV;
      oldX = cbs->event->xbutton.x;
      oldY = cbs->event->xbutton.y;
      XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
		rbgc, oldX, 0, oldX, imageHeight);
      lineDrawnV = true;
      // grab server and draw line(s)
      XChangeActivePointerGrab(display, PointerMotionHintMask |
			       ButtonMotionMask | ButtonReleaseMask |
			       OwnerGrabButtonMask, cursor, CurrentTime);
      XGrabServer(display);
      while(true) {
        XNextEvent(display, &nextEvent);

	switch(nextEvent.type) {

	  case MotionNotify:

	    if(lineDrawnV) {   // undraw the old line
              XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
		        rbgc, oldX, 0, oldX, imageHeight);
#             if (BL_SPACEDIM == 3)
	      if(V==ZPLANE) {  // undraw in other planes
                XDrawLine(display, amrPicturePtrArray[YPLANE]->PictureWindow(),
		          rbgc, oldX, 0,
			  oldX, amrPicturePtrArray[YPLANE]->ImageSizeV() - 1);
	      }
	      if(V==YPLANE) {
                XDrawLine(display, amrPicturePtrArray[ZPLANE]->PictureWindow(),
		          rbgc, oldX, 0,
			  oldX, amrPicturePtrArray[ZPLANE]->ImageSizeV() - 1);
	      }
	      if(V==XPLANE) {
                XDrawLine(display, amrPicturePtrArray[ZPLANE]->PictureWindow(),
		          rbgc, 0, imageWidth-oldX,
			  amrPicturePtrArray[ZPLANE]->ImageSizeH() - 1,
			  imageWidth-oldX);
	      }
#             endif
	    }

	    while(XCheckTypedEvent(display, MotionNotify, &nextEvent)) {
	      ;  // do nothing
	    }

	    XQueryPointer(display, amrPicturePtrArray[V]->PictureWindow(),
			  &whichRoot, &whichChild,
			  &rootX, &rootY, &newX, &newY, &inputMask);

	    // draw the new line
            XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
		           rbgc, newX, 0, newX, imageHeight);
#             if (BL_SPACEDIM == 3)
	      if(V==ZPLANE) {  // undraw in other planes
                XDrawLine(display, amrPicturePtrArray[YPLANE]->PictureWindow(),
		          rbgc, newX, 0,
			  newX, amrPicturePtrArray[YPLANE]->ImageSizeV() - 1);
	      }
	      if(V==YPLANE) {
                XDrawLine(display, amrPicturePtrArray[ZPLANE]->PictureWindow(),
		          rbgc, newX, 0,
			  newX, amrPicturePtrArray[ZPLANE]->ImageSizeV() - 1);
	      }
	      if(V==XPLANE) {
                XDrawLine(display, amrPicturePtrArray[ZPLANE]->PictureWindow(),
		          rbgc, 0, imageWidth-newX,
			  amrPicturePtrArray[ZPLANE]->ImageSizeH() - 1,
			  imageWidth-newX);
	      }
#             endif
	    lineDrawnV = true;

	    oldX = newX;
	    oldY = newY;

	  break;

	  case ButtonRelease:
            XUngrabServer(display);

	    if(V==ZPLANE) {
	      XYyz = Max(0, Min(imageWidth, nextEvent.xbutton.x));
	      XZyz = XYyz;
	      amrPicturePtrArray[ZPLANE]->SetVLine(XYyz);
#             if (BL_SPACEDIM == 3)
	      amrPicturePtrArray[YPLANE]->SetVLine(XZyz);
	      amrPicturePtrArray[XPLANE]->
			 ChangeSlice(XYyz/scale + ivLowOffsetMAL[XDIR]);
#             endif
	    }
	    if(V==YPLANE) {
	      XZyz = Max(0, Min(imageWidth, nextEvent.xbutton.x));
	      XYyz = XZyz;
	      amrPicturePtrArray[YPLANE]->SetVLine(XZyz);
	      amrPicturePtrArray[ZPLANE]->SetVLine(XYyz);
	      amrPicturePtrArray[XPLANE]->
			 ChangeSlice(XZyz/scale + ivLowOffsetMAL[XDIR]);
	    }
	    if(V==XPLANE) {
	      YZxz = Max(0, Min(imageWidth, nextEvent.xbutton.x));
	      XYxz = amrPicturePtrArray[ZPLANE]->ImageSizeV()-1 - YZxz;
	      amrPicturePtrArray[XPLANE]->SetVLine(YZxz);
	      amrPicturePtrArray[ZPLANE]->SetHLine(XYxz);
	      amrPicturePtrArray[YPLANE]->
			 ChangeSlice(YZxz/scale + ivLowOffsetMAL[YDIR]);
	    }

            for(np = 0; np < NPLANES; np++) {
              amrPicturePtrArray[np]->DoExposePicture();
            }

	    DoExposeRef();

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
void CBDoExposePalette(Widget, XtPointer client_data, XExposeEvent) {
  ((Palette *) client_data)->ExposePalette();
}


// -------------------------------------------------------------------
void CBDoExposePicture(Widget w, XtPointer client_data, XExposeEvent) {
  AmrPicture *app = (AmrPicture *) client_data;
  Palette *palptr = app->GetPalPtr();
  app->DoExposePicture();

  // draw bounding box
  int isX = app->ImageSizeH();
  int isY = app->ImageSizeV();
  XSetForeground(app->GetGAPtr()->PDisplay(), app->GetGAPtr()->PGC(),
		 palptr->WhiteIndex());
  XDrawLine(app->GetGAPtr()->PDisplay(), XtWindow(w), app->GetGAPtr()->PGC(),
		 0, isY, isX, isY);
  XDrawLine(app->GetGAPtr()->PDisplay(), XtWindow(w), app->GetGAPtr()->PGC(),
		 isX, 0, isX, isY);
}


// -------------------------------------------------------------------
void CBDoExposeRef(Widget, XtPointer client_data, XExposeEvent) {
  ((PltApp *) client_data)->DoExposeRef();
}


// -------------------------------------------------------------------
void PltApp::CBSpeedScale(Widget, XtPointer client_data, XtPointer cbs) {
  PltApp *obj = (PltApp *) client_data;
  obj->DoSpeedScale((XmScaleCallbackStruct *) cbs);
}


// -------------------------------------------------------------------
void PltApp::DoSpeedScale(XmScaleCallbackStruct *cbs) {
  frameSpeed = 600 - cbs->value;
# if(BL_SPACEDIM == 3)
    int v;
    for(v = ZPLANE; v <= XPLANE; v++) {
      amrPicturePtrArray[v]->SetFrameSpeed(600 - cbs->value);
    }
# endif
  XSync(GAptr->PDisplay(), false);
}


// -------------------------------------------------------------------
void PltApp::DoBackStep(int plane) {
  if(plane == XPLANE) {
    if(amrPicturePtrArray[plane]->GetSlice() > amrPicturePtrArray[plane]->
    				GetSubDomain()[amrPicturePtrArray[plane]->
   				MaxAllowableLevel()].smallEnd(XDIR))
    {
      amrPicturePtrArray[ZPLANE]->SetVLine(amrPicturePtrArray[ZPLANE]->
				           GetVLine()-1* currentScale);
      amrPicturePtrArray[ZPLANE]->DoExposePicture();
      amrPicturePtrArray[YPLANE]->SetVLine(amrPicturePtrArray[YPLANE]->
				GetVLine()-1* currentScale);
      amrPicturePtrArray[YPLANE]->DoExposePicture();
      amrPicturePtrArray[plane]->ChangeSlice(amrPicturePtrArray[plane]->
				GetSlice()-1);
    } else {
      amrPicturePtrArray[ZPLANE]->SetVLine(amrPicturePtrArray[YPLANE]->
				ImageSizeH()-1);
      amrPicturePtrArray[ZPLANE]->DoExposePicture();
      amrPicturePtrArray[YPLANE]->SetVLine(amrPicturePtrArray[YPLANE]->
				ImageSizeH()-1);
      amrPicturePtrArray[YPLANE]->DoExposePicture();
      amrPicturePtrArray[plane]->ChangeSlice(amrPicturePtrArray[plane]->
				GetSubDomain()[amrPicturePtrArray[plane]->
				MaxAllowableLevel()].bigEnd(XDIR));
    }
  }
  if(plane == YPLANE) {
    if(amrPicturePtrArray[plane]->GetSlice() > amrPicturePtrArray[plane]->
    				GetSubDomain()[amrPicturePtrArray[plane]->
   				MaxAllowableLevel()].smallEnd(YDIR))
    {
      amrPicturePtrArray[XPLANE]->SetVLine(amrPicturePtrArray[XPLANE]->
				GetVLine()-1* currentScale);
      amrPicturePtrArray[XPLANE]->DoExposePicture();
      amrPicturePtrArray[ZPLANE]->SetHLine(amrPicturePtrArray[ZPLANE]->
				GetHLine()+1* currentScale);
      amrPicturePtrArray[ZPLANE]->DoExposePicture();
      amrPicturePtrArray[plane]->ChangeSlice(amrPicturePtrArray[plane]->
				GetSlice()-1);
    } else {
      amrPicturePtrArray[XPLANE]->SetVLine(amrPicturePtrArray[XPLANE]->
				ImageSizeH()-1);
      amrPicturePtrArray[XPLANE]->DoExposePicture();
      amrPicturePtrArray[ZPLANE]->SetHLine(0);
      amrPicturePtrArray[ZPLANE]->DoExposePicture();
      amrPicturePtrArray[plane]->ChangeSlice(amrPicturePtrArray[plane]->
				GetSubDomain()[amrPicturePtrArray[plane]->
				MaxAllowableLevel()].bigEnd(YDIR));
    }
  }
  if(plane == ZPLANE) {
    if(amrPicturePtrArray[plane]->GetSlice() > amrPicturePtrArray[plane]->
    				GetSubDomain()[amrPicturePtrArray[plane]->
   				MaxAllowableLevel()].smallEnd(ZDIR))
    {
      amrPicturePtrArray[XPLANE]->SetHLine(amrPicturePtrArray[XPLANE]->
				GetHLine()+1* currentScale);
      amrPicturePtrArray[XPLANE]->DoExposePicture();
      amrPicturePtrArray[YPLANE]->SetHLine(amrPicturePtrArray[YPLANE]->
				GetHLine()+1* currentScale);
      amrPicturePtrArray[YPLANE]->DoExposePicture();
      amrPicturePtrArray[plane]->ChangeSlice(amrPicturePtrArray[plane]->
				GetSlice()-1);
    } else {
      amrPicturePtrArray[XPLANE]->SetHLine(0);
      amrPicturePtrArray[XPLANE]->DoExposePicture();
      amrPicturePtrArray[YPLANE]->SetHLine(0);
      amrPicturePtrArray[YPLANE]->DoExposePicture();
      amrPicturePtrArray[plane]->ChangeSlice(amrPicturePtrArray[plane]->
				GetSubDomain()[amrPicturePtrArray[plane]->
				MaxAllowableLevel()].bigEnd(ZDIR));
    }
  }
  DoExposeRef();
}


// -------------------------------------------------------------------
void PltApp::DoForwardStep(int plane) {
  if(plane == XPLANE) {
    if(amrPicturePtrArray[plane]->GetSlice() < amrPicturePtrArray[plane]->
    				GetSubDomain()[amrPicturePtrArray[plane]->
   				MaxAllowableLevel()].bigEnd(XDIR))
    {
      amrPicturePtrArray[ZPLANE]->SetVLine(amrPicturePtrArray[ZPLANE]->
				GetVLine()+1* currentScale);
      amrPicturePtrArray[ZPLANE]->DoExposePicture();
      amrPicturePtrArray[YPLANE]->SetVLine(amrPicturePtrArray[YPLANE]->
				GetVLine()+1* currentScale);
      amrPicturePtrArray[YPLANE]->DoExposePicture();
      amrPicturePtrArray[plane]->ChangeSlice(amrPicturePtrArray[plane]->
				GetSlice()+1);
    } else {
      amrPicturePtrArray[ZPLANE]->SetVLine(0);
      amrPicturePtrArray[ZPLANE]->DoExposePicture();
      amrPicturePtrArray[YPLANE]->SetVLine(0);
      amrPicturePtrArray[YPLANE]->DoExposePicture();
      amrPicturePtrArray[plane]->ChangeSlice(amrPicturePtrArray[plane]->
				GetSubDomain()[amrPicturePtrArray[plane]->
				MaxAllowableLevel()].smallEnd(XDIR));
    }
  }
  if(plane == YPLANE) {
    if(amrPicturePtrArray[plane]->GetSlice() < amrPicturePtrArray[plane]->
    				GetSubDomain()[amrPicturePtrArray[plane]->
   				MaxAllowableLevel()].bigEnd(YDIR))
    {
      amrPicturePtrArray[XPLANE]->SetVLine(amrPicturePtrArray[XPLANE]->
				GetVLine()+1* currentScale);
      amrPicturePtrArray[XPLANE]->DoExposePicture();
      amrPicturePtrArray[ZPLANE]->SetHLine(amrPicturePtrArray[ZPLANE]->
				GetHLine()-1* currentScale);
      amrPicturePtrArray[ZPLANE]->DoExposePicture();
      amrPicturePtrArray[plane]->ChangeSlice(amrPicturePtrArray[plane]->
				GetSlice()+1);
    } else {
      amrPicturePtrArray[XPLANE]->SetVLine(0);
      amrPicturePtrArray[XPLANE]->DoExposePicture();
      amrPicturePtrArray[ZPLANE]->SetHLine(amrPicturePtrArray[XPLANE]->
				      ImageSizeV()-1);
      amrPicturePtrArray[ZPLANE]->DoExposePicture();
      amrPicturePtrArray[plane]->ChangeSlice(amrPicturePtrArray[plane]->
				GetSubDomain()[amrPicturePtrArray[plane]->
				MaxAllowableLevel()].smallEnd(YDIR));
    }
  }
  if(plane == ZPLANE) {
    if(amrPicturePtrArray[plane]->GetSlice() < amrPicturePtrArray[plane]->
    				GetSubDomain()[amrPicturePtrArray[plane]->
   				MaxAllowableLevel()].bigEnd(ZDIR))
    {
      amrPicturePtrArray[XPLANE]->SetHLine(amrPicturePtrArray[XPLANE]->
				GetHLine()-1* currentScale);
      amrPicturePtrArray[XPLANE]->DoExposePicture();
      amrPicturePtrArray[YPLANE]->SetHLine(amrPicturePtrArray[YPLANE]->
				GetHLine()-1* currentScale);
      amrPicturePtrArray[YPLANE]->DoExposePicture();
      amrPicturePtrArray[plane]->ChangeSlice(amrPicturePtrArray[plane]->
				GetSlice()+1);
    } else {
      amrPicturePtrArray[XPLANE]->SetHLine(amrPicturePtrArray[XPLANE]->
					ImageSizeV()-1);
      amrPicturePtrArray[XPLANE]->DoExposePicture();
      amrPicturePtrArray[YPLANE]->SetHLine(amrPicturePtrArray[YPLANE]->
					ImageSizeV()-1);
      amrPicturePtrArray[YPLANE]->DoExposePicture();
      amrPicturePtrArray[plane]->ChangeSlice(amrPicturePtrArray[plane]->
				GetSubDomain()[amrPicturePtrArray[plane]->
				MaxAllowableLevel()].smallEnd(ZDIR));
    }
  }
  DoExposeRef();
}


// -------------------------------------------------------------------
void PltApp::DoChangePlane(XtPointer cd, XtIntervalId *) {
  PltApp *obj = (PltApp *) (((ClientDataStruct *) cd)->object);
  Widget cbw = ((ClientDataStruct *) cd)->w;
  if(((ClientDataStruct *) cd)->tempint1 == true) {  // double click
    if(cbw == obj->wControls[WCXNEG]) {
      obj->amrPicturePtrArray[XPLANE]->Sweep(ANIMNEGDIR);
    } else if(cbw == obj->wControls[WCXPOS]) {
      obj->amrPicturePtrArray[XPLANE]->Sweep(ANIMPOSDIR);
    } else if(cbw == obj->wControls[WCYNEG]) {
      obj->amrPicturePtrArray[YPLANE]->Sweep(ANIMNEGDIR);
    } else if(cbw == obj->wControls[WCYPOS]) {
      obj->amrPicturePtrArray[YPLANE]->Sweep(ANIMPOSDIR);
    } else if(cbw == obj->wControls[WCZNEG]) {
      obj->amrPicturePtrArray[ZPLANE]->Sweep(ANIMNEGDIR);
    } else if(cbw == obj->wControls[WCZPOS]) {
      obj->amrPicturePtrArray[ZPLANE]->Sweep(ANIMPOSDIR);
    } else if(cbw == obj->wControls[WCATNEG]) {
      obj->Animate(ANIMNEGDIR);
    } else if(cbw == obj->wControls[WCATPOS]) {
      obj->Animate(ANIMPOSDIR);
    }
  } else {  // single click
    if(cbw == obj->wControls[WCXNEG]) {
      obj->DoBackStep(XPLANE);
    } else if(cbw == obj->wControls[WCXPOS]) {
      obj->DoForwardStep(XPLANE);
    } else if(cbw == obj->wControls[WCYNEG]) {
      obj->DoBackStep(YPLANE);
    } else if(cbw == obj->wControls[WCYPOS]) {
      obj->DoForwardStep(YPLANE);
    } else if(cbw == obj->wControls[WCZNEG]) {
      obj->DoBackStep(ZPLANE);
    } else if(cbw == obj->wControls[WCZPOS]) {
      obj->DoForwardStep(ZPLANE);
    } else if(cbw == obj->wControls[WCATNEG]) {
      obj->DoAnimBackStep();
    } else if(cbw == obj->wControls[WCATPOS]) {
      obj->DoAnimForwardStep();
    }
  }
}


// -------------------------------------------------------------------
void PltApp::CBChangePlane(Widget w, XtPointer client_data, XtPointer cbs) {
  PltApp *obj = (PltApp *) client_data;

  if(w == obj->wControls[WCSTOP]) {
    obj->amrPicturePtrArray[XPLANE]->DoStop();
    obj->amrPicturePtrArray[YPLANE]->DoStop();
    obj->amrPicturePtrArray[ZPLANE]->DoStop();
    return;
  }
  if(w == obj->wControls[WCASTOP]) {
    obj->StopAnimation();
    return;
  }

  obj->clientData.w = w;
  obj->clientData.object = obj;
  obj->multiclickInterval = XtGetMultiClickTime(obj->GAptr->PDisplay());

  XmPushButtonCallbackStruct *cbstr = (XmPushButtonCallbackStruct *) cbs;
  if(cbstr->click_count == 1) {  // single click
    obj->clientData.tempint1 = false;
    obj->multiclickIId = XtAppAddTimeOut(obj->appContext,
		    obj->multiclickInterval,
		    &PltApp::DoChangePlane, (XtPointer) &(obj->clientData));
  } else if(cbstr->click_count == 2) {  // double click
    XtRemoveTimeOut(obj->multiclickIId);
    obj->clientData.tempint1 = true;
    DoChangePlane((XtPointer) &(obj->clientData), &(obj->multiclickIId));
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
  ParallelDescriptor::Abort("PltApp::ResetAnimation() not yet implemented.");
/*
  StopAnimation();
  if( ! interfaceReady) {
#   if(BL_SPACEDIM == 2)
      int maxAllowableLevel = amrPicturePtrArray[ZPLANE]->MaxAllowableLevel();
      AmrPicture *tempap = amrPicturePtrArray[ZPLANE];
      AmrPlot *newAmrPlot = new AmrPlot;
      Array<Box> domain = tempap->GetSubDomain();
      char tempString[BUFSIZ];
      strcpy(tempString, tempap->CurrentDerived().c_str());
      FileType fileType = GetDefaultFileType();
      assert(fileType != INVALIDTYPE);
      if(fileType == BOXDATA || fileType == BOXCHAR || fileType == FAB) {
        newAmrPlot->ReadNonPlotFile(fileNames[currentFrame], fileType);
      } else {
        newAmrPlot->ReadPlotFile(fileNames[currentFrame]);
      }
      amrPlotPtr = newAmrPlot;
      tempap->SetAmrPlotPtr(newAmrPlot); 

      Box fineDomain = domain[tempap->MaxAllowableLevel()];
      fineDomain.refine(CRRBetweenLevels(tempap->MaxAllowableLevel(),
			amrPlotPtr->FinestLevel(), amrPlotPtr->RefRatio()));
      amrPicturePtrArray[ZPLANE] = new AmrPicture(ZPLANE, minAllowableLevel, GAptr, 
    			fineDomain, tempap, this);
      amrPicturePtrArray[ZPLANE]->SetMaxDrawnLevel(maxDrawnLevel);

      XtRemoveEventHandler(wPlotPlane[ZPLANE], ExposureMask, false, 
  	(XtEventHandler) CBDoExposePicture, (XtPointer) tempap);
      delete tempap;
      amrPicturePtrArray[ZPLANE]->CreatePicture(XtWindow(wPlotPlane[ZPLANE]),
                                                pltPaletteptr, tempString);
      XtAddEventHandler(wPlotPlane[ZPLANE], ExposureMask, false, 
  	                (XtEventHandler) CBDoExposePicture,
	                (XtPointer) amrPicturePtrArray[ZPLANE]);
      interfaceReady = true;
#   endif
  }
*/
}


// -------------------------------------------------------------------
void PltApp::StopAnimation() {
  if(animating) {
    XtRemoveTimeOut(animating);
    animating = false;
  }
}


// -------------------------------------------------------------------
void PltApp::Animate(AnimDirection direction) {
  StopAnimation();
  animating = XtAppAddTimeOut(appContext, frameSpeed,
	&PltApp::CBUpdateFrame, (XtPointer) this);
  animDirection = direction;
}


// -------------------------------------------------------------------
void PltApp::DirtyFrames() {
  int i;
  paletteDrawn = false;
  for(i = 0; i < animFrames; i++) {
    if(readyFrames[i]) {
      XDestroyImage(frameBuffer[i]);
    }
    readyFrames[i] = false;
  }
}


// -------------------------------------------------------------------
void PltApp::CBUpdateFrame(XtPointer client_data, XtIntervalId *) {
  ((PltApp *) client_data)->DoUpdateFrame();
}


// -------------------------------------------------------------------
void PltApp::DoUpdateFrame() {
  if(animDirection == ANIMPOSDIR) {
    currentFrame++;
    if(currentFrame == animFrames) {
      currentFrame = 0;
    }
  } else {
    currentFrame--;
    if(currentFrame < 0) {
      currentFrame = animFrames - 1;
    }
  }
  ShowFrame();
  XSync(GAptr->PDisplay(), false);
  animating = XtAppAddTimeOut(appContext, frameSpeed,
	                      &PltApp::CBUpdateFrame, (XtPointer) this);
}


// -------------------------------------------------------------------
void PltApp::ShowFrame() {
  ParallelDescriptor::Abort("PltApp::ShowFrame() not yet implemented.");
/*
  interfaceReady = false;
  if( ! readyFrames[currentFrame] || datasetShowing || rangeType == USEFILE) {
#   if(BL_SPACEDIM == 2)
      AmrPicture *tempap = amrPicturePtrArray[ZPLANE];
      AmrPlot *newAmrPlot = new AmrPlot;
      Array<Box> domain = tempap->GetSubDomain();
      char tempString[BUFSIZ];
      strcpy(tempString, tempap->CurrentDerived().c_str());

      FileType fileType = GetDefaultFileType();
      assert(fileType != INVALIDTYPE);
      if(fileType == BOXDATA || fileType == BOXCHAR || fileType == FAB) {
        newAmrPlot->ReadNonPlotFile(fileNames[currentFrame], fileType);
      } else {
        newAmrPlot->ReadPlotFile(fileNames[currentFrame]);
      }
      amrPlotPtr = newAmrPlot;
      tempap->SetAmrPlotPtr(newAmrPlot); 

      const AmrData &amrData = dataServices->AmrDataRef();
      int finestLevel(amrData.FinestLevel());
      int maxlev = DetermineMaxAllowableLevel(amrData.ProbDomain(finestLevel),
		       finestLevel, MaxPictureSize(), amrData.RefRatio());

      Box fineDomain = domain[tempap->MaxAllowableLevel()];
      fineDomain.refine(CRRBetweenLevels(tempap->MaxAllowableLevel(),
			finestLevel, amrData.RefRatio()));
      amrPicturePtrArray[ZPLANE] = new AmrPicture(ZPLANE, minAllowableLevel, GAptr,
	                                          fineDomain, tempap, this);
      XtRemoveEventHandler(wPlotPlane[ZPLANE], ExposureMask, false, 
  	                (XtEventHandler) CBDoExposePicture, (XtPointer) tempap);

      delete tempap;

      amrPicturePtrArray[ZPLANE]->SetMaxDrawnLevel(maxDrawnLevel);
      amrPicturePtrArray[ZPLANE]->CreatePicture(XtWindow(wPlotPlane[ZPLANE]),
                                                pltPaletteptr, tempString);
      XtAddEventHandler(wPlotPlane[ZPLANE], ExposureMask, false, 
  	                (XtEventHandler) CBDoExposePicture,
	                (XtPointer) amrPicturePtrArray[ZPLANE]);
      frameBuffer[currentFrame] = amrPicturePtrArray[ZPLANE]->GetPictureXImage();
#   endif
    readyFrames[currentFrame] = true;
    if(rangeType == USEFILE) {
      paletteDrawn = false;
    } else {
      paletteDrawn = true;
    }
  }

  XPutImage(GAptr->PDisplay(), XtWindow(wPlotPlane[ZPLANE]), GAptr->PGC(),
    frameBuffer[currentFrame], 0, 0, 0, 0,
    amrPicturePtrArray[ZPLANE]->ImageSizeH(),
    amrPicturePtrArray[ZPLANE]->ImageSizeV());

  char tempFileName[1024];
  strcpy(tempFileName, fileNames[currentFrame].c_str());
  XmString fileString = XmStringCreateSimple(tempFileName);
  XtVaSetValues(wWhichFileLabel, XmNlabelString, fileString, NULL);
  XmStringFree(fileString);
  XmScaleSetValue(wWhichFileScale, currentFrame);

  if(datasetShowing) {
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
    datasetPtr->Render(trueRegion, amrPicturePtrArray[ZPLANE], this,
		       hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  }
*/
}  // end ShowFrame



// -------------------------------------------------------------------
void PltApp::SetGlobalMinMax(Real specifiedMin, Real specifiedMax) {
  assert(specifiedMax > specifiedMin);
  for(int np = 0; np < NPLANES; np++) {
    amrPicturePtrArray[np]->SetWhichRange(USESPECIFIED);
    amrPicturePtrArray[np]->SetDataMin(specifiedMin);
    amrPicturePtrArray[np]->SetDataMax(specifiedMax);
  }
}


// -------------------------------------------------------------------
int  PltApp::initialScale;
int  PltApp::defaultShowBoxes;
int  PltApp::initialWindowHeight;
int  PltApp::initialWindowWidth;
int  PltApp::placementOffsetX    = 0;
int  PltApp::placementOffsetY    = 0;
int  PltApp::reserveSystemColors = 0;
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
  PltApp::reserveSystemColors = reservesystemcolors;
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
