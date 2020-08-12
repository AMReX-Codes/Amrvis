// ---------------------------------------------------------------
// PltApp.cpp
// ---------------------------------------------------------------
#include <AMReX_ParallelDescriptor.H>
#include <AMReX_DataServices.H>

#include <Xm/Protocols.h>
#include <Xm/ToggleBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/FileSB.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/DrawingA.h>
#include <Xm/Text.h>
#include <Xm/ScrolledW.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <Xm/List.h>
#include <Xm/Scale.h>
#include <Xm/SeparatoG.h>
#include <Xm/Frame.h>
#include <Xm/ArrowB.h>
#include <Xm/CascadeB.h>

#include <X11/cursorfont.h>


#include <PltApp.H>
#include <PltAppState.H>
#include <AmrPicture.H>
#include <GraphicsAttributes.H>
#include <ProjectionPicture.H>
#include <XYPlotWin.H>
#include <MessageArea.H>

#if defined(BL_PARALLELVOLUMERENDER)
#include <PVolRender.H>
#endif

#include <cctype>
#include <sstream>
#include <cmath>
#include <cstdlib>
using std::cout;
using std::cerr;
using std::endl;
using std::flush;

using namespace amrex;

// Hack for window manager calls
#ifndef FALSE
#define FALSE false
#endif

const int MAXSCALE(32);

cMessageArea pltAppMessageText;
char cbuff[100];


#define MARK fprintf(stderr, "Mark at file %s, line %d.\n", __FILE__, __LINE__)

static bool UsingFileRange(const Amrvis::MinMaxRangeType rt) {
  return(rt == Amrvis::FILEGLOBALMINMAX    ||
         rt == Amrvis::FILESUBREGIONMINMAX ||
         rt == Amrvis::FILEUSERMINMAX);
}


// -------------------------------------------------------------------
PltApp::~PltApp() {
  int np;
#if (BL_SPACEDIM == 3)
  for(np = 0; np != Amrvis::NPLANES; ++np) {
    amrPicturePtrArray[np]->DoStop();
  }
#endif
  if(animating2d) {
    StopAnimation();
  }
  for(np = 0; np != Amrvis::NPLANES; ++np) {
    delete amrPicturePtrArray[np];
  }
#if (BL_SPACEDIM == 3)
  delete projPicturePtr;
#endif
  delete XYplotparameters;
  delete pltPaletteptr;
  delete gaPtr;
  delete pltAppState;
  if(datasetShowing) {
    delete datasetPtr;
  }
  XtDestroyWidget(wAmrVisTopLevel);

  // delete all the call back parameter structs
  for(int nSize(0); nSize < cbdPtrs.size(); ++nSize) {
    delete cbdPtrs[nSize];
  }
}


// -------------------------------------------------------------------
PltApp::PltApp(XtAppContext app, Widget w, const string &filename,
	       const Vector<DataServices *> &dataservicesptr, bool isAnim)
  : wTopLevel(w),
    appContext(app),
    currentRangeType(Amrvis::GLOBALMINMAX),
    animating2d(isAnim),
    paletteDrawn(false)
{
  dataServicesPtr = dataservicesptr;
  currentFrame = 0;
  fileName = filename;

#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  lightingWindowExists = false;
#endif

  if( ! dataservicesptr[0]->AmrDataOk()) {
    return;
  }
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  bFileRangeButtonSet = false;

  if(animating2d) {
    animFrames = AVGlobals::GetFileCount(); 
    BL_ASSERT(dataServicesPtr.size() == animFrames);
    fileNames.resize(animFrames);
    for(int i(0); i < animFrames; ++i) {
      fileNames[i] = AVGlobals::GetComlineFilename(i); 
    }
  } else {
    animFrames = 1;
    fileNames.resize(animFrames);
    fileNames[currentFrame] = fileName;
  }

  std::ostringstream headerout;
  headerout << AVGlobals::StripSlashes(fileNames[currentFrame])
            << "   T = " << amrData.Time() << "  " << headerSuffix;

  pltAppState = new PltAppState(animFrames, amrData.NumDeriveFunc());
  pltAppState->SetContourType(Amrvis::RASTERONLY);
  pltAppState->SetCurrentFrame(currentFrame);

  // Set the delete response to DO_NOTHING so that it can be app defined.
  wAmrVisTopLevel = XtVaCreatePopupShell(headerout.str().c_str(), 
			 topLevelShellWidgetClass, wTopLevel,
			 XmNwidth,	    initialWindowWidth,
			 XmNheight,	    initialWindowHeight,
			 XmNx,		    100+placementOffsetX,
			 XmNy,		    200+placementOffsetY,
			 XmNdeleteResponse, XmDO_NOTHING,
			 NULL);

  gaPtr = new GraphicsAttributes(wAmrVisTopLevel);
  display = gaPtr->PDisplay();
  xgc = gaPtr->PGC();
  if(gaPtr->PVisual() != XDefaultVisual(display, gaPtr->PScreenNumber())) {
    XtVaSetValues(wAmrVisTopLevel, XmNvisual, gaPtr->PVisual(),
		  XmNdepth, gaPtr->PDepth(), NULL);
  }

  if( ! dataServicesPtr[currentFrame]->CanDerive(PltApp::initialDerived)) {
    cerr << "Unknown initial derived:  "
	 << PltApp::initialDerived << endl;
    cerr << "Defaulting to:  "
	 << dataServicesPtr[currentFrame]->PlotVarNames()[0] << endl;
    SetInitialDerived(dataServicesPtr[currentFrame]->PlotVarNames()[0]);
  }

  pltAppState->SetCurrentDerived(PltApp::initialDerived,
				 amrData.StateNumber(initialDerived));
  pltAppState->SetShowingBoxes(GetDefaultShowBoxes());
  int finestLevel(amrData.FinestLevel());
  pltAppState->SetFinestLevel(finestLevel);
  int maxlev =
        AVGlobals::DetermineMaxAllowableLevel(amrData.ProbDomain()[finestLevel],
			       finestLevel, AVGlobals::MaxPictureSize(),
			       amrData.RefRatio());
  int minAllowableLevel(0);
  pltAppState->SetMinAllowableLevel(minAllowableLevel);
  pltAppState->SetMaxAllowableLevel(maxlev);
  pltAppState->SetMinDrawnLevel(minAllowableLevel);
  pltAppState->SetMaxDrawnLevel(maxlev);
  Box maxDomain(amrData.ProbDomain()[maxlev]);
#if (BL_SPACEDIM == 1)
  unsigned long dataSize(static_cast<unsigned long>(maxDomain.length(Amrvis::XDIR)));
#else
  unsigned long dataSize(static_cast<unsigned long>(maxDomain.length(Amrvis::XDIR)) *
                         static_cast<unsigned long>(maxDomain.length(Amrvis::YDIR)));
#endif
  if(AVGlobals::MaxPictureSize() == 0) {
    maxAllowableScale = 1;
  } else  {
#if (BL_SPACEDIM == 1)
    maxAllowableScale = (int) ((exp2(15) - 1) / dataSize);
#else
    maxAllowableScale = (int) sqrt((Real) (AVGlobals::MaxPictureSize() / dataSize));
#endif
    maxAllowableScale = max(1, maxAllowableScale);
  }

  int currentScale(max(1, min(GetInitialScale(), maxAllowableScale)));
  pltAppState->SetCurrentScale(currentScale);
  pltAppState->SetMaxScale(maxAllowableScale);

  // ------------------------------- handle commprof timeline format
  bTimeline       = false;
  initTimeline    = false;
  timelineMin     = -1;
  timelineMax     = -1;
  const string pfVersion(amrData.PlotFileVersion());
  if(pfVersion.compare(0, 16, "CommProfTimeline") == 0) {
    cout << ">>>> CommProfTimeline file." << endl;
    //pltPaletteptr->SetTimeline(true);
    pltAppState->SetFormatString("%8.0f");
    pltAppState->SetShowingBoxes(false);

    {
      string mfnFileName(amrData.GetFileName() + "/MPIFuncNames.txt");
      std::ifstream mfnNames(mfnFileName.c_str());
      mpiFNames.insert(std::make_pair(-1, "non-mpi"));

      // The file has a timeline. Perform Timeline Init.
      initTimeline = true;
      // Is the initial derived object the timeline?
      string derivedName = dataServicesPtr[currentFrame]->
                   PlotVarNames()[amrData.StateNumber(initialDerived)];
      if (derivedName == "timeline"){
        bTimeline = true;
      }
      if(mfnNames.fail()) {
        cout << "**** Error:  could not open:  " << mfnFileName << endl;
      } else {
	int i;
        string fName;
        while( ! mfnNames.eof()) {
          mfnNames >> i >> fName;
	  if( ! mfnNames.eof()) {
	    mpiFNames.insert(std::make_pair(i, fName));
	    timelineMax = std::max(i, timelineMax);
	  }
        }
        mfnNames.close();
      }
    }

    {
      string ntnFileName(amrData.GetFileName() + "/NameTagNames.txt");
      std::ifstream ntnNames(ntnFileName.c_str());
      if(ntnNames.fail()) {
        cout << "**** Error:  could not open:  " << ntnFileName << endl;
      } else {
        int ntnSize(0);
        ntnNames >> ntnSize >> nameTagMultiplier;
        ntnNames.ignore(1, '\n');
        nameTagNames.resize(ntnSize);
        for(int i(0); i < nameTagNames.size(); ++i) {
          std::getline(ntnNames, nameTagNames[i]);
        }
        ntnNames.close();
      }
    }

    {
      string bnFileName(amrData.GetFileName() + "/BarrierNames.txt");
      std::ifstream bnNames(bnFileName.c_str());
      if(bnNames.fail()) {
        cout << "**** Error:  could not open:  " << bnFileName << endl;
      } else {
        int bnSize(0);
        bnNames >> bnSize >> barrierMultiplier;
        bnNames.ignore(1, '\n');
        barrierNames.resize(bnSize);
        for(int i(0); i < barrierNames.size(); ++i) {
          std::getline(bnNames, barrierNames[i]);
        }
        bnNames.close();
      }
    }

    {
      string ctFileName(amrData.GetFileName() + "/CallTrace.txt");
      std::ifstream cTrace(ctFileName.c_str());
      if(cTrace.fail()) {
        cout << "**** Error:  could not open:  " << ctFileName << endl;
        callTraceExists = false;
      } else {
	ParseCallTraceFile(cTrace);
        cTrace.close();
        callTraceExists = true;
      }
    }

    {
      string trFileName(amrData.GetFileName() + "/TimeRange.txt");
      std::ifstream trRange(trFileName.c_str());
      if(trRange.fail()) {
        cout << "**** Error:  could not open:  " << trFileName << endl;
	timelineRangeStart = 0.0;
	timelineRangeStop  = amrData.Time();
      } else {
        trRange >> timelineRangeStart >> timelineRangeStop;
        trRange.close();
      }
    }
  }

  bRegions = false;
  if(pfVersion.compare(0, 11, "RegionsProf") == 0) {
    cout << ">>>> RegionsProf file." << endl;
    pltAppState->SetFormatString("%12.0f");
    pltAppState->SetShowingBoxes(false);

    {
      string regFileName(amrData.GetFileName() + "/RegionNames.txt");
      std::ifstream rNames(regFileName.c_str());
      regNames.insert(std::make_pair(-1, "not in region"));
      bRegions = true;
      if(rNames.fail()) {
        cout << "**** Error:  could not open:  " << regFileName << endl;
      } else {
	int i;
        string fName;
	char s[Amrvis::BUFSIZE];
        while( ! rNames.getline(s, Amrvis::BUFSIZE).eof()) {  // deal with quoted names
	  string ss(s);
	  const string quote("\"");
	  unsigned int qpos(ss.rfind(quote));
	  fName = ss.substr(1, qpos - 1);
	  string si(ss.substr(qpos + 2, ss.length() - qpos + 2));
	  std::stringstream iString(si);
	  iString >> i;
	  regNames.insert(std::make_pair(i, fName));
	  regionsMax = std::max(i, regionsMax);
        }
        rNames.close();
      }
    }
  }


  pltAppState->SetMinMaxRangeType(Amrvis::GLOBALMINMAX);
  Real rGlobalMin, rGlobalMax;
  rGlobalMin =  std::numeric_limits<Real>::max();
  rGlobalMax = -std::numeric_limits<Real>::max();
  int coarseLevel(0);
  int iCDerNum(pltAppState->CurrentDerivedNumber());
  string asCDer(pltAppState->CurrentDerived());
  int fineLevel(amrData.FinestLevel());
  const Vector<Box> &onBox(amrData.ProbDomain());
  for(int iFrame(0); iFrame < animFrames; ++iFrame) {
    Real rFileMin, rFileMax;

    FindAndSetMinMax(Amrvis::FILEGLOBALMINMAX, iFrame, asCDer, iCDerNum,
		     onBox, coarseLevel, fineLevel, false);  // dont reset if set
    pltAppState->GetMinMax(Amrvis::FILEGLOBALMINMAX, iFrame, iCDerNum,
			   rFileMin, rFileMax);
    rGlobalMin = min(rFileMin, rGlobalMin);
    rGlobalMax = max(rFileMax, rGlobalMax);

    // also set FILESUBREGIONMINMAXs and FILEUSERMINMAXs to the global values
    pltAppState->SetMinMax(Amrvis::FILESUBREGIONMINMAX, iFrame,
			   pltAppState->CurrentDerivedNumber(),
			   rFileMin, rFileMax);
    pltAppState->SetMinMax(Amrvis::FILEUSERMINMAX, iFrame,
			   pltAppState->CurrentDerivedNumber(),
			   rFileMin, rFileMax);
  }  // end for(iFrame...)

  // now set global values for each file in the animation
  for(int iFrame(0); iFrame < animFrames; ++iFrame) {
    // set GLOBALMINMAXs
    pltAppState->SetMinMax(Amrvis::GLOBALMINMAX, iFrame,
			   pltAppState->CurrentDerivedNumber(),
			   rGlobalMin, rGlobalMax);
    // also set SUBREGIONMINMAXs and USERMINMAXs to the global values
    pltAppState->SetMinMax(Amrvis::SUBREGIONMINMAX, iFrame,
			   pltAppState->CurrentDerivedNumber(),
			   rGlobalMin, rGlobalMax);
    pltAppState->SetMinMax(Amrvis::USERMINMAX, iFrame,
			   pltAppState->CurrentDerivedNumber(),
			   rGlobalMin, rGlobalMax);
  }  // end for(iFrame...)

  if(AVGlobals::UseSpecifiedMinMax()) {
    pltAppState->SetMinMaxRangeType(Amrvis::USERMINMAX);
    Real specifiedMin, specifiedMax;
    AVGlobals::GetSpecifiedMinMax(specifiedMin, specifiedMax);
    for(int iFrame(0); iFrame < animFrames; ++iFrame) {
      pltAppState->SetMinMax(Amrvis::USERMINMAX, iFrame,
			     pltAppState->CurrentDerivedNumber(),
			     specifiedMin, specifiedMax);
      pltAppState->SetMinMax(Amrvis::FILEUSERMINMAX, iFrame,
			     pltAppState->CurrentDerivedNumber(),
			     specifiedMin, specifiedMax);
    }
  }


  amrPicturePtrArray[Amrvis::ZPLANE] = new AmrPicture(gaPtr, this, pltAppState,
					dataServicesPtr[currentFrame],
					pltAppState->GetCGSmoothing());
#if (BL_SPACEDIM == 3)
  amrPicturePtrArray[Amrvis::YPLANE] = new AmrPicture(Amrvis::YPLANE, gaPtr,
					amrData.ProbDomain()[finestLevel],
					NULL, this,
					pltAppState,
					pltAppState->GetCGSmoothing());
  amrPicturePtrArray[Amrvis::XPLANE] = new AmrPicture(Amrvis::XPLANE, gaPtr,
					amrData.ProbDomain()[finestLevel],
					NULL, this,
					pltAppState,
					pltAppState->GetCGSmoothing());
#endif

  for(int i(0); i != BL_SPACEDIM; ++i) {
    ivLowOffsetMAL.setVal(i, amrData.ProbDomain()[maxlev].smallEnd(i));
  }

  contourNumString = string("10");
  pltAppState->SetNumContours(10);

  palFilename = PltApp::defaultPaletteString;
  lightingFilename = PltApp::defaultLightingFilename;

  std::ostringstream suffixout;
  suffixout << "Region:  " << maxDomain << "  Levels:  "
            << pltAppState->MinAllowableLevel() << ":"
	    << pltAppState->MaxAllowableLevel()
	    << "  Finest Level:  " << amrData.FinestLevel() << "  ";
  headerSuffix = suffixout.str();
  headerout.str(std::string());
  headerout << AVGlobals::StripSlashes(fileNames[currentFrame])
            << "   T = " << amrData.Time() << "   " << headerSuffix;
  XtVaSetValues(wAmrVisTopLevel,
                XmNtitle, const_cast<char *>(headerout.str().c_str()),
		NULL);

  PltAppInit();
}


// -------------------------------------------------------------------
PltApp::PltApp(XtAppContext app, Widget w, const Box &region,
	       const IntVect &offset,
	       PltApp *sPltParent, const string &palfile,
	       bool isAnim, const string &newderived, const string &filename)
  : wTopLevel(w),
    appContext(app),
    animating2d(isAnim),
    paletteDrawn(false),
    animFrames(sPltParent->animFrames),
    lightingFilename(sPltParent->lightingFilename)
{
  dataServicesPtr = sPltParent->dataServicesPtr;
  currentFrame = sPltParent->currentFrame;
  fileName = filename;
  fileNames = sPltParent->fileNames;
  palFilename = palfile;
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  bFileRangeButtonSet = sPltParent->bFileRangeButtonSet;

  pltAppState = new PltAppState(animFrames, amrData.NumDeriveFunc());
  *pltAppState = *sPltParent->GetPltAppState();

#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  lightingWindowExists = false;
#endif
  contourNumString = sPltParent->contourNumString.c_str();

  int finestLevel(amrData.FinestLevel());
  pltAppState->SetFinestLevel(finestLevel);
  int maxlev = AVGlobals::DetermineMaxAllowableLevel(region, finestLevel,
					             AVGlobals::MaxPictureSize(),
					             amrData.RefRatio());
  int minAllowableLevel = amrData.FinestContainingLevel(region, finestLevel);

  if(minAllowableLevel > maxlev) {
    //maxlev = minAllowableLevel;
    minAllowableLevel = maxlev;
    cout << "**** Conflict with allowable levels and max pixmap size:  using maxlev = "
         << maxlev << endl;
  }
  pltAppState->SetMinAllowableLevel(minAllowableLevel);
  pltAppState->SetMaxAllowableLevel(maxlev);
  pltAppState->SetMinDrawnLevel(minAllowableLevel);
  pltAppState->SetMaxDrawnLevel(maxlev);

  Box maxDomain(region);
  if(maxlev < finestLevel) {
    maxDomain.coarsen(amrex::CRRBetweenLevels(maxlev, finestLevel,
                      amrData.RefRatio()));
  }

#if (BL_SPACEDIM == 1)
  unsigned long dataSize(static_cast<unsigned long>(maxDomain.length(Amrvis::XDIR)));
#else
  unsigned long dataSize(static_cast<unsigned long>(maxDomain.length(Amrvis::XDIR)) *
                         static_cast<unsigned long>(maxDomain.length(Amrvis::YDIR)));
#endif
  if(AVGlobals::MaxPictureSize() == 0) {
    maxAllowableScale = 1;
  } else  {
#if (BL_SPACEDIM == 1)
    maxAllowableScale = (int) ((exp2(15) - 1) / dataSize);
#else
    maxAllowableScale = (int) sqrt((Real) (AVGlobals::MaxPictureSize() / dataSize));
#endif
    maxAllowableScale = max(1, maxAllowableScale);
  }

  int currentScale(max(1, min(maxAllowableScale,
			      sPltParent->GetPltAppState()->CurrentScale())));
  pltAppState->SetCurrentScale(currentScale);
  
 // ------------------------------- handle commprof timeline format
  bTimeline       = false; 
  initTimeline    = false;
  timelineMin     = -1;
  timelineMax     = -1;
  const string pfVersion(amrData.PlotFileVersion());
  if(pfVersion.compare(0, 16, "CommProfTimeline") == 0) {
    cout << ">>>> CommProfTimeline file." << endl;
    //pltPaletteptr->SetTimeline(true);
    pltAppState->SetFormatString("%8.0f");
    pltAppState->SetShowingBoxes(false);

    {
      string mfnFileName(amrData.GetFileName() + "/MPIFuncNames.txt");
      std::ifstream mfnNames(mfnFileName.c_str());
      mpiFNames.insert(std::make_pair(-1, "non-mpi"));

      // The file has a timeline. Perform Timeline Init.
      initTimeline = true;
      // Is the derived subregion object a timeline?
      if (newderived == "timeline"){ 
        bTimeline = true;
      }

      if(mfnNames.fail()) {
        cout << "**** Error:  could not open:  " << mfnFileName << endl;
      } else {
        int i;
        string fName;
        while( ! mfnNames.eof()) {
          mfnNames >> i >> fName;
          if( ! mfnNames.eof()) {
            mpiFNames.insert(std::make_pair(i, fName));
            timelineMax = std::max(i, timelineMax);
	  }
        }
        mfnNames.close();
        //pltPaletteptr->SetMPIFuncNames(mpiFNames);
      }
    }

    {
      string ntnFileName(amrData.GetFileName() + "/NameTagNames.txt");
      std::ifstream ntnNames(ntnFileName.c_str());
      if(ntnNames.fail()) {
        cout << "**** Error:  could not open:  " << ntnFileName << endl;
      } else {
        int ntnSize(0);
        ntnNames >> ntnSize >> nameTagMultiplier;
        ntnNames.ignore(1, '\n');
        nameTagNames.resize(ntnSize);
        for(int i(0); i < nameTagNames.size(); ++i) {
          std::getline(ntnNames, nameTagNames[i]);
        }
        ntnNames.close();
      }
    }

    {
      string bnFileName(amrData.GetFileName() + "/BarrierNames.txt");
      std::ifstream bnNames(bnFileName.c_str());
      if(bnNames.fail()) {
        cout << "**** Error:  could not open:  " << bnFileName << endl;
      } else {
        int bnSize(0);
        bnNames >> bnSize >> barrierMultiplier;
        bnNames.ignore(1, '\n');
        barrierNames.resize(bnSize);
        for(int i(0); i < barrierNames.size(); ++i) {
          std::getline(bnNames, barrierNames[i]);
        }
        bnNames.close();
      }
    }

    {
      string ctFileName(amrData.GetFileName() + "/CallTrace.txt");
      std::ifstream cTrace(ctFileName.c_str());
      if(cTrace.fail()) {
        cout << "**** Error:  could not open:  " << ctFileName << endl;
      } else {
	ParseCallTraceFile(cTrace);
        cTrace.close();
      }
    }

    {
      string trFileName(amrData.GetFileName() + "/TimeRange.txt");
      std::ifstream trRange(trFileName.c_str());
      if(trRange.fail()) {
        cout << "**** Error:  could not open:  " << trFileName << endl;
	timelineRangeStart = 0.0;
	timelineRangeStop  = amrData.Time();
      } else {
        trRange >> timelineRangeStart >> timelineRangeStop;
        trRange.close();
      }
    }
  }

  bRegions = false;
  if(pfVersion.compare(0, 11, "RegionsProf") == 0) {
    cout << ">>>> RegionsProf file." << endl;
    pltAppState->SetFormatString("%12.0f");
    pltAppState->SetShowingBoxes(false);

    {
      string regFileName(amrData.GetFileName() + "/RegionNames.txt");
      std::ifstream rNames(regFileName.c_str());
      regNames.insert(std::make_pair(-1, "not in region"));
      bRegions = true;
      if(rNames.fail()) {
        cout << "**** Error:  could not open:  " << regFileName << endl;
      } else {
	int i;
        string fName;
	char s[Amrvis::BUFSIZE];
        while( ! rNames.getline(s, Amrvis::BUFSIZE).eof()) {  // deal with quoted names
	  string ss(s);
	  const string quote("\"");
	  unsigned int qpos(ss.rfind(quote));
	  fName = ss.substr(1, qpos - 1);
	  string si(ss.substr(qpos + 2, ss.length() - qpos + 2));
	  std::stringstream iString(si);
	  iString >> i;
	  regNames.insert(std::make_pair(i, fName));
	  regionsMax = std::max(i, regionsMax);
        }
        rNames.close();
      }
    }
  }

// ---------------
  Vector<Box> onBox(pltAppState->MaxAllowableLevel() + 1);
  onBox[pltAppState->MaxAllowableLevel()] = maxDomain;
  for(int ilev(pltAppState->MaxAllowableLevel() - 1); ilev >= 0; --ilev) {
    Box tempbox(maxDomain);
    tempbox.coarsen(amrex::CRRBetweenLevels(ilev, finestLevel,
                    amrData.RefRatio()));
    onBox[ilev] = tempbox;
  }
  int iCDerNum(pltAppState->CurrentDerivedNumber());
  const string asCDer(pltAppState->CurrentDerived());
  int coarseLevel(0);
  int fineLevel(pltAppState->MaxAllowableLevel());
  Real rSMin, rSMax;
  rSMin =  std::numeric_limits<Real>::max();
  rSMax = -std::numeric_limits<Real>::max();
  for(int iFrame(0); iFrame < animFrames; ++iFrame) {
    // GLOBALMINMAX is already set to parent's values
    // USERMINMAX is already set to parent's values
    // FILEGLOBALMINMAX is already set to parent's values
    // FILEUSERMINMAX is already set to parent's values
    // these do not change for a subregion

    // set FILESUBREGIONMINMAX
    FindAndSetMinMax(Amrvis::FILESUBREGIONMINMAX, iFrame, asCDer, iCDerNum, onBox,
	             coarseLevel, fineLevel, true);  // reset if already set

    Real rTempMin, rTempMax;
    pltAppState->GetMinMax(Amrvis::FILESUBREGIONMINMAX, iFrame, iCDerNum,
			   rTempMin, rTempMax);
    // collect file values
    rSMin = min(rSMin, rTempMin);
    rSMax = max(rSMax, rTempMax);
  }
  // now set each frame's SUBREGIONMINMAX to the overall subregion minmax
  for(int iFrame(0); iFrame < animFrames; ++iFrame) {
    pltAppState->SetMinMax(Amrvis::SUBREGIONMINMAX, iFrame, iCDerNum, rSMin, rSMax);
  }
// ---------------

  std::ostringstream suffixout;
  suffixout << "Subregion:  " << maxDomain << "  Levels:  "
            << pltAppState->MinAllowableLevel() << ":"
	    << pltAppState->MaxAllowableLevel()
	    << "  Finest Level:  " << amrData.FinestLevel() << "  ";
  headerSuffix = suffixout.str();
  std::ostringstream headerout;
  headerout << AVGlobals::StripSlashes(fileNames[currentFrame])
            << "   T = " << amrData.Time() << "  " << headerSuffix;
				
  wAmrVisTopLevel = XtVaCreatePopupShell(headerout.str().c_str(), 
					 topLevelShellWidgetClass, wTopLevel,
					 XmNwidth,	    initialWindowWidth,
					 XmNheight,	    initialWindowHeight,
					 XmNx,		    120+placementOffsetX,
					 XmNy,		    220+placementOffsetY,
					 XmNdeleteResponse, XmDO_NOTHING,
					 NULL);

  if(AVGlobals::Verbose()) {
    cout << "_in PltApp::PltApp:  subregion" << endl;
    pltAppState->PrintSetMap();  cout << endl;
  }

  gaPtr = new GraphicsAttributes(wAmrVisTopLevel);
  display = gaPtr->PDisplay();
  xgc = gaPtr->PGC();

  if(gaPtr->PVisual() != XDefaultVisual(display, gaPtr->PScreenNumber())) {
    XtVaSetValues(wAmrVisTopLevel, XmNvisual, gaPtr->PVisual(), XmNdepth, 8, NULL);
  }
  for(int np(0); np < Amrvis::NPLANES; ++np) {
    amrPicturePtrArray[np] = new AmrPicture(np, gaPtr, region,
					    sPltParent, this,
					    pltAppState,
					    pltAppState->GetCGSmoothing());
  }
  
#if (BL_SPACEDIM == 3)
  for(int i(0); i < BL_SPACEDIM; ++i) {
   amrPicturePtrArray[i]->SetHVLine(pltAppState->CurrentScale());
  }
#endif
  ivLowOffsetMAL = offset;
  PltAppInit(true);
}


