// -------------------------------------------------------------------
//  Dataset.cpp
// -------------------------------------------------------------------

const int CHARACTERWIDTH  = 13;
const int CHARACTERHEIGHT = 22;
const int MAXINDEXCHARS   = 4;

#include "Dataset.H"
#include "PltApp.H"
#include "DataServices.H"
#include "GlobalUtilities.H"

#ifdef BL_USE_NEW_HFILES
#include <strstream>
#include <cfloat>
using std::ostrstream;
using std::ends;
# else
#include <strstream.h>
#include <float.h>
#endif


// -------------------------------------------------------------------
Dataset::Dataset(Widget top, const Box &alignedRegion, AmrPicture *apptr,
		 PltApp *pltappptr, int hdir, int vdir, int sdir)
{

  // alignedRegion is defined on the maxAllowableLevel
  int i;
  stringOk = true;
  datasetPoint = false;
  hDIR = hdir;
  vDIR = vdir;
  sDIR = sdir;
  wAmrVisTopLevel = top; 
  pltAppPtr = pltappptr;
  amrPicturePtr = apptr;
  dataServicesPtr = pltAppPtr->GetDataServicesPtr();
  finestLevel = amrPicturePtr->FinestLevel();
  maxAllowableLevel = amrPicturePtr->MaxAllowableLevel();
  char tempFormat[32];
  strcpy(tempFormat, pltAppPtr->GetFormatString().c_str());
  XmString sFormatString = XmStringCreateSimple(tempFormat);
  char *fbuff;
  XmStringGetLtoR(sFormatString, XmSTRING_DEFAULT_CHARSET, &fbuff);
  formatString = fbuff;
  hStringOffset = 12;
  vStringOffset = -4;

  XmStringFree(sFormatString);

  whiteIndex = int(pltAppPtr->GetPalettePtr()->WhiteIndex());
  blackIndex = int(pltAppPtr->GetPalettePtr()->BlackIndex());

  indexWidth  = MAXINDEXCHARS * CHARACTERWIDTH;
  indexHeight = CHARACTERHEIGHT + 7;
  
  hScrollBarPos = vScrollBarPos = 0;

  pixSizeX = 1;
  pixSizeY = 1;

  // ************************************************ Dataset Window 
   char header[BUFSIZ];
   char shortfilename[BUFSIZ];
   char pltfilename[BUFSIZ];
   strcpy(pltfilename, pltAppPtr->GetFileName().c_str());
   int fnl = strlen(pltfilename) - 1;
   while(fnl>-1 && pltfilename[fnl] != '/') {
     fnl--;
   }
   strcpy(shortfilename, &pltfilename[fnl+1]);
   
   ostrstream outstr(header, sizeof(header));
   outstr << shortfilename << "  " << amrPicturePtr->CurrentDerived()
          << "  " << alignedRegion << ends;
   wDatasetTopLevel = XtVaCreatePopupShell(header, topLevelShellWidgetClass,
                                           wAmrVisTopLevel,
                                           XmNwidth,	800,
                                           XmNheight,	500,
                                           NULL);
   
  GAptr = new GraphicsAttributes(wDatasetTopLevel);
  if(GAptr->PVisual() !=
     XDefaultVisual(GAptr->PDisplay(),GAptr->PScreenNumber())) {
      XtVaSetValues(wDatasetTopLevel, XmNvisual, GAptr->PVisual(),
                    XmNdepth, GAptr->PDepth(), NULL);
  }

  wDatasetForm = XtVaCreateManagedWidget("datasetform",
			xmFormWidgetClass, wDatasetTopLevel,
			NULL);

  wDatasetTools = XtVaCreateManagedWidget("datasettools",
			xmFormWidgetClass, wDatasetForm,
	                XmNtopAttachment,       XmATTACH_FORM,
                	XmNleftAttachment,      XmATTACH_FORM,
                	XmNrightAttachment,     XmATTACH_FORM,
                	XmNheight,              50,
			NULL);

  // ************************************************ Format Text Field
  i=0;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNtopOffset, WOFFSET);      i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNleftOffset, WOFFSET);      i++;
  char fbuff2[64];
  strcpy(fbuff2, formatString.c_str());
  XtSetArg(args[i], XmNvalue, fbuff2);      i++;
  XtSetArg(args[i], XmNcolumns, 12);      i++;
  wFormat = XtCreateManagedWidget("format", xmTextFieldWidgetClass,
				    wDatasetTools, args, i);
  XtAddCallback(wFormat, XmNactivateCallback, &Dataset::CBReadString,
		(XtPointer) this);
  Dimension bHeight;
  XtVaGetValues(wFormat, XmNheight, &bHeight, NULL);

  // ************************************************ Color Button
  i=0;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNtopOffset, WOFFSET);      i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET);      i++;
  XtSetArg(args[i], XmNleftWidget, wFormat);      i++;
  XtSetArg(args[i], XmNleftOffset, WOFFSET);      i++;
  XtSetArg(args[i], XmNheight, bHeight);      i++;
  wColorButton = XmCreateToggleButton(wDatasetTools, "Color", args, i);
  XtAddCallback(wColorButton, XmNvalueChangedCallback, &Dataset::CBColorButton,
		(XtPointer) this);
  XmToggleButtonSetState(wColorButton, true, false);

  // ************************************************ Close Button
  i=0;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNtopOffset, WOFFSET);      i++;
  XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNrightOffset, 20);      i++;
  XtSetArg(args[i], XmNheight, bHeight);      i++;
  wQuitButton = XmCreatePushButton(wDatasetTools, "Close", args, i);
  XtAddCallback(wQuitButton, XmNactivateCallback, &Dataset::CBQuitButton,
		(XtPointer) this);

  // ************************************************ Max

  XmString sMax = XmStringCreateSimple("");
  wMaxValue = XtVaCreateManagedWidget("maxValue", 
                                    xmLabelWidgetClass, wDatasetTools,
                                    XmNlabelString, sMax, 
                                    XmNtopAttachment, XmATTACH_FORM, 
                                    XmNtopOffset, WOFFSET+8,
                                    XmNrightAttachment, XmATTACH_WIDGET,
                                    XmNrightWidget, wQuitButton,
                                    XmNrightOffset, WOFFSET+10,
                                    XmNheight, bHeight,
                                    NULL);
  XmStringFree(sMax);

  // ************************************************ Min 
  XmString sMin = XmStringCreateSimple("");
  wMinValue = XtVaCreateManagedWidget("minValue", 
                                    xmLabelWidgetClass, wDatasetTools,
                                    XmNlabelString, sMin, 
                                    XmNtopAttachment, XmATTACH_FORM, 
                                    XmNtopOffset, WOFFSET+8,
                                    XmNrightAttachment, XmATTACH_WIDGET,
                                    XmNrightWidget, wMaxValue,
                                    XmNrightOffset, WOFFSET+6,
                                    XmNheight, bHeight,
                                    NULL);
  XmStringFree(sMin);
                                    
  

  // ************************************************ Levels
  

  XmString sLabel = XmStringCreateSimple("");
  wLevels = XtVaCreateManagedWidget("levels", 
                                    xmLabelWidgetClass, wDatasetTools,
                                    XmNlabelString, sLabel, 
                                    XmNtopAttachment, XmATTACH_FORM, 
                                    XmNtopOffset, WOFFSET+8,
                                    XmNrightAttachment, XmATTACH_WIDGET,
                                    XmNrightWidget, wMinValue,
                                    XmNrightOffset, WOFFSET+6,
                                    XmNheight, bHeight,
                                    NULL);
  XmStringFree(sLabel);
                                    
                                    
  // ************************************************ data area 
  wScrollArea = 
    XtVaCreateManagedWidget("scrollArea",
                            xmScrolledWindowWidgetClass,
                            wDatasetForm,
                            XmNtopAttachment,	XmATTACH_WIDGET,
                            XmNtopWidget,		wDatasetTools,
                            XmNleftAttachment,	XmATTACH_FORM,
                            XmNrightAttachment,	XmATTACH_FORM,
                            XmNbottomAttachment,	XmATTACH_FORM,
                            XmNscrollingPolicy,	XmAUTOMATIC,
                            NULL);

  String trans =
	"<Btn1Motion>: DrawingAreaInput() ManagerGadgetButtonMotion() \n\
	<Btn1Down>: DrawingAreaInput() ManagerGadgetButtonMotion() \n\
	<Btn1Up>: DrawingAreaInput() ManagerGadgetButtonMotion()";


  wPixArea = XtVaCreateManagedWidget("pixArea", xmDrawingAreaWidgetClass,
			wScrollArea,
			XmNtranslations,	XtParseTranslationTable(trans),
			XmNwidth,		pixSizeX, 
			XmNheight,		pixSizeY,
			NULL);		
  XtAddCallback(wPixArea, XmNinputCallback, &Dataset::CBPixInput,
		(XtPointer) this);
  XtVaSetValues(wScrollArea, XmNworkWindow, wPixArea, NULL);

  XtManageChild(wScrollArea);
  XtManageChild(wPixArea);
  XtManageChild(wColorButton);
  XtManageChild(wQuitButton);
  XtPopup(wDatasetTopLevel, XtGrabNone);

  XtAddEventHandler(wPixArea, ExposureMask, false,
		&Dataset::CBDoExposeDataset, (XtPointer) this);	

  Widget wHScrollBar, wVScrollBar;

  XtVaGetValues(wScrollArea, XmNhorizontalScrollBar, &wHScrollBar,
	        XmNverticalScrollBar, &wVScrollBar, NULL);

  XtAddCallback(wHScrollBar, XmNdragCallback,
		&Dataset::CBScrolling, (XtPointer) this);	
  XtAddCallback(wHScrollBar, XmNvalueChangedCallback,
		&Dataset::CBEndScrolling, (XtPointer) this);	
  XtAddCallback(wVScrollBar, XmNdragCallback,
		&Dataset::CBScrolling, (XtPointer) this);	
  XtAddCallback(wVScrollBar, XmNvalueChangedCallback,
		&Dataset::CBEndScrolling, (XtPointer) this);	

  dragging = false;
  drags = 0;

  myDataStringArray = NULL;
  bDataStringArrayAllocated = false;

  Render(alignedRegion, amrPicturePtr, pltAppPtr, hdir, vdir, sdir);
}  // end Dataset::Dataset


