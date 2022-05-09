// ---------------------------------------------------------------
// AmrPicture.cpp
// ---------------------------------------------------------------


#include <AmrPicture.H>
#include <PltApp.H>
#include <PltAppState.H>
#include <Palette.H>
#include <AMReX_DataServices.H>
#include <ProjectionPicture.H>

using std::cout;
using std::cerr;
using std::endl;

using namespace amrex;

#include <ctime>

#ifdef BL_USE_ARRAYVIEW
#include <ArrayView.H>
#endif


std::string densityName("density");
static std::string dimNameBase[] = { "x", "y", "z" };
static std::string vecNameBase[] = { "vel", "_vel", "velocity", "_velocity",
                                     "mom", "_mom", "momentum", "_momentum" };
static bool vecIsMom[]           = { false, false, false, false,
                                     true, true, true, true };

// ---------------------------------------------------------------------
bool DrawRaster(Amrvis::ContourType cType) {
  return (cType == Amrvis::RASTERONLY || cType == Amrvis::RASTERCONTOURS || cType == Amrvis::VECTORS);
}


// ---------------------------------------------------------------------
bool DrawContours(Amrvis::ContourType cType) {
  return (cType == Amrvis::RASTERCONTOURS || cType == Amrvis::COLORCONTOURS || cType == Amrvis::BWCONTOURS);
}


// ---------------------------------------------------------------------
static bool UsingFileRange(const Amrvis::MinMaxRangeType rt) {
  return(rt == Amrvis::FILEGLOBALMINMAX    ||
         rt == Amrvis::FILESUBREGIONMINMAX ||
         rt == Amrvis::FILEUSERMINMAX);
}


// ---------------------------------------------------------------------
AmrPicture::AmrPicture(GraphicsAttributes *gaptr,
		       PltApp *pltappptr, PltAppState *pltappstateptr,
		       amrex::DataServices *dataservicesptr,
		       bool bcartgridsmoothing)
           : gaPtr(gaptr),
             pltAppPtr(pltappptr),
             pltAppStatePtr(pltappstateptr),
             dataServicesPtr(dataservicesptr),
             myView(Amrvis::XY),
             bCartGridSmoothing(bcartgridsmoothing),
             isSubDomain(false)
{
  int i, ilev;
  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());

  numberOfLevels    = maxAllowableLevel + 1;
  maxLevelWithGrids = maxAllowableLevel;

  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());

  subDomain.resize(numberOfLevels);
  for(ilev = 0; ilev <= maxAllowableLevel; ++ilev) {  // set type
    subDomain[ilev].convert(amrData.ProbDomain()[0].type());
  }

  subDomain[maxAllowableLevel].setSmall
		(amrData.ProbDomain()[maxAllowableLevel].smallEnd());
  subDomain[maxAllowableLevel].setBig
		(amrData.ProbDomain()[maxAllowableLevel].bigEnd());
  for(i = maxAllowableLevel - 1; i >= minDrawnLevel; --i) {
    subDomain[i] = subDomain[maxAllowableLevel];
    subDomain[i].coarsen(amrex::CRRBetweenLevels(i, maxAllowableLevel,
			 amrData.RefRatio()));
  }

  dataSizeH.resize(numberOfLevels);
  dataSizeV.resize(numberOfLevels);
  dataSize.resize(numberOfLevels);
  for(ilev = 0; ilev <= maxAllowableLevel; ++ilev) {
    dataSizeH[ilev] = subDomain[ilev].length(Amrvis::XDIR);
#if (BL_SPACEDIM == 1)
    dataSizeV[ilev] = 1;
#else
    dataSizeV[ilev] = subDomain[ilev].length(Amrvis::YDIR); 
#endif
    dataSize[ilev]  = dataSizeH[ilev] * dataSizeV[ilev];  // for a picture (slice).
  }

  if(AVGlobals::GivenInitialPlanes() || AVGlobals::GivenInitialPlanesReal()) {
    BL_ASSERT(BL_SPACEDIM == 3);
    IntVect initialplanes;
    if (AVGlobals::GivenInitialPlanes()) {
        initialplanes = AVGlobals::GetInitialPlanes();
    } else if (AVGlobals::GivenInitialPlanesReal()) {
        auto const location = AVGlobals::GetInitialPlanesReal();
        IntVect ivLoc;
        int ivLevel;
        amrData.IntVectFromLocation(pltAppStatePtr->FinestLevel(), location, ivLoc, ivLevel, initialplanes);
    }
    int coarsenCRR = amrex::CRRBetweenLevels(maxAllowableLevel,
                                                 pltAppStatePtr->FinestLevel(),
                                                 amrData.RefRatio());
    int tempSliceV = initialplanes[Amrvis::XDIR];  // at finest lev
    int tempSliceH = initialplanes[Amrvis::YDIR];  // at finest lev
    tempSliceV /= coarsenCRR;
    tempSliceH /= coarsenCRR;
    tempSliceH = subDomain[maxAllowableLevel].bigEnd(Amrvis::YDIR) - tempSliceH;
    vLine = amrex::max(std::min(tempSliceV,
                    subDomain[maxAllowableLevel].bigEnd(Amrvis::XDIR)), 
                    subDomain[maxAllowableLevel].smallEnd(Amrvis::XDIR));
    hLine = amrex::max(std::min(tempSliceH,
                    subDomain[maxAllowableLevel].bigEnd(Amrvis::YDIR)), 
                    subDomain[maxAllowableLevel].smallEnd(Amrvis::YDIR));
    vLine *= pltAppStatePtr->CurrentScale();
    hLine *= pltAppStatePtr->CurrentScale();
    subcutY = hLine;
    subcut2ndY = hLine;

    int tempSlice = initialplanes[Amrvis::YZ - myView];  // at finest lev
    tempSlice /= coarsenCRR;
    slice = amrex::max(std::min(tempSlice,
                    subDomain[maxAllowableLevel].bigEnd(Amrvis::YZ-myView)), 
                    subDomain[maxAllowableLevel].smallEnd(Amrvis::YZ-myView));
  } else {
    vLine = 0;
#if (BL_SPACEDIM == 1)
    hLine = 0;
#else
    hLine = subDomain[maxAllowableLevel].bigEnd(Amrvis::YDIR) *
	    pltAppStatePtr->CurrentScale();
#endif
    subcutY = hLine;
    subcut2ndY = hLine;

#if (BL_SPACEDIM != 3)
    slice = 0;
#else
    slice = subDomain[maxAllowableLevel].smallEnd(Amrvis::YZ-myView);
#endif
  }
  sliceBox.resize(numberOfLevels);

  for(ilev = minDrawnLevel; ilev <= maxAllowableLevel; ++ilev) {  // set type
    sliceBox[ilev].convert(subDomain[minDrawnLevel].type());
  }

  sliceBox[maxAllowableLevel] = subDomain[maxAllowableLevel];

  AmrPictureInit();
}  // end AmrPicture constructor


// ---------------------------------------------------------------------
AmrPicture::AmrPicture(int view, GraphicsAttributes *gaptr,
		       const Box &region,
                       PltApp *parentPltAppPtr,
		       PltApp *pltappptr, PltAppState *pltappstateptr,
		       bool bcartgridsmoothing)
	   : gaPtr(gaptr),
             pltAppPtr(pltappptr),
             pltAppStatePtr(pltappstateptr),
             myView(view),
             bCartGridSmoothing(bcartgridsmoothing),
             isSubDomain(true)
{
  BL_ASSERT(pltappptr != NULL);

  amrex::ignore_unused(parentPltAppPtr);

  int ilev;

  dataServicesPtr = pltAppPtr->GetDataServicesPtr();
  const AmrData &amrData = dataServicesPtr->AmrDataRef();

  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());
  numberOfLevels = maxAllowableLevel + 1;
  maxLevelWithGrids = maxAllowableLevel;
 
  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());

  subDomain.resize(numberOfLevels);
  Box sdBox(region);     // region is at the finestLevel
  subDomain[maxAllowableLevel] =
             sdBox.coarsen(amrex::CRRBetweenLevels(maxAllowableLevel,
			   pltAppStatePtr->FinestLevel(),
			   amrData.RefRatio()));

  for(ilev = maxAllowableLevel - 1; ilev >= minDrawnLevel; --ilev) {
    subDomain[ilev] = subDomain[maxAllowableLevel];
    subDomain[ilev].coarsen(amrex::CRRBetweenLevels(ilev,
                            maxAllowableLevel, amrData.RefRatio()));
  }

  dataSizeH.resize(numberOfLevels);
  dataSizeV.resize(numberOfLevels);
  dataSize.resize(numberOfLevels);
  for(ilev = 0; ilev <= maxAllowableLevel; ++ilev) {
    if(myView==Amrvis::XZ) {
      dataSizeH[ilev] = subDomain[ilev].length(Amrvis::XDIR);
      dataSizeV[ilev] = subDomain[ilev].length(Amrvis::ZDIR);
    } else if(myView==Amrvis::YZ) {
      dataSizeH[ilev] = subDomain[ilev].length(Amrvis::YDIR);
      dataSizeV[ilev] = subDomain[ilev].length(Amrvis::ZDIR);
    } else {
      dataSizeH[ilev] = subDomain[ilev].length(Amrvis::XDIR);
#if (BL_SPACEDIM == 1)
      dataSizeV[ilev] = 1;
#else
      dataSizeV[ilev] = subDomain[ilev].length(Amrvis::YDIR);
#endif
    }
    dataSize[ilev]  = dataSizeH[ilev] * dataSizeV[ilev];
  }

# if (BL_SPACEDIM == 3)
  if(myView==Amrvis::XY) {
      hLine = (subDomain[maxAllowableLevel].bigEnd(Amrvis::YDIR) -
      		subDomain[maxAllowableLevel].smallEnd(Amrvis::YDIR)) *
		pltAppStatePtr->CurrentScale();
      vLine = 0;
  } else if(myView==Amrvis::XZ) {
      hLine = (subDomain[maxAllowableLevel].bigEnd(Amrvis::ZDIR) -
      		subDomain[maxAllowableLevel].smallEnd(Amrvis::ZDIR)) *
		pltAppStatePtr->CurrentScale();
      vLine = 0;
  } else {
      hLine = (subDomain[maxAllowableLevel].bigEnd(Amrvis::ZDIR) -
      		subDomain[maxAllowableLevel].smallEnd(Amrvis::ZDIR)) *
		pltAppStatePtr->CurrentScale();
      vLine = 0;
  }

    if(parentPltAppPtr != NULL) {
      int tempSlice = parentPltAppPtr->GetAmrPicturePtr(myView)->GetSlice();
      tempSlice *= amrex::CRRBetweenLevels(parentPltAppPtr->GetPltAppState()->
				    MaxDrawnLevel(), 
                                    pltAppStatePtr->MaxDrawnLevel(),
                                    amrData.RefRatio());
      slice = amrex::max(std::min(tempSlice,
                      subDomain[maxAllowableLevel].bigEnd(Amrvis::YZ-myView)), 
                      subDomain[maxAllowableLevel].smallEnd(Amrvis::YZ-myView));
    } else {
      if(AVGlobals::GivenInitialPlanes() || AVGlobals::GivenInitialPlanesReal()) {
        IntVect initialplanes;
        if (AVGlobals::GivenInitialPlanes()) {
            initialplanes = AVGlobals::GetInitialPlanes();
        } else if (AVGlobals::GivenInitialPlanesReal()) {
            auto const location = AVGlobals::GetInitialPlanesReal();
            IntVect ivLoc;
            int ivLevel;
            amrData.IntVectFromLocation(pltAppStatePtr->FinestLevel(), location, ivLoc, ivLevel, initialplanes);
        }
        int tempSlice = initialplanes[Amrvis::YZ - myView];  // finest lev
        int coarsenCRR = amrex::CRRBetweenLevels(maxAllowableLevel,
                                                     pltAppStatePtr->FinestLevel(),
                                                     amrData.RefRatio());
        tempSlice /= coarsenCRR;
        slice = amrex::max(std::min(tempSlice,
                        subDomain[maxAllowableLevel].bigEnd(Amrvis::YZ-myView)), 
                        subDomain[maxAllowableLevel].smallEnd(Amrvis::YZ-myView));

        int tempSliceV, tempSliceH;
        if(myView==Amrvis::XY) {
          tempSliceV = initialplanes[Amrvis::XDIR];  // at finest lev
          tempSliceH = initialplanes[Amrvis::YDIR];  // at finest lev
          tempSliceV /= coarsenCRR;
          tempSliceH /= coarsenCRR;
          tempSliceH = subDomain[maxAllowableLevel].bigEnd(Amrvis::YDIR) - tempSliceH;
          vLine = amrex::max(std::min(tempSliceV,
                          subDomain[maxAllowableLevel].bigEnd(Amrvis::XDIR)), 
                          subDomain[maxAllowableLevel].smallEnd(Amrvis::XDIR));
          hLine = amrex::max(std::min(tempSliceH,
                          subDomain[maxAllowableLevel].bigEnd(Amrvis::YDIR)), 
                          subDomain[maxAllowableLevel].smallEnd(Amrvis::YDIR));
        } else if(myView==Amrvis::XZ) {
          tempSliceV = initialplanes[Amrvis::XDIR];  // at finest lev
          tempSliceH = initialplanes[Amrvis::ZDIR];  // at finest lev
          tempSliceV /= coarsenCRR;
          tempSliceH /= coarsenCRR;
          tempSliceH = subDomain[maxAllowableLevel].bigEnd(Amrvis::ZDIR) - tempSliceH;
          vLine = amrex::max(std::min(tempSliceV,
                          subDomain[maxAllowableLevel].bigEnd(Amrvis::XDIR)), 
                          subDomain[maxAllowableLevel].smallEnd(Amrvis::XDIR));
          hLine = amrex::max(std::min(tempSliceH,
                          subDomain[maxAllowableLevel].bigEnd(Amrvis::ZDIR)), 
                          subDomain[maxAllowableLevel].smallEnd(Amrvis::ZDIR));
        } else {
          tempSliceV = initialplanes[Amrvis::YDIR];  // at finest lev
          tempSliceH = initialplanes[Amrvis::ZDIR];  // at finest lev
          tempSliceV /= coarsenCRR;
          tempSliceH /= coarsenCRR;
          tempSliceH = subDomain[maxAllowableLevel].bigEnd(Amrvis::ZDIR) - tempSliceH;
          vLine = amrex::max(std::min(tempSliceV,
                          subDomain[maxAllowableLevel].bigEnd(Amrvis::YDIR)), 
                          subDomain[maxAllowableLevel].smallEnd(Amrvis::YDIR));
          hLine = amrex::max(std::min(tempSliceH,
                          subDomain[maxAllowableLevel].bigEnd(Amrvis::ZDIR)), 
                          subDomain[maxAllowableLevel].smallEnd(Amrvis::ZDIR));
        }
        vLine *= pltAppStatePtr->CurrentScale();
        hLine *= pltAppStatePtr->CurrentScale();
      } else {
        slice = subDomain[maxAllowableLevel].smallEnd(Amrvis::YZ - myView);
      }
    }
    
