
//
// $Id: AmrVisTool.cpp,v 1.50 2001-05-09 20:03:36 vince Exp $
//

// ---------------------------------------------------------------
// AmrVisTool.cpp
// ---------------------------------------------------------------

#include <stdio.h>

// X/Motif headers
#include <Xm/Xm.h>
//  #include <MainW.h>
//  #include <PushB.h>
//  #include <PushBG.h>
#include <Xm/FileSB.h>
//  #include <MessageB.h>
//  #include <Label.h>
//  #include <Text.h>
//  #include <RowColumn.h>
#include <Xm/Form.h>
//  #include <DrawingA.h>
//  #include <ToggleB.h>
//  #include <ToggleBG.h>
#include <X11/Xos.h>
// BoxLib has index member functions, Xos might define it (LessTif).
#undef index

#include "MessageArea.H"
#include "GraphicsAttributes.H"
#include "Palette.H"
#include "PltApp.H"
#include "GlobalUtilities.H"
#include "ParmParse.H"
#include "ParallelDescriptor.H"
#include "DataServices.H"

#ifdef BL_VOLUMERENDER
#include "VolRender.H"
#endif

#ifdef BL_USE_ARRAYVIEW
#include "ArrayView.H"
#endif

const int OPENITEM = 0;
const int QUITITEM = 1;
//const int SETITEM  = 0;
//const int HELPITEM = 0;

void CreateMainWindow(int argc, char *argv[]);
void BatchFunctions();

// CallBack functions
void CBFileMenu(Widget, XtPointer, XtPointer);
void CBOpenPltFile(Widget, XtPointer, XtPointer);

XtAppContext	app;
Widget		wTopLevel, wTextOut, wDialog;
Widget	wMainWindow, wMenuBar;
Arg		args[32];
cMessageArea	messageText;
char		buffer[BUFSIZ];
XmString	sDirectory = XmStringCreateSimple("none");
List<PltApp *>  pltAppList;


//--------------------------------------------------------------
void PrintMessage(char *message) {
  sprintf(buffer, "%s", message);
  messageText.PrintText(buffer);
}


