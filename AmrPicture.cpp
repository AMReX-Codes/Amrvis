
//
// $Id: AmrPicture.cpp,v 1.73 2002-02-07 23:59:02 vince Exp $
//

// ---------------------------------------------------------------
// AmrPicture.cpp
// ---------------------------------------------------------------
#include "AmrPicture.H"
#include "PltApp.H"
#include "PltAppState.H"
#include "Palette.H"
#include "DataServices.H"
#include "ProjectionPicture.H"

using std::cout;
using std::cerr;
using std::endl;
using std::max;
using std::min;

#include <ctime>

#ifdef BL_USE_ARRAYVIEW
#include "ArrayView.H"
#endif

bool DrawRaster(ContourType cType) {
  return (cType == RASTERONLY || cType == RASTERCONTOURS || cType == VECTORS);
}


bool DrawContours(ContourType cType) {
  return (cType == RASTERCONTOURS || cType == COLORCONTOURS || cType == BWCONTOURS);
}


// ---------------------------------------------------------------------
AmrPicture::AmrPicture(GraphicsAttributes *gaptr,
		       PltApp *pltappptr, PltAppState *pltappstateptr,
		       DataServices *dataservicesptr,
		       bool bcartgridsmoothing)
           : gaPtr(gaptr),
             pltAppPtr(pltappptr),
             pltAppStatePtr(pltappstateptr),
             dataServicesPtr(dataservicesptr),
             myView(XY),
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
    subDomain[i].coarsen(CRRBetweenLevels(i, maxAllowableLevel,
			 amrData.RefRatio()));
  }

  dataSizeH.resize(numberOfLevels);
  dataSizeV.resize(numberOfLevels);
  dataSize.resize(numberOfLevels);
  for(ilev = 0; ilev <= maxAllowableLevel; ++ilev) {
    dataSizeH[ilev] = subDomain[ilev].length(XDIR);
    dataSizeV[ilev] = subDomain[ilev].length(YDIR); 
    dataSize[ilev]  = dataSizeH[ilev] * dataSizeV[ilev];  // for a picture (slice).
  }

  vLine = 0;
  hLine = subDomain[maxAllowableLevel].bigEnd(YDIR) *
	  pltAppStatePtr->CurrentScale();
  subcutY = hLine;
  subcut2ndY = hLine;

#if (BL_SPACEDIM == 2)
  slice = 0;
#else
  slice = subDomain[maxAllowableLevel].smallEnd(YZ-myView);
#endif
  sliceBox.resize(numberOfLevels);

  for(ilev = minDrawnLevel; ilev <= maxAllowableLevel; ilev++) {  // set type
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
  int ilev;

  dataServicesPtr = pltAppPtr->GetDataServicesPtr();
  const AmrData &amrData = dataServicesPtr->AmrDataRef();

  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());
  numberOfLevels = maxAllowableLevel + 1;
  maxLevelWithGrids = maxAllowableLevel;
 
  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());

  subDomain.resize(numberOfLevels);
  Box sdBox(region);     // region is at the finestLevel
  subDomain[maxAllowableLevel] = sdBox.coarsen(CRRBetweenLevels(maxAllowableLevel,
					       pltAppStatePtr->FinestLevel(),
					       amrData.RefRatio()));

  for(ilev = maxAllowableLevel - 1; ilev >= minDrawnLevel; --ilev) {
    subDomain[ilev] = subDomain[maxAllowableLevel];
    subDomain[ilev].coarsen
	      (CRRBetweenLevels(ilev, maxAllowableLevel, amrData.RefRatio()));
  }

  dataSizeH.resize(numberOfLevels);
  dataSizeV.resize(numberOfLevels);
  dataSize.resize(numberOfLevels);
  for(ilev = 0; ilev <= maxAllowableLevel; ++ilev) {
    if(myView==XZ) {
      dataSizeH[ilev] = subDomain[ilev].length(XDIR);
      dataSizeV[ilev] = subDomain[ilev].length(ZDIR);
    } else if(myView==YZ) {
      dataSizeH[ilev] = subDomain[ilev].length(YDIR);
      dataSizeV[ilev] = subDomain[ilev].length(ZDIR);
    } else {
      dataSizeH[ilev] = subDomain[ilev].length(XDIR);
      dataSizeV[ilev] = subDomain[ilev].length(YDIR);
    }
    dataSize[ilev]  = dataSizeH[ilev] * dataSizeV[ilev];
  }

# if (BL_SPACEDIM == 3)
    
  if(myView==XY) {
      hLine = (subDomain[maxAllowableLevel].bigEnd(YDIR) -
      		subDomain[maxAllowableLevel].smallEnd(YDIR)) *
		pltAppStatePtr->CurrentScale();
      vLine = 0;
    } else if(myView==XZ) {
      hLine = (subDomain[maxAllowableLevel].bigEnd(ZDIR) -
      		subDomain[maxAllowableLevel].smallEnd(ZDIR)) *
		pltAppStatePtr->CurrentScale();
      vLine = 0;
    } else {
      hLine = (subDomain[maxAllowableLevel].bigEnd(ZDIR) -
      		subDomain[maxAllowableLevel].smallEnd(ZDIR)) *
		pltAppStatePtr->CurrentScale();
      vLine = 0;
    }


    if(parentPltAppPtr != NULL) {
      int tempSlice = parentPltAppPtr->GetAmrPicturePtr(myView)->GetSlice();
      tempSlice *= CRRBetweenLevels(parentPltAppPtr->GetPltAppState()->
				    MaxDrawnLevel(), 
                                    pltAppStatePtr->MaxDrawnLevel(),
                                    amrData.RefRatio());
      slice = max(min(tempSlice,
                      subDomain[maxAllowableLevel].bigEnd(YZ-myView)), 
                  subDomain[maxAllowableLevel].smallEnd(YZ-myView));
    } else {
      slice = subDomain[maxAllowableLevel].smallEnd(YZ-myView);
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
  if(myView==XZ) {
    sliceBox[maxAllowableLevel].setSmall(YDIR, slice);
    sliceBox[maxAllowableLevel].setBig(YDIR, slice);
  } else if(myView==YZ) {
    sliceBox[maxAllowableLevel].setSmall(XDIR, slice);
    sliceBox[maxAllowableLevel].setBig(XDIR, slice);
  } else {  // XY
#   if (BL_SPACEDIM == 3)
      sliceBox[maxAllowableLevel].setSmall(ZDIR, slice);
      sliceBox[maxAllowableLevel].setBig(ZDIR, slice);
#   endif
  }

  CoarsenSliceBox();

  sliceFab.resize(numberOfLevels);
  for(iLevel = minDrawnLevel; iLevel <= maxAllowableLevel; ++iLevel) {
    sliceFab[iLevel] = new FArrayBox(sliceBox[iLevel], 1);
  }
  xImageArray.resize(numberOfLevels);

  display = gaPtr->PDisplay();
  xgc = gaPtr->PGC();

  // use maxAllowableLevel because all imageSizes are the same
  // regardless of which level is showing
  // dont use imageBox.length() because of node centered boxes
  imageSizeH = pltAppStatePtr->CurrentScale() * dataSizeH[maxAllowableLevel];
  imageSizeV = pltAppStatePtr->CurrentScale() * dataSizeV[maxAllowableLevel];
  int widthpad = gaPtr->PBitmapPaddedWidth(imageSizeH);
  imageSize = imageSizeV * widthpad * gaPtr->PBytesPerPixel();

  imageData.resize(numberOfLevels);
  scaledImageData.resize(numberOfLevels);
  for(iLevel = 0; iLevel < minDrawnLevel; ++iLevel) {
    imageData[iLevel]       = NULL;
    scaledImageData[iLevel] = NULL;
  }
  for(iLevel = minDrawnLevel; iLevel <= maxAllowableLevel; ++iLevel) {
    imageData[iLevel] = new unsigned char[dataSize[iLevel]];
    scaledImageData[iLevel] = (unsigned char *) malloc(imageSize);
  }

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
  if(myView == XZ) {
    hColor = MaxPaletteIndex();
    vColor = 65;
  } else if(myView == YZ) {
    hColor = MaxPaletteIndex();
    vColor = 220;
  } else {
    hColor = 220;
    vColor = 65;
  }
  pixMapCreated = false;

  SetSlice(myView, slice);
}  // end AmrPictureInit()