// -------------------------------------------------------------------
void PltApp::PltAppInit(bool bSubVolume) {
  int np;
  AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  int minAllowableLevel(pltAppState->MinAllowableLevel());
  int maxAllowableLevel(pltAppState->MaxAllowableLevel());
  wRangeRadioButton.resize(Amrvis::BNUMBEROFMINMAX);

  // User defined widget destroy callback -- will free all memory used to create
  // window.
  XmAddWMProtocolCallback(wAmrVisTopLevel,
			  XmInternAtom(display, const_cast<char *>("WM_DELETE_WINDOW"), false),
			  (XtCallbackProc) CBQuitPltApp, (XtPointer) this);

  for(np = 0; np != BL_SPACEDIM; ++np) {
    XYplotwin[np] = NULL; // No 1D plot windows initially.

    // For speed (and clarity) we store the values of the finest value of h of
    // each dimension, as well as the low value of the problem domain in simple
    // arrays.  These are both in problem space.
    finestDx[np] = amrData.DxLevel()[maxAllowableLevel][np];
    gridOffset[np] = amrData.ProbLo()[np];
  }
  bSyncFrame = false;

  placementOffsetX += 20;
  placementOffsetY += 20;
  
  servingButton = 0;
  activeView = Amrvis::ZPLANE;
  int maxDrawnLevel(maxAllowableLevel);
  startX = 0;
  startY = 0;
  endX = 0;
  endY = 0;

  animationIId = 0;
  frameSpeed = 300;

  readyFrames.resize(animFrames, false);
  frameBuffer.resize(animFrames);

  selectionBox.convert(amrData.ProbDomain()[0].type());
  selectionBox.setSmall(IntVect::TheZeroVector());
  selectionBox.setBig(IntVect::TheZeroVector());

  for(np = 0; np != Amrvis::NPLANES; ++np) {
    amrPicturePtrArray[np]->SetFrameSpeed(frameSpeed);
    amrPicturePtrArray[np]->SetRegion(startX, startY, endX, endY);
  }

  pltAppState->SetFormatString(PltApp::initialFormatString);

  infoShowing      = false;
  callTraceShowing = false;
  contoursShowing  = false;
  setRangeShowing  = false;
  bSetRangeRedraw  = false;
  datasetShowing   = false;
  bFormatShowing   = false;
  writingRGB       = false;
			  
  int palListLength(AVPalette::PALLISTLENGTH);
  int palWidth(AVPalette::PALWIDTH);
  int totalPalWidth(AVPalette::TOTALPALWIDTH);
  int totalPalHeight(AVPalette::TOTALPALHEIGHT);
  if(bRegions || bTimeline) {
    totalPalWidth += 100;
  }
  totalPalWidth += extraPaletteWidth;

  pltPaletteptr = new Palette(wTopLevel, palListLength, palWidth,
			      totalPalWidth, totalPalHeight,
			      reserveSystemColors);
  
  // ------------------------------- handle commprof timeline format
  if(initTimeline) {
    pltPaletteptr->SetTimeline(bTimeline);
    pltPaletteptr->SetMPIFuncNames(mpiFNames);
  }

  // ------------------------------- handle commprof regions format
  if(bRegions) {
    pltPaletteptr->SetRegions(true);
    pltPaletteptr->SetRegionNames(regNames);
  }

  // gc for gxxor rubber band line drawing
  rbgc = XCreateGC(display, gaPtr->PRoot(), 0, NULL);
  XSetFunction(display, rbgc, GXxor);
  cursor = XCreateFontCursor(display, XC_left_ptr);

  // No need to store these widgets in the class after this function is called.
  Widget wMainArea, wPalFrame, wPlotFrame, wPalForm;

  wMainArea = XtVaCreateManagedWidget("MainArea", xmFormWidgetClass,
				      wAmrVisTopLevel,
				      NULL);

  // ------------------------------- menu bar
  Widget wMenuBar, wMenuPulldown, wid, wCascade;
  char selectText[20], accelText[20], accel[20];
  XmString label_str;
  XtSetArg(args[0], XmNtopAttachment, XmATTACH_FORM);
  XtSetArg(args[1], XmNleftAttachment, XmATTACH_FORM);
  XtSetArg(args[2], XmNrightAttachment, XmATTACH_FORM);
  wMenuBar = XmCreateMenuBar(wMainArea, const_cast<char *>("menubar"), args, 3);

  // ------------------------------- file menu
  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, const_cast<char *>("Filepulldown"), NULL, 0);
  XtVaCreateManagedWidget("File", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'F', XmNsubMenuId, wMenuPulldown, NULL);

  // To look at a subregion of the plot
  label_str = XmStringCreateSimple(const_cast<char *>("Ctrl+S"));
  wid =
    XtVaCreateManagedWidget((BL_SPACEDIM != 3) ? "Subregion..." : "Subvolume...",
			    xmPushButtonGadgetClass, wMenuPulldown,
			    XmNmnemonic, 'S',
			    XmNaccelerator, "Ctrl<Key>S",
			    XmNacceleratorText, label_str,
			    NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoSubregion);

  // To change the palette
  wid = XtVaCreateManagedWidget("Palette...", xmPushButtonGadgetClass,
				wMenuPulldown, XmNmnemonic,  'P', NULL);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoPaletteButton);

  // To output to various formats (.ps, .rgb, .fab)
  wCascade = XmCreatePulldownMenu(wMenuPulldown, const_cast<char *>("outputmenu"), NULL, 0);
  XtVaCreateManagedWidget("Export", xmCascadeButtonWidgetClass, wMenuPulldown,
			  XmNmnemonic, 'E', XmNsubMenuId, wCascade, NULL);
  wid = XtVaCreateManagedWidget("PS File...", xmPushButtonGadgetClass,
				wCascade, XmNmnemonic, 'P', NULL);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoOutput, (XtPointer) 0);
  if(AVGlobals::IsSGIrgbFile()) {
    wid = XtVaCreateManagedWidget("RGB File...", xmPushButtonGadgetClass,
				  wCascade, XmNmnemonic, 'R', NULL);
  } else {
    wid = XtVaCreateManagedWidget("PPM File...", xmPushButtonGadgetClass,
				  wCascade, XmNmnemonic, 'M', NULL);
  }
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoOutput, (XtPointer) 1);
  wid = XtVaCreateManagedWidget("FAB File...", xmPushButtonGadgetClass,
				wCascade, XmNmnemonic, 'F', NULL);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoOutput, (XtPointer) 2);

  // Quit
  XtVaCreateManagedWidget(NULL, xmSeparatorGadgetClass, wMenuPulldown, NULL);
  label_str = XmStringCreateSimple(const_cast<char *>("Ctrl+Q"));
  wid = XtVaCreateManagedWidget("Quit", xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'Q',
				XmNaccelerator, "Ctrl<Key>Q",
				XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  XtAddCallback(wid, XmNactivateCallback, (XtCallbackProc) CBQuitAll,
		(XtPointer) this);
  
  // Close
  XtVaCreateManagedWidget(NULL, xmSeparatorGadgetClass, wMenuPulldown, NULL);
  label_str = XmStringCreateSimple(const_cast<char *>("Ctrl+C"));
  wid = XtVaCreateManagedWidget("Close", xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'C',
				XmNaccelerator, "Ctrl<Key>C",
				XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  XtAddCallback(wid, XmNactivateCallback, (XtCallbackProc) CBQuitPltApp,
		(XtPointer) this);
  
  // ------------------------------- view menu
  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, const_cast<char *>("MenuPulldown"), NULL, 0);
  XtVaCreateManagedWidget("View", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'V', XmNsubMenuId, wMenuPulldown, NULL);

  // To scale the raster / contour windows
  int maxallow(min(MAXSCALE, maxAllowableScale));
  wCascade = XmCreatePulldownMenu(wMenuPulldown, const_cast<char *>("scalemenu"), NULL, 0);
  XtVaCreateManagedWidget("Scale", xmCascadeButtonWidgetClass, wMenuPulldown,
			  XmNmnemonic, 'S', XmNsubMenuId, wCascade, NULL);
  for(int scale(1); scale <= maxallow; ++scale) {
    sprintf(selectText, "%ix", scale);
    wid = XtVaCreateManagedWidget(selectText, xmToggleButtonGadgetClass, wCascade,
				  XmNset, false, NULL);
    if(scale <= 10) {
      // scale values <= 10 are shortcutted by pressing the number 1-0
      sprintf(accel, "<Key>%i", scale % 10);
      sprintf(accelText, "%i", scale % 10);
      label_str = XmStringCreateSimple(accelText);
      XtVaSetValues(wid, XmNmnemonic, scale + 'O',
		    XmNaccelerator, accel,
		    XmNacceleratorText, label_str,
		    NULL);
      XmStringFree(label_str);
    } else if(scale <= 20) {
      // scale values <= 20 can be obtained by holding down ALT and pressing 1-0
      sprintf(accel, "Alt<Key>%i", scale % 10);
      sprintf(accelText, "Alt+%i", scale % 10);
      label_str = XmStringCreateSimple(accelText);
      XtVaSetValues(wid,
		    XmNaccelerator, accel,
		    XmNacceleratorText, label_str,
		    NULL);
      XmStringFree(label_str);
    }      
    if(scale == pltAppState->CurrentScale()) {
      // Toggle buttons indicate which is the current scale
      XtVaSetValues(wid, XmNset, true, NULL);
      wCurrScale = wid;
    }
    AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::ChangeScale,
		      (XtPointer) static_cast<long> (scale));
  }

  // Levels button, to view various levels of refinement
  wCascade = XmCreatePulldownMenu(wMenuPulldown, const_cast<char *>("levelmenu"), NULL, 0);
  XtVaCreateManagedWidget("Level", xmCascadeButtonWidgetClass, wMenuPulldown,
			  XmNmnemonic, 'L', XmNsubMenuId, wCascade, NULL);

  wCurrLevel = NULL;
  BL_ASSERT(minAllowableLevel <= maxDrawnLevel);
  for(int menuLevel(minAllowableLevel); menuLevel <= maxDrawnLevel; ++menuLevel) {
    sprintf(selectText, "%i/%i", menuLevel, amrData.FinestLevel());
    wid = XtVaCreateManagedWidget(selectText, xmToggleButtonGadgetClass, wCascade,
				  XmNset, false, NULL);
    if(menuLevel <= 10) {
      // Levels <= 10 are shortcutted by holding down the CTRL key and pressing 1-0
      sprintf(accel, "Ctrl<Key>%i", menuLevel % 10);
      sprintf(accelText, "Ctrl+%i", menuLevel % 10);
      label_str = XmStringCreateSimple(accelText);
      XtVaSetValues(wid, XmNmnemonic, menuLevel + '0',
		    XmNaccelerator, accel,
		    XmNacceleratorText, label_str,
		    NULL);			      
      XmStringFree(label_str);
    }
    if(menuLevel == maxDrawnLevel) {
      // Toggle button indicates current level in view
      XtVaSetValues(wid, XmNset, true, NULL);
      wCurrLevel = wid;
    }
    AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::ChangeLevel,
		      (XtPointer) static_cast<long>(menuLevel));
  }
  Widget wTempDrawLevel = wid;

  // Button to create (or pop up) a dialog to set contour settings
  label_str = XmStringCreateSimple(const_cast<char *>("C"));
  wid = XtVaCreateManagedWidget("Contours...",
				xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'C',
				// XmNaccelerator, "<Key>C",
				// XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoContoursButton);

  // Button to create (or pop up) a dialog to set range
  label_str = XmStringCreateSimple(const_cast<char *>("R"));
  wid = XtVaCreateManagedWidget("Range...",
				xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'R',
				// XmNaccelerator, "<Key>R",
				// XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoSetRangeButton);

  // To create (or update and pop up) a dialog indicating the data values
  // of a selected region
  label_str = XmStringCreateSimple(const_cast<char *>("Ctrl+D"));
  wid = XtVaCreateManagedWidget("Dataset...",
				xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'D',
			        XmNaccelerator, "Ctrl<Key>D",
				XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoDatasetButton);

  // To change the number format string
  label_str = XmStringCreateSimple(const_cast<char *>("F"));
  wid = XtVaCreateManagedWidget("Number Format...",
				xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic,  'F',
				// XmNaccelerator, "<Key>F",
				// XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoNumberFormatButton);

  if(bTimeline) {
    // To display the call stack
    wid = XtVaCreateManagedWidget("Call Trace...",
				  xmPushButtonGadgetClass, wMenuPulldown,
				  XmNmnemonic,  'T',
				  NULL);
    AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoCallTraceButton);

  }

  XtVaCreateManagedWidget(NULL, xmSeparatorGadgetClass, wMenuPulldown,
			  NULL);

  // Toggle viewing the boxes
  label_str = XmStringCreateSimple(const_cast<char *>("b"));
  wid = XtVaCreateManagedWidget("Boxes",
				xmToggleButtonGadgetClass, wMenuPulldown,
				XmNmnemonic, 'B',
				XmNset, pltAppState->GetShowingBoxes(),
				XmNaccelerator, "<Key>B",
				XmNacceleratorText, label_str,
				NULL);
  XmStringFree(label_str);
  AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::DoBoxesButton);

  if(amrData.CartGrid()) {
    // cart grid smoothing
    label_str = XmStringCreateSimple(const_cast<char *>("s"));
    wid = XtVaCreateManagedWidget("CG Smooth",
				  xmToggleButtonGadgetClass, wMenuPulldown,
				  XmNmnemonic, 'S',
				  XmNset, pltAppState->GetCGSmoothing(),
				  XmNaccelerator, "<Key>S",
				  XmNacceleratorText, label_str,
				  NULL);
    XmStringFree(label_str);
    AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::DoCGSmoothing);

    label_str = XmStringCreateSimple(const_cast<char *>("Alt+b"));
    wid = XtVaCreateManagedWidget("Show Body",
				  xmToggleButtonGadgetClass, wMenuPulldown,
				  XmNmnemonic, 'B',
				  XmNset, AVGlobals::GetShowBody(),
				  XmNaccelerator, "Alt<Key>b",
				  XmNacceleratorText, label_str,
				  NULL);
    XmStringFree(label_str);
    AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::DoCGShowBody);
    unsigned long numberOfDerived(dataServicesPtr[currentFrame]->NumDeriveFunc());
    AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::ChangeDerived,
		      (XtPointer) numberOfDerived);
  }

  // ------------------------------- derived menu
  int maxMenuItems(initialMaxMenuItems);  // arbitrarily
  int numberOfDerived(dataServicesPtr[currentFrame]->NumDeriveFunc());
  const Vector<string> &derivedStrings =
	     dataServicesPtr[currentFrame]->PlotVarNames();

  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, const_cast<char *>("DerivedPulldown"), NULL, 0);
  XtVaSetValues(wMenuPulldown,
		XmNpacking, XmPACK_COLUMN,
		XmNnumColumns, numberOfDerived / maxMenuItems +
		((numberOfDerived % maxMenuItems == 0) ? 0 : 1), NULL);
  XtVaCreateManagedWidget("Variable", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'a', XmNsubMenuId,   wMenuPulldown, NULL);
  wCurrDerived = XtVaCreateManagedWidget(derivedStrings[0].c_str(),
					 xmToggleButtonGadgetClass, wMenuPulldown,
					 XmNset, true, NULL);
  AddStaticCallback(wCurrDerived, XmNvalueChangedCallback, &PltApp::ChangeDerived,
		    (XtPointer) 0);
  for(unsigned long derived(1); derived < numberOfDerived; ++derived) {
    wid = XtVaCreateManagedWidget(derivedStrings[derived].c_str(),
				  xmToggleButtonGadgetClass, wMenuPulldown,
				  XmNset, false, NULL);
    if(derivedStrings[derived] == pltAppState->CurrentDerived()) {
      XtVaSetValues(wCurrDerived, XmNset, false, NULL);
      XtVaSetValues(wid, XmNset, true, NULL);
      wCurrDerived = wid;
    }
    AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::ChangeDerived,
		      (XtPointer) derived);
  }  

#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  lightingModel = ! AVGlobals::StartWithValueModel();
  showing3dRender = false;
  preClassify = true;

  // RENDER MENU
  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, const_cast<char *>("RenderPulldown"), NULL, 0);
  XtVaCreateManagedWidget("Render", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'R', XmNsubMenuId, wMenuPulldown, NULL);

  wAutoDraw =
    XtVaCreateManagedWidget("Autodraw", xmToggleButtonGadgetClass, wMenuPulldown,
			    XmNmnemonic, 'A', XmNset, false, NULL);
  AddStaticCallback(wAutoDraw, XmNvalueChangedCallback, &PltApp::DoAutoDraw);
  
  wid = XtVaCreateManagedWidget("Lighting...",
				xmPushButtonGadgetClass, wMenuPulldown,
				XmNmnemonic, 'L', NULL);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoCreateLightingWindow);
  
  wCascade = XmCreatePulldownMenu(wMenuPulldown, const_cast<char *>("rendermodemenu"), NULL, 0);
  XtVaCreateManagedWidget("Render Mode", xmCascadeButtonWidgetClass, wMenuPulldown,
			  XmNmnemonic, 'M', XmNsubMenuId, wCascade, NULL);
  wid = XtVaCreateManagedWidget("Light", xmToggleButtonGadgetClass,
				wCascade, XmNset, lightingModel, NULL);
  AddStaticCallback(wid, XmNvalueChangedCallback,
		    &PltApp::DoRenderModeMenu, (XtPointer) 0);
  if(lightingModel == true) {
    wCurrentRenderMode = wid;
  }
  wid = XtVaCreateManagedWidget("Value", xmToggleButtonGadgetClass,
				wCascade, XmNset,( ! lightingModel), NULL);
  AddStaticCallback(wid, XmNvalueChangedCallback,
		    &PltApp::DoRenderModeMenu, (XtPointer) 1);
  if(lightingModel == false) {
    wCurrentRenderMode = wid;
  }
  
  wCascade = XmCreatePulldownMenu(wMenuPulldown, const_cast<char *>("classifymenu"), NULL, 0);
  XtVaCreateManagedWidget("Classify", xmCascadeButtonWidgetClass, wMenuPulldown,
			  XmNmnemonic, 'C', XmNsubMenuId, wCascade, NULL);
  wid = XtVaCreateManagedWidget("PC", xmToggleButtonGadgetClass,
				wCascade, XmNset, true, NULL);
  AddStaticCallback(wid, XmNvalueChangedCallback,
		    &PltApp::DoClassifyMenu, (XtPointer) 0);
  wCurrentClassify = wid;

  wid = XtVaCreateManagedWidget("OT", xmToggleButtonGadgetClass,
				wCascade, XmNset, false, NULL);
  AddStaticCallback(wid, XmNvalueChangedCallback,
		    &PltApp::DoClassifyMenu, (XtPointer) 1);
#endif

  // --------------------------------------------------------------- help menu
  wMenuPulldown = XmCreatePulldownMenu(wMenuBar, const_cast<char *>("MenuPulldown"), NULL, 0);
  XtVaCreateManagedWidget("Help", xmCascadeButtonWidgetClass, wMenuBar,
			  XmNmnemonic, 'H', XmNsubMenuId,   wMenuPulldown, NULL);
  wid = XtVaCreateManagedWidget("Info...", xmPushButtonGadgetClass,
				wMenuPulldown, XmNmnemonic,  'I', NULL);
  AddStaticCallback(wid, XmNactivateCallback, &PltApp::DoInfoButton);
  
  XtManageChild(wMenuBar);


  // --------------------------------------------Palette frame and drawing area
  wPalFrame = XtVaCreateManagedWidget("paletteframe", xmFrameWidgetClass, wMainArea,
			    XmNtopAttachment,   XmATTACH_WIDGET,
			    XmNtopWidget,       wMenuBar,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNrightOffset,     1,
			    XmNshadowType,      XmSHADOW_ETCHED_IN,
			    NULL);
  wPalForm = XtVaCreateManagedWidget("paletteform", xmFormWidgetClass, wPalFrame,
			    NULL);

  wPalArea = XtVaCreateManagedWidget("palarea", xmDrawingAreaWidgetClass, wPalForm,
			    XmNtopAttachment,    XmATTACH_FORM,
			    XmNtopOffset,        30,
			    XmNleftAttachment,   XmATTACH_FORM,
			    XmNrightAttachment,  XmATTACH_FORM,
			    XmNbottomAttachment, XmATTACH_FORM,
			    XmNwidth,		 totalPalWidth,
			    XmNheight,		 AVPalette::TOTALPALHEIGHT,
			    NULL);
  AddStaticEventHandler(wPalArea, ExposureMask, &PltApp::DoExposePalette);

  // Indicate the unit type of the palette (legend) area above it.
  strcpy(buffer, pltAppState->CurrentDerived().c_str());
  label_str = XmStringCreateSimple(buffer);
  wPlotLabel = XtVaCreateManagedWidget("plotlabel", xmLabelWidgetClass, wPalForm,
			    XmNtopAttachment,    XmATTACH_FORM,
			    XmNleftAttachment,   XmATTACH_FORM,
			    XmNrightAttachment,  XmATTACH_FORM,
			    XmNbottomAttachment, XmATTACH_WIDGET,
			    XmNbottomWidget,     wPalArea,
			    XmNleftOffset,       0,
			    XmNrightOffset,      0,
			    XmNbottomOffset,     0,
			    XmNbackground,       pltPaletteptr->AVBlackPixel(),
			    XmNforeground,       pltPaletteptr->AVWhitePixel(),
			    XmNlabelString,      label_str,
			    NULL);
  XmStringFree(label_str);


  // ************************************** Controls frame and area
  Widget wControlsFrame;
  wControlsFrame = XtVaCreateManagedWidget("controlsframe",
			    xmFrameWidgetClass, wMainArea,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNrightOffset,     1,
			    XmNtopAttachment,   XmATTACH_WIDGET,
			    XmNtopWidget,       wPalFrame,
			    XmNshadowType,      XmSHADOW_ETCHED_IN,
			    NULL);

  unsigned long wc;
  int wcfWidth(totalPalWidth), wcfHeight(AVPalette::TOTALPALHEIGHT);
  int centerX(wcfWidth / 2), centerY((wcfHeight / 2) - 16);
  int controlSize(16);
  int halfbutton(controlSize / 2);
  wControlForm = XtVaCreateManagedWidget("refArea",
			    xmDrawingAreaWidgetClass, wControlsFrame,
			    XmNwidth,	wcfWidth,
			    XmNheight,	wcfHeight,
			    XmNresizePolicy, XmRESIZE_NONE,
			    NULL);
  AddStaticEventHandler(wControlForm, ExposureMask, &PltApp::DoExposeRef);

#if (BL_SPACEDIM == 3)

  wControls[WCSTOP] =
    XtVaCreateManagedWidget("0", xmPushButtonWidgetClass, wControlForm,
			    XmNx, centerX - halfbutton,
			    XmNy, centerY - halfbutton,
			    XmCMarginBottom, 2,
			    NULL);
  XtManageChild(wControls[WCSTOP]);
  
  wControls[WCXNEG] =
    XtVaCreateManagedWidget("wcxneg", xmArrowButtonWidgetClass, wControlForm,
			    XmNarrowDirection,      XmARROW_LEFT,
			    XmNx, centerX + halfbutton,
			    XmNy, centerY - halfbutton,
			    NULL);
  wControls[WCXPOS] =
    XtVaCreateManagedWidget("wcxpos", xmArrowButtonWidgetClass, wControlForm,
			    XmNarrowDirection,      XmARROW_RIGHT,
			    XmNx, centerX + 3 * halfbutton,
			    XmNy, centerY - halfbutton,
			    NULL);
  wControls[WCYNEG] =
    XtVaCreateManagedWidget("wcyneg", xmArrowButtonWidgetClass, wControlForm,
			    XmNarrowDirection,      XmARROW_DOWN,
			    XmNx, centerX - halfbutton,
			    XmNy, centerY - 3 * halfbutton,
			    NULL);
  wControls[WCYPOS] =
    XtVaCreateManagedWidget("wcypos", xmArrowButtonWidgetClass, wControlForm,
			    XmNarrowDirection,      XmARROW_UP,
			    XmNx, centerX - halfbutton,
			    XmNy, centerY - 5 * halfbutton,
			    NULL);
  wControls[WCZNEG] =
    XtVaCreateManagedWidget("wczneg", xmArrowButtonWidgetClass, wControlForm,
			    XmNarrowDirection,      XmARROW_RIGHT,
			    XmNx, centerX - 3 * halfbutton + 1,
			    XmNy, centerY + halfbutton - 1,
			    NULL);
  wControls[WCZPOS] =
    XtVaCreateManagedWidget("wczpos", xmArrowButtonWidgetClass, wControlForm,
			    XmNarrowDirection,      XmARROW_LEFT,
			    XmNx, centerX - 5 * halfbutton + 2,
			    XmNy, centerY + 3 * halfbutton - 2,
			    NULL);
  
  for(wc = WCSTOP; wc <= WCZPOS; ++wc) {
    XtVaSetValues(wControls[wc],
		  XmNwidth, controlSize,
		  XmNheight, controlSize,
		  XmNborderWidth, 0,
		  XmNhighlightThickness, 0,
		  NULL); 
    AddStaticCallback(wControls[wc], XmNactivateCallback, &PltApp::ChangePlane,
		      (XtPointer) wc);
  }
  