//--------------------------------------------------------------
int main(int argc, char *argv[]) {
  Box		comlineBox;
  aString	comlineFileName;

    ParallelDescriptor::StartParallel(&argc,&argv);

  ParmParse pp(0, argv, NULL, NULL);


  GetDefaults("amrvis.defaults");
  //GetLightingDefaults("amrvis.lighting");

  ParseCommandLine(argc, argv);

  if(Verbose()) {
    AmrData::SetVerbose(true);
  }
  AmrData::SetSkipPltLines(GetSkipPltLines());
  AmrData::SetStaticBoundaryWidth(GetBoundaryWidth());

  if(SleepTime() > 0) {
    sleep(SleepTime());
  }

  if(GivenBox()) {
    comlineBox = GetBoxFromCommandLine();
  }

  if(GetFabOutFormat() == 8) {
    DataServices::SetFabOutSize(8);
  }
  if(GetFabOutFormat() == 32) {
    DataServices::SetFabOutSize(32);
  }

  bool bBatchMode(false);
  if(MakeSWFData() || DumpSlices() || GivenBoxSlice()) {
    bBatchMode = true;
  }
  if(bBatchMode && IsAnimation()) {
    BoxLib::Abort("Batch mode and animation mode are incompatible.");
  }
  if(GivenBox()) {
    BoxLib::Abort("Command line subbox not supported yet.");
  }

  if(bBatchMode) {
    DataServices::SetBatchMode();
    BatchFunctions();
    ParallelDescriptor::EndParallel();
  } else {

    if(ParallelDescriptor::IOProcessor()) {
      CreateMainWindow(argc, argv);
    }

    if(IsAnimation()) {
      BL_ASSERT(GetFileCount() > 0);
      bool bAmrDataOk(true);
      FileType fileType = GetDefaultFileType();
      BL_ASSERT(fileType != INVALIDTYPE);
      Array<DataServices *> dspArray(GetFileCount());
      for(int nPlots = 0; nPlots < GetFileCount(); ++nPlots) {
        comlineFileName = GetComlineFilename(nPlots);
        dspArray[nPlots] = new DataServices(comlineFileName, fileType);
        if(ParallelDescriptor::IOProcessor()) {
          dspArray[nPlots]->IncrementNumberOfUsers();
	}
	if( ! dspArray[nPlots]->AmrDataOk()) {
	  bAmrDataOk = false;
	}
      }

      if(ParallelDescriptor::IOProcessor()) {
	if(bAmrDataOk) {
          PltApp *temp = new PltApp(app, wTopLevel, GetComlineFilename(0),
			            dspArray, IsAnimation());
	  if(temp == NULL) {
	    cerr << "Error:  could not make a new PltApp." << endl;
            for(int nPlots = 0; nPlots < GetFileCount(); ++nPlots) {
              dspArray[nPlots]->DecrementNumberOfUsers();
	    }
	  } else {
            pltAppList.append(temp);
	  }
	} else {
          if(ParallelDescriptor::IOProcessor()) {
            for(int nPlots = 0; nPlots < GetFileCount(); ++nPlots) {
              dspArray[nPlots]->DecrementNumberOfUsers();
	    }
	  }
	}
      }
    } else {
      // loop through the command line list of plot files
      FileType fileType = GetDefaultFileType();
      BL_ASSERT(fileType != INVALIDTYPE);
      for(int nPlots(0); nPlots < GetFileCount(); ++nPlots) {
        comlineFileName = GetComlineFilename(nPlots);
        if(ParallelDescriptor::IOProcessor()) {
          cout << endl << "FileName = " << comlineFileName << endl;
        }
	Array<DataServices *> dspArray(1);
	dspArray[0] = new DataServices(comlineFileName, fileType);

        if(ParallelDescriptor::IOProcessor()) {
	  if(dspArray[0]->AmrDataOk()) {
            PltApp *temp = new PltApp(app, wTopLevel, comlineFileName,
			              dspArray, IsAnimation());
	    if(temp == NULL) {
	      cerr << "Error:  could not make a new PltApp." << endl;
	    } else {
              pltAppList.append(temp);
              dspArray[0]->IncrementNumberOfUsers();
	    }
	  }
        }
      }  // end for(nPlots...)
    }  // end if(IsAnimation())

    if(ParallelDescriptor::IOProcessor()) {
      XtAppMainLoop(app);
    } else {
      // all other processors wait for a request
      DataServices::Dispatch(DataServices::InvalidRequestType, NULL, NULL);
    }

  }  // end if(bBatchMode)

  //ParallelDescriptor::EndParallel();
}  // end main()

 
// ---------------------------------------------------------------
void CreateMainWindow(int argc, char *argv[]) {
  int i;
  aString	comlineFileName;

  String fallbacks[] = {"*fontList:variable=charset",
			NULL };


  wTopLevel = XtVaAppInitialize(&app, "AmrVisTool", NULL, 0, 
                                (int *) &argc, argv, fallbacks,
			XmNx,		350,
			XmNy,		10,
                        XmNwidth,	500,
			XmNheight,	150,
			NULL);
  GraphicsAttributes *TheGAptr = new GraphicsAttributes(wTopLevel);
  if(TheGAptr->PVisual() != XDefaultVisual(TheGAptr->PDisplay(), 
                                        TheGAptr->PScreenNumber()))
  {
      Colormap colormap = XCreateColormap(TheGAptr->PDisplay(), 
                                          RootWindow(TheGAptr->PDisplay(),
                                                     TheGAptr->PScreenNumber()),
                                          TheGAptr->PVisual(), AllocNone);
      XtVaSetValues(wTopLevel, XmNvisual, TheGAptr->PVisual(), XmNdepth, 8,
                    XmNcolormap, colormap, NULL);
  }
  wMainWindow = XtVaCreateManagedWidget ("mainWindow", 
	          xmFormWidgetClass,   wTopLevel, 
	          XmNscrollBarDisplayPolicy, XmAS_NEEDED,
	          XmNscrollingPolicy,        XmAUTOMATIC,
	          NULL);

  XmString sFile    = XmStringCreateSimple("File");
  XmString sOpen    = XmStringCreateSimple("Open...");
  XmString sQuit    = XmStringCreateSimple("Quit");

  wMenuBar = XmVaCreateSimpleMenuBar(wMainWindow, "menuBar",
		XmVaCASCADEBUTTON, sFile, 'F',
		XmNtopAttachment,	XmATTACH_FORM,
		XmNleftAttachment,	XmATTACH_FORM,
		XmNrightAttachment,	XmATTACH_FORM,
		XmNheight,		30,
		NULL);

  XmString sCtrlQ = XmStringCreateSimple("Ctrl+Q");
  XmString sCtrlO = XmStringCreateSimple("Ctrl+O");
  XmVaCreateSimplePulldownMenu(wMenuBar, "fileMenu", 0,
    (XtCallbackProc) CBFileMenu,
    XmVaPUSHBUTTON, sOpen, 'O', "Ctrl<Key>O", sCtrlO,
    //XmVaSEPARATOR,   // the separator is buggy in old versions of motif
    XmVaPUSHBUTTON, sQuit, 'Q', "Ctrl<Key>Q", sCtrlQ,
    NULL);


  XmStringFree(sFile);
  XmStringFree(sOpen);
  XmStringFree(sQuit);
  XmStringFree(sCtrlO);
  XmStringFree(sCtrlQ);

  XtManageChild(wMenuBar);

  i = 0;
  //XtSetArg(args[i], XmNrows,    20);       ++i;
  //XtSetArg(args[i], XmNcolumns, 80);       ++i;
  XtSetArg(args[i], XmNeditable, false);       ++i;
  XtSetArg(args[i], XmNeditMode, XmMULTI_LINE_EDIT);       ++i;
  XtSetArg(args[i], XmNwordWrap, true);       ++i;
  XtSetArg(args[i], XmNscrollHorizontal, false);       ++i;
  XtSetArg(args[i], XmNblinkRate, 0);       ++i;
  XtSetArg(args[i], XmNautoShowCursorPosition, true);       ++i;
  XtSetArg(args[i], XmNcursorPositionVisible,  false);      ++i;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);      ++i;
  XtSetArg(args[i], XmNtopWidget, wMenuBar);      ++i;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);      ++i;
  XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);      ++i;
  XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);      ++i;
  wTextOut = XmCreateScrolledText(wMainWindow, "textOut", args, i);
  XtManageChild(wTextOut);

  messageText.Init(wTextOut);

  //XmMainWindowSetAreas(wMainWindow, wMenuBar, NULL, NULL, NULL, wTextOut);
  XtRealizeWidget(wTopLevel);
}  // end CreateMainWindow()

 
// ---------------------------------------------------------------
void BatchFunctions() {
  aString	comlineFileName;

  // loop through the command line list of plot files
  for(int nPlots = 0; nPlots < GetFileCount(); ++nPlots) {
    comlineFileName = GetComlineFilename(nPlots);
    cout << "FileName = " << comlineFileName << endl;
    FileType fileType = GetDefaultFileType();
    BL_ASSERT(fileType != INVALIDTYPE);
    DataServices dataServices(comlineFileName, fileType);

    aString derived(GetInitialDerived());
    if( ! dataServices.CanDerive(derived)) {
      if(ParallelDescriptor::IOProcessor()) {
        cerr << "Bad initial derived:  cannot derive " << derived << endl;
      }
      derived = dataServices.PlotVarNames()[0];
      if(ParallelDescriptor::IOProcessor()) {
        cerr << "Defaulting to " << derived << endl;
      }
      SetInitialDerived(derived);
    }


#if (BL_SPACEDIM == 3)
#ifdef BL_VOLUMERENDER
    if(MakeSWFData()) {
      AmrData &amrData = dataServices.AmrDataRef();
      BL_ASSERT(dataServices.CanDerive(GetInitialDerived()));
      int minDrawnLevel = 0;
      int maxDrawnLevel;
      if(UseMaxLevel()) {
        maxDrawnLevel = Max(0, Min(GetMaxLevel(), amrData.FinestLevel()));
      } else {
        maxDrawnLevel = amrData.FinestLevel();
      }
      Array<Box> drawDomain = amrData.ProbDomain();

      int iPaletteStart = 2;
      int iPaletteEnd = MaxPaletteIndex();
      int iBlackIndex = 1;
      int iWhiteIndex = 0;
      int iColorSlots = MaxPaletteIndex() + 1 - iPaletteStart;
      Palette volPal(PALLISTLENGTH, PALWIDTH, TOTALPALWIDTH, TOTALPALHEIGHT, 0);
      cout << "_in BatchFunctions:  palette name = " << GetPaletteName() << endl;
      volPal.ReadSeqPalette(GetPaletteName(), false);
      VolRender volRender(drawDomain, minDrawnLevel, maxDrawnLevel, &volPal);
      Real dataMin, dataMax;
      if(UseSpecifiedMinMax()) {
        GetSpecifiedMinMax(dataMin, dataMax);
      } else {
        amrData.MinMax(drawDomain[maxDrawnLevel], GetInitialDerived(),
                       maxDrawnLevel, dataMin, dataMax);
      }

      volRender.MakeSWFData(&dataServices, dataMin, dataMax, GetInitialDerived(),
                            iPaletteStart, iPaletteEnd,
                            iBlackIndex, iWhiteIndex, iColorSlots);
      volRender.WriteSWFData(comlineFileName, MakeSWFLight());

    } 
#endif
#endif

    if(DumpSlices()) {
        if(SliceAllVars()) {
          for(int slicedir=0; slicedir<GetDumpSlices().length(); ++slicedir) {
            ListIterator<int> li(GetDumpSlices()[slicedir]);
            while(li) {
              int slicenum = li();
              DataServices::Dispatch(DataServices::DumpSlicePlaneAllVars,
				     &dataServices,
                                     slicedir, slicenum);
              ++li;
            }
          }
        } else {
            for(int slicedir=0; slicedir<GetDumpSlices().length(); slicedir++) {
              ListIterator<int> li(GetDumpSlices()[slicedir]);
              while(li) {
                int slicenum = li();
                DataServices::Dispatch(DataServices::DumpSlicePlaneOneVar,
				       &dataServices,
                                       slicedir, slicenum,
				       (void *) &derived);
                ++li;
              }
            }
        }
    }   // end if(DumpSlices())

    if(GivenBoxSlice()) {
        Box comLineBox(GetBoxFromCommandLine());
	BL_ASSERT(comLineBox.ok());
        if(SliceAllVars()) {
          DataServices::Dispatch(DataServices::DumpSliceBoxAllVars,
				 &dataServices,
				 (void *) &comLineBox);
        } else {
            DataServices::Dispatch(DataServices::DumpSliceBoxOneVar,
				   &dataServices,
                                   (void *) &comLineBox,
				   (void *) &derived);
        }
    }  // end if(GivenBoxSlice())

  }  // end for(nPlots...)

}  // end BatchFunctions

 
// ---------------------------------------------------------------
void CBFileMenu(Widget, XtPointer client_data, XtPointer) {
  Arg args[MAXARGS];
  int i = 0;
  unsigned long item = (unsigned long) client_data;

  if(item == QUITITEM) {
    for(ListIterator<PltApp *> li(pltAppList); li; ++li) {
      PltApp *obj = pltAppList[li];
      Array<DataServices *> dataServicesPtr = obj->GetDataServicesPtrArray();
      for(int ids = 0; ids < dataServicesPtr.length(); ++ids) {
        dataServicesPtr[ids]->DecrementNumberOfUsers();
      }
      delete obj;
    }
    pltAppList.clear();
    DataServices::Dispatch(DataServices::ExitRequest, NULL);
  }
  if(item == OPENITEM) {
    i = 0;
    FileType fileType(GetDefaultFileType());
    XmString sMask;
    if(fileType == FAB) {
      sMask = XmStringCreateSimple("*.fab");
    } else if(fileType == MULTIFAB) {
      sMask = XmStringCreateSimple("*_H");
    } else {
      sMask = XmStringCreateSimple("plt*");
    }

    XmString sNone = XmStringCreateSimple("none");
    if( ! XmStringCompare(sDirectory, sNone)) {
      XtSetArg (args[i], XmNdirectory, sDirectory); ++i;
    }
    XtSetArg (args[i], XmNpattern, sMask); ++i;
    XtSetArg (args[i], XmNfileTypeMask, XmFILE_ANY_TYPE); ++i;
    wDialog = XmCreateFileSelectionDialog(wTopLevel, "Open File", args, i);
    XtAddCallback(wDialog, XmNokCallback, (XtCallbackProc) CBOpenPltFile,
		  NULL);
    XtAddCallback(wDialog, XmNcancelCallback, 
		  (XtCallbackProc) XtUnmanageChild, NULL);
    XmStringFree(sMask);
    XmStringFree(sNone);
    XtManageChild(wDialog);
    XtPopup(XtParent(wDialog), XtGrabExclusive);
  }
}