// ---------------------------------------------------------------------
void AmrPicture::SetHVLine(int scale) {
  int maxDrawnLevel(pltAppStatePtr->MaxDrawnLevel());
  int first(0);
  for(int i(0); i <= YZ; ++i) {
    if(i == myView) {
      // do nothing
    } else {
      if(first == 0) {
        hLine = imageSizeV-1 -
		((pltAppPtr->GetAmrPicturePtr(i)->GetSlice() -
		  pltAppPtr->GetAmrPicturePtr(YZ - i)->
		  GetSubDomain()[maxDrawnLevel].smallEnd(YZ - i)) *
		  scale);
        first = 1;
      } else {
        vLine = ( pltAppPtr->GetAmrPicturePtr(i)->GetSlice() -
		  pltAppPtr->GetAmrPicturePtr(YZ - i)->
		  GetSubDomain()[maxDrawnLevel].smallEnd(YZ - i)) *
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
    free(scaledImageData[iLevel]);
    delete sliceFab[iLevel];
  }

  if(pixMapCreated) {
    XFreePixmap(display, pixMap);
  }
}


// ---------------------------------------------------------------------
void AmrPicture::SetSlice(int view, int here) {
  int lev;
  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());
  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());
  sliceDir = YZ - view;
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
        }
        sliceFab[lev]->resize(sliceBox[lev], 1);
      }
    }
# endif

  for(lev = minDrawnLevel; lev < gpArray.size(); ++lev) {
    gpArray[lev].clear();
  }
  gpArray.clear();

  gpArray.resize(numberOfLevels);
  maxLevelWithGrids = maxAllowableLevel;

  Array<int> nGrids(numberOfLevels);
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
	temp.shift(XDIR, -subDomain[lev].smallEnd(XDIR));
	temp.shift(YDIR, -subDomain[lev].smallEnd(YDIR));
#if (BL_SPACEDIM == 3)
	  temp.shift(ZDIR, -subDomain[lev].smallEnd(ZDIR));
#endif
        gpArray[lev][gridNumber].GridPictureInit(lev,
		CRRBetweenLevels(lev, maxAllowableLevel, amrData.RefRatio()),
		pltAppStatePtr->CurrentScale(), imageSizeH, imageSizeV,
		temp, sliceDataBox, sliceDir);
        ++gridNumber;
      }
    }
  }
}  // end SetSlice(...)


// ---------------------------------------------------------------------
void AmrPicture::APChangeContour(ContourType prevCType) {
  ContourType cType(pltAppStatePtr->GetContourType());
  Real minUsing, maxUsing;
  pltAppStatePtr->GetMinMax(minUsing, maxUsing);
  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());
  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());

  if(DrawRaster(cType) != DrawRaster(prevCType)) {  // recreate the raster image
    AmrData &amrData = dataServicesPtr->AmrDataRef();
    for(int iLevel(minDrawnLevel); iLevel <= maxAllowableLevel; ++iLevel) {
      CreateImage(*(sliceFab[iLevel]), imageData[iLevel],
 		  dataSizeH[iLevel], dataSizeV[iLevel],
 	          minUsing, maxUsing, palPtr);
      CreateScaledImage(&(xImageArray[iLevel]), pltAppStatePtr->CurrentScale() *
                 CRRBetweenLevels(iLevel, maxAllowableLevel, amrData.RefRatio()),
                 imageData[iLevel], scaledImageData[iLevel],
                 dataSizeH[iLevel], dataSizeV[iLevel],
                 imageSizeH, imageSizeV);
    }
    if( ! pltAppPtr->PaletteDrawn()) {
      pltAppPtr->PaletteDrawn(true);
      palPtr->Draw(minUsing, maxUsing, pltAppStatePtr->GetFormatString());
    }
  }
  int maxDrawnLevel(pltAppStatePtr->MaxDrawnLevel());
  APDraw(minDrawnLevel, maxDrawnLevel);
}


