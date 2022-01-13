// ---------------------------------------------------------------
// PltAppOutput.cpp
// ---------------------------------------------------------------


#include <AMReX_ParallelDescriptor.H>

#include <Xm/Xm.h>
#include <Xm/SelectioB.h>
#include <Xm/ToggleB.h>
#include <Xm/Text.h>

#include <PltApp.H>
#include <PltAppState.H>
#include <AMReX_DataServices.H>
#include <ProjectionPicture.H>
#include <Output.H>
#include <XYPlotWin.H>

using std::cout;
using std::cerr;
using std::endl;
using std::strcpy;

using namespace amrex;


// -------------------------------------------------------------------
void PltApp::DoOutput(Widget w, XtPointer data, XtPointer) {
  amrex::ignore_unused(w);
  int i;
  static Widget wGetFileName;
  XmString sMessage;
  sMessage = XmStringCreateSimple(const_cast<char *>("Please enter a filename base:"));

  i=0;
  XtSetArg(args[i], XmNselectionLabelString, sMessage); ++i;
  XtSetArg(args[i], XmNautoUnmanage, false); ++i;
  XtSetArg(args[i], XmNkeyboardFocusPolicy, XmPOINTER); ++i;
  wGetFileName = XmCreatePromptDialog(wAmrVisTopLevel, const_cast<char *>("Save as"), args, i);
  XmStringFree(sMessage);

  unsigned long which = (unsigned long) data;
  switch(which) {
    case 0:
      AddStaticCallback(wGetFileName, XmNokCallback,&PltApp::DoCreatePSFile);
    break;
    case 1:
      AddStaticCallback(wGetFileName, XmNokCallback,&PltApp::DoCreateRGBFile);
    break;
    case 2:
      AddStaticCallback(wGetFileName, XmNokCallback,&PltApp::DoCreateFABFile);
    break;
    default:
      cerr << "Error in PltApp::DoOutput:  bad selection = " << data << endl;
      return;
  }

  XtAddCallback(wGetFileName, XmNcancelCallback,
  		(XtCallbackProc)XtDestroyWidget, NULL);
  XtSetSensitive(XmSelectionBoxGetChild(wGetFileName,
  		XmDIALOG_HELP_BUTTON), false);

  char tempstr[Amrvis::BUFSIZE], tempfilename[Amrvis::BUFSIZE];
  if(animating2d) {
    strcpy(tempfilename, AVGlobals::StripSlashes(fileNames[currentFrame]).c_str());
  } else {
    strcpy(tempfilename, AVGlobals::StripSlashes(fileNames[0]).c_str());
  }
  sprintf(tempstr, "%s_%s", pltAppState->CurrentDerived().c_str(), tempfilename);
  XmTextSetString(XmSelectionBoxGetChild(wGetFileName, XmDIALOG_TEXT), tempstr);
  XtManageChild(wGetFileName);
  XtPopup(XtParent(wGetFileName), XtGrabNone);
}  // end DoOutput


