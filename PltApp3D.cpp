// -------------------------------------------------------------------
// PltApp3D.C 
// -------------------------------------------------------------------
#include "PltApp.H"

// -------------------------------------------------------------------
void PltApp::DoExposeTransDA() {
  if(autorender) {
     projPicturePtr->DrawPicture();
  } else {
    projPicturePtr->DrawBoxes();
  }
  if(labelAxes) {
    if(autorender) {
      projPicturePtr->MakeBoxes();
    }
    projPicturePtr->LabelAxes();
  }
}


// -------------------------------------------------------------------
void PltApp::DoTransInput(Widget w, XtPointer, XtPointer call_data) {
  Real temp;
  XmDrawingAreaCallbackStruct* cbs =
        (XmDrawingAreaCallbackStruct*) call_data;
  if(cbs->event->xany.type == ButtonPress) {
    servingButton = cbs->event->xbutton.button;
    startX = endX = cbs->event->xbutton.x;
    startY = endY = cbs->event->xbutton.y;
    acc = 0;
  }
  if(servingButton==1) {
    endX = cbs->event->xbutton.x;
    endY = cbs->event->xbutton.y;
    temp = viewTrans.GetRho()+(startY-endY)*0.003;
    if(temp > 6.282) {
      temp -= 6.282;
    }
    if(temp < 0.0) {
      temp = 6.282;
    }
    viewTrans.SetRho(temp);

    temp = viewTrans.GetTheta()+(endX-startX)*0.003;
    if(temp>6.282) {
      temp -= 6.282;
    }
    if(temp<0.0) {
      temp = 6.282;
    }
    viewTrans.SetTheta(temp);
    viewTrans.MakeTransform();

    if(autorender) {
      DoRender(w, NULL, NULL);
    } else {
      projPicturePtr->MakeBoxes();
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
    }

    DoExposeTransDA();
    acc += 1;
    if(acc==10) {
      startX = cbs->event->xbutton.x;
      startY = cbs->event->xbutton.y;
      acc = 0;
    }
  } 

  if(servingButton==2) {
    endX = cbs->event->xbutton.x;
    endY = cbs->event->xbutton.y;
    temp = viewTrans.GetPhi()+(endY-startY)*0.003;
    if(temp>6.282) {
      temp -= 6.282;
    }
    if(temp<0.0) {
      temp = 6.282;
    }
    viewTrans.SetPhi(temp);

    viewTrans.MakeTransform();

    if(autorender) {
      DoRender(w, NULL, NULL);
    } else {
      projPicturePtr->MakeBoxes();
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
    }
    DoExposeTransDA();
    acc += 1;
    if(acc==10) {
      startX = cbs->event->xbutton.x;
      startY = cbs->event->xbutton.y;
      acc = 0;
    }
  } 

  if(servingButton==3) {
    endX = cbs->event->xbutton.x;
    endY = cbs->event->xbutton.y;
    Real sx,sy,sz;
    viewTrans.GetScale(sx,sy,sz);
    temp = sx+(endY-startY)*0.003;
    if(temp<0.1) {
      temp = 0.1;
    }
    viewTrans.SetScale(temp, temp, temp);
    viewTrans.MakeTransform();

    if(autorender) {
      DoRender(w, NULL, NULL);
    } else {
      projPicturePtr->MakeBoxes();
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
    }
    DoExposeTransDA();
    acc += 1;
    if(acc==15) {
      startX = cbs->event->xbutton.x;
      startY = cbs->event->xbutton.y;
      acc = 0;
    }
  } 
  if(cbs->event->xany.type == ButtonRelease) {
    startX = endX = 0;
    startY = endY = 0;
    acc = 0;
  }
}  // end DoTransInput(...)


// -------------------------------------------------------------------
void PltApp::CBTransResize(Widget w, XtPointer client_data, XtPointer call_data) {
	PltApp *obj = (PltApp *) client_data;
	obj->DoTransResize(w, client_data, call_data);
}