#endif
  int adjustHeight2D = (BL_SPACEDIM != 3) ? centerY : 0;
  Dimension slw;
  
  if(animating2d) {
    wControls[WCASTOP] =
      XtVaCreateManagedWidget("0", xmPushButtonWidgetClass, wControlForm,
			      XmNx, centerX-halfbutton,
			      XmCMarginBottom, 2,
			      NULL);
    
    wControls[WCATNEG] =
      XtVaCreateManagedWidget("wcatneg", xmArrowButtonWidgetClass, wControlForm,
			      XmNarrowDirection,      XmARROW_LEFT,
			      XmNx, centerX - 3 * halfbutton,
			      NULL);
    wControls[WCATPOS] =
      XtVaCreateManagedWidget("wcatpos", xmArrowButtonWidgetClass, wControlForm,
			      XmNarrowDirection, XmARROW_RIGHT,
			      XmNx, centerX + halfbutton,
			      NULL);
    for(wc = WCATNEG; wc <= WCATPOS; ++wc) {
      XtVaSetValues(wControls[wc], XmNwidth,  controlSize, XmNheight, controlSize,
		    XmNborderWidth, 0, XmNhighlightThickness, 0,
		    XmNy, wcfHeight-adjustHeight2D, NULL); 
      AddStaticCallback(wControls[wc], XmNactivateCallback, &PltApp::ChangePlane,
			(XtPointer) wc);
    }

    if(AVGlobals::IsSGIrgbFile()) {
	wControls[WCARGB] =
	    XtVaCreateManagedWidget("rgb >", xmPushButtonWidgetClass, wControlForm,
				    XmCMarginBottom, 2,
				    XmNwidth,  3 * controlSize,
				    XmNheight, controlSize,
				    XmNborderWidth, 0,
				    XmNhighlightThickness, 0,
				    XmNx, centerX - 3 * halfbutton,
				    XmNy, wcfHeight + 3 * halfbutton-adjustHeight2D,
				    NULL);
    } else {
	wControls[WCARGB] =
	    XtVaCreateManagedWidget("ppm >", xmPushButtonWidgetClass, wControlForm,
				    XmCMarginBottom, 2,
				    XmNwidth,  3*controlSize,
				    XmNheight, controlSize,
				    XmNborderWidth, 0,
				    XmNhighlightThickness, 0,
				    XmNx, centerX-3*halfbutton,
				    XmNy, wcfHeight + 3 * halfbutton-adjustHeight2D,
				    NULL);
    }
    AddStaticCallback(wControls[WCARGB], XmNactivateCallback, &PltApp::ChangePlane,
		      (XtPointer) WCARGB);

    wWhichFileScale =
      XtVaCreateManagedWidget("whichFileScale", xmScaleWidgetClass, wControlForm,
			      XmNminimum,     0,
			      XmNmaximum,     animFrames-1,
			      XmNvalue,	      currentFrame,
			      XmNorientation, XmHORIZONTAL,
			      XmNx,           0,
			      XmNscaleWidth,  wcfWidth * 2 / 3,
			      XmNy,           wcfHeight-7*halfbutton-adjustHeight2D,
			      NULL);
    AddStaticCallback(wWhichFileScale, XmNvalueChangedCallback,
		      &PltApp::DoAnimFileScale);
    
    Widget wAnimLabelFast =
      XtVaCreateManagedWidget("File", xmLabelWidgetClass, wControlForm,	
			      XmNy, wcfHeight-7*halfbutton-adjustHeight2D,
			      NULL);
    XtVaGetValues(wAnimLabelFast, XmNwidth, &slw, NULL);
    XtVaSetValues(wAnimLabelFast, XmNx, wcfWidth - slw, NULL);
    
    string ssFileName(AVGlobals::StripSlashes(fileNames[currentFrame]));
    XmString fileString =
        XmStringCreateSimple(const_cast<char *>(ssFileName.c_str()));
    wWhichFileLabel = XtVaCreateManagedWidget("whichFileLabel",
			      xmLabelWidgetClass, wControlForm,	
			      XmNx, 0,
			      XmNy, wcfHeight -4*halfbutton-3-adjustHeight2D,
			      XmNlabelString,         fileString,
			      NULL);
    XmStringFree(fileString);

    // We add a which-time label to indicate the time of each plot file.
    std::ostringstream tempTimeOut;
    tempTimeOut << "T = " << amrData.Time();
    XmString timeString =
        XmStringCreateSimple(const_cast<char *>(tempTimeOut.str().c_str()));
    wWhichTimeLabel = XtVaCreateManagedWidget("whichTimeLabel",
			      xmLabelWidgetClass, wControlForm,
			      XmNx, 0,
			      XmNy, wcfHeight - 2*halfbutton-3-adjustHeight2D,
			      XmNlabelString, timeString,
			      NULL);
    XmStringFree(timeString);
    
    std::ostringstream headerout;
    headerout << ssFileName << "  " << tempTimeOut.str() << "  " << headerSuffix;
    XtVaSetValues(wAmrVisTopLevel,
                  XmNtitle, const_cast<char *>(headerout.str().c_str()),
		  NULL);

  }  // end if(animating2d)

  if(animating2d || BL_SPACEDIM == 3) {   
    Widget wSpeedScale =
      XtVaCreateManagedWidget("speed", xmScaleWidgetClass, wControlForm,
			      XmNminimum,		0, 
			      XmNmaximum,		599, 
			      XmNvalue,		        600 - frameSpeed,
			      XmNorientation,		XmHORIZONTAL,
			      XmNx,                     0,
			      XmNscaleWidth,  		wcfWidth * 2 / 3,
			      XmNy, wcfHeight - 9 * halfbutton - adjustHeight2D,
			      NULL);
    
    AddStaticCallback(wSpeedScale, XmNvalueChangedCallback, &PltApp::DoSpeedScale);
    Widget wSpeedLabel =
      XtVaCreateManagedWidget("Speed", xmLabelWidgetClass, wControlForm,	
			      XmNy, wcfHeight - 9 * halfbutton - adjustHeight2D,
			      NULL);
    XtVaGetValues(wSpeedLabel, XmNwidth, &slw, NULL);
    XtVaSetValues(wSpeedLabel, XmNx, wcfWidth-slw, NULL);
     
  }

  // ************************** Plot frame and area

  wPlotFrame = XtVaCreateManagedWidget("plotframe",
			    xmFrameWidgetClass,   wMainArea,
			    XmNrightAttachment,	  XmATTACH_WIDGET,
			    XmNrightWidget,	  wPalFrame,
			    XmNleftAttachment,    XmATTACH_FORM,
			    XmNbottomAttachment,  XmATTACH_FORM,
			    XmNtopAttachment,	  XmATTACH_WIDGET,
			    XmNtopWidget,         wMenuBar,
			    XmNshadowType,	  XmSHADOW_ETCHED_IN,
			    XmNspacing,           0,
			    XmNmarginHeight,      0,
			    NULL);

  Widget wPicArea = XtVaCreateManagedWidget("picarea",
			    xmFormWidgetClass, wPlotFrame,
			    NULL);

  wLocArea = XtVaCreateManagedWidget("locarea",xmDrawingAreaWidgetClass, wPicArea,
			    XmNleftAttachment,   XmATTACH_FORM,
			    XmNrightAttachment,  XmATTACH_FORM,
			    XmNbottomAttachment, XmATTACH_FORM,
			    XmNheight,	         30,
			    NULL);

  wPlotArea = XtVaCreateManagedWidget("plotarea", xmFormWidgetClass, wPicArea,
			    XmNtopAttachment,    XmATTACH_FORM,
			    XmNleftAttachment,   XmATTACH_FORM,
			    XmNrightAttachment,  XmATTACH_FORM,
			    XmNbottomAttachment, XmATTACH_WIDGET,
			    XmNbottomWidget,     wLocArea, NULL);

  wScrollArea[Amrvis::ZPLANE] = XtVaCreateManagedWidget("scrollAreaXY",
		xmScrolledWindowWidgetClass,
		wPlotArea,
		XmNleftAttachment,	XmATTACH_FORM,
		XmNleftOffset,		0,
		XmNtopAttachment,	XmATTACH_FORM,
		XmNtopOffset,		0,
		XmNscrollingPolicy,	XmAUTOMATIC,
		NULL);

  trans = const_cast<char *>(
	"<Btn1Motion>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn1Down>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn1Up>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn2Motion>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn2Down>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn2Up>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn3Motion>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn3Down>: DrawingAreaInput() ManagerGadgetButtonMotion()	\n\
	<Btn3Up>: DrawingAreaInput() ManagerGadgetButtonMotion()");
	
  wPlotPlane[Amrvis::ZPLANE] = XtVaCreateManagedWidget("plotArea",
			    xmDrawingAreaWidgetClass, wScrollArea[Amrvis::ZPLANE],
			    XmNtranslations,  XtParseTranslationTable(trans),
			    XmNwidth, amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeH() + 1,
			    XmNheight, amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeV() + 1,
			    NULL);
  XtVaSetValues(wScrollArea[Amrvis::ZPLANE], XmNworkWindow, wPlotPlane[Amrvis::ZPLANE], NULL); 
#if (BL_SPACEDIM == 2 || BL_SPACEDIM == 1)
  XtVaSetValues(wScrollArea[Amrvis::ZPLANE], XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM, NULL);		
#elif (BL_SPACEDIM == 3)
  XtVaSetValues(wAmrVisTopLevel, XmNwidth, 1100, XmNheight, 650, NULL);
  
  XtVaSetValues(wScrollArea[Amrvis::ZPLANE], 
		XmNrightAttachment,	XmATTACH_POSITION,
		XmNrightPosition,	50,	// %
		XmNbottomAttachment,	XmATTACH_POSITION,
		XmNbottomPosition,	50,
		NULL);

// ********************************************************* XZ
  wScrollArea[Amrvis::YPLANE] =
    XtVaCreateManagedWidget("scrollAreaXZ",
			    xmScrolledWindowWidgetClass, wPlotArea,
			    XmNrightAttachment,	 XmATTACH_FORM,
			    XmNleftAttachment,	 XmATTACH_WIDGET,
			    XmNleftWidget,	 wScrollArea[Amrvis::ZPLANE],
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition,	 50,
			    XmNtopAttachment,	 XmATTACH_FORM,
			    XmNtopOffset,	 0,
			    XmNscrollingPolicy,	 XmAUTOMATIC,
			    NULL);
  
  wPlotPlane[Amrvis::YPLANE] = XtVaCreateManagedWidget("plotArea",
		         xmDrawingAreaWidgetClass, wScrollArea[Amrvis::YPLANE],
		         XmNtranslations, XtParseTranslationTable(trans),
		         XmNwidth,  amrPicturePtrArray[Amrvis::YPLANE]->ImageSizeH() + 1,
		         XmNheight, amrPicturePtrArray[Amrvis::YPLANE]->ImageSizeV() + 1,
		         NULL);
  XtVaSetValues(wScrollArea[Amrvis::YPLANE], XmNworkWindow, wPlotPlane[Amrvis::YPLANE], NULL); 
  
  // ********************************************************* YZ
  wScrollArea[Amrvis::XPLANE] =
    XtVaCreateManagedWidget("scrollAreaYZ",
			    xmScrolledWindowWidgetClass, wPlotArea,
			    XmNbottomAttachment,	XmATTACH_FORM,
			    XmNleftAttachment,	XmATTACH_FORM,
			    XmNrightAttachment,	XmATTACH_POSITION,
			    XmNrightPosition,	50,	// %
			    XmNtopAttachment,	XmATTACH_POSITION,
			    XmNtopPosition,		50,	// %
			    XmNscrollingPolicy,	XmAUTOMATIC,
			    NULL);
  
  wPlotPlane[Amrvis::XPLANE] = XtVaCreateManagedWidget("plotArea",
			    xmDrawingAreaWidgetClass, wScrollArea[Amrvis::XPLANE],
			    XmNtranslations,  XtParseTranslationTable(trans),
			    XmNwidth,  amrPicturePtrArray[Amrvis::XPLANE]->ImageSizeH() + 1,
			    XmNheight, amrPicturePtrArray[Amrvis::XPLANE]->ImageSizeV() + 1,
			    NULL);
  XtVaSetValues(wScrollArea[Amrvis::XPLANE], XmNworkWindow, wPlotPlane[Amrvis::XPLANE], NULL); 
  
  // ****************************************** Transform Area & buttons
  wOrientXY = XtVaCreateManagedWidget("XY",
				    xmPushButtonGadgetClass, wPlotArea,
				    XmNleftAttachment, XmATTACH_WIDGET,
				    XmNleftWidget, wScrollArea[Amrvis::XPLANE],
				    XmNleftOffset, Amrvis::WOFFSET,
				    XmNtopAttachment, XmATTACH_POSITION,
				    XmNtopPosition, 50,
				    NULL);
  AddStaticCallback(wOrientXY, XmNactivateCallback, &PltApp::DoOrient,
                    (XtPointer) OXY);
  XtManageChild(wOrientXY);
  
  wOrientYZ = XtVaCreateManagedWidget("YZ",
				    xmPushButtonGadgetClass, wPlotArea,
				    XmNleftAttachment, XmATTACH_WIDGET,
				    XmNleftWidget, wOrientXY,
				    XmNleftOffset, Amrvis::WOFFSET,
				    XmNtopAttachment, XmATTACH_POSITION,
				    XmNtopPosition, 50,
				    NULL);
  AddStaticCallback(wOrientYZ, XmNactivateCallback, &PltApp::DoOrient,
                    (XtPointer) OYZ);
  XtManageChild(wOrientYZ);
  
  wOrientXZ = XtVaCreateManagedWidget("XZ",
				    xmPushButtonGadgetClass, wPlotArea,
				    XmNleftAttachment, XmATTACH_WIDGET,
				    XmNleftWidget, wOrientYZ,
				    XmNleftOffset, Amrvis::WOFFSET,
				    XmNtopAttachment, XmATTACH_POSITION,
				    XmNtopPosition, 50,
				    NULL);
  AddStaticCallback(wOrientXZ, XmNactivateCallback, &PltApp::DoOrient,
                    (XtPointer) OXZ);
  XtManageChild(wOrientXZ);
  
  wLabelAxes = XtVaCreateManagedWidget("XYZ",
				       xmPushButtonGadgetClass, wPlotArea,
				       XmNleftAttachment, XmATTACH_WIDGET,
				       XmNleftWidget, wOrientXZ,
				       XmNleftOffset, Amrvis::WOFFSET,
				       XmNtopAttachment, XmATTACH_POSITION,
				       XmNtopPosition, 50,
				       NULL);
  AddStaticCallback(wLabelAxes, XmNactivateCallback, &PltApp::DoLabelAxes);
  XtManageChild(wLabelAxes);
  
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  wRender = XtVaCreateManagedWidget("Draw",
				    xmPushButtonGadgetClass, wPlotArea,
				    XmNleftAttachment, XmATTACH_WIDGET,
				    XmNleftWidget, wLabelAxes,
				    XmNleftOffset, Amrvis::WOFFSET,
				    XmNtopAttachment, XmATTACH_POSITION,
				    XmNtopPosition, 50, NULL);
  AddStaticCallback(wRender, XmNactivateCallback, &PltApp::DoRender);
  XtManageChild(wRender);
#endif
  
  XmString sDetach = XmStringCreateSimple(const_cast<char *>("Detach"));
  wDetach =
    XtVaCreateManagedWidget("detach", xmPushButtonGadgetClass,
			    wPlotArea,
			    XmNlabelString, sDetach,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNrightOffset, Amrvis::WOFFSET,
			    XmNtopAttachment, XmATTACH_POSITION,
			    XmNtopPosition, 50,
			    NULL);
  XmStringFree(sDetach);
  AddStaticCallback(wDetach, XmNactivateCallback, &PltApp::DoDetach);
  XtManageChild(wDetach);
  
  wTransDA = XtVaCreateManagedWidget("transDA", xmDrawingAreaWidgetClass,
			     wPlotArea,
			     XmNtranslations, XtParseTranslationTable(trans),
			     XmNleftAttachment,	        XmATTACH_WIDGET,
			     XmNleftWidget,		wScrollArea[Amrvis::XPLANE],
			     XmNtopAttachment,	        XmATTACH_WIDGET,
			     XmNtopWidget,		wOrientXY,
			     XmNrightAttachment,	XmATTACH_FORM,
			     XmNbottomAttachment,	XmATTACH_FORM,
			     NULL);

#endif
  
  for(np = 0; np < Amrvis::NPLANES; ++np) {
    AddStaticCallback(wPlotPlane[np], XmNinputCallback,
		      &PltApp::DoRubberBanding, (XtPointer) static_cast<long>(np));
    AddStaticEventHandler(wPlotPlane[np], ExposureMask, &PltApp::PADoExposePicture,
			  (XtPointer) static_cast<long>(np));
    AddStaticEventHandler(wPlotPlane[np], PointerMotionMask | LeaveWindowMask,
			  &PltApp::DoDrawPointerLocation,
			  (XtPointer) static_cast<long>(np));
  }
  
  
  // ***************************************************************** 
  XtManageChild(wPalArea);
  XtManageChild(wPlotArea);
  XtPopup(wAmrVisTopLevel, XtGrabNone);
  
  pltPaletteptr->SetWindow(XtWindow(wPalArea));
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPalArea), false);
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPlotArea), false);
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wAmrVisTopLevel), false);
  for(np = 0; np != Amrvis::NPLANES; ++np) {
    pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wPlotPlane[np]), false);
  }
  
#if (BL_SPACEDIM == 3)
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wTransDA), false);

  Dimension width, height;
  XtVaGetValues(wTransDA, XmNwidth, &width, XmNheight, &height, NULL);
  daWidth  = width;
  daHeight = height;
  
  projPicturePtr = new ProjectionPicture(this, &viewTrans, pltPaletteptr,
					 wTransDA, daWidth, daHeight);
  AddStaticCallback(wTransDA, XmNinputCallback, &PltApp::DoTransInput);
  AddStaticCallback(wTransDA, XmNresizeCallback, &PltApp::DoTransResize);
  AddStaticEventHandler(wTransDA, ExposureMask, &PltApp::DoExposeTransDA);
  DoTransResize(wTransDA, NULL, NULL);
  
#endif

  char plottertitle[50];
  sprintf(plottertitle, "XYPlot%dd", BL_SPACEDIM);
  XYplotparameters = new XYPlotParameters(pltPaletteptr, gaPtr, plottertitle);

  for(np = 0; np < Amrvis::NPLANES; ++np) {
    amrPicturePtrArray[np]->CreatePicture(XtWindow(wPlotPlane[np]),
					  pltPaletteptr);
  }

#if (BL_SPACEDIM == 3)
  viewTrans.MakeTransform();
  labelAxes = false;
  transDetached = false;
  
  for(np = 0; np < Amrvis::NPLANES; ++np) {
    startcutX[np] = 0;
    startcutY[np] = amrPicturePtrArray[np]->GetHLine();
    finishcutX[np] = 0;
    finishcutY[np] = amrPicturePtrArray[np]->GetHLine();
    amrPicturePtrArray[np]->ToggleShowSubCut();
  }

  projPicturePtr->ToggleShowSubCut(); 
  projPicturePtr->MakeBoxes(); 

#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  projPicturePtr->GetVolRenderPtr()->SetLightingModel(lightingModel);
#endif

#endif

  interfaceReady = true;

  cbdPtrs.reserve(512);  // arbitrarily
  
  if(bSubVolume) {
    //ChangeLevel(wTempDrawLevel, (XtPointer)(pltAppState->MaxDrawnLevel()), NULL);
    XtVaSetValues(wTempDrawLevel, XmNset, true, NULL);
    ChangeLevel(wTempDrawLevel,
                (XtPointer) static_cast<long>(pltAppState->MaxAllowableLevel()),
		NULL);
  }
}  // end PltAppInit()


// -------------------------------------------------------------------
void PltApp::FindAndSetMinMax(const Amrvis::MinMaxRangeType mmrangetype,
			      const int framenumber,
		              const string &currentderived,
			      const int derivednumber,
		              const Vector<Box> &onBox,
		              const int coarselevel, const int finelevel,
		              const bool resetIfSet)
{
  Real rMin, rMax, levMin, levMax;
  bool isSet(pltAppState->IsSet(mmrangetype, framenumber, derivednumber));
  if(isSet == false || resetIfSet) {  // find and set the mins and maxes
    rMin =  std::numeric_limits<Real>::max();
    rMax = -std::numeric_limits<Real>::max();
    for(int lev(coarselevel); lev <= finelevel; ++lev) {
      bool minMaxValid(false);
      DataServices::Dispatch(DataServices::MinMaxRequest,
                             dataServicesPtr[framenumber],
                             (void *) &(onBox[lev]),
                             (void *) &(currentderived),
                             lev, &levMin, &levMax, &minMaxValid);
      if(minMaxValid) {
        rMin = min(rMin, levMin);
        rMax = max(rMax, levMax);
      }
    }
    if(bTimeline) {
      rMin = timelineMin;
      rMax = timelineMax;
    }
    pltAppState->SetMinMax(mmrangetype, framenumber, derivednumber, rMin, rMax);
  }
}


// -------------------------------------------------------------------
void PltApp::DoExposeRef(Widget, XtPointer, XtPointer) {
  int zPlanePosX(10), zPlanePosY(15);
  int whiteColor(pltPaletteptr->WhiteIndex());
  int zpColor(whiteColor);
  char sX[Amrvis::LINELENGTH], sY[Amrvis::LINELENGTH], sZ[Amrvis::LINELENGTH];
  
  XClearWindow(display, XtWindow(wControlForm));

  strcpy(sX, "X");
  strcpy(sY, "Y");
  strcpy(sZ, "Z");
  
  if(bTimeline) {
    XSetForeground(display, xgc, pltPaletteptr->makePixel(static_cast<unsigned char>(whiteColor)));
    int axisLengthX = 138;
    int axisLengthY = 32;
    int maLevel(pltAppState->MaxAllowableLevel());
    const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
    Box domainBox(amrData.ProbDomain()[maLevel]);
    Box subdomainBox(amrPicturePtrArray[Amrvis::ZPLANE]->GetSubDomain()[maLevel]);
    Real dLength(domainBox.length(0));
    Real sdXL(subdomainBox.smallEnd(0));
    Real sdXH(subdomainBox.smallEnd(0) + subdomainBox.length(0));
    int sdLineXL = domainBox.smallEnd(0) + (static_cast<int>(axisLengthX * sdXL / dLength));
    int sdLineXH = domainBox.smallEnd(0) + (static_cast<int>(axisLengthX * sdXH / dLength));
    Real totalTime(timelineRangeStop - timelineRangeStart);
    Real subTimeRangeStart, subTimeRangeStop;
    if(sdLineXL > 0) {
      subTimeRangeStart = totalTime * static_cast<Real>(sdLineXL) / static_cast<Real>(axisLengthX);
    } else {
      subTimeRangeStart = 0.0;
    }
    if(sdLineXH > 0) {
      subTimeRangeStop = totalTime * static_cast<Real>(sdLineXH) / static_cast<Real>(axisLengthX);
    } else {
      subTimeRangeStop = 0.0;
    }
    subTimeRangeStart += timelineRangeStart;
    subTimeRangeStop  += timelineRangeStart;
std::cout << "TRTRTRTR:  subTimeRangeStart subTimeRangeStop = " << subTimeRangeStart << "  " << subTimeRangeStop << std::endl;
    DrawTimeRange(wControlForm, sdLineXL, sdLineXH, axisLengthX, axisLengthY,
                  subTimeRangeStart, subTimeRangeStop, "mpi rank");

  } else {
    DrawAxes(wControlForm, zPlanePosX, zPlanePosY, 0, sX, sY, zpColor);
#if (BL_SPACEDIM == 3)
  int maxAllowLev(pltAppState->MaxAllowableLevel());
  int maxDrawnLev(pltAppState->MaxDrawnLevel());
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  int crrDiff(amrex::CRRBetweenLevels(maxDrawnLev, maxAllowLev,
              amrData.RefRatio()));
  int axisLength(20);
  int ypColor(whiteColor), xpColor(whiteColor);
  int xyzAxisLength(50);
  int stringOffsetX(4), stringOffsetY(20);
  int yPlanePosX(80), yPlanePosY(15);
  int xPlanePosX(10);
  int xPlanePosY((int) (axisLength + zPlanePosY + 1.4 * stringOffsetY));
  char temp[Amrvis::LINELENGTH];
  Dimension width, height;
  XtVaGetValues(wControlForm, XmNwidth, &width, XmNheight, &height, NULL);
  int centerX((int) width / 2);
  int centerY((int) height / 2 - 16);
  
  DrawAxes(wControlForm, yPlanePosX, yPlanePosY, 0, sX, sZ, ypColor);
  DrawAxes(wControlForm, xPlanePosX, xPlanePosY, 0, sY, sZ, xpColor);
  
  sprintf(temp, "Z=%i", amrPicturePtrArray[Amrvis::ZPLANE]->GetSlice() / crrDiff);
  XSetForeground(display, xgc, pltPaletteptr->makePixel(zpColor));
  XDrawString(display, XtWindow(wControlForm), xgc,
	      centerX-xyzAxisLength+12, centerY+xyzAxisLength+4,
	      temp, strlen(temp));
  
  sprintf(temp, "Y=%i", amrPicturePtrArray[Amrvis::YPLANE]->GetSlice() / crrDiff);
  XSetForeground(display, xgc, pltPaletteptr->makePixel(ypColor));
  XDrawString(display, XtWindow(wControlForm), xgc,
	      centerX+stringOffsetX, centerY-xyzAxisLength+4,
	      temp, strlen(temp));
  
  sprintf(temp, "X=%i", amrPicturePtrArray[Amrvis::XPLANE]->GetSlice() / crrDiff);
  XSetForeground(display, xgc, pltPaletteptr->makePixel(xpColor));
  XDrawString(display, XtWindow(wControlForm), xgc,
	      centerX+4*stringOffsetX, centerY+stringOffsetY+2,
	      temp, strlen(temp));
  
  XSetForeground(XtDisplay(wControlForm), xgc,
		 pltPaletteptr->makePixel(ypColor));
  XDrawLine(display, XtWindow(wControlForm), xgc,
	    centerX, centerY, centerX, centerY-xyzAxisLength);
  XDrawLine(display, XtWindow(wControlForm), xgc,
	    centerX, centerY, centerX+xyzAxisLength, centerY);
  XDrawLine(display, XtWindow(wControlForm), xgc,
	    centerX, centerY,
	    (int) (centerX - 0.9 * xyzAxisLength),
	    (int) (centerY + 0.9 * xyzAxisLength));
#endif
  }
}


// -------------------------------------------------------------------
void PltApp::DrawAxes(Widget wArea, int xpos, int ypos, int /* orientation */ ,
		      char *hlabel, char *vlabel, int color)
{
  int axisLength(20);
  char hLabel[Amrvis::LINELENGTH], vLabel[Amrvis::LINELENGTH];
  strcpy(hLabel, hlabel);
  strcpy(vLabel, vlabel);
  XSetForeground(XtDisplay(wArea), xgc, pltPaletteptr->makePixel(color));
  XDrawLine(XtDisplay(wArea), XtWindow(wArea), xgc,
	    xpos+5, ypos, xpos+5, ypos+axisLength);
  XDrawLine(XtDisplay(wArea), XtWindow(wArea), xgc,
	    xpos+5, ypos+axisLength, xpos+5+axisLength, ypos+axisLength);
  XDrawString(XtDisplay(wArea), XtWindow(wArea), xgc,
	      xpos+5+axisLength, ypos+5+axisLength, hLabel, strlen(hLabel));
  XDrawString(XtDisplay(wArea), XtWindow(wArea), xgc,
	      xpos, ypos, vLabel, strlen(vLabel));
}  



// -------------------------------------------------------------------
void PltApp::ChangeScale(Widget w, XtPointer client_data, XtPointer) {
  if(w == wCurrScale) {
    XtVaSetValues(w, XmNset, true, NULL);
    return;
  }
  unsigned long newScale = (unsigned long) client_data;
  XtVaSetValues(wCurrScale, XmNset, false, NULL);
  wCurrScale = w;
  int previousScale(pltAppState->CurrentScale());
  int currentScale(newScale);
  pltAppState->SetCurrentScale(currentScale);
  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }
  for(int whichView(0); whichView < Amrvis::NPLANES; ++whichView) {
    if(whichView == activeView) {
      startX = (int) startX / previousScale * currentScale;
      startY = (int) startY / previousScale * currentScale;
      endX   = (int) endX   / previousScale * currentScale;
      endY   = (int) endY   / previousScale * currentScale;
      amrPicturePtrArray[activeView]->SetRegion(startX, startY, endX, endY);
    }
    startcutX[whichView]  /= previousScale;
    startcutY[whichView]  /= previousScale;
    finishcutX[whichView] /= previousScale;
    finishcutY[whichView] /= previousScale;
    startcutX[whichView]  *= currentScale;
    startcutY[whichView]  *= currentScale;
    finishcutX[whichView] *= currentScale;
    finishcutY[whichView] *= currentScale;
    amrPicturePtrArray[whichView]->SetSubCut(startcutX[whichView],
					     startcutY[whichView],
				             finishcutX[whichView],
					     finishcutY[whichView]);
    amrPicturePtrArray[whichView]->APChangeScale(currentScale, previousScale);
    XtVaSetValues(wPlotPlane[whichView], 
		  XmNwidth,  amrPicturePtrArray[whichView]->ImageSizeH() + 1,
		  XmNheight, amrPicturePtrArray[whichView]->ImageSizeV() + 1,
		  NULL);
  }
  DoExposeRef();
  writingRGB = false;
}

// -------------------------------------------------------------------
void PltApp::ChangeLevel(Widget w, XtPointer client_data, XtPointer) {

  unsigned long newLevel = (unsigned long) client_data;
  int minDrawnLevel(pltAppState->MinAllowableLevel());
  int maxDrawnLevel(newLevel);

  if(wCurrLevel != NULL) {
    XtVaSetValues(wCurrLevel, XmNset, false, NULL);
  }
  if(w == wCurrLevel) {
    XtVaSetValues(w, XmNset, true, NULL);
    //return;
  }
  wCurrLevel = w;

  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }

  pltAppState->SetMinDrawnLevel(minDrawnLevel);
  pltAppState->SetMaxDrawnLevel(maxDrawnLevel);
  for(int whichView(0); whichView < Amrvis::NPLANES; ++whichView) {
    amrPicturePtrArray[whichView]->APChangeLevel();
    
#if (BL_SPACEDIM == 3)
    if(whichView == Amrvis::ZPLANE) {
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
      if( ! XmToggleButtonGetState(wAutoDraw)) {
	projPicturePtr->MakeBoxes(); 
	XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
	DoExposeTransDA();
      }
#else
      projPicturePtr->MakeBoxes(); 
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
      DoExposeTransDA();
#endif
    }
#endif
  }

  if(datasetShowing) {
    int hdir(-1), vdir(-1), sdir(-1);
    if(activeView==Amrvis::ZPLANE) { hdir = Amrvis::XDIR; vdir = Amrvis::YDIR; sdir = Amrvis::ZDIR; }
    if(activeView==Amrvis::YPLANE) { hdir = Amrvis::XDIR; vdir = Amrvis::ZDIR; sdir = Amrvis::YDIR; }
    if(activeView==Amrvis::XPLANE) { hdir = Amrvis::YDIR; vdir = Amrvis::ZDIR; sdir = Amrvis::XDIR; }
    datasetPtr->DatasetRender(trueRegion, amrPicturePtrArray[activeView],
		              this, pltAppState, hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  }
  DoExposeRef();
}

