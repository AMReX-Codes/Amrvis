// -------------------------------------------------------------------
//  Dataset.C
// -------------------------------------------------------------------

const int CHARACTERWIDTH  = 13;
const int DATAITEMHEIGHT  = 22;

#include "Dataset.H"
#include "PltApp.H"
#include "DataServices.H"
#include "GlobalUtilities.H"
#include <float.h>


// -------------------------------------------------------------------
Dataset::Dataset(Widget top, const Box &alignedRegion, AmrPicture *apptr,
		 PltApp *pltappptr, int hdir, int vdir, int sdir)
{

  // alignedRegion is defined on the maxAllowableLevel

  int i;
  stringOk = true;
  hDIR = hdir;
  vDIR = vdir;
  sDIR = sdir;
  showColor = true;
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
  XmStringFree(sFormatString);

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
  wColorButton = XmCreatePushButton(wDatasetTools, "Color", args, i);
  XtAddCallback(wColorButton, XmNactivateCallback, &Dataset::CBColorButton,
		(XtPointer) this);

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

  // ************************************************ data area 
  wScrollArea = XtVaCreateManagedWidget("scrollArea",
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

  Render(alignedRegion, amrPicturePtr, pltAppPtr, hdir, vdir, sdir);
}  // end Dataset::Dataset


// -------------------------------------------------------------------
Dataset::~Dataset() {
  delete GAptr;
  delete [] datasetRegion;
  delete [] dataStringArray;
  XtDestroyWidget(wDatasetTopLevel);
}


// -------------------------------------------------------------------
void Dataset::Render(const Box &alignedRegion, AmrPicture *apptr,
		     PltApp *pltappptr, int hdir, int vdir, int sdir)
{
  int lev, i, c, d, stringCount;
  Box temp, dataBox;
  Real *dataPoint; 

  hDIR = hdir;
  vDIR = vdir;
  sDIR = sdir;
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
  int blackIndex(palptr->BlackIndex());
  //int whiteIndex(palptr->WhiteIndex());
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
  for(lev = 0; lev <= maxAllowableLevel; ++lev) {
    DataServices::Dispatch(DataServices::FillVarOneFab, dataServicesPtr,
			   &dataFab[lev], dataFab[lev].box(),
			   lev, amrPicturePtr->CurrentDerived());
  //
    for(int iBox = 0; iBox < amrData.boxArray(lev).length(); ++iBox) {
      temp = amrData.boxArray(lev)[iBox];
      if(datasetRegion[lev].intersects(temp)) {
	int ddl;
	temp &= datasetRegion[lev];
	dataBox = temp;
        FArrayBox dataFabTemp(dataBox, 1);
	dataFabTemp.copy(dataFab[lev]);
		  //amrPlotPtr->StateNumber(amrPicturePtr->CurrentDerived()), 0, 1);
	dataPoint = dataFabTemp.dataPtr();
	stringCount += dataBox.length(vDIR) * dataBox.length(hDIR);
	for(d = 0; d < dataBox.length(vDIR); ++d) {
	  ddl = d*dataBox.length(hDIR);
	  for(c = 0; c < dataBox.length(hDIR); ++c) {
	    sprintf(dataString, formatString.c_str(), dataPoint[c+ddl]);
	    largestWidth = Max((int)strlen(dataString), largestWidth);
	  }
	}
        //delete dataFab;
      }
    }
  //
  }

  dataItemWidth = largestWidth * CHARACTERWIDTH;
  if(Verbose()) {
    cout << stringCount << " data points" << endl;
  }
  numStrings = stringCount;
  dataStringArray = new StringLoc[numStrings];
  int ns;
  for(ns = 0; ns < numStrings; ns++) {
    dataStringArray[ns].olflag = false;
  }
  if(dataStringArray == NULL) {
    cout << "Error in Dataset::Render:  out of memory" << endl;
    return;
  }

  // determine size of data area

  pixSizeX = datasetRegion[maxAllowableLevel].length(hDIR) * dataItemWidth;
  pixSizeY = datasetRegion[maxAllowableLevel].length(vDIR) * DATAITEMHEIGHT;

  // create StringLoc array and define color scheme 
  if(pixSizeX == 0 || pixSizeY == 0) {
    noData = true;
    sprintf (dataString, "No intersection.");
    pixSizeX = strlen(dataString) * CHARACTERWIDTH;
    pixSizeY = DATAITEMHEIGHT+5;
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
    int lastLevLow = 0, lastLevHigh = 0;
    int csm1 = colorSlots - 1;
    Real datamin = amrPicturePtr->GetWhichMin();
    Real globalDiff = amrPicturePtr->GetWhichMax() - amrPicturePtr->GetWhichMin();
    Real oneOverGlobalDiff;
    if(globalDiff < FLT_MIN) {
      oneOverGlobalDiff = 0.0;  // so we dont divide by zero
    } else {
      oneOverGlobalDiff = 1.0 / globalDiff;
    }

    for(lev = 0; lev <= maxAllowableLevel; ++lev) {
      for(int iBox = 0; iBox < amrData.boxArray(lev).length(); ++iBox) {
        temp = amrData.boxArray(lev)[iBox];
    //
        if(datasetRegion[lev].intersects(temp)) {
	  temp &= datasetRegion[lev];
	  dataBox = temp;
	  temp.refine(CRRBetweenLevels(lev,
				 maxAllowableLevel, amrData.RefRatio()));
	  temp.shift(hDIR, -datasetRegion[maxAllowableLevel].smallEnd(hDIR)); 
	  temp.shift(vDIR, -datasetRegion[maxAllowableLevel].smallEnd(vDIR)); 
          //dataFab = new FArrayBox(dataBox, 1);
          FArrayBox dataFabTemp(dataBox, 1);
	  dataFabTemp.copy(dataFab[lev]);
		  //amrPlotPtr->StateNumber(amrPicturePtr->CurrentDerived()), 0, 1);
	  dataPoint = dataFabTemp.dataPtr();
	  int ddl;
	  int crr = CRRBetweenLevels(lev, maxAllowableLevel, amrData.RefRatio());
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
		    (temp.smallEnd(vDIR) + d * crr) * DATAITEMHEIGHT - 4;

	        for(i = lastLevLow; i < lastLevHigh; i++) { // remove overlap
	 	  if(dataStringArray[i].xloc ==
				       dataStringArray[stringCount].xloc
		     && dataStringArray[i].yloc ==
			               dataStringArray[stringCount].yloc)
		  {
		    dataStringArray[i].olflag = true;
                  }
	        }	

	        strcpy(dataStringArray[stringCount].ds, dataString);
	        dataStringArray[stringCount].dslen = strlen(dataString);

	        ++stringCount;

	      }  // end for(c...)
    	    }  // end for(d...)

	  //delete dataFab;

        }  // end if(datasetRegion[lev].intersects(temp))
      }
      lastLevLow = lastLevHigh;
      lastLevHigh = stringCount; 
    //
    }
  }  // end if(pixSizeX...)

  XSetWindowColormap(GAptr->PDisplay(), XtWindow(wDatasetTopLevel),
	pltAppPtr->GetPalettePtr()->GetColormap());
  XSetWindowColormap(GAptr->PDisplay(), XtWindow(wPixArea),
	pltAppPtr->GetPalettePtr()->GetColormap());

  XSetForeground(XtDisplay(wPixArea), GAptr->PGC(), blackIndex);

}  // end Dataset::Render