// -------------------------------------------------------------------
void PltApp::DoCreatePSFile(Widget w, XtPointer, XtPointer call_data) {
  XmSelectionBoxCallbackStruct *cbs = (XmSelectionBoxCallbackStruct *) call_data;
  char psfilename[Amrvis::BUFSIZE];
  char *fileNameBase;
  int imageSizeX, imageSizeY;
  XImage *printImage;

  if(animating2d) {
    ResetAnimation();
  }

  XmStringGetLtoR(cbs->value, XmSTRING_DEFAULT_CHARSET, &fileNameBase);

  // write the Amrvis::ZPLANE picture
  sprintf(psfilename, "%s_XY.ps", fileNameBase);
  printImage = amrPicturePtrArray[Amrvis::ZPLANE]->GetPictureXImage();
  imageSizeX = amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeH();
  imageSizeY = amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeV();
  WritePSFile(psfilename, printImage, imageSizeX, imageSizeY, *pltPaletteptr);

#ifdef AMREX_DEBUG
/*
  {
  int minDrawnLevel(pltAppState->MinDrawnLevel());
  int maxDrawnLevel(pltAppState->MaxDrawnLevel());
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  sprintf(psfilename, "%s_XY_new.ps", fileNameBase);
  bool bDrawBoxesIntoImage(false);
  printImage = amrPicturePtrArray[Amrvis::ZPLANE]->GetPictureXImage(bDrawBoxesIntoImage);
  Vector< Vector<GridBoxes> > gridBoxes;
  amrPicturePtrArray[Amrvis::ZPLANE]->GetGridBoxes(gridBoxes, minDrawnLevel, maxDrawnLevel);
  WriteNewPSFile(psfilename, printImage, imageSizeX, imageSizeY, *pltPaletteptr,
		 amrData, minDrawnLevel, maxDrawnLevel, gridBoxes);
  }
*/
#endif

#if (BL_SPACEDIM==3)
  int minDrawnLevel(pltAppState->MinDrawnLevel());
  int maxDrawnLevel(pltAppState->MaxDrawnLevel());
  // write the Amrvis::YPLANE picture
  sprintf(psfilename, "%s_XZ.ps", fileNameBase);
  printImage = amrPicturePtrArray[Amrvis::YPLANE]->GetPictureXImage();
  imageSizeX = amrPicturePtrArray[Amrvis::YPLANE]->ImageSizeH();
  imageSizeY = amrPicturePtrArray[Amrvis::YPLANE]->ImageSizeV();
  WritePSFile(psfilename, printImage, imageSizeX, imageSizeY, *pltPaletteptr);

  // write the Amrvis::XPLANE picture
  sprintf(psfilename, "%s_YZ.ps", fileNameBase);
  printImage = amrPicturePtrArray[Amrvis::XPLANE]->GetPictureXImage();
  imageSizeX = amrPicturePtrArray[Amrvis::XPLANE]->ImageSizeH();
  imageSizeY = amrPicturePtrArray[Amrvis::XPLANE]->ImageSizeV();
  WritePSFile(psfilename, printImage, imageSizeX, imageSizeY, *pltPaletteptr);

  // write the iso picture
#ifdef BL_VOLUMERENDER
  if( ! (XmToggleButtonGetState(wAutoDraw) || showing3dRender ) ) {
    printImage = projPicturePtr->DrawBoxesIntoPixmap(minDrawnLevel, maxDrawnLevel);
  } else {
    printImage = projPicturePtr->GetPictureXImage();
  }
#else
  printImage = projPicturePtr->DrawBoxesIntoPixmap(minDrawnLevel, maxDrawnLevel);
#endif
  sprintf(psfilename, "%s_XYZ.ps", fileNameBase);
  imageSizeX = projPicturePtr->ImageSizeH();
  imageSizeY = projPicturePtr->ImageSizeV();
  WritePSFile(psfilename, printImage, imageSizeX, imageSizeY, *pltPaletteptr);
# endif

  // write the palette
  sprintf(psfilename, "%s_pal.ps", fileNameBase);
  printImage = pltPaletteptr->GetPictureXImage();
  imageSizeX = pltPaletteptr->PaletteWidth();
  imageSizeY = pltPaletteptr->PaletteHeight();
  const Vector<Real> &pValueList = pltPaletteptr->PaletteDataList();
  string pNumFormat(pltPaletteptr->PaletteNumberFormat());
  WritePSPaletteFile(psfilename, printImage, imageSizeX, imageSizeY, 
                     pValueList, pNumFormat, *pltPaletteptr);

  XtFree(fileNameBase);
  XtDestroyWidget(w);

}  // end DoCreatePSFile