// -------------------------------------------------------------------
void PltApp::ChangeDerived(Widget w, XtPointer client_data, XtPointer) {
  if(w == wCurrDerived) {
    XtVaSetValues(w, XmNset, true, NULL);
    if(AVGlobals::Verbose()) {
      cout << "--------------------- unchanged derived." << endl;
    }
    return;
  }
  unsigned long derivedNumber = (unsigned long) client_data;
  int numberOfDerived(dataServicesPtr[currentFrame]->NumDeriveFunc());
  bool resetMinMax(false);
  if(derivedNumber == numberOfDerived) {  // ---- this is a flag for resetting current derived
    XtVaSetValues(w, XmNset, true, NULL);
    derivedNumber = pltAppState->CurrentDerivedNumber();
    resetMinMax = true;
  } else {
    XtVaSetValues(wCurrDerived, XmNset, false, NULL);
    wCurrDerived = w;
  }
  string derivedName = dataServicesPtr[currentFrame]->PlotVarNames()[derivedNumber];
  pltAppState->SetCurrentDerived(derivedName, derivedNumber);

  if ((bTimeline == true) && (derivedName != "timeline")){
    bTimeline = false;
    pltPaletteptr->SetTimeline(bTimeline);
    pltPaletteptr->RedrawPalette();
  }
  else if ((bTimeline == false) && (derivedName == "timeline")){
    bTimeline = true;
    pltPaletteptr->SetTimeline(bTimeline);
    pltPaletteptr->RedrawPalette();
  }

  int maxDrawnLevel = pltAppState->MaxDrawnLevel();

  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();

  // possibly set all six minmax types here
  const Vector<Box> &onSubregionBox = amrPicturePtrArray[Amrvis::ZPLANE]->GetSubDomain();
  const Vector<Box> &onBox(amrData.ProbDomain());
  int iCDerNum(pltAppState->CurrentDerivedNumber());
  int levelZero(0);
  int coarseLevel(pltAppState->MinAllowableLevel());
  int fineLevel(pltAppState->MaxAllowableLevel());
  Real rGlobalMin, rGlobalMax;
  Real rSubregionMin, rSubregionMax;
  rGlobalMin    =  std::numeric_limits<Real>::max();
  rGlobalMax    = -std::numeric_limits<Real>::max();
  rSubregionMin =  std::numeric_limits<Real>::max();
  rSubregionMax = -std::numeric_limits<Real>::max();
  const string asCDer(pltAppState->CurrentDerived());
  for(int iFrame(0); iFrame < animFrames; ++iFrame) {
    // set FILEGLOBALMINMAX  dont reset if already set
    FindAndSetMinMax(Amrvis::FILEGLOBALMINMAX, iFrame, asCDer, iCDerNum, onBox,
	             //levelZero, pltAppState->FinestLevel(), false);
	             levelZero, amrData.FinestLevel(), resetMinMax);

    // set FILESUBREGIONMINMAX  dont reset if already set
    FindAndSetMinMax(Amrvis::FILESUBREGIONMINMAX, iFrame, asCDer, iCDerNum, onSubregionBox,
	             coarseLevel, fineLevel, resetMinMax);

    // collect file values
    Real rTempMin, rTempMax;
	       
    pltAppState->GetMinMax(Amrvis::FILEGLOBALMINMAX, iFrame, iCDerNum,
			   rTempMin, rTempMax);
    rGlobalMin = min(rGlobalMin, rTempMin);
    rGlobalMax = max(rGlobalMax, rTempMax);

    pltAppState->GetMinMax(Amrvis::FILESUBREGIONMINMAX, iFrame, iCDerNum,
			   rTempMin, rTempMax);
    rSubregionMin = min(rSubregionMin, rTempMin);
    rSubregionMax = max(rSubregionMax, rTempMax);

    // set FILEUSERMINMAX  dont reset if already set
    // this sets values to FILESUBREGIONMINMAX values if the user has not set them
    bool isSet(pltAppState->IsSet(Amrvis::FILEUSERMINMAX, iFrame, iCDerNum));
    if( ! isSet) {
      pltAppState->SetMinMax(Amrvis::FILEUSERMINMAX, iFrame, iCDerNum, rTempMin, rTempMax);
    }
  }

  for(int iFrame(0); iFrame < animFrames; ++iFrame) {
    pltAppState->SetMinMax(Amrvis::GLOBALMINMAX, iFrame, iCDerNum, rGlobalMin, rGlobalMax);
    pltAppState->SetMinMax(Amrvis::SUBREGIONMINMAX, iFrame, iCDerNum,
			   rSubregionMin, rSubregionMax);
    // set FILEUSERMINMAX  dont reset if already set
    // this sets values to FILESUBREGIONMINMAX values if the user has not set them
    bool isSet(pltAppState->IsSet(Amrvis::USERMINMAX, iFrame, iCDerNum));
    if( ! isSet) {
      pltAppState->SetMinMax(Amrvis::USERMINMAX, iFrame, iCDerNum,
			     rSubregionMin, rSubregionMax);
    }
  }
  if(AVGlobals::Verbose()) {
    cout << "_in PltApp::ChangeDerived" << endl;
    pltAppState->PrintSetMap();
    cout << endl;
  }


#if (BL_SPACEDIM == 3)
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
  projPicturePtr->GetVolRenderPtr()->InvalidateSWFData();
  projPicturePtr->GetVolRenderPtr()->InvalidateVPData();
#endif
  Clear();
#endif

  writingRGB = false;
  paletteDrawn = false;
  if(animating2d) {
    ResetAnimation();
    DirtyFrames();
    if(UsingFileRange(currentRangeType)) {
      Real dataMin, dataMax;
      int sCoarseLevel(0);
      int sFineLevel(maxDrawnLevel);
      for(int lev(sCoarseLevel); lev <= sFineLevel; ++lev) {
	bool minMaxValid(false);
	DataServices::Dispatch(DataServices::MinMaxRequest,
			       dataServicesPtr[currentFrame],
			       (void *) &(amrData.ProbDomain()[lev]),
			       (void *) &(pltAppState->CurrentDerived()),
			       lev, &dataMin, &dataMax, &minMaxValid);
	if( ! minMaxValid) {
	  continue;
	}
      }
    } else if(strcmp(pltAppState->CurrentDerived().c_str(),"vfrac") == 0) {
    } else {
      string outbuf("Finding global min & max values for ");
      outbuf += pltAppState->CurrentDerived();
      outbuf += "...\n";
      PrintMessage(const_cast<char *>(outbuf.c_str()));
      
      Real dataMin, dataMax;
      int sCoarseLevel(0);
      int sFineLevel(maxDrawnLevel);
      for(int iFrame(0); iFrame < animFrames; ++iFrame) {
	for(int lev(sCoarseLevel); lev <= sFineLevel; ++lev) {
	  bool minMaxValid(false);
	  DataServices::Dispatch(DataServices::MinMaxRequest,
				 dataServicesPtr[iFrame],
				 (void *) &(amrData.ProbDomain()[lev]),
				 (void *) &(pltAppState->CurrentDerived()),
				 lev, &dataMin, &dataMax, &minMaxValid);
	  if( ! minMaxValid) {
	    continue;
	  }
	}
      }
    } 
  }  // end if(animating2d)
  
  strcpy(buffer, pltAppState->CurrentDerived().c_str());
  XmString label_str = XmStringCreateSimple(buffer);
  XtVaSetValues(wPlotLabel, XmNlabelString, label_str, NULL);
  XmStringFree(label_str);

  for(int iv(0); iv < Amrvis::NPLANES; ++iv) {
    amrPicturePtrArray[iv]->APMakeImages(pltPaletteptr);
  }
  
  if(datasetShowing) {
    int hdir(-1), vdir(-1), sdir(-1);
    hdir = (activeView == Amrvis::XPLANE) ? Amrvis::YDIR : Amrvis::XDIR;
    vdir = (activeView == Amrvis::ZPLANE) ? Amrvis::YDIR : Amrvis::ZDIR;
    sdir = (activeView == Amrvis::YPLANE) ? Amrvis::YDIR : ((activeView == Amrvis::XYPLANE) ? Amrvis::XDIR : Amrvis::ZDIR);
    datasetPtr->DatasetRender(trueRegion, amrPicturePtrArray[activeView],
		              this, pltAppState, hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  }

  if(setRangeShowing) {
    bSetRangeRedraw = true;
    XtDestroyWidget(wSetRangeTopLevel);
    setRangeShowing = false;
    DoSetRangeButton(NULL, NULL, NULL);
    setRangeShowing = true;
  }
}  // end ChangeDerived(...)


// -------------------------------------------------------------------
void PltApp::ChangeContour(Widget /*w*/, XtPointer input_data, XtPointer) {
  Amrvis::ContourType prevCType(pltAppState->GetContourType());
  Amrvis::ContourType newCType = Amrvis::ContourType((unsigned long) input_data);
  if(newCType == prevCType) {
    return;
  }
  pltAppState->SetContourType(newCType);
  XtVaSetValues(wContourLabel,
		XmNsensitive, (newCType != Amrvis::RASTERONLY),
		NULL);
  XtVaSetValues(wNumberContours,
	        XmNsensitive, (newCType != Amrvis::RASTERONLY),
		NULL);
  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }
  for(int np(0); np < Amrvis::NPLANES; ++np) {
    amrPicturePtrArray[np]->APChangeContour(prevCType);
  }
}


// -------------------------------------------------------------------
void PltApp::ReadContourString(Widget w, XtPointer, XtPointer) {
  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }

  char temp[64];
  strcpy(temp, XmTextFieldGetString(w));
  string tmpContourNumString(temp);

  // unexhaustive string check to prevent errors 
  bool stringOk(true);

  //check to make sure the input is an integer, and its length < 3.
  if(tmpContourNumString.length() >= 3) {
    stringOk = false;
    cerr << "Bad contour number string, must be less than 100." << endl;
  } else {
    for(int i(0); i < tmpContourNumString.length(); ++i) {
      if( ! isdigit(tmpContourNumString[i])) {
        stringOk = false;
      }
    }
  }

  if(stringOk) {
    contourNumString = tmpContourNumString;
    pltAppState->SetNumContours(atoi(contourNumString.c_str()));
    for(int ii(0); ii < Amrvis::NPLANES; ++ii) {
        amrPicturePtrArray[ii]->APDraw(pltAppState->MinDrawnLevel(), 
				       pltAppState->MaxDrawnLevel());
    }
  }

  XtVaSetValues(w, XmNvalue, contourNumString.c_str(), NULL);
}


// -------------------------------------------------------------------
void PltApp::ToggleRange(Widget /*w*/, XtPointer client_data, XtPointer call_data) {
  unsigned long r = (unsigned long) client_data;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *) call_data;
  if(state->set == true) {
    currentRangeType = (Amrvis::MinMaxRangeType) r;
    if(bFileRangeButtonSet) {
      if(currentRangeType == Amrvis::GLOBALMINMAX) {
        currentRangeType = Amrvis::FILEGLOBALMINMAX;
      } else if(currentRangeType == Amrvis::SUBREGIONMINMAX) {
        currentRangeType = Amrvis::FILESUBREGIONMINMAX;
      } else if(currentRangeType == Amrvis::USERMINMAX) {
        currentRangeType = Amrvis::FILEUSERMINMAX;
      }
    }
  }
}


// -------------------------------------------------------------------
void PltApp::DoSubregion(Widget, XtPointer, XtPointer) {
  Box subregionBox;
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  int finestLevel(amrData.FinestLevel());
  int maxAllowableLevel(pltAppState->MaxAllowableLevel());
  int newMinAllowableLevel;
  
  // subregionBox is at the maxAllowableLevel
  
#if (BL_SPACEDIM == 1)
  if(selectionBox.bigEnd(Amrvis::XDIR) == 0) {
    return;
  }
  subregionBox = selectionBox + ivLowOffsetMAL;
#endif
  
#if (BL_SPACEDIM == 2)
  if(selectionBox.bigEnd(Amrvis::XDIR) == 0 || selectionBox.bigEnd(Amrvis::YDIR) == 0) {
    return;
  }
  subregionBox = selectionBox + ivLowOffsetMAL;
#endif
  
#if (BL_SPACEDIM == 3)
  int np;
  int currentScale(pltAppState->CurrentScale());
  for(np = 0; np < Amrvis::NPLANES; ++np) {
    startcutX[np]  /= currentScale;
    startcutY[np]  /= currentScale;
    finishcutX[np] /= currentScale;
    finishcutY[np] /= currentScale;
  }
  
  subregionBox.setSmall(Amrvis::XDIR, min(startcutX[Amrvis::ZPLANE], finishcutX[Amrvis::ZPLANE]));
  subregionBox.setSmall(Amrvis::YDIR, min(startcutX[Amrvis::XPLANE], finishcutX[Amrvis::XPLANE]));
  subregionBox.setSmall(Amrvis::ZDIR, (amrPicturePtrArray[Amrvis::XPLANE]->ImageSizeV()-1)/
			currentScale - max(startcutY[Amrvis::XPLANE], finishcutY[Amrvis::XPLANE]));
  subregionBox.setBig(Amrvis::XDIR, max(startcutX[Amrvis::ZPLANE], finishcutX[Amrvis::ZPLANE]));
  subregionBox.setBig(Amrvis::YDIR, max(startcutX[Amrvis::XPLANE], finishcutX[Amrvis::XPLANE]));
  subregionBox.setBig(Amrvis::ZDIR, (amrPicturePtrArray[Amrvis::XPLANE]->ImageSizeV()-1)/
		      currentScale - min(startcutY[Amrvis::XPLANE], finishcutY[Amrvis::XPLANE]));
  
  if(subregionBox.numPts() <= 4) {
    for(np = 0; np < Amrvis::NPLANES; ++np) {
      startcutX[np]  *= currentScale;
      startcutY[np]  *= currentScale;
      finishcutX[np] *= currentScale;
      finishcutY[np] *= currentScale;
    }
    return;
  }

  subregionBox += ivLowOffsetMAL;   // shift to true data region
#endif
  
  Box tempRefinedBox(subregionBox);
  tempRefinedBox.refine(amrex::CRRBetweenLevels(maxAllowableLevel, finestLevel,
					            amrData.RefRatio()));
  // this puts tempRefinedBox in terms of the finest level
  newMinAllowableLevel = pltAppState->MinAllowableLevel();
  //newMinAllowableLevel = min(newMinAllowableLevel, maxAllowableLevel);
  
  // coarsen to the newMinAllowableLevel to align grids
  subregionBox.coarsen(amrex::CRRBetweenLevels(newMinAllowableLevel,
					maxAllowableLevel, amrData.RefRatio()));
  
  Box subregionBoxMAL(subregionBox);
  
  // refine to the finestLevel
  subregionBox.refine(amrex::CRRBetweenLevels(newMinAllowableLevel, finestLevel,
				       amrData.RefRatio()));
  
  maxAllowableLevel = AVGlobals::DetermineMaxAllowableLevel(subregionBox,
                                                 finestLevel,
						 AVGlobals::MaxPictureSize(),
						 amrData.RefRatio());
  subregionBoxMAL.refine(amrex::CRRBetweenLevels(newMinAllowableLevel,
					  maxAllowableLevel, amrData.RefRatio()));
  
  IntVect ivOffset(subregionBoxMAL.smallEnd());
  
  // get the old slices and check if they will be within the new subregion.
  // if not -- choose the limit
  
  // then pass the slices to the subregion constructor below...
  SubregionPltApp(wTopLevel, subregionBox, ivOffset,
		  this, palFilename, animating2d, pltAppState->CurrentDerived(),
		  fileName);
  
#if (BL_SPACEDIM == 3)
  for(np = 0; np < Amrvis::NPLANES; ++np) {
    startcutX[np]  *= currentScale;
    startcutY[np]  *= currentScale;
    finishcutX[np] *= currentScale;
    finishcutY[np] *= currentScale;
  }
#endif
}  // end DoSubregion

  
// -------------------------------------------------------------------
void PltApp::DoDatasetButton(Widget, XtPointer, XtPointer) {
#if (BL_SPACEDIM == 1)
  if(selectionBox.bigEnd(Amrvis::XDIR) == 0) {
    return;
  }
#else
  if(selectionBox.bigEnd(Amrvis::XDIR) == 0 || selectionBox.bigEnd(Amrvis::YDIR) == 0) {
    return;
  }
#endif

  int hdir(-1), vdir(-1), sdir(-1);
  
  trueRegion = selectionBox;
  
#if (BL_SPACEDIM == 3)
  trueRegion.setSmall(Amrvis::ZDIR, amrPicturePtrArray[Amrvis::ZPLANE]->GetSlice()); 
  trueRegion.setBig(Amrvis::ZDIR,   amrPicturePtrArray[Amrvis::ZPLANE]->GetSlice()); 
#endif

  hdir = (activeView == Amrvis::XPLANE) ? Amrvis::YDIR : Amrvis::XDIR;
  vdir = (activeView == Amrvis::ZPLANE) ? Amrvis::YDIR : Amrvis::ZDIR;
  switch (activeView) {
  case Amrvis::ZPLANE: // orient box to view and shift
    trueRegion.shift(Amrvis::XDIR, ivLowOffsetMAL[Amrvis::XDIR]);
#if (BL_SPACEDIM != 1)
    trueRegion.shift(Amrvis::YDIR, ivLowOffsetMAL[Amrvis::YDIR]);
#endif
    sdir = Amrvis::ZDIR;
    break;
  case Amrvis::YPLANE:
    trueRegion.setSmall(Amrvis::ZDIR, trueRegion.smallEnd(Amrvis::YDIR)); 
    trueRegion.setBig(Amrvis::ZDIR, trueRegion.bigEnd(Amrvis::YDIR)); 
    trueRegion.setSmall(Amrvis::YDIR, amrPicturePtrArray[Amrvis::YPLANE]->GetSlice()); 
    trueRegion.setBig(Amrvis::YDIR, amrPicturePtrArray[Amrvis::YPLANE]->GetSlice()); 
    trueRegion.shift(Amrvis::XDIR, ivLowOffsetMAL[Amrvis::XDIR]);
    trueRegion.shift(Amrvis::ZDIR, ivLowOffsetMAL[Amrvis::ZDIR]);
    sdir = Amrvis::YDIR;
    break;
  case Amrvis::XPLANE:
    trueRegion.setSmall(Amrvis::ZDIR, trueRegion.smallEnd(Amrvis::YDIR)); 
    trueRegion.setBig(Amrvis::ZDIR, trueRegion.bigEnd(Amrvis::YDIR)); 
    trueRegion.setSmall(Amrvis::YDIR, trueRegion.smallEnd(Amrvis::XDIR)); 
    trueRegion.setBig(Amrvis::YDIR, trueRegion.bigEnd(Amrvis::XDIR)); 
    trueRegion.setSmall(Amrvis::XDIR, amrPicturePtrArray[Amrvis::XPLANE]->GetSlice()); 
    trueRegion.setBig(Amrvis::XDIR, amrPicturePtrArray[Amrvis::XPLANE]->GetSlice()); 
    trueRegion.shift(Amrvis::YDIR, ivLowOffsetMAL[Amrvis::YDIR]);
    trueRegion.shift(Amrvis::ZDIR, ivLowOffsetMAL[Amrvis::ZDIR]);
    sdir = Amrvis::XDIR;
  }
  
  if(datasetShowing) {
    datasetPtr->DoRaise();
    datasetPtr->DatasetRender(trueRegion, amrPicturePtrArray[activeView], this,
		              pltAppState, hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  } else {
    datasetShowing = true;
    datasetPtr = new Dataset(trueRegion, amrPicturePtrArray[activeView], this,
			     pltAppState, hdir, vdir, sdir);
  }
}  // end DoDatasetButton


// -------------------------------------------------------------------
void PltApp::QuitDataset() {
  datasetShowing = false;
  delete datasetPtr;
}


// -------------------------------------------------------------------
void PltApp::DoPaletteButton(Widget, XtPointer, XtPointer) {
  static Widget wPalDialog;
  wPalDialog = XmCreateFileSelectionDialog(wAmrVisTopLevel,
				const_cast<char *>("Choose Palette"),
				NULL, 0);

  AddStaticCallback(wPalDialog, XmNokCallback, &PltApp::DoOpenPalFile);
  XtAddCallback(wPalDialog, XmNcancelCallback,
		(XtCallbackProc) XtUnmanageChild, (XtPointer) this);
  XtManageChild(wPalDialog);
  XtPopup(XtParent(wPalDialog), XtGrabExclusive);
}


// -------------------------------------------------------------------
void PltApp::DoCallTraceButton(Widget, XtPointer, XtPointer) {
  if(callTraceShowing) {
    XtPopup(wCallTraceTopLevel, XtGrabNone);
    XMapRaised(XtDisplay(wCallTraceTopLevel), XtWindow(wCallTraceTopLevel));
    return;
  }

  callTraceShowing = true;
  Dimension width, height;
  Position  xpos, ypos;
  XtVaGetValues(wAmrVisTopLevel, XmNx, &xpos, XmNy, &ypos,
		XmNwidth, &width, XmNheight, &height, NULL);
  
  wCallTraceTopLevel = 
    XtVaCreatePopupShell("Call Trace",
			 topLevelShellWidgetClass, wAmrVisTopLevel,
			 XmNwidth,	400,
			 XmNheight,	300,
			 XmNx,		50+xpos+width/2,
			 XmNy,		ypos-10,
			 NULL);
  
  AddStaticCallback(wCallTraceTopLevel, XmNdestroyCallback, &PltApp::DestroyCallTraceWindow);
  
  // set visual in case the default isn't 256 pseudocolor
  if(gaPtr->PVisual() != XDefaultVisual(display, gaPtr->PScreenNumber())) {
    XtVaSetValues(wCallTraceTopLevel, XmNvisual, gaPtr->PVisual(), XmNdepth, 8, NULL);
  }
  
  Widget wCallTraceForm =
    XtVaCreateManagedWidget("CallTraceform", xmFormWidgetClass, wCallTraceTopLevel, NULL);
  

  Widget wCallTraceCloseButton =
    XtVaCreateManagedWidget("Close",
			    xmPushButtonGadgetClass, wCallTraceForm,
			    XmNbottomAttachment, XmATTACH_FORM,
			    XmNleftAttachment, XmATTACH_FORM,
			    NULL);
  int i(0);
  XtSetArg(args[i], XmNeditable, false);       ++i;
  XtSetArg(args[i], XmNeditMode, XmMULTI_LINE_EDIT);       ++i;
  XtSetArg(args[i], XmNwordWrap, true);       ++i;
  XtSetArg(args[i], XmNblinkRate, 0);       ++i;
  XtSetArg(args[i], XmNautoShowCursorPosition, true);       ++i;
  XtSetArg(args[i], XmNcursorPositionVisible,  false);      ++i;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);      ++i;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);      ++i;
  XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);      ++i;
  XtSetArg(args[i], XmNbottomAttachment, XmATTACH_WIDGET);      ++i;
  XtSetArg(args[i], XmNbottomWidget, wCallTraceCloseButton);      ++i;

  wCallTraceList = XmCreateScrolledText(wCallTraceForm,
                                        const_cast<char *>("CallTracescrolledlist"),
					args, i);
  AddStaticCallback(wCallTraceCloseButton, XmNactivateCallback,
		    &PltApp::CloseCallTraceWindow);
  
  XtManageChild(wCallTraceList);
  XtManageChild(wCallTraceCloseButton);

  XtPopup(wCallTraceTopLevel, XtGrabNone);

  pltAppMessageText.Init(wCallTraceList);
  AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  
  std::ostringstream prob;
  prob.precision(16);

  string ctFileName(amrData.GetFileName() + "/CallTrace.txt");
  std::ifstream cTrace(ctFileName.c_str());
  if(cTrace.fail()) {
    cout << "**** Error:  could not open:  " << ctFileName << endl;
  } else {
    char s[Amrvis::BUFSIZE];
    while( ! cTrace.getline(s, Amrvis::BUFSIZE).eof()) {
      prob << s << '\n';
    }
    cTrace.close();
  }
  prob << '\n';

  bool scrollToTop(true);
  pltAppMessageText.PrintText(prob.str().c_str(), scrollToTop);
}


// -------------------------------------------------------------------
void PltApp::DestroyCallTraceWindow(Widget, XtPointer /*xp*/, XtPointer) {
  callTraceShowing = false;
}


// -------------------------------------------------------------------
void PltApp::CloseCallTraceWindow(Widget, XtPointer, XtPointer) {
  XtPopdown(wCallTraceTopLevel);
  callTraceShowing = false;
}


// -------------------------------------------------------------------
void PltApp::DestroyInfoWindow(Widget, XtPointer /*xp*/, XtPointer) {
  infoShowing = false;
}


// -------------------------------------------------------------------
void PltApp::CloseInfoWindow(Widget, XtPointer, XtPointer) {
  XtPopdown(wInfoTopLevel);
  infoShowing = false;
}


// -------------------------------------------------------------------
void PltApp::DoInfoButton(Widget, XtPointer, XtPointer) {
  if(infoShowing) {
    XtPopup(wInfoTopLevel, XtGrabNone);
    XMapRaised(XtDisplay(wInfoTopLevel), XtWindow(wInfoTopLevel));
    return;
  }

  infoShowing = true;
  Dimension width, height;
  Position  xpos, ypos;
  XtVaGetValues(wAmrVisTopLevel, XmNx, &xpos, XmNy, &ypos,
		XmNwidth, &width, XmNheight, &height, NULL);
  
  wInfoTopLevel = 
    XtVaCreatePopupShell("Info",
			 topLevelShellWidgetClass, wAmrVisTopLevel,
			 XmNwidth,	400,
			 XmNheight,	300,
			 XmNx,		50+xpos+width/2,
			 XmNy,		ypos-10,
			 NULL);
  
  AddStaticCallback(wInfoTopLevel, XmNdestroyCallback, &PltApp::DestroyInfoWindow);
  
  // set visual in case the default isn't 256 pseudocolor
  if(gaPtr->PVisual() != XDefaultVisual(display, gaPtr->PScreenNumber())) {
    XtVaSetValues(wInfoTopLevel, XmNvisual, gaPtr->PVisual(), XmNdepth, 8, NULL);
  }
  
  Widget wInfoForm =
    XtVaCreateManagedWidget("infoform", xmFormWidgetClass, wInfoTopLevel, NULL);
  
  int i(0);
  XtSetArg(args[i], XmNeditable, false);       ++i;
  XtSetArg(args[i], XmNeditMode, XmMULTI_LINE_EDIT);       ++i;
  XtSetArg(args[i], XmNwordWrap, true);       ++i;
  XtSetArg(args[i], XmNblinkRate, 0);       ++i;
  XtSetArg(args[i], XmNautoShowCursorPosition, true);       ++i;
  XtSetArg(args[i], XmNcursorPositionVisible,  false);      ++i;
  XtSetArg(args[i], XmNtopAttachment, XmATTACH_FORM);      ++i;
  XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);      ++i;
  XtSetArg(args[i], XmNrightAttachment, XmATTACH_FORM);      ++i;
  XtSetArg(args[i], XmNbottomAttachment, XmATTACH_POSITION);      ++i;
  XtSetArg(args[i], XmNbottomPosition, 80);      ++i;

  Widget wInfoList = XmCreateScrolledText(wInfoForm,
                                          const_cast<char *>("infoscrolledlist"),
					  args, i);

  Widget wInfoCloseButton =
    XtVaCreateManagedWidget("Close",
			    xmPushButtonGadgetClass, wInfoForm,
			    XmNtopAttachment, XmATTACH_POSITION,
			    XmNtopPosition, 85,
			    XmNbottomAttachment, XmATTACH_POSITION,
			    XmNbottomPosition, 95,
			    XmNrightAttachment, XmATTACH_POSITION,
			    XmNrightPosition, 75,
			    XmNleftAttachment, XmATTACH_POSITION,
			    XmNleftPosition, 25,
			    NULL);
  AddStaticCallback(wInfoCloseButton, XmNactivateCallback,
		    &PltApp::CloseInfoWindow);
  
  XtManageChild(wInfoList);
  XtManageChild(wInfoCloseButton);

  XtPopup(wInfoTopLevel, XtGrabNone);

  pltAppMessageText.Init(wInfoList);
  AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  
  std::ostringstream prob;
  prob.precision(15);

  prob << fileNames[currentFrame] << '\n';
  prob << amrData.PlotFileVersion().c_str() << '\n';
  prob << "time:  "<< amrData.Time() << '\n';
  prob << "levels:  " << amrData.FinestLevel() + 1 << '\n';
  prob << "prob domain:" << '\n';
  for(int k(0); k <= amrData.FinestLevel(); ++k) {
    prob << "  level " << k << ":  " << amrData.ProbDomain()[k] << '\n';
  }
  prob << "refratios: ";
  for(int k(0); k < amrData.FinestLevel(); ++k) {
    prob << " " << amrData.RefRatio()[k];
  }
  prob << '\n';
  prob << "probsize:  ";
  for(int k(0); k < BL_SPACEDIM; ++k) {
    prob << " " << amrData.ProbSize()[k];
  }
  prob << '\n';
  prob << "prob lo:   ";
  for(int k(0); k < BL_SPACEDIM; ++k) {
    prob << " " << amrData.ProbLo()[k];
  }
  prob << '\n';
  prob << "prob hi:   ";
  for(int k(0); k < BL_SPACEDIM; ++k) {
    prob << " " << amrData.ProbHi()[k];
  }
  prob << '\n';

  pltAppMessageText.PrintText(prob.str().c_str());
}


