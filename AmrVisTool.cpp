//--------------------------------------------------------------
// AmrVisTool.cpp
//--------------------------------------------------------------
#include <stream.h>
#include <strstream.h>
#include <iostream.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

// X/Motif headers
#include <Xm.h>
#include <MainW.h>
#include <PushB.h>
#include <PushBG.h>
#include <FileSB.h>
#include <MessageB.h>
#include <Label.h>
#include <Text.h>
#include <RowColumn.h>
#include <Form.h>
#include <DrawingA.h>
#include <ToggleB.h>
#include <ToggleBG.h>
#include <Xos.h>

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
void CBPrefMenu(Widget, XtPointer, XtPointer);
void CBHelpMenu(Widget, XtPointer, XtPointer);

XtAppContext	app;
Widget		wTopLevel, wTextOut, wDialog;
Widget	wMainWindow, wMenuBar;
XmString	sFile, sOpen, sQuit, sPreferences, sSet, sHelp, sHelpMenu;
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
void main(int argc, char *argv[]) {
  Box		comlineBox;
  aString	comlineFileName;

  ParmParse pp(0, argv, NULL, NULL);

  GetDefaults("amrvis.defaults");

  ParseCommandLine(argc, argv);

  if(Verbose()) {
    AmrData::SetVerbose(true);
  }
  AmrData::SetSkipPltLines(GetSkipPltLines());
  AmrData::SetStaticBoundaryWidth(GetBoundaryWidth());

  StartParallel(NProcs());

  if(SleepTime() > 0) {
    sleep(SleepTime());
  }

  if(GivenBox()) {
    comlineBox = GetBoxFromCommandLine();
  }

  int pltApps(GetFileCount());
  bool bBatchMode(false);
  if(MakeSWFData() || DumpSlices() || GivenBoxSlice()) {
    bBatchMode = true;
  }
  if(bBatchMode && IsAnimation()) {
    ParallelDescriptor::Abort("Batch mode and animation mode are incompatible.");
  }


  if(bBatchMode) {
    DataServices::SetBatchMode();
    BatchFunctions();
    EndParallel();
  } else {

    if(ParallelDescriptor::IOProcessor()) {
      CreateMainWindow(argc, argv);
    }

    if(IsAnimation() && pltApps > 0) {
      pltApps = 1; 
    }


    // loop through the command line list of plot files
    for(int nPlots = 0; nPlots < pltApps; nPlots++) {
      comlineFileName = GetComlineFilename(nPlots);
      if(ParallelDescriptor::IOProcessor()) {
        cout << "FileName = " << comlineFileName << endl;
      }
      FileType fileType = GetDefaultFileType();
      assert(fileType != INVALIDTYPE);
      DataServices *dataServicesPtr = new DataServices(comlineFileName, fileType);

      if(GivenBox()) {
        ParallelDescriptor::Abort("Command line subbox not supported yet.");
      }


      if(ParallelDescriptor::IOProcessor()) {
	if(dataServicesPtr->AmrDataOk()) {
          PltApp *temp = new PltApp(app, wTopLevel, comlineFileName,
			            dataServicesPtr, IsAnimation());
	  if(temp == NULL) {
	    cerr << "Error:  could not make a new PltApp." << endl;
	  } else {
            pltAppList.append(temp);
            dataServicesPtr->IncrementNumberOfUsers();
            //cout << ">>>> pltAppList appending " << temp << endl;
	  }
	}
      }

    }  // end for(nPlots...)


    if(ParallelDescriptor::IOProcessor()) {
      XtAppMainLoop(app);
    } else {
      // all other processors wait for a request
      DataServices::Dispatch(DataServices::InvalidRequestType, NULL, NULL);
    }

  }  // end if(bBatchMode)

  //EndParallel();
}  // end main()

 
// ---------------------------------------------------------------
void CreateMainWindow(int argc, char *argv[]) {
  int i;
  aString	comlineFileName;

#ifdef CRAY
#  if (BL_SPACEDIM == 2)
     String fallbacks[6];
     fallbacks[0] = "*fontList:7x13=charset";
     fallbacks[1] = "*bottomShadowColor:gray";
     fallbacks[2] = "*topShadowColor:gray";
     fallbacks[3] = "*background:black";
     fallbacks[4] = "*foreground:white";
     fallbacks[5] = NULL;

#  endif
#  if (BL_SPACEDIM == 3)
     //String fallbacks[6];
     String fallbacks[4];
     fallbacks[0] = "*fontList:7x13=charset";
     fallbacks[1] = "*bottomShadowColor:gray";
     fallbacks[2] = "*topShadowColor:gray";
     fallbacks[3] = NULL;
     //fallbacks[3] = "*background:black";
     //fallbacks[4] = "*foreground:white";
     //fallbacks[5] = NULL;
     //String fallbacks[] = {"*fontList:7x13=charset",
                           //"*background:black",
                           //"*foreground:white",
                           //"*bottomShadowColor:gray28",
                           //"*topShadowColor:gray42",
                           //NULL };
#  endif

#else

  //String fallbacks[] = {"*fontList:7x13=charset", "*background:0",
			//"*foreground:white", "*bottomShadowColor:gray16",
			//"*topShadowColor:gray12", NULL };
#if (BL_SPACEDIM == 2)
  String fallbacks[] = {"*fontList:7x13=charset", "*background:black",
			"*foreground:white", "*bottomShadowColor:gray",
			"*topShadowColor:gray", NULL };
#endif
#if (BL_SPACEDIM == 3)
  String fallbacks[] = {"*fontList:7x13=charset",
			//"*background:black",
			//"*foreground:white",
			"*bottomShadowColor:gray",
			"*topShadowColor:gray",
			NULL };
#endif

#endif


  wTopLevel = XtVaAppInitialize(&app, "AmrVisTool", NULL, 0, 
			(int *) &argc, argv, fallbacks,
			XmNx,		350,
			XmNy,		10,
			XmNwidth,	500,
			XmNheight,	150,
			NULL);

  wMainWindow = XtVaCreateManagedWidget ("mainWindow", 
	          xmFormWidgetClass,   wTopLevel, 
	          XmNscrollBarDisplayPolicy, XmAS_NEEDED,
	          XmNscrollingPolicy,        XmAUTOMATIC,
	          NULL);

  sFile    = XmStringCreateSimple("File");
  sOpen    = XmStringCreateSimple("Open...");
  sQuit    = XmStringCreateSimple("Exit");
  sPreferences = XmStringCreateSimple("Preferences");
  sSet = XmStringCreateSimple("Set...");
  sHelp    = XmStringCreateSimple("Help");
  sHelpMenu    = XmStringCreateSimple("Help...");

  wMenuBar = XmVaCreateSimpleMenuBar(wMainWindow, "menuBar",
		XmVaCASCADEBUTTON, sFile, 'F',
		XmVaCASCADEBUTTON, sPreferences, 'P',
		XmVaCASCADEBUTTON, sHelp, 'H',
		XmNtopAttachment,	XmATTACH_FORM,
		XmNleftAttachment,	XmATTACH_FORM,
		XmNrightAttachment,	XmATTACH_FORM,
		XmNheight,		30,
		NULL);

  XmVaCreateSimplePulldownMenu(wMenuBar, "fileMenu", 0, CBFileMenu,
    XmVaPUSHBUTTON, sOpen, 'O', NULL, NULL,
    //XmVaSEPARATOR,   // the separator is buggy in old versions of motif
    XmVaPUSHBUTTON, sQuit, 'Q', NULL, NULL,
    NULL);

  XmVaCreateSimplePulldownMenu(wMenuBar, "prefMenu", 1, CBPrefMenu,
    XmVaPUSHBUTTON, sSet, 'O', NULL, NULL,
    NULL);

  XmVaCreateSimplePulldownMenu(wMenuBar, "helpMenu", 2, CBHelpMenu,
    XmVaPUSHBUTTON, sHelpMenu, 'O', NULL, NULL,
    NULL);

  XmStringFree(sFile);
  XmStringFree(sOpen);
  XmStringFree(sQuit);
  XmStringFree(sPreferences);
  XmStringFree(sSet);
  XmStringFree(sHelp);
  XmStringFree(sHelpMenu);

  XtManageChild(wMenuBar);

  i = 0;
  //XtSetArg(args[i], XmNrows,    20);       i++;
  //XtSetArg(args[i], XmNcolumns, 80);       i++;
  XtSetArg(args[i], XmNeditable, false);       i++;
  XtSetArg(args[i], XmNeditMode, XmMULTI_LINE_EDIT);       i++;
  XtSetArg(args[i], XmNwordWrap, true);       i++;
  XtSetArg(args[i], XmNscrollHorizontal, false);       i++;
  XtSetArg(args[i], XmNblinkRate, 0);       i++;
  XtSetArg(args[i], XmNautoShowCursorPosition, true);       i++;
  XtSetArg(args[i], XmNcursorPositionVisible,  false);      i++;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET);      i++;
  XtSetArg(args[i], XmNtopWidget, wMenuBar);      i++;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);      i++;
  XtSetArg(args[i], XmNbottomAttachment, XmATTACH_FORM);      i++;
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
  for(int nPlots = 0; nPlots < GetFileCount(); nPlots++) {
    comlineFileName = GetComlineFilename(nPlots);
    cout << "FileName = " << comlineFileName << endl;
    FileType fileType = GetDefaultFileType();
    assert(fileType != INVALIDTYPE);
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
      assert(dataServices.CanDerive(GetInitialDerived()));
      int minDrawnLevel = 0;
      int maxDrawnLevel;
      if(UseMaxLevel()) {
        maxDrawnLevel = Max(0, Min(GetMaxLevel(), amrData.FinestLevel()));
      } else {
        maxDrawnLevel = amrData.FinestLevel();
      }
      Array<Box> drawDomain = amrData.ProbDomain();

      VolRender volRender(drawDomain, minDrawnLevel, maxDrawnLevel);
      Real dataMin, dataMax;
      if(UseSpecifiedMinMax()) {
        GetSpecifiedMinMax(dataMin, dataMax);
      } else {
        amrData.MinMax(drawDomain[maxDrawnLevel], GetInitialDerived(),
                       maxDrawnLevel, dataMin, dataMax);
	//bool minMaxValid;
        //DataServices::Dispatch(DataServices::MinMaxRequest,
		       //&dataServices,
		       //drawDomain[maxDrawnLevel], GetInitialDerived(),
                       //maxDrawnLevel, dataMin, dataMax, minMaxValid);
      }

      int iPaletteStart = 2;
      int iPaletteEnd = 255;
      int iBlackIndex = 1;
      int iWhiteIndex = 0;
      int iColorSlots = 254;
      volRender.MakeSWFData(&dataServices, dataMin, dataMax, GetInitialDerived(),
                            iPaletteStart, iPaletteEnd,
                            iBlackIndex, iWhiteIndex, iColorSlots);
      volRender.WriteSWFData(comlineFileName);

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
                                       slicedir, slicenum, derived);
                ++li;
              }
            }
        }
    }   // end if(DumpSlices())

    if(GivenBoxSlice()) {
        Box comLineBox(GetBoxFromCommandLine());
	assert(comLineBox.ok());
        if(SliceAllVars()) {
          DataServices::Dispatch(DataServices::DumpSliceBoxAllVars,
				 &dataServices,
				 comLineBox);
        } else {
            DataServices::Dispatch(DataServices::DumpSliceBoxOneVar,
				   &dataServices,
                                   comLineBox, derived);
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
      //cout << "<<<< pltAppList removing " << obj << endl;
      DataServices *dataServicesPtr = obj->GetDataServicesPtr();
      dataServicesPtr->DecrementNumberOfUsers();
      delete obj;
    }
    pltAppList.clear();
    DataServices::Dispatch(DataServices::ExitRequest, NULL);
  }
  if(item == OPENITEM) {
    i=0;
    FileType fileType = GetDefaultFileType();
    XmString sMask;
    if(fileType==NEWPLT) {
      sMask = XmStringCreateSimple("plt*");
    } else if(fileType==FAB) {
      sMask = XmStringCreateSimple("*.fab");
    } else if(fileType==MULTIFAB) {
      sMask = XmStringCreateSimple("*.multifab");
    } else {
      sMask = XmStringCreateSimple("*");
    }

    XmString sNone = XmStringCreateSimple("none");
    if( ! XmStringCompare(sDirectory, sNone)) {
      XtSetArg (args[i], XmNdirectory, sDirectory); i++;
    }
    XtSetArg (args[i], XmNpattern, sMask); i++;
    XtSetArg (args[i], XmNfileTypeMask, XmFILE_ANY_TYPE); i++;
    wDialog = XmCreateFileSelectionDialog(wTopLevel, "Open File", args, i);
    XtAddCallback(wDialog, XmNokCallback, CBOpenPltFile,   NULL);
    XtAddCallback(wDialog, XmNcancelCallback, 
		  (XtCallbackProc)XtUnmanageChild, NULL);
    XmStringFree(sMask);
    XmStringFree(sNone);
    XtManageChild(wDialog);
    XtPopup(XtParent(wDialog), XtGrabExclusive);
  }
}


