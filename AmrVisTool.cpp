
//
// $Id: AmrVisTool.cpp,v 1.78 2011-01-13 18:56:40 vince Exp $
//

// ---------------------------------------------------------------
// AmrVisTool.cpp
// ---------------------------------------------------------------

#include "ParallelDescriptor.H"

#include <stdio.h>
#if ! (defined(BL_OSF1) || defined(BL_Darwin) || defined(BL_AIX) || defined(BL_IRIX64) || defined(BL_CYGWIN_NT) || defined(BL_CRAYX1))
#include <endian.h>
#endif

// X/Motif headers
#include <Xm/Xm.h>
#include <Xm/FileSB.h>
#include <Xm/Form.h>
#include <X11/Xos.h>
// BoxLib has index member functions, Xos might define it (LessTif).
#undef index

#include "MessageArea.H"
#include "GraphicsAttributes.H"
#include "Palette.H"
#include "PltApp.H"
#include "GlobalUtilities.H"
#include "ParmParse.H"
#include "DataServices.H"
#include "PltAppState.H"

#ifdef BL_VOLUMERENDER
#include "VolRender.H"
#endif

#ifdef BL_USE_ARRAYVIEW
#include "ArrayView.H"
#endif

using std::cout;
using std::cerr;
using std::endl;
using std::min;
using std::max;

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
list<PltApp *>  pltAppList;


//--------------------------------------------------------------
void PrintMessage(char *message) {
  sprintf(buffer, "%s", message);
  messageText.PrintText(buffer);
}