// -------------------------------------------------------------------
void Dataset::DrawGrid(int startX, int startY, int endX, int endY, int refRatio) {
  int i;
  Palette *palptr = pltAppPtr->GetPalettePtr();
  int whiteIndex(palptr->WhiteIndex());
  int blackIndex(palptr->BlackIndex());

  XSetBackground(XtDisplay(wPixArea), GAptr->PGC(), blackIndex);
  XSetForeground(XtDisplay(wPixArea), GAptr->PGC(), whiteIndex);

  XDrawLine(XtDisplay(wPixArea), XtWindow(wPixArea), GAptr->PGC(),
	    startX + 1, startY, startX + 1, endY);
  for(i = startX; i <= endX; i += dataItemWidth * refRatio) {
	XDrawLine(XtDisplay(wPixArea), XtWindow(wPixArea), GAptr->PGC(),
		  i, startY, i, endY);
  }
  XDrawLine(XtDisplay(wPixArea), XtWindow(wPixArea), GAptr->PGC(),
	    endX - 1, startY, endX - 1, endY);

  XDrawLine(XtDisplay(wPixArea), XtWindow(wPixArea), GAptr->PGC(),
	    startX, startY + 1, endX, startY + 1);
  for(i = startY; i <= endY; i += DATAITEMHEIGHT * refRatio) {
	XDrawLine(XtDisplay(wPixArea), XtWindow(wPixArea), GAptr->PGC(),
		  startX, i, endX, i);
  }
  XDrawLine(XtDisplay(wPixArea), XtWindow(wPixArea), GAptr->PGC(),
	    startX, endY - 1, endX, endY - 1);
}


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
  Palette *palptr = pltAppPtr->GetPalettePtr();
  int whiteIndex(palptr->WhiteIndex());
  int hplot, vplot; 
  int xcell((int) (cbs->event->xbutton.x) / dataItemWidth);
  int ycell((int) (cbs->event->xbutton.y) / DATAITEMHEIGHT);
  Box pictureBox(amrPicturePtr->GetSubDomain()[maxAllowableLevel]);
  Box regionBox(datasetRegion[maxAllowableLevel]);

  if(xcell >= 0 && xcell < pixSizeX/dataItemWidth &&
     ycell >= 0 && ycell < pixSizeY/DATAITEMHEIGHT)
  {
    if(amrPicturePtr->GetMyView() == XY) {
      hplot = regionBox.smallEnd(XDIR) - pictureBox.smallEnd(XDIR) + xcell;
      vplot = regionBox.smallEnd(YDIR) - pictureBox.smallEnd(YDIR) +
	      regionBox.length(YDIR)-1 - ycell;
    }

    if(amrPicturePtr->GetMyView() == XZ) {
      hplot = regionBox.smallEnd(XDIR) - pictureBox.smallEnd(XDIR) + xcell;
      vplot = regionBox.smallEnd(ZDIR) - pictureBox.smallEnd(ZDIR) +
	      regionBox.length(ZDIR)-1 - ycell;
    }

    if(amrPicturePtr->GetMyView() == YZ) {
      hplot = regionBox.smallEnd(YDIR) - pictureBox.smallEnd(YDIR) + xcell;
      vplot = regionBox.smallEnd(ZDIR) - pictureBox.smallEnd(ZDIR) +
	      regionBox.length(ZDIR)-1 - ycell;
    }
    amrPicturePtr->SetDatasetPoint(hplot, vplot, whiteIndex);
    amrPicturePtr->DoExposePicture();
  }

  if(cbs->event->xany.type == ButtonRelease) {
    amrPicturePtr->DatasetPointOff();
    amrPicturePtr->DoExposePicture();
  }
} 


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
    Render(datasetRegion[maxAllowableLevel], amrPicturePtr, pltAppPtr,
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
  showColor = (showColor ? false : true);
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
  Palette *palptr = pltAppPtr->GetPalettePtr();
  int whiteIndex(palptr->WhiteIndex());
  int blackIndex(palptr->BlackIndex());
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
    int hScrollBarPos(0), vScrollBarPos(0);
    Dimension wdth, hght;

    XtVaGetValues(wScrollArea, XmNhorizontalScrollBar, &hScrollBar,
	          XmNverticalScrollBar, &vScrollBar, NULL);
#ifndef SCROLLBARERROR
    XmScrollBarGetValues(hScrollBar, &hScrollBarPos, NULL, NULL, NULL);
    XmScrollBarGetValues(vScrollBar, &vScrollBarPos, NULL, NULL, NULL);
#endif
    XtVaGetValues(wScrollArea, XmNwidth, &wdth, XmNheight, &hght, NULL);

    XClearWindow(GAptr->PDisplay(), XtWindow(wPixArea));

    // draw grid structure for entire region 
    for(lev = 0; lev <= maxAllowableLevel; ++lev) {
      for(int iBox = 0; iBox < amrData.boxArray(lev).length(); ++iBox) {
        temp = amrData.boxArray(lev)[iBox];
        if(datasetRegion[lev].intersects(temp)) {
	  temp &= datasetRegion[lev];
	  dataBox = temp;
	  temp.refine(CRRBetweenLevels(lev,
				 maxAllowableLevel, amrData.RefRatio()));
	  temp.shift(hDIR, -datasetRegion[maxAllowableLevel].smallEnd(hDIR)); 
	  temp.shift(vDIR, -datasetRegion[maxAllowableLevel].smallEnd(vDIR)); 
	  DrawGrid(temp.smallEnd(hDIR) * dataItemWidth,
		pixSizeY-1 - (temp.bigEnd(vDIR)+1) * DATAITEMHEIGHT,
		(temp.bigEnd(hDIR)+1) * dataItemWidth,
		pixSizeY-1 - temp.smallEnd(vDIR) * DATAITEMHEIGHT,
		CRRBetweenLevels(lev, maxAllowableLevel, amrData.RefRatio()));
        }
      }
    }

    if(dragging) {
      return;
    }

    int xloc, yloc;
    int xh = (int) hScrollBarPos - dataItemWidth;
    int yv = (int) vScrollBarPos - dataItemWidth;

  // draw data strings
  if(showColor) {
    for(stringCount=0; stringCount<numStrings; stringCount++) {
      xloc = dataStringArray[stringCount].xloc;
      yloc = dataStringArray[stringCount].yloc;
#ifndef SCROLLBARERROR
      if(dataStringArray[stringCount].olflag == false  &&
	xloc > xh && yloc > yv &&
	xloc < hScrollBarPos+wdth && yloc < vScrollBarPos+hght)
#else
      if(dataStringArray[stringCount].olflag == false)
#endif
      {
  	XSetForeground(XtDisplay(wPixArea), GAptr->PGC(), 
       		         dataStringArray[stringCount].color); 
        XDrawString(XtDisplay(wPixArea), XtWindow(wPixArea),
			GAptr->PGC(), xloc, yloc,
			dataStringArray[stringCount].ds,
			dataStringArray[stringCount].dslen);
      }
    }

  } else {
    XSetForeground(XtDisplay(wPixArea), GAptr->PGC(), whiteIndex);
    for(stringCount=0; stringCount<numStrings; stringCount++) {
      xloc = dataStringArray[stringCount].xloc;
      yloc = dataStringArray[stringCount].yloc;
#ifndef SCROLLBARERROR
      if(dataStringArray[stringCount].olflag == false  &&
	xloc > xh && yloc > yv &&
	xloc < hScrollBarPos+wdth && yloc < vScrollBarPos+hght)
#else
      if(dataStringArray[stringCount].olflag == false)
#endif
      {
        XDrawString(XtDisplay(wPixArea), XtWindow(wPixArea),
			GAptr->PGC(), xloc, yloc,
			dataStringArray[stringCount].ds,
			dataStringArray[stringCount].dslen);
      }
    }
  }

  }
}  // end DoExpose


// -------------------------------------------------------------------
void Dataset::CBDoExposeDataset(Widget, XtPointer client_data,
				XEvent *, Boolean *)
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