// ---------------------------------------------------------------
void CBOpenPltFile(Widget w, XtPointer, XtPointer call_data) {

  char *filename = NULL;
  if( ! XmStringGetLtoR(
	((XmFileSelectionBoxCallbackStruct*) call_data)->value,
	XmSTRING_DEFAULT_CHARSET,
	&filename))
  {
    cerr << "CBOpenPltFile : system error" << endl;
    return;
  }
  cout<<filename<<endl;
  char path[BUFSIZ];
  //cout << "_in CBOpenPltFile:  filename = " << filename << endl;
  strcpy(path, filename);
  int i = strlen(path) - 1;
  while(i>-1 && path[i] != '/') {
    --i;
  }
  path[i+1] = '\0';
  //cout << "_in CBOpenPltFile:  path = " << path << endl;
  sDirectory = XmStringCreateSimple(path);

  sprintf(buffer, "Selected file = %s\n", filename);
  messageText.PrintText(buffer);

  DataServices *dataServicesPtr = new DataServices(filename, GetDefaultFileType());
  DataServices::Dispatch(DataServices::NewRequest, dataServicesPtr, NULL);

  PltApp *temp = new PltApp(app, wTopLevel, filename, dataServicesPtr, false);
  if(temp == NULL) {
    cerr << "Error:  could not make a new PltApp." << endl;
  } else {
    pltAppList.append(temp);
    dataServicesPtr->IncrementNumberOfUsers();
    //cout << ">>>> pltAppList appending " << temp << endl;
  }

  XtPopdown(XtParent(w));
}