# else
    vLine = 0;
    hLine = 0;
    slice = 0;
# endif
    
  subcutY = hLine;
  subcut2ndY = hLine;

  sliceBox.resize(maxAllowableLevel + 1);
  sliceBox[maxAllowableLevel] = subDomain[maxAllowableLevel];

  AmrPictureInit();
}  // end AmrPicture constructor


// ---------------------------------------------------------------------
void AmrPicture::AmrPictureInit() {
  int iLevel;
  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());
  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());
  if(myView==Amrvis::XZ) {
    sliceBox[maxAllowableLevel].setSmall(Amrvis::YDIR, slice);
    sliceBox[maxAllowableLevel].setBig(Amrvis::YDIR, slice);
  } else if(myView==Amrvis::YZ) {
    sliceBox[maxAllowableLevel].setSmall(Amrvis::XDIR, slice);
    sliceBox[maxAllowableLevel].setBig(Amrvis::XDIR, slice);
  } else {  // Amrvis::XY
#   if (BL_SPACEDIM == 3)
      sliceBox[maxAllowableLevel].setSmall(Amrvis::ZDIR, slice);
      sliceBox[maxAllowableLevel].setBig(Amrvis::ZDIR, slice);
#   endif
  }

  CoarsenSliceBox();

  sliceFab.resize(numberOfLevels);
  for(iLevel = minDrawnLevel; iLevel <= maxAllowableLevel; ++iLevel) {
    sliceFab[iLevel] = new FArrayBox(sliceBox[iLevel], 1);
  }
  vfSliceFab.resize(numberOfLevels);  // always resize this
  if(dataServicesPtr->AmrDataRef().CartGrid()) {
    for(iLevel = minDrawnLevel; iLevel <= maxAllowableLevel; ++iLevel) {
      vfSliceFab[iLevel] = new FArrayBox(sliceBox[iLevel], 1);
    }
  }
  xImageArray.resize(numberOfLevels);

  display = gaPtr->PDisplay();
  xgc = gaPtr->PGC();

  // use maxAllowableLevel because all imageSizes are the same
  // regardless of which level is showing
  // dont use imageBox.length() because of node centered boxes
  imageSizeH = pltAppStatePtr->CurrentScale() * dataSizeH[maxAllowableLevel];
  imageSizeV = pltAppStatePtr->CurrentScale() * dataSizeV[maxAllowableLevel];
  if(imageSizeH > 65528 || imageSizeV > 65528) {  // not quite sizeof(short)
    // XImages cannot be larger than this
    cerr << "*** imageSizeH = " << imageSizeH << endl;
    cerr << "*** imageSizeV = " << imageSizeV << endl;
    amrex::Abort("Error in AmrPicture:  Image size too large.  Exiting.");
  }
  int widthpad = gaPtr->PBitmapPaddedWidth(imageSizeH);
  imageSize = imageSizeV * widthpad * gaPtr->PBytesPerPixel();

  imageData.resize(numberOfLevels);
  scaledImageData.resize(numberOfLevels);
  for(iLevel = 0; iLevel < minDrawnLevel; ++iLevel) {
    imageData[iLevel]       = nullptr;
    scaledImageData[iLevel] = nullptr;
  }
  for(iLevel = minDrawnLevel; iLevel <= maxAllowableLevel; ++iLevel) {
    imageData[iLevel] = new unsigned char[dataSize[iLevel]];
    BL_ASSERT(imageData[iLevel] != nullptr);
    scaledImageData[iLevel] = new unsigned char[imageSize];
    BL_ASSERT(scaledImageData[iLevel] != nullptr);
  }
  //scaledImageDataBodyMask = nullptr;

  pendingTimeOut = 0;
  frameSpeed = 300;
  hdspoint = 0;
  vdspoint = 0;
  dsBoxSize = 0;
  datasetPointShowing = false;
  datasetPointColor = 0;
  subCutShowing = false;
  subcutX = 0;
  subcut2ndX = 0;
  framesMade = false;
  if(myView == Amrvis::XZ) {
    hColor = AVGlobals::MaxPaletteIndex();
    vColor = 65;
  } else if(myView == Amrvis::YZ) {
    hColor = AVGlobals::MaxPaletteIndex();
    vColor = 220;
  } else {
    hColor = 220;
    vColor = 65;
  }
  pixMapCreated = false;

  SetSlice(myView, slice);

  int nVecNames(sizeof(vecNameBase)/sizeof(*vecNameBase));
  int nIsMom(sizeof(vecIsMom)/sizeof(*vecIsMom));
  if(nVecNames != nIsMom) {
    amrex::Abort("Error:  nVecNames != nIsMom.");
  }
  vecNames.resize(nVecNames);
  for(int ivn(0); ivn < vecNames.size(); ++ivn) {
    vecNames[ivn].resize(BL_SPACEDIM);
    for(int idim(0); idim < BL_SPACEDIM; ++idim) {
      vecNames[ivn][idim] = dimNameBase[idim] + vecNameBase[ivn];
    }
  }
}  // end AmrPictureInit()



// ---------------------------------------------------------------------
void AmrPicture::SetHVLine(int scale) {
  int maxDrawnLevel(pltAppStatePtr->MaxDrawnLevel());
  int first(0);
  for(int i(0); i <= Amrvis::YZ; ++i) {
    if(i == myView) {
      // do nothing
    } else {
      if(first == 0) {
        hLine = imageSizeV-1 -
		((pltAppPtr->GetAmrPicturePtr(i)->GetSlice() -
		  pltAppPtr->GetAmrPicturePtr(Amrvis::YZ - i)->
		  GetSubDomain()[maxDrawnLevel].smallEnd(Amrvis::YZ - i)) *
		  scale);
        first = 1;
      } else {
        vLine = ( pltAppPtr->GetAmrPicturePtr(i)->GetSlice() -
		  pltAppPtr->GetAmrPicturePtr(Amrvis::YZ - i)->
		  GetSubDomain()[maxDrawnLevel].smallEnd(Amrvis::YZ - i)) *
		  scale;
      }
    }
  }
}


// ---------------------------------------------------------------------
AmrPicture::~AmrPicture() {
  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());
  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());
  if(framesMade) {
    for(int isl(0); isl < subDomain[maxAllowableLevel].length(sliceDir); ++isl) {
      XDestroyImage(frameBuffer[isl]);
    }
  }
  for(int iLevel(minDrawnLevel); iLevel <= maxAllowableLevel; ++iLevel) {
    delete [] imageData[iLevel];
    //free(scaledImageData[iLevel]);
    delete [] scaledImageData[iLevel];
    delete sliceFab[iLevel];
    if(dataServicesPtr->AmrDataRef().CartGrid()) {
      delete vfSliceFab[iLevel];
    }
  }
  //delete [] scaledImageDataBodyMask;

  if(pixMapCreated) {
    XFreePixmap(display, pixMap);
  }
}


// ---------------------------------------------------------------------
void AmrPicture::SetSlice(int view, int here) {
  amrex::ignore_unused(here);
  int lev;
  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());
  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());
  sliceDir = Amrvis::YZ - view;
  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  BL_ASSERT(amrData.RefRatio().size() > 0);

# if (BL_SPACEDIM == 3)
    slice = here;
    sliceBox[maxAllowableLevel].setSmall(sliceDir, slice);
    sliceBox[maxAllowableLevel].setBig(sliceDir, slice);
    CoarsenSliceBox();
    for(lev = minDrawnLevel; lev <= maxAllowableLevel; ++lev) {
      if(sliceFab[lev]->box() != sliceBox[lev]) {
        if(sliceFab[lev]->box().numPts() != sliceBox[lev].numPts()) {
          delete [] imageData[lev];
	  cerr << endl;
	  cerr << "Suspicious sliceBox in AmrPicture::SetSlice" << endl;
	  cerr << "sliceFab[" << lev << "].box() = "
	       << sliceFab[lev]->box() << endl;
	  cerr << "sliceBox[" << lev << "]       = " << sliceBox[lev]       << endl;
	  cerr << endl;

          imageData[lev] = new unsigned char[sliceBox[lev].numPts()];
          BL_ASSERT(imageData[lev] != nullptr);
        }
        sliceFab[lev]->resize(sliceBox[lev], 1);
        if(dataServicesPtr->AmrDataRef().CartGrid()) {
          vfSliceFab[lev]->resize(sliceBox[lev], 1);
	}
      }
    }
# endif

  for(lev = minDrawnLevel; lev < gpArray.size(); ++lev) {
    gpArray[lev].clear();
  }
  gpArray.clear();

  gpArray.resize(numberOfLevels);
  maxLevelWithGrids = maxAllowableLevel;

  if(minDrawnLevel > maxAllowableLevel) {
    cerr << "**** Error:  minDrawnLevel > maxAllowableLevel = "
         << minDrawnLevel << "  " <<  maxAllowableLevel << endl;
    minDrawnLevel = maxAllowableLevel;
  }
  if(numberOfLevels < maxAllowableLevel + 1) {
    cerr << "*** Error:  bad index:  numberOfLevels  maxAllowableLevel = "
         << numberOfLevels << "  " << maxAllowableLevel << endl;
    numberOfLevels = maxAllowableLevel + 1;
  }
  Vector<int> nGrids(numberOfLevels);
  for(lev = minDrawnLevel; lev <= maxAllowableLevel; ++lev) {
    nGrids[lev] = amrData.NIntersectingGrids(lev, sliceBox[lev]);
    gpArray[lev].resize(nGrids[lev]);
    if(nGrids[lev] == 0 && maxLevelWithGrids == maxAllowableLevel) {
      maxLevelWithGrids = lev - 1;
    }
  }

  if(nGrids[minDrawnLevel] == 0) {
    cerr << "Error in AmrPicture::SetSlice:  No Grids intersected." << endl;
    cerr << "slice = " << slice << endl;
    cerr << "sliceBox[maxallowlev] = " << sliceBox[maxAllowableLevel] << endl;
    cerr << "maxAllowableLevel maxLevelWithGrids = " << maxAllowableLevel
	 << "  " << maxLevelWithGrids << endl;
    cerr << "subDomain[maxallowlev] = " << subDomain[maxAllowableLevel] << endl;
    return;
  }

  int gridNumber;
  for(lev = minDrawnLevel; lev <= maxLevelWithGrids; ++lev) {
    gridNumber = 0;
    for(int iBox(0); iBox < amrData.boxArray(lev).size(); ++iBox) {
      Box temp(amrData.boxArray(lev)[iBox]);
      if(sliceBox[lev].intersects(temp)) {
	temp &= sliceBox[lev];
	Box sliceDataBox(temp);
	temp.shift(Amrvis::XDIR, -subDomain[lev].smallEnd(Amrvis::XDIR));
#if (BL_SPACEDIM > 1)
	temp.shift(Amrvis::YDIR, -subDomain[lev].smallEnd(Amrvis::YDIR));
#endif
#if (BL_SPACEDIM == 3)
	  temp.shift(Amrvis::ZDIR, -subDomain[lev].smallEnd(Amrvis::ZDIR));
#endif
        gpArray[lev][gridNumber].GridPictureInit(lev,
		amrex::CRRBetweenLevels(lev, maxAllowableLevel,
		                            amrData.RefRatio()),
		pltAppStatePtr->CurrentScale(), imageSizeH, imageSizeV,
		temp, sliceDataBox, sliceDir);
        ++gridNumber;
      }
    }
  }
}  // end SetSlice(...)


