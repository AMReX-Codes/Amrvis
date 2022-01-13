// ---------------------------------------------------------------
// PltApp3D.cpp
// ---------------------------------------------------------------
#include <PltApp.H>
#include <PltAppState.H>
#include <ProjectionPicture.H>
#include <Quaternion.H>

#include <Xm/Xm.h>
#include <Xm/Protocols.h>
#include <Xm/ToggleBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/LabelG.h>
#include <Xm/Form.h>
#include <Xm/DrawingA.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <Xm/FileSB.h>

const unsigned long openingLW(100);
const unsigned long savingLW(101);
const Real oo64(1.0 / 64.0);
Widget wTempLabel;

using std::cout;
using std::cerr;
using std::endl;

using namespace amrex;


// -------------------------------------------------------------------
void PltApp::DoExposeTransDA(Widget, XtPointer, XtPointer) {
  int minDrawnLevel(pltAppState->MinDrawnLevel());
  int maxDrawnLevel(pltAppState->MaxDrawnLevel());

#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  if(XmToggleButtonGetState(wAutoDraw)) {
    projPicturePtr->DrawPicture();
  } else {
    projPicturePtr->DrawBoxes(minDrawnLevel, maxDrawnLevel);
  }

  if(labelAxes) {
    if(XmToggleButtonGetState(wAutoDraw)) {
      projPicturePtr->MakeBoxes();
    }
    projPicturePtr->LabelAxes();
  }
#else
  projPicturePtr->DrawBoxes(minDrawnLevel, maxDrawnLevel);
  if(labelAxes) {
    projPicturePtr->LabelAxes();
  }
#endif
}


