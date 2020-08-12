// ---------------------------------------------------------------
// AmrVisTool.cpp
// ---------------------------------------------------------------

#include <AMReX_ParallelDescriptor.H>
#include <AMReX_BLProfiler.H>

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

#include <MessageArea.H>
#include <GraphicsAttributes.H>
#include <Palette.H>
#include <PltApp.H>
#include <GlobalUtilities.H>
#include <AMReX_ParmParse.H>
#include <AMReX_DataServices.H>
#include <PltAppState.H>
#ifdef BL_USE_PROFPARSER
#include <ProfApp.H>
#include <AMReX_DataServices.H>
#endif

#ifdef BL_VOLUMERENDER
#include <VolRender.H>
#endif

#ifdef BL_USE_ARRAYVIEW
#include <ArrayView.H>
#endif

using std::cout;
using std::cerr;
using std::endl;

using namespace amrex;

const int OPENITEM = 0;
const int QUITITEM = 1;

void CreateMainWindow(int argc, char *argv[]);
void BatchFunctions();
extern void PrintProfParserBatchUsage(std::ostream &os);
extern bool ProfParserBatchFunctions(int argc, char *argv[], bool runDefault,
                                     bool &bParserProf);


// CallBack functions
void CBFileMenu(Widget, XtPointer, XtPointer);
void CBOpenPltFile(Widget, XtPointer, XtPointer);

XtAppContext	app;
Widget		wTopLevel, wTextOut, wDialog;
Widget	wMainWindow, wMenuBar;
Arg		args[amrex::Amrvis::MAXARGS];
cMessageArea	messageText;
char		buffer[amrex::Amrvis::BUFSIZE];
XmString	sDirectory = XmStringCreateSimple(const_cast<char *>("none"));
list<PltApp *>  pltAppList;
#ifdef BL_USE_PROFPARSER
  list<ProfApp *>  profAppList;
#endif


//--------------------------------------------------------------
void PrintMessage(const char *message) {
  sprintf(buffer, "%s", message);
  messageText.PrintText(buffer);
}