// -------------------------------------------------------------------
Dataset::~Dataset() {
  delete GAptr;
  delete [] datasetRegion;
  delete [] dataStringArray;
  if(bDataStringArrayAllocated) {
    for(int j = 0; j<=maxAllowableLevel; ++j) {
      delete [] myDataStringArray[j];
    }
    delete [] myDataStringArray;
  }
  XtDestroyWidget(wDatasetTopLevel);
}


// -------------------------------------------------------------------
void Dataset::Render(const Box &alignedRegion, AmrPicture *apptr,
		     PltApp *pltappptr, int hdir, int vdir, int sdir)
{
  int lev, i, c, d, stringCount;
  Box temp, dataBox;
  Real *dataPoint; 
  
  if(bDataStringArrayAllocated) {
    for(int j = 0; j<=maxAllowableLevel; ++j) {
      delete [] myDataStringArray[j];
    }
    delete [] myDataStringArray;
  }
  
  maxDrawnLevel = pltAppPtr->MaxDrawnLevel(); 
  minDrawnLevel = pltAppPtr->MinDrawnLevel();
  
  hDIR = hdir;
  vDIR = vdir;
  sDIR = sdir;
  if (hDIR == 0)
    hAxisString = "x";
  else if (hDIR == 1)     
    hAxisString = "y";
  else 
    hAxisString = "error";
  if (vDIR == 1)
    vAxisString = "y";
  else if (vDIR == 2)
    vAxisString = "z";
  else
    vAxisString = "error";

  if( ! stringOk) {
    return;
  }
  
  pltAppPtr = pltappptr;
  amrPicturePtr = apptr;
  dataServicesPtr = pltAppPtr->GetDataServicesPtr();
  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  finestLevel = amrPicturePtr->NumberOfLevels() - 1;
  maxAllowableLevel = amrPicturePtr->MaxAllowableLevel();
  
  // set up datasetRegion
  //we shall waste some space to that datasetRegion[lev] corresponds to lev
  datasetRegion = new Box[maxAllowableLevel+1];
  datasetRegion[maxAllowableLevel] = alignedRegion;
  for(i = maxAllowableLevel - 1; i >= 0; --i) {
    datasetRegion[i] = datasetRegion[maxAllowableLevel];
    datasetRegion[i].coarsen(
         CRRBetweenLevels(i, maxAllowableLevel, amrData.RefRatio()));
  }
  
  // datasetRegion is now an array of Boxes that encloses the selected region
  
  Array<FArrayBox> dataFab(maxAllowableLevel+1);
  for(i = 0; i <= maxAllowableLevel; ++i) {
    dataFab[i].resize(datasetRegion[i], 1);
  }
  
  Palette *palptr = pltAppPtr->GetPalettePtr();
  int colorSlots(palptr->ColorSlots());
  int paletteStart(palptr->PaletteStart());
  int paletteEnd(palptr->PaletteEnd());
  
  char header[BUFSIZ];
  char shortfilename[BUFSIZ];
  char pltfilename[BUFSIZ];
  strcpy(pltfilename, pltAppPtr->GetFileName().c_str());
  int fnl = strlen(pltfilename) - 1;
  while(fnl>-1 && pltfilename[fnl] != '/') {
    --fnl;
  }
  strcpy(shortfilename, &pltfilename[fnl+1]);
  
  ostrstream outstr(header, sizeof(header));
  outstr << shortfilename << "  " << amrPicturePtr->CurrentDerived()
         << "  " << alignedRegion << ends;
  
  XtVaSetValues(wDatasetTopLevel, XmNtitle, header, NULL);
  // find largest data width and count # of data strings 
  
  int largestWidth(0);
  stringCount = 0;
  myStringCount = new int[maxAllowableLevel];
  for(lev = 0; lev <= maxAllowableLevel; ++lev) {
    myStringCount[lev] = 0;
    DataServices::Dispatch(DataServices::FillVarOneFab, dataServicesPtr,
                           &dataFab[lev], dataFab[lev].box(),
                           lev, amrPicturePtr->CurrentDerived());
    for(int iBox = 0; iBox < amrData.boxArray(lev).length(); ++iBox) {
      temp = amrData.boxArray(lev)[iBox];
      if(datasetRegion[lev].intersects(temp)) {
        int ddl;
        temp &= datasetRegion[lev];
        dataBox = temp;
        FArrayBox dataFabTemp(dataBox, 1);
        dataFabTemp.copy(dataFab[lev]);
        dataPoint = dataFabTemp.dataPtr();
        stringCount += dataBox.length(vDIR) * dataBox.length(hDIR);
        myStringCount[lev] +=  dataBox.length(vDIR) * dataBox.length(hDIR);
        for(d = 0; d < dataBox.length(vDIR); ++d) {
          ddl = d*dataBox.length(hDIR);
          for(c = 0; c < dataBox.length(hDIR); ++c) {
            sprintf(dataString, formatString.c_str(), dataPoint[c+ddl]);
            largestWidth = Max((int)strlen(dataString), largestWidth);
          }
        }
      }
    }
  }

  char levelInfo[15], maxInfo[20], minInfo[20], maxInfoV[25], minInfoV[25];
  
  sprintf(levelInfo, "Level: %i", maxDrawnLevel);

  XmString sNewLevel = XmStringCreateSimple(levelInfo);
  XtVaSetValues(wLevels, 
                XmNlabelString, sNewLevel, 
                NULL);
  XmStringFree(sNewLevel);
  
  
  sprintf(minInfoV, formatString.c_str(), dataFab[maxDrawnLevel].min() );
  sprintf(minInfo, "Min:%s", minInfoV);
  XmString sNewMin = XmStringCreateSimple(minInfo);
  XtVaSetValues(wMinValue, XmNlabelString, sNewMin, NULL);
  XmStringFree(sNewMin);

  sprintf(maxInfoV, formatString.c_str(), dataFab[maxDrawnLevel].max() );
  sprintf(maxInfo, "Max:%s", maxInfoV);
  XmString sNewMax = XmStringCreateSimple(maxInfo);
  XtVaSetValues(wMaxValue, XmNlabelString, sNewMax, NULL);
  XmStringFree(sNewMax);

  
  if(Verbose()) {
    cout << stringCount << " data points" << endl;
  }
  numStrings = stringCount;
  dataStringArray = new StringLoc[numStrings];
  int ns;
  for(ns = 0; ns < numStrings; ns++) {
    dataStringArray[ns].olflag = maxDrawnLevel;
  }
  if(dataStringArray == NULL) {
    cout << "Error in Dataset::Render:  out of memory" << endl;
    return;
  }
  myDataStringArray = new StringLoc * [maxAllowableLevel+1];
  int level;
  for(level = 0; level <= maxDrawnLevel; ++level) {
    myDataStringArray[level] = new StringLoc [myStringCount[level] ];
  }
  for(level = maxDrawnLevel + 1; level <= maxAllowableLevel; ++level) {
    myDataStringArray[level] = NULL;
  }
  bDataStringArrayAllocated = true;

  //determine the length of the character labels for the indices
  Real vItemCount(datasetRegion[maxDrawnLevel].bigEnd(vDIR));
  int vIndicesWidth(ceil(log10(vItemCount+1)));
  Real hItemCount(datasetRegion[maxDrawnLevel].bigEnd(hDIR));
  int hIndicesWidth(ceil(log10(hItemCount+1)));

  largestWidth = Max(largestWidth, hIndicesWidth);  
  indexWidth = Max(MAXINDEXCHARS, vIndicesWidth+1) * CHARACTERWIDTH;

  // determine size of data area
  dataItemWidth = largestWidth * CHARACTERWIDTH;
  pixSizeX = datasetRegion[maxDrawnLevel].length(hDIR) * dataItemWidth
    + ((maxDrawnLevel-minDrawnLevel+1)*indexWidth);
  pixSizeY = datasetRegion[maxDrawnLevel].length(vDIR) * CHARACTERHEIGHT
    + ((maxDrawnLevel-minDrawnLevel+1)*indexHeight);
  
  // create StringLoc array and define color scheme 
  if(pixSizeX == 0 || pixSizeY == 0) {
    noData = true;
    sprintf (dataString, "No intersection.");
    pixSizeX = strlen(dataString) * CHARACTERWIDTH;
    pixSizeY = CHARACTERHEIGHT+5;
    XtVaSetValues(wPixArea,
                  XmNwidth,	pixSizeX,
                  XmNheight,	pixSizeY,
                  NULL);
  } else {
    noData = false;
    XtVaSetValues(wPixArea,
                  XmNwidth,	pixSizeX,
                  XmNheight,	pixSizeY,
                  NULL);
    
    stringCount = 0; 
    int lastLevLow(0), lastLevHigh(0);
    int csm1(colorSlots - 1);
    Real datamin(amrPicturePtr->GetWhichMin());
    Real globalDiff(amrPicturePtr->GetWhichMax() - amrPicturePtr->GetWhichMin());
    Real oneOverGlobalDiff;
    if(globalDiff < FLT_MIN) {
      oneOverGlobalDiff = 0.0;  // so we dont divide by zero
    } else {
      oneOverGlobalDiff = 1.0 / globalDiff;
    }
    
    
    for(lev = minDrawnLevel; lev <= maxDrawnLevel; ++lev) {
      for(int iBox = 0; iBox < amrData.boxArray(lev).length(); ++iBox) {
        temp = amrData.boxArray(lev)[iBox];
        if(datasetRegion[lev].intersects(temp)) {
          temp &= datasetRegion[lev];
          dataBox = temp;
          temp.refine(CRRBetweenLevels(lev,
                                       maxDrawnLevel, amrData.RefRatio()));
          temp.shift(hDIR, -datasetRegion[maxDrawnLevel].smallEnd(hDIR)); 
          temp.shift(vDIR, -datasetRegion[maxDrawnLevel].smallEnd(vDIR)); 
          FArrayBox dataFabTemp(dataBox, 1);
          dataFabTemp.copy(dataFab[lev]);
          dataPoint = dataFabTemp.dataPtr();
          int ddl;
          int crr = CRRBetweenLevels(lev, maxDrawnLevel, amrData.RefRatio());
          Real amrmax(amrPicturePtr->GetWhichMax());
          Real amrmin(amrPicturePtr->GetWhichMin());
          
          for(d = 0; d < dataBox.length(vDIR); ++d) {
            ddl = d * dataBox.length(hDIR);
            for(c = 0; c < dataBox.length(hDIR); ++c) {
              sprintf(dataString, formatString.c_str(), dataPoint[c+ddl]);
              if(dataPoint[c+ddl] > amrmax) {
                dataStringArray[stringCount].color = paletteEnd;    // clip
              } else if(dataPoint[c+ddl] < amrmin) {
                dataStringArray[stringCount].color = paletteStart;  // clip
              } else {
                dataStringArray[stringCount].color = (int)
                  (((dataPoint[c+ddl] - datamin) * oneOverGlobalDiff) * csm1 ); 
                dataStringArray[stringCount].color += paletteStart;
              }	
              dataStringArray[stringCount].xloc = (temp.smallEnd(hDIR)
                                                   + c * crr) * dataItemWidth + 5;
              dataStringArray[stringCount].yloc = pixSizeY-1 -
                (temp.smallEnd(vDIR) + d * crr) * CHARACTERHEIGHT - 4;
              
              for(i = lastLevLow; i < lastLevHigh; i++) { // remove overlap
                if(dataStringArray[i].xloc ==
                   dataStringArray[stringCount].xloc
                   && dataStringArray[i].yloc ==
                   dataStringArray[stringCount].yloc)
                  {
                    dataStringArray[i].olflag = lev-1; 
                    // highest level at which visible
                  }
              }	
              
              strcpy(dataStringArray[stringCount].ds, dataString);
              dataStringArray[stringCount].dslen = strlen(dataString);
              
              ++stringCount;
              
            }  // end for(c...)
          }  // end for(d...)
          
        }  // end if(datasetRegion[lev].intersects(temp))
      }
      lastLevLow = lastLevHigh;
      lastLevHigh = stringCount; 
    }
    
    
  }  // end if(pixSizeX...)
  
  //now load into **StringLoc
  
  int SC(0);
  for(lev = minDrawnLevel;lev<=maxDrawnLevel;lev++) {
    for(stringCount = 0; stringCount<myStringCount[lev]; stringCount++) {
      myDataStringArray[lev][stringCount] = dataStringArray[SC++];
    }
  }
  
  
  XSetWindowColormap(GAptr->PDisplay(), XtWindow(wDatasetTopLevel),
                     pltAppPtr->GetPalettePtr()->GetColormap());
  XSetWindowColormap(GAptr->PDisplay(), XtWindow(wPixArea),
                     pltAppPtr->GetPalettePtr()->GetColormap());
  XSetForeground(XtDisplay(wPixArea), GAptr->PGC(), blackIndex);
  
  Box tempBox = datasetRegion[maxDrawnLevel];
  tempBox.shift(hDIR, -datasetRegion[maxDrawnLevel].smallEnd(hDIR)); 
  tempBox.shift(vDIR, -datasetRegion[maxDrawnLevel].smallEnd(vDIR)); 
  
  hIndexArray = new StringLoc* [maxDrawnLevel+1];
  vIndexArray = new StringLoc* [maxDrawnLevel+1];
  for (int j = 0; j<=maxDrawnLevel;j++) {
    hIndexArray[j] = NULL;
    vIndexArray[j] = NULL;
  }
  for(int level = minDrawnLevel; level <= maxDrawnLevel; ++level) {
      Box temp(datasetRegion[level]);
      
      temp.refine(CRRBetweenLevels(level, maxDrawnLevel, amrData.RefRatio()));
      temp.shift(hDIR, -datasetRegion[maxDrawnLevel].smallEnd(hDIR)); 
      temp.shift(vDIR, -datasetRegion[maxDrawnLevel].smallEnd(vDIR)); 
      
      double dBoxSize(CRRBetweenLevels(level, maxDrawnLevel, amrData.RefRatio()));
      int boxSize = (ceil(dBoxSize)-dBoxSize >= 0.5? 
                     floor(dBoxSize): ceil(dBoxSize));
      int vStartPos=fmod(datasetRegion[maxDrawnLevel].bigEnd(vDIR)+1, boxSize);
      vStartPos = (vStartPos != 0? vStartPos - boxSize:vStartPos);
      // fill the box index arrays 
      Box iABox(datasetRegion[level]);
      
      hIndexArray[level] = new StringLoc[iABox.length(hDIR)];
      vIndexArray[level] = new StringLoc[iABox.length(vDIR)];
      
      // horizontal
      for(d = 0; d < iABox.length(hDIR); ++d) {
        sprintf(dataString, "%d", d + iABox.smallEnd(hDIR));
        hIndexArray[level][d].color = blackIndex;
        hIndexArray[level][d].xloc =(temp.smallEnd(hDIR)+d*boxSize)*dataItemWidth
          +hStringOffset;
        hIndexArray[level][d].yloc = 0;  // find this dynamically when drawing
        strcpy(hIndexArray[level][d].ds, dataString);
        hIndexArray[level][d].dslen = strlen(dataString);
        hIndexArray[level][d].olflag = false;
      }  // end for(d...)
      
      // vertical
      for(d = 0; d < iABox.length(vDIR); d++) {
        sprintf(dataString, "%d", d + iABox.smallEnd(vDIR));
        vIndexArray[level][d].color = blackIndex;
        vIndexArray[level][d].xloc = 0;// find this dynamically when drawing
        vIndexArray[level][d].yloc =
          ((vStartPos+boxSize*(datasetRegion[level].length(vDIR)-d))
           * CHARACTERHEIGHT)+vStringOffset;
        strcpy(vIndexArray[level][d].ds, dataString);
        vIndexArray[level][d].dslen = strlen(dataString);
        vIndexArray[level][d].olflag = false;
      }  // end for(d...)
    }  //end for(int level = minDrawnLevel...)
  
}  // end Dataset::Render