//--------------------------------------------------------------
int main(int argc, char *argv[]) {
  Box		comlineBox;
  string	comlineFileName;

  // here we trick boxlib
  int argcNoPP(1);
  BoxLib::Initialize(argcNoPP, argv);

  AVGlobals::GetDefaults("amrvis.defaults");

  AVGlobals::ParseCommandLine(argc, argv);

  if(AVGlobals::Verbose()) {
    AmrData::SetVerbose(true);
  }
  AmrData::SetSkipPltLines(AVGlobals::GetSkipPltLines());
  AmrData::SetStaticBoundaryWidth(AVGlobals::GetBoundaryWidth());

  if(AVGlobals::SleepTime() > 0) {
    sleep(AVGlobals::SleepTime());
  }

  if(AVGlobals::GivenBox()) {
    comlineBox = AVGlobals::GetBoxFromCommandLine();
  }

  if(AVGlobals::GetFabOutFormat() == 1) {
    DataServices::SetFabOutSize(1);
  }
  if(AVGlobals::GetFabOutFormat() == 8) {
    DataServices::SetFabOutSize(8);
  }
  if(AVGlobals::GetFabOutFormat() == 32) {
    DataServices::SetFabOutSize(32);
  }

  bool bBatchMode(false);
  if(AVGlobals::CreateSWFData() || AVGlobals::DumpSlices() ||
     AVGlobals::GivenBoxSlice())
  {
    bBatchMode = true;
  }
  if(bBatchMode && AVGlobals::IsAnimation()) {
    BoxLib::Abort("Batch mode and animation mode are incompatible.");
  }

  if(bBatchMode) {
    DataServices::SetBatchMode();
    BatchFunctions();
    //ParallelDescriptor::EndParallel();
    BoxLib::Finalize();
  } else {

    if(ParallelDescriptor::IOProcessor()) {
      CreateMainWindow(argc, argv);
    }

    if(AVGlobals::IsAnimation()) {
      BL_ASSERT(AVGlobals::GetFileCount() > 0);
      bool bAmrDataOk(true);
      FileType fileType = AVGlobals::GetDefaultFileType();
      BL_ASSERT(fileType != INVALIDTYPE);
      Array<DataServices *> dspArray(AVGlobals::GetFileCount());
      for(int nPlots = 0; nPlots < AVGlobals::GetFileCount(); ++nPlots) {
        comlineFileName = AVGlobals::GetComlineFilename(nPlots);
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
          PltApp *temp = new PltApp(app, wTopLevel,
	                            AVGlobals::GetComlineFilename(0),
			            dspArray, AVGlobals::IsAnimation());
	  if(temp == NULL) {
	    cerr << "Error:  could not make a new PltApp." << endl;
            for(int nPlots = 0; nPlots < AVGlobals::GetFileCount(); ++nPlots) {
              dspArray[nPlots]->DecrementNumberOfUsers();
	    }
	  } else {
            pltAppList.push_back(temp);
              if(AVGlobals::GivenBox()) {
		DataServices *dsp = temp->GetDataServicesPtr();
	        const AmrData &amrData = dsp->AmrDataRef();
		Box bPD(amrData.ProbDomain()[amrData.FinestLevel()]);
		Box itypComlineBox(bPD);  // for correct box type
		itypComlineBox.setSmall(comlineBox.smallEnd());
		itypComlineBox.setBig(comlineBox.bigEnd());
		Box comlineBoxErr(itypComlineBox);
		itypComlineBox &= bPD;
		if(itypComlineBox.ok()) {
                  SubregionPltApp(wTopLevel, comlineBox, comlineBox.smallEnd(),
		         temp, temp->GetPaletteName(), AVGlobals::IsAnimation(),
		         temp->GetPltAppState()->CurrentDerived(), comlineFileName);
		  CBQuitPltApp(NULL, temp, NULL);
		} else {
	          cerr << "Error:  bad subregion box on the command line:  "
		       << comlineBoxErr << endl;
		}
              }
	  }
	} else {
          if(ParallelDescriptor::IOProcessor()) {
            for(int nPlots = 0; nPlots < AVGlobals::GetFileCount(); ++nPlots) {
              dspArray[nPlots]->DecrementNumberOfUsers();
	    }
	  }
	}
      }
    } else {
      // loop through the command line list of plot files
      FileType fileType = AVGlobals::GetDefaultFileType();
      BL_ASSERT(fileType != INVALIDTYPE);

      Array<DataServices *> dspArray(AVGlobals::GetFileCount());
      for(int nPlots(0); nPlots < AVGlobals::GetFileCount(); ++nPlots) {
        comlineFileName = AVGlobals::GetComlineFilename(nPlots);
        if(ParallelDescriptor::IOProcessor()) {
          cout << endl << "FileName = " << comlineFileName << endl;
        }
	dspArray[nPlots] = new DataServices(comlineFileName, fileType);
      }

      for(int nPlots(0); nPlots < AVGlobals::GetFileCount(); ++nPlots) {
        if(ParallelDescriptor::IOProcessor()) {
	  if(dspArray[nPlots]->AmrDataOk()) {
	    Array<DataServices *> dspArrayOne(1);
	    dspArrayOne[0] = dspArray[nPlots];
            PltApp *temp = new PltApp(app, wTopLevel, dspArrayOne[0]->GetFileName(),
			              dspArrayOne, AVGlobals::IsAnimation());
	    if(temp == NULL) {
	      cerr << "Error:  could not make a new PltApp." << endl;
	    } else {
              pltAppList.push_back(temp);
              dspArray[nPlots]->IncrementNumberOfUsers();
              if(AVGlobals::GivenBox()) {
		DataServices *dsp = temp->GetDataServicesPtr();
	        const AmrData &amrData = dsp->AmrDataRef();
		Box bPD(amrData.ProbDomain()[amrData.FinestLevel()]);
		Box itypComlineBox(bPD);  // for correct box type
		itypComlineBox.setSmall(comlineBox.smallEnd());
		itypComlineBox.setBig(comlineBox.bigEnd());
		Box comlineBoxErr(itypComlineBox);
		itypComlineBox &= bPD;
		if(itypComlineBox.ok()) {
                  SubregionPltApp(wTopLevel, comlineBox, comlineBox.smallEnd(),
		         temp, temp->GetPaletteName(), AVGlobals::IsAnimation(),
		         temp->GetPltAppState()->CurrentDerived(), dsp->GetFileName());
		  CBQuitPltApp(NULL, temp, NULL);
		} else {
	          cerr << "Error:  bad subregion box on the command line:  "
		       << comlineBoxErr << endl;
		}
              }
	    }
	  }
        }
      }  // end for(nPlots...)
    }  // end if(AVGlobals::IsAnimation())


    if(ParallelDescriptor::IOProcessor()) {
      XtAppMainLoop(app);
    } else {
      // all other processors wait for a request
      DataServices::Dispatch(DataServices::InvalidRequestType, NULL, NULL);
    }

  }  // end if(bBatchMode)

  return 0;

}  // end main()

 
// ---------------------------------------------------------------
void CreateMainWindow(int argc, char *argv[]) {
  int i;
  string	comlineFileName;

  String fallbacks[] = {"*fontList:variable=charset",
			NULL };


  wTopLevel = XtVaAppInitialize(&app, "AmrVisTool", NULL, 0, 
                                (int *) &argc, argv, fallbacks,
			XmNx,		350,
			XmNy,		10,
                        XmNwidth,	500,
			XmNheight,	150,
			NULL);
  GraphicsAttributes *theGAPtr = new GraphicsAttributes(wTopLevel);
  if(theGAPtr->PVisual() != XDefaultVisual(theGAPtr->PDisplay(), 
                                        theGAPtr->PScreenNumber()))
  {
      Colormap colormap = XCreateColormap(theGAPtr->PDisplay(), 
                                          RootWindow(theGAPtr->PDisplay(),
                                                     theGAPtr->PScreenNumber()),
                                          theGAPtr->PVisual(), AllocNone);
      XtVaSetValues(wTopLevel, XmNvisual, theGAPtr->PVisual(), XmNdepth, 8,
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
  string	comlineFileName;

  // loop through the command line list of plot files
  for(int nPlots = 0; nPlots < AVGlobals::GetFileCount(); ++nPlots) {
    comlineFileName = AVGlobals::GetComlineFilename(nPlots);
    if(ParallelDescriptor::IOProcessor()) {
      cout << "FileName = " << comlineFileName << endl;
    }
    FileType fileType = AVGlobals::GetDefaultFileType();
    BL_ASSERT(fileType != INVALIDTYPE);
    DataServices dataServices(comlineFileName, fileType);

    string derived(AVGlobals::GetInitialDerived());
    if( ! dataServices.CanDerive(derived)) {
      if(ParallelDescriptor::IOProcessor()) {
        cerr << "Bad initial derived:  cannot derive " << derived << endl;
      }
      derived = dataServices.PlotVarNames()[0];
      if(ParallelDescriptor::IOProcessor()) {
        cerr << "Defaulting to " << derived << endl;
      }
      AVGlobals::SetInitialDerived(derived);
    }


#if (BL_SPACEDIM == 3)
#ifdef BL_VOLUMERENDER
    if(AVGlobals::CreateSWFData()) {
      AmrData &amrData = dataServices.AmrDataRef();
      BL_ASSERT(dataServices.CanDerive(AVGlobals::GetInitialDerived()));
      int minDrawnLevel = 0;
      int maxDrawnLevel;
      if(AVGlobals::UseMaxLevel()) {
        maxDrawnLevel = max(0, min(AVGlobals::GetMaxLevel(), amrData.FinestLevel()));
      } else {
        maxDrawnLevel = amrData.FinestLevel();
      }
      if(ParallelDescriptor::IOProcessor()) {
        cout << "_in BatchFunctions:  using max level = " << maxDrawnLevel << endl;
      }
      Array<Box> drawDomain = amrData.ProbDomain();

      if(AVGlobals::GivenBox()) {
        Box comlineBox = AVGlobals::GetBoxFromCommandLine();
        //cout << "+++++++++++++++++++++++ comlinebox    = " << comlineBox << endl;
        int finelev(amrData.FinestLevel());
	if(amrData.ProbDomain()[finelev].contains(comlineBox) == false) {
	  cerr << "Error:  bad comlineBox:  probDomain(finestLevel) = "
	       << amrData.ProbDomain()[finelev] << endl;
	  BoxLib::Abort("Exiting.");
	}
        drawDomain[finelev] = comlineBox;
        for(int ilev(amrData.FinestLevel() - 1); ilev >= 0; --ilev) {
	  int crr(AVGlobals::CRRBetweenLevels(ilev, finelev, amrData.RefRatio()));
          drawDomain[ilev] = drawDomain[finelev];
          drawDomain[ilev].coarsen(crr);
        //cout << "++++++ drawDomain[" << ilev << "] = " << drawDomain[ilev] << endl;
        }
      }

      int iPaletteStart(3);
      int iPaletteEnd(AVGlobals::MaxPaletteIndex());
      int iBlackIndex(0);
      int iWhiteIndex(1);
      int iColorSlots(AVGlobals::MaxPaletteIndex() + 1 - iPaletteStart);
      Palette volPal(PALLISTLENGTH, PALWIDTH, TOTALPALWIDTH, TOTALPALHEIGHT, 0);
      if(ParallelDescriptor::IOProcessor()) {
        cout << "_in BatchFunctions:  palette name = "
             << AVGlobals::GetPaletteName() << endl;
      }
      volPal.ReadSeqPalette(AVGlobals::GetPaletteName(), false);
      VolRender volRender(drawDomain, minDrawnLevel, maxDrawnLevel, &volPal,
			  AVGlobals::GetLightingFileName());
      Real dataMin, dataMax;
      if(AVGlobals::UseSpecifiedMinMax()) {
        AVGlobals::GetSpecifiedMinMax(dataMin, dataMax);
      } else {
        amrData.MinMax(drawDomain[maxDrawnLevel], AVGlobals::GetInitialDerived(),
                       maxDrawnLevel, dataMin, dataMax);
      }

      volRender.MakeSWFData(&dataServices, dataMin, dataMax,
                            AVGlobals::GetInitialDerived(),
                            iPaletteStart, iPaletteEnd,
                            iBlackIndex, iWhiteIndex, iColorSlots,
			    PltApp::GetDefaultShowBoxes());
      volRender.WriteSWFData(comlineFileName, AVGlobals::MakeSWFLight());

    } 
#endif
#endif

    if(AVGlobals::DumpSlices()) {
	if(AVGlobals::UseMaxLevel() == true) {
	  dataServices.SetWriteToLevel(AVGlobals::GetMaxLevel());
	}
        if(AVGlobals::SliceAllVars()) {
          for(int slicedir(0); slicedir < AVGlobals::GetDumpSlices().size();
	      ++slicedir)
          {
	    for(list<int>::iterator li =
	                   AVGlobals::GetDumpSlices()[slicedir].begin();
	        li != AVGlobals::GetDumpSlices()[slicedir].end();
		++li)
	    {
              int slicenum = *li;
              DataServices::Dispatch(DataServices::DumpSlicePlaneAllVars,
				     &dataServices, slicedir, slicenum);
	    }
          }
        } else {
            for(int slicedir(0); slicedir < AVGlobals::GetDumpSlices().size();
	        ++slicedir)
            {
	      for(list<int>::iterator li =
	               AVGlobals::GetDumpSlices()[slicedir].begin();
	          li != AVGlobals::GetDumpSlices()[slicedir].end();
		  ++li)
	      {
                int slicenum = *li;
                DataServices::Dispatch(DataServices::DumpSlicePlaneOneVar,
				       &dataServices, slicedir, slicenum,
				       (void *) &derived);
	      }
            }
        }
    }   // end if(AVGlobals::DumpSlices())

    if(AVGlobals::GivenBoxSlice()) {
	if(AVGlobals::UseMaxLevel() == true) {
	  dataServices.SetWriteToLevel(AVGlobals::GetMaxLevel());
	}
        Box comLineBox(AVGlobals::GetBoxFromCommandLine());
	BL_ASSERT(comLineBox.ok());
        if(AVGlobals::SliceAllVars()) {
          DataServices::Dispatch(DataServices::DumpSliceBoxAllVars,
				 &dataServices,
				 (void *) &comLineBox);
        } else {
            DataServices::Dispatch(DataServices::DumpSliceBoxOneVar,
				   &dataServices,
                                   (void *) &comLineBox,
				   (void *) &derived);
        }
    }  // end if(AVGlobals::GivenBoxSlice())

  }  // end for(nPlots...)

}  // end BatchFunctions


// ---------------------------------------------------------------
void QuitAll() {
  for(list<PltApp *>::iterator li = pltAppList.begin();
      li != pltAppList.end(); ++li)
  {
    PltApp *obj = *li;
    Array<DataServices *> dataServicesPtr = obj->GetDataServicesPtrArray();
    for(int ids(0); ids < dataServicesPtr.size(); ++ids) {
      dataServicesPtr[ids]->DecrementNumberOfUsers();
    }
    delete obj;
  }
  pltAppList.clear();
  DataServices::Dispatch(DataServices::ExitRequest, NULL);
}

 
// ---------------------------------------------------------------
void CBFileMenu(Widget, XtPointer client_data, XtPointer) {
  Arg args[MAXARGS];
  int i = 0;
  unsigned long item = (unsigned long) client_data;

  if(item == QUITITEM) {
    QuitAll();
  } else if(item == OPENITEM) {
    i = 0;
    FileType fileType(AVGlobals::GetDefaultFileType());
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
  FileType fileType(AVGlobals::GetDefaultFileType());
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

  DataServices *dataServicesPtr = new DataServices();
  dataServicesPtr->Init(filename, AVGlobals::GetDefaultFileType());

  DataServices::Dispatch(DataServices::NewRequest, dataServicesPtr, NULL);

  Array<DataServices *> dspArray(1);
  dspArray[0] = dataServicesPtr;
  bool bIsAnim(false);

  PltApp *temp = new PltApp(app, wTopLevel, filename, dspArray, bIsAnim);

  if(temp == NULL) {
    cerr << "Error:  could not make a new PltApp." << endl;
  } else {
    pltAppList.push_back(temp);
    dspArray[0]->IncrementNumberOfUsers();
  }

  XtPopdown(XtParent(w));
}


// ---------------------------------------------------------------
void SubregionPltApp(Widget wTopLevel, const Box &trueRegion,
		     const IntVect &offset,
		     PltApp *pltparent,
		     const string &palfile, int isAnim,
		     const string &currentderived, const string &file)
{
  PltApp *temp = new PltApp(app, wTopLevel, trueRegion, offset,
		    pltparent, palfile, isAnim, currentderived, file);
  if(temp == NULL) {
    cerr << "Error in SubregionPltApp:  could not make a new PltApp." << endl;
  } else {
    pltAppList.push_back(temp);
    Array<DataServices *> dataServicesPtr = temp->GetDataServicesPtrArray();
    for(int ids(0); ids < dataServicesPtr.size(); ++ids) {
      dataServicesPtr[ids]->IncrementNumberOfUsers();
    }
  }
}


// ---------------------------------------------------------------
void CBQuitPltApp(Widget ofPltApp, XtPointer client_data, XtPointer) {
  PltApp *obj = (PltApp *) client_data;
  pltAppList.remove(obj);

  Array<DataServices *> &dataServicesPtr = obj->GetDataServicesPtrArray();
  for(int ids(0); ids < dataServicesPtr.size(); ++ids) {
    dataServicesPtr[ids]->DecrementNumberOfUsers();
    DataServices::Dispatch(DataServices::DeleteRequest, dataServicesPtr[ids], NULL);
  }

  delete obj;
}


// ---------------------------------------------------------------
void CBQuitAll(Widget, XtPointer, XtPointer) {
  QuitAll();
}
// ---------------------------------------------------------------
// ---------------------------------------------------------------