// ---------------------------------------------------------------
void CBOpenPltFile(Widget w, XtPointer, XtPointer call_data) {

  char *filename(NULL);
  if( ! XmStringGetLtoR(
	((XmFileSelectionBoxCallbackStruct*) call_data)->value,
	XmSTRING_DEFAULT_CHARSET,
	&filename))
  {
    cerr << "CBOpenPltFile : system error" << endl;
    return;
  }
  FileType fileType(GetDefaultFileType());
  if(fileType == MULTIFAB) {
    // delete the _H from the filename if it is there
    const char *uH = "_H";
    char *fm2 = filename + (strlen(filename) - 2);
    if(strcmp(uH, fm2) == 0) {
      filename[strlen(filename) - 2] = '\0';
    }
  }
  char path[BUFSIZ];
  strcpy(path, filename);
  int pathPos(strlen(path) - 1);
  while(pathPos > -1 && path[pathPos] != '/') {
    --pathPos;
  }
  path[pathPos + 1] = '\0';
  sDirectory = XmStringCreateSimple(path);

  sprintf(buffer, "Selected file = %s\n", filename);
  messageText.PrintText(buffer);

  DataServices *dataServicesPtr = new DataServices(filename, GetDefaultFileType());
  DataServices::Dispatch(DataServices::NewRequest, dataServicesPtr, NULL);

  Array<DataServices *> dspArray(1);
  dspArray[0] = dataServicesPtr;
  bool bIsAnim(false);
  PltApp *temp = new PltApp(app, wTopLevel, filename, dspArray, bIsAnim);
  if(temp == NULL) {
    cerr << "Error:  could not make a new PltApp." << endl;
  } else {
    pltAppList.append(temp);
    dspArray[0]->IncrementNumberOfUsers();
  }

  XtPopdown(XtParent(w));
}