// -------------------------------------------------------------------
void PltApp::DoContoursButton(Widget, XtPointer, XtPointer) {
  Position xpos, ypos;
  Dimension width, height;

  if(contoursShowing) {
    XtPopup(wContoursTopLevel, XtGrabNone);
    XMapRaised(XtDisplay(wContoursTopLevel), XtWindow(wContoursTopLevel));
    return;
  }

  Widget wContoursForm, wCloseButton, wContourRadio, wid;
  
  XtVaGetValues(wAmrVisTopLevel, XmNx, &xpos, XmNy, &ypos,
		XmNwidth, &width, XmNheight, &height, NULL);
  
  wContoursTopLevel = XtVaCreatePopupShell("Set Contours",
			 topLevelShellWidgetClass, wAmrVisTopLevel,
			 XmNwidth,		   170,
			 XmNheight,		   220,
			 XmNx,			   xpos+width/2,
			 XmNy,			   ypos+height/2,
			 NULL);
  
  AddStaticCallback(wContoursTopLevel, XmNdestroyCallback,
		    &PltApp::DestroyContoursWindow);
  
  //set visual in case the default isn't 256 pseudocolor
  if(gaPtr->PVisual() != XDefaultVisual(display, gaPtr->PScreenNumber())) {
    XtVaSetValues(wContoursTopLevel, XmNvisual, gaPtr->PVisual(), XmNdepth, 8,NULL);
  }
        
    
  wContoursForm = XtVaCreateManagedWidget("Contoursform",
			    xmFormWidgetClass, wContoursTopLevel,
			    NULL);

  wContourRadio = XmCreateRadioBox(wContoursForm,
                                   const_cast<char *>("contourtype"), NULL, 0);
  XtVaSetValues(wContourRadio,
		XmNtopAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM, NULL);		
  
  const char *conItems[Amrvis::NCONTOPTIONS] = {"Raster", "Raster & Contours", "Color Contours",
		                  "B/W Contours", "Velocity Vectors"};
  for(int j(0); j < Amrvis::NCONTOPTIONS; ++j) {
    wid = XtVaCreateManagedWidget(conItems[j], xmToggleButtonGadgetClass,
				  wContourRadio, NULL);
    if(j == (int) pltAppState->GetContourType()) {
      XtVaSetValues(wid, XmNset, true, NULL);
    }
    AddStaticCallback(wid, XmNvalueChangedCallback, &PltApp::ChangeContour,
		      (XtPointer) static_cast<long>(j));
  }

  XmString label_str = XmStringCreateSimple(const_cast<char *>("Number of lines:"));
  wContourLabel = XtVaCreateManagedWidget("numcontours",
			    xmLabelWidgetClass, wContoursForm,
			    XmNlabelString,     label_str, 
			    XmNleftAttachment,  XmATTACH_FORM,
			    XmNleftOffset,      Amrvis::WOFFSET,
			    XmNtopAttachment,   XmATTACH_WIDGET,
			    XmNtopWidget,       wContourRadio,
			    XmNtopOffset,       Amrvis::WOFFSET,
			    XmNsensitive,
			          pltAppState->GetContourType() != Amrvis::RASTERONLY,
			    NULL);
  XmStringFree(label_str);

  wNumberContours = XtVaCreateManagedWidget("contours",
			    xmTextFieldWidgetClass, wContoursForm,
			    XmNtopAttachment,    XmATTACH_WIDGET,
			    XmNtopWidget,        wContourRadio,
			    XmNtopOffset,        Amrvis::WOFFSET,
			    XmNleftAttachment,   XmATTACH_WIDGET,
			    XmNleftWidget,       wContourLabel,
			    XmNleftOffset,       Amrvis::WOFFSET,
			    XmNvalue,            contourNumString.c_str(),
			    XmNcolumns,          4,
			    XmNsensitive,
				  pltAppState->GetContourType() != Amrvis::RASTERONLY,
			    NULL);
  AddStaticCallback(wNumberContours, XmNactivateCallback,
		    &PltApp::ReadContourString);
  
  wCloseButton = XtVaCreateManagedWidget(" Close ",
			    xmPushButtonGadgetClass, wContoursForm,
			    XmNbottomAttachment, XmATTACH_FORM,
			    XmNbottomOffset, Amrvis::WOFFSET,
			    XmNleftAttachment, XmATTACH_POSITION,
			    XmNleftPosition, 33,
			    XmNrightAttachment, XmATTACH_POSITION,
			    XmNrightPosition, 66,
			    NULL);
  AddStaticCallback(wCloseButton, XmNactivateCallback,
		    &PltApp::CloseContoursWindow);
  
  XtManageChild(wContourRadio);
  XtManageChild(wCloseButton);
  XtPopup(wContoursTopLevel, XtGrabNone);
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wContoursTopLevel));
  contoursShowing = true;
}


// -------------------------------------------------------------------
void PltApp::DestroyContoursWindow(Widget, XtPointer, XtPointer) {
  contoursShowing = false;
}


// -------------------------------------------------------------------
void PltApp::CloseContoursWindow(Widget, XtPointer, XtPointer) {
  XtPopdown(wContoursTopLevel);
}


// -------------------------------------------------------------------
void PltApp::DoToggleFileRangeButton(Widget /*w*/, XtPointer /*client_data*/,
				     XtPointer /*call_data*/)
{
  bFileRangeButtonSet = XmToggleButtonGetState(wFileRangeCheckBox);
  if(bFileRangeButtonSet) {
    if(currentRangeType == Amrvis::GLOBALMINMAX) {
      currentRangeType = Amrvis::FILEGLOBALMINMAX;
    } else if(currentRangeType == Amrvis::SUBREGIONMINMAX) {
      currentRangeType = Amrvis::FILESUBREGIONMINMAX;
    } else if(currentRangeType == Amrvis::USERMINMAX) {
      currentRangeType = Amrvis::FILEUSERMINMAX;
    }
  } else {
    if(currentRangeType == Amrvis::FILEGLOBALMINMAX) {
      currentRangeType = Amrvis::GLOBALMINMAX;
    } else if(currentRangeType == Amrvis::FILESUBREGIONMINMAX) {
      currentRangeType = Amrvis::SUBREGIONMINMAX;
    } else if(currentRangeType == Amrvis::FILEUSERMINMAX) {
      currentRangeType = Amrvis::USERMINMAX;
    }
  }
}


// -------------------------------------------------------------------
void PltApp::DoSetRangeButton(Widget, XtPointer, XtPointer) {
  Position xpos, ypos;
  Dimension width, height;

  if(setRangeShowing) {
    XtPopup(wSetRangeTopLevel, XtGrabNone);
    XMapRaised(XtDisplay(wSetRangeTopLevel), XtWindow(wSetRangeTopLevel));
    return;
  }

  char range[Amrvis::LINELENGTH];
  char saveRangeString[Amrvis::LINELENGTH];
  char format[Amrvis::LINELENGTH];
  char fMin[Amrvis::LINELENGTH];
  char fMax[Amrvis::LINELENGTH];
  strcpy(format, pltAppState->GetFormatString().c_str());
  Widget wSetRangeForm, wDoneButton, wApplyButton, wCancelButton;
  Widget wSetRangeRadioBox, wRangeRC, wid;

  
  // do these here to set XmNwidth of wSetRangeTopLevel
  Real rtMin, rtMax;
  pltAppState->GetMinMax(Amrvis::GLOBALMINMAX, currentFrame,
			 pltAppState->CurrentDerivedNumber(),
			 rtMin, rtMax);
  sprintf(fMin, format, rtMin);
  sprintf(fMax, format, rtMax);
  sprintf(range, "Min: %s  Max: %s", fMin, fMax);
  strcpy(saveRangeString, range);
  
  XtVaGetValues(wAmrVisTopLevel,
		XmNx, &xpos,
		XmNy, &ypos,
		XmNwidth, &width,
		XmNheight, &height,
		NULL);
  
  wSetRangeTopLevel = XtVaCreatePopupShell("Set Range",
			 topLevelShellWidgetClass, wAmrVisTopLevel,
			 XmNwidth,		250,
			 XmNheight,		200,
			 XmNx,			xpos+width/2,
			 XmNy,			ypos,
			 NULL);
  
  AddStaticCallback(wSetRangeTopLevel, XmNdestroyCallback,
		    &PltApp::DestroySetRangeWindow);
  
  //set visual in case the default isn't 256 pseudocolor
  if(gaPtr->PVisual() != XDefaultVisual(display, gaPtr->PScreenNumber())) {
    XtVaSetValues(wSetRangeTopLevel, XmNvisual, gaPtr->PVisual(),
		  XmNdepth, 8, NULL);
  }
        
    
  wSetRangeForm = XtVaCreateManagedWidget("setrangeform",
					  xmFormWidgetClass, wSetRangeTopLevel,
					  NULL);
  
  // make the buttons
  wDoneButton = XtVaCreateManagedWidget(" Ok ",
					xmPushButtonGadgetClass, wSetRangeForm,
					XmNbottomAttachment, XmATTACH_FORM,
					XmNbottomOffset, Amrvis::WOFFSET,
					XmNleftAttachment, XmATTACH_FORM,
					XmNleftOffset, Amrvis::WOFFSET,
					NULL);
  bool bKillSRWindow(true);
  AddStaticCallback(wDoneButton, XmNactivateCallback, &PltApp::DoDoneSetRange,
		    (XtPointer) bKillSRWindow);
  
  wApplyButton = XtVaCreateManagedWidget(" Apply ",
					  xmPushButtonGadgetClass, wSetRangeForm,
					  XmNbottomAttachment, XmATTACH_FORM,
					  XmNbottomOffset, Amrvis::WOFFSET,
					  XmNleftAttachment, XmATTACH_WIDGET,
					  XmNleftWidget, wDoneButton,
					  XmNleftOffset, Amrvis::WOFFSET,
					  NULL);
  bKillSRWindow = false;
  AddStaticCallback(wApplyButton, XmNactivateCallback, &PltApp::DoDoneSetRange,
		    (XtPointer) bKillSRWindow);
  
  wCancelButton = XtVaCreateManagedWidget(" Cancel ",
					  xmPushButtonGadgetClass, wSetRangeForm,
					  XmNbottomAttachment, XmATTACH_FORM,
					  XmNbottomOffset, Amrvis::WOFFSET,
					  XmNrightAttachment, XmATTACH_FORM,
					  XmNrightOffset, Amrvis::WOFFSET,
					  NULL);
  AddStaticCallback(wCancelButton, XmNactivateCallback, &PltApp::DoCancelSetRange);
  
  // make the radio box
  wSetRangeRadioBox = XmCreateRadioBox(wSetRangeForm,
                                       const_cast<char *>("setrangeradiobox"),
				       NULL, 0);
  XtVaSetValues(wSetRangeRadioBox, XmNmarginHeight, 8, NULL);
  XtVaSetValues(wSetRangeRadioBox, XmNmarginWidth, 4, NULL);
  XtVaSetValues(wSetRangeRadioBox, XmNspacing, 14, NULL);
  
  wRangeRadioButton[Amrvis::BGLOBALMINMAX] = XtVaCreateManagedWidget("Global",
			    xmToggleButtonWidgetClass, wSetRangeRadioBox, NULL);
  AddStaticCallback(wRangeRadioButton[Amrvis::BGLOBALMINMAX], XmNvalueChangedCallback,
		    &PltApp::ToggleRange, (XtPointer) Amrvis::BGLOBALMINMAX);
  wRangeRadioButton[Amrvis::BSUBREGIONMINMAX] = XtVaCreateManagedWidget("Local",
			    xmToggleButtonWidgetClass, wSetRangeRadioBox,
			    NULL);
  AddStaticCallback(wRangeRadioButton[Amrvis::BSUBREGIONMINMAX], XmNvalueChangedCallback,
		    &PltApp::ToggleRange, (XtPointer) Amrvis::BSUBREGIONMINMAX);
  wRangeRadioButton[Amrvis::BUSERMINMAX] = XtVaCreateManagedWidget("User",
			    xmToggleButtonWidgetClass, wSetRangeRadioBox, NULL);
  AddStaticCallback(wRangeRadioButton[Amrvis::BUSERMINMAX], XmNvalueChangedCallback,
		    &PltApp::ToggleRange, (XtPointer) Amrvis::BUSERMINMAX);
  if(animating2d) {
    int i(0);
    XtSetArg(args[i], XmNleftAttachment, XmATTACH_FORM);  ++i;
    XtSetArg(args[i], XmNleftOffset, Amrvis::WOFFSET);            ++i;
    XtSetArg(args[i], XmNtopAttachment, XmATTACH_WIDGET); ++i;
    XtSetArg(args[i], XmNtopWidget, wSetRangeRadioBox); ++i;
    XtSetArg(args[i], XmNtopOffset, Amrvis::WOFFSET); ++i;
    XtSetArg(args[i], XmNset, bFileRangeButtonSet); ++i;
    wFileRangeCheckBox = XmCreateToggleButton(wSetRangeForm,
                              const_cast<char *>("File_Range"),
			      args, i);
    AddStaticCallback(wFileRangeCheckBox, XmNvalueChangedCallback,
		      &PltApp::DoToggleFileRangeButton, NULL);
  }
    
  currentRangeType = pltAppState->GetMinMaxRangeType();
  Amrvis::MinMaxRangeTypeForButtons mmButton(Amrvis::BINVALIDMINMAX);
  if(currentRangeType == Amrvis::GLOBALMINMAX || currentRangeType == Amrvis::FILEGLOBALMINMAX) {
    mmButton = Amrvis::BGLOBALMINMAX;
  } else if(currentRangeType == Amrvis::SUBREGIONMINMAX ||
	    currentRangeType == Amrvis::FILESUBREGIONMINMAX)
  {
    mmButton = Amrvis::BSUBREGIONMINMAX;
  } else if(currentRangeType == Amrvis::USERMINMAX || currentRangeType == Amrvis::FILEUSERMINMAX) {
    mmButton = Amrvis::BUSERMINMAX;
  } else {
    amrex::Abort("Bad button range type.");
  }
  XtVaSetValues(wRangeRadioButton[mmButton], XmNset, true, NULL);
  
  wRangeRC = XtVaCreateManagedWidget("rangeRC", xmRowColumnWidgetClass,
			    wSetRangeForm,
			    XmNtopAttachment,   XmATTACH_FORM,
			    XmNtopOffset,       0,
			    XmNleftAttachment,  XmATTACH_WIDGET,
			    XmNleftWidget,      wSetRangeRadioBox,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNpacking,         XmPACK_COLUMN,
			    XmNnumColumns,      3,
			    XmNorientation,     XmHORIZONTAL,
			    NULL);
  // make the strings representing data min and maxes
  pltAppState->GetMinMax(Amrvis::GLOBALMINMAX, currentFrame,
			 pltAppState->CurrentDerivedNumber(), rtMin, rtMax);
  sprintf(fMin, format, rtMin);
  sprintf(fMax, format, rtMax);
  sprintf(range, "Min: %s", fMin);
  XtVaCreateManagedWidget(range, xmLabelGadgetClass, wRangeRC, NULL);
  //XtVaSetValues(wid, XmNleftOffset, 20, NULL);
  sprintf(range, "Max: %s", fMax);
  XtVaCreateManagedWidget(range, xmLabelGadgetClass, wRangeRC, NULL);

  pltAppState->GetMinMax(Amrvis::SUBREGIONMINMAX, currentFrame,
			 pltAppState->CurrentDerivedNumber(),
			 rtMin, rtMax);
  sprintf(fMin, format, rtMin);
  sprintf(fMax, format, rtMax);
  sprintf(range, "Min: %s", fMin);
  XtVaCreateManagedWidget(range, xmLabelGadgetClass, wRangeRC, NULL);
  sprintf(range, "Max: %s", fMax);
  XtVaCreateManagedWidget(range, xmLabelGadgetClass, wRangeRC, NULL);

  pltAppState->GetMinMax(Amrvis::USERMINMAX, currentFrame,
			 pltAppState->CurrentDerivedNumber(),
			 rtMin, rtMax);
  wid = XtVaCreateManagedWidget("wid", xmRowColumnWidgetClass, wRangeRC,
			    XmNorientation, XmHORIZONTAL,
			    XmNleftOffset,       0,
			    //XmNborderWidth,      0,
			    NULL);
  XtVaCreateManagedWidget("Min:", xmLabelGadgetClass, wid, NULL);
  //XtVaSetValues(wid, XmNmarginWidth, 0, NULL);
  sprintf(range, format, rtMin);
  wUserMin = XtVaCreateManagedWidget("local range",
			    xmTextFieldWidgetClass, wid,
			    XmNvalue,		range,
			    XmNeditable,	true,
			    XmNcolumns,		strlen(range)+2,
			    NULL);
  AddStaticCallback(wUserMin, XmNactivateCallback, &PltApp::DoUserMin);
  
  wid = XtVaCreateManagedWidget("wid", xmRowColumnWidgetClass, wRangeRC,
			    XmNorientation, XmHORIZONTAL,
 			    XmNleftOffset,       0,
			    //XmNborderWidth,      0,
			    NULL);
  XtVaCreateManagedWidget("Max:", xmLabelGadgetClass, wid, NULL);
  sprintf(range, format, rtMax);
  wUserMax = XtVaCreateManagedWidget("local range",
			    xmTextFieldWidgetClass, wid,
			    XmNvalue,		range,
			    XmNeditable,	true,
			    XmNcolumns,		strlen(range)+2,
			    NULL);
  AddStaticCallback(wUserMax, XmNactivateCallback, &PltApp::DoUserMax);
  
  XtManageChild(wSetRangeRadioBox);
  XtManageChild(wRangeRC);
  XtManageChild(wDoneButton);
  XtManageChild(wApplyButton);
  XtManageChild(wCancelButton);
  if(animating2d) {
    XtManageChild(wFileRangeCheckBox);
  }
  XtPopup(wSetRangeTopLevel, XtGrabNone);
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wSetRangeTopLevel));
  setRangeShowing = true;

  int isrw, iwidw, itotalwidth, maxwidth(600);
  XtVaGetValues(wSetRangeRadioBox, XmNwidth, &isrw, NULL);
  XtVaGetValues(wid, XmNwidth, &iwidw, NULL);
  itotalwidth = min(isrw + (2 * iwidw) + 10, maxwidth);
  XtVaSetValues(wSetRangeTopLevel, XmNwidth, itotalwidth, NULL);
}


// -------------------------------------------------------------------
void PltApp::SetNewFormatString(const string &newformatstring) {
  pltAppState->SetFormatString(newformatstring);
  pltPaletteptr->SetFormat(newformatstring);
  pltPaletteptr->RedrawPalette();
  if(datasetShowing) {
    int hdir(-1), vdir(-1), sdir(-1);
    if(activeView == Amrvis::ZPLANE) { hdir = Amrvis::XDIR; vdir = Amrvis::YDIR; sdir = Amrvis::ZDIR; }
    if(activeView == Amrvis::YPLANE) { hdir = Amrvis::XDIR; vdir = Amrvis::ZDIR; sdir = Amrvis::YDIR; }
    if(activeView == Amrvis::XPLANE) { hdir = Amrvis::YDIR; vdir = Amrvis::ZDIR; sdir = Amrvis::XDIR; }
    datasetPtr->DatasetRender(trueRegion, amrPicturePtrArray[activeView],
		              this, pltAppState, hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  }
  if(setRangeShowing) {
    bSetRangeRedraw = true;
    XtDestroyWidget(wSetRangeTopLevel);
    setRangeShowing = false;
    DoSetRangeButton(NULL, NULL, NULL);
    setRangeShowing = true;
  }
}


// -------------------------------------------------------------------
void PltApp::DoNumberFormatButton(Widget, XtPointer, XtPointer) {
  Position xpos, ypos;
  Dimension width, height;

  if(bFormatShowing) {
    XtPopup(wNumberFormatTopLevel, XtGrabNone);
    XMapRaised(XtDisplay(wNumberFormatTopLevel), XtWindow(wNumberFormatTopLevel));
    return;
  }

  char format[Amrvis::LINELENGTH];
  strcpy(format, pltAppState->GetFormatString().c_str());
  Widget wNumberFormatForm, wDoneButton, wApplyButton, wCancelButton, wid;

  
  XtVaGetValues(wAmrVisTopLevel,
		XmNx, &xpos,
		XmNy, &ypos,
		XmNwidth, &width,
		XmNheight, &height,
		NULL);
  
  wNumberFormatTopLevel = XtVaCreatePopupShell("Number Format",
			 topLevelShellWidgetClass, wAmrVisTopLevel,
			 XmNwidth,		200,
			 XmNheight,		100,
			 XmNx,			xpos+width/2,
			 XmNy,			ypos,
			 NULL);
  
  AddStaticCallback(wNumberFormatTopLevel, XmNdestroyCallback,
		    &PltApp::DestroyNumberFormatWindow);
  
  //set visual in case the default isn't 256 pseudocolor
  if(gaPtr->PVisual() != XDefaultVisual(display, gaPtr->PScreenNumber())) {
    XtVaSetValues(wNumberFormatTopLevel, XmNvisual, gaPtr->PVisual(),
		  XmNdepth, 8, NULL);
  }
        
    
  wNumberFormatForm = XtVaCreateManagedWidget("setrangeform",
				  xmFormWidgetClass, wNumberFormatTopLevel,
				  NULL);
  
  // make the buttons
  wDoneButton = XtVaCreateManagedWidget(" Ok ",
					xmPushButtonGadgetClass, wNumberFormatForm,
					XmNbottomAttachment, XmATTACH_FORM,
					XmNbottomOffset, Amrvis::WOFFSET,
					XmNleftAttachment, XmATTACH_FORM,
					XmNleftOffset, Amrvis::WOFFSET,
					NULL);
  bool bKillNFWindow(true);
  AddStaticCallback(wDoneButton, XmNactivateCallback, &PltApp::DoDoneNumberFormat,
		    (XtPointer) bKillNFWindow);
  
  wApplyButton = XtVaCreateManagedWidget(" Apply ",
				  xmPushButtonGadgetClass, wNumberFormatForm,
				  XmNbottomAttachment, XmATTACH_FORM,
				  XmNbottomOffset, Amrvis::WOFFSET,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, wDoneButton,
				  XmNleftOffset, Amrvis::WOFFSET,
				  NULL);
  bKillNFWindow = false;
  AddStaticCallback(wApplyButton, XmNactivateCallback, &PltApp::DoDoneNumberFormat,
		    (XtPointer) bKillNFWindow);
  
  wCancelButton = XtVaCreateManagedWidget(" Cancel ",
				  xmPushButtonGadgetClass, wNumberFormatForm,
				  XmNbottomAttachment, XmATTACH_FORM,
				  XmNbottomOffset, Amrvis::WOFFSET,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNrightOffset, Amrvis::WOFFSET,
				  NULL);
  AddStaticCallback(wCancelButton, XmNactivateCallback,
		    &PltApp::DoCancelNumberFormat);
  
  wid = XtVaCreateManagedWidget("Format:",
			    xmLabelGadgetClass, wNumberFormatForm,
			    XmNtopAttachment, XmATTACH_FORM,
			    XmNtopOffset, Amrvis::WOFFSET,
			    XmNleftAttachment, XmATTACH_FORM,
			    XmNleftOffset, Amrvis::WOFFSET,
			    NULL);
  wFormat = XtVaCreateManagedWidget("format",
			    xmTextFieldWidgetClass, wNumberFormatForm,
			    XmNvalue,		format,
			    XmNeditable,	true,
			    XmNcolumns,		strlen(format) + 4,
			    XmNtopAttachment, XmATTACH_FORM,
			    XmNtopOffset, Amrvis::WOFFSET,
			    XmNleftAttachment, XmATTACH_WIDGET,
			    XmNleftWidget, wid,
			    XmNleftOffset, Amrvis::WOFFSET,
			    NULL);
  bKillNFWindow = true;
  AddStaticCallback(wFormat, XmNactivateCallback, &PltApp::DoDoneNumberFormat,
		    (XtPointer) bKillNFWindow);
  
  XtManageChild(wDoneButton);
  XtManageChild(wApplyButton);
  XtManageChild(wCancelButton);

  XtPopup(wNumberFormatTopLevel, XtGrabNone);
  pltPaletteptr->SetWindowPalette(palFilename, XtWindow(wNumberFormatTopLevel));
  bFormatShowing = true;
}


// -------------------------------------------------------------------
void PltApp::DoDoneNumberFormat(Widget, XtPointer client_data, XtPointer) {
  bool bKillNFWindow = (bool) client_data;
  paletteDrawn = false;

  string tempFormatString;

  char temp[64];
  strcpy(temp, XmTextFieldGetString(wFormat));
  if(temp[0] != '%') {
    tempFormatString  = "%";
    tempFormatString += temp;
  } else {
    tempFormatString = temp;
  }
  // unexhaustive string check to prevent errors
  bool stringOk(true);
  for(int i(0); i < tempFormatString.length(); ++i) {
    if(tempFormatString[i] == 's' || tempFormatString[i] == 'u'
        || tempFormatString[i] == 'p')
    {
      stringOk = false;
    }
  }
  if(stringOk) {
    SetNewFormatString(tempFormatString);
  } else {
    cerr << "Error in format string." << endl;
  }

  if(bKillNFWindow) {
    XtDestroyWidget(wNumberFormatTopLevel);
  }

}


// -------------------------------------------------------------------
void PltApp::DoCancelNumberFormat(Widget, XtPointer, XtPointer) {
  XtDestroyWidget(wNumberFormatTopLevel);
}


// -------------------------------------------------------------------
void PltApp::DestroyNumberFormatWindow(Widget, XtPointer, XtPointer) {
  bFormatShowing = false;
}


// -------------------------------------------------------------------
void PltApp::DoDoneSetRange(Widget, XtPointer client_data, XtPointer) {
  bool bKillSRWindow = (bool) client_data;
  paletteDrawn = false;

  int np;
  Real umin(atof(XmTextFieldGetString(wUserMin)));
  Real umax(atof(XmTextFieldGetString(wUserMax)));

  for(int iFrame(0); iFrame < animFrames; ++iFrame) {
    pltAppState->SetMinMax(Amrvis::USERMINMAX, iFrame,
			   pltAppState->CurrentDerivedNumber(),
			   umin, umax);
  }

  if(currentRangeType != pltAppState->GetMinMaxRangeType() ||
     currentRangeType == Amrvis::USERMINMAX || currentRangeType == Amrvis::FILEUSERMINMAX)
  {
    pltAppState->SetMinMaxRangeType(currentRangeType);	
    for(np = 0; np < Amrvis::NPLANES; ++np) {
      amrPicturePtrArray[np]->APMakeImages(pltPaletteptr);
    }
#if (BL_SPACEDIM == 3)
    //Clear();
#endif
    if(animating2d) {
      ResetAnimation();
      DirtyFrames(); 
      ShowFrame();
    }
#if (BL_SPACEDIM == 3)
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
    projPicturePtr->GetVolRenderPtr()->InvalidateSWFData();
    projPicturePtr->GetVolRenderPtr()->InvalidateVPData();
    XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
    if(XmToggleButtonGetState(wAutoDraw) || showing3dRender) {
      DoRender();
    }
#endif
#endif
  }
  if(bKillSRWindow) {
    XtDestroyWidget(wSetRangeTopLevel);
  }

  if(datasetShowing) {
    datasetPtr->DoRaise();
    int hdir(-1), vdir(-1), sdir(-1);
    if(activeView==Amrvis::ZPLANE) { hdir = Amrvis::XDIR; vdir = Amrvis::YDIR; sdir = Amrvis::ZDIR; }
    if(activeView==Amrvis::YPLANE) { hdir = Amrvis::XDIR; vdir = Amrvis::ZDIR; sdir = Amrvis::YDIR; }
    if(activeView==Amrvis::XPLANE) { hdir = Amrvis::YDIR; vdir = Amrvis::ZDIR; sdir = Amrvis::XDIR; }
    datasetPtr->DatasetRender(trueRegion, amrPicturePtrArray[activeView], this,
                              pltAppState, hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  }
}


// -------------------------------------------------------------------
void PltApp::DoCancelSetRange(Widget, XtPointer, XtPointer) {
  XtDestroyWidget(wSetRangeTopLevel);
  setRangeShowing = false;
}


// -------------------------------------------------------------------
void PltApp::DestroySetRangeWindow(Widget, XtPointer, XtPointer) {
  if(bSetRangeRedraw == false) {
    setRangeShowing = false;
  }
  bSetRangeRedraw = false;
}


// -------------------------------------------------------------------
void PltApp::DoUserMin(Widget, XtPointer, XtPointer) {
  if(currentRangeType == Amrvis::GLOBALMINMAX || currentRangeType == Amrvis::FILEGLOBALMINMAX) {
    XtVaSetValues(wRangeRadioButton[Amrvis::BGLOBALMINMAX], XmNset, false, NULL);
  }
  if(currentRangeType == Amrvis::SUBREGIONMINMAX ||
     currentRangeType == Amrvis::FILESUBREGIONMINMAX)
  {
    XtVaSetValues(wRangeRadioButton[Amrvis::BSUBREGIONMINMAX], XmNset, false, NULL);
  }
  XtVaSetValues(wRangeRadioButton[Amrvis::BUSERMINMAX], XmNset, true, NULL);
  if(bFileRangeButtonSet) {
    currentRangeType = Amrvis::FILEUSERMINMAX;
  } else {
    currentRangeType = Amrvis::USERMINMAX;
  }
  bool bKillSRWindow(true);
  DoDoneSetRange(NULL, (XtPointer) bKillSRWindow, NULL);
}


// -------------------------------------------------------------------
void PltApp::DoUserMax(Widget, XtPointer, XtPointer) {
  if(currentRangeType == Amrvis::GLOBALMINMAX || currentRangeType == Amrvis::FILEGLOBALMINMAX) {
    XtVaSetValues(wRangeRadioButton[Amrvis::BGLOBALMINMAX], XmNset, false, NULL);
  }
  if(currentRangeType == Amrvis::SUBREGIONMINMAX ||
     currentRangeType == Amrvis::FILESUBREGIONMINMAX)
  {
    XtVaSetValues(wRangeRadioButton[Amrvis::BSUBREGIONMINMAX], XmNset, false, NULL);
  }
  XtVaSetValues(wRangeRadioButton[Amrvis::BUSERMINMAX], XmNset, true, NULL);
  if(bFileRangeButtonSet) {
    currentRangeType = Amrvis::FILEUSERMINMAX;
  } else {
    currentRangeType = Amrvis::USERMINMAX;
  }
  bool bKillSRWindow(true);
  DoDoneSetRange(NULL, (XtPointer) bKillSRWindow, NULL);
}


// -------------------------------------------------------------------
void PltApp::DoBoxesButton(Widget, XtPointer, XtPointer) {
  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }
  pltAppState->SetShowingBoxes( ! pltAppState->GetShowingBoxes());
  for(int np(0); np < Amrvis::NPLANES; ++np) {
    amrPicturePtrArray[np]->DoExposePicture();
  }
#if (BL_SPACEDIM == 3)
    projPicturePtr->MakeBoxes(); 
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
    if( ! XmToggleButtonGetState(wAutoDraw)) {
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
      DoExposeTransDA();
    }
#else
      XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
      DoExposeTransDA();
#endif
#endif

  // draw a bounding box around the image
  /*
  int imageSizeX = amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeH();
  int imageSizeY = amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeV();
  XSetForeground(display, xgc, pltPaletteptr->WhiteIndex());
  XDrawLine(display, XtWindow(wPlotPlane[Amrvis::ZPLANE]), xgc,
            0, imageSizeY, imageSizeX, imageSizeY);
  XDrawLine(display, XtWindow(wPlotPlane[Amrvis::ZPLANE]), xgc,
            imageSizeX, 0, imageSizeX, imageSizeY);
  */
}


// -------------------------------------------------------------------
void PltApp::DoCGSmoothing(Widget, XtPointer, XtPointer) {
  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }
  pltAppState->SetCGSmoothing( ! pltAppState->GetCGSmoothing());
  int currentScale(pltAppState->CurrentScale());
  for(int np(0); np < Amrvis::NPLANES; ++np) {
    amrPicturePtrArray[np]->SetCartGridSmoothing(pltAppState->GetCGSmoothing());
    amrPicturePtrArray[np]->APChangeScale(currentScale, currentScale);
  }
}