// -------------------------------------------------------------------
void Dataset::DrawGrid(int startX, int startY, int finishX, int finishY, 
                       int gridspacingX, int gridspacingY,
                       int foregroundIndex, int backgroundIndex) {
    int i;
    Display *display = XtDisplay(wPixArea);
    GC gc = GAptr->PGC();
    Window dataWindow = XtWindow(wPixArea);

    XSetBackground(display, gc, backgroundIndex);
    XSetForeground(display, gc, foregroundIndex);

    XDrawLine(display, dataWindow, gc, startX+1, startY, startX+1, finishY);
    for(i = startX; i <= finishX; i += gridspacingX) {
	XDrawLine(display, dataWindow, gc, i, startY, i, finishY);
    }

    XDrawLine(display, dataWindow, gc, finishX - 1, startY, finishX - 1, finishY);
    
    XDrawLine(display, dataWindow, gc, startX, startY + 1, finishX, startY + 1);
    for(i = startY; i <= finishY; i += gridspacingY) {
	XDrawLine(display, dataWindow, gc, startX, i, finishX, i);
    }
    XDrawLine(display, dataWindow, gc, startX, finishY - 1, finishX, finishY - 1);
} // end DrawGrid(...)


// -------------------------------------------------------------------
void Dataset::DrawGrid(int startX, int startY, int finishX, int finishY, 
                       int refRatio, int foregroundIndex, int backgroundIndex)
{
    int i;
    Display *display = XtDisplay(wPixArea);
    GC gc = GAptr->PGC();
    Window dataWindow = XtWindow(wPixArea);

    XSetBackground(display, gc, backgroundIndex);
    XSetForeground(display, gc, foregroundIndex);

    XDrawLine(display, dataWindow, gc, startX+1, startY, startX+1, finishY);
    for(i = startX; i <= finishX; i += dataItemWidth * refRatio) {
	XDrawLine(display, dataWindow, gc, i, startY, i, finishY);
    }

    XDrawLine(display, dataWindow, gc, finishX - 1, startY, finishX - 1, finishY);
    
    XDrawLine(display, dataWindow, gc, startX, startY + 1, finishX, startY + 1);
    for(i = startY; i <= finishY; i += CHARACTERHEIGHT * refRatio) {
	XDrawLine(display, dataWindow, gc, startX, i, finishX, i);
    }
    XDrawLine(display, dataWindow, gc, startX, finishY - 1, finishX, finishY - 1);
} // end DrawGrid(...)