// -------------------------------------------------------------------
void PltApp::DoCreateRGBFile(Widget w, XtPointer, XtPointer call_data) {
  XmSelectionBoxCallbackStruct *cbs = (XmSelectionBoxCallbackStruct *) call_data;
  char rgbfilename[Amrvis::BUFSIZE];
  char *fileNameBase;
  int imageSizeX, imageSizeY;
  XImage *printImage;

  if(animating2d) {
    ResetAnimation();
  }

  XmStringGetLtoR(cbs->value, XmSTRING_DEFAULT_CHARSET, &fileNameBase);

  // write the Amrvis::ZPLANE picture
  char suffix[4];
  if(AVGlobals::IsSGIrgbFile()) {
    strcpy(suffix, "rgb");
  } else {
    strcpy(suffix, "ppm");
  }
  sprintf(rgbfilename, "%s_XY.%s", fileNameBase,suffix);
  printImage = amrPicturePtrArray[Amrvis::ZPLANE]->GetPictureXImage();
  imageSizeX = amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeH();
  imageSizeY = amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeV();
  if(AVGlobals::IsSGIrgbFile()) {
    WriteRGBFile(rgbfilename, printImage, imageSizeX, imageSizeY, *pltPaletteptr);
  } else {
    WritePPMFile(rgbfilename, printImage, imageSizeX, imageSizeY, *pltPaletteptr);
  }
  

#if (BL_SPACEDIM==3)
  // write the Amrvis::YPLANE picture
  sprintf(rgbfilename, "%s_XZ.%s", fileNameBase, suffix);
  printImage = amrPicturePtrArray[Amrvis::YPLANE]->GetPictureXImage();
  imageSizeX = amrPicturePtrArray[Amrvis::YPLANE]->ImageSizeH();
  imageSizeY = amrPicturePtrArray[Amrvis::YPLANE]->ImageSizeV();
  if(AVGlobals::IsSGIrgbFile()) {
    WriteRGBFile(rgbfilename, printImage, imageSizeX, imageSizeY, *pltPaletteptr);
  } else {
    WritePPMFile(rgbfilename, printImage, imageSizeX, imageSizeY, *pltPaletteptr);
  }

  // write the Amrvis::XPLANE picture
  sprintf(rgbfilename, "%s_YZ.%s", fileNameBase, suffix);
  printImage = amrPicturePtrArray[Amrvis::XPLANE]->GetPictureXImage();
  imageSizeX = amrPicturePtrArray[Amrvis::XPLANE]->ImageSizeH();
  imageSizeY = amrPicturePtrArray[Amrvis::XPLANE]->ImageSizeV();
  if(AVGlobals::IsSGIrgbFile()) {
    WriteRGBFile(rgbfilename, printImage, imageSizeX, imageSizeY, *pltPaletteptr);
  } else {
    WritePPMFile(rgbfilename, printImage, imageSizeX, imageSizeY, *pltPaletteptr);
  }

  // write the iso picture
  int minDrawnLevel(pltAppState->MinDrawnLevel());
  int maxDrawnLevel(pltAppState->MaxDrawnLevel());
#ifdef BL_VOLUMERENDER
  if( ! (XmToggleButtonGetState(wAutoDraw) || showing3dRender )) {
    printImage = projPicturePtr->DrawBoxesIntoPixmap(minDrawnLevel, maxDrawnLevel);
  } else {
    printImage = projPicturePtr->GetPictureXImage();
  }
#else
  printImage = projPicturePtr->DrawBoxesIntoPixmap(minDrawnLevel, maxDrawnLevel);
#endif
  sprintf(rgbfilename, "%s_XYZ.%s", fileNameBase, suffix);
  imageSizeX = projPicturePtr->ImageSizeH();
  imageSizeY = projPicturePtr->ImageSizeV();
  if(AVGlobals::IsSGIrgbFile()) {
    WriteRGBFile(rgbfilename, printImage, imageSizeX, imageSizeY, *pltPaletteptr);
  } else {
    WritePPMFile(rgbfilename, printImage, imageSizeX, imageSizeY, *pltPaletteptr);
  }
# endif

  // write the palette
  sprintf(rgbfilename, "%s_pal.%s", fileNameBase, suffix);
  printImage = pltPaletteptr->GetPictureXImage();
  imageSizeX = pltPaletteptr->PaletteWidth();
  imageSizeY = pltPaletteptr->PaletteHeight();
  if(AVGlobals::IsSGIrgbFile()) {
    WriteRGBFile(rgbfilename, printImage, imageSizeX, imageSizeY, *pltPaletteptr);
  } else {
    WritePPMFile(rgbfilename, printImage, imageSizeX, imageSizeY, *pltPaletteptr);
  }

  XtFree(fileNameBase);
  XtDestroyWidget(w);

}  // end DoCreateRGBFile