// -------------------------------------------------------------------
void PltApp::DoCGShowBody(Widget, XtPointer, XtPointer) {
  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }
  AVGlobals::SetShowBody( ! AVGlobals::GetShowBody());
  AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  amrData.SetShowBody(AVGlobals::GetShowBody());
  int currentScale(pltAppState->CurrentScale());
  for(int np(0); np < Amrvis::NPLANES; ++np) {
    amrPicturePtrArray[np]->APChangeScale(currentScale, currentScale);
  }
}


// -------------------------------------------------------------------
void PltApp::DoOpenPalFile(Widget w, XtPointer, XtPointer call_data) {
  char *palfile;
  if( ! XmStringGetLtoR(((XmFileSelectionBoxCallbackStruct *) call_data)->value,
		        XmSTRING_DEFAULT_CHARSET, &palfile))
  {
    cerr << "PltApp::DoOpenPalFile : system error" << endl;
    return;
  }
  XtPopdown(XtParent(w));
  pltPaletteptr->ChangeWindowPalette(palfile, XtWindow(wAmrVisTopLevel));
# ifdef BL_VOLUMERENDER
  projPicturePtr->GetVolRenderPtr()->SetTransferProperties();
  projPicturePtr->GetVolRenderPtr()->InvalidateVPData();
  showing3dRender = false;
  if(XmToggleButtonGetState(wAutoDraw)) {
    DoRender(wAutoDraw, NULL, NULL);
  }
# endif
  palFilename = palfile;
  XtFree(palfile);
  
#if (BL_SPACEDIM == 3)
  projPicturePtr->ResetPalette(pltPaletteptr);
# endif

  XYplotparameters->ResetPalette(pltPaletteptr);
  for(int np(0); np != BL_SPACEDIM; ++np) {
    if(XYplotwin[np]) {
      XYplotwin[np]->SetPalette();
    }
  }
  for(int npp(0); npp < Amrvis::NPLANES; ++npp) {
    amrPicturePtrArray[npp]->CreatePicture(XtWindow(wPlotPlane[npp]),
					  pltPaletteptr);
  }
  if(datasetShowing) {
    datasetPtr->DoExpose(false);
  }
  if(animating2d) {
    ResetAnimation();
    DirtyFrames(); 
  }
}


// -------------------------------------------------------------------
XYPlotDataList *PltApp::CreateLinePlot(int V, int sdir, int mal, int ix,
				       const string *derived)
{
  const AmrData &amrData(dataServicesPtr[currentFrame]->AmrDataRef());
  
  // Create an array of boxes corresponding to the intersected line.
  int tdir(-1), dir1(-1);
#if (BL_SPACEDIM == 3)
  int dir2(-1);
#endif
  switch (V) {
  case Amrvis::ZPLANE:
#if (BL_SPACEDIM == 1)
    tdir = Amrvis::XDIR;
#else
    tdir = (sdir == Amrvis::XDIR) ? Amrvis::YDIR : Amrvis::XDIR;
#endif
    dir1 = tdir;
#if (BL_SPACEDIM == 3)
    dir2 = Amrvis::ZDIR;
    break;
  case Amrvis::YPLANE:
    if(sdir == Amrvis::XDIR) {
      tdir = Amrvis::ZDIR;
      dir1 = Amrvis::YDIR;
      dir2 = Amrvis::ZDIR;
    } else {
      tdir = Amrvis::XDIR;
      dir1 = Amrvis::XDIR;
      dir2 = Amrvis::YDIR;
    }
    break;
  case Amrvis::XPLANE:
    dir1 = Amrvis::XDIR;
    if(sdir == Amrvis::YDIR) {
      tdir = Amrvis::ZDIR; 
    } else {
      tdir = Amrvis::YDIR;
    }
    dir1 = Amrvis::XDIR;
    dir2 = tdir;
    break;
#endif
  }
  Vector<Box> ssTrueRegion(mal + 1);
  ssTrueRegion[mal] = amrPicturePtrArray[V]->GetSliceBox(mal);
#if (BL_SPACEDIM != 1)
  ssTrueRegion[mal].setSmall(tdir, ivLowOffsetMAL[tdir] + ix);
  ssTrueRegion[mal].setBig(tdir, ssTrueRegion[mal].smallEnd(tdir));
#endif
  int lev;
  for(lev = mal - 1; lev >= 0; --lev) {
    ssTrueRegion[lev] = ssTrueRegion[mal];
    ssTrueRegion[lev].coarsen(amrex::CRRBetweenLevels(lev, mal,
                            amrData.RefRatio()));
  }
  // Create an array of titles corresponding to the intersected line.
  Vector<Real> XdX(mal + 1);
  Vector<char *> intersectStr(mal + 1);
  
#if (BL_SPACEDIM == 3)
  char bufferL[128];
  sprintf(bufferL, "%s%s %s%s",
	  (dir1 == Amrvis::XDIR) ? "X=" : "Y=", pltAppState->GetFormatString().c_str(),
	  (dir2 == Amrvis::YDIR) ? "Y=" : "Z=", pltAppState->GetFormatString().c_str());
#endif
  for(lev = 0; lev <= mal; ++lev) {
    XdX[lev] = amrData.DxLevel()[lev][sdir];
    intersectStr[lev] = new char[128];  // ---- these are deleted by XYPlotDataList
#if (BL_SPACEDIM == 1)
    sprintf(intersectStr[lev], "X=");
    sprintf(intersectStr[lev]+2, pltAppState->GetFormatString().c_str(),
	    gridOffset[dir1] +
	    (0.5 + ssTrueRegion[lev].smallEnd(dir1))*amrData.DxLevel()[lev][dir1]);
#elif (BL_SPACEDIM == 2)
    sprintf(intersectStr[lev], ((dir1 == Amrvis::XDIR) ? "X=" : "Y="));
    sprintf(intersectStr[lev]+2, pltAppState->GetFormatString().c_str(),
	    gridOffset[dir1] +
	    (0.5 + ssTrueRegion[lev].smallEnd(dir1))*amrData.DxLevel()[lev][dir1]);
#elif (BL_SPACEDIM == 3)
    sprintf(intersectStr[lev], bufferL,
	    amrData.DxLevel()[lev][dir1] * (0.5 + ssTrueRegion[lev].smallEnd(dir1)) +
	    gridOffset[dir1],
	    amrData.DxLevel()[lev][dir2] * (0.5 + ssTrueRegion[lev].smallEnd(dir2)) +
	    gridOffset[dir2]);
#endif	    
  }
  XYPlotDataList *newlist = new XYPlotDataList(*derived,
                                     pltAppState->MinDrawnLevel(), mal,
				     ix, amrData.RefRatio(),
		                     XdX, intersectStr, gridOffset[sdir]);

  bool lineOK;
  DataServices::Dispatch(DataServices::LineValuesRequest,
			 dataServicesPtr[currentFrame],
			 mal + 1,
			 (void *) (ssTrueRegion.dataPtr()),
			 sdir,
			 (void *) derived,
			 pltAppState->MinAllowableLevel(), mal,
			 (void *) newlist, &lineOK);
  if(lineOK) {
    return newlist;
  }
  delete newlist;
  return NULL;
}


// -------------------------------------------------------------------
void PltApp::DoRubberBanding(Widget, XtPointer client_data, XtPointer call_data)
{
  XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct *) call_data;

  if(cbs->event->xany.type != ButtonPress) {
    return;
  }

  int scale(pltAppState->CurrentScale());
  int V((unsigned long) client_data);
  int imageHeight(amrPicturePtrArray[V]->ImageSizeV() - 1);
  int imageWidth(amrPicturePtrArray[V]->ImageSizeH() - 1);
  int oldX(max(0, min(imageWidth,  cbs->event->xbutton.x)));
  int oldY(max(0, min(imageHeight, cbs->event->xbutton.y)));
  int mal(pltAppState->MaxAllowableLevel());
  int minDrawnLevel(pltAppState->MinDrawnLevel());
  int maxDrawnLevel(pltAppState->MaxDrawnLevel());
  const AmrData &amrData(dataServicesPtr[currentFrame]->AmrDataRef());
  int rootX, rootY;
  unsigned int inputMask;
  Window whichRoot, whichChild;
  bool bShiftDown(cbs->event->xbutton.state & ShiftMask);
  bool bControlDown(cbs->event->xbutton.state & ControlMask);

#if (BL_SPACEDIM == 3)
  int x1, y1, z1, x2, y2, z2, rStartPlane;
  for(int np = 0; np != BL_SPACEDIM; ++np) {
    amrPicturePtrArray[np]->DoStop();
  }
#endif

  servingButton = cbs->event->xbutton.button;
  if(animating2d) {
    ResetAnimation();
  }

  XSetForeground(display, rbgc, pltPaletteptr->makePixel(120));
  XChangeActivePointerGrab(display, PointerMotionHintMask |
			   ButtonMotionMask | ButtonReleaseMask |
			   OwnerGrabButtonMask, cursor, CurrentTime);
  AVXGrab avxGrab(display);

  if(servingButton == 1) {
    if(bShiftDown) {
      oldX = 0;
    }
    if(bControlDown) {
      oldY = 0;
    }
    int rectDrawn(false);
    int anchorX(oldX);
    int anchorY(oldY);
    int newX, newY;

    while(true) {
      XNextEvent(display, &nextEvent);
      
      switch(nextEvent.type) {
	
      case MotionNotify:
	
	if(rectDrawn) {   // undraw the old rectangle(s)
	  rWidth  = std::abs(oldX-anchorX);
	  rHeight = std::abs(oldY-anchorY);
	  rStartX = (anchorX < oldX) ? anchorX : oldX;
	  rStartY = (anchorY < oldY) ? anchorY : oldY;
	  XDrawRectangle(display, amrPicturePtrArray[V]->PictureWindow(),
			 rbgc, rStartX, rStartY, rWidth, rHeight);
#if (BL_SPACEDIM == 3)
	  // 3D sub domain selecting
	  switch (V) {
	  case Amrvis::ZPLANE:
	    XDrawRectangle(display, amrPicturePtrArray[Amrvis::YPLANE]->PictureWindow(),
			   rbgc, rStartX, startcutY[Amrvis::YPLANE], rWidth,
			   std::abs(finishcutY[Amrvis::YPLANE]-startcutY[Amrvis::YPLANE]));
	    rStartPlane = (anchorY < oldY) ? oldY : anchorY;
	    XDrawRectangle(display, amrPicturePtrArray[Amrvis::XPLANE]->PictureWindow(),
			   rbgc, imageHeight-rStartPlane, startcutY[Amrvis::XPLANE],
			   rHeight,
			   std::abs(finishcutY[Amrvis::XPLANE]-startcutY[Amrvis::XPLANE]));
	    break;
	  case Amrvis::YPLANE:
	    XDrawRectangle(display, amrPicturePtrArray[Amrvis::ZPLANE]->PictureWindow(),
			   rbgc, rStartX, startcutY[Amrvis::ZPLANE], rWidth,
			   std::abs(finishcutY[Amrvis::ZPLANE]-startcutY[Amrvis::ZPLANE]));
	    XDrawRectangle(display, amrPicturePtrArray[Amrvis::XPLANE]->PictureWindow(),
			   rbgc, startcutX[Amrvis::XPLANE], rStartY,
			   std::abs(finishcutX[Amrvis::XPLANE]-startcutX[Amrvis::XPLANE]),
			   rHeight);
	    break;
	  default: // Amrvis::XPLANE
	    rStartPlane = (anchorX < oldX) ? oldX : anchorX;
	    XDrawRectangle(display, amrPicturePtrArray[Amrvis::ZPLANE]->PictureWindow(),
			   rbgc, startcutX[Amrvis::ZPLANE], imageWidth-rStartPlane,
			   std::abs(finishcutX[Amrvis::ZPLANE]-startcutX[Amrvis::ZPLANE]), rWidth);
	    XDrawRectangle(display, amrPicturePtrArray[Amrvis::YPLANE]->PictureWindow(),
			   rbgc, startcutX[Amrvis::YPLANE], rStartY,
			   std::abs(finishcutX[Amrvis::YPLANE]-startcutX[Amrvis::YPLANE]),
			   rHeight);
	  }
#endif
	}

	DoDrawPointerLocation(None, (XtPointer) static_cast<long>(V), &nextEvent);

	while(XCheckTypedEvent(display, MotionNotify, &nextEvent)) {
	  ;  // do nothing
	}
	
	XQueryPointer(display, amrPicturePtrArray[V]->PictureWindow(),
		      &whichRoot, &whichChild,
		      &rootX, &rootY, &newX, &newY, &inputMask);
	
        if(bShiftDown) {
	  newX = imageWidth;
	}
        if(bControlDown) {
	  newY = imageHeight;
	}
	newX = max(0, min(imageWidth,  newX));
	newY = max(0, min(imageHeight, newY));
	rWidth  = std::abs(newX-anchorX);   // draw the new rectangle
	rHeight = std::abs(newY-anchorY);
	rStartX = (anchorX < newX) ? anchorX : newX;
	rStartY = (anchorY < newY) ? anchorY : newY;
	XDrawRectangle(display, amrPicturePtrArray[V]->PictureWindow(),
		       rbgc, rStartX, rStartY, rWidth, rHeight);
	rectDrawn = true;
	
	oldX = newX;
	oldY = newY;
	
#if (BL_SPACEDIM == 3)
	// 3D sub domain selecting
	startcutX[V]  = rStartX;
	startcutY[V]  = rStartY;
	finishcutX[V] = rStartX + rWidth;
	finishcutY[V] = rStartY + rHeight;
	switch (V) {
	case Amrvis::ZPLANE:
	  startcutX[Amrvis::YPLANE] = startcutX[V];
	  finishcutX[Amrvis::YPLANE] = finishcutX[V];
	  startcutX[Amrvis::XPLANE] = imageHeight - startcutY[V];
	  finishcutX[Amrvis::XPLANE] = imageHeight - finishcutY[V];
	  // draw in other planes
	  XDrawRectangle(display, amrPicturePtrArray[Amrvis::YPLANE]->PictureWindow(),
			 rbgc, rStartX, startcutY[Amrvis::YPLANE], rWidth,
			 std::abs(finishcutY[Amrvis::YPLANE]-startcutY[Amrvis::YPLANE]));
	  rStartPlane = (anchorY < newY) ? newY : anchorY;
	  XDrawRectangle(display, amrPicturePtrArray[Amrvis::XPLANE]->PictureWindow(),
			 rbgc, imageHeight-rStartPlane, startcutY[Amrvis::XPLANE],
			 rHeight,
			 std::abs(finishcutY[Amrvis::XPLANE]-startcutY[Amrvis::XPLANE]));
	  break;
	case Amrvis::YPLANE:
	  startcutX[Amrvis::ZPLANE] = startcutX[V];
	  finishcutX[Amrvis::ZPLANE] = finishcutX[V];
	  startcutY[Amrvis::XPLANE] = startcutY[V];
	  finishcutY[Amrvis::XPLANE] = finishcutY[V];
	  XDrawRectangle(display, amrPicturePtrArray[Amrvis::ZPLANE]->PictureWindow(),
			 rbgc, rStartX, startcutY[Amrvis::ZPLANE], rWidth,
			 std::abs(finishcutY[Amrvis::ZPLANE]-startcutY[Amrvis::ZPLANE]));
	  XDrawRectangle(display, amrPicturePtrArray[Amrvis::XPLANE]->PictureWindow(),
			 rbgc, startcutX[Amrvis::XPLANE], rStartY,
			 std::abs(finishcutX[Amrvis::XPLANE]-startcutX[Amrvis::XPLANE]), rHeight);
	  break;
	default: // Amrvis::XPLANE
	  startcutY[Amrvis::YPLANE] = startcutY[V];
	  finishcutY[Amrvis::YPLANE] = finishcutY[V];
	  startcutY[Amrvis::ZPLANE] = imageWidth - startcutX[V];
	  finishcutY[Amrvis::ZPLANE] = imageWidth - finishcutX[V];
	  rStartPlane = (anchorX < newX) ? newX : anchorX;
	  XDrawRectangle(display, amrPicturePtrArray[Amrvis::ZPLANE]->PictureWindow(),
			 rbgc, startcutX[Amrvis::ZPLANE], imageWidth-rStartPlane,
			 std::abs(finishcutX[Amrvis::ZPLANE]-startcutX[Amrvis::ZPLANE]), rWidth);
	  XDrawRectangle(display, amrPicturePtrArray[Amrvis::YPLANE]->PictureWindow(),
			 rbgc, startcutX[Amrvis::YPLANE], rStartY,
			 std::abs(finishcutX[Amrvis::YPLANE]-startcutX[Amrvis::YPLANE]), rHeight);
	}
	
#if defined(BL_VOLUMERENDER) || defined(BL_PARALLELVOLUMERENDER)
	if( ! XmToggleButtonGetState(wAutoDraw))
#endif
	  {
	    x1 = startcutX[Amrvis::ZPLANE]/scale + ivLowOffsetMAL[Amrvis::XDIR];
	    y1 = startcutX[Amrvis::XPLANE]/scale + ivLowOffsetMAL[Amrvis::YDIR];
	    z1 = (amrPicturePtrArray[Amrvis::YPLANE]->ImageSizeV()-1 -
		  startcutY[Amrvis::YPLANE])/scale + ivLowOffsetMAL[Amrvis::ZDIR];
	    x2 = finishcutX[Amrvis::ZPLANE]/scale + ivLowOffsetMAL[Amrvis::XDIR];
	    y2 = finishcutX[Amrvis::XPLANE]/scale + ivLowOffsetMAL[Amrvis::YDIR];
	    z2 = (amrPicturePtrArray[Amrvis::YPLANE]->ImageSizeV()-1 -
		  finishcutY[Amrvis::YPLANE])/scale + ivLowOffsetMAL[Amrvis::ZDIR];
	    
	    if(z2 > 65536) {    // fix bad -z values
	      z2 = amrPicturePtrArray[Amrvis::ZPLANE]->GetSubDomain()[mal].smallEnd(Amrvis::ZDIR);
	    }
	    IntVect ivmin(min(x1,x2), min(y1,y2), min(z1,z2));
	    IntVect ivmax(max(x1,x2), max(y1,y2), max(z1,z2));
	    Box xyz12(ivmin, ivmax);
	    xyz12 &= amrPicturePtrArray[Amrvis::ZPLANE]->GetSubDomain()[mal];
	    
	    projPicturePtr->SetSubCut(xyz12); 
	    //projPicturePtr->MakeBoxes();
	    XClearWindow(display, XtWindow(wTransDA));
	    DoExposeTransDA();
	  }
#endif
	
	break;
	
      case ButtonRelease: {
        avxGrab.ExplicitUngrab();
	
	startX = (max(0, min(imageWidth,  anchorX))) / scale;
	startY = (max(0, min(imageHeight, anchorY))) / scale;
	endX   = (max(0, min(imageWidth,  nextEvent.xbutton.x))) / scale;
	endY   = (max(0, min(imageHeight, nextEvent.xbutton.y))) / scale;

	if(bShiftDown) {
	  startX = 0;
	  endX   = imageWidth  / scale;
	}
	if(bControlDown) {
	  startY = 0;
	  endY   = imageHeight / scale;
	}
	
	// make "aligned" box with correct size, converted to AMR space.
	selectionBox.setSmall(Amrvis::XDIR, min(startX, endX));
#if (BL_SPACEDIM != 1)
	selectionBox.setSmall(Amrvis::YDIR, ((imageHeight + 1) / scale) -
				     max(startY, endY) - 1);
#endif
	selectionBox.setBig(Amrvis::XDIR, max(startX, endX));
#if (BL_SPACEDIM != 1)
	selectionBox.setBig(Amrvis::YDIR, ((imageHeight + 1) / scale)  -
				     min(startY, endY) - 1);
#endif
	
	// selectionBox is now at the maxAllowableLevel because
	// it is defined on the pixmap ( / scale)
	
	if(anchorX == nextEvent.xbutton.x && anchorY == nextEvent.xbutton.y) {
	  // data at click
	  int y, intersectedLevel(-1);
	  Box intersectedGrid;
	  Vector<Box> trueRegionArray(mal+1);
	  int plane(amrPicturePtrArray[V]->GetSlice());
	  
	  trueRegionArray[mal] = selectionBox;
	  
	  // convert to point box
	  trueRegionArray[mal].setBig(Amrvis::XDIR, trueRegionArray[mal].smallEnd(Amrvis::XDIR));
#if (BL_SPACEDIM != 1)
	  trueRegionArray[mal].setBig(Amrvis::YDIR, trueRegionArray[mal].smallEnd(Amrvis::YDIR));
#endif
	  
	  if(V == Amrvis::ZPLANE) {
	    trueRegionArray[mal].shift(Amrvis::XDIR, ivLowOffsetMAL[Amrvis::XDIR]);
#if (BL_SPACEDIM != 1)
	    trueRegionArray[mal].shift(Amrvis::YDIR, ivLowOffsetMAL[Amrvis::YDIR]);
#endif
	    if(BL_SPACEDIM == 3) {
	      trueRegionArray[mal].setSmall(Amrvis::ZDIR, plane);
	      trueRegionArray[mal].setBig(Amrvis::ZDIR, plane);
	    }	
	  }	
	  if(V == Amrvis::YPLANE) {
	    trueRegionArray[mal].setSmall(Amrvis::ZDIR, trueRegionArray[mal].smallEnd(Amrvis::YDIR));
	    trueRegionArray[mal].setBig(Amrvis::ZDIR, trueRegionArray[mal].bigEnd(Amrvis::YDIR));
	    trueRegionArray[mal].setSmall(Amrvis::YDIR, plane);
	    trueRegionArray[mal].setBig(Amrvis::YDIR, plane);
	    trueRegionArray[mal].shift(Amrvis::XDIR, ivLowOffsetMAL[Amrvis::XDIR]);
	    trueRegionArray[mal].shift(Amrvis::ZDIR, ivLowOffsetMAL[Amrvis::ZDIR]);
	  }
	  if(V == Amrvis::XPLANE) {
	    trueRegionArray[mal].setSmall(Amrvis::ZDIR, trueRegionArray[mal].smallEnd(Amrvis::YDIR));
	    trueRegionArray[mal].setBig(Amrvis::ZDIR, trueRegionArray[mal].bigEnd(Amrvis::YDIR));
	    trueRegionArray[mal].setSmall(Amrvis::YDIR, trueRegionArray[mal].smallEnd(Amrvis::XDIR));
	    trueRegionArray[mal].setBig(Amrvis::YDIR, trueRegionArray[mal].bigEnd(Amrvis::XDIR));
	    trueRegionArray[mal].setSmall(Amrvis::XDIR, plane);
	    trueRegionArray[mal].setBig(Amrvis::XDIR, plane);
	    trueRegionArray[mal].shift(Amrvis::YDIR, ivLowOffsetMAL[Amrvis::YDIR]);
	    trueRegionArray[mal].shift(Amrvis::ZDIR, ivLowOffsetMAL[Amrvis::ZDIR]);
	  }
	  
	  for(y = mal - 1; y >= 0; --y) {
	    trueRegionArray[y] = trueRegionArray[mal];
	    trueRegionArray[y].coarsen(amrex::CRRBetweenLevels(y, mal,
	                          amrData.RefRatio()));
	    trueRegionArray[y].setBig(Amrvis::XDIR, trueRegionArray[y].smallEnd(Amrvis::XDIR));
#if (BL_SPACEDIM != 1)
	    trueRegionArray[y].setBig(Amrvis::YDIR, trueRegionArray[y].smallEnd(Amrvis::YDIR));
#endif
	  }
	  bool goodIntersect;
	  Real dataValue;
	  DataServices::Dispatch(DataServices::PointValueRequest,
				 dataServicesPtr[currentFrame],
				 trueRegionArray.size(),
				 (void *) (trueRegionArray.dataPtr()),
				 (void *) &pltAppState->CurrentDerived(),
				 minDrawnLevel, maxDrawnLevel,
				 &intersectedLevel, &intersectedGrid,
				 &dataValue, &goodIntersect);
	  char dataValueCharString[Amrvis::LINELENGTH];
	  sprintf(dataValueCharString, pltAppState->GetFormatString().c_str(),
	          dataValue);
	  string dataValueString(dataValueCharString);

	  const string vfDerived("vfrac");
	  bool bShowBody(AVGlobals::GetShowBody());
	  if(amrData.CartGrid() && pltAppState->CurrentDerived() != vfDerived &&
	     bShowBody)
	  {
	    DataServices::Dispatch(DataServices::PointValueRequest,
				   dataServicesPtr[currentFrame],
				   trueRegionArray.size(),
				   (void *) (trueRegionArray.dataPtr()),
				   (void *) &vfDerived,
				   minDrawnLevel, maxDrawnLevel,
				   &intersectedLevel, &intersectedGrid,
				   &dataValue, &goodIntersect);
	    Real vfeps(amrData.VfEps(intersectedLevel));
	    if(dataValue < vfeps) {
	      dataValueString = "body";
	    }
	  }
	  bool bIsMF(dataServicesPtr[currentFrame]->GetFileType() == Amrvis::MULTIFAB);
	  if(bIsMF && intersectedLevel == 0) {
	    dataValueString = "no data";
	  }

	  if(bTimeline) {
	    dataValueString = GetMPIFName(dataValue);
	  }

	  if(bRegions) {
	    dataValueString = GetRegionName(dataValue);
	  }

	  std::ostringstream buffout;
	  if(goodIntersect) {
	    double sDLoc;
	    buffout << '\n';
	    buffout << "level = " << intersectedLevel << '\n';
	    if(bTimeline) {
	      int idx;
	      buffout << "point = " << trueRegionArray[intersectedLevel].smallEnd() << '\n';
	      idx = Amrvis::XDIR;
	      buffout << "::::  amrData.Time() = " << amrData.Time() << std::endl;
	      buffout << "::::  amrData.ProbDomain()[mal] = " << amrData.ProbDomain()[mal] << std::endl;
	      sDLoc = gridOffset[idx] + (0.5 + trueRegionArray[mal].smallEnd()[idx]) *
		             amrData.DxLevel()[mal][idx];
	      sDLoc = amrData.Time() * sDLoc / static_cast<Real>(amrData.ProbDomain()[mal].length(idx));
	      char dLocStr[Amrvis::LINELENGTH];
	      sprintf(dLocStr, pltAppState->GetFormatString().c_str(), sDLoc);
	      buffout << "time   = " << dLocStr << '\n';
	      idx = Amrvis::YDIR;
	      int iLoc = int( gridOffset[idx] + trueRegionArray[mal].smallEnd()[idx] );
	      iLoc *= amrex::CRRBetweenLevels(maxDrawnLevel, amrData.FinestLevel(), amrData.RefRatio());
	      buffout << "rank   = " << iLoc << '\n';
              if(callTraceExists) {
                if(callTraceShowing) {
   	            XtPopup(wCallTraceTopLevel, XtGrabNone);
	    	    XMapRaised(XtDisplay(wCallTraceTopLevel), XtWindow(wCallTraceTopLevel));
	            std::ostringstream traceout;
	            DeriveCallStack(traceout, sDLoc, sDLoc);
		    cMessageArea traceMessageText;
                    traceMessageText.Init(wCallTraceList);
                    bool scrollToTop(true), clear(true);
                    traceMessageText.PrintText(traceout.str().c_str(), scrollToTop, clear);
                  } else {
	            DeriveCallStack(std::cout, sDLoc, sDLoc);
	          }
               }

	    } else {
	        buffout << "point = " << trueRegionArray[intersectedLevel].smallEnd() << '\n';
	        buffout << "grid  = " << intersectedGrid << '\n';
	        buffout << "loc   = (";
	        for(int idx(0); idx != BL_SPACEDIM; ++idx) {
	          if(idx != 0) {
	            buffout << ", ";
	          }
	          double ssDLoc = gridOffset[idx] +
		                  (0.5 + trueRegionArray[mal].smallEnd()[idx]) *
		                  amrData.DxLevel()[mal][idx];
	          char dLocStr[Amrvis::LINELENGTH];
	          sprintf(dLocStr, pltAppState->GetFormatString().c_str(), ssDLoc);
	          buffout << dLocStr;
	        }
	        buffout << ")\n";
	      }
	    buffout << "value = " << dataValueString << '\n';
	  } else {
            buffout << "Bad point at mouse click" << '\n';
          }
	  
	  PrintMessage(const_cast<char *>(buffout.str().c_str()));

	} else {
	  
	  // tell the amrpicture about the box
	  activeView = V;
	  if(startX < endX) { // box in scaled pixmap space
	    startX = selectionBox.smallEnd(Amrvis::XDIR) * scale;
	    endX   = selectionBox.bigEnd(Amrvis::XDIR)   * scale;
	  } else {	
	    startX = selectionBox.bigEnd(Amrvis::XDIR)   * scale;
	    endX   = selectionBox.smallEnd(Amrvis::XDIR) * scale;
	  }
	  
	  if(startY < endY) {
#if (BL_SPACEDIM == 1)
	    startY = 0;
	    endY   = scale;
#else
	    startY = imageHeight - selectionBox.bigEnd(Amrvis::YDIR)   * scale;
	    endY   = imageHeight - selectionBox.smallEnd(Amrvis::YDIR) * scale;
#endif
	  } else {
#if (BL_SPACEDIM == 1)
	    startY = 0;
	    endY   = scale;
#else
	    startY = imageHeight - selectionBox.smallEnd(Amrvis::YDIR) * scale;
	    endY   = imageHeight - selectionBox.bigEnd(Amrvis::YDIR)   * scale;
#endif
	  }
	
#if (BL_SPACEDIM == 1)
	  int nodeAdjustment = (scale - 1) * selectionBox.type()[Amrvis::XDIR];
#else
	  int nodeAdjustment = (scale - 1) * selectionBox.type()[Amrvis::YDIR];
#endif
	  startY -= nodeAdjustment;
	  endY   -= nodeAdjustment;
	  
	  amrPicturePtrArray[V]->SetRegion(startX, startY, endX, endY);
	  
#if (BL_SPACEDIM == 3)
	  amrPicturePtrArray[V]->SetSubCut(startcutX[V], startcutY[V],
					   finishcutX[V], finishcutY[V]);
	  if(V == Amrvis::ZPLANE) {
	    amrPicturePtrArray[Amrvis::YPLANE]->SetSubCut(startcutX[V], -1,
						  finishcutX[V], -1);
	    amrPicturePtrArray[Amrvis::XPLANE]->SetSubCut(imageHeight-startcutY[V],-1,
						  imageHeight-finishcutY[V],-1);
	  }
	  if(V == Amrvis::YPLANE) {
	    amrPicturePtrArray[Amrvis::ZPLANE]->SetSubCut(startcutX[V], -1,
						  finishcutX[V], -1);
	    amrPicturePtrArray[Amrvis::XPLANE]->SetSubCut(-1, startcutY[V], 
						  -1, finishcutY[V]);
	  }
	  if(V == Amrvis::XPLANE) {
	    amrPicturePtrArray[Amrvis::ZPLANE]->SetSubCut(-1, imageWidth-startcutX[V],
						  -1, imageWidth-finishcutX[V]);
	    amrPicturePtrArray[Amrvis::YPLANE]->SetSubCut(-1, startcutY[V], 
						  -1, finishcutY[V]);
	  }
#endif
	  
	  for(int np(0); np < Amrvis::NPLANES; ++np) {
	    amrPicturePtrArray[np]->DoExposePicture();
	  }

	  if(bTimeline) {    // ---- find the selected time range and show the call stack
	    double dLocL, dLocH;
	    Box sTrueRegion(selectionBox);
	    sTrueRegion.shift(Amrvis::XDIR, ivLowOffsetMAL[Amrvis::XDIR]);
	    sTrueRegion.shift(Amrvis::YDIR, ivLowOffsetMAL[Amrvis::YDIR]);

	    if(bTimeline) {
	      dLocL = gridOffset[Amrvis::XDIR] + (0.5 + sTrueRegion.smallEnd()[Amrvis::XDIR]) *
		             amrData.DxLevel()[mal][Amrvis::XDIR];

	      dLocL = amrData.Time() * dLocL /
	                      static_cast<Real>(amrData.ProbDomain()[mal].length(Amrvis::XDIR));

	      dLocH = gridOffset[Amrvis::XDIR] + (0.5 + sTrueRegion.bigEnd()[Amrvis::XDIR]) *
		             amrData.DxLevel()[mal][Amrvis::XDIR];

	      dLocH = amrData.Time() * dLocH /
	                      static_cast<Real>(amrData.ProbDomain()[mal].length(Amrvis::XDIR));
              if(callTraceExists) {
                if(callTraceShowing) {
	          XtPopup(wCallTraceTopLevel, XtGrabNone);
		  XMapRaised(XtDisplay(wCallTraceTopLevel), XtWindow(wCallTraceTopLevel));
	          std::ostringstream traceout;
	          DeriveCallStack(traceout, dLocL, dLocH);
		  cMessageArea traceMessageText;
                  traceMessageText.Init(wCallTraceList);
                  bool scrollToTop(true), clear(true);
                  traceMessageText.PrintText(traceout.str().c_str(), scrollToTop, clear);
                } else {
	          DeriveCallStack(std::cout, dLocL, dLocH);
	        }
              }
	    }
	  }
	}
	return;
      }
      break;

      case NoExpose:
      break;

      default:
	break;
      }  // end switch
    }  // end while(true)
  }



  if(servingButton == 2) {
    XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
	      rbgc, 0, oldY, imageWidth, oldY);
    