// -------------------------------------------------------------------
void Dataset::CBQuitButton(Widget, XtPointer client_data, XtPointer) {
  Dataset *obj = (Dataset*) client_data;
  obj->DoQuitButton();
}


// -------------------------------------------------------------------
void Dataset::CBColorButton(Widget, XtPointer client_data, XtPointer) {
  Dataset *obj = (Dataset *) client_data;
  obj->DoColorButton();
}


// -------------------------------------------------------------------
void Dataset::CBReadString(Widget w, XtPointer client_data,
			XtPointer call_data)
{
  Dataset *obj = (Dataset *) client_data;
  obj->DoReadString(w, (XmSelectionBoxCallbackStruct *) call_data);
}


// -------------------------------------------------------------------
void Dataset::CBPixInput(Widget, XtPointer client_data, XtPointer call_data)
{
  Dataset *obj = (Dataset *) client_data;
  obj->DoPixInput((XmDrawingAreaCallbackStruct *) call_data);
}


// -------------------------------------------------------------------
void Dataset::DoPixInput(XmDrawingAreaCallbackStruct *cbs) {
  //Palette *palptr = pltAppPtr->GetPalettePtr();
  int hplot, vplot;
  int HDIR, VDIR, DEPTHDIR;
  static int serverControlOK = 0;
  int xcell((int) (cbs->event->xbutton.x) / dataItemWidth);
  int ycell((int) (cbs->event->xbutton.y) / CHARACTERHEIGHT);
  Box pictureBox(amrPicturePtr->GetSubDomain()[maxDrawnLevel]);
  Box regionBox(datasetRegion[maxDrawnLevel]);
  
  if(cbs->event->xany.type == ButtonPress) {
    serverControlOK++;
  }
  
  if(xcell >= 0 && xcell < pixSizeX/dataItemWidth &&
     ycell >= 0 && ycell < pixSizeY/CHARACTERHEIGHT)
  {
      if(amrPicturePtr->GetMyView() == XY) {
        HDIR = XDIR; VDIR = YDIR; DEPTHDIR = ZDIR;
      } else if(amrPicturePtr->GetMyView() == XZ) {
        HDIR = XDIR; VDIR = ZDIR; DEPTHDIR = YDIR;
      } else if(amrPicturePtr->GetMyView() == YZ) {
        HDIR = YDIR; VDIR = ZDIR; DEPTHDIR = XDIR;
      }
      
      if ( xcell > regionBox.bigEnd(HDIR)-regionBox.smallEnd(HDIR))  
        xcell = regionBox.bigEnd(HDIR)-regionBox.smallEnd(HDIR);
      if ( ycell > regionBox.bigEnd(VDIR)-regionBox.smallEnd(VDIR))  
        ycell = regionBox.bigEnd(VDIR)-regionBox.smallEnd(VDIR);
      hplot = regionBox.smallEnd(HDIR) + xcell;
      vplot = regionBox.smallEnd(VDIR) + regionBox.length(VDIR)-1 - ycell;

      const AmrData &amrData = dataServicesPtr->AmrDataRef();
      int baseRatio = CRRBetweenLevels(maxDrawnLevel, 
                                       maxAllowableLevel, 
                                       amrData.RefRatio());

# if (BL_SPACEDIM == 3)
      int boxCoor[3];
      boxCoor[HDIR] = hplot;
      boxCoor[VDIR] = vplot; 
      boxCoor[DEPTHDIR] = pltAppPtr->GetAmrPicturePtr(YZ-DEPTHDIR)->GetSlice();
      boxCoor[DEPTHDIR] /= baseRatio;
      IntVect boxLocation(boxCoor[0], boxCoor[1], boxCoor[2]);
# endif
# if (BL_SPACEDIM == 2)
      int boxCoor[2];
      boxCoor[HDIR] = hplot;
      boxCoor[VDIR] = vplot; 
      IntVect boxLocation(boxCoor[0], boxCoor[1]);
# endif
      Box chosenBox(boxLocation, boxLocation);
      int finestCLevel(amrData.FinestContainingLevel(chosenBox, maxDrawnLevel));
      finestCLevel = 
        ( finestCLevel >= minDrawnLevel ? finestCLevel : minDrawnLevel );
      int boxSize(CRRBetweenLevels(finestCLevel, maxAllowableLevel, 
                  amrData.RefRatio()));
      int modBy(CRRBetweenLevels(finestCLevel, maxDrawnLevel, amrData.RefRatio()));
      hplot -= pictureBox.smallEnd(HDIR);
      hplot -= fmod(hplot,modBy);
      vplot -= pictureBox.smallEnd(VDIR);
      vplot -= fmod(vplot,modBy);
      hplot *= baseRatio;   vplot *= baseRatio;
      vplot += boxSize;
      if(datasetPoint == true) {
        amrPicturePtr->UnDrawDatasetPoint();//box if already drawns...
      } else {
        datasetPoint = true;  // if we just started...
      }
      amrPicturePtr->DrawDatasetPoint(hplot, vplot, boxSize);
    }
  
  if(cbs->event->xany.type == ButtonRelease) {
    amrPicturePtr->UnDrawDatasetPoint();
    serverControlOK--;
    datasetPoint = false;
    if(serverControlOK != 0) {
      cerr << "incorrect server control balance -- serverControlOK: "
          << serverControlOK <<endl;
    }
    amrPicturePtr->DoExposePicture();// redraw this once to 
    // protect from incorrect bit manipulation (didn't grab the server)
  }
} // end DoPixInput