// ---------------------------------------------------------------
void SubregionPltApp(Widget wTopLevel, const Box &trueRegion,
		     const IntVect &offset,
		     AmrPicture *parentPicturePtr, PltApp *pltparent,
		     const aString &palfile, int isAnim,
		     const aString &currentderived, const aString &file)
{
  PltApp *temp = new PltApp(app, wTopLevel, trueRegion, offset, parentPicturePtr,
		    pltparent, palfile, isAnim, currentderived, file);
  if(temp == NULL) {
    cerr << "Error in SubregionPltApp:  could not make a new PltApp." << endl;
  } else {
    DataServices *dataServicesPtr = temp->GetDataServicesPtr();
    pltAppList.append(temp);
    dataServicesPtr->IncrementNumberOfUsers();
    //cout << ">>>> pltAppList appending " << temp << endl;
  }
}


// ---------------------------------------------------------------
void CBQuitPltApp(Widget ofPltApp, XtPointer client_data, XtPointer) {
  PltApp *obj = (PltApp *) client_data;
  pltAppList.remove(obj);
  //cout << "<<<< pltAppList removing " << obj << endl;

  DataServices *dataServicesPtr = obj->GetDataServicesPtr();
  dataServicesPtr->DecrementNumberOfUsers();
  DataServices::Dispatch(DataServices::DeleteRequest, dataServicesPtr, NULL);

  delete obj;
}