// -------------------------------------------------------------------
void PltApp::DoCreateFABFile(Widget w, XtPointer, XtPointer call_data) {
  XmSelectionBoxCallbackStruct *cbs = (XmSelectionBoxCallbackStruct *) call_data;
  char fabfilename[Amrvis::BUFSIZE];
  char *fileNameBase;
  XmStringGetLtoR(cbs->value, XmSTRING_DEFAULT_CHARSET, &fileNameBase);
  sprintf(fabfilename, "%s.fab", fileNameBase);
  string fabFileName(fabfilename);
  int maxDrawnLevel(pltAppState->MaxDrawnLevel());
        
  string derivedQuantity(pltAppState->CurrentDerived());
  Vector<Box> bx = amrPicturePtrArray[0]->GetSubDomain();
  DataServices::Dispatch(DataServices::WriteFabOneVar,
			 dataServicesPtr[currentFrame],
                         (void *) &fabFileName,
			 (void *) &(bx[maxDrawnLevel]),
			 maxDrawnLevel,
			 (void *) &derivedQuantity);
  XtFree(fileNameBase);
  XtDestroyWidget(w);
}  // end DoCreateFABFile



// -------------------------------------------------------------------
void PltApp::DoCreateAnimRGBFile() {
  char outFileName[Amrvis::BUFSIZE];
  int imageSizeX, imageSizeY;
  XImage *printImage;

  ResetAnimation();

  char suffix[32];
  if(AVGlobals::IsSGIrgbFile()) {
    strcpy(suffix, "rgb");
  } else {
    strcpy(suffix, "ppm");
  }
  sprintf(outFileName, "%s_%s.F%05i.%s", pltAppState->CurrentDerived().c_str(),
	  AVGlobals::StripSlashes(fileNames[currentFrame]).c_str(),
	  currentFrame, suffix);

  cout << "******* Creating file:  " << outFileName << endl;

  // write the picture
  printImage = amrPicturePtrArray[Amrvis::ZPLANE]->GetPictureXImage();
  imageSizeX = amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeH();
  imageSizeY = amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeV();
  if(AVGlobals::IsSGIrgbFile()) {
    WriteRGBFile(outFileName, printImage, imageSizeX, imageSizeY, *pltPaletteptr);
  } else {
    if(AVGlobals::IsAnnotated()) {
      const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
      Real time(amrData.Time());
      WritePPMFileAnnotated(outFileName, printImage, imageSizeX, imageSizeY,
                            *pltPaletteptr, currentFrame, time, gaPtr,
			    AVGlobals::StripSlashes(fileNames[currentFrame]),
			    pltAppState->CurrentDerived());
    } else {
      WritePPMFile(outFileName, printImage, imageSizeX, imageSizeY, *pltPaletteptr);
    }
  }

#if (BL_SPACEDIM == 2)
  for(int dim(0); dim < BL_SPACEDIM; ++dim) {
    if(XYplotwin[dim]) {
      if(dim == 0) {
        strcpy(suffix, "X.dat");
      } else {
        strcpy(suffix, "Y.dat");
      }
      sprintf(outFileName, "%s_%s.%s", pltAppState->CurrentDerived().c_str(),
	      AVGlobals::StripSlashes(fileNames[currentFrame]).c_str(),
	      suffix);
      cout << "******* Creating xyline file:  " << outFileName << endl;
      FILE *fs = fopen(outFileName, "w");
      if(fs == NULL) {
        cerr << "*** Error:  could not open file:  " << outFileName << endl;
      } else {
        XYplotwin[dim]->DoASCIIDump(fs, outFileName);
        fclose(fs);
      }
    }
  }
#endif

}  // end DoCreateAnimRGBFile
// -------------------------------------------------------------------
// -------------------------------------------------------------------