// -------------------------------------------------------------------
void Dataset::DoReadString(Widget w, XmSelectionBoxCallbackStruct *) {
  char temp[64];
  strcpy(temp, XmTextFieldGetString(w));
  if(temp[0] != '%') {
    formatString  = "%";
    formatString += temp;
  } else {
    formatString = temp;
  }
  // unexhaustive string check to prevent errors 
  stringOk = true;
  for(int i=0; i < formatString.length(); i++) {
    if(formatString[i] == 's' || formatString[i] == 'u'
        || formatString[i] == 'p')
    {
      stringOk = false;
    }
  }
  if(stringOk) {
    pltAppPtr->SetFormatString(formatString);
    XClearWindow(GAptr->PDisplay(), XtWindow(wPixArea));
    Render(datasetRegion[maxDrawnLevel], amrPicturePtr, pltAppPtr,
	   hDIR, vDIR, sDIR);
    pltAppPtr->GetPalettePtr()->SetFormat(formatString);
    pltAppPtr->GetPalettePtr()->Redraw();  // change palette numbers
  }
  DoExpose(false);
}


// -------------------------------------------------------------------
void Dataset::DoQuitButton() {
  pltAppPtr->QuitDataset();
  delete this;
}


// -------------------------------------------------------------------
void Dataset::DoColorButton() {
  DoExpose(false);
}