// ---------------------------------------------------------------------
void AmrPicture::APChangeContour(Amrvis::ContourType prevCType) {
  Amrvis::ContourType cType(pltAppStatePtr->GetContourType());
  Real minUsing, maxUsing;
  pltAppStatePtr->GetMinMax(minUsing, maxUsing);
  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());
  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());
  FArrayBox *vffp;
  Real vfeps(0.0);

  if(DrawRaster(cType) != DrawRaster(prevCType)) {  // recreate the raster image
    AmrData &amrData = dataServicesPtr->AmrDataRef();
    for(int iLevel(minDrawnLevel); iLevel <= maxAllowableLevel; ++iLevel) {
      if(dataServicesPtr->AmrDataRef().CartGrid()) {
        vfeps = dataServicesPtr->AmrDataRef().VfEps(iLevel);
	vffp = vfSliceFab[iLevel];
      } else {
        vfeps = 0.0;
	vffp  = NULL;
      }
      CreateImage(*(sliceFab[iLevel]), imageData[iLevel],
 		  dataSizeH[iLevel], dataSizeV[iLevel],
 	          minUsing, maxUsing, palPtr,
		  vffp, vfeps);
      bool bCreateMask(iLevel == minDrawnLevel);
      CreateScaledImage(&(xImageArray[iLevel]), pltAppStatePtr->CurrentScale() *
                 amrex::CRRBetweenLevels(iLevel, maxAllowableLevel,
		                             amrData.RefRatio()),
                 imageData[iLevel], scaledImageData[iLevel],
                 dataSizeH[iLevel], dataSizeV[iLevel],
                 imageSizeH, imageSizeV, iLevel, bCreateMask);
    }
    if( ! pltAppPtr->PaletteDrawn()) {
      pltAppPtr->PaletteDrawn(true);
      palPtr->DrawPalette(minUsing, maxUsing, pltAppStatePtr->GetFormatString());
    }
  }
  int maxDrawnLevel(pltAppStatePtr->MaxDrawnLevel());
  APDraw(minDrawnLevel, maxDrawnLevel);
}


// ---------------------------------------------------------------------
void AmrPicture::DrawBoxes(Vector< Vector<GridPicture> > &gp, Drawable &drawable) {
  short xbox, ybox;
  unsigned short wbox, hbox;
  bool bIsWindow(true);
  bool bIsPixmap(false);
  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());
  int maxDrawnLevel(pltAppStatePtr->MaxDrawnLevel());

  if(pltAppStatePtr->GetShowingBoxes()) {
    for(int level(minDrawnLevel); level <= maxDrawnLevel; ++level) {
      if(level == minDrawnLevel) {
        XSetForeground(display, xgc, palPtr->AVWhitePixel());
      } else {
        XSetForeground(display, xgc,
	        palPtr->makePixel(palPtr->SafePaletteIndex(level, maxDrawnLevel)));
      }
      if(amrData.Terrain()) {
	DrawTerrBoxes(level, bIsWindow, bIsPixmap);
      } else {
        for(int i(0); i < gp[level].size(); ++i) {
	  xbox = gp[level][i].HPositionInPicture();
	  ybox = gp[level][i].VPositionInPicture();
	  wbox = gp[level][i].ImageSizeH(); 
	  hbox = gp[level][i].ImageSizeV(); 
          XDrawRectangle(display, drawable, xgc, xbox, ybox, wbox, hbox);
        }
      }
    }
  }
  // draw bounding box
  //XSetForeground(display, xgc, palPtr->WhiteIndex());
  //XDrawRectangle(display, drawable, xgc, 0, 0, imageSizeH-1, imageSizeV-1);
}


// ---------------------------------------------------------------------
void AmrPicture::DrawTerrBoxes(int /*level*/, bool /*bIsWindow*/, bool /*bIsPixmap*/) {
  cerr << endl;
  cerr << "***** Error:  should not be in AmrPicture::DrawTerrBoxes." << endl;
  cerr << "Continuing..." << endl;
  cerr << endl;
}


// ---------------------------------------------------------------------
void AmrPicture::APDraw(int /*fromLevel*/, int toLevel) {
  if( ! pixMapCreated) {
    pixMap = XCreatePixmap(display, pictureWindow,
			   imageSizeH, imageSizeV, gaPtr->PDepth());
    pixMapCreated = true;
  }  
 
  XPutImage(display, pixMap, xgc, xImageArray[toLevel], 0, 0, 0, 0,
	    imageSizeH, imageSizeV);
           
  Amrvis::ContourType cType(pltAppStatePtr->GetContourType());
  if(DrawContours(cType)) {
    DrawContour(sliceFab, display, pixMap, xgc);
  } else if(cType == Amrvis::VECTORS) {
    DrawVectorField(display, pixMap, xgc);
  }

  if( ! pltAppPtr->Animating()) {  // this should always be true
    DoExposePicture();
  }
}  // end AmrPicture::Draw.


// ---------------------------------------------------------------------
void AmrPicture::ToggleShowSubCut() {
  subCutShowing = (subCutShowing ? false : true);
  DoExposePicture();
}


// ---------------------------------------------------------------------
void AmrPicture::SetRegion(int startX, int startY, int endX, int endY) {
  regionX = startX; 
  regionY = startY; 
  region2ndX = endX; 
  region2ndY = endY; 
}


// ---------------------------------------------------------------------
void AmrPicture::SetSubCut(int startX, int startY, int endX, int endY) {
  if(startX != -1) {
    subcutX = startX; 
  }
  if(startY != -1) {
    subcutY = startY; 
  }
  if(endX != -1) {
    subcut2ndX = endX; 
  }
  if(endY != -1) {
    subcut2ndY = endY; 
  }
}


// ---------------------------------------------------------------------
void AmrPicture::DoExposePicture() {
  if(pltAppPtr->Animating()) {
    XPutImage(display, pictureWindow, xgc, pltAppPtr->CurrentFrameXImage(),
              0, 0, 0, 0, imageSizeH, imageSizeV);
  } else {
    if(pendingTimeOut == 0) {
      XCopyArea(display, pixMap, pictureWindow, 
 		xgc, 0, 0, imageSizeH, imageSizeV, 0, 0); 

      DrawBoxes(gpArray, pictureWindow);

      if( ! subCutShowing) {  // draw selected region
        XSetForeground(display, xgc, palPtr->makePixel(60));
        XDrawLine(display, pictureWindow, xgc,
		  regionX+1, regionY+1, region2ndX+1, regionY+1); 
        XDrawLine(display, pictureWindow, xgc,
		  regionX+1, regionY+1, regionX+1, region2ndY+1); 
        XDrawLine(display, pictureWindow, xgc,
		  regionX+1, region2ndY+1, region2ndX+1, region2ndY+1); 
        XDrawLine(display, pictureWindow, xgc,
		  region2ndX+1, regionY+1, region2ndX+1, region2ndY+1);

        XSetForeground(display, xgc, palPtr->makePixel(175));
        XDrawLine(display, pictureWindow, xgc,
		  regionX, regionY, region2ndX, regionY); 
        XDrawLine(display, pictureWindow, xgc,
		  regionX, regionY, regionX, region2ndY); 
        XDrawLine(display, pictureWindow, xgc,
		  regionX, region2ndY, region2ndX, region2ndY); 
        XDrawLine(display, pictureWindow, xgc,
		  region2ndX, regionY, region2ndX, region2ndY);
      }

#if (BL_SPACEDIM == 3)
      // draw plane "cutting" lines
      XSetForeground(display, xgc, palPtr->makePixel(hColor));
      XDrawLine(display, pictureWindow, xgc, 0, hLine, imageSizeH - 1, hLine); 
      XSetForeground(display, xgc, palPtr->makePixel(vColor));
      XDrawLine(display, pictureWindow, xgc, vLine, 0, vLine, imageSizeV - 1); 
      
      XSetForeground(display, xgc, palPtr->makePixel(hColor-30));
      XDrawLine(display, pictureWindow, xgc, 0, hLine+1, imageSizeH - 1, hLine+1); 
      XSetForeground(display, xgc, palPtr->makePixel(vColor-30));
      XDrawLine(display, pictureWindow, xgc, vLine+1, 0, vLine+1, imageSizeV - 1); 
      
      if(subCutShowing) {  // draw subvolume cutting border 
        XSetForeground(display, xgc, palPtr->makePixel(90));
        XDrawLine(display, pictureWindow, xgc,
		  subcutX+1, subcutY+1, subcut2ndX+1, subcutY+1); 
        XDrawLine(display, pictureWindow, xgc,
		  subcutX+1, subcutY+1, subcutX+1, subcut2ndY+1); 
        XDrawLine(display, pictureWindow, xgc,
		  subcutX+1, subcut2ndY+1, subcut2ndX+1, subcut2ndY+1); 
        XDrawLine(display, pictureWindow, xgc,
		  subcut2ndX+1, subcutY+1, subcut2ndX+1, subcut2ndY+1);
          
        XSetForeground(display, xgc, palPtr->makePixel(155));
        XDrawLine(display, pictureWindow, xgc,
		  subcutX, subcutY, subcut2ndX, subcutY); 
        XDrawLine(display, pictureWindow, xgc,
		  subcutX, subcutY, subcutX, subcut2ndY); 
        XDrawLine(display, pictureWindow, xgc,
		  subcutX, subcut2ndY, subcut2ndX, subcut2ndY); 
        XDrawLine(display, pictureWindow, xgc,
		  subcut2ndX, subcutY, subcut2ndX, subcut2ndY);
      }
#endif
    }
  }
}  // end DoExposePicture


// ---------------------------------------------------------------------
void AmrPicture::APChangeSlice(int here) {
  if(pendingTimeOut != 0) {
    DoStop();
  }
  SetSlice(myView, here);
  APMakeImages(palPtr);
}


// ---------------------------------------------------------------------
void AmrPicture::CreatePicture(Window drawPictureHere, Palette *palptr) {
  palPtr = palptr;
  pictureWindow = drawPictureHere;
  APMakeImages(palptr);
}


// ---------------------------------------------------------------------
void AmrPicture::APMakeImages(Palette *palptr) {
  BL_ASSERT(palptr != NULL);
  palPtr = palptr;
  AmrData &amrData = dataServicesPtr->AmrDataRef();
  if(UsingFileRange(pltAppStatePtr->GetMinMaxRangeType())) {
    pltAppPtr->PaletteDrawn(false);
  }

  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());
  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());
  if(framesMade) {
    for(int i(0); i < subDomain[maxAllowableLevel].length(sliceDir); ++i) {
      XDestroyImage(frameBuffer[i]);
    }
    framesMade = false;
  }
  if(pendingTimeOut != 0) {
    DoStop();
  }

  FArrayBox *vffp;
  Real vfeps(0.0);
  Real minUsing, maxUsing;
  pltAppStatePtr->GetMinMax(minUsing, maxUsing);
  VSHOWVAL(AVGlobals::Verbose(), minUsing)
  VSHOWVAL(AVGlobals::Verbose(), maxUsing)
  VSHOWVAL(AVGlobals::Verbose(), minDrawnLevel)
  VSHOWVAL(AVGlobals::Verbose(), maxAllowableLevel)

  const string currentDerived(pltAppStatePtr->CurrentDerived());
  const string vfracDerived("vfrac");
  for(int iLevel(minDrawnLevel); iLevel <= maxAllowableLevel; ++iLevel) {
    amrex::DataServices::Dispatch(amrex::DataServices::FillVarOneFab, dataServicesPtr,
		           (void *) (sliceFab[iLevel]),
			   (void *) (&(sliceFab[iLevel]->box())),
			   iLevel,
			   (void *) &currentDerived);
    if(amrData.CartGrid()) {
      BL_ASSERT(vfSliceFab[iLevel]->box() == sliceFab[iLevel]->box());
      amrex::DataServices::Dispatch(amrex::DataServices::FillVarOneFab, dataServicesPtr,
		             (void *) (vfSliceFab[iLevel]),
			     (void *) (&(vfSliceFab[iLevel]->box())),
			     iLevel,
			     (void *) &vfracDerived);
      vfeps = dataServicesPtr->AmrDataRef().VfEps(iLevel);
      vffp = vfSliceFab[iLevel];
    } else {
      vfeps = 0.0;
      vffp  = NULL;
    }
    CreateImage(*(sliceFab[iLevel]), imageData[iLevel],
 		dataSizeH[iLevel], dataSizeV[iLevel],
 	        minUsing, maxUsing, palPtr, vffp, vfeps);
    bool bCreateMask(iLevel == minDrawnLevel);
    CreateScaledImage(&(xImageArray[iLevel]), pltAppStatePtr->CurrentScale() *
                amrex::CRRBetweenLevels(iLevel, maxAllowableLevel,
		amrData.RefRatio()),
                imageData[iLevel], scaledImageData[iLevel],
                dataSizeH[iLevel], dataSizeV[iLevel],
                imageSizeH, imageSizeV, iLevel, bCreateMask);
  }
  if( ! pltAppPtr->PaletteDrawn()) {
    pltAppPtr->PaletteDrawn(true);
    palptr->DrawPalette(minUsing, maxUsing, pltAppStatePtr->GetFormatString());
  }
  int maxDrawnLevel(pltAppStatePtr->MaxDrawnLevel());
  APDraw(minDrawnLevel, maxDrawnLevel);

}  // end AP Make Images