#if (BL_SPACEDIM == 3)
    switch (V) {
    case Amrvis::ZPLANE:
      XDrawLine(display, amrPicturePtrArray[Amrvis::XPLANE]->PictureWindow(),
		rbgc, imageHeight-oldY, 0, imageHeight-oldY,
		amrPicturePtrArray[Amrvis::XPLANE]->ImageSizeV());
      break;
    case Amrvis::YPLANE:
      XDrawLine(display, amrPicturePtrArray[Amrvis::XPLANE]->PictureWindow(), rbgc,
		0, oldY, amrPicturePtrArray[Amrvis::XPLANE]->ImageSizeH(), oldY);
      break;
    default: // Amrvis::XPLANE
      XDrawLine(display, amrPicturePtrArray[Amrvis::YPLANE]->PictureWindow(), rbgc,
		0, oldY, amrPicturePtrArray[Amrvis::YPLANE]->ImageSizeH(), oldY);
    }
#endif
    int tempi;
    while(true) {
      XNextEvent(display, &nextEvent);
      
      switch(nextEvent.type) {
	
      case MotionNotify:
	
	XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
		  rbgc, 0, oldY, imageWidth, oldY);
#if (BL_SPACEDIM == 3)
	// undraw in other planes
	if(V == Amrvis::ZPLANE) {
	  XDrawLine(display, amrPicturePtrArray[Amrvis::XPLANE]->PictureWindow(),
		    rbgc, imageHeight-oldY, 0, imageHeight-oldY,
		    amrPicturePtrArray[Amrvis::XPLANE]->ImageSizeV());
	} else if(V == Amrvis::YPLANE) {
	  XDrawLine(display, amrPicturePtrArray[Amrvis::XPLANE]->PictureWindow(), rbgc,
		    0, oldY, amrPicturePtrArray[Amrvis::XPLANE]->ImageSizeH(), oldY);
	} else if(V == Amrvis::XPLANE) {
	  XDrawLine(display, amrPicturePtrArray[Amrvis::YPLANE]->PictureWindow(), rbgc,
		    0, oldY, amrPicturePtrArray[Amrvis::YPLANE]->ImageSizeH(), oldY);
	}
#endif

	DoDrawPointerLocation(None, (XtPointer) static_cast<long>(V), &nextEvent);
	while(XCheckTypedEvent(display, MotionNotify, &nextEvent)) {
	  ;  // do nothing
	}
	XQueryPointer(display, amrPicturePtrArray[V]->PictureWindow(),
		      &whichRoot, &whichChild,
		      &rootX, &rootY, &oldX, &oldY, &inputMask);
	
	// draw the new line
	XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
		  rbgc, 0, oldY, imageWidth, oldY);
#if (BL_SPACEDIM == 3)
	switch (V) {
	case Amrvis::ZPLANE:
	  XDrawLine(display, amrPicturePtrArray[Amrvis::XPLANE]->PictureWindow(),
		    rbgc, imageHeight-oldY, 0, imageHeight-oldY,
		    amrPicturePtrArray[Amrvis::XPLANE]->ImageSizeV());
	  break;
	case Amrvis::YPLANE:
	  XDrawLine(display, amrPicturePtrArray[Amrvis::XPLANE]->PictureWindow(), rbgc,
		    0, oldY, amrPicturePtrArray[Amrvis::XPLANE]->ImageSizeH(), oldY);
	  break;
	default: // Amrvis::XPLANE
	  XDrawLine(display, amrPicturePtrArray[Amrvis::YPLANE]->PictureWindow(), rbgc,
		    0, oldY, amrPicturePtrArray[Amrvis::YPLANE]->ImageSizeH(), oldY);
	}
#endif
	
	break;
	
      case ButtonRelease:
        avxGrab.ExplicitUngrab();
	tempi = max(0, min(imageHeight, nextEvent.xbutton.y));
	amrPicturePtrArray[V]->SetHLine(tempi);
#if (BL_SPACEDIM == 3)
	if(V == Amrvis::ZPLANE) {
	  amrPicturePtrArray[Amrvis::XPLANE]->SetVLine(amrPicturePtrArray[Amrvis::XPLANE]->
				                       ImageSizeH()-1 - tempi);
	}
	if(V == Amrvis::YPLANE) {
	  amrPicturePtrArray[Amrvis::XPLANE]->SetHLine(tempi);
	}
	if(V == Amrvis::XPLANE) {
	  amrPicturePtrArray[Amrvis::YPLANE]->SetHLine(tempi);
	}

        if( ! ( (cbs->event->xbutton.state & ShiftMask) ||
                (cbs->event->xbutton.state & ControlMask) ) )
        {
	  if(V == Amrvis::ZPLANE) {
	    amrPicturePtrArray[Amrvis::YPLANE]->
	      APChangeSlice((imageHeight - tempi)/scale + ivLowOffsetMAL[Amrvis::YDIR]);
	  }
	  if(V == Amrvis::YPLANE) {
	    amrPicturePtrArray[Amrvis::ZPLANE]->
	      APChangeSlice((imageHeight - tempi)/scale + ivLowOffsetMAL[Amrvis::ZDIR]);
	  }
	  if(V == Amrvis::XPLANE) {
	    amrPicturePtrArray[Amrvis::ZPLANE]->
	      APChangeSlice((imageHeight - tempi)/scale + ivLowOffsetMAL[Amrvis::ZDIR]);
	  }
	  for(int np(0); np < Amrvis::NPLANES; ++np) {
	    amrPicturePtrArray[np]->DoExposePicture();
	    projPicturePtr->ChangeSlice(np, amrPicturePtrArray[np]->GetSlice());
	  }

	  projPicturePtr->MakeSlices();
	  XClearWindow(display, XtWindow(wTransDA));
	  DoExposeTransDA();
	  DoExposeRef();
          if(datasetShowing) {
            DoDatasetButton(NULL, NULL, NULL);
          }
	  return;
	}
#endif

	for(int np(0); np != Amrvis::NPLANES; ++np) {
	  amrPicturePtrArray[np]->DoExposePicture();
	}

	if(oldY >= 0 && oldY <= imageHeight) {
	  int sdir(-1);
	  switch (V) {
	    case Amrvis::ZPLANE:
	      sdir = Amrvis::XDIR;
	    break;
	    case Amrvis::YPLANE:
	      sdir = Amrvis::XDIR;
	    break;
	    case Amrvis::XPLANE:
	      sdir = Amrvis::YDIR;
	    break;
	  }
	  XYPlotDataList *newlist = CreateLinePlot(V, sdir, mal,
				      (imageHeight + 1) / scale - 1 - oldY / scale,
			              &pltAppState->CurrentDerived());
	  if(newlist) {
	    newlist->SetLevel(maxDrawnLevel);
	    if(XYplotwin[sdir] == NULL) {
              char cTempFN[Amrvis::BUFSIZE];
              strcpy(cTempFN,
	             AVGlobals::StripSlashes(fileNames[currentFrame]).c_str());
	      XYplotwin[sdir] = new XYPlotWin(cTempFN, appContext, wAmrVisTopLevel,
					      this, sdir, currentFrame);
	    }
	    XYplotwin[sdir]->AddDataList(newlist);
	  }
	}

	return;
	
      default:
	break;
      }  // end switch
    }  // end while(true)
  }





  if(servingButton == 3) {
    int tempi;
    XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
	      rbgc, oldX, 0, oldX, imageHeight);
#if (BL_SPACEDIM == 3)
    if(V == Amrvis::ZPLANE) {
      XDrawLine(display, amrPicturePtrArray[Amrvis::YPLANE]->PictureWindow(), rbgc,
		oldX, 0, oldX, amrPicturePtrArray[Amrvis::YPLANE]->ImageSizeV());
    }
    if(V == Amrvis::YPLANE) {
      XDrawLine(display, amrPicturePtrArray[Amrvis::ZPLANE]->PictureWindow(),rbgc,
		oldX, 0, oldX, amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeV());
    }
    if(V == Amrvis::XPLANE) {
      XDrawLine(display, amrPicturePtrArray[Amrvis::ZPLANE]->PictureWindow(), rbgc,
		0, imageWidth - oldX, amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeH(),
		imageWidth - oldX);
    }
#endif
    while(true) {
      XNextEvent(display, &nextEvent);
      
      switch(nextEvent.type) {
	
      case MotionNotify:
	
	// undraw the old line
	XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
		  rbgc, oldX, 0, oldX, imageHeight);
#if (BL_SPACEDIM == 3)
	// undraw in other planes
	if(V == Amrvis::ZPLANE) {
	  XDrawLine(display, amrPicturePtrArray[Amrvis::YPLANE]->PictureWindow(), rbgc,
		    oldX, 0, oldX, amrPicturePtrArray[Amrvis::YPLANE]->ImageSizeV());
	}
	if(V == Amrvis::YPLANE) {
	  XDrawLine(display, amrPicturePtrArray[Amrvis::ZPLANE]->PictureWindow(), rbgc,
		    oldX, 0, oldX, amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeV());
	}
	if(V == Amrvis::XPLANE) {
	  XDrawLine(display, amrPicturePtrArray[Amrvis::ZPLANE]->PictureWindow(), rbgc,
		    0, imageWidth-oldX, amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeH(),
		    imageWidth-oldX);
	}
#endif

	DoDrawPointerLocation(None, (XtPointer)  static_cast<long>(V), &nextEvent);
	while(XCheckTypedEvent(display, MotionNotify, &nextEvent)) {
	  ;  // do nothing
	}
	XQueryPointer(display, amrPicturePtrArray[V]->PictureWindow(),
		      &whichRoot, &whichChild,
		      &rootX, &rootY, &oldX, &oldY, &inputMask);
	
	// draw the new line
	XDrawLine(display, amrPicturePtrArray[V]->PictureWindow(),
		  rbgc, oldX, 0, oldX, imageHeight);
#if (BL_SPACEDIM == 3)
	if(V == Amrvis::ZPLANE) {
	  XDrawLine(display, amrPicturePtrArray[Amrvis::YPLANE]->PictureWindow(), rbgc,
		    oldX, 0, oldX, amrPicturePtrArray[Amrvis::YPLANE]->ImageSizeV());
	}
	if(V == Amrvis::YPLANE) {
	  XDrawLine(display, amrPicturePtrArray[Amrvis::ZPLANE]->PictureWindow(), rbgc,
		    oldX, 0, oldX, amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeV());
	}
	if(V == Amrvis::XPLANE) {
	  XDrawLine(display, amrPicturePtrArray[Amrvis::ZPLANE]->PictureWindow(), rbgc,
		    0, imageWidth - oldX, amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeH(),
		    imageWidth - oldX);
	}
#endif
	
	break;
	
      case ButtonRelease:
        avxGrab.ExplicitUngrab();
	tempi = max(0, min(imageWidth, nextEvent.xbutton.x));
	amrPicturePtrArray[V]->SetVLine(tempi);
#if (BL_SPACEDIM == 3)
	if(V == Amrvis::ZPLANE) {
	  amrPicturePtrArray[Amrvis::YPLANE]->SetVLine(tempi);
	}
	if(V == Amrvis::YPLANE) {
	  amrPicturePtrArray[Amrvis::ZPLANE]->SetVLine(tempi);
	}
	if(V == Amrvis::XPLANE) {
	  amrPicturePtrArray[Amrvis::ZPLANE]->SetHLine(amrPicturePtrArray[Amrvis::ZPLANE]->
				                        ImageSizeV()-1 - tempi);
	}
        if( ! ( (cbs->event->xbutton.state & ShiftMask) ||
                (cbs->event->xbutton.state & ControlMask) ) )
        {

	  if(V == Amrvis::ZPLANE) {
	    amrPicturePtrArray[Amrvis::XPLANE]->
	      APChangeSlice((tempi / scale) + ivLowOffsetMAL[Amrvis::XDIR]);
	  }
	  if(V == Amrvis::YPLANE) {
	    amrPicturePtrArray[Amrvis::XPLANE]->
	      APChangeSlice((tempi / scale) + ivLowOffsetMAL[Amrvis::XDIR]);
	  }
	  if(V == Amrvis::XPLANE) {
	    amrPicturePtrArray[Amrvis::YPLANE]->
	      APChangeSlice((tempi / scale) + ivLowOffsetMAL[Amrvis::YDIR]);
	  }
	  
	  for(int np(0); np < Amrvis::NPLANES; ++np) {
	    amrPicturePtrArray[np]->DoExposePicture();
	    projPicturePtr->ChangeSlice(np, amrPicturePtrArray[np]->GetSlice());
	  }
	  
	  projPicturePtr->MakeSlices();
	  XClearWindow(display, XtWindow(wTransDA));
	  DoExposeTransDA();
	  DoExposeRef();
          if(datasetShowing) {
            DoDatasetButton(NULL, NULL, NULL);
          }
	  return;
	}
#endif

	for(int np(0); np != Amrvis::NPLANES; ++np) {
	  amrPicturePtrArray[np]->DoExposePicture();
	}

	if(oldX >= 0 && oldX <= imageWidth) {
	  int sdir(-1);
	  switch (V) {
	    case Amrvis::ZPLANE:
#if (BL_SPACEDIM == 1)
	      sdir = Amrvis::XDIR;
#else
	      sdir = Amrvis::YDIR;
#endif
	    break;
	    case Amrvis::YPLANE:
	      sdir = Amrvis::ZDIR;
	    break;
	    case Amrvis::XPLANE:
	      sdir = Amrvis::ZDIR;
	    break;
	  }
	  XYPlotDataList *newlist = CreateLinePlot(V, sdir, mal, oldX / scale,
	                                           &pltAppState->CurrentDerived());
	  if(newlist) {
	    newlist->SetLevel(maxDrawnLevel);
	    if(XYplotwin[sdir] == NULL) {
              char cTempFN[Amrvis::BUFSIZE];
              strcpy(cTempFN,
	             AVGlobals::StripSlashes(fileNames[currentFrame]).c_str());
	      XYplotwin[sdir] = new XYPlotWin(cTempFN, appContext, wAmrVisTopLevel,
					      this, sdir, currentFrame);
	    }
	    XYplotwin[sdir]->AddDataList(newlist);
	  }
	}
	
	return;
	
      default:
	break;
      }  // end switch
    }  // end while(true)
  }


}  // end DoRubberBanding


// -------------------------------------------------------------------
void PltApp::DoExposePalette(Widget, XtPointer, XtPointer) {
  pltPaletteptr->ExposePalette();
}


// -------------------------------------------------------------------
void PltApp::PADoExposePicture(Widget /*w*/, XtPointer client_data, XtPointer) {
  unsigned long np = (unsigned long) client_data;
//cout << "==%%%%%%%%%%%%=== _in PADoExposePicture:  currentFrame = " << currentFrame << endl;
  
  amrPicturePtrArray[np]->DoExposePicture();
  // draw bounding box
  /*
  int isX = amrPicturePtrArray[np]->ImageSizeH();
  int isY = amrPicturePtrArray[np]->ImageSizeV();
  XSetForeground(display, xgc, pltPaletteptr->WhiteIndex());
  XDrawLine(display, XtWindow(w), xgc, 0, isY, isX, isY);
  XDrawLine(display, XtWindow(w), xgc, isX, 0, isX, isY);
  */
}