// -------------------------------------------------------------------
void PltApp::DoTransInput(Widget /*w*/, XtPointer, XtPointer call_data) {
  Real temp, scaledLBS(projPicturePtr->LongestBoxSide() * oo64);
  AmrQuaternion quatRotation, quatRotation2;
  AmrQuaternion newRotation, newRotation2;

  XmDrawingAreaCallbackStruct* cbs = (XmDrawingAreaCallbackStruct *) call_data;
  if(cbs->event->xany.type == ButtonPress) {
    servingButton = cbs->event->xbutton.button;
    startX = endX = cbs->event->xbutton.x;
    startY = endY = cbs->event->xbutton.y;
    acc = 0;
  }
  if(servingButton == 1) {
    endX = cbs->event->xbutton.x;
    endY = cbs->event->xbutton.y;
    if(cbs->event->xbutton.state & ShiftMask) {
      viewTrans.MakeTranslation(startX, startY, endX, endY, scaledLBS);
    } else {
      quatRotation = viewTrans.GetRotation();
      quatRotation2 = viewTrans.GetRenderRotation();
      newRotation = viewTrans.Screen2Quat(projPicturePtr->ImageSizeH()-startX,
                                          startY,
                                          projPicturePtr->ImageSizeH()-endX,
                                          endY,
                                          scaledLBS);
      newRotation2 = viewTrans.Screen2Quat(startX, startY, endX, endY, scaledLBS);
                                // the last number scales the rotations correctly
      quatRotation = newRotation * quatRotation;
      viewTrans.SetRotation(quatRotation);
      quatRotation2 = newRotation2 * quatRotation2;
      viewTrans.SetRenderRotation(quatRotation2);
    }
    
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
    if(XmToggleButtonGetState(wAutoDraw)) {
      DoRender();
    } else {
      viewTrans.MakeTransform();
      if(showing3dRender) {
        showing3dRender = false;
      }
      projPicturePtr->MakeBoxes();
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
    }
#else
    viewTrans.MakeTransform();
    projPicturePtr->MakeBoxes();
    XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
#endif
    DoExposeTransDA();
    ++acc;
    if(acc == 10) {
      startX = cbs->event->xbutton.x;
      startY = cbs->event->xbutton.y;
      acc = 0;
    }
  } 
  
  if(servingButton == 2) {
    endX = cbs->event->xbutton.x;
    endY = cbs->event->xbutton.y;
    Real change((startY - endY) * 0.003);
    quatRotation = viewTrans.GetRotation();
    quatRotation2 = viewTrans.GetRenderRotation();
    // should replace the trigonometric equations...
    newRotation = AmrQuaternion(cos(-change), 0, 0, sin(-change));
    newRotation2 = AmrQuaternion(cos(change), 0, 0, sin(change));
    
    quatRotation = newRotation * quatRotation;
    viewTrans.SetRotation(quatRotation);
    quatRotation2 = newRotation2 * quatRotation2;
    viewTrans.SetRenderRotation(quatRotation2);
    
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
    if(XmToggleButtonGetState(wAutoDraw)) {
      DoRender();
    } else {
      viewTrans.MakeTransform();
      if(showing3dRender) {
        showing3dRender = false;
      }
      projPicturePtr->MakeBoxes();
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
    }
#else
    viewTrans.MakeTransform();
    projPicturePtr->MakeBoxes();
    XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
#endif
    DoExposeTransDA();
    ++acc;
    if(acc == 10) {
      startX = cbs->event->xbutton.x;
      startY = cbs->event->xbutton.y;
      acc = 0;
    }
  } 
  
  if(servingButton == 3) {
    endX = cbs->event->xbutton.x;
    endY = cbs->event->xbutton.y;
    temp = viewTrans.GetScale() + (endY - startY) * 0.003;
    temp = std::max(temp, (Real) 0.1);
    viewTrans.SetScale(temp);
    
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
    if(XmToggleButtonGetState(wAutoDraw)) {
      DoRender();
    } else {
      viewTrans.MakeTransform();
      if(showing3dRender) {
        showing3dRender = false;
      }
      projPicturePtr->MakeBoxes();
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
    }
#else
    viewTrans.MakeTransform();
    projPicturePtr->MakeBoxes();
    XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
#endif
    DoExposeTransDA();
    acc += 1;
    if(acc == 15) {
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
void PltApp::DoDetach(Widget, XtPointer, XtPointer) {
  Position xpos, ypos;
  Dimension wdth, hght;

  transDetached = true;
  XtUnmanageChild(wOrientXY);
  XtUnmanageChild(wOrientYZ);
  XtUnmanageChild(wOrientXZ);
  XtUnmanageChild(wLabelAxes);
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  XtUnmanageChild(wRender);
#endif
  XtUnmanageChild(wDetach);
  XtDestroyWidget(wTransDA);

  XtVaGetValues(wAmrVisTopLevel, XmNx, &xpos, XmNy, &ypos,
	XmNwidth, &wdth, XmNheight, &hght, NULL);

  string outbuf = "XYZ  ";
  outbuf += AVGlobals::StripSlashes(fileName);
  strcpy(buffer, outbuf.c_str());

  wDetachTopLevel = XtVaCreatePopupShell(buffer,
			 topLevelShellWidgetClass, wAmrVisTopLevel,
			 XmNwidth,		500,
			 XmNheight,		500,
			 XmNx,			xpos+wdth/2,
			 XmNy,			ypos+hght/2,			 
			 NULL);
  
  AddStaticCallback(wDetachTopLevel, XmNdestroyCallback, &PltApp::DoAttach);

  if(gaPtr->PVisual() != XDefaultVisual(display, gaPtr->PScreenNumber())) {
    XtVaSetValues(wDetachTopLevel, XmNvisual, gaPtr->PVisual(), XmNdepth, 8, NULL);
  }

  Widget wDetachForm, wAttachS, wDLabelAxes;
  Widget wDOrientXY, wDOrientYZ, wDOrientXZ;

  wDetachForm = XtVaCreateManagedWidget("detachform",
					xmFormWidgetClass, wDetachTopLevel,
					NULL);

  wAttachS = XtVaCreateManagedWidget("Attach", xmPushButtonWidgetClass,
				     wDetachForm,
				     XmNtopAttachment, XmATTACH_FORM,
				     XmNtopOffset, Amrvis::WOFFSET,
				     XmNrightAttachment, XmATTACH_FORM,
				     XmNrightOffset, Amrvis::WOFFSET,
				     NULL);
  AddStaticCallback(wAttachS, XmNactivateCallback, &PltApp::DoAttach);
  XtManageChild(wAttachS);

  wDOrientXY = XtVaCreateManagedWidget("XY", xmPushButtonWidgetClass,
				     wDetachForm,
				     XmNleftAttachment, XmATTACH_FORM,
				     XmNleftOffset, Amrvis::WOFFSET,
				     XmNtopAttachment, XmATTACH_FORM,
				     XmNtopOffset, Amrvis::WOFFSET, NULL);
  AddStaticCallback(wDOrientXY, XmNactivateCallback, &PltApp::DoOrient,
                    (XtPointer) OXY);
  XtManageChild(wDOrientXY);

  wDOrientYZ = XtVaCreateManagedWidget("YZ", xmPushButtonWidgetClass,
				     wDetachForm,
			             XmNleftAttachment, XmATTACH_WIDGET,
			             XmNleftWidget, wDOrientXY,
				     XmNleftOffset, Amrvis::WOFFSET,
				     XmNtopAttachment, XmATTACH_FORM,
				     XmNtopOffset, Amrvis::WOFFSET, NULL);
  AddStaticCallback(wDOrientYZ, XmNactivateCallback, &PltApp::DoOrient,
                    (XtPointer) OYZ);
  XtManageChild(wDOrientYZ);

  wDOrientXZ = XtVaCreateManagedWidget("XZ", xmPushButtonWidgetClass,
				     wDetachForm,
			             XmNleftAttachment, XmATTACH_WIDGET,
			             XmNleftWidget, wDOrientYZ,
				     XmNleftOffset, Amrvis::WOFFSET,
				     XmNtopAttachment, XmATTACH_FORM,
				     XmNtopOffset, Amrvis::WOFFSET, NULL);
  AddStaticCallback(wDOrientXZ, XmNactivateCallback, &PltApp::DoOrient,
                    (XtPointer) OXZ);
  XtManageChild(wDOrientXZ);

  wDLabelAxes = XtVaCreateManagedWidget("XYZ", xmPushButtonWidgetClass,
					wDetachForm,
					XmNleftAttachment, XmATTACH_WIDGET,
					XmNleftWidget, wDOrientXZ,
					XmNleftOffset, Amrvis::WOFFSET,
					XmNtopAttachment, XmATTACH_FORM,
					XmNtopOffset, Amrvis::WOFFSET, NULL);
  AddStaticCallback(wDLabelAxes, XmNactivateCallback, &PltApp::DoLabelAxes);
  XtManageChild(wDLabelAxes);

#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  Widget wDRender;
  wDRender = XtVaCreateManagedWidget("Draw", xmPushButtonWidgetClass,
			     wDetachForm,
			     XmNleftAttachment, XmATTACH_WIDGET,
			     XmNleftWidget, wDLabelAxes,
			     XmNleftOffset, Amrvis::WOFFSET,
			     XmNtopAttachment, XmATTACH_FORM,
			     XmNtopOffset, Amrvis::WOFFSET,
			     NULL);
  AddStaticCallback(wDRender, XmNactivateCallback, &PltApp::DoRender);
  XtManageChild(wDRender);
#endif

  wTransDA = XtVaCreateManagedWidget("detachDA", xmDrawingAreaWidgetClass,
			     wDetachForm,
			     XmNtranslations, XtParseTranslationTable(trans),
			     XmNleftAttachment,   XmATTACH_FORM,
			     XmNleftOffset,	  Amrvis::WOFFSET,
			     XmNtopAttachment,	  XmATTACH_WIDGET,
			     XmNtopWidget,	  wDOrientXY,
			     XmNtopOffset,	  Amrvis::WOFFSET,
			     XmNrightAttachment,  XmATTACH_FORM,
			     XmNrightOffset,	  Amrvis::WOFFSET,
			     XmNbottomAttachment, XmATTACH_FORM,
			     XmNbottomOffset,	  Amrvis::WOFFSET,
			     NULL);
  projPicturePtr->SetDrawingArea(wTransDA);

  XtPopup(wDetachTopLevel, XtGrabNone);
  XSetWindowColormap(display, XtWindow(wDetachTopLevel),
		     pltPaletteptr->GetColormap());
  XSetWindowColormap(display, XtWindow(wTransDA), pltPaletteptr->GetColormap());
  AddStaticCallback(wTransDA, XmNinputCallback, &PltApp::DoTransInput);
  AddStaticCallback(wTransDA, XmNresizeCallback, &PltApp::DoTransResize);
  AddStaticEventHandler(wTransDA, ExposureMask, &PltApp::DoExposeTransDA);

  DoTransResize(wTransDA, NULL, NULL);
}


// -------------------------------------------------------------------
void PltApp::DoAttach(Widget, XtPointer, XtPointer) {
  transDetached = false;
  XtDestroyWidget(wDetachTopLevel);

  XtManageChild(wOrientXY);
  XtManageChild(wOrientYZ);
  XtManageChild(wOrientXZ);
  XtManageChild(wLabelAxes);
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  XtManageChild(wRender);
#endif
  XtManageChild(wDetach);
  wTransDA = XtVaCreateManagedWidget("transDA", xmDrawingAreaWidgetClass,
			     wPlotArea,
			     XmNtranslations,	XtParseTranslationTable(trans),
			     XmNleftAttachment,	XmATTACH_WIDGET,
			     XmNleftWidget,	wScrollArea[Amrvis::XPLANE],
			     XmNleftOffset,	Amrvis::WOFFSET,
			     XmNtopAttachment,	XmATTACH_WIDGET,
			     XmNtopWidget,		wOrientXY,
			     XmNtopOffset,		Amrvis::WOFFSET,
			     XmNrightAttachment,	XmATTACH_FORM,
			     XmNrightOffset,		Amrvis::WOFFSET,
			     XmNbottomAttachment,	XmATTACH_FORM,
			     XmNbottomOffset,	Amrvis::WOFFSET,
			     NULL);
  AddStaticCallback(wTransDA, XmNinputCallback, &PltApp::DoTransInput);
  AddStaticCallback(wTransDA, XmNresizeCallback, &PltApp::DoTransResize);
  AddStaticEventHandler(wTransDA, ExposureMask, &PltApp::DoExposeTransDA);

  projPicturePtr->SetDrawingArea(wTransDA);
  DoTransResize(wTransDA, NULL, NULL);
}


#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
// -------------------------------------------------------------------
void PltApp::DoApplyLightingWindow(Widget, XtPointer, XtPointer) {
  //read new input
  Real ambient = atof(XmTextFieldGetString(wLWambient));
  Real diffuse = atof(XmTextFieldGetString(wLWdiffuse));
  Real specular = atof(XmTextFieldGetString(wLWspecular));
  Real shiny = atof(XmTextFieldGetString(wLWshiny));
  Real minray = atof(XmTextFieldGetString(wLWminOpacity));
  Real maxray = atof(XmTextFieldGetString(wLWmaxOpacity));
  
  if(0.0 > ambient || ambient > 1.0) {
    cerr << "Error:  ambient value must be in the range (0.0, 1.0)." << endl;
  }
  if(0.0 > diffuse || diffuse > 1.0) {
    cerr << "Error:  diffuse value must be in the range (0.0, 1.0)." << endl;
  }
  if(0.0 > specular || specular > 1.0) {
    cerr << "Error:  specular value must be in the range (0.0, 1.0)." << endl;
  }
  if(0.0 > minray || minray > 1.0) {
    cerr << "Error:  minray value must be in the range (0.0, 1.0)." << endl;
  }
  if(0.0 > maxray || maxray > 1.0) {
    cerr << "Error:  maxray value must be in the range (0.0, 1.0)." << endl;
  }

  if(0.0 > ambient || ambient > 1.0   || 0.0 > diffuse || diffuse > 1.0 ||
     0.0 > specular || specular > 1.0 || 0.0 > minray || minray > 1.0 ||
     0.0 > maxray || maxray > 1.0 )
  {
    return;  // rewrite old values
  }

  VolRender *volRenderPtr = projPicturePtr->GetVolRenderPtr();
  
  if(ambient != volRenderPtr->GetAmbient() ||
     diffuse != volRenderPtr->GetDiffuse() ||
     specular != volRenderPtr->GetSpecular() ||
     shiny != volRenderPtr->GetShiny() ||
     minray != volRenderPtr->GetMinRayOpacity() ||
     maxray != volRenderPtr->GetMaxRayOpacity())
  {
    volRenderPtr->SetLighting(ambient, diffuse, specular, shiny, minray, maxray);
    // update render image if necessary
    projPicturePtr->GetVolRenderPtr()->InvalidateVPData();
    if(XmToggleButtonGetState(wAutoDraw)) {
      DoRender();
    }
  }
}


// -------------------------------------------------------------------
void PltApp::DoDoneLightingWindow(Widget w, XtPointer xp1, XtPointer xp2) {
  DoApplyLightingWindow(w, xp1, xp2);
  XtDestroyWidget(wLWTopLevel);
}


// -------------------------------------------------------------------
void PltApp::DestroyLightingWindow(Widget /*w*/, XtPointer /*xp*/, XtPointer) {
  lightingWindowExists = false;
}


// -------------------------------------------------------------------
void PltApp::DoCancelLightingWindow(Widget, XtPointer, XtPointer) {
  XtDestroyWidget(wLWTopLevel);
}


// -------------------------------------------------------------------
void PltApp::DoOpenFileLightingWindow(Widget /*w*/, XtPointer oos, XtPointer /*call_data*/)
{
  static Widget wOpenLWDialog;
  wOpenLWDialog = XmCreateFileSelectionDialog(wAmrVisTopLevel,
                                const_cast<char *>("Lighting File"), NULL, 0);

  AddStaticCallback(wOpenLWDialog, XmNokCallback, &PltApp::DoOpenLightingFile, oos);
  XtAddCallback(wOpenLWDialog, XmNcancelCallback,
                (XtCallbackProc) XtUnmanageChild, (XtPointer) this);
  XtManageChild(wOpenLWDialog);
  XtPopup(XtParent(wOpenLWDialog), XtGrabExclusive);
}


// -------------------------------------------------------------------
void PltApp::DoOpenLightingFile(Widget w, XtPointer oos, XtPointer call_data)
{
  unsigned long ioos((unsigned long) oos);
  bool bFileOk;
  Real ambient, diffuse, specular, shiny, minray, maxray;
  char *lightingfile;
  if( ! XmStringGetLtoR(((XmFileSelectionBoxCallbackStruct *) call_data)->value,
                        XmSTRING_DEFAULT_CHARSET, &lightingfile))
  {
    cerr << "PltApp::DoOpenLightingFile:: system error" << endl;
    return;
  }
  XtPopdown(XtParent(w));

  string asLFile(lightingfile);
  ambient = atof(XmTextFieldGetString(wLWambient));
  diffuse = atof(XmTextFieldGetString(wLWdiffuse));
  specular = atof(XmTextFieldGetString(wLWspecular));
  shiny = atof(XmTextFieldGetString(wLWshiny));
  minray = atof(XmTextFieldGetString(wLWminOpacity));
  maxray = atof(XmTextFieldGetString(wLWmaxOpacity));
  if(ioos == openingLW) {
    cout << "_in DoOpenLightingFile:  openingLW." << endl;
    bFileOk = AVGlobals::ReadLightingFile(asLFile, ambient, diffuse, specular,
			                  shiny, minray, maxray);
    if(bFileOk) {
      char cTempReal[32];
      sprintf(cTempReal, "%3.2f", ambient);
      XmTextFieldSetString(wLWambient, cTempReal);
      sprintf(cTempReal, "%3.2f", diffuse);
      XmTextFieldSetString(wLWdiffuse, cTempReal);
      sprintf(cTempReal, "%3.2f", specular);
      XmTextFieldSetString(wLWspecular, cTempReal);
      sprintf(cTempReal, "%3.2f", shiny);
      XmTextFieldSetString(wLWshiny, cTempReal);
      sprintf(cTempReal, "%3.2f", minray);
      XmTextFieldSetString(wLWminOpacity, cTempReal);
      sprintf(cTempReal, "%3.2f", maxray);
      XmTextFieldSetString(wLWmaxOpacity, cTempReal);

      DoApplyLightingWindow(NULL, NULL, NULL);

      lightingFilename = AVGlobals::StripSlashes(asLFile);
    }
  } else {
    BL_ASSERT(ioos == savingLW);
    cout << "_in DoOpenLightingFile:  savingLW." << endl;
    bFileOk = AVGlobals::WriteLightingFile(asLFile, ambient, diffuse, specular,
                                           shiny, minray, maxray);
    if(bFileOk) {
      lightingFilename = AVGlobals::StripSlashes(asLFile);
    }
  }
  XtFree(lightingfile);
}


// -------------------------------------------------------------------
void PltApp::DoCreateLightingWindow(Widget, XtPointer, XtPointer) {
  //Position xpos, ypos;
  if(lightingWindowExists) {
    XRaiseWindow(display, XtWindow(wLWTopLevel));
    return;
  }

  Dimension wWidth(0), wHeight(0);
  Dimension ww, wh;
  lightingWindowExists = true;
  
  string LWtitlebar = "Lighting";
  strcpy(buffer, LWtitlebar.c_str());
  
  wLWTopLevel = XtVaCreatePopupShell(buffer,
			 topLevelShellWidgetClass, 
			 wAmrVisTopLevel,
			 XmNwidth,  100,
			 XmNheight, 100,
			 NULL);
  
  AddStaticCallback(wLWTopLevel, XmNdestroyCallback,
		    &PltApp::DestroyLightingWindow);
  
  if(gaPtr->PVisual() != XDefaultVisual(display, gaPtr->PScreenNumber())) {
    XtVaSetValues(wLWTopLevel, XmNvisual, gaPtr->PVisual(), XmNdepth, 8, NULL);
  }
  
  wLWForm = XtVaCreateManagedWidget("detachform",
				    xmFormWidgetClass, wLWTopLevel,
				    NULL);
  
  // make the buttons
  Widget wLWDoneButton = XtVaCreateManagedWidget(" Ok ",
			    xmPushButtonGadgetClass, wLWForm,
			    XmNbottomAttachment, XmATTACH_FORM,
			    XmNbottomOffset, Amrvis::WOFFSET,
			    XmNleftAttachment, XmATTACH_FORM,
			    XmNleftOffset, Amrvis::WOFFSET,
			    NULL);
  AddStaticCallback(wLWDoneButton, XmNactivateCallback,
		    &PltApp::DoDoneLightingWindow);
  

  Widget wLWApplyButton = XtVaCreateManagedWidget("Apply",
			    xmPushButtonGadgetClass, wLWForm,
			    XmNbottomAttachment, XmATTACH_FORM,
			    XmNbottomOffset, Amrvis::WOFFSET,
			    XmNleftAttachment, XmATTACH_WIDGET,
			    XmNleftWidget,     wLWDoneButton,
			    NULL);
  AddStaticCallback(wLWApplyButton, XmNactivateCallback,
		    &PltApp::DoApplyLightingWindow);
  
  Widget wLWOpenButton = XtVaCreateManagedWidget("Open...",
			    xmPushButtonGadgetClass, wLWForm,
			    XmNbottomAttachment, XmATTACH_FORM,
			    XmNbottomOffset, Amrvis::WOFFSET,
			    XmNleftAttachment, XmATTACH_WIDGET,
			    XmNleftWidget,     wLWApplyButton,
			    NULL);
  AddStaticCallback(wLWOpenButton, XmNactivateCallback,
		    &PltApp::DoOpenFileLightingWindow,
		    (XtPointer) openingLW);
  
  Widget wLWSaveButton = XtVaCreateManagedWidget("Save...",
			    xmPushButtonGadgetClass, wLWForm,
			    XmNbottomAttachment, XmATTACH_FORM,
			    XmNbottomOffset, Amrvis::WOFFSET,
			    XmNleftAttachment, XmATTACH_WIDGET,
			    XmNleftWidget,     wLWOpenButton,
			    NULL);
  AddStaticCallback(wLWSaveButton, XmNactivateCallback,
		    &PltApp::DoOpenFileLightingWindow,
		    (XtPointer) savingLW);
  
  Widget wLWCancelButton = XtVaCreateManagedWidget("Cancel",
			    xmPushButtonGadgetClass, wLWForm,
			    XmNbottomAttachment, XmATTACH_FORM,
			    XmNbottomOffset, Amrvis::WOFFSET,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNrightOffset, Amrvis::WOFFSET,
			    NULL);
  AddStaticCallback(wLWCancelButton, XmNactivateCallback,
		    &PltApp::DoCancelLightingWindow);
  

  VolRender *volRenderPtr = projPicturePtr->GetVolRenderPtr();

  //make the input forms
  //Widget wTempLabel;
  wTempLabel = XtVaCreateManagedWidget("ambient: ",
			    xmLabelGadgetClass, wLWForm,
			    XmNtopAttachment, XmATTACH_FORM,
			    XmNtopOffset, Amrvis::WOFFSET,
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition, 12,
			    XmNleftAttachment, XmATTACH_FORM,
			    XmNleftOffset, Amrvis::WOFFSET,
			    NULL);
  
  char cNbuff[64];
  sprintf(cNbuff, "%3.2f", volRenderPtr->GetAmbient());
  wLWambient = XtVaCreateManagedWidget("variable",
			    xmTextFieldWidgetClass, wLWForm,
			    XmNtopAttachment, XmATTACH_FORM,
			    XmNtopOffset, Amrvis::WOFFSET,
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition, 12,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNrightOffset, Amrvis::WOFFSET,
			    XmNvalue, cNbuff,
			    XmNcolumns, 6,
			    NULL);
  wTempLabel = XtVaCreateManagedWidget("diffuse: ",
			    xmLabelGadgetClass, wLWForm,
			    XmNtopAttachment, XmATTACH_WIDGET,
			    XmNtopWidget, wLWambient,
			    XmNtopOffset, Amrvis::WOFFSET,
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition, 25,
			    XmNleftAttachment, XmATTACH_FORM,
			    XmNleftOffset, Amrvis::WOFFSET,
			    NULL);
  
  sprintf(cNbuff, "%3.2f", volRenderPtr->GetDiffuse());
  wLWdiffuse = XtVaCreateManagedWidget("variable",
			    xmTextFieldWidgetClass, wLWForm,
			    XmNtopAttachment, XmATTACH_WIDGET,
			    XmNtopWidget, wLWambient,
			    XmNtopOffset, Amrvis::WOFFSET,
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition, 25,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNrightOffset, Amrvis::WOFFSET, 
			    XmNvalue, cNbuff,
			    XmNcolumns, 6,
			    NULL);
  wTempLabel = XtVaCreateManagedWidget("specular: ",
			    xmLabelGadgetClass, wLWForm,
			    XmNtopAttachment, XmATTACH_WIDGET,
			    XmNtopWidget, wLWdiffuse,
			    XmNtopOffset, Amrvis::WOFFSET,
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition, 37,
			    XmNleftAttachment, XmATTACH_FORM,
			    XmNleftOffset, Amrvis::WOFFSET,
			    NULL);
  
  sprintf(cNbuff, "%3.2f", volRenderPtr->GetSpecular());
  wLWspecular = XtVaCreateManagedWidget("variable",
			    xmTextFieldWidgetClass, wLWForm,
			    XmNtopAttachment, XmATTACH_WIDGET,
			    XmNtopWidget, wLWdiffuse,
			    XmNtopOffset, Amrvis::WOFFSET,
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition, 37,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNrightOffset, Amrvis::WOFFSET,
			    XmNvalue, cNbuff,
			    XmNcolumns, 6,
			    NULL);
  wTempLabel = XtVaCreateManagedWidget("shiny: ",
			    xmLabelGadgetClass, wLWForm,
			    XmNtopAttachment, XmATTACH_WIDGET,
			    XmNtopWidget, wLWspecular,
			    XmNtopOffset, Amrvis::WOFFSET,
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition, 50,
			    XmNleftAttachment, XmATTACH_FORM,
			    XmNleftOffset, Amrvis::WOFFSET,
			    NULL);
  
  sprintf(cNbuff, "%3.2f", volRenderPtr->GetShiny());
  wLWshiny = XtVaCreateManagedWidget("variable",
			    xmTextFieldWidgetClass, wLWForm,
			    XmNtopAttachment, XmATTACH_WIDGET,
			    XmNtopWidget, wLWspecular,
			    XmNtopOffset, Amrvis::WOFFSET,
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition, 50,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNrightOffset, Amrvis::WOFFSET,
			    XmNvalue, cNbuff,
			    XmNcolumns, 6,
			    NULL);

  wTempLabel = XtVaCreateManagedWidget("minRayOpacity: ",
			    xmLabelGadgetClass, wLWForm,
			    xmTextFieldWidgetClass, wLWForm,
			    XmNtopAttachment, XmATTACH_WIDGET,
			    XmNtopOffset, Amrvis::WOFFSET,
			    XmNtopWidget, wLWshiny,
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition, 62,
			    XmNleftAttachment, XmATTACH_FORM,
			    XmNleftOffset, Amrvis::WOFFSET,
			    NULL);
  
  sprintf(cNbuff, "%3.2f", volRenderPtr->GetMinRayOpacity());
  wLWminOpacity = XtVaCreateManagedWidget("minray",
			    xmTextFieldWidgetClass, wLWForm,
			    XmNtopAttachment, XmATTACH_WIDGET,
			    XmNtopOffset, Amrvis::WOFFSET,
			    XmNtopWidget, wLWshiny,
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition, 62,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNrightOffset, Amrvis::WOFFSET,
			    XmNvalue, cNbuff,
			    XmNcolumns, 6,
			    NULL);
  
  wTempLabel = XtVaCreateManagedWidget("maxRayOpacity: ",
			    xmLabelGadgetClass, wLWForm,
			    XmNtopAttachment, XmATTACH_WIDGET,
			    XmNtopOffset, Amrvis::WOFFSET,
			    XmNtopWidget, wLWminOpacity,
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition, 75,
			    XmNleftAttachment, XmATTACH_FORM,
			    XmNleftOffset, Amrvis::WOFFSET,
			    NULL);
  
  sprintf(cNbuff, "%3.2f", volRenderPtr->GetMaxRayOpacity());
  wLWmaxOpacity = XtVaCreateManagedWidget("variable", xmTextFieldWidgetClass,
			    wLWForm,
			    XmNtopAttachment, XmATTACH_WIDGET,
			    XmNtopOffset, Amrvis::WOFFSET,
			    XmNtopWidget, wLWminOpacity,
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition, 75,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNrightOffset, Amrvis::WOFFSET,
			    XmNvalue, cNbuff,
			    XmNcolumns, 6,
			    NULL);
  
  
  XtVaGetValues(wLWOpenButton, XmNwidth, &ww, XmNheight, &wh, NULL);
  wWidth = 5 * ww;
  wHeight = 12 * wh;

  XtVaSetValues(wLWTopLevel, XmNwidth, wWidth, XmNheight, wHeight, NULL);

  //XtManageChild(wLWCancelButton);
  //XtManageChild(wLWDoneButton);
  //XtManageChild(wLWApplyButton);

  XtPopup(wLWTopLevel, XtGrabNone);
  XSetWindowColormap(display, XtWindow(wLWTopLevel), pltPaletteptr->GetColormap());
}


// -------------------------------------------------------------------
void PltApp::DoAutoDraw(Widget, XtPointer, XtPointer) {
  if(XmToggleButtonGetState(wAutoDraw)) {
    DoRender();
  } else {
    if(showing3dRender) {
      showing3dRender = false;
    }
    projPicturePtr->MakeBoxes();
  }
  XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
  DoExposeTransDA();
}


// -------------------------------------------------------------------
void PltApp::DoRenderModeMenu(Widget w, XtPointer item_no, XtPointer /*client_data*/) {
  if(wCurrentRenderMode == w) {
    XtVaSetValues(w, XmNset, true, NULL);
    return;
  }
  XtVaSetValues(wCurrentRenderMode, XmNset, false, NULL);
  wCurrentRenderMode = w;
  
#if defined(BL_VOLUMERENDER)
  if(item_no == (XtPointer) 0) {
    lightingModel = true;
  } else if(item_no == (XtPointer) 1) {
    lightingModel = false;  // use value model
  }
  projPicturePtr->GetVolRenderPtr()->SetLightingModel(lightingModel);
  projPicturePtr->GetVolRenderPtr()->InvalidateVPData();
  if(XmToggleButtonGetState(wAutoDraw) || showing3dRender) {
    DoRender();
  }
#endif
}


// -------------------------------------------------------------------
void PltApp::DoClassifyMenu(Widget w, XtPointer item_no, XtPointer /*client_data*/) {
  if(wCurrentClassify == w) {
    XtVaSetValues(w, XmNset, true, NULL);
    return;
  }
  XtVaSetValues(wCurrentClassify, XmNset, false, NULL);
  wCurrentClassify = w;

#if defined(BL_VOLUMERENDER)
  if(item_no == (XtPointer) 0) {
    preClassify = true;   // Use Pre-Classified Mode
  } else if(item_no == (XtPointer) 1) {
    preClassify = false;  // Use Octree Mode
  }
  projPicturePtr->GetVolRenderPtr()->SetPreClassifyAlgorithm(preClassify);
  projPicturePtr->GetVolRenderPtr()->InvalidateVPData();
  if(XmToggleButtonGetState(wAutoDraw) || showing3dRender) {
    DoRender();
  }
#endif
}


// -------------------------------------------------------------------
void PltApp::DoRender(Widget, XtPointer, XtPointer) {
  showing3dRender = true;

  VolRender *volRender = projPicturePtr->GetVolRenderPtr();
  if( ! volRender->SWFDataValid()) {
    int iPaletteStart = pltPaletteptr->PaletteStart();
    int iPaletteEnd   = pltPaletteptr->PaletteEnd();
    int iBlackIndex   = pltPaletteptr->BlackIndex();
    int iWhiteIndex   = pltPaletteptr->WhiteIndex();
    int iColorSlots   = pltPaletteptr->ColorSlots();
    Real minUsing, maxUsing;
    pltAppState->GetMinMax(minUsing, maxUsing);

    volRender->MakeSWFData(dataServicesPtr[currentFrame],
			   minUsing, maxUsing,
			   pltAppState->CurrentDerived(), 
			   iPaletteStart, iPaletteEnd,
			   iBlackIndex, iWhiteIndex,
			   iColorSlots, pltAppState->GetShowingBoxes());
  }
  if( ! volRender->VPDataValid()) {
    volRender->MakeVPData();
  }

  projPicturePtr->MakePicture();
  projPicturePtr->DrawPicture();
}
#endif


// -------------------------------------------------------------------
void PltApp::DoTransResize(Widget, XtPointer, XtPointer) {
  Dimension wdth, hght;

  XSetWindowColormap(display, XtWindow(wTransDA),
                     pltPaletteptr->GetColormap());

  XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
  XtVaGetValues(wTransDA, XmNwidth, &wdth, XmNheight, &hght, NULL);
  daWidth = wdth;
  daHeight = hght;
  projPicturePtr->SetDrawingAreaDimensions(daWidth, daHeight);
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  if(XmToggleButtonGetState(wAutoDraw)) {
    DoRender();
  } else {
    viewTrans.MakeTransform();
    if(showing3dRender) {
      showing3dRender = false;
    }
    projPicturePtr->MakeBoxes();
  }
#else
  viewTrans.MakeTransform();
  projPicturePtr->MakeBoxes();
#endif
  DoExposeTransDA();
}


// -------------------------------------------------------------------
void PltApp::Clear() {
  XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  XmToggleButtonSetState(wAutoDraw, false, false);
#endif
  // viewTrans.SetRotation(AmrQuaternion(1, 0, 0, 0));
  // viewTrans.MakeTransform();
  DoExposeTransDA();
}


// -------------------------------------------------------------------
void PltApp::DoOrient(Widget /*w*/, XtPointer client_data, XtPointer) {
  Real rW(1.0), rX(0.0), rY(0.0), rZ(0.0);
  Real rW2(1.0), rX2(0.0), rY2(0.0), rZ2(0.0);
  //int iOrientation((int) client_data);
  long iOrientation(reinterpret_cast<long> (client_data));
  switch(iOrientation) {
    case OXY:
      rW  =  1.0;
      rX  =  0.0;
      rY  =  0.0;
      rZ  =  0.0;
      rW2 =  1.0;
      rX2 =  0.0;
      rY2 =  0.0;
      rZ2 =  0.0;
    break;

    case OYZ:
      rW  =  0.5;
      rX  = -0.5;
      rY  = -0.5;
      rZ  = -0.5;
      rW2 =  0.5;
      rX2 = -0.5;
      rY2 =  0.5;
      rZ2 =  0.5;
    break;

    case OXZ:
      rW  = -0.5;
      rX  = -0.5;
      rY  = -0.5;
      rZ  = -0.5;
      rW2 = -0.5;
      rX2 = -0.5;
      rY2 =  0.5;
      rZ2 =  0.5;
    break;
  }
  viewTrans.SetRotation(AmrQuaternion(rW, rX, rY, rZ));
  viewTrans.SetRenderRotation(AmrQuaternion(rW2, rX2, rY2, rZ2));
  viewTrans.ResetTranslation();
  viewTrans.MakeTransform();
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  if(XmToggleButtonGetState(wAutoDraw)) {
    DoRender();
  } else {
    if(showing3dRender) {
      showing3dRender = false;
    }
    projPicturePtr->MakeBoxes();
  }
#else
  projPicturePtr->MakeBoxes();
#endif
  XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
  DoExposeTransDA();
}


// -------------------------------------------------------------------
void PltApp::DoLabelAxes(Widget, XtPointer, XtPointer) {
  labelAxes = (labelAxes ? false : true);
  XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
  DoExposeTransDA();
}