// -------------------------------------------------------------------
void Dataset::DoRaise() {
  XtPopup(wDatasetTopLevel, XtGrabNone);
  XMapRaised(XtDisplay(wDatasetTopLevel), XtWindow(wDatasetTopLevel));
}


// -------------------------------------------------------------------
void Dataset::DoExpose(int fromExpose) {

    if(fromExpose && drags) {
        --drags;
        return;
    }
    
    dataServicesPtr = pltAppPtr->GetDataServicesPtr();
    const AmrData &amrData = dataServicesPtr->AmrDataRef();
    if(noData) {
        cout << "_in Dataset::DoExpose:  noData" << endl;
        XSetBackground(XtDisplay(wPixArea), GAptr->PGC(), blackIndex);
        XSetForeground(XtDisplay(wPixArea), GAptr->PGC(), whiteIndex);
        XDrawString(XtDisplay(wPixArea), XtWindow(wPixArea), GAptr->PGC(),
                    2, pixSizeY-5, dataString, strlen(dataString));
    } else {
        unsigned int lev, stringCount;
        Box temp, dataBox;
        Widget hScrollBar, vScrollBar;
        Dimension wdth, hght;
        
        XtVaGetValues(wScrollArea, XmNhorizontalScrollBar, &hScrollBar,
                      XmNverticalScrollBar, &vScrollBar, NULL);
        
#ifndef SCROLLBARERROR
        int hSliderSize, hIncrement, hPageIncrement;
        int vSliderSize, vIncrement, vPageIncrement;
        
        XmScrollBarGetValues(hScrollBar, &hScrollBarPos, &hSliderSize,
                             &hIncrement, &hPageIncrement);
        XmScrollBarGetValues(vScrollBar, &vScrollBarPos, &vSliderSize,
                             &vIncrement, &vPageIncrement);
#endif
        
        Dimension scrollAreaSpacing;
        XtVaGetValues(wScrollArea,
                      XmNwidth, &width,
                      XmNheight, &height,
                      XmNspacing, &scrollAreaSpacing,
                      NULL);
        
        xh = (int) hScrollBarPos - dataItemWidth;
        yv = (int) vScrollBarPos - dataItemWidth;
        
        int hScrollBarBuffer(32);
        int vScrollBarBuffer(32);
        if(pixSizeY == vSliderSize) {  // the vertical scroll bar is not visible
          vScrollBarBuffer = scrollAreaSpacing;
        }
        if(pixSizeX == hSliderSize) {  // the horizontal scroll bar is not visible
          hScrollBarBuffer = scrollAreaSpacing;
        }
        
        
        hIndexAreaHeight = indexHeight;
        hIndexAreaEnd    = Min((int) pixSizeY,
			       vScrollBarPos + height - hScrollBarBuffer);
        hIndexAreaStart  = hIndexAreaEnd + 1 - hIndexAreaHeight;
        vIndexAreaWidth  = indexWidth;
        vIndexAreaEnd    = Min((int) pixSizeX,
			       hScrollBarPos + width - vScrollBarBuffer);
        vIndexAreaStart  = vIndexAreaEnd + 1 - vIndexAreaWidth;
        
        XtVaGetValues(wScrollArea, XmNwidth, &wdth, XmNheight, &hght, NULL);
        
        XClearWindow(GAptr->PDisplay(), XtWindow(wPixArea));
        
        int min_level(minDrawnLevel);
        int max_level(maxDrawnLevel);
        int level_diff(max_level - min_level);
        
        // draw grid structure for entire region 
        for(lev = minDrawnLevel; lev <= maxDrawnLevel; ++lev) {
            for(int iBox = 0; iBox < amrData.boxArray(lev).length(); ++iBox) {
                temp = amrData.boxArray(lev)[iBox];
                if(datasetRegion[lev].intersects(temp)) {
                    temp &= datasetRegion[lev];
                    dataBox = temp;
                    temp.refine(CRRBetweenLevels(lev,
                                            maxDrawnLevel, amrData.RefRatio()));
                    temp.shift(hDIR, -datasetRegion[maxDrawnLevel].smallEnd(hDIR)); 
                    temp.shift(vDIR, -datasetRegion[maxDrawnLevel].smallEnd(vDIR));
                    DrawGrid(temp.smallEnd(hDIR) * dataItemWidth,
                           (pixSizeY-1 - (temp.bigEnd(vDIR)+1) * CHARACTERHEIGHT)
                           -((level_diff+1)*hIndexAreaHeight),
                           (temp.bigEnd(hDIR)+1) * dataItemWidth,
                           (pixSizeY-1 - temp.smallEnd(vDIR) * CHARACTERHEIGHT)
                           -((level_diff+1)*hIndexAreaHeight),
                           CRRBetweenLevels(lev, maxDrawnLevel, amrData.RefRatio()),
                           whiteIndex, blackIndex);
                                        
                }
            }
        }
        if(dragging) {
            DrawIndices();
            return;
        }
        
        int xloc, yloc;
        int xh((int) hScrollBarPos - dataItemWidth);
        int yv((int) vScrollBarPos - dataItemWidth);
        
        // draw data strings
        if(XmToggleButtonGetState(wColorButton)) {
            for(int lvl = minDrawnLevel; lvl<=maxDrawnLevel; lvl++) {
                for(stringCount=0; stringCount<myStringCount[lvl]; stringCount++) {
                    xloc = myDataStringArray[lvl][stringCount].xloc;
                    yloc = myDataStringArray[lvl][stringCount].yloc -
			  ((maxDrawnLevel-minDrawnLevel+1)*hIndexAreaHeight);
#ifndef SCROLLBARERROR
                    if(myDataStringArray[lvl][stringCount].olflag >= maxDrawnLevel  &&
                       xloc > xh && yloc > yv &&
                       xloc < hScrollBarPos+wdth && yloc < vScrollBarPos+hght)
#else
                    if(myDataStringArray[lvl][stringCount].olflag >= maxDrawnLevel)
#endif
                { 
                    XSetForeground(XtDisplay(wPixArea), GAptr->PGC(), 
                                   myDataStringArray[lvl][stringCount].color); 
                    XDrawString(XtDisplay(wPixArea), XtWindow(wPixArea),
                                GAptr->PGC(), xloc, yloc,
                                myDataStringArray[lvl][stringCount].ds,
                                myDataStringArray[lvl][stringCount].dslen);
                }
                }
            }
        } else {
            XSetForeground(XtDisplay(wPixArea), GAptr->PGC(), whiteIndex);
            for(int lvl = minDrawnLevel; lvl<=maxDrawnLevel; lvl++) {
                for(stringCount=0; stringCount<myStringCount[lvl]; stringCount++) {
                    xloc = myDataStringArray[lvl][stringCount].xloc;
                    yloc = myDataStringArray[lvl][stringCount].yloc -
			   ((max_level-min_level+1)*hIndexAreaHeight);
#ifndef SCROLLBARERROR
                    if(myDataStringArray[lvl][stringCount].olflag >= maxDrawnLevel &&
                       xloc > xh && yloc > yv &&
                       xloc < hScrollBarPos+wdth && yloc < vScrollBarPos+hght)
#else
                    if(myDataStringArray[lvl][stringCount].olflag >= maxDrawnLevel)
#endif
                    {
                        XDrawString(XtDisplay(wPixArea), XtWindow(wPixArea),
                                    GAptr->PGC(), xloc, yloc,
                                    myDataStringArray[lvl][stringCount].ds,
                                    myDataStringArray[lvl][stringCount].dslen);
                    }
                }
            }
        }
        
        DrawIndices();
    }
}  // end DoExpose