// -------------------------------------------------------------------
// convert Real to char in imagedata from fab
void AmrPicture::CreateImage(const FArrayBox &fab, unsigned char *imagedata,
			     int datasizeh, int datasizev,
			     Real globalMin, Real globalMax, Palette *palptr,
			     const FArrayBox *vfracFab, const Real vfeps)
{
  int jdsh, jtmp1;
  int dIndex, iIndex;
  Real oneOverGDiff;
  if((globalMax - globalMin) < FLT_MIN) {
    oneOverGDiff = 0.0;
  } else {
    oneOverGDiff = 1.0 / (globalMax - globalMin);
  }
  const Real *dataPoint = fab.dataPtr();
  bool bCartGrid(dataServicesPtr->AmrDataRef().CartGrid());
  const Real *vfDataPoint = 0;
  if(bCartGrid) {
    BL_ASSERT(vfracFab != NULL);
    vfDataPoint = vfracFab->dataPtr();
  }
      
  
  // flips the image in Vert dir: j => datasizev-j-1
  if(DrawRaster(pltAppStatePtr->GetContourType())) {
    Real dPoint;
    int paletteStart(palptr->PaletteStart());
    int paletteEnd(palptr->PaletteEnd());
    int colorSlots(palptr->ColorSlots());
    int csm1(colorSlots - 1);
    if(bCartGrid == false || (bCartGrid && AVGlobals::GetShowBody() == false)) {
      for(int j(0); j < datasizev; ++j) {
        jdsh = j * datasizeh;
        jtmp1 = (datasizev-j-1) * datasizeh;
        for(int i(0); i < datasizeh; ++i) {
          dIndex = i + jtmp1;
          dPoint = dataPoint[dIndex];
          iIndex = i + jdsh;
          if(dPoint > globalMax) {  // clip
            imagedata[iIndex] = paletteEnd;
          } else if(dPoint < globalMin) {  // clip
            imagedata[iIndex] = paletteStart;
          } else {
            imagedata[iIndex] = (unsigned char)
              ((((dPoint - globalMin) * oneOverGDiff) * csm1) );
              //  ^^^^^^^^^^^^^^^^^^ Real data
            imagedata[iIndex] += paletteStart;
          } 
        }
      }
    } else {  // mask the body
      int bodyColor(palptr->BlackIndex());
      Real dVFPoint;
      for(int j(0); j < datasizev; ++j) {
        jdsh = j * datasizeh;
        jtmp1 = (datasizev-j-1) * datasizeh;
        for(int i(0); i < datasizeh; ++i) {
          dIndex = i + jtmp1;
          dPoint = dataPoint[dIndex];
          dVFPoint = vfDataPoint[dIndex];
          iIndex = i + jdsh;
          if(dPoint > globalMax) {  // clip
            imagedata[iIndex] = paletteEnd;
          } else if(dPoint < globalMin) {  // clip
            imagedata[iIndex] = paletteStart;
          } else {
            imagedata[iIndex] = (unsigned char)
              ((((dPoint - globalMin) * oneOverGDiff) * csm1) );
              //  ^^^^^^^^^^^^^^^^^^ Real data
            imagedata[iIndex] += paletteStart;
          } 
          if(dVFPoint < vfeps) {  // set to body color
            imagedata[iIndex] = bodyColor;
	  }
        }
      }
    }

  } else {

    if(AVGlobals::LowBlack()) {
      int blackIndex(palptr->BlackIndex());
      for(int i(0); i < (datasizeh * datasizev); ++i) {
        imagedata[i] = blackIndex;
      }
    } else {
      int whiteIndex(palptr->WhiteIndex());
      for(int i(0); i < (datasizeh * datasizev); ++i) {
        imagedata[i] = whiteIndex;
      }
    }
  }
}  // end CreateImage(...)


// ---------------------------------------------------------------------
void AmrPicture::CreateScaledImage(XImage **ximage, int scale,
				   unsigned char *imagedata,
				   unsigned char *scaledimagedata,
				   int datasizeh, int datasizev,
				   int imagesizeh, int imagesizev,
				   int level, bool bCreateMask)
{ 
  int widthpad = gaPtr->PBitmapPaddedWidth(imagesizeh);
  *ximage = XCreateImage(display, gaPtr->PVisual(),
		gaPtr->PDepth(), ZPixmap, 0, (char *) scaledimagedata,
		widthpad, imagesizev,
		XBitmapPad(display), widthpad * gaPtr->PBytesPerPixel());

  if( ! bCartGridSmoothing) {
    for(int j(0); j < imagesizev; ++j) {
      int jtmp(datasizeh * (j/scale));
      for(int i(0); i < widthpad; ++i) {
        int itmp(i / scale);
        unsigned char imm1(imagedata[ itmp + jtmp ]);
        XPutPixel(*ximage, i, j, palPtr->makePixel(imm1));
      }
    }

  } else {  // bCartGridSmoothing

    int i, j, ii, jj, rrcs, iis;
    int iMDL(pltAppStatePtr->MaxDrawnLevel());
    int blackIndex = palPtr->BlackIndex();
    int bodyColor = blackIndex;
    const AmrData &amrData = dataServicesPtr->AmrDataRef();
    Real vfeps = amrData.VfEps(iMDL);
    Real *vfracPoint = vfSliceFab[level]->dataPtr();
    Real vfp, omvfe = 1.0 - vfeps;
    int vidx, svidx;
    Vector<Real> stencil(9, -3.0);
    int nBodyCells, nScaledImageCells;

    rrcs = scale;
    nScaledImageCells = rrcs*rrcs;

    Vector<Real> sumH(3, 0.0), sumV(3, 0.0);
    Vector<Real> diffAvgV(3, 0.0), diffAvgH(3, 0.0);
    Real smallValue(0.000001);
    Real avgV, avgH;
    Real normV, normH;
    int  isum, jsum, nStartV, nEndV, nStartH, nEndH, tempSum;
    int nV, nH, stencilSize = 3;
    int nCalcBodyCells, fluidCell = 1, bodyCell = 0, markedCell = -1;
    Real cellDx = 1.0/((Real) rrcs), cellDy = 1.0/((Real) rrcs);
    Real yBody, slope;
    int iCurrent, jCurrent, jBody;
    int isIndex;

    Vector<int> imageStencil(nScaledImageCells, -5000);

    int dataSizeHMDL(datasizeh), dataSizeVMDL(datasizev);

  if(bCreateMask || scaledImageDataBodyMask.size() != (imagesizeh * imagesizev)) {

    scaledImageDataBodyMask.resize(imagesizeh * imagesizev);

    for(j = 0; j < imagesizev; ++j) {
      for(i = 0; i < imagesizeh; ++i) {
	int index(i + (imagesizev-1-j)*imagesizeh);
        scaledImageDataBodyMask[index] = 1;  // ---- init to fluid cell
      }
    }

    for(j = 0; j < dataSizeVMDL; ++j) {
      for(i = 0; i < dataSizeHMDL; ++i) {
        vidx = i + (dataSizeVMDL-1-j)*dataSizeHMDL;  // get volfrac for cell(i,j)
        vfp = vfracPoint[vidx];

        if(vfp > vfeps && vfp < omvfe) {  // ---- a mixed cell

          for(iis = 0; iis < 9; ++iis) {
            stencil[iis] = -2.0*vfeps;  // flag value for boundary stencils
          }

          // ---- fill the stencil with volume fractions
          svidx = (i-1) + (dataSizeVMDL-1-(j-1))*dataSizeHMDL;  // up left
          if((i-1) >= 0 && (dataSizeVMDL-1-(j-1)) < dataSizeVMDL) {
            stencil[0] = vfracPoint[svidx];
          }
          svidx = (i  ) + (dataSizeVMDL-1-(j-1))*dataSizeHMDL;  // up
          if((dataSizeVMDL-1-(j-1)) < dataSizeVMDL) {
            stencil[1] = vfracPoint[svidx];
          }
          svidx = (i+1) + (dataSizeVMDL-1-(j-1))*dataSizeHMDL;  // up right
          if((i+1) < dataSizeHMDL && (dataSizeVMDL-1-(j-1)) < dataSizeVMDL) {
            stencil[2] = vfracPoint[svidx];
          }
          svidx = (i-1) + (dataSizeVMDL-1-(j  ))*dataSizeHMDL;  // left
          if((i-1) >= 0) {
            stencil[3] = vfracPoint[svidx];
          }
          stencil[4] = vfp;                                     // the center
          svidx = (i+1) + (dataSizeVMDL-1-(j  ))*dataSizeHMDL;  // right
          if((i+1) < dataSizeHMDL) {
            stencil[5] = vfracPoint[svidx];
          }
          svidx = (i-1) + (dataSizeVMDL-1-(j+1))*dataSizeHMDL;  // down left
          if((i-1) >= 0 && ((int)(dataSizeVMDL-1-(j+1))) >= 0) {
            stencil[6] = vfracPoint[svidx];
          }
          svidx = (i  ) + (dataSizeVMDL-1-(j+1))*dataSizeHMDL;  // down
          if(((int)(dataSizeVMDL-1-(j+1))) >= 0) {
            stencil[7] = vfracPoint[svidx];
          }
          svidx = (i+1) + (dataSizeVMDL-1-(j+1))*dataSizeHMDL;  // down right
          if((i+1) < dataSizeHMDL && ((int)(dataSizeVMDL-1-(j+1))) >= 0) {
            stencil[8] = vfracPoint[svidx];
          }

#if (BL_SPACEDIM==3)
	  bool allMixed(true);
	  for(int ist(0); ist < 9; ++ist) {
	    if( ! (stencil[ist] > vfeps && stencil[ist] < omvfe)) {
	      allMixed = false;
	    }
	  }
	  if(allMixed) {
	    continue;
	  }
#endif

#if (BL_SPACEDIM==2)
#ifdef AV_CGS_FIXSLNC
          // fix for straight lines near corners
          Real flagValue(-2.0 * vfeps);
          if(fabs(stencil[4] - stencil[3]) < smallValue &&
	     fabs(stencil[4] - stencil[5]) > smallValue)
	  {
            stencil[2] = flagValue;
            stencil[5] = flagValue;
            stencil[8] = flagValue;
          }
          if(fabs(stencil[4] - stencil[5]) < smallValue &&
	     fabs(stencil[4] - stencil[3]) > smallValue)
	  {
            stencil[0] = flagValue;
            stencil[3] = flagValue;
            stencil[6] = flagValue;
          }
          if(fabs(stencil[4] - stencil[1]) < smallValue &&
	     fabs(stencil[4] - stencil[7]) > smallValue)
	  {
            stencil[6] = flagValue;
            stencil[7] = flagValue;
            stencil[8] = flagValue;
          }
          if(fabs(stencil[4] - stencil[7]) < smallValue &&
	          fabs(stencil[4] - stencil[1]) > smallValue)
	  {
            stencil[0] = flagValue;
            stencil[1] = flagValue;
            stencil[2] = flagValue;
          }
#endif
#endif

          // ---- there should be this many body cells calculated
          nBodyCells = (int) ((1.0-vfp)*nScaledImageCells);

          nStartV = 0;
          nEndV   = 2;
          nStartH = 0;
          nEndH   = 2;
          if(stencil[0] < smallValue && stencil[1] < smallValue && stencil[2] < smallValue) {
            ++nStartH;
          }
          if(stencil[0] < smallValue && stencil[3] < smallValue && stencil[6] < smallValue) {
            ++nStartV;
          }
          if(stencil[2] < smallValue && stencil[5] < smallValue && stencil[8] < smallValue) {
            --nEndV;
          }
          if(stencil[6] < smallValue && stencil[7] < smallValue && stencil[8] < smallValue) {
            --nEndH;
          }

          nV = nEndV - nStartV + 1;
          nH = nEndH - nStartH + 1;

          for(jsum=nStartH; jsum<=nEndH; ++jsum) {
            sumH[jsum] = 0;
            for(isum=nStartV; isum<=nEndV; ++isum) {
              sumH[jsum] += stencil[isum + stencilSize*jsum];
            }
          }
          for(isum=nStartV; isum<=nEndV; ++isum) {
            sumV[isum] = 0;
            for(jsum=nStartH; jsum<=nEndH; ++jsum) {
              sumV[isum] += stencil[isum + stencilSize*jsum];
            }
          }

          tempSum = 0;
          for(isum=nStartV; isum<=nEndV; ++isum) {
            tempSum += int( sumV[isum] );
          }
          avgV = tempSum / ((Real) nV);
          tempSum = 0;
          for(isum=nStartH; isum<=nEndH; ++isum) {
            tempSum += int( sumH[isum] );
          }
          avgH = tempSum / ((Real) nH);

          for(isum=nStartV; isum<=nEndV; ++isum) {
            diffAvgV[isum] = sumV[isum] - avgV;
          }
          for(isum=nStartH; isum<=nEndH; ++isum) {
            diffAvgH[isum] = sumH[isum] - avgH;
          }

          //for(isum=nStartV; isum< nEndV; ++isum) {
            //minDAV = std::min(diffAvgV[isum], diffAvgV[isum+1]);
            //maxDAV = std::max(diffAvgV[isum], diffAvgV[isum+1]);
          //}
          //for(isum=nStartH; isum<=nEndH; ++isum) {
            //minDAH = std::min(diffAvgH[isum], diffAvgH[isum+1]);
            //maxDAH = std::max(diffAvgH[isum], diffAvgH[isum+1]);
          //}
          normH =  ((diffAvgV[nEndV] - diffAvgV[nStartV]) * ((Real) nH));
                                                  // nH is correct here
          normV = -((diffAvgH[nEndH] - diffAvgH[nStartH]) * ((Real) nV));
                                                  // nV is correct here

          for(ii = 0; ii < rrcs * rrcs; ++ii) {
            imageStencil[ii] = fluidCell;
          }
          if(fabs(normV) > smallValue) {
            slope = normH/normV;  // perpendicular to normal
          } else {
            slope = normH;  // avoid divide by zero
          }

          if(normV > smallValue && normH > smallValue) {  // -------- upper right quadrant
            iCurrent = 0;
            jCurrent = 0;
            nCalcBodyCells = 0;
            while(nCalcBodyCells < nBodyCells) {
              ++iCurrent;
              if(iCurrent > rrcs) {
                iCurrent = 1;
                ++jCurrent;
              }
              if(jCurrent > rrcs) {
                break;
              }
              for(ii = 0; ii < iCurrent; ++ii) {
                yBody = (slope * ((iCurrent-ii)*cellDx)) + (jCurrent*cellDy);
                jBody = std::min(rrcs-1, (int) (yBody/cellDy));
                for(jj = 0; jj <= jBody; ++jj) {
                  isIndex = ii + ((rrcs - (jj + 1)) * rrcs);
                  imageStencil[isIndex] = bodyCell;  // yflip
                }
              }
              // sum the body cells
              // do it this way to allow redundant setting of body cells
              nCalcBodyCells = 0;
              for(ii = 0; ii < rrcs * rrcs; ++ii) {
                if(imageStencil[ii] == bodyCell) {
                  ++nCalcBodyCells;
                }
              }
            }  // end while(...)

          } else if(normV < -smallValue && normH < -smallValue) {    // -------- lower left quadrant
            iCurrent = rrcs;
            jCurrent = rrcs;
            nCalcBodyCells = 0;
            while(nCalcBodyCells < nBodyCells) {
              --iCurrent;
              if(iCurrent < 0) {
                iCurrent = rrcs-1;
                --jCurrent;
              }
              if(jCurrent < 1) {
                break;
              }
              for(ii = rrcs; ii > iCurrent; --ii) {
                yBody = (slope * ((ii - iCurrent) * cellDx)) +
                        ((rrcs - jCurrent) * cellDy);
                jBody = amrex::max(0, (int) (rrcs - (yBody / cellDy)));
                for(jj = jBody; jj < rrcs; ++jj) {
                  isIndex = (ii - 1) + ((rrcs - (jj + 1)) * rrcs);
                  imageStencil[isIndex] = bodyCell;  // yflip
                }
              }
              // sum the body cells
              nCalcBodyCells = 0;
              for(ii = 0; ii < rrcs * rrcs; ++ii) {
                if(imageStencil[ii] == bodyCell) { ++nCalcBodyCells; }
              }
            }  // end while(...)

          } else if(normV > smallValue && normH < -smallValue) {     // -------- upper left quadrant
            iCurrent = rrcs;
            jCurrent = 0;
            nCalcBodyCells = 0;
            while(nCalcBodyCells < nBodyCells) {
              --iCurrent;
              if(iCurrent < 0) {
                iCurrent = rrcs-1;
                ++jCurrent;
              }
              if(jCurrent > rrcs) {
                break;
              }
              for(ii=rrcs; ii>iCurrent; --ii) {
                yBody = (-slope * ((ii - iCurrent) * cellDx)) + (jCurrent * cellDy);
                jBody = std::min(rrcs - 1, (int) (yBody / cellDy));
                for(jj = 0; jj <= jBody; ++jj) {
                  isIndex = (ii - 1) + ((rrcs - (jj + 1)) * rrcs);
                  imageStencil[isIndex] = bodyCell;  // yflip
                }
              }
              // sum the body cells
              nCalcBodyCells = 0;
              for(ii = 0; ii < rrcs * rrcs; ++ii) {
                if(imageStencil[ii] == bodyCell) { ++nCalcBodyCells; }
              }
            }  // end while(...)

          } else if(normV < -smallValue && normH > smallValue) {     // -------- lower right quadrant
            iCurrent = 0;
            jCurrent = rrcs;
            nCalcBodyCells = 0;
            while(nCalcBodyCells < nBodyCells) {
              ++iCurrent;
              if(iCurrent > rrcs) {
                iCurrent = 1;
                --jCurrent;
              }
              if(jCurrent < 1) {
                break;
              }
              for(ii = 0; ii < iCurrent; ++ii) {
                yBody = (-slope * ((iCurrent - ii) * cellDx)) +
                        ((rrcs - jCurrent) * cellDy);
                jBody = amrex::max(0, (int) (rrcs - (yBody / cellDy)));
                for(jj = jBody; jj < rrcs; ++jj) {
                  isIndex = ii + ((rrcs - (jj + 1)) * rrcs);  // yflip
                  imageStencil[isIndex] = bodyCell;
                }
              }
              // sum the body cells
              nCalcBodyCells = 0;
              for(ii = 0; ii < rrcs * rrcs; ++ii) {
                if(imageStencil[ii] == bodyCell) { ++nCalcBodyCells; }
              }
            }  // end while(...)

          } else if(fabs(normV) < smallValue) {  // -------- vertical face
            if(normH > smallValue) {  // body is on the left edge of the cell
              for(jj = 0; jj < rrcs; ++jj) {
                for(ii = 0; ii < (nBodyCells / rrcs); ++ii) {
                  isIndex = ii + ((rrcs-(jj+1))*rrcs);
                  imageStencil[isIndex] = bodyCell;
                }
              }
            } else {           // body is on the right edge of the cell
              for(jj = 0; jj < rrcs; ++jj) {
                for(ii = rrcs - (nBodyCells / rrcs); ii < rrcs; ++ii) {
                  isIndex = ii + ((rrcs-(jj+1))*rrcs);
                  imageStencil[isIndex] = bodyCell;
                }
              }
            }

          } else if(fabs(normH) < smallValue) {  // -------- horizontal face
            if(normV > smallValue) {  // body is on the bottom edge of the cell
              for(jj = 0; jj < (nBodyCells / rrcs); ++jj) {
                for(ii = 0; ii < rrcs; ++ii) {
                  isIndex = ii + ((rrcs - (jj + 1)) * rrcs);
                  imageStencil[isIndex] = bodyCell;
                }
              }
            } else {           // body is on the top edge of the cell
              for(jj = rrcs - (nBodyCells / rrcs); jj < rrcs; ++jj) {
                for(ii = 0; ii < rrcs; ++ii) {
                  isIndex = ii + ((rrcs - (jj + 1)) * rrcs);
                  imageStencil[isIndex] = bodyCell;
                }
              }
            }

          } else {

            for(ii = 0; ii < rrcs * rrcs; ++ii) {
              imageStencil[ii] = markedCell;
            }

          }  // end long if block

          // set cells to correct color
          for(jj = 0; jj < rrcs; ++jj) {
            for(ii = 0; ii < rrcs; ++ii) {
              if(imageStencil[ii + (jj * rrcs)] == fluidCell) {
              } else if(imageStencil[ii + (jj*rrcs)] == markedCell) {
              } else if(imageStencil[ii + (jj*rrcs)] == bodyCell) {
                scaledImageDataBodyMask[((i*rrcs)+ii)+(((j*rrcs)+jj)*imageSizeH)] = 0;
              } else {  // undefined
		amrex::Abort("undefined stencil value.");
              }
            }  // end for(ii...)
          }  // end for(jj...)

        } else {  // ---- non mixed cell
	  // ---- do nothing here for the scaledImageDataBodyMask
        }

      }  // end for(i...)
    }  // end for(j...)

  }  // ---- end create body mask

    // ---- fill with image data
    for(int jjss(0); jjss < imagesizev; ++jjss) {
      int jtmp(datasizeh * (jjss/scale));
      for(int iiss(0); iiss < widthpad; ++iiss) {
        int itmp(iiss / scale);
        unsigned char imm1(imagedata[ itmp + jtmp ]);
        XPutPixel(*ximage, iiss, jjss, palPtr->makePixel(imm1));
      }
    }
    // ---- mask the smoothed body cells
    for(int jjss(0); jjss < imagesizev; ++jjss) {
      for(int iiss(0); iiss < imagesizeh; ++iiss) {
	int index(iiss + (imagesizev-1-jjss)*imagesizeh);
        if(scaledImageDataBodyMask[index] == 0) {
	  int iii(iiss), jjj(imagesizev-1-jjss);
	  XPutPixel(*ximage, iii, jjj, palPtr->makePixel(bodyColor));
	}
      }
    }

  }  // end if(bCartGridSmoothing)

}