// -------------------------------------------------------------------
void PltApp::DoAttach(Widget, XtPointer, XtPointer) {
  transDetached = false;
  XtRemoveCallback(wTransDA, XmNinputCallback, &PltApp::CBTransInput, NULL);
  XtRemoveCallback(wTransDA, XmNresizeCallback, &PltApp::CBTransResize, NULL);
  XtRemoveEventHandler(wTransDA, ExposureMask, false,
	(XtEventHandler) CBExposeTransDA, (XtPointer) this);

  XtDestroyWidget(wDetachTopLevel);

  XtManageChild(wOrient);
  XtManageChild(wLabelAxes);
#ifdef BL_VOLUMERENDER
  XtManageChild(wRender);
  XtManageChild(wAutoDraw);
  XtManageChild(wReadTransfer);
#endif
  XtManageChild(wDetach);

  wTransDA = XtVaCreateManagedWidget("transDA", xmDrawingAreaWidgetClass,
		wPlotArea,
		XmNtranslations,	XtParseTranslationTable(trans),
		XmNleftAttachment,	XmATTACH_WIDGET,
		//XmNleftWidget,		wPlotPlane[XPLANE],
		XmNleftWidget,		wScrollArea[XPLANE],
		XmNleftOffset,		WOFFSET,
		XmNtopAttachment,	XmATTACH_WIDGET,
		XmNtopWidget,		wOrient,
		XmNtopOffset,		WOFFSET,
		XmNrightAttachment,	XmATTACH_FORM,
		XmNrightOffset,		WOFFSET,
		XmNbottomAttachment,	XmATTACH_FORM,
		XmNbottomOffset,	WOFFSET,
		NULL);
  XtAddCallback(wTransDA, XmNinputCallback, &PltApp::CBTransInput,
		(XtPointer) this);
  XtAddCallback(wTransDA, XmNresizeCallback, &PltApp::CBTransResize,
		(XtPointer) this);
  XtAddEventHandler(wTransDA, ExposureMask, false,
  	(XtEventHandler) CBExposeTransDA,
	(XtPointer) this);
  projPicturePtr->SetDrawingArea(wTransDA);
#ifdef BL_VOLUMERENDER
    XmString sAutoDraw = XmStringCreateSimple("Autodraw");
    XtVaSetValues(wAutoDraw, XmNlabelString, sAutoDraw, NULL);
    XmStringFree(sAutoDraw);
#endif
  DoTransResize(wTransDA, NULL, NULL);
}