// -------------------------------------------------------------------
void PltApp::DoDrawPointerLocation(Widget, XtPointer data, XtPointer cbe) {
  XEvent *event = (XEvent *) cbe;

  if(event->type == LeaveNotify) {
    XClearWindow(display, XtWindow(wLocArea));
    return;
  }

  unsigned long V((unsigned long) data);
  Window whichRoot, whichChild;
  int rootX(0), rootY(0), newX(0), newY(0);
  unsigned int inputMask(0);
  int currentScale(pltAppState->CurrentScale());
  
  XQueryPointer(display, XtWindow(wPlotPlane[V]), &whichRoot, &whichChild,
		&rootX, &rootY, &newX, &newY, &inputMask);

  int iVertLoc, iHorizLoc;
  char locText[Amrvis::LINELENGTH], locTextFormat[Amrvis::LINELENGTH];

#if (BL_SPACEDIM == 3)
  int iPlaneLoc(amrPicturePtrArray[V]->GetSlice());

  double Xloc(gridOffset[Amrvis::XDIR]);
  double Yloc(gridOffset[Amrvis::YDIR]);
  double Zloc(gridOffset[Amrvis::ZDIR]);
  switch(V) {
    case Amrvis::ZPLANE:
      iVertLoc = ((amrPicturePtrArray[V]->ImageSizeV())/currentScale) -
                  1 - (newY / currentScale) + ivLowOffsetMAL[Amrvis::YDIR];
      iHorizLoc = newX / currentScale + ivLowOffsetMAL[Amrvis::XDIR];
      Xloc += (0.5 + iHorizLoc) * finestDx[Amrvis::XDIR];
      Yloc += (0.5 + iVertLoc)  * finestDx[Amrvis::YDIR];
      Zloc += (0.5 + iPlaneLoc) * finestDx[Amrvis::ZDIR];
    break;

    case Amrvis::YPLANE:
      iVertLoc = ((amrPicturePtrArray[V]->ImageSizeV())/currentScale) -
                  1 - (newY / currentScale) + ivLowOffsetMAL[Amrvis::ZDIR];
      iHorizLoc = newX / currentScale + ivLowOffsetMAL[Amrvis::XDIR];
      Xloc += (0.5 + iHorizLoc) * finestDx[Amrvis::XDIR];
      Yloc += (0.5 + iPlaneLoc) * finestDx[Amrvis::YDIR];
      Zloc += (0.5 + iVertLoc)  * finestDx[Amrvis::ZDIR];
    break;

    case Amrvis::XPLANE:
      iVertLoc = ((amrPicturePtrArray[V]->ImageSizeV())/currentScale) -
                  1 - (newY / currentScale) + ivLowOffsetMAL[Amrvis::ZDIR];
      iHorizLoc = newX / currentScale + ivLowOffsetMAL[Amrvis::YDIR];
      Xloc += (0.5 + iPlaneLoc) * finestDx[Amrvis::XDIR];
      Yloc += (0.5 + iHorizLoc) * finestDx[Amrvis::YDIR];
      Zloc += (0.5 + iVertLoc)  * finestDx[Amrvis::ZDIR];
    break;
  }
  string fstr = pltAppState->GetFormatString();
  sprintf(locTextFormat, "(%s, %s, %s)", fstr.c_str(), fstr.c_str(), fstr.c_str());
  sprintf(locText, locTextFormat, Xloc, Yloc, Zloc);
#elif (BL_SPACEDIM == 2)
  iVertLoc = ((amrPicturePtrArray[V]->ImageSizeV())/currentScale) -
             1 - (newY / currentScale) + ivLowOffsetMAL[Amrvis::YDIR];
  iHorizLoc = newX / currentScale + ivLowOffsetMAL[Amrvis::XDIR];
  double Xloc(gridOffset[Amrvis::XDIR] + (0.5 + iHorizLoc) * finestDx[Amrvis::XDIR]);
  double Yloc(gridOffset[Amrvis::YDIR] + (0.5 + iVertLoc) * finestDx[Amrvis::YDIR]);
  string fstr = pltAppState->GetFormatString();
  sprintf(locTextFormat, "(%s, %s)", fstr.c_str(), fstr.c_str());
  sprintf(locText, locTextFormat, Xloc, Yloc);
#elif (BL_SPACEDIM == 1)
  iHorizLoc = newX / currentScale + ivLowOffsetMAL[Amrvis::XDIR];
  double Xloc(gridOffset[Amrvis::XDIR] + (0.5 + iHorizLoc) * finestDx[Amrvis::XDIR]);
  string fstr = pltAppState->GetFormatString();
  sprintf(locTextFormat, "(%s)", fstr.c_str());
  sprintf(locText, locTextFormat, Xloc);
#endif

  XSetForeground(display, xgc, pltPaletteptr->WhiteIndex());
  XClearWindow(display, XtWindow(wLocArea));
  XDrawString(display, XtWindow(wLocArea), xgc, 10, 20, locText, strlen(locText));
} 

// -------------------------------------------------------------------
void PltApp::DoSpeedScale(Widget, XtPointer, XtPointer call_data) {
  XmScaleCallbackStruct *cbs = (XmScaleCallbackStruct *) call_data;
  frameSpeed = 600 - cbs->value;
# if(BL_SPACEDIM == 3)
  for(int v(0); v != 3; ++v) {
    amrPicturePtrArray[v]->SetFrameSpeed(600 - cbs->value);
  }
# endif
  XSync(display, false);
}


// -------------------------------------------------------------------
void PltApp::DoBackStep(int plane) {
  int currentScale(pltAppState->CurrentScale());
  int maxAllowLev(pltAppState->MaxAllowableLevel());
  int maxDrawnLev(pltAppState->MaxDrawnLevel());
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  int crrDiff(amrex::CRRBetweenLevels(maxDrawnLev, maxAllowLev,
              amrData.RefRatio()));
  AmrPicture *appX = amrPicturePtrArray[Amrvis::XPLANE];
  AmrPicture *appY = amrPicturePtrArray[Amrvis::YPLANE];
  AmrPicture *appZ = amrPicturePtrArray[Amrvis::ZPLANE];
  switch(plane) {
   case Amrvis::XPLANE:
    if(appX->GetSlice() / crrDiff >
       appX->GetSubDomain()[maxAllowLev].smallEnd(Amrvis::XDIR) / crrDiff)
    {
      appZ->SetVLine(appZ->GetVLine() - currentScale * crrDiff);
      appZ->DoExposePicture();
      appY->SetVLine(appY->GetVLine() - currentScale * crrDiff);
      appY->DoExposePicture();
      appX->APChangeSlice(appX->GetSlice() - crrDiff);
      break;
    }
    appZ->SetVLine(appY->ImageSizeH() - 1);
    appZ->DoExposePicture();
    appY->SetVLine(appY->ImageSizeH() - 1);
    appY->DoExposePicture();
    appX-> APChangeSlice(appX->GetSubDomain()[maxAllowLev].bigEnd(Amrvis::XDIR));
   break;

   case Amrvis::YPLANE:
    if(appY->GetSlice() / crrDiff >
       appY->GetSubDomain()[maxAllowLev].smallEnd(Amrvis::YDIR) / crrDiff)
    {
      appX->SetVLine(appX->GetVLine() - currentScale * crrDiff);
      appX->DoExposePicture();
      appZ->SetHLine(appZ->GetHLine() + currentScale * crrDiff);
      appZ->DoExposePicture();
      appY->APChangeSlice(appY->GetSlice() - crrDiff);
      break;
    }
    appX->SetVLine(appX->ImageSizeH() - 1);
    appX->DoExposePicture();
    appZ->SetHLine(0);
    appZ->DoExposePicture();
    appY->APChangeSlice(appY->GetSubDomain()[maxAllowLev].bigEnd(Amrvis::YDIR));
   break;

   case Amrvis::ZPLANE:
    if(appZ->GetSlice() / crrDiff >
       appZ->GetSubDomain()[maxAllowLev].smallEnd(Amrvis::ZDIR) / crrDiff)
    {
      appX->SetHLine(appX->GetHLine() + currentScale * crrDiff);
      appX->DoExposePicture();
      appY->SetHLine(appY->GetHLine() + currentScale * crrDiff);
      appY->DoExposePicture();
      appZ->APChangeSlice(appZ->GetSlice() - crrDiff);
      break;
    }
    appX->SetHLine(0);
    appX->DoExposePicture();
    appY->SetHLine(0);
    appY->DoExposePicture();
    appZ->APChangeSlice(appZ->GetSubDomain()[maxAllowLev].bigEnd(Amrvis::ZDIR));
  }

#if (BL_SPACEDIM == 3)
  projPicturePtr->ChangeSlice(plane, amrPicturePtrArray[plane]->GetSlice());
  projPicturePtr->MakeSlices();
  XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
  DoExposeTransDA();
  if(datasetShowing) {
    DoDatasetButton(NULL, NULL, NULL);
  }
#endif
  DoExposeRef();
}


// -------------------------------------------------------------------
void PltApp::DoForwardStep(int plane) {
  int currentScale(pltAppState->CurrentScale());
  int maxAllowLev(pltAppState->MaxAllowableLevel());
  int maxDrawnLev(pltAppState->MaxDrawnLevel());
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  int crrDiff(amrex::CRRBetweenLevels(maxDrawnLev, maxAllowLev,
              amrData.RefRatio()));
  AmrPicture *appX = amrPicturePtrArray[Amrvis::XPLANE];
  AmrPicture *appY = amrPicturePtrArray[Amrvis::YPLANE];
  AmrPicture *appZ = amrPicturePtrArray[Amrvis::ZPLANE];
  switch(plane) {
   case Amrvis::XPLANE:
    if(appX->GetSlice() / crrDiff <
       appX->GetSubDomain()[maxAllowLev].bigEnd(Amrvis::XDIR) / crrDiff)
    {
      appZ->SetVLine(appZ->GetVLine() + currentScale * crrDiff);
      appZ->DoExposePicture();
      appY->SetVLine(appY->GetVLine() + currentScale * crrDiff);
      appY->DoExposePicture();
      appX->APChangeSlice(appX->GetSlice() + crrDiff);
      break;
    }
    appZ->SetVLine(0);
    appZ->DoExposePicture();
    appY->SetVLine(0);
    appY->DoExposePicture();
    appX->APChangeSlice(appX->GetSubDomain()[maxAllowLev].smallEnd(Amrvis::XDIR));
   break;

   case Amrvis::YPLANE:
    if(appY->GetSlice() / crrDiff <
       appY->GetSubDomain()[maxAllowLev].bigEnd(Amrvis::YDIR) / crrDiff)
    {
      appX->SetVLine(appX->GetVLine() + currentScale * crrDiff);
      appX->DoExposePicture();
      appZ->SetHLine(appZ->GetHLine() - currentScale * crrDiff);
      appZ->DoExposePicture();
      appY->APChangeSlice(appY->GetSlice() + crrDiff);
      break;
    }
    appX->SetVLine(0);
    appX->DoExposePicture();
    appZ->SetHLine(appX->ImageSizeV() - 1);
    appZ->DoExposePicture();
    appY->APChangeSlice(appY->GetSubDomain()[maxAllowLev].smallEnd(Amrvis::YDIR));
   break;

   case Amrvis::ZPLANE:
    if(appZ->GetSlice() / crrDiff <
       appZ->GetSubDomain()[maxAllowLev].bigEnd(Amrvis::ZDIR) / crrDiff)
    {
      appX->SetHLine(appX->GetHLine() - currentScale * crrDiff);
      appX->DoExposePicture();
      appY->SetHLine(appY->GetHLine() - currentScale * crrDiff);
      appY->DoExposePicture();
      appZ->APChangeSlice(appZ->GetSlice() + crrDiff);
      break;
    }
    appX->SetHLine(appX->ImageSizeV() - 1);
    appX->DoExposePicture();
    appY->SetHLine(appY->ImageSizeV() - 1);
    appY->DoExposePicture();
    appZ->APChangeSlice(appZ->GetSubDomain()[maxAllowLev].smallEnd(Amrvis::ZDIR));
  }
#if (BL_SPACEDIM == 3)
  projPicturePtr->ChangeSlice(plane, amrPicturePtrArray[plane]->GetSlice());
  projPicturePtr->MakeSlices();
  XClearWindow(XtDisplay(wTransDA), XtWindow(wTransDA));
  DoExposeTransDA();
  if(datasetShowing) {
    DoDatasetButton(NULL, NULL, NULL);
  }
#endif
  DoExposeRef();
}


// -------------------------------------------------------------------
void PltApp::ChangePlane(Widget, XtPointer data, XtPointer cbs) {

  unsigned long which = (unsigned long) data;

#if (BL_SPACEDIM == 3)
  if(which == WCSTOP) {
    amrPicturePtrArray[Amrvis::XPLANE]->DoStop();
    amrPicturePtrArray[Amrvis::YPLANE]->DoStop();
    amrPicturePtrArray[Amrvis::ZPLANE]->DoStop();
    return;
  }
#endif

  if(which == WCASTOP) {
    writingRGB = false;
    bSyncFrame = true;
    pltAppState->SetCurrentFrame(currentFrame);
    StopAnimation();
    return;
  }
  
  XmPushButtonCallbackStruct *cbstr = (XmPushButtonCallbackStruct *) cbs;
  bool bShiftDown(cbstr->event->xbutton.state & ShiftMask);
  if(cbstr->click_count > 1 || bShiftDown) {
    switch(which) {
#if (BL_SPACEDIM == 3)
      case WCXNEG: amrPicturePtrArray[Amrvis::XPLANE]->Sweep(Amrvis::ANIMNEGDIR); return;
      case WCXPOS: amrPicturePtrArray[Amrvis::XPLANE]->Sweep(Amrvis::ANIMPOSDIR); return;
      case WCYNEG: amrPicturePtrArray[Amrvis::YPLANE]->Sweep(Amrvis::ANIMNEGDIR); return;
      case WCYPOS: amrPicturePtrArray[Amrvis::YPLANE]->Sweep(Amrvis::ANIMPOSDIR); return;
      case WCZNEG: amrPicturePtrArray[Amrvis::ZPLANE]->Sweep(Amrvis::ANIMNEGDIR); return;
      case WCZPOS: amrPicturePtrArray[Amrvis::ZPLANE]->Sweep(Amrvis::ANIMPOSDIR); return;
#endif
      case WCATNEG: Animate(Amrvis::ANIMNEGDIR); return;
      case WCATPOS: Animate(Amrvis::ANIMPOSDIR); return;
      case WCARGB: writingRGB = true; Animate(Amrvis::ANIMPOSDIR); return;
      default: return;
    }
  }
  switch(which) {
#if (BL_SPACEDIM == 3)
    case WCXNEG: DoBackStep(Amrvis::XPLANE);    return;
    case WCXPOS: DoForwardStep(Amrvis::XPLANE); return;
    case WCYNEG: DoBackStep(Amrvis::YPLANE);    return;
    case WCYPOS: DoForwardStep(Amrvis::YPLANE); return;
    case WCZNEG: DoBackStep(Amrvis::ZPLANE);    return;
    case WCZPOS: DoForwardStep(Amrvis::ZPLANE); return;
#endif
    case WCATNEG: DoAnimBackStep();     return;
    case WCATPOS: DoAnimForwardStep();  return;
    case WCARGB: writingRGB = true; DoAnimForwardStep(); return;
  }
}


// -------------------------------------------------------------------
void PltApp::DoAnimBackStep() {
  StopAnimation();
  --currentFrame;
  if(currentFrame < 0) {
    currentFrame = animFrames - 1;
  }
  pltAppState->SetCurrentFrame(currentFrame);
  ShowFrame();
}


// -------------------------------------------------------------------
void PltApp::DoAnimForwardStep() {
  StopAnimation();
  if(writingRGB) {
    DoCreateAnimRGBFile();
    writingRGB = false;
  }
  ++currentFrame;
  if(currentFrame == animFrames) {
    currentFrame = 0;
  }
  pltAppState->SetCurrentFrame(currentFrame);
  ShowFrame();
}


// -------------------------------------------------------------------
void PltApp::DoAnimFileScale(Widget, XtPointer, XtPointer cbs) {
  StopAnimation();
  currentFrame = ((XmScaleCallbackStruct *) cbs)->value;
  pltAppState->SetCurrentFrame(currentFrame);
  ShowFrame();
}


// -------------------------------------------------------------------
void PltApp::ResetAnimation() {
  StopAnimation();
  if( ! interfaceReady) {
#if(BL_SPACEDIM != 3)
    int maLev(pltAppState->MaxAllowableLevel());
    AmrPicture *Tempap = amrPicturePtrArray[Amrvis::ZPLANE];
    XtRemoveEventHandler(wPlotPlane[Amrvis::ZPLANE], ExposureMask, false, 
			 (XtEventHandler) &PltApp::StaticEvent,
			 (XtPointer) Tempap);
    Box fineDomain(amrPicturePtrArray[Amrvis::ZPLANE]->GetSubDomain()[maLev]);
    delete Tempap;
    
    const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
    fineDomain.refine(amrex::CRRBetweenLevels(maLev, amrData.FinestLevel(),
                                                  amrData.RefRatio()));
    amrPicturePtrArray[Amrvis::ZPLANE] = new AmrPicture(Amrvis::ZPLANE, gaPtr, fineDomain, 
						NULL, this,
						pltAppState,
						pltAppState->GetCGSmoothing());
    amrPicturePtrArray[Amrvis::ZPLANE]->SetRegion(startX, startY, endX, endY);
    //pltAppState->SetMaxDrawnLevel(maxDrawnLevel);
    //SetNumContours(false);
    //XtRemoveEventHandler(wPlotPlane[Amrvis::ZPLANE], ExposureMask, false, 
			 //(XtEventHandler) &PltApp::StaticEvent,
			 //(XtPointer) Tempap);
    //delete Tempap;
    amrPicturePtrArray[Amrvis::ZPLANE]->CreatePicture(XtWindow(wPlotPlane[Amrvis::ZPLANE]),
					      pltPaletteptr);
    AddStaticEventHandler(wPlotPlane[Amrvis::ZPLANE], ExposureMask,
			  &PltApp::PADoExposePicture, (XtPointer) Amrvis::ZPLANE);
    interfaceReady = true;
#endif
  }
}


// -------------------------------------------------------------------
void PltApp::StopAnimation() {
  if(animationIId) {
    XtRemoveTimeOut(animationIId);
    animationIId = 0;
  }
#if (BL_SPACEDIM != 3)
  for(int dim(0); dim < BL_SPACEDIM; ++dim) {
    if(XYplotwin[dim]) {
      XYplotwin[dim]->StopAnimation();
    }
  }
#endif
}


// -------------------------------------------------------------------
void PltApp::Animate(Amrvis::AnimDirection direction) {
  StopAnimation();
  animationIId = AddStaticTimeOut(frameSpeed, &PltApp::DoUpdateFrame);
  animDirection = direction;
#if (BL_SPACEDIM != 3)
  for(int dim(0); dim != BL_SPACEDIM; ++dim) {
    if(XYplotwin[dim]) {
      XYplotwin[dim]->InitializeAnimation(currentFrame, animFrames);
    }
  }
#endif
}


// -------------------------------------------------------------------
void PltApp::DirtyFrames() {
  paletteDrawn = false;
  for(int i(0); i < animFrames; ++i) {
    if(readyFrames[i] == true) {
      XDestroyImage(frameBuffer[i]);
    }
    readyFrames[i] = false;
  }
}


// -------------------------------------------------------------------
void PltApp::DoUpdateFrame(Widget, XtPointer, XtPointer) {
  if(animDirection == Amrvis::ANIMPOSDIR) {
    if(writingRGB) {
      DoCreateAnimRGBFile();
    }
    ++currentFrame;
    if(currentFrame == animFrames) {
      currentFrame = 0;
    }
  } else {
    --currentFrame;
    if(currentFrame < 0) {
      currentFrame = animFrames - 1;
    }
  }
  pltAppState->SetCurrentFrame(currentFrame);
  ShowFrame();
  XSync(display, false);
  animationIId = AddStaticTimeOut(frameSpeed, &PltApp::DoUpdateFrame);
}


// -------------------------------------------------------------------
void PltApp::ShowFrame() {
  interfaceReady = false;
  const AmrData &amrData = dataServicesPtr[currentFrame]->AmrDataRef();
  if( ! readyFrames[currentFrame] || datasetShowing || bSyncFrame ||
      UsingFileRange(currentRangeType))
  {
#if (BL_SPACEDIM != 3)
    AmrPicture *tempapSF = amrPicturePtrArray[Amrvis::ZPLANE];
    Vector<Box> domain = amrPicturePtrArray[Amrvis::ZPLANE]->GetSubDomain();
    XtRemoveEventHandler(wPlotPlane[Amrvis::ZPLANE], ExposureMask, false, 
			 (XtEventHandler) &PltApp::StaticEvent,
			 (XtPointer) tempapSF);
    delete tempapSF;
    
    Box fineDomain(domain[pltAppState->MaxAllowableLevel()]);
    fineDomain.refine(amrex::CRRBetweenLevels(pltAppState->MaxAllowableLevel(),
				                  amrData.FinestLevel(),
						  amrData.RefRatio()));
    amrPicturePtrArray[Amrvis::ZPLANE] = new AmrPicture(Amrvis::ZPLANE, gaPtr, fineDomain, 
						NULL, this,
						pltAppState,
						pltAppState->GetCGSmoothing());
    amrPicturePtrArray[Amrvis::ZPLANE]->SetRegion(startX, startY, endX, endY);
    
    amrPicturePtrArray[Amrvis::ZPLANE]->CreatePicture(XtWindow(wPlotPlane[Amrvis::ZPLANE]),
					      pltPaletteptr);
    AddStaticEventHandler(wPlotPlane[Amrvis::ZPLANE], ExposureMask,
			  &PltApp::PADoExposePicture, (XtPointer) Amrvis::ZPLANE);
    frameBuffer[currentFrame] = amrPicturePtrArray[Amrvis::ZPLANE]->GetPictureXImage();
#endif
    readyFrames[currentFrame] = true;
    paletteDrawn = ! UsingFileRange(currentRangeType);
    bSyncFrame = false;
  }
  
  XPutImage(display, XtWindow(wPlotPlane[Amrvis::ZPLANE]), xgc,
	    frameBuffer[currentFrame], 0, 0, 0, 0,
	    amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeH(),
	    amrPicturePtrArray[Amrvis::ZPLANE]->ImageSizeV());
  

  if(AVGlobals::CacheAnimFrames() == false) {
    XDestroyImage(frameBuffer[currentFrame]);
    readyFrames[currentFrame] = false;
  }


  string shortFileName(AVGlobals::StripSlashes(fileNames[currentFrame]));

  XmString xmFileString =
      XmStringCreateSimple(const_cast<char *>(shortFileName.c_str()));
  XtVaSetValues(wWhichFileLabel, XmNlabelString, xmFileString, NULL);
  XmStringFree(xmFileString);
  
  std::ostringstream tempTimeOut;
  tempTimeOut << "T = " << amrData.Time();
  XmString timeString =
      XmStringCreateSimple(const_cast<char *>(tempTimeOut.str().c_str()));
  XtVaSetValues(wWhichTimeLabel, XmNlabelString, timeString, NULL);
  XmStringFree(timeString);


  std::ostringstream headerout;
  headerout << shortFileName << "  " << tempTimeOut.str() << "  " << headerSuffix;
  XtVaSetValues(wAmrVisTopLevel,
                XmNtitle, const_cast<char *>(headerout.str().c_str()),
		NULL);

  XmScaleSetValue(wWhichFileScale, currentFrame);
  
  if(datasetShowing) {
    int hdir(-1), vdir(-1), sdir(-1);
    if(activeView == Amrvis::ZPLANE) {
      hdir = Amrvis::XDIR;
      vdir = Amrvis::YDIR;
      sdir = Amrvis::ZDIR;
    }
    if(activeView == Amrvis::YPLANE) {
      hdir = Amrvis::XDIR;
      vdir = Amrvis::ZDIR;
      sdir = Amrvis::YDIR;
    }
    if(activeView == Amrvis::XPLANE) {
      hdir = Amrvis::YDIR;
      vdir = Amrvis::ZDIR;
      sdir = Amrvis::XDIR;
    }
    datasetPtr->DatasetRender(trueRegion, amrPicturePtrArray[activeView],
                              this, pltAppState, hdir, vdir, sdir);
    datasetPtr->DoExpose(false);
  }
#if (BL_SPACEDIM != 3)
  for(int dim(0); dim < BL_SPACEDIM; ++dim) {
    if(XYplotwin[dim]) {
      XYplotwin[dim]->UpdateFrame(currentFrame);
    }
  }
#endif
}  // end ShowFrame


// -------------------------------------------------------------------
string PltApp::GetMPIFName(Real r) {
  int i(static_cast<int>(r));
  std::map<int, std::string>::iterator mfnIter = mpiFNames.find(i);
  if(mfnIter != mpiFNames.end()) {
    if(mfnIter->second == "NameTag") {
      int nt(lround((r - i) * nameTagMultiplier));
      return(mfnIter->second + "::" + nameTagNames[nt]);
    } else if(mfnIter->second == "Barrier") {
      int nt(lround((r - i) * barrierMultiplier));
      return(mfnIter->second + "::" + barrierNames[nt]);
    } else {
      return(mfnIter->second);
    }
  } else {
    return("Bad mpiFName value.");
  }
}


// -------------------------------------------------------------------
string PltApp::GetRegionName(Real r) {
  int i(static_cast<int>(r));
  std::map<int, std::string>::iterator regIter = regNames.find(i);
  if(regIter != regNames.end()) {
    return(regIter->second);
  } else {
    return("Bad regName value.");
  }
}


// -------------------------------------------------------------------
void PltApp::AddStaticCallback(Widget w, String cbtype, memberCB cbf, void *d) {
  CBData *cbs = new CBData(this, d, cbf);

  // cbdPtrs.push_back(cbs)
  int nSize(cbdPtrs.size());
  cbdPtrs.resize(nSize + 1);
  cbdPtrs[nSize] = cbs;

  XtAddCallback(w, cbtype, (XtCallbackProc ) &PltApp::StaticCallback,
		(XtPointer) cbs);
}


// -------------------------------------------------------------------
void PltApp::AddStaticEventHandler(Widget w, EventMask mask, memberCB cbf, void *d)
{
  CBData *cbs = new CBData(this, d, cbf);

  // cbdPtrs.push_back(cbs)
  int nSize(cbdPtrs.size());
  cbdPtrs.resize(nSize + 1);
  cbdPtrs[nSize] = cbs;

  XtAddEventHandler(w, mask, false, (XtEventHandler) &PltApp::StaticEvent,
		    (XtPointer) cbs);
}
 
 
// -------------------------------------------------------------------
XtIntervalId PltApp::AddStaticTimeOut(int time, memberCB cbf, void *d) {
  CBData *cbs = new CBData(this, d, cbf);

  // cbdPtrs.push_back(cbs)
  int nSize(cbdPtrs.size());
  cbdPtrs.resize(nSize + 1);
  cbdPtrs[nSize] = cbs;

  return XtAppAddTimeOut(appContext, time,
			 (XtTimerCallbackProc) &PltApp::StaticTimeOut,
			 (XtPointer) cbs);
}


// -------------------------------------------------------------------
void PltApp::StaticCallback(Widget w, XtPointer client_data, XtPointer call_data) {
  CBData *cbs = (CBData *) client_data;
  PltApp *obj = cbs->instance;
  (obj->*(cbs->cbFunc))(w, (XtPointer) cbs->data, call_data);
}


// -------------------------------------------------------------------
void PltApp::StaticEvent(Widget w, XtPointer client_data, XEvent *event, char*) {
  CBData *cbs = (CBData *) client_data;
  PltApp *obj = cbs->instance;
  (obj->*(cbs->cbFunc))(w, (XtPointer) cbs->data, (XtPointer) event);
}


// -------------------------------------------------------------------
void PltApp::StaticTimeOut(XtPointer client_data, XtIntervalId * call_data) {
  CBData *cbs = (CBData *) client_data;
  PltApp *obj = cbs->instance;
  (obj->*(cbs->cbFunc))(None, (XtPointer) cbs->data, (XtPointer) call_data);
}


// -------------------------------------------------------------------
int  PltApp::initialScale;
int  PltApp::initialMaxMenuItems = 20;
int  PltApp::defaultShowBoxes;
int  PltApp::initialWindowHeight;
int  PltApp::initialWindowWidth;
int  PltApp::placementOffsetX    = 0;
int  PltApp::placementOffsetY    = 0;
int  PltApp::reserveSystemColors = 50;
int  PltApp::extraPaletteWidth = 0;
string PltApp::defaultPaletteString;
string PltApp::defaultLightingFilename;
string PltApp::initialDerived;
string PltApp::initialFormatString;

bool  PltApp::PaletteDrawn()          { return PltApp::paletteDrawn;     }
int   PltApp::GetInitialScale()       { return PltApp::initialScale;     }
int   PltApp::GetDefaultShowBoxes()   { return PltApp::defaultShowBoxes; }
void  PltApp::PaletteDrawn(bool tOrF) { paletteDrawn = tOrF; }

void PltApp::SetDefaultPalette(const string &palString) {
  PltApp::defaultPaletteString = palString;
}

void PltApp::SetDefaultLightingFile(const string &lightFileString) {
  PltApp::defaultLightingFilename = lightFileString;
}

void PltApp::SetInitialDerived(const string &initialderived) {
  PltApp::initialDerived = initialderived;
}

void PltApp::SetInitialMaxMenuItems(int initMaxMenuItems) {
  PltApp::initialMaxMenuItems = initMaxMenuItems;
}

void PltApp::SetInitialScale(int initScale) {
  PltApp::initialScale = initScale;
}

void PltApp::SetInitialFormatString(const string &formatstring) {
  PltApp::initialFormatString = formatstring;
}

void PltApp::SetDefaultShowBoxes(int showboxes) {
  PltApp::defaultShowBoxes = showboxes;
}

void PltApp::SetInitialWindowHeight(int initWindowHeight) {
  PltApp::initialWindowHeight = initWindowHeight;
}

void PltApp::SetInitialWindowWidth(int initWindowWidth) {
  PltApp::initialWindowWidth = initWindowWidth;
}

void PltApp::SetReserveSystemColors(int reservesystemcolors) {
  reservesystemcolors = max(0, min(128, reservesystemcolors));  // arbitrarily
  PltApp::reserveSystemColors = reservesystemcolors;
}

void PltApp::SetExtraPaletteWidth(int extrapalettewidth) {
  extrapalettewidth = max(0, min(256, extrapalettewidth));  // arbitrarily
  PltApp::extraPaletteWidth = extrapalettewidth;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