// ---------------------------------------------------------------------
void AmrPicture::APChangeScale(int newScale, int previousScale) { 
  int iLevel;
  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());
  int maxDrawnLevel(pltAppStatePtr->MaxDrawnLevel());
  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());
  if(framesMade) {
    for(int i(0); i < subDomain[maxAllowableLevel].length(sliceDir); ++i) {
      XDestroyImage(frameBuffer[i]);
    }
    framesMade = false;
  }
  if(pendingTimeOut != 0) {
    DoStop();
  }
  imageSizeH = newScale   * dataSizeH[maxAllowableLevel];
  imageSizeV = newScale   * dataSizeV[maxAllowableLevel];
  int widthpad = gaPtr->PBitmapPaddedWidth(imageSizeH);
  imageSize  = widthpad * imageSizeV * gaPtr->PBytesPerPixel();
  XClearWindow(display, pictureWindow);

  if(pixMapCreated) {
    XFreePixmap(display, pixMap);
  }  
  pixMap = XCreatePixmap(display, pictureWindow,
			   imageSizeH, imageSizeV, gaPtr->PDepth());
  pixMapCreated = true;

  for(iLevel = minDrawnLevel; iLevel <= maxAllowableLevel; ++iLevel) {
   for(int grid(0); grid < gpArray[iLevel].size(); ++grid) {
     gpArray[iLevel][grid].ChangeScale(newScale, imageSizeH, imageSizeV);
   }
  }

  AmrData &amrData = dataServicesPtr->AmrDataRef();

  for(iLevel = minDrawnLevel; iLevel <= maxAllowableLevel; ++iLevel) {
    bool bCreateMask(iLevel == minDrawnLevel);
    delete [] scaledImageData[iLevel];
    scaledImageData[iLevel] = new unsigned char[imageSize];
    BL_ASSERT(scaledImageData[iLevel] != nullptr);
    CreateScaledImage(&xImageArray[iLevel], newScale *
                amrex::CRRBetweenLevels(iLevel, maxAllowableLevel,
		amrData.RefRatio()),
                imageData[iLevel], scaledImageData[iLevel],
                dataSizeH[iLevel], dataSizeV[iLevel],
                imageSizeH, imageSizeV, iLevel, bCreateMask);
  }

  hLine = ((hLine / previousScale) * newScale) + (newScale - 1);
  vLine = ((vLine / previousScale) * newScale) + (newScale - 1);
  APDraw(minDrawnLevel, maxDrawnLevel);
}


// ---------------------------------------------------------------------
void AmrPicture::APChangeLevel() { 
  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());
  if(framesMade) {
    for(int i(0); i < subDomain[maxAllowableLevel].length(sliceDir); ++i) {
      XDestroyImage(frameBuffer[i]);
    }
    framesMade = false;
  }
  if(pendingTimeOut != 0) {
    DoStop();
  }
  XClearWindow(display, pictureWindow);

  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());
  int maxDrawnLevel(pltAppStatePtr->MaxDrawnLevel());
  APDraw(minDrawnLevel, maxDrawnLevel);
}


// ---------------------------------------------------------------------
XImage *AmrPicture::GetPictureXImage(const bool bdrawboxesintoimage) {
  int xbox, ybox, wbox, hbox;
  XImage *ximage;

  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());
  int maxDrawnLevel(pltAppStatePtr->MaxDrawnLevel());
  int maxDataLevel(pltAppStatePtr->MaxAllowableLevel());
  if(pltAppStatePtr->GetShowingBoxes() && bdrawboxesintoimage) {
    for(int level(minDrawnLevel); level <= maxDrawnLevel; ++level) {
      if(level == minDrawnLevel) {
        XSetForeground(display, xgc, palPtr->WhiteIndex());
      } else {
        XSetForeground(display, xgc,
		palPtr->makePixel(palPtr->SafePaletteIndex(level, maxDataLevel)));
      }
      for(int i(0); i < gpArray[level].size(); ++i) {
	xbox = gpArray[level][i].HPositionInPicture();
	ybox = gpArray[level][i].VPositionInPicture();
	wbox = gpArray[level][i].ImageSizeH(); 
	hbox = gpArray[level][i].ImageSizeV(); 

        XDrawRectangle(display, pixMap, xgc, xbox, ybox, wbox, hbox);
      }
    }
  }
  //XSetForeground(display, xgc, palPtr->makePixel(AVGlobals::MaxPaletteIndex()));
  //XDrawRectangle(display, pixMap, xgc, 0, 0, imageSizeH-1, imageSizeV-1);

  ximage = XGetImage(display, pixMap, 0, 0,
		imageSizeH, imageSizeV, AllPlanes, ZPixmap);

  if(pixMapCreated) {
    XFreePixmap(display, pixMap);
  }  
  pixMap = XCreatePixmap(display, pictureWindow,
			   imageSizeH, imageSizeV, gaPtr->PDepth());
  pixMapCreated = true;
  APDraw(minDrawnLevel, maxDrawnLevel);
  return ximage;
}