// ---------------------------------------------------------------
void CBPrefMenu(Widget, XtPointer, XtPointer) {
  /*
  static Widget wDialog, wPrefWindow, wTypeRadioBox, wTypeOkButton, wTypeRowCol;
  static Widget wToggle;
  Arg args[5];
  int i = 0;
  int item = (int) client_data;
  */
}


// ---------------------------------------------------------------
void CBHelpMenu(Widget, XtPointer client_data, XtPointer) {
  /*
  static Widget wDialog, wHelpWindow, wTypeOkButton, wTypeRowCol;
  static Widget wToggle;
  Arg args[5];
  int i = 0;
  int item = (int) client_data;

  if(item == HELPITEM) {
    wHelpWindow = XtVaCreatePopupShell("Help", topLevelShellWidgetClass,
   			wTopLevel,
			XmNwidth,	230,
			XmNheight,	320,
			XmNx,		250,
			XmNy,		150,
			NULL);

    wTypeRowCol = XtVaCreateManagedWidget("typerowcol", xmRowColumnWidgetClass,
    			wHelpWindow, NULL);

    wTypeOkButton = XmCreatePushButton(wTypeRowCol, "Ok", NULL, 0);

    XtManageChild(wHelpWindow);
    XtManageChild(wTypeOkButton);
    XtPopup(XtParent(wHelpWindow), XtGrabExclusive);
  }  
  */
}
// ---------------------------------------------------------------
// ---------------------------------------------------------------