// ---------------------------------------------------------------------
void AmrPicture::DrawBoxes(Array< Array<GridPicture> > &gp, Drawable &drawable) {
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
        XSetForeground(display, xgc, palPtr->WhiteIndex());
      } else {
        XSetForeground(display, xgc,
		       palPtr->pixelate(MaxPaletteIndex()-80*(level-1)));
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
void AmrPicture::DrawTerrBoxes(int level, bool bIsWindow, bool bIsPixmap) {
  cerr << endl;
  cerr << "***** Error:  should not be in AmrPicture::DrawTerrBoxes." << endl;
  cerr << "Continuing..." << endl;
  cerr << endl;

/*
  int i, lev, ix;
  short xbox, ybox;
  unsigned short wbox, hbox;

  BL_ASSERT((bIsWindow ^ bIsPixmap) == true);

  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  int expansion(pltAppPtr->GetExpansion());
  int hScale(pltAppStatePtr->CurrentScale() * expansion);

  if(finestLevel+1 != numberOfLevels) {
    for(lev = numberOfLevels-1; lev < finestLevel; lev++) {
       //IntVect ref_ratio = amrData.RefRatio(lev);
       hScale = hScale / amrData.RefRatio()[lev];
    }
  }

  int vScale(hScale * pltAppPtr->GetZStretch());

  const Array<Real> &dx  = amrData.DxLevel()[finestLevel];
  const Array<Real> &dx0 = amrData.DxLevel()[0];

  //  Note : this is based on DrawRTBoxes but hasnt been tested
  Real index_zhi = vScale * dx0[0] / (expansion * dx[0]) *
                   (subDomain[0].bigEnd()[BL_SPACEDIM-1] + 1);

      for(i = 0; i < gpArray[level].length(); ++i) {
        xbox = gpArray[level][i].HPositionInPicture();
        ybox = gpArray[level][i].VPositionInPicture();
        wbox = gpArray[level][i].ImageSizeH();
        hbox = gpArray[level][i].ImageSizeV();

        if(level == 0) {
          if(bIsWindow) {
            XDrawRectangle(display, pictureWindow, xgc, xbox, ybox, wbox, hbox);
          } else if(bIsPixmap) {
            XDrawRectangle(display, pixMap, xgc, xbox, ybox, wbox, hbox);
          }

        } else {

          GridPlot *gridptr = gpArray[level][i].GetGridPtr();
          DataBoxPlot *meshdb = gridptr->Mesh();
          const Real *meshdat = meshdb->dataPtr();
          const int *mlo  = meshdb->loVect();
          const int *mhi  = meshdb->hiVect();
          const int *mlen = meshdb->length();

#if (BL_SPACEDIM == 2)
#define M_L(i,j) i-mlo[0]+mlen[0]*(j-mlo[1])
#elif (BL_SPACEDIM == 3)
#define M_L(i,j,k) i-mlo[0]+mlen[0]*( (j-mlo[1]) + mlen[1]*(k-mlo[2]) )
#endif

#if (BL_SPACEDIM == 2)
          Real xlo, xhi, ylo, yhi;

          // Draw left boundary
          ylo = index_zhi - vScale * meshdat[M_L(mlo[0], mlo[1])] / dx[0];
          yhi = index_zhi - vScale * meshdat[M_L(mlo[0], mhi[1])] / dx[0];
          xlo = hScale * mlo[0];
          xhi = hScale * mlo[0];
          if(bIsWindow) {
            XDrawLine(display, pictureWindow, xgc, xlo, ylo, xhi, yhi);
          } else if(bIsPixmap) {
            XDrawLine(display, pixMap, xgc, xlo, ylo, xhi, yhi);
          }

          // Draw right boundary
          ylo = index_zhi - vScale * meshdat[M_L(mhi[0], mlo[1])] / dx[0];
          yhi = index_zhi - vScale * meshdat[M_L(mhi[0], mhi[1])] / dx[0];
          xlo = hScale * mhi[0];
          xhi = hScale * mhi[0];
          if(bIsWindow) {
            XDrawLine(display, pictureWindow, xgc, xlo, ylo, xhi, yhi);
          } else if(bIsPixmap) {
            XDrawLine(display, pixMap, xgc, xlo, ylo, xhi, yhi);
          }

          // Draw bottom boundary
          for(ix = mlo[0]; ix < mhi[0]; ++ix) {
            ylo = index_zhi-vScale*meshdat[M_L(ix,mlo[1])] / dx[0];
            yhi = index_zhi-vScale*meshdat[M_L(ix+1,mlo[1])] / dx[0];
            xlo = hScale * ix;
            xhi = hScale * (ix + 1);
            if(bIsWindow) {
              XDrawLine(display, pictureWindow, xgc, xlo, ylo, xhi, yhi);
            } else if(bIsPixmap) {
              XDrawLine(display, pixMap, xgc, xlo, ylo, xhi, yhi);
            }
          }

          // Draw top boundary
          for(ix = mlo[0]; ix < mhi[0]; ++ix) {
            ylo = index_zhi - vScale * meshdat[M_L(ix, mhi[1])] / dx[0];
            yhi = index_zhi - vScale * meshdat[M_L(ix + 1, mhi[1])] / dx[0];
            xlo = hScale * ix;
            xhi = hScale * (ix + 1);
            if(bIsWindow) {
              XDrawLine(display, pictureWindow, xgc, xlo, ylo, xhi, yhi);
            } else if(bIsPixmap) {
              XDrawLine(display, pixMap, xgc, xlo, ylo, xhi, yhi);
            }
          }
#endif
        }
      }
*/
}


// ---------------------------------------------------------------------
void AmrPicture::APDraw(int fromLevel, int toLevel) {
  if( ! pixMapCreated) {
    pixMap = XCreatePixmap(display, pictureWindow,
			   imageSizeH, imageSizeV, gaPtr->PDepth());
    pixMapCreated = true;
  }  
 
  XPutImage(display, pixMap, xgc, xImageArray[toLevel], 0, 0, 0, 0,
	    imageSizeH, imageSizeV);
           
  ContourType cType(pltAppStatePtr->GetContourType());
  if(DrawContours(cType)) {
    DrawContour(sliceFab, display, pixMap, xgc);
  } else if(cType == VECTORS) {
    DrawVectorField(display, pixMap, xgc);
  }

//
//char *fontName = "12x24";
//XFontStruct *fontInfo;
//fontInfo = XLoadQueryFont(display, fontName);

//GC fontGC = XCreateGC(display, gaPtr->PRoot(), 0, NULL);
//XSetFont(display, fontGC, fontInfo->fid);

//XSetForeground(display, fontGC, palPtr->WhiteIndex());
  //XDrawString(display, pixMap,
              //fontGC,
              //imageSizeH / 2,
              //imageSizeV / 2,
              //currentDerived.c_str(), currentDerived.length());
//

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
        XSetForeground(display, xgc, palPtr->pixelate(60));
        XDrawLine(display, pictureWindow, xgc,
		  regionX+1, regionY+1, region2ndX+1, regionY+1); 
        XDrawLine(display, pictureWindow, xgc,
		  regionX+1, regionY+1, regionX+1, region2ndY+1); 
        XDrawLine(display, pictureWindow, xgc,
		  regionX+1, region2ndY+1, region2ndX+1, region2ndY+1); 
        XDrawLine(display, pictureWindow, xgc,
		  region2ndX+1, regionY+1, region2ndX+1, region2ndY+1);

        XSetForeground(display, xgc, palPtr->pixelate(175));
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
      XSetForeground(display, xgc, palPtr->pixelate(hColor));
      XDrawLine(display, pictureWindow, xgc, 0, hLine, imageSizeH, hLine); 
      XSetForeground(display, xgc, palPtr->pixelate(vColor));
      XDrawLine(display, pictureWindow, xgc, vLine, 0, vLine, imageSizeV); 
      
      XSetForeground(display, xgc, palPtr->pixelate(hColor-30));
      XDrawLine(display, pictureWindow, xgc, 0, hLine+1, imageSizeH, hLine+1); 
      XSetForeground(display, xgc, palPtr->pixelate(vColor-30));
      XDrawLine(display, pictureWindow, xgc, vLine+1, 0, vLine+1, imageSizeV); 
      
      if(subCutShowing) {  // draw subvolume cutting border 
        XSetForeground(display, xgc, palPtr->pixelate(90));
        XDrawLine(display, pictureWindow, xgc,
		  subcutX+1, subcutY+1, subcut2ndX+1, subcutY+1); 
        XDrawLine(display, pictureWindow, xgc,
		  subcutX+1, subcutY+1, subcutX+1, subcut2ndY+1); 
        XDrawLine(display, pictureWindow, xgc,
		  subcutX+1, subcut2ndY+1, subcut2ndX+1, subcut2ndY+1); 
        XDrawLine(display, pictureWindow, xgc,
		  subcut2ndX+1, subcutY+1, subcut2ndX+1, subcut2ndY+1);
          
        XSetForeground(display, xgc, palPtr->pixelate(155));
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

  Real minUsing, maxUsing;
  pltAppStatePtr->GetMinMax(minUsing, maxUsing);
  VSHOWVAL(Verbose(), minUsing)
  VSHOWVAL(Verbose(), maxUsing)
  VSHOWVAL(Verbose(), minDrawnLevel)
  VSHOWVAL(Verbose(), maxAllowableLevel)

  const string currentDerived(pltAppStatePtr->CurrentDerived());
  for(int iLevel(minDrawnLevel); iLevel <= maxAllowableLevel; ++iLevel) {
    DataServices::Dispatch(DataServices::FillVarOneFab, dataServicesPtr,
		           (void *) (sliceFab[iLevel]),
			   (void *) (&(sliceFab[iLevel]->box())),
			   iLevel,
			   (void *) &currentDerived);
    CreateImage(*(sliceFab[iLevel]), imageData[iLevel],
 		dataSizeH[iLevel], dataSizeV[iLevel],
 	        minUsing, maxUsing, palPtr);
    CreateScaledImage(&(xImageArray[iLevel]), pltAppStatePtr->CurrentScale() *
                CRRBetweenLevels(iLevel, maxAllowableLevel, amrData.RefRatio()),
                imageData[iLevel], scaledImageData[iLevel],
                dataSizeH[iLevel], dataSizeV[iLevel],
                imageSizeH, imageSizeV);
  }
  if( ! pltAppPtr->PaletteDrawn()) {
    pltAppPtr->PaletteDrawn(true);
    palptr->Draw(minUsing, maxUsing, pltAppStatePtr->GetFormatString());
  }
  int maxDrawnLevel(pltAppStatePtr->MaxDrawnLevel());
  APDraw(minDrawnLevel, maxDrawnLevel);

}  // end AP Make Images


// -------------------------------------------------------------------
// convert Real to char in imagedata from fab
void AmrPicture::CreateImage(const FArrayBox &fab, unsigned char *imagedata,
			     int datasizeh, int datasizev,
			     Real globalMin, Real globalMax, Palette *palptr)
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
  
  // flips the image in Vert dir: j => datasizev-j-1
  if(DrawRaster(pltAppStatePtr->GetContourType())) {
    Real dPoint;
    int paletteStart(palptr->PaletteStart());
    int paletteEnd(palptr->PaletteEnd());
    int colorSlots(palptr->ColorSlots());
    int csm1(colorSlots - 1);
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

  } else {

    if(LowBlack()) {
      Pixel blackIndex(palptr->BlackIndex());
      for(int i(0); i < (datasizeh * datasizev); ++i) {
        imagedata[i] = blackIndex;
      }
    } else {
      Pixel whiteIndex(palptr->WhiteIndex());
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
				   int imagesizeh, int imagesizev)
{ 
  int widthpad = gaPtr->PBitmapPaddedWidth(imagesizeh);
  *ximage = XCreateImage(display, gaPtr->PVisual(),
		gaPtr->PDepth(), ZPixmap, 0, (char *) scaledimagedata,
		widthpad, imagesizev,
		XBitmapPad(display), widthpad * gaPtr->PBytesPerPixel());

  if( ! bCartGridSmoothing) {
    if(true) {
	for(int j(0); j < imagesizev; ++j) {
	    int jtmp(datasizeh * (j/scale));
	    for(int i(0); i < widthpad; ++i) {
		int itmp(i / scale);
		unsigned char imm1(imagedata[ itmp + jtmp ]);
		XPutPixel(*ximage, i, j, palPtr->makePixel(imm1));
	      }
	  }
    } else {
	(*ximage)->byte_order = MSBFirst;
	(*ximage)->bitmap_bit_order = MSBFirst;
	for(int j(0); j < imagesizev; ++j) {
	  int jish(j * imagesizeh);
	  int jtmp(datasizeh * (j/scale));
	  for(int i(0); i < imagesizeh; ++i) {
	    scaledimagedata[i+jish] = imagedata [ (i/scale) + jtmp ];
	  }
	}
    }
  } else {  // bCartGridSmoothing
/*
    int ii, jj, rrcs, iis;
    int blackIndex, whiteIndex, bodyColor;
    blackIndex = palPtr->BlackIndex();
    whiteIndex = palPtr->WhiteIndex();
    bodyColor = blackIndex;
    Real vfeps = gridptr->AmrPlotPtr()->VfEps(myLevel);
    Real *vfracPoint = vfracData->dataPtr();
    Real vfp, omvfe = 1.0 - vfeps;
    int vidx, svidx;
    Real stencil[9];
    int nBodyCells, nScaledImageCells;

    nScaledImageCells = rrcs*rrcs;

    Real sumH[3], sumV[3];
    Real diffAvgV[3], diffAvgH[3], avgV, avgH;
    //Real minDAV, maxDAV, minDAH, maxDAH;
    Real normV, normH;
    int  isum, jsum, nStartV, nEndV, nStartH, nEndH, tempSum;
    int nV, nH, stencilSize = 3;
    int nCalcBodyCells, fluidCell = 1, bodyCell = 0, markedCell = -1;
    Real cellDx = 1.0/((Real) rrcs), cellDy = 1.0/((Real) rrcs);
    Real yBody, slope;
    int iCurrent, jCurrent, jBody;
    int isIndex;

    Array<int> imageStencil(nScaledImageCells);

    for(j = 0; j < dataSizeV; ++j) {
      for(i = 0; i < dataSizeH; ++i) {
        vidx = i + (dataSizeV-1-j)*dataSizeH;  // get the volfrac for cell(i,j)
        vfp = vfracPoint[vidx];

        if(vfp > vfeps && vfp < omvfe) {  // a mixed cell

          for(iis = 0; iis < 9; iis++) {
            stencil[iis] = -2.0*vfeps;  // flag value for boundary stencils
          }

          // fill the stencil with volume fractions
          svidx = (i-1) + (dataSizeV-1-(j-1))*dataSizeH;  // up left
          if((i-1) >= 0 && (dataSizeV-1-(j-1)) < dataSizeV) {
            stencil[0] = vfracPoint[svidx];
          }
          svidx = (i  ) + (dataSizeV-1-(j-1))*dataSizeH;  // up
          if((dataSizeV-1-(j-1)) < dataSizeV) {
            stencil[1] = vfracPoint[svidx];
          }
          svidx = (i+1) + (dataSizeV-1-(j-1))*dataSizeH;  // up right
          if((i+1) < dataSizeH && (dataSizeV-1-(j-1)) < dataSizeV) {
            stencil[2] = vfracPoint[svidx];
          }
          svidx = (i-1) + (dataSizeV-1-(j  ))*dataSizeH;  // left
          if((i-1) >= 0) {
            stencil[3] = vfracPoint[svidx];
          }
          stencil[4] = vfp;  // the center
          svidx = (i+1) + (dataSizeV-1-(j  ))*dataSizeH;  // right
          if((i+1) < dataSizeH) {
            stencil[5] = vfracPoint[svidx];
          }
          svidx = (i-1) + (dataSizeV-1-(j+1))*dataSizeH;  // down left
          if((i-1) >= 0 && ((int)(dataSizeV-1-(j+1))) >= 0) {
            stencil[6] = vfracPoint[svidx];
          }
          svidx = (i  ) + (dataSizeV-1-(j+1))*dataSizeH;  // down
          if(((int)(dataSizeV-1-(j+1))) >= 0) {
            stencil[7] = vfracPoint[svidx];
          }
          svidx = (i+1) + (dataSizeV-1-(j+1))*dataSizeH;  // down right
          if((i+1) < dataSizeH && ((int)(dataSizeV-1-(j+1))) >= 0) {
            stencil[8] = vfracPoint[svidx];
          }

#if (BL_SPACEDIM==2)
          // fix for straight lines near corners
          Real smallval = 0.0001;
          if(Abs(stencil[4] - stencil[3]) < smallval &&
             Abs(stencil[4] - stencil[5]) > smallval) {
            stencil[2] = -2.0*vfeps;  // flag value
            stencil[5] = -2.0*vfeps;  // flag value
            stencil[8] = -2.0*vfeps;  // flag value
          }
          if(Abs(stencil[4] - stencil[5]) < smallval &&
             Abs(stencil[4] - stencil[3]) > smallval) {
            stencil[0] = -2.0*vfeps;  // flag value
            stencil[3] = -2.0*vfeps;  // flag value
            stencil[6] = -2.0*vfeps;  // flag value
          }
          if(Abs(stencil[4] - stencil[1]) < smallval &&
             Abs(stencil[4] - stencil[7]) > smallval) {
            stencil[6] = -2.0*vfeps;  // flag value
            stencil[7] = -2.0*vfeps;  // flag value
            stencil[8] = -2.0*vfeps;  // flag value
          }
          if(Abs(stencil[4] - stencil[7]) < smallval &&
             Abs(stencil[4] - stencil[1]) > smallval) {
            stencil[0] = -2.0*vfeps;  // flag value
            stencil[1] = -2.0*vfeps;  // flag value
            stencil[2] = -2.0*vfeps;  // flag value
          }
#endif

          nBodyCells = (int) ((1.0-vfp)*nScaledImageCells);
                    // there should be this many body cells calculated

          nStartV = 0;
          nEndV   = 2;
          nStartH = 0;
          nEndH   = 2;
          if(stencil[0] < 0.0 && stencil[1] < 0.0 && stencil[2] < 0.0) {
            nStartH++;
          }
          if(stencil[0] < 0.0 && stencil[3] < 0.0 && stencil[6] < 0.0) {
            nStartV++;
          }
          if(stencil[2] < 0.0 && stencil[5] < 0.0 && stencil[8] < 0.0) {
            nEndV--;
          }
          if(stencil[6] < 0.0 && stencil[7] < 0.0 && stencil[8] < 0.0) {
            nEndH--;
          }

          nV = nEndV - nStartV + 1;
          nH = nEndH - nStartH + 1;

          for(jsum=nStartH; jsum<=nEndH; jsum++) {
            sumH[jsum] = 0;
            for(isum=nStartV; isum<=nEndV; isum++) {
              sumH[jsum] += stencil[isum + stencilSize*jsum];
            }
          }
          for(isum=nStartV; isum<=nEndV; isum++) {
            sumV[isum] = 0;
            for(jsum=nStartH; jsum<=nEndH; jsum++) {
              sumV[isum] += stencil[isum + stencilSize*jsum];
            }
          }

          tempSum = 0;
          for(isum=nStartV; isum<=nEndV; isum++) {
            tempSum += sumV[isum];
          }
          avgV = tempSum / ((Real) nV);
          tempSum = 0;
          for(isum=nStartH; isum<=nEndH; isum++) {
            tempSum += sumH[isum];
          }
          avgH = tempSum / ((Real) nH);

          for(isum=nStartV; isum<=nEndV; isum++) {
            diffAvgV[isum] = sumV[isum] - avgV;
          }
          for(isum=nStartH; isum<=nEndH; isum++) {
            diffAvgH[isum] = sumH[isum] - avgH;
          }

          //for(isum=nStartV; isum< nEndV; isum++) {
            //minDAV = min(diffAvgV[isum], diffAvgV[isum+1]);
            //maxDAV = max(diffAvgV[isum], diffAvgV[isum+1]);
          //}
          //for(isum=nStartH; isum<=nEndH; isum++) {
            //minDAH = min(diffAvgH[isum], diffAvgH[isum+1]);
            //maxDAH = max(diffAvgH[isum], diffAvgH[isum+1]);
          //}
          normH =  ((diffAvgV[nEndV] - diffAvgV[nStartV]) * ((Real) nH));
                                                  // nH is correct here
          normV = -((diffAvgH[nEndH] - diffAvgH[nStartH]) * ((Real) nV));
                                                  // nV is correct here

          for(ii = 0; ii < rrcs * rrcs; ++ii) {
            imageStencil[ii] = fluidCell;
          }
          if(Abs(normV) > 0.000001) {
            slope = normH/normV;  // perpendicular to normal
          } else {
            slope = normH;  // avoid divide by zero
          }

          if(normV > 0.0 && normH > 0.0) {         // upper right quadrant

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
              jBody = min(rrcs-1, (int) (yBody/cellDy));
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
                nCalcBodyCells++;
              }
            }

          }  // end while(...)

          } else if(normV < 0.0 && normH < 0.0) {    // lower left quadrant

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
                jBody = max(0, (int) (rrcs - (yBody / cellDy)));
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


          } else if(normV > 0.0 && normH < 0.0) {     //  upper left quadrant

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

            for(ii=rrcs; ii>iCurrent; ii--) {
              yBody = (-slope * ((ii-iCurrent)*cellDx)) + (jCurrent*cellDy);
              jBody = min(rrcs-1, (int) (yBody/cellDy));
              for(jj=0; jj<=jBody; jj++) {
                isIndex = (ii-1) + ((rrcs-(jj+1))*rrcs);
                imageStencil[isIndex] = bodyCell;  // yflip
              }
            }

            // sum the body cells
            nCalcBodyCells = 0;
            for(ii = 0; ii < rrcs * rrcs; ++ii) {
              if(imageStencil[ii] == bodyCell) { nCalcBodyCells++; }
            }

          }  // end while(...)

          } else if(normV < 0.0 && normH > 0.0) {     // lower right quadrant
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
              jBody = max(0, (int) (rrcs - (yBody / cellDy)));
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

          } else if(Abs(normV) < 0.000001) {  // vertical face

            if(normH > 0.0) {  // body is on the left edge of the cell
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

          } else if(Abs(normH) < 0.000001) {  // horizontal face

            if(normV > 0.0) {  // body is on the bottom edge of the cell
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
              if(imageStencil[ii + (jj * rrcs)] == fluidCell) {  // in fluid
                scaledImageData[((i * rrcs) + ii)+(((j * rrcs)+jj)*imageSizeH)] =
                                                imageData[i + j*dataSizeH];
              } else if(imageStencil[ii + (jj*rrcs)] == markedCell) {
                scaledImageData[((i*rrcs)+ii)+(((j*rrcs)+jj)*imageSizeH)] =
                                                                whiteIndex;
              } else {  // in body
                scaledImageData[((i*rrcs)+ii)+(((j*rrcs)+jj)*imageSizeH)] =
                                                                 bodyColor;
              }
            }  // end for(ii...)
          }  // end for(jj...)

        } else {  // non mixed cell
          for(jj = 0; jj < rrcs; ++jj) {
            for(ii = 0; ii < rrcs; ++ii) {
              scaledImageData[((i*rrcs)+ii) + (((j*rrcs)+jj) * imageSizeH)] =
                       imageData[i + j*dataSizeH];
            }
          }
        }

      }  // end for(i...)
    }  // end for(j...)
*/


  }  // end if(bCartGridSmoothing)

}  // end CreateScaledImage()


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
    free(scaledImageData[iLevel]);
    scaledImageData[iLevel] = (unsigned char *) malloc(imageSize);
    CreateScaledImage(&xImageArray[iLevel], newScale *
                CRRBetweenLevels(iLevel, maxAllowableLevel, amrData.RefRatio()),
                imageData[iLevel], scaledImageData[iLevel],
                dataSizeH[iLevel], dataSizeV[iLevel],
                imageSizeH, imageSizeV);
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
  if(pltAppStatePtr->GetShowingBoxes() && bdrawboxesintoimage) {
    for(int level(minDrawnLevel); level <= maxDrawnLevel; ++level) {
      if(level == minDrawnLevel) {
        XSetForeground(display, xgc, palPtr->WhiteIndex());
      } else {
        XSetForeground(display, xgc,
		       palPtr->pixelate(MaxPaletteIndex()-80*level));
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
  /*
  XSetForeground(display, xgc, palPtr->pixelate(MaxPaletteIndex()));
  XDrawRectangle(display, pixMap, xgc, 0, 0, imageSizeH-1, imageSizeV-1);
  */

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
void AmrPicture::GetGridBoxes(Array< Array<GridBoxes> > &gb,
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
    sliceBox[i].coarsen(CRRBetweenLevels(i, maxAllowableLevel,
			dataServicesPtr->AmrDataRef().RefRatio()));
  }
}


// ---------------------------------------------------------------------
void AmrPicture::CreateFrames(AnimDirection direction) {
  char buffer[BUFSIZ];
  bool cancelled(false);
  int islice, i, j, lev, gridNumber;
  Array<int> intersectGrids;
  int maxLevelWithGridsHere;
  int posneg(1);
  if(direction == ANIMNEGDIR) {
    posneg = -1;
  }
  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  int minDrawnLevel(pltAppStatePtr->MinDrawnLevel());
  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());

  sprintf(buffer, "Creating frames..."); 
  PrintMessage(buffer);
  Array<Box> interBox(numberOfLevels);
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
      interBox[j].coarsen(CRRBetweenLevels(j, maxAllowableLevel,
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
	  temp.shift(XDIR, -subDomain[lev].smallEnd(XDIR));
	  temp.shift(YDIR, -subDomain[lev].smallEnd(YDIR));
          temp.shift(ZDIR, -subDomain[lev].smallEnd(ZDIR));
          frameGrids[islice][lev][gridNumber].GridPictureInit(lev,
                  CRRBetweenLevels(lev, maxAllowableLevel, amrData.RefRatio()),
                  pltAppStatePtr->CurrentScale(), imageSizeH, imageSizeV,
                  temp, sliceDataBox, sliceDir);
          ++gridNumber;
        }
      }
    }   // end for(lev...)

    // get the data for this slice
    string currentDerived(pltAppStatePtr->CurrentDerived());
    FArrayBox imageFab(interBox[maxDrawnLevel], 1);
    DataServices::Dispatch(DataServices::FillVarOneFab, dataServicesPtr,
			   (void *) &imageFab,
			   (void *) (&(imageFab.box())),
			   maxDrawnLevel,
                           (void *) &currentDerived);

    Real minUsing, maxUsing;
    pltAppStatePtr->GetMinMax(minUsing, maxUsing);
    CreateImage(imageFab, frameImageData,
		dataSizeH[maxDrawnLevel], dataSizeV[maxDrawnLevel],
                minUsing, maxUsing, palPtr);

    // this cannot be deleted because it belongs to the XImage
    unsigned char *frameScaledImageData;
    frameScaledImageData = (unsigned char *)malloc(imageSize);
    CreateScaledImage(&(frameBuffer[islice]), pltAppStatePtr->CurrentScale() *
           CRRBetweenLevels(maxDrawnLevel, maxAllowableLevel, amrData.RefRatio()),
           frameImageData, frameScaledImageData,
           dataSizeH[maxDrawnLevel], dataSizeV[maxDrawnLevel],
           imageSizeH, imageSizeV);
    ShowFrameImage(islice);
#if (BL_SPACEDIM == 3)
    
    if( ! framesMade) {
      pltAppPtr->GetProjPicturePtr()->ChangeSlice(YZ-sliceDir, islice+start);
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
    for(int i(0); i <= iEnd; ++i) {
      int iDestroySlice((((slice - start + (posneg * i)) + length) % length));
      XDestroyImage(frameBuffer[iDestroySlice]);
    }
    framesMade = false;
    sprintf(buffer, "Cancelled.\n"); 
    PrintMessage(buffer);
    APChangeSlice(start+islice);
    AmrPicture *apXY = pltAppPtr->GetAmrPicturePtr(XY);
    AmrPicture *apXZ = pltAppPtr->GetAmrPicturePtr(XZ);
    AmrPicture *apYZ = pltAppPtr->GetAmrPicturePtr(YZ);
    if(sliceDir == XDIR) {
      apXY->SetVLine((slice - subDomain[maxAllowableLevel].smallEnd(XDIR)) *
                                 pltAppStatePtr->CurrentScale());
      apXY->DoExposePicture();
      apXZ->SetVLine((slice - subDomain[maxAllowableLevel].smallEnd(XDIR)) *
                                  pltAppStatePtr->CurrentScale());
      apXZ->DoExposePicture();
    }
    if(sliceDir == YDIR) {
      apXY->SetHLine(apXY->ImageSizeV()-1 - (slice -
				  subDomain[maxAllowableLevel].smallEnd(YDIR)) *
                       		  pltAppStatePtr->CurrentScale());
      apXY->DoExposePicture();
      apYZ->SetVLine((slice - subDomain[maxAllowableLevel].smallEnd(YDIR)) *
                                  pltAppStatePtr->CurrentScale());
      apYZ->DoExposePicture();
    }
    if(sliceDir == ZDIR) {
      apYZ->SetHLine(apYZ->ImageSizeV()-1 - (slice -
				  subDomain[maxAllowableLevel].smallEnd(ZDIR)) *
				  pltAppStatePtr->CurrentScale());
      apYZ->DoExposePicture();
      apXZ->SetHLine(apXZ->ImageSizeV()-1 - (slice -
				  subDomain[maxAllowableLevel].smallEnd(ZDIR)) *
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
  if(sweepDirection == ANIMPOSDIR) {
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
  if(sweepDirection == ANIMPOSDIR) {
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
void AmrPicture::Sweep(AnimDirection direction) {
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
void AmrPicture::DrawSlice(int iSlice) {
  XDrawLine(display, pictureWindow, pltAppPtr->GetRbgc(),
            0, 30, imageSizeH, 30);
}


// ---------------------------------------------------------------------
void AmrPicture::ShowFrameImage(int iSlice) {
  AmrPicture *apXY = pltAppPtr->GetAmrPicturePtr(XY);
  AmrPicture *apXZ = pltAppPtr->GetAmrPicturePtr(XZ);
  AmrPicture *apYZ = pltAppPtr->GetAmrPicturePtr(YZ);
  int iRelSlice(iSlice);

  XPutImage(display, pictureWindow, xgc,
            frameBuffer[iRelSlice], 0, 0, 0, 0, imageSizeH, imageSizeV);

  DrawBoxes(frameGrids[iRelSlice], pictureWindow);

  if(DrawContours(pltAppStatePtr->GetContourType()) == true) {
    DrawContour(sliceFab, display, pictureWindow, xgc);
  }


  // draw plane "cutting" lines
  XSetForeground(display, xgc, palPtr->pixelate(hColor));
  XDrawLine(display, pictureWindow, xgc, 0, hLine, imageSizeH, hLine); 
  XSetForeground(display, xgc, palPtr->pixelate(vColor));
  XDrawLine(display, pictureWindow, xgc, vLine, 0, vLine, imageSizeV); 
  XSetForeground(display, xgc, palPtr->pixelate(hColor-30));
  XDrawLine(display, pictureWindow, xgc, 0, hLine+1, imageSizeH, hLine+1); 
  XSetForeground(display, xgc, palPtr->pixelate(vColor-30));
  XDrawLine(display, pictureWindow, xgc, vLine+1, 0, vLine+1, imageSizeV); 
  
  if(sliceDir == XDIR) {
    apXY->SetVLine(iRelSlice * pltAppStatePtr->CurrentScale());
    apXY->DoExposePicture();
    apXZ->SetVLine(iRelSlice * pltAppStatePtr->CurrentScale());
    apXZ->DoExposePicture();
  }
  if(sliceDir == YDIR) {
    apXY->SetHLine(apXY->ImageSizeV() - 1 - iRelSlice * pltAppStatePtr->CurrentScale());
    apXY->DoExposePicture();
    apYZ->SetVLine(iRelSlice * pltAppStatePtr->CurrentScale());
    apYZ->DoExposePicture();
  }
  if(sliceDir == ZDIR) {
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
void AmrPicture::DrawContour(Array <FArrayBox *> passedSliceFab,
                             Display *passed_display, 
                             Drawable &passedPixMap, 
                             const GC &passedGC)
{
  //Real v_min(GetWhichMin());
  //Real v_max(GetWhichMax());
  Real v_min, v_max;
  pltAppStatePtr->GetMinMax(v_min, v_max);
  Real v_off;
  if(pltAppStatePtr->GetNumContours() != 0) {
    v_off = v_min + 0.5 * (v_max - v_min) / (Real) pltAppStatePtr->GetNumContours();
  } else {
    v_off = 1.0;
  }
  int hDir, vDir;
  if(myView == XZ) {
    hDir = XDIR;
    vDir = ZDIR;
  } else if(myView == YZ) {
    hDir = YDIR;
    vDir = ZDIR;
  } else {
    hDir = XDIR;
    vDir = YDIR;
  }
  
  const AmrData &amrData = dataServicesPtr->AmrDataRef();

  Array <Real> pos_low(BL_SPACEDIM);
  Array <Real> pos_high(BL_SPACEDIM);
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
      int lratio(CRRBetweenLevels(lev, lev+1, amrData.RefRatio()));
      // construct mask array.  must be size FAB.
      const BoxArray &nextFinest = amrData.boxArray(lev+1);
      for(int j(0); j < nextFinest.size(); ++j) {
        Box coarseBox(BoxLib::coarsen(nextFinest[j],lratio));
        if(coarseBox.intersects(passedSliceFab[lev]->box())) {
          coarseBox &= passedSliceFab[lev]->box();
          mask.setVal(true,coarseBox,0);
        }
      }
    }
    
    // get rid of the complement
    const BoxArray &levelBoxArray = amrData.boxArray(lev);
    BoxArray complement = BoxLib::complementIn(passedSliceFab[lev]->box(), 
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
    pltAppStatePtr->GetMinMax(minUsing, maxUsing);
    if((maxUsing - minUsing) < FLT_MIN) {
      oneOverGDiff = 0.0;
    } else {
      oneOverGDiff = 1.0 / (maxUsing - minUsing);
    }

    int drawColor;
    if(LowBlack()) {
      drawColor = palPtr->WhiteIndex();
    } else {
      drawColor = palPtr->BlackIndex();
    }
    for(int icont(0); icont < pltAppStatePtr->GetNumContours(); ++icont) {
      Real frac((Real) icont / pltAppStatePtr->GetNumContours());
      Real value(v_off + frac * (v_max - v_min));
      if(pltAppStatePtr->GetContourType() == COLORCONTOURS) {
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
                        Display *display, Drawable &dPixMap, const GC &gc,
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
  
  bool left, right, bottom, top;  // does contour line intersect this side?
  Real xLeft, yLeft;              // where contour line intersects left side
  Real xRight, yRight;            // where contour line intersects right side
  Real xBottom, yBottom;          // where contour line intersects bottom side
  Real xTop, yTop;                // where contour line intersects top side
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
      Real left_bottom(data[(i) + (j) * xLength]);         // left bottom value
      Real left_top(data[(i) + (j+1) * xLength]);          // left top value
      Real right_bottom(data[(i+1) + (j) * xLength]);      // right bottom value
      Real right_top(data[(i+1) + (j+1) * xLength]);       // right top value
      xLeft = leftEdge + xDiff*(i);
      xRight = xLeft + xDiff;
      yBottom = bottomEdge + yDiff * j;
      yTop = yBottom + yDiff;
      
      left = Between(left_bottom,value,left_top);
      right = Between(right_bottom,value,right_top);
      bottom = Between(left_bottom,value,right_bottom);
      top = Between(left_top,value,right_top);
      
      // figure out where things intersect the cell
      if(left) {
        if(left_bottom != left_top) {
          yLeft = yBottom + yDiff * (value - left_bottom) / (left_top-left_bottom);
        } else {
          yLeft = yBottom;
          bFailureStatus = true;
        }
      }
      if(right) {
        if(right_bottom != right_top) {
          yRight = yBottom + yDiff*(value-right_bottom)/(right_top-right_bottom);
        } else {
          yRight = yBottom;
          bFailureStatus = true;
        }
      }
      if(bottom) {
        if(left_bottom != right_bottom) {
          xBottom = xLeft + xDiff*(value-left_bottom)/(right_bottom-left_bottom);
        } else {
          xBottom = xRight;
          bFailureStatus = true;
        }
      }
      if(top) {
        if(left_top != right_top) {
          xTop = xLeft + xDiff*(value-left_top)/(right_top-left_top);
        } else {
          xTop = xRight;
          bFailureStatus = true;
        }
      }
      
      XSetForeground(display, xgc, palPtr->pixelate(FGColor));
      
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
        XDrawLine(display, dPixMap, gc, 
                  (int) xLeft,
		  (int) (imageSizeV - yLeft),
		  (int) xRight,
		  (int) (imageSizeV - yRight));
        XDrawLine(display, dPixMap, gc,
                  (int) xTop,
		  (int) (imageSizeV - yTop),
		  (int) xBottom,
		  (int) (imageSizeV - yBottom));
      } else if(top && bottom) {   // only intersects top and bottom sides
        XDrawLine(display, dPixMap, gc,
                  (int) xTop,
		  (int) (imageSizeV - yTop),
		  (int) xBottom,
		  (int) (imageSizeV - yBottom));
      } else if(left) {
        if(right) {
          XDrawLine(display, dPixMap, gc,
                    (int) xLeft,
		    (int) (imageSizeV - yLeft),
		    (int) xRight,
		    (int) (imageSizeV - yRight));
        } else if(top) {
          XDrawLine(display, dPixMap, gc,
                    (int) xLeft,
		    (int) (imageSizeV - yLeft),
		    (int) xTop,
		    (int) (imageSizeV-yTop));
        } else {
          XDrawLine(display, dPixMap, gc,
                    (int) xLeft,
		    (int) (imageSizeV - yLeft),
		    (int) xBottom,
		    (int) (imageSizeV - yBottom));
        }
      } else if(right) {
        if(top) {
          XDrawLine(display, dPixMap, gc,
                    (int) xRight,
		    (int) (imageSizeV - yRight),
		    (int) xTop,
		    (int) (imageSizeV - yTop));
        } else {
          XDrawLine(display, dPixMap, gc,
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
VectorDerived AmrPicture::FindVectorDerived(Array<string> &aVectorDeriveNames) {
  // figure out if we are using velocities, momentums, or if neither are available
  // first try velocities

  if(dataServicesPtr->CanDerive("x_velocity") &&
     dataServicesPtr->CanDerive("y_velocity")
#if (BL_SPACEDIM == 3)
     && dataServicesPtr->CanDerive("z_velocity")
#endif
    )
  {
    aVectorDeriveNames[0] = "x_velocity";
    aVectorDeriveNames[1] = "y_velocity";
#if (BL_SPACEDIM == 3)
    aVectorDeriveNames[2] = "z_velocity";
#endif
    return enVelocity;

  } else
    if(dataServicesPtr->CanDerive("xvel") &&
       dataServicesPtr->CanDerive("yvel")
#if (BL_SPACEDIM == 3)
       && dataServicesPtr->CanDerive("zvel")
#endif
    )
  {
    aVectorDeriveNames[0] = "xvel";
    aVectorDeriveNames[1] = "yvel";
#if (BL_SPACEDIM == 3)
    aVectorDeriveNames[2] = "zvel";
#endif
    return enVelocity;

  } else
    if(dataServicesPtr->CanDerive("x_vel") &&
       dataServicesPtr->CanDerive("y_vel")
#if (BL_SPACEDIM == 3)
       && dataServicesPtr->CanDerive("z_vel")
#endif
    )
  {
    aVectorDeriveNames[0] = "x_vel";
    aVectorDeriveNames[1] = "y_vel";
#if (BL_SPACEDIM == 3)
    aVectorDeriveNames[2] = "z_vel";
#endif
    return enVelocity;

  } else
    if(dataServicesPtr->CanDerive("x_momentum") &&
       dataServicesPtr->CanDerive("y_momentum")
#if (BL_SPACEDIM == 3)
       && dataServicesPtr->CanDerive("z_momentum")
#endif
       && dataServicesPtr->CanDerive("density")
    )
  {
    aVectorDeriveNames[0] = "x_momentum";
    aVectorDeriveNames[1] = "y_momentum";
#if (BL_SPACEDIM == 3)
    aVectorDeriveNames[2] = "z_momentum";
#endif
    return enMomentum;

  } else
    if(dataServicesPtr->CanDerive("xmom") &&
       dataServicesPtr->CanDerive("ymom")
#if (BL_SPACEDIM == 3)
       && dataServicesPtr->CanDerive("zmom")
#endif
       && dataServicesPtr->CanDerive("density")
    )
  {
    aVectorDeriveNames[0] = "xmom";
    aVectorDeriveNames[1] = "ymom";
#if (BL_SPACEDIM == 3)
    aVectorDeriveNames[2] = "zmom";
#endif
    return enMomentum;

  } else
    if(dataServicesPtr->CanDerive("x_mom") &&
       dataServicesPtr->CanDerive("y_mom")
#if (BL_SPACEDIM == 3)
       && dataServicesPtr->CanDerive("z_mom")
#endif
       && dataServicesPtr->CanDerive("density")
    )
  {
    aVectorDeriveNames[0] = "x_mom";
    aVectorDeriveNames[1] = "y_mom";
#if (BL_SPACEDIM == 3)
    aVectorDeriveNames[2] = "z_mom";
#endif
    return enMomentum;

  } else {
    return enNoneFound;
  }

}


// ---------------------------------------------------------------------
void AmrPicture::DrawVectorField(Display *pDisplay, 
                                 Drawable &pDrawable, const GC &pGC)
{
  int hDir, vDir;
  if(myView == XY) {
    hDir = XDIR;
    vDir = YDIR;
  } else if(myView == XZ) {
    hDir = XDIR;
    vDir = ZDIR;
  } else if(myView == YZ) {
    hDir = YDIR;
    vDir = ZDIR;
  } else {
    BL_ASSERT(false);
  }
  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  int DVFscale(pltAppStatePtr->CurrentScale());
  int maxDrawnLevel(pltAppStatePtr->MaxDrawnLevel());
  int maxAllowableLevel(pltAppStatePtr->MaxAllowableLevel());
  int DVFRatio(CRRBetweenLevels(maxDrawnLevel, 
                                maxAllowableLevel, amrData.RefRatio()));
  // get velocity field
  Box DVFSliceBox(sliceFab[maxDrawnLevel]->box());
  int maxLength(DVFSliceBox.longside());
  FArrayBox density(DVFSliceBox);
  FArrayBox hVelocity(DVFSliceBox);
  FArrayBox vVelocity(DVFSliceBox);
  VectorDerived whichVectorDerived;
  Array<string> choice(BL_SPACEDIM);

  whichVectorDerived = FindVectorDerived(choice);

  if(whichVectorDerived == enNoneFound) {
    cerr << "Could not find suitable quantities to create velocity vectors."
	 << endl;
    return;
  }

  if(whichVectorDerived == enVelocity) {
    // fill the velocities
    string hVel(choice[hDir]);
    string vVel(choice[vDir]);
    DataServices::Dispatch(DataServices::FillVarOneFab, dataServicesPtr, 
                           (void *) &hVelocity,
			   (void *) (&(hVelocity.box())),
                           maxDrawnLevel,
			   (void *) &hVel);
    DataServices::Dispatch(DataServices::FillVarOneFab, dataServicesPtr, 
                           (void *) &vVelocity,
			   (void *) (&(vVelocity.box())),
                           maxDrawnLevel,
			   (void *) &vVel);

  } else {  // using momentums
    BL_ASSERT(whichVectorDerived == enMomentum);
    // fill the density and momentum:
    string sDensity("density");
    if( ! dataServicesPtr->CanDerive(sDensity)) {
      cerr << "Found momentums in the plot file but not density." << endl;
      return;
    }

    string hMom(choice[hDir]);
    string vMom(choice[vDir]);

    DataServices::Dispatch(DataServices::FillVarOneFab, dataServicesPtr, 
                           (void *) &density,
			   (void *) (&(density.box())),
                           maxDrawnLevel,
			   (void *) &sDensity);
    DataServices::Dispatch(DataServices::FillVarOneFab, dataServicesPtr, 
                           (void *) &hVelocity,
			   (void *) (&(hVelocity.box())),
                           maxDrawnLevel,
			   (void *) &hMom);
    DataServices::Dispatch(DataServices::FillVarOneFab, dataServicesPtr, 
                           (void *) &vVelocity,
			   (void *) (&(vVelocity.box())),
                           maxDrawnLevel,
			   (void *) &vMom);

    // then divide to get velocity
    hVelocity /= density;
    vVelocity /= density;
  }
  
  // compute maximum speed
  Real smax(0.0);
  int npts(hVelocity.box().numPts());
  const Real *hdat = hVelocity.dataPtr();
  const Real *vdat = vVelocity.dataPtr();

  for(int k(0); k < npts; ++k) {
    Real s(sqrt(hdat[k] * hdat[k] + vdat[k] * vdat[k]));
    smax = max(smax,s);
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