// ---------------------------------------------------------------------
void AmrPicture::GetGridBoxes(Vector< Vector<GridBoxes> > &gb,
                              const int minlev, const int maxlev)
{
  gb.resize(maxlev + 1);  // resize from zero
  for(int level(minlev); level <= maxlev; ++level) {
    gb[level].resize(gpArray[level].size());
    for(int i(0); i < gpArray[level].size(); ++i) {
      gb[level][i].xbox = gpArray[level][i].HPositionInPicture();
      gb[level][i].ybox = gpArray[level][i].VPositionInPicture();
      gb[level][i].wbox = gpArray[level][i].ImageSizeH(); 
      gb[level][i].hbox = gpArray[level][i].ImageSizeV(); 
    }
  }
}


// ---------------------------------------------------------------------
void AmrPicture::CoarsenSliceBox() {
  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());
  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());
  for(int i(maxAllowableLevel - 1); i >= minDrawnLevel; --i) {
    sliceBox[i] = sliceBox[maxAllowableLevel];
    sliceBox[i].coarsen(amrex::CRRBetweenLevels(i, maxAllowableLevel,
			dataServicesPtr->AmrDataRef().RefRatio()));
  }
}


// ---------------------------------------------------------------------
void AmrPicture::CreateFrames(Amrvis::AnimDirection direction) {
  char buffer[Amrvis::BUFSIZE];
  bool cancelled(false);
  int islice(0), i, j, lev, gridNumber;
  Vector<int> intersectGrids;
  int maxLevelWithGridsHere;
  int posneg(1);
  if(direction == Amrvis::ANIMNEGDIR) {
    posneg = -1;
  }
  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());
  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());

  sprintf(buffer, "Creating frames..."); 
  PrintMessage(buffer);
  Vector<Box> interBox(numberOfLevels);
  interBox[maxAllowableLevel] = subDomain[maxAllowableLevel];
  int start = subDomain[maxAllowableLevel].smallEnd(sliceDir);
  int length = subDomain[maxAllowableLevel].length(sliceDir);
  if(framesMade) {
    for(int ii(0); ii < subDomain[maxAllowableLevel].length(sliceDir); ++ii) {
      XDestroyImage(frameBuffer[ii]);
    }
  }
  frameGrids.resize(length); 
  frameBuffer.resize(length);
  int maxDrawnLevel(pltAppStatePtr->MaxDrawnLevel());
  unsigned char *frameImageData = new unsigned char[dataSize[maxDrawnLevel]];
  int iEnd(0);
  for(i = 0; i < length; ++i) {
    islice = (((slice - start + (posneg * i)) + length) % length);
			          // ^^^^^^^^ + length for negative values
    intersectGrids.resize(maxAllowableLevel + 1);
    interBox[maxAllowableLevel].setSmall(sliceDir, start+islice);
    interBox[maxAllowableLevel].setBig(sliceDir, start+islice);
    for(j = maxAllowableLevel - 1; j >= minDrawnLevel; --j) {
      interBox[j] = interBox[maxAllowableLevel];
      interBox[j].coarsen(amrex::CRRBetweenLevels(j, maxAllowableLevel,
			  amrData.RefRatio()));
    }
    for(lev = minDrawnLevel; lev <= maxAllowableLevel; ++lev) {
      intersectGrids[lev] = amrData.NIntersectingGrids(lev, interBox[lev]);
    }
    maxLevelWithGridsHere = maxDrawnLevel;
    frameGrids[islice].resize(maxLevelWithGridsHere + 1); 
    for(lev = minDrawnLevel; lev <= maxLevelWithGridsHere; ++lev) {
      frameGrids[islice][lev].resize(intersectGrids[lev]);
    } 

    for(lev = minDrawnLevel; lev <= maxLevelWithGridsHere; ++lev) {
      gridNumber = 0;
      for(int iBox(0); iBox < amrData.boxArray(lev).size(); ++iBox) {
	Box temp(amrData.boxArray(lev)[iBox]);
        if(interBox[lev].intersects(temp)) {
	  temp &= interBox[lev];
          Box sliceDataBox(temp);
	  temp.shift(Amrvis::XDIR, -subDomain[lev].smallEnd(Amrvis::XDIR));
	  temp.shift(Amrvis::YDIR, -subDomain[lev].smallEnd(Amrvis::YDIR));
          temp.shift(Amrvis::ZDIR, -subDomain[lev].smallEnd(Amrvis::ZDIR));
          frameGrids[islice][lev][gridNumber].GridPictureInit(lev,
                  amrex::CRRBetweenLevels(lev, maxAllowableLevel,
		  amrData.RefRatio()),
                  pltAppStatePtr->CurrentScale(), imageSizeH, imageSizeV,
                  temp, sliceDataBox, sliceDir);
          ++gridNumber;
        }
      }
    }   // end for(lev...)

    // get the data for this slice
    const string currentDerived(pltAppStatePtr->CurrentDerived());
    FArrayBox imageFab(interBox[maxDrawnLevel], 1);
    amrex::DataServices::Dispatch(amrex::DataServices::FillVarOneFab, dataServicesPtr,
			   (void *) &imageFab,
			   (void *) (&(imageFab.box())),
			   maxDrawnLevel,
                           (void *) &currentDerived);

    Real minUsing, maxUsing;
    pltAppStatePtr->GetMinMax(minUsing, maxUsing);

    FArrayBox *vffp = NULL;
    Real vfeps(0.0);
    const string vfracDerived("vfrac");
    if(amrData.CartGrid()) {
      vffp = new FArrayBox(interBox[maxDrawnLevel], 1);
      amrex::DataServices::Dispatch(amrex::DataServices::FillVarOneFab, dataServicesPtr,
		             (void *) (vffp),
			     (void *) (&(vffp->box())),
			     maxDrawnLevel,
			     (void *) &vfracDerived);
      vfeps = dataServicesPtr->AmrDataRef().VfEps(maxDrawnLevel);
    } else {
      vfeps = 0.0;
      vffp  = NULL;
    }
    CreateImage(imageFab, frameImageData,
		dataSizeH[maxDrawnLevel], dataSizeV[maxDrawnLevel],
                minUsing, maxUsing, palPtr, vffp, vfeps);

    // this cannot be deleted because it belongs to the XImage
    unsigned char *frameScaledImageData;
    frameScaledImageData = (unsigned char *) malloc(imageSize);

    bool bCreateMask(true);

    CreateScaledImage(&(frameBuffer[islice]), pltAppStatePtr->CurrentScale() *
           amrex::CRRBetweenLevels(maxDrawnLevel, maxAllowableLevel,
	   amrData.RefRatio()),
           frameImageData, frameScaledImageData,
           dataSizeH[maxDrawnLevel], dataSizeV[maxDrawnLevel],
           imageSizeH, imageSizeV, maxDrawnLevel, bCreateMask);

    ShowFrameImage(islice);
#if (BL_SPACEDIM == 3)
    
    if( ! framesMade) {
      pltAppPtr->GetProjPicturePtr()->ChangeSlice(Amrvis::YZ-sliceDir, islice+start);
      pltAppPtr->GetProjPicturePtr()->MakeSlices();
      XClearWindow(XtDisplay(pltAppPtr->GetWTransDA()),
                   XtWindow(pltAppPtr->GetWTransDA()));
      pltAppPtr->DoExposeTransDA();
    }
    XEvent event;
    if(XCheckMaskEvent(display, ButtonPressMask, &event)) {
      if(event.xany.window == XtWindow(pltAppPtr->GetStopButtonWidget())) {
        XPutBackEvent(display, &event);
        cancelled = true;
        iEnd = i;
        break;
      }
    }
#endif
  }  // end for(i=0; ...)

  delete [] frameImageData;

  if(cancelled) {
    for(int iiss(0); iiss <= iEnd; ++iiss) {
      int iDestroySlice((((slice - start + (posneg * iiss)) + length) % length));
      XDestroyImage(frameBuffer[iDestroySlice]);
    }
    framesMade = false;
    sprintf(buffer, "Cancelled.\n"); 
    PrintMessage(buffer);
    APChangeSlice(start+islice);
    AmrPicture *apXY = pltAppPtr->GetAmrPicturePtr(Amrvis::XY);
    AmrPicture *apXZ = pltAppPtr->GetAmrPicturePtr(Amrvis::XZ);
    AmrPicture *apYZ = pltAppPtr->GetAmrPicturePtr(Amrvis::YZ);
    if(sliceDir == Amrvis::XDIR) {
      apXY->SetVLine((slice - subDomain[maxAllowableLevel].smallEnd(Amrvis::XDIR)) *
                                 pltAppStatePtr->CurrentScale());
      apXY->DoExposePicture();
      apXZ->SetVLine((slice - subDomain[maxAllowableLevel].smallEnd(Amrvis::XDIR)) *
                                  pltAppStatePtr->CurrentScale());
      apXZ->DoExposePicture();
    }
    if(sliceDir == Amrvis::YDIR) {
      apXY->SetHLine(apXY->ImageSizeV()-1 - (slice -
				  subDomain[maxAllowableLevel].smallEnd(Amrvis::YDIR)) *
                       		  pltAppStatePtr->CurrentScale());
      apXY->DoExposePicture();
      apYZ->SetVLine((slice - subDomain[maxAllowableLevel].smallEnd(Amrvis::YDIR)) *
                                  pltAppStatePtr->CurrentScale());
      apYZ->DoExposePicture();
    }
    if(sliceDir == Amrvis::ZDIR) {
      apYZ->SetHLine(apYZ->ImageSizeV()-1 - (slice -
				  subDomain[maxAllowableLevel].smallEnd(Amrvis::ZDIR)) *
				  pltAppStatePtr->CurrentScale());
      apYZ->DoExposePicture();
      apXZ->SetHLine(apXZ->ImageSizeV()-1 - (slice -
				  subDomain[maxAllowableLevel].smallEnd(Amrvis::ZDIR)) *
                                  pltAppStatePtr->CurrentScale());
      apXZ->DoExposePicture();
    }

  } else {
    framesMade = true;
    sprintf(buffer, "Done.\n"); 
    PrintMessage(buffer);
  }
  DoExposePicture();

}  // end CreateFrames


// ---------------------------------------------------------------------
void AmrPicture::DrawDatasetPoint(int hplot, int vplot, int size) {
  hdspoint = hplot;
  vdspoint = vplot;
  dsBoxSize = size;
  int hpoint = hdspoint * pltAppStatePtr->CurrentScale();
  int vpoint = imageSizeV-1 - vdspoint * pltAppStatePtr->CurrentScale();
  int side = dsBoxSize * pltAppStatePtr->CurrentScale();
  XDrawRectangle(display, pictureWindow, pltAppPtr->GetRbgc(),
            hpoint, vpoint, side, side);
}


// ---------------------------------------------------------------------
void AmrPicture::UnDrawDatasetPoint() {
  int hpoint = hdspoint * pltAppStatePtr->CurrentScale();
  int vpoint = imageSizeV-1 - vdspoint * pltAppStatePtr->CurrentScale();
  int side = dsBoxSize * pltAppStatePtr->CurrentScale();
  XDrawRectangle(display, pictureWindow, pltAppPtr->GetRbgc(),
            hpoint, vpoint, side, side);
}


// ---------------------------------------------------------------------
void AmrPicture::CBFrameTimeOut(XtPointer client_data, XtIntervalId *) {
  ((AmrPicture *) client_data)->DoFrameUpdate();
}


// ---------------------------------------------------------------------
void AmrPicture::CBContourSweep(XtPointer client_data, XtIntervalId *) {
  ((AmrPicture *) client_data)->DoContourSweep();
}


// ---------------------------------------------------------------------
void AmrPicture::DoFrameUpdate() {
  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());
  if(sweepDirection == Amrvis::ANIMPOSDIR) {
    if(slice < subDomain[maxAllowableLevel].bigEnd(sliceDir)) {
      ++slice;
    } else {
      slice = subDomain[maxAllowableLevel].smallEnd(sliceDir);
    }
  } else {
    if(slice > subDomain[maxAllowableLevel].smallEnd(sliceDir)) {
      --slice;
    } else {
      slice = subDomain[maxAllowableLevel].bigEnd(sliceDir);
    }
  } 
  BL_ASSERT(DrawContours(pltAppStatePtr->GetContourType()) == false);
  int iRelSlice(slice - subDomain[maxAllowableLevel].smallEnd(sliceDir));
  ShowFrameImage(iRelSlice);
  XSync(display, false);
  pendingTimeOut = XtAppAddTimeOut(pltAppPtr->GetAppContext(), frameSpeed,
			   (XtTimerCallbackProc) &AmrPicture::CBFrameTimeOut,
                           (XtPointer) this);
}  // end DoFrameUpdate()


// ---------------------------------------------------------------------
void AmrPicture::DoContourSweep() {
  if(sweepDirection == Amrvis::ANIMPOSDIR) {
    pltAppPtr->DoForwardStep(myView);
  } else {
    pltAppPtr->DoBackStep(myView);
  } 
  pendingTimeOut = XtAppAddTimeOut(pltAppPtr->GetAppContext(), frameSpeed,
			   (XtTimerCallbackProc) &AmrPicture::CBContourSweep,
                           (XtPointer) this);
}