// ---------------------------------------------------------------
void SubregionPltApp(Widget wTopLevel, const Box &trueRegion,
		     const IntVect &offset,
		     //AmrPicture *parentPicturePtr,
		     PltApp *pltparent,
		     const aString &palfile, int isAnim,
		     const aString &currentderived, const aString &file)
{
  PltApp *temp = new PltApp(app, wTopLevel, trueRegion, offset,
		    //parentPicturePtr,
		    pltparent, palfile, isAnim, currentderived, file);
  if(temp == NULL) {
    cerr << "Error in SubregionPltApp:  could not make a new PltApp." << endl;
  } else {
    pltAppList.append(temp);
    Array<DataServices *> dataServicesPtr = temp->GetDataServicesPtrArray();
    for(int ids = 0; ids < dataServicesPtr.length(); ++ids) {
      dataServicesPtr[ids]->IncrementNumberOfUsers();
    }
  }
}


// ---------------------------------------------------------------
void CBQuitPltApp(Widget ofPltApp, XtPointer client_data, XtPointer) {
  PltApp *obj = (PltApp *) client_data;
  pltAppList.remove(obj);

  Array<DataServices *> &dataServicesPtr = obj->GetDataServicesPtrArray();
  for(int ids = 0; ids < dataServicesPtr.length(); ++ids) {
    dataServicesPtr[ids]->DecrementNumberOfUsers();
    DataServices::Dispatch(DataServices::DeleteRequest, dataServicesPtr[ids], NULL);
  }

  delete obj;
}
// ---------------------------------------------------------------
// ---------------------------------------------------------------