// -------------------------------------------------------------------
void Dataset::DrawIndices() {
    int xloc, yloc;
    unsigned int stringCount;
    GC gc = GAptr->PGC();
    Display *display = XtDisplay(wPixArea);
    Window dataWindow = XtWindow(wPixArea);
    int levelRange = maxDrawnLevel - minDrawnLevel;
    
    XSetForeground(display, gc, whiteIndex);
   for(int count = 0; count<= levelRange; ++count) {
    // horizontal
        XFillRectangle(display, dataWindow, gc, hScrollBarPos,
                       hIndexAreaStart-(hIndexAreaHeight*count), 
                       Min((unsigned int) width,  pixSizeX),
                       hIndexAreaHeight);
        // vertical
        XFillRectangle(display, dataWindow, gc, 
                       vIndexAreaStart-(vIndexAreaWidth*count),
                       vScrollBarPos, vIndexAreaWidth,
                       Min((unsigned int) height, pixSizeY));
    }
    const AmrData &amrData = dataServicesPtr->AmrDataRef();

    for(int level = minDrawnLevel; level<= maxDrawnLevel; ++level) {
       Box temp(datasetRegion[level]);
       int count(level - minDrawnLevel);

       temp.refine(CRRBetweenLevels(level, maxDrawnLevel, amrData.RefRatio()));
       temp.shift(hDIR, -datasetRegion[maxDrawnLevel].smallEnd(hDIR)); 
       temp.shift(vDIR, -datasetRegion[maxDrawnLevel].smallEnd(vDIR));

       double dBoxSize(CRRBetweenLevels(level, maxDrawnLevel, amrData.RefRatio()));
       int boxSize((ceil(dBoxSize)-dBoxSize >= 0.5 ?
                      floor(dBoxSize): ceil(dBoxSize)));

       // draw the horizontal box index grid -- on top of the white background.
       DrawGrid(temp.smallEnd(hDIR) * dataItemWidth, 
                hIndexAreaStart-(count*hIndexAreaHeight)-1,
                vIndexAreaStart-(count*vIndexAreaWidth), 
                hIndexAreaEnd-(count*hIndexAreaHeight),
                (boxSize)*dataItemWidth, hIndexAreaHeight, 
                blackIndex, whiteIndex);
       // draw the vertical box index grid
       int vStart(fmod(datasetRegion[maxDrawnLevel].bigEnd(vDIR)+1, boxSize));
       vStart = (vStart != 0? vStart - boxSize:vStart);
       DrawGrid(vIndexAreaStart-(count*vIndexAreaWidth)-1,
                vStart*CHARACTERHEIGHT,
                vIndexAreaEnd-(count*vIndexAreaWidth), 
                hIndexAreaStart-(count*hIndexAreaHeight), 
                vIndexAreaWidth,boxSize*CHARACTERHEIGHT,
                blackIndex, whiteIndex);

       
       XSetForeground(display, gc, blackIndex);
       // draw the corner axis labels
       XDrawLine(display, dataWindow, gc, vIndexAreaStart, hIndexAreaStart,
                 vIndexAreaEnd,   hIndexAreaEnd - 1);
       // frame the corner box
       XDrawLine(display, dataWindow, gc, vIndexAreaStart,   hIndexAreaEnd - 1,
                 vIndexAreaEnd,     hIndexAreaEnd - 1);
       XDrawLine(display, dataWindow, gc, vIndexAreaEnd - 1, hIndexAreaStart,
                 vIndexAreaEnd - 1, hIndexAreaEnd);
       XDrawString(display, dataWindow, gc,
                   vIndexAreaStart + hStringOffset,
                   hIndexAreaEnd   + vStringOffset,
                   hAxisString.c_str(), hAxisString.length());
       XDrawString(display, dataWindow, gc,
                   vIndexAreaStart + (indexWidth/2)  + hStringOffset,
                   hIndexAreaEnd   - (indexHeight/2) + vStringOffset,
                   vAxisString.c_str(), vAxisString.length());
       
       // draw the box indices
       // horizontal
       yloc = hIndexAreaEnd + vStringOffset-(count*hIndexAreaHeight);
       for(stringCount = 0; stringCount < datasetRegion[level].length(hDIR);
	   ++stringCount)
       {
           xloc = hIndexArray[level][stringCount].xloc;
           if((xloc > xh) &&
	      (xloc < (vIndexAreaStart-(count*vIndexAreaWidth) - (indexWidth / 3))))
	   {
               XDrawString(display, dataWindow, gc, xloc, yloc,
                           hIndexArray[level][stringCount].ds, 
                           hIndexArray[level][stringCount].dslen); 
           }
       } // end for(...)
       // vertical
       xloc = vIndexAreaStart + hStringOffset-(count*vIndexAreaWidth);
       for(stringCount = 0; stringCount < datasetRegion[level].length(vDIR);
	   ++stringCount)
       {
           yloc = vIndexArray[level][stringCount].yloc;
           if((yloc > yv) && (yloc < hIndexAreaStart-(count*hIndexAreaHeight))) {
               XDrawString(display, dataWindow, gc, xloc, yloc,
                           vIndexArray[level][stringCount].ds, 
                           vIndexArray[level][stringCount].dslen);
           }
       } 
   }    //end for( int level . . .
}  // end DrawIndices


// -------------------------------------------------------------------
void Dataset::CBDoExposeDataset(Widget, XtPointer client_data, XEvent *, Boolean *)
{
    XEvent nextEvent;
    Dataset *dset = (Dataset *) client_data;
    while(XCheckTypedWindowEvent(dset->GAptr->PDisplay(),
                                 XtWindow(dset->wPixArea), Expose, &nextEvent))
    {
      if(dset->drags) {
        dset->drags--;
      }
  }
  dset->DoExpose(true);
}


// -------------------------------------------------------------------
void Dataset::CBScrolling(Widget, XtPointer client_data, XtPointer) {
  Dataset *dset = (Dataset *) client_data;
  dset->dragging = true;
  dset->drags++;
  dset->DoExpose(false);
}

// -------------------------------------------------------------------
void Dataset::CBEndScrolling(Widget, XtPointer client_data, XtPointer) {
  Dataset *dset = (Dataset *) client_data;
  dset->dragging = false;
  dset->DoExpose(false);
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