// ---------------------------------------------------------------------
void AmrPicture::DoStop() {
  if(pendingTimeOut != 0) {
    XtRemoveTimeOut(pendingTimeOut);
    pendingTimeOut = 0;
    APChangeSlice(slice);
  }
}


// ---------------------------------------------------------------------
void AmrPicture::Sweep(Amrvis::AnimDirection direction) {
  if(pendingTimeOut != 0) {
    DoStop();
  }
  if(DrawContours(pltAppStatePtr->GetContourType()) == true) {
    pendingTimeOut = XtAppAddTimeOut(pltAppPtr->GetAppContext(), frameSpeed,
				(XtTimerCallbackProc) &AmrPicture::CBContourSweep,
                                (XtPointer) this);
    sweepDirection = direction;
    return; 
  }
  if( ! framesMade) {
    CreateFrames(direction);
  }
  if(framesMade) {
    pendingTimeOut = XtAppAddTimeOut(pltAppPtr->GetAppContext(), frameSpeed,
				(XtTimerCallbackProc) &AmrPicture::CBFrameTimeOut,
				(XtPointer) this);
    sweepDirection = direction;
  }
}


// ---------------------------------------------------------------------
void AmrPicture::DrawSlice(int /*iSlice*/) {
  XDrawLine(display, pictureWindow, pltAppPtr->GetRbgc(),
            0, 30, imageSizeH, 30);
}


// ---------------------------------------------------------------------
void AmrPicture::ShowFrameImage(int iSlice) {
  AmrPicture *apXY = pltAppPtr->GetAmrPicturePtr(Amrvis::XY);
  AmrPicture *apXZ = pltAppPtr->GetAmrPicturePtr(Amrvis::XZ);
  AmrPicture *apYZ = pltAppPtr->GetAmrPicturePtr(Amrvis::YZ);
  int iRelSlice(iSlice);

  XPutImage(display, pictureWindow, xgc,
            frameBuffer[iRelSlice], 0, 0, 0, 0, imageSizeH, imageSizeV);

  DrawBoxes(frameGrids[iRelSlice], pictureWindow);

  if(DrawContours(pltAppStatePtr->GetContourType()) == true) {
    DrawContour(sliceFab, display, pictureWindow, xgc);
  }


  // draw plane "cutting" lines
  XSetForeground(display, xgc, palPtr->makePixel(hColor));
  XDrawLine(display, pictureWindow, xgc, 0, hLine, imageSizeH, hLine); 
  XSetForeground(display, xgc, palPtr->makePixel(vColor));
  XDrawLine(display, pictureWindow, xgc, vLine, 0, vLine, imageSizeV); 
  XSetForeground(display, xgc, palPtr->makePixel(hColor-30));
  XDrawLine(display, pictureWindow, xgc, 0, hLine+1, imageSizeH, hLine+1); 
  XSetForeground(display, xgc, palPtr->makePixel(vColor-30));
  XDrawLine(display, pictureWindow, xgc, vLine+1, 0, vLine+1, imageSizeV); 
  
  if(sliceDir == Amrvis::XDIR) {
    apXY->SetVLine(iRelSlice * pltAppStatePtr->CurrentScale());
    apXY->DoExposePicture();
    apXZ->SetVLine(iRelSlice * pltAppStatePtr->CurrentScale());
    apXZ->DoExposePicture();
  }
  if(sliceDir == Amrvis::YDIR) {
    apXY->SetHLine(apXY->ImageSizeV() - 1 - iRelSlice * pltAppStatePtr->CurrentScale());
    apXY->DoExposePicture();
    apYZ->SetVLine(iRelSlice * pltAppStatePtr->CurrentScale());
    apYZ->DoExposePicture();
  }
  if(sliceDir == Amrvis::ZDIR) {
    apYZ->SetHLine(apYZ->ImageSizeV() - 1 - iRelSlice * pltAppStatePtr->CurrentScale());
    apYZ->DoExposePicture();
    apXZ->SetHLine(apXZ->ImageSizeV() - 1 - iRelSlice * pltAppStatePtr->CurrentScale());
    apXZ->DoExposePicture();
  }

#if (BL_SPACEDIM == 3)
  if(framesMade) {
      for(int nP(0); nP < 3; ++nP)
          pltAppPtr->GetProjPicturePtr()->
              ChangeSlice(nP, pltAppPtr->GetAmrPicturePtr(nP)->GetSlice());
      pltAppPtr->GetProjPicturePtr()->MakeSlices();
      XClearWindow(XtDisplay(pltAppPtr->GetWTransDA()), 
                   XtWindow(pltAppPtr->GetWTransDA()));
      pltAppPtr->DoExposeTransDA();
  }
#endif

  pltAppPtr->DoExposeRef();
}  // end ShowFrameImage()


// ---------------------------------------------------------------------
void AmrPicture::DrawContour(Vector<FArrayBox *> passedSliceFab,
                             Display *passed_display, 
                             Drawable &passedPixMap, 
                             const GC &passedGC)
{
  Real v_min, v_max;
  pltAppStatePtr->GetMinMax(v_min, v_max);
  Real v_off;
  if(pltAppStatePtr->GetNumContours() != 0) {
    v_off = v_min + 0.5 * (v_max - v_min) / (Real) pltAppStatePtr->GetNumContours();
  } else {
    v_off = 1.0;
  }
  int hDir, vDir;
  if(myView == Amrvis::XZ) {
    hDir = Amrvis::XDIR;
    vDir = Amrvis::ZDIR;
  } else if(myView == Amrvis::YZ) {
    hDir = Amrvis::YDIR;
    vDir = Amrvis::ZDIR;
  } else {
    hDir = Amrvis::XDIR;
    vDir = Amrvis::YDIR;
  }
  
  const AmrData &amrData = dataServicesPtr->AmrDataRef();

  Vector<Real> pos_low(BL_SPACEDIM);
  Vector<Real> pos_high(BL_SPACEDIM);
  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());
  int maxDrawnLevel(pltAppStatePtr->MaxDrawnLevel());
  amrData.LoNodeLoc(maxDrawnLevel, passedSliceFab[maxDrawnLevel]->smallEnd(), 
                    pos_low);
  amrData.HiNodeLoc(maxDrawnLevel, passedSliceFab[maxDrawnLevel]->bigEnd(), 
                    pos_high);
  
  Real xlft(pos_low[hDir]);
  Real ybot(pos_low[vDir]);
  Real xrgt(pos_high[hDir]);
  Real ytop(pos_high[vDir]);

  for(int lev(minDrawnLevel); lev <= maxDrawnLevel; ++lev) {
    const bool *mask_array(NULL);
    bool mask_bool(false);
    BaseFab<bool> mask(passedSliceFab[lev]->box());
    mask.setVal(false);
    
    if(lev != maxDrawnLevel) {
      int lratio(amrex::CRRBetweenLevels(lev, lev + 1, amrData.RefRatio()));
      // construct mask array.  must be size FAB.
      const BoxArray &nextFinest = amrData.boxArray(lev+1);
      for(int j(0); j < nextFinest.size(); ++j) {
        Box coarseBox(amrex::coarsen(nextFinest[j],lratio));
        if(coarseBox.intersects(passedSliceFab[lev]->box())) {
          coarseBox &= passedSliceFab[lev]->box();
          mask.setVal(true,coarseBox,0);
        }
      }
    }
    
    // get rid of the complement
    const BoxArray &levelBoxArray = amrData.boxArray(lev);
    BoxArray complement = amrex::complementIn(passedSliceFab[lev]->box(), 
                                               levelBoxArray);
    for(int i(0); i < complement.size(); ++i) {
      mask.setVal(true, complement[i], 0);
    }
    
    mask_array = mask.dataPtr();
    mask_bool = true;
    
    int paletteEnd(palPtr->PaletteEnd());
    int paletteStart(palPtr->PaletteStart());
    int csm1(palPtr->ColorSlots() - 1);
    Real oneOverGDiff;
    Real minUsing, maxUsing;
#ifdef AV_ALTCONTOUR
    Box fabBox(passedSliceFab[maxDrawnLevel]->box());
    FArrayBox altDerFab(fabBox, 1);
    string whichAltDerived("pressure");
    AmrData &amrData = dataServicesPtr->AmrDataRef();
    amrex::DataServices::Dispatch(amrex::DataServices::FillVarOneFab, dataServicesPtr,
                           (void *) &altDerFab,
                           (void *) (&(altDerFab.box())),
                           maxDrawnLevel,
                           (void *) &whichAltDerived);
    amrData.MinMax(fabBox, whichAltDerived, maxDrawnLevel, minUsing, maxUsing);
cout << "(((((((((((((((( using minmax = " << minUsing << "  " << maxUsing << endl;
SHOWVAL(fabBox);
SHOWVAL(whichAltDerived);
SHOWVAL(maxDrawnLevel);
//minUsing = 1.37e-11;
//maxUsing = 3.96e-09;
    passedSliceFab[lev]->copy(altDerFab);

#else
    pltAppStatePtr->GetMinMax(minUsing, maxUsing);
#endif
    if((maxUsing - minUsing) < FLT_MIN) {
      oneOverGDiff = 0.0;
    } else {
      oneOverGDiff = 1.0 / (maxUsing - minUsing);
    }

    int drawColor;
    if(AVGlobals::LowBlack()) {
      drawColor = palPtr->WhiteIndex();
    } else {
      drawColor = palPtr->BlackIndex();
    }
    for(int icont(0); icont < pltAppStatePtr->GetNumContours(); ++icont) {
      Real frac((Real) icont / pltAppStatePtr->GetNumContours());
      Real value(v_off + frac * (v_max - v_min));
      if(pltAppStatePtr->GetContourType() == Amrvis::COLORCONTOURS) {
        if(value > maxUsing) {  // clip
          drawColor = paletteEnd;
        } else if(value < minUsing) {  // clip
          drawColor = paletteStart;
        } else {
          drawColor = (int) ((((value - minUsing) * oneOverGDiff) * csm1) );
                       //    ^^^^^^^^^^^^^^^^^ Real data
          drawColor += paletteStart;
        }
      }
      DrawContour(*(passedSliceFab[lev]), value, mask_bool, mask_array,
              passed_display, passedPixMap, passedGC, drawColor,
              passedSliceFab[lev]->box().length(hDir), 
              passedSliceFab[lev]->box().length(vDir),
              xlft, ybot, xrgt, ytop);
    }
  } // loop over levels
}