// -------------------------------------------------------------------
void PltApp::DoDetach(Widget, XtPointer, XtPointer) {
  int i;
  Position xpos, ypos;
  Dimension wdth, hght;

  transDetached = true;
  XtUnmanageChild(wTransDA);
  XtUnmanageChild(wOrient);
  XtUnmanageChild(wLabelAxes);
#ifdef BL_VOLUMERENDER
  XtUnmanageChild(wRender);
  XtUnmanageChild(wAutoDraw);
  XtUnmanageChild(wReadTransfer);
#endif
  XtUnmanageChild(wDetach);
  XtRemoveCallback(wTransDA, XmNinputCallback, &PltApp::CBTransInput, NULL);
  XtRemoveCallback(wTransDA, XmNresizeCallback, &PltApp::CBTransResize, NULL);
  XtRemoveEventHandler(wTransDA, ExposureMask, false,
	(XtEventHandler) CBExposeTransDA, (XtPointer) this);
  XtDestroyWidget(wTransDA);

  XtVaGetValues(wAmrVisTopLevel, XmNx, &xpos, XmNy, &ypos,
	XmNwidth, &wdth, XmNheight, &hght, NULL);

  aString outbuf = "XYZ ";
  outbuf += fileName;
  strcpy(buffer, outbuf.c_str());

  wDetachTopLevel = XtVaCreatePopupShell(buffer,
	topLevelShellWidgetClass, wAmrVisTopLevel,
	XmNwidth,		500,
	XmNheight,		500,
	XmNx,			xpos+wdth/2,
	XmNy,			ypos+hght/2,
	NULL);

  wDetachForm = XtVaCreateManagedWidget("detachform",
	xmFormWidgetClass, wDetachTopLevel,
	NULL);

  i=0;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);   i++;
  XtSetArg(args[i], XmNtopOffset, WOFFSET);    i++;
  XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNrightOffset, WOFFSET);      i++;
  XmString sAttach = XmStringCreateSimple("Attach");
  XtSetArg(args[i], XmNlabelString, sAttach);    i++;
  wAttach = XmCreatePushButton(wDetachForm, "attach", args, i);
  XmStringFree(sAttach);
  AddStaticCallback(wAttach, XmNactivateCallback, &PltApp::DoAttach);
  XtManageChild(wAttach);

  i=0;
  XmString sOrient = XmStringCreateSimple("0");
  XtSetArg(args[i], XmNlabelString, sOrient); i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM); i++;
  XtSetArg(args[i], XmNleftOffset, WOFFSET); i++;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM); i++;
  XtSetArg(args[i], XmNtopOffset, WOFFSET); i++;
  wDOrient = XmCreatePushButton(wDetachForm, "dorient", args, i);
  XmStringFree(sOrient);
  AddStaticCallback(wDOrient, XmNactivateCallback, &PltApp::DoOrient);
  XtManageChild(wDOrient);

  i=0;
  XmString sLabel = XmStringCreateSimple("XYZ");
  XtSetArg(args[i], XmNlabelString, sLabel); i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET); i++;
  XtSetArg(args[i], XmNleftWidget, wDOrient); i++;
  XtSetArg(args[i], XmNleftOffset, WOFFSET); i++;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM); i++;
  XtSetArg(args[i], XmNtopOffset, WOFFSET); i++;
  wDLabelAxes = XmCreatePushButton(wDetachForm, "dlabel", args, i);
  XmStringFree(sLabel);
  AddStaticCallback(wDLabelAxes, XmNactivateCallback, &PltApp::DoLabelAxes);
  XtManageChild(wDLabelAxes);

#ifdef BL_VOLUMERENDER
  i=0;
  XmString sRender = XmStringCreateSimple("Draw");
  XtSetArg(args[i], XmNlabelString, sRender); i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET); i++;
  XtSetArg(args[i], XmNleftWidget, wDLabelAxes); i++;
  XtSetArg(args[i], XmNleftOffset, WOFFSET); i++;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM); i++;
  XtSetArg(args[i], XmNtopOffset, WOFFSET); i++;
  wDRender = XmCreatePushButton(wDetachForm, "drender", args, i);
  XmStringFree(sRender);
  AddStaticCallback(wDRender, XmNactivateCallback, &PltApp::DoRender);
  XtManageChild(wDRender);

  i=0;
  XmString sAutoDraw;
  sAutoDraw = XmStringCreateSimple("AutoDraw");
  XtSetArg(args[i], XmNlabelString, sAutoDraw); i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET); i++;
  XtSetArg(args[i], XmNleftWidget, wDRender); i++;
  XtSetArg(args[i], XmNleftOffset, WOFFSET); i++;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM); i++;
  XtSetArg(args[i], XmNtopOffset, WOFFSET); i++;
  wDAutoDraw = XmCreatePushButton(wDetachForm, "dautodraw", args, i);
  XmStringFree(sAutoDraw);
  AddStaticCallback(wDAutoDraw, XmNactivateCallback, &PltApp::DoAutoDraw);
  XtManageChild(wDAutoDraw);

  i=0;
  XmString sReadTrans = XmStringCreateSimple("Trans");
  XtSetArg(args[i], XmNlabelString, sReadTrans); i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_WIDGET); i++;
  XtSetArg(args[i], XmNleftWidget, wDAutoDraw); i++;
  XtSetArg(args[i], XmNleftOffset, WOFFSET); i++;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM); i++;
  XtSetArg(args[i], XmNtopOffset, WOFFSET); i++;
  wDReadTransfer = XmCreatePushButton(wDetachForm, "dreadtrans", args, i);
  XmStringFree(sReadTrans);
  AddStaticCallback(wDReadTransfer, XmNactivateCallback,
                    &PltApp::DoReadTransferFile);
  XtManageChild(wDReadTransfer);