//--------------------------------------------------------------
int main(int argc, char *argv[]) {
  amrex::Box    comlineBox;
  string	comlineFileName;

  bool useParmParse(false);
  amrex::Initialize(argc, argv, useParmParse);

  AVGlobals::GetDefaults("amrvis.defaults");

#ifdef BL_USE_PROFPARSER
  amrex::BLProfiler::SetBlProfDirName("bl_prof_amrvis");

  if(argc > 2 && AVGlobals::IsProfDirName(argv[argc - 1])) {
    // ---- run the amrprofparser batch functions
    amrex::DataServices::SetBatchMode();
    bool runDefault(false), bParserProf(false), bAnyFunctionsRun;
    bAnyFunctionsRun = ProfParserBatchFunctions(argc, argv, runDefault, bParserProf);

    if( ! bAnyFunctionsRun) {
      if(amrex::ParallelDescriptor::IOProcessor()) {
        cout << '\n';
        cout << "Usage:  " << argv[0] << "  [options]  profddirname\n";
        PrintProfParserBatchUsage(cout);
        cout << endl;
      }
    }

    if (!bParserProf){
      amrex::BLProfiler::SetNoOutput();
    }
    amrex::Finalize();
    return 0;
  }
#endif

  AVGlobals::ParseCommandLine(argc, argv);

  amrex::AmrData::SetVerbose(AVGlobals::Verbose());
  amrex::AmrData::SetSkipPltLines(AVGlobals::GetSkipPltLines());
  amrex::AmrData::SetStaticBoundaryWidth(AVGlobals::GetBoundaryWidth());

  if(AVGlobals::SleepTime() > 0) {
    sleep(AVGlobals::SleepTime());
  }

  if(AVGlobals::GivenBox()) {
    comlineBox = AVGlobals::GetBoxFromCommandLine();
  }

  if(AVGlobals::GetFabOutFormat() == 1) {
    amrex::DataServices::SetFabOutSize(1);
  }
  if(AVGlobals::GetFabOutFormat() == 8) {
    amrex::DataServices::SetFabOutSize(8);
  }
  if(AVGlobals::GetFabOutFormat() == 32) {
    amrex::DataServices::SetFabOutSize(32);
  }

  bool bBatchMode(false);
  if(AVGlobals::CreateSWFData() || AVGlobals::DumpSlices() ||
     AVGlobals::GivenBoxSlice())
  {
    bBatchMode = true;
  }
  if(bBatchMode && AVGlobals::IsAnimation()) {
    amrex::Abort("Batch mode and animation mode are incompatible.");
  }

  if(bBatchMode) {
    amrex::DataServices::SetBatchMode();
    BatchFunctions();
    amrex::Finalize();
  } else {

    if(amrex::ParallelDescriptor::IOProcessor()) {
      CreateMainWindow(argc, argv);
    }

    if(AVGlobals::IsAnimation()) {
      BL_ASSERT(AVGlobals::GetFileCount() > 0);
      bool bAmrDataOk(true);
      amrex::Amrvis::FileType fileType = AVGlobals::GetDefaultFileType();
      BL_ASSERT(fileType != amrex::Amrvis::INVALIDTYPE);
      amrex::Vector<amrex::DataServices *> dspArray(AVGlobals::GetFileCount());
      for(int nPlots = 0; nPlots < AVGlobals::GetFileCount(); ++nPlots) {
        comlineFileName = AVGlobals::GetComlineFilename(nPlots);
        dspArray[nPlots] = new amrex::DataServices(comlineFileName, fileType);
        if(amrex::ParallelDescriptor::IOProcessor()) {
          dspArray[nPlots]->IncrementNumberOfUsers();
	}
	if( ! dspArray[nPlots]->AmrDataOk()) {
	  bAmrDataOk = false;
	}
      }

      if(amrex::ParallelDescriptor::IOProcessor()) {
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
		amrex::DataServices *dsp = temp->GetDataServicesPtr();
	        const amrex::AmrData &amrData = dsp->AmrDataRef();
		amrex::Box bPD(amrData.ProbDomain()[amrData.FinestLevel()]);
		amrex::Box itypComlineBox(bPD);  // for correct box type
		itypComlineBox.setSmall(comlineBox.smallEnd());
		itypComlineBox.setBig(comlineBox.bigEnd());
		amrex::Box comlineBoxErr(itypComlineBox);
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
          if(amrex::ParallelDescriptor::IOProcessor()) {
            for(int nPlots = 0; nPlots < AVGlobals::GetFileCount(); ++nPlots) {
              dspArray[nPlots]->DecrementNumberOfUsers();
	    }
	  }
	}
      }
    } else {
      // loop through the command line list of plot files
      amrex::Amrvis::FileType fileType = AVGlobals::GetDefaultFileType();
      BL_ASSERT(fileType != amrex::Amrvis::INVALIDTYPE);

#ifdef BL_USE_PROFPARSER
     if(fileType == amrex::Amrvis::PROFDATA) {
       if(amrex::ParallelDescriptor::IOProcessor()) {
         cout << "]]]]:  fileType is amrex::Amrvis::PROFDATA." << endl;
         PrintMessage("]]]]:  fileType is amrex::Amrvis::PROFDATA.\n");
       }
       if(AVGlobals::GetFileCount() == 1) {
	 string dirName(AVGlobals::GetComlineFilename(0));
	 cout << "]]]]]]]]:  dirName = " << dirName << endl;

         amrex::Vector<amrex::DataServices *> pdspArray(AVGlobals::GetFileCount());
         for(int nPlots(0); nPlots < AVGlobals::GetFileCount(); ++nPlots) {
           comlineFileName = AVGlobals::GetComlineFilename(nPlots);
           if(amrex::ParallelDescriptor::IOProcessor()) {
             cout << endl << "FileName = " << comlineFileName << endl;
           }
	   pdspArray[nPlots] = new amrex::DataServices(comlineFileName, fileType);

           if(amrex::ParallelDescriptor::IOProcessor()) {
             pdspArray[nPlots]->IncrementNumberOfUsers();
             ProfApp *temp = new ProfApp(app, wTopLevel, comlineFileName,
		                         pdspArray);
	     if(temp == nullptr) {
	       cerr << "Error:  could not make a new ProfApp." << endl;
	     } else {
               profAppList.push_back(temp);
	     }
           }
         }

       } else {
         amrex::Abort("**** Error:  only a single bl_prof directory is supported.");
       }
     } else
#endif
     {

      amrex::Vector<amrex::DataServices *> dspArray(AVGlobals::GetFileCount());
      for(int nPlots(0); nPlots < AVGlobals::GetFileCount(); ++nPlots) {
        comlineFileName = AVGlobals::GetComlineFilename(nPlots);
        if(amrex::ParallelDescriptor::IOProcessor()) {
          cout << endl << "FileName = " << comlineFileName << endl;
        }
	dspArray[nPlots] = new amrex::DataServices(comlineFileName, fileType);
      }  // end for(nPlots...)

      for(int nPlots(0); nPlots < AVGlobals::GetFileCount(); ++nPlots) {
        if(amrex::ParallelDescriptor::IOProcessor()) {
	  if(dspArray[nPlots]->AmrDataOk()) {
	    amrex::Vector<amrex::DataServices *> dspArrayOne(1);
	    dspArrayOne[0] = dspArray[nPlots];
            PltApp *temp = new PltApp(app, wTopLevel, dspArrayOne[0]->GetFileName(),
			              dspArrayOne, AVGlobals::IsAnimation());
	    if(temp == nullptr) {
	      cerr << "Error:  could not make a new PltApp." << endl;
	    } else {
              pltAppList.push_back(temp);
              dspArray[nPlots]->IncrementNumberOfUsers();
              if(AVGlobals::GivenBox()) {
		amrex::DataServices *dsp = temp->GetDataServicesPtr();
	        const amrex::AmrData &amrData = dsp->AmrDataRef();
		amrex::Box bPD(amrData.ProbDomain()[amrData.FinestLevel()]);
		amrex::Box itypComlineBox(bPD);  // for correct box type
		itypComlineBox.setSmall(comlineBox.smallEnd());
		itypComlineBox.setBig(comlineBox.bigEnd());
		amrex::Box comlineBoxErr(itypComlineBox);
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

     }

    }  // end if(AVGlobals::IsAnimation())


    if(amrex::ParallelDescriptor::IOProcessor()) {
      XtAppMainLoop(app);
    } else {
      // all other processors wait for a request
      amrex::DataServices::Dispatch(amrex::DataServices::InvalidRequestType, NULL, NULL);
    }

  }  // end if(bBatchMode)

  cout << amrex::ParallelDescriptor::MyProc() << "::]]]]:  exiting main." << endl;

  return 0;

}  // end main()

 
// ---------------------------------------------------------------
void CreateMainWindow(int argc, char *argv[]) {
  int i;
  string	comlineFileName;

  String fallbacks[] = {const_cast<char *>("*fontList:variable=charset"),
			NULL };


  wTopLevel = XtVaAppInitialize(&app, "AmrVisTool", NULL, 0, 
                                (int *) &argc, argv, fallbacks,
			XmNx,		350,
			XmNy,		10,
                        XmNwidth,	600,
			XmNheight,	200,
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

  XmString sFile    = XmStringCreateSimple(const_cast<char *>("File"));
  XmString sOpen    = XmStringCreateSimple(const_cast<char *>("Open..."));
  XmString sQuit    = XmStringCreateSimple(const_cast<char *>("Quit"));

  wMenuBar = XmVaCreateSimpleMenuBar(wMainWindow, const_cast<char *>("menuBar"),
		XmVaCASCADEBUTTON, sFile, 'F',
		XmNtopAttachment,	XmATTACH_FORM,
		XmNleftAttachment,	XmATTACH_FORM,
		XmNrightAttachment,	XmATTACH_FORM,
		XmNheight,		30,
		NULL);

  XmString sCtrlQ = XmStringCreateSimple(const_cast<char *>("Ctrl+Q"));
  XmString sCtrlO = XmStringCreateSimple(const_cast<char *>("Ctrl+O"));
  XmVaCreateSimplePulldownMenu(wMenuBar, const_cast<char *>("fileMenu"), 0,
    (XtCallbackProc) CBFileMenu,
    XmVaPUSHBUTTON, sOpen, 'O', "Ctrl<Key>O", sCtrlO,
    XmVaPUSHBUTTON, sQuit, 'Q', "Ctrl<Key>Q", sCtrlQ,
    NULL);


  XmStringFree(sFile);
  XmStringFree(sOpen);
  XmStringFree(sQuit);
  XmStringFree(sCtrlO);
  XmStringFree(sCtrlQ);

  XtManageChild(wMenuBar);

  i = 0;
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
  wTextOut = XmCreateScrolledText(wMainWindow, const_cast<char *>("textOut"), args, i);
  XtManageChild(wTextOut);

  messageText.Init(wTextOut);

  XtRealizeWidget(wTopLevel);
}  // end CreateMainWindow()

 
// ---------------------------------------------------------------
void BatchFunctions() {
  string comlineFileName;

  // loop through the command line list of plot files
  for(int nPlots = 0; nPlots < AVGlobals::GetFileCount(); ++nPlots) {
    comlineFileName = AVGlobals::GetComlineFilename(nPlots);
    if(amrex::ParallelDescriptor::IOProcessor()) {
      cout << "FileName = " << comlineFileName << endl;
    }
    amrex::Amrvis::FileType fileType = AVGlobals::GetDefaultFileType();
    BL_ASSERT(fileType != amrex::Amrvis::INVALIDTYPE);
    amrex::DataServices dataServices(comlineFileName, fileType);

    string derived(AVGlobals::GetInitialDerived());
    if( ! dataServices.CanDerive(derived)) {
      if(amrex::ParallelDescriptor::IOProcessor()) {
        cerr << "Unknown initial derived:  " << derived << endl;
      }
      derived = dataServices.PlotVarNames()[0];
      if(amrex::ParallelDescriptor::IOProcessor()) {
        cerr << "Defaulting to:  " << derived << endl;
      }
      AVGlobals::SetInitialDerived(derived);
    }


#if (BL_SPACEDIM == 3)
#ifdef BL_VOLUMERENDER
    if(AVGlobals::CreateSWFData()) {
      amrex::AmrData &amrData = dataServices.AmrDataRef();
      BL_ASSERT(dataServices.CanDerive(AVGlobals::GetInitialDerived()));
      int minDrawnLevel = 0;
      int maxDrawnLevel;
      if(AVGlobals::UseMaxLevel()) {
        maxDrawnLevel = max(0, min(AVGlobals::GetMaxLevel(), amrData.FinestLevel()));
      } else {
        maxDrawnLevel = amrData.FinestLevel();
      }
      if(amrex::ParallelDescriptor::IOProcessor()) {
        cout << "_in BatchFunctions:  using max level = " << maxDrawnLevel << endl;
      }
      amrex::Vector<amrex::Box> drawDomain = amrData.ProbDomain();

      if(AVGlobals::GivenBox()) {
        amrex::Box comlineBox = AVGlobals::GetBoxFromCommandLine();
        int finelev(amrData.FinestLevel());
	if(amrData.ProbDomain()[finelev].contains(comlineBox) == false) {
	  cerr << "Error:  bad comlineBox:  probDomain(finestLevel) = "
	       << amrData.ProbDomain()[finelev] << endl;
	  amrex::Abort("Exiting.");
	}
        drawDomain[finelev] = comlineBox;
        for(int ilev(amrData.FinestLevel() - 1); ilev >= 0; --ilev) {
	  int crr(amrex::CRRBetweenLevels(ilev, finelev, amrData.RefRatio()));
          drawDomain[ilev] = drawDomain[finelev];
          drawDomain[ilev].coarsen(crr);
        }
      }

      int iPaletteStart(3);
      int iPaletteEnd(AVGlobals::MaxPaletteIndex());
      int iBlackIndex(0);
      int iWhiteIndex(1);
      int iColorSlots(AVGlobals::MaxPaletteIndex() + 1 - iPaletteStart);
      Palette volPal(AVPalette::PALLISTLENGTH, AVPalette::PALWIDTH,
                     AVPalette::TOTALPALWIDTH, AVPalette::TOTALPALHEIGHT, 0);
      if(amrex::ParallelDescriptor::IOProcessor()) {
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
              amrex::DataServices::Dispatch(amrex::DataServices::DumpSlicePlaneAllVars,
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
                amrex::DataServices::Dispatch(amrex::DataServices::DumpSlicePlaneOneVar,
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
        amrex::Box comLineBox(AVGlobals::GetBoxFromCommandLine());
	BL_ASSERT(comLineBox.ok());
        if(AVGlobals::SliceAllVars()) {
          amrex::DataServices::Dispatch(amrex::DataServices::DumpSliceBoxAllVars,
				 &dataServices,
				 (void *) &comLineBox);
        } else {
            amrex::DataServices::Dispatch(amrex::DataServices::DumpSliceBoxOneVar,
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
    amrex::Vector<amrex::DataServices *> dataServicesPtr = obj->GetDataServicesPtrArray();
    for(int ids(0); ids < dataServicesPtr.size(); ++ids) {
      dataServicesPtr[ids]->DecrementNumberOfUsers();
    }
    delete obj;
  }
  pltAppList.clear();

  amrex::DataServices::Dispatch(amrex::DataServices::ExitRequest, NULL);
}

 
// ---------------------------------------------------------------
void CBFileMenu(Widget, XtPointer client_data, XtPointer) {
  int i(0);
  unsigned long item = (unsigned long) client_data;

  if(item == QUITITEM) {
    QuitAll();
  } else if(item == OPENITEM) {
    i = 0;
    amrex::Amrvis::FileType fileType(AVGlobals::GetDefaultFileType());
    XmString sMask;
    if(fileType == amrex::Amrvis::FAB) {
      sMask = XmStringCreateSimple(const_cast<char *>("*.fab"));
    } else if(fileType == amrex::Amrvis::MULTIFAB) {
      sMask = XmStringCreateSimple(const_cast<char *>("*_H"));
#ifdef BL_USE_PROFPARSER
    } else if(fileType == amrex::Amrvis::PROFDATA) {
      sMask = XmStringCreateSimple(const_cast<char *>("bl_prof*"));
#endif
    } else {
      sMask = XmStringCreateSimple(const_cast<char *>("plt*"));
    }

    XmString sNone = XmStringCreateSimple(const_cast<char *>("none"));
    if( ! XmStringCompare(sDirectory, sNone)) {
      XtSetArg (args[i], XmNdirectory, sDirectory); ++i;
    }
    XtSetArg (args[i], XmNpattern, sMask); ++i;
    XtSetArg (args[i], XmNfileTypeMask, XmFILE_ANY_TYPE); ++i;
    wDialog = XmCreateFileSelectionDialog(wTopLevel, const_cast<char *>("Open File"), args, i);
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
  amrex::Amrvis::FileType fileType(AVGlobals::GetDefaultFileType());
  if(fileType == amrex::Amrvis::MULTIFAB) {
    // delete the _H from the filename if it is there
    const char *uH = "_H";
    char *fm2 = filename + (strlen(filename) - 2);
    if(strcmp(uH, fm2) == 0) {
      filename[strlen(filename) - 2] = '\0';
    }
  }
  char path[amrex::Amrvis::BUFSIZE];
  strcpy(path, filename);
  int pathPos(strlen(path) - 1);
  while(pathPos > -1 && path[pathPos] != '/') {
    --pathPos;
  }
  path[pathPos + 1] = '\0';
  sDirectory = XmStringCreateSimple(path);

  sprintf(buffer, "Selected file = %s\n", filename);
  messageText.PrintText(buffer);

  amrex::DataServices *dataServicesPtr = new amrex::DataServices();
  dataServicesPtr->Init(filename, AVGlobals::GetDefaultFileType());

  amrex::DataServices::Dispatch(amrex::DataServices::NewRequest, dataServicesPtr, NULL);

  amrex::Vector<amrex::DataServices *> dspArray(1);
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
void SubregionPltApp(Widget swTopLevel, const amrex::Box &trueRegion,
		     const amrex::IntVect &offset,
		     PltApp *pltparent,
		     const string &palfile, int isAnim,
		     const string &currentderived, const string &file)
{
  PltApp *temp = new PltApp(app, swTopLevel, trueRegion, offset,
		    pltparent, palfile, isAnim, currentderived, file);
  if(temp == NULL) {
    cerr << "Error in SubregionPltApp:  could not make a new PltApp." << endl;
  } else {
    pltAppList.push_back(temp);
    amrex::Vector<amrex::DataServices *> dataServicesPtr = temp->GetDataServicesPtrArray();
    for(int ids(0); ids < dataServicesPtr.size(); ++ids) {
      dataServicesPtr[ids]->IncrementNumberOfUsers();
    }
  }
}


#ifdef BL_USE_PROFPARSER
// ---------------------------------------------------------------
void SubregionProfApp(Widget swTopLevel, const amrex::Box &trueRegion,
		      const amrex::IntVect &offset,
		      ProfApp *profparent, const string &palfile,
		      const string &file)
{
  ProfApp *temp = new ProfApp(app, swTopLevel, trueRegion, offset,
		              profparent, palfile, file);
  cout << "_in SubregionProfApp:  offset = " << offset << endl;
  if(temp == NULL) {
    cerr << "Error in SubregionProfApp:  could not make a new ProfApp." << endl;
  } else {
    profAppList.push_back(temp);
    //amrex::Vector<amrex::DataServices *> dataServicesPtr = temp->GetDataServicesPtrArray();
    //for(int ids(0); ids < dataServicesPtr.size(); ++ids) {
      //dataServicesPtr[ids]->IncrementNumberOfUsers();
    //}
  }
}
#endif


// ---------------------------------------------------------------
void CBQuitPltApp(Widget /*ofPltApp*/, XtPointer client_data, XtPointer) {
  PltApp *obj = (PltApp *) client_data;
  pltAppList.remove(obj);

  amrex::Vector<amrex::DataServices *> &dataServicesPtr = obj->GetDataServicesPtrArray();
  for(int ids(0); ids < dataServicesPtr.size(); ++ids) {
    dataServicesPtr[ids]->DecrementNumberOfUsers();
    amrex::DataServices::Dispatch(amrex::DataServices::DeleteRequest, dataServicesPtr[ids], NULL);
  }

  delete obj;
}


#ifdef BL_USE_PROFPARSER
// ---------------------------------------------------------------
void CBQuitProfApp(Widget /*ofProfApp*/, XtPointer client_data, XtPointer) {
  ProfApp *obj = (ProfApp *) client_data;
  profAppList.remove(obj);

  amrex::Vector<amrex::DataServices *> &dataServicesPtr = obj->GetDataServicesPtrArray();
  for(int ids(0); ids < dataServicesPtr.size(); ++ids) {
    dataServicesPtr[ids]->DecrementNumberOfUsers();
    amrex::DataServices::Dispatch(amrex::DataServices::DeleteRequest, dataServicesPtr[ids], NULL);
  }

  delete obj;
}
#endif


// ---------------------------------------------------------------
void CBQuitAll(Widget, XtPointer, XtPointer) {
  QuitAll();
}
// ---------------------------------------------------------------
// ---------------------------------------------------------------