// contour plotting
// ---------------------------------------------------------------------
bool AmrPicture::DrawContour(const FArrayBox &fab, Real value,
                        bool has_mask, const bool *mask,
                        Display *sdisplay, Drawable &dPixMap, const GC &gc,
			int FGColor, int xLength, int yLength,
                        Real leftEdge, Real bottomEdge, 
                        Real rightEdge, Real topEdge)
{
  // data     = data to be contoured
  // value    = value to contour
  // has_mask = true if mask array available
  // mask     = array of mask values.  will not contour in masked off cells
  // xLength       = dimension of arrays in X direction
  // yLength       = dimension of arrays in Y direction
  // leftEdge     = position of left   edge of grid in domain
  // rightEdge     = position of right  edge of grid in domain
  // bottomEdge     = position of bottom edge of grid in domain
  // topEdge     = position of top    edge of grid in domain
  const Real *data = fab.dataPtr();
  Real xDiff((rightEdge - leftEdge) / (xLength - 1));
  Real yDiff((topEdge - bottomEdge) / (yLength - 1));
  
  bool left, right, bottom, top;    // does contour line intersect this side?
  Real xLeft(0.0), yLeft(0.0);      // where contour line intersects left side
  Real xRight(0.0), yRight(0.0);    // where contour line intersects right side
  Real xBottom(0.0), yBottom(0.0);  // where contour line intersects bottom side
  Real xTop(0.0), yTop(0.0);        // where contour line intersects top side
  bool bFailureStatus(false);
  for(int j(0); j < yLength - 1; ++j) {
    for(int i(0); i < xLength - 1; ++i) {
      if(has_mask) {
        if(mask[(i)   + (j)   * xLength] ||
           mask[(i+1) + (j)   * xLength] ||
           mask[(i+1) + (j+1) * xLength] ||
           mask[(i)   + (j+1) * xLength])
	{
          continue;
        }
      }
      Real leftBottom(data[(i) + (j) * xLength]);         // left bottom value
      Real leftTop(data[(i) + (j+1) * xLength]);          // left top value
      Real rightBottom(data[(i+1) + (j) * xLength]);      // right bottom value
      Real rightTop(data[(i+1) + (j+1) * xLength]);       // right top value
      xLeft   = leftEdge + xDiff*(i);
      xRight  = xLeft + xDiff;
      yBottom = bottomEdge + yDiff * j;
      yTop    = yBottom + yDiff;
      
      left   = Between(leftBottom, value, leftTop);
      right  = Between(rightBottom, value, rightTop);
      bottom = Between(leftBottom, value, rightBottom);
      top    = Between(leftTop, value, rightTop);
      
      // figure out where things intersect the cell
      if(left) {
        if(leftBottom != leftTop) {
          yLeft = yBottom + yDiff * (value - leftBottom) / (leftTop-leftBottom);
        } else {
          yLeft = yBottom;
          bFailureStatus = true;
        }
      }
      if(right) {
        if(rightBottom != rightTop) {
          yRight = yBottom + yDiff * (value-rightBottom) / (rightTop-rightBottom);
        } else {
          yRight = yBottom;
          bFailureStatus = true;
        }
      }
      if(bottom) {
        if(leftBottom != rightBottom) {
          xBottom = xLeft + xDiff * (value-leftBottom) / (rightBottom-leftBottom);
        } else {
          xBottom = xRight;
          bFailureStatus = true;
        }
      }
      if(top) {
        if(leftTop != rightTop) {
          xTop = xLeft + xDiff * (value - leftTop) / (rightTop - leftTop);
        } else {
          xTop = xRight;
          bFailureStatus = true;
        }
      }
      
      XSetForeground(sdisplay, xgc, palPtr->makePixel(FGColor));
      
      Real hReal2X((Real) imageSizeH / (rightEdge - leftEdge));
      Real vReal2X((Real) imageSizeV / (topEdge - bottomEdge));
      
      if(left) { 
        xLeft -= leftEdge;
	yLeft -= bottomEdge;
        xLeft *= hReal2X;
	yLeft *= vReal2X;
      }
      if(right) {
        xRight -= leftEdge;
	yRight -= bottomEdge;
        xRight *= hReal2X;
	yRight *= vReal2X;
      }
      if(top) {
        xTop -= leftEdge;
	yTop -= bottomEdge;
        xTop *= hReal2X;
	yTop *= vReal2X; 
      }
      if(bottom) {
        xBottom -= leftEdge;
	yBottom -= bottomEdge;
        xBottom *= hReal2X;
	yBottom *= vReal2X; 
      }
      
      // finally, draw contour line
      if(left && right && bottom && top) {
        // intersects all sides, generate saddle point
        XDrawLine(sdisplay, dPixMap, gc, 
                  (int) xLeft,
		  (int) (imageSizeV - yLeft),
		  (int) xRight,
		  (int) (imageSizeV - yRight));
        XDrawLine(sdisplay, dPixMap, gc,
                  (int) xTop,
		  (int) (imageSizeV - yTop),
		  (int) xBottom,
		  (int) (imageSizeV - yBottom));
      } else if(top && bottom) {   // only intersects top and bottom sides
        XDrawLine(sdisplay, dPixMap, gc,
                  (int) xTop,
		  (int) (imageSizeV - yTop),
		  (int) xBottom,
		  (int) (imageSizeV - yBottom));
      } else if(left) {
        if(right) {
          XDrawLine(sdisplay, dPixMap, gc,
                    (int) xLeft,
		    (int) (imageSizeV - yLeft),
		    (int) xRight,
		    (int) (imageSizeV - yRight));
        } else if(top) {
          XDrawLine(sdisplay, dPixMap, gc,
                    (int) xLeft,
		    (int) (imageSizeV - yLeft),
		    (int) xTop,
		    (int) (imageSizeV-yTop));
        } else {
          XDrawLine(sdisplay, dPixMap, gc,
                    (int) xLeft,
		    (int) (imageSizeV - yLeft),
		    (int) xBottom,
		    (int) (imageSizeV - yBottom));
        }
      } else if(right) {
        if(top) {
          XDrawLine(sdisplay, dPixMap, gc,
                    (int) xRight,
		    (int) (imageSizeV - yRight),
		    (int) xTop,
		    (int) (imageSizeV - yTop));
        } else {
          XDrawLine(sdisplay, dPixMap, gc,
                    (int) xRight,
		    (int) (imageSizeV - yRight),
		    (int) xBottom,
		    (int) (imageSizeV - yBottom));
        }
      }
      
    }  // end for(i...)
  }  // end for(j...)
  return bFailureStatus;
}


// ---------------------------------------------------------------------
VectorDerived AmrPicture::FindVectorDerived(Vector<string> &aVectorDeriveNames) {
  // use the user specified names if available, otherwise try to
  // guess some common names
  
  if(AVGlobals::GivenUserVectorNames() != AVGlobals::enUserNone) {
    if(dataServicesPtr->CanDerive(AVGlobals::UserVectorNames()[0]) &&
       dataServicesPtr->CanDerive(AVGlobals::UserVectorNames()[1])
#if (BL_SPACEDIM == 3)
       && dataServicesPtr->CanDerive(AVGlobals::UserVectorNames()[2])
#endif
      )
    {
      aVectorDeriveNames = AVGlobals::UserVectorNames();
      if(AVGlobals::GivenUserVectorNames() == AVGlobals::enUserVelocities) {
        return enVelocity;
      } else if(AVGlobals::GivenUserVectorNames() == AVGlobals::enUserMomentums) {
        if(dataServicesPtr->CanDerive(densityName)) {
          return enMomentum;
	} else {
          if(myView == Amrvis::XY) {
	    cerr << "*** Error:  found momentums but not " << densityName  << endl;
	  }
          return enNoneFound;
	}
      } else {
	cerr << "*** Error in FindVectorDerived:  bad user type." << endl;
        return enNoneFound;  // something is wrong if we get here
      }
    } else {
      aVectorDeriveNames = AVGlobals::UserVectorNames();
      if(myView == Amrvis::XY) {
        cerr << "*** Error:  cannot find components:  ";
        for(int ii(0); ii < BL_SPACEDIM; ++ii) {
          cerr << aVectorDeriveNames[ii] << "  ";
        }
        cerr << endl;
      }
      return enNoneFound;  // bad component names were specified by the user
    }
  }

  for(int ivn(0); ivn < vecNames.size(); ++ivn) {
    if(dataServicesPtr->CanDerive(vecNames[ivn])) {
      aVectorDeriveNames = vecNames[ivn];
      if(vecIsMom[ivn]) {
        if(dataServicesPtr->CanDerive(densityName)) {
          return enMomentum;
	} else {
          if(myView == Amrvis::XY) {
	    cerr << "*** Error:  found momentums but not " << densityName << endl;
          }
          return enNoneFound;
	}
      } else {
        return enVelocity;
      }
    }
  }

  return enNoneFound;
}


// ---------------------------------------------------------------------
void AmrPicture::DrawVectorField(Display *pDisplay, 
                                 Drawable &pDrawable, const GC &pGC)
{
  int hDir(-1), vDir(-1);
  if(myView == Amrvis::XY) {
    hDir = Amrvis::XDIR;
    vDir = Amrvis::YDIR;
  } else if(myView == Amrvis::XZ) {
    hDir = Amrvis::XDIR;
    vDir = Amrvis::ZDIR;
  } else if(myView == Amrvis::YZ) {
    hDir = Amrvis::YDIR;
    vDir = Amrvis::ZDIR;
  } else {
    BL_ASSERT(false);
  }
  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  int DVFscale(pltAppStatePtr->CurrentScale());
  int maxDrawnLevel(pltAppStatePtr->MaxDrawnLevel());
  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());
  int DVFRatio(amrex::CRRBetweenLevels(maxDrawnLevel, 
                                  maxAllowableLevel, amrData.RefRatio()));
  // get velocity field
  Box DVFSliceBox(sliceFab[maxDrawnLevel]->box());
  int maxLength(DVFSliceBox.longside());
  FArrayBox densityFab(DVFSliceBox);
  FArrayBox hVelocity(DVFSliceBox);
  FArrayBox vVelocity(DVFSliceBox);
  VectorDerived whichVectorDerived;
  Vector<string> choice(BL_SPACEDIM);

  whichVectorDerived = FindVectorDerived(choice);

  if(whichVectorDerived == enNoneFound) {
    if(myView == Amrvis::XY) {
      cerr << "Could not find suitable quantities to create velocity vectors."
	   << endl;
    }
    return;
  }

  if(whichVectorDerived == enVelocity) {
    // fill the velocities
    string hVel(choice[hDir]);
    string vVel(choice[vDir]);
    amrex::DataServices::Dispatch(amrex::DataServices::FillVarOneFab, dataServicesPtr, 
                           (void *) &hVelocity,
			   (void *) (&(hVelocity.box())),
                           maxDrawnLevel,
			   (void *) &hVel);
    amrex::DataServices::Dispatch(amrex::DataServices::FillVarOneFab, dataServicesPtr, 
                           (void *) &vVelocity,
			   (void *) (&(vVelocity.box())),
                           maxDrawnLevel,
			   (void *) &vVel);

  } else {  // using momentums
    BL_ASSERT(whichVectorDerived == enMomentum);
    // fill the density and momentum:
    string sDensity(densityName);
    if( ! dataServicesPtr->CanDerive(sDensity)) {
      cerr << "Found momentums in the plot file but not " << densityName << endl;
      return;
    }

    string hMom(choice[hDir]);
    string vMom(choice[vDir]);

    amrex::DataServices::Dispatch(amrex::DataServices::FillVarOneFab, dataServicesPtr, 
                           (void *) &densityFab,
			   (void *) (&(densityFab.box())),
                           maxDrawnLevel,
			   (void *) &sDensity);
    amrex::DataServices::Dispatch(amrex::DataServices::FillVarOneFab, dataServicesPtr, 
                           (void *) &hVelocity,
			   (void *) (&(hVelocity.box())),
                           maxDrawnLevel,
			   (void *) &hMom);
    amrex::DataServices::Dispatch(amrex::DataServices::FillVarOneFab, dataServicesPtr, 
                           (void *) &vVelocity,
			   (void *) (&(vVelocity.box())),
                           maxDrawnLevel,
			   (void *) &vMom);

    // then divide to get velocity
    hVelocity /= densityFab;
    vVelocity /= densityFab;
  }
  
  // compute maximum speed
  Real smax(0.0);
  int npts(hVelocity.box().numPts());
  const Real *hdat = hVelocity.dataPtr();
  const Real *vdat = vVelocity.dataPtr();

  for(int k(0); k < npts; ++k) {
    Real s(sqrt(hdat[k] * hdat[k] + vdat[k] * vdat[k]));
    smax = amrex::max(smax,s);
  }
  
  
  if(smax < 1.0e-8) {
    cout << "CONTOUR: zero velocity field" << endl;
  } else {
    DrawVectorField(pDisplay, pDrawable, pGC, hDir, vDir, maxLength,
                      hdat, vdat, smax, DVFSliceBox, DVFscale*DVFRatio);
  }
}


// ---------------------------------------------------------------------
void AmrPicture::DrawVectorField(Display *pDisplay, Drawable &pDrawable, 
                                   const GC &pGC, int hDir, int vDir, int maxLength,
                                   const Real *hdat, const Real *vdat, 
                                   const Real velocityMax, const Box &dvfSliceBox,
                                   int dvfFactor)
{
  int maxpoints(pltAppStatePtr->GetNumContours());  // partition longest side
  BL_ASSERT(maxpoints > 0);
  Real sight_h(maxLength / maxpoints);
  int stride((int) sight_h);
  if(stride < 1) {
    stride = 1;
  }
  Real Amax(1.25 * sight_h);
  Real arrowLength(0.25);
  XSetForeground(pDisplay, pGC, palPtr->WhiteIndex());
  int ilo(dvfSliceBox.smallEnd(hDir));
  int leftEdge(ilo);
  int ihi(dvfSliceBox.bigEnd(hDir));
  int nx(ihi - ilo + 1);
  int jlo(dvfSliceBox.smallEnd(vDir));
  int bottomEdge(jlo);
  int jhi(dvfSliceBox.bigEnd(vDir));
  Real xlft(ilo + 0.5);
  Real ybot(jlo + 0.5);
  Real x, y, x1, y1, x2, y2, a, b, p1, p2;
  //Real s;
  //Real eps(1.0e-3);
  int iX1, iY1, iX2, iY2;
  for(int jj(jlo); jj <= jhi; jj += stride) {
    y = ybot + (jj-jlo);
    for(int ii(ilo); ii <= ihi; ii += stride) {
      x = xlft + (ii-ilo);
      x1 = hdat[ii-ilo + nx * (jj-jlo)];
      y1 = vdat[ii-ilo + nx * (jj-jlo)];
      //s = sqrt(x1 * x1 + y1 * y1);
      //if(s < eps) {
	//cout << "**** s eps = " << s << "  " << eps << endl;
        //continue;
      //}
      a = Amax * (x1 / velocityMax);
      b = Amax * (y1 / velocityMax);
      x2 = x + a;
      y2 = y + b; 
      iX1 = (int) ((x - leftEdge) * dvfFactor);
      iY1 = (int) (imageSizeV - ((y - bottomEdge) * dvfFactor));
      iX2 = (int) ((x2 - leftEdge) * dvfFactor);
      iY2 = (int) (imageSizeV - ((y2 - bottomEdge) * dvfFactor));
      XDrawLine(pDisplay, pDrawable, pGC, iX1, iY1, iX2, iY2);

      // draw the arrow heads
      p1 = x2 - arrowLength * a;
      p2 = y2 - arrowLength * b;
      p1 = p1 - (arrowLength * 0.5) * b;
      p2 = p2 + (arrowLength * 0.5) * a;
      iX1 = iX2;
      iY1 = iY2;
      iX2 = (int) ((p1 - leftEdge) * dvfFactor);
      iY2 = (int) (imageSizeV - ((p2 - bottomEdge) * dvfFactor));
      XDrawLine(pDisplay, pDrawable, pGC, iX1, iY1, iX2, iY2);
    }
  }
}
// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