#endif

  wTransDA = XtVaCreateManagedWidget("detachDA", xmDrawingAreaWidgetClass,
		wDetachForm,
		XmNtranslations,	XtParseTranslationTable(trans),
                XmNleftAttachment,      XmATTACH_FORM,
		XmNleftOffset,		WOFFSET,
		XmNtopAttachment,	XmATTACH_WIDGET,
		XmNtopWidget,		wDOrient,
		XmNtopOffset,		WOFFSET,
		XmNrightAttachment,	XmATTACH_FORM,
		XmNrightOffset,		WOFFSET,
		XmNbottomAttachment,	XmATTACH_FORM,
		XmNbottomOffset,	WOFFSET,
		NULL);
  XtAddCallback(wTransDA, XmNinputCallback, &PltApp::CBTransInput,
		(XtPointer) this);
  projPicturePtr->SetDrawingArea(wTransDA);

  XtPopup(wDetachTopLevel, XtGrabNone);
  XSetWindowColormap(GAptr->PDisplay(), XtWindow(wDetachTopLevel),
        pltPaletteptr->GetColormap());
  XSetWindowColormap(GAptr->PDisplay(), XtWindow(wTransDA),
        pltPaletteptr->GetColormap());

  XtAddCallback(wTransDA, XmNresizeCallback, &PltApp::CBTransResize,
		(XtPointer) this);
  XtAddEventHandler(wTransDA, ExposureMask, false,
  	(XtEventHandler) CBExposeTransDA, (XtPointer) this);

  DoTransResize(wTransDA, NULL, NULL);
}


// -------------------------------------------------------------------
void PltApp::DoTransResize(Widget w, XtPointer, XtPointer) {
  Dimension wdth, hght;

  XSetWindowColormap(GAptr->PDisplay(), XtWindow(wTransDA),
                     pltPaletteptr->GetColormap());

  XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
  XtVaGetValues(wTransDA, XmNwidth, &wdth, XmNheight, &hght, NULL);
  daWidth = wdth;
  daHeight = hght;
  projPicturePtr->SetDrawingAreaDimensions(daWidth, daHeight);
  if(autorender) {
    DoRender(w, NULL, NULL);
  } else {
    projPicturePtr->MakeBoxes();
  }
  DoExposeTransDA();
}


// -------------------------------------------------------------------
void PltApp::DoReadTransferFile(Widget, XtPointer, XtPointer) {
  VolRender *volRender = projPicturePtr->GetVolRenderPtr();
  projPicturePtr->ReadTransferFile("vpramps.dat");

  if( ! volRender->SWFDataAllocated()) {
    return;
  }
  if( ! volRender->SWFDataValid()) {
    return;
  }
  if(volRender->VPDataValid()) {
    volRender->MakeVPData();    // reclassify the data
  }
  projPicturePtr->MakePicture();

  DoExposeTransDA();
}


// -------------------------------------------------------------------
void PltApp::Clear() {
  XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
  autorender = false;
  viewTrans.SetRho(0.0);
  viewTrans.SetTheta(0.0);
  viewTrans.SetPhi(0.0);
  viewTrans.MakeTransform();
  DoExposeTransDA();
}


// -------------------------------------------------------------------
void PltApp::DoAutoDraw(Widget w, XtPointer, XtPointer) {
#ifdef BL_VOLUMERENDER
  XmString sAutoDraw;
  sAutoDraw = XmStringCreateSimple("Autodraw");
  if(autorender) {
    autorender = false;
    projPicturePtr->MakeBoxes();
  } else {
    autorender = true;
    DoRender(w, NULL, NULL);
  }
  if(transDetached) {
    XtVaSetValues(wDAutoDraw, XmNlabelString, sAutoDraw, NULL);
  } else {
    XtVaSetValues(wAutoDraw, XmNlabelString, sAutoDraw, NULL);
  }
  XmStringFree(sAutoDraw);

  XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
  DoExposeTransDA();
#endif
}


// -------------------------------------------------------------------
void PltApp::DoOrient(Widget w, XtPointer, XtPointer) {
  viewTrans.SetRho(0.0);
  viewTrans.SetTheta(0.0);
  viewTrans.SetPhi(0.0);
  viewTrans.MakeTransform();
  if(autorender) {
    DoRender(w, NULL, NULL);
  } else {
    projPicturePtr->MakeBoxes();
  }
  XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
  DoExposeTransDA();
}


// -------------------------------------------------------------------
void PltApp::CBTransInput(Widget w, XtPointer client_data, XtPointer call_data) {
  PltApp *obj = (PltApp *) client_data;
  obj->DoTransInput(w, client_data, (XmDrawingAreaCallbackStruct *) call_data);
}


// -------------------------------------------------------------------
void PltApp::DoLabelAxes(Widget, XtPointer, XtPointer) {
  labelAxes = (labelAxes ? false : true);
  XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
  DoExposeTransDA();
}


/*
// -------------------------------------------------------------------
void PltApp::WriteSWFData(const aString &filenamebase) {
  cout << "_in WriteSWFData:  filenamebase = " << filenamebase << endl;
  VolRender *volRender = projPicturePtr->GetVolRenderPtr();
  //projPicturePtr->AllocateSWFData();
  int iPaletteStart = pltPaletteptr->PaletteStart();
  int iPaletteEnd   = pltPaletteptr->PaletteEnd();
  int iBlackIndex   = pltPaletteptr->BlackIndex();
  int iWhiteIndex   = pltPaletteptr->WhiteIndex();
  int iColorSlots   = pltPaletteptr->ColorSlots();
  volRender->MakeSWFData(dataServicesPtr,
			 globalMin, globalMax,
			 currentDerived, 
			 iPaletteStart, iPaletteEnd,
			 iBlackIndex, iWhiteIndex,
			 iColorSlots);
  volRender->WriteSWFData(filenamebase);
}
*/


// -------------------------------------------------------------------
void PltApp::DoRender(Widget, XtPointer, XtPointer) {
  //XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
  //if( ! projPicturePtr->SWFDataAllocated()) {
    //projPicturePtr->AllocateSWFData();
  //}
  VolRender *volRender = projPicturePtr->GetVolRenderPtr();
  if( ! volRender->SWFDataValid()) {
    int iPaletteStart = pltPaletteptr->PaletteStart();
    int iPaletteEnd   = pltPaletteptr->PaletteEnd();
    int iBlackIndex   = pltPaletteptr->BlackIndex();
    int iWhiteIndex   = pltPaletteptr->WhiteIndex();
    int iColorSlots   = pltPaletteptr->ColorSlots();
    Real minUsing(amrPicturePtrArray[ZPLANE]->MinUsing());
    Real maxUsing(amrPicturePtrArray[ZPLANE]->MaxUsing());
    volRender->MakeSWFData(dataServicesPtr,
				minUsing, maxUsing,
				currentDerived, 
				iPaletteStart, iPaletteEnd,
				iBlackIndex, iWhiteIndex,
				iColorSlots);
  }
  if( ! volRender->VPDataValid()) {
    volRender->MakeVPData();
  }
  if( ! autorender) {
    projPicturePtr->MakeBoxes();
  }
  projPicturePtr->MakePicture();
  projPicturePtr->DrawPicture();
}


// -------------------------------------------------------------------
void CBExposeTransDA(Widget, XtPointer client_data, XExposeEvent) {
  ((PltApp *) client_data)->DoExposeTransDA();
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
