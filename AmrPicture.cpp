//BL_COPYRIGHT_NOTICE

//
// $Id: AmrPicture.cpp,v 1.41 1999-12-01 22:55:43 vince Exp $
//

// ---------------------------------------------------------------
// AmrPicture.cpp
// ---------------------------------------------------------------
#include "AmrPicture.H"
#include "PltApp.H"
#include "DataServices.H"
#ifdef BL_USE_NEW_HFILES
#include <ctime>
#else
#include <time.h>
#endif

#ifdef BL_USE_ARRAYVIEW
#include "ArrayView.H"
#endif

// ---------------------------------------------------------------------
AmrPicture::AmrPicture(int mindrawnlevel, GraphicsAttributes *gaptr,
		       PltApp *pltappptr, DataServices *dataservicesptr,
		       bool bcartgridsmoothing)
           : contours(false),
             raster(true),
             colContour(false),
             vectorField(false),
             minDrawnLevel(mindrawnlevel),
             GAptr(gaptr),
             pltAppPtr(pltappptr),
             myView(XY),
             isSubDomain(false),
             findSubRange(false),
             dataServicesPtr(dataservicesptr),
             bCartGridSmoothing(bcartgridsmoothing)
{
  int i, ilev;

  const AmrData &amrData = dataServicesPtr->AmrDataRef();

  if(UseSpecifiedMinMax()) {
    whichRange = USESPECIFIED;
    GetSpecifiedMinMax(dataMinSpecified, dataMaxSpecified);
  } else {
    whichRange = USEGLOBAL;
    dataMinSpecified = 0.0;
    dataMaxSpecified = 0.0;
  }
  showBoxes = PltApp::GetDefaultShowBoxes();

  finestLevel = amrData.FinestLevel();
  maxAllowableLevel =
          DetermineMaxAllowableLevel(amrData.ProbDomain()[finestLevel],
		        finestLevel, MaxPictureSize(), amrData.RefRatio());

  numberOfLevels    = maxAllowableLevel + 1;
  maxDrawnLevel     = maxAllowableLevel;
  maxLevelWithGrids = maxAllowableLevel;

  subDomain.resize(numberOfLevels);
  for(ilev = 0; ilev <= maxAllowableLevel; ++ilev) {  // set type
    subDomain[ilev].convert(amrData.ProbDomain()[0].type());
  }

  subDomain[maxAllowableLevel].setSmall
		(amrData.ProbDomain()[maxAllowableLevel].smallEnd());
  subDomain[maxAllowableLevel].setBig
		(amrData.ProbDomain()[maxAllowableLevel].bigEnd());
  for(i = maxAllowableLevel - 1; i >= minDrawnLevel; i--) {
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
  hLine = subDomain[maxAllowableLevel].bigEnd(YDIR) * pltAppPtr->CurrentScale();
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
AmrPicture::AmrPicture(int view, int mindrawnlevel, 
                       GraphicsAttributes *gaptr, Box region,
                       AmrPicture *parentPicturePtr,
                       PltApp *parentPltAppPtr, PltApp *pltappptr,
		       bool bcartgridsmoothing)
	   : myView(view),
             minDrawnLevel(mindrawnlevel),
             GAptr(gaptr),
             pltAppPtr(pltappptr),
             isSubDomain(true),
             bCartGridSmoothing(bcartgridsmoothing)
{
  BL_ASSERT(parentPicturePtr != NULL);
  BL_ASSERT(pltappptr != NULL);
  int ilev;

  contours = parentPicturePtr->Contours();
  raster = parentPicturePtr->Raster();
  colContour = parentPicturePtr->ColorContour();
  vectorField = parentPicturePtr->VectorField();

  if(myView == XY) {
    findSubRange = true;
  } else {
    findSubRange = false;
  }
  dataServicesPtr = pltAppPtr->GetDataServicesPtr();
  const AmrData &amrData = dataServicesPtr->AmrDataRef();

  whichRange = parentPicturePtr->GetWhichRange();
  dataMinSpecified = parentPicturePtr->GetSpecifiedMin();
  dataMaxSpecified = parentPicturePtr->GetSpecifiedMax();
  showBoxes = parentPicturePtr->ShowingBoxes();

  finestLevel = amrData.FinestLevel();
  maxAllowableLevel = DetermineMaxAllowableLevel(region, finestLevel,
				   MaxPictureSize(), amrData.RefRatio());

  numberOfLevels    = maxAllowableLevel + 1;
  maxDrawnLevel     = maxAllowableLevel;
  maxLevelWithGrids = maxAllowableLevel;
 
  subDomain.resize(numberOfLevels);
  // region is at the finestLevel
  subDomain[maxAllowableLevel] = region.coarsen(CRRBetweenLevels(maxAllowableLevel,
					     finestLevel, amrData.RefRatio()));

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
		pltAppPtr->CurrentScale();
      vLine = 0;
    } else if(myView==XZ) {
      hLine = (subDomain[maxAllowableLevel].bigEnd(ZDIR) -
      		subDomain[maxAllowableLevel].smallEnd(ZDIR)) *
		pltAppPtr->CurrentScale();
      vLine = 0;
    } else {
      hLine = (subDomain[maxAllowableLevel].bigEnd(ZDIR) -
      		subDomain[maxAllowableLevel].smallEnd(ZDIR)) *
		pltAppPtr->CurrentScale();
      vLine = 0;
    }


    if(parentPltAppPtr != NULL) {
      int tempSlice = parentPltAppPtr->GetAmrPicturePtr(myView)->GetSlice();
      tempSlice *= CRRBetweenLevels(parentPltAppPtr->MaxDrawnLevel(), 
                                    maxDrawnLevel, amrData.RefRatio());
      slice = Max(Min(tempSlice,
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
  numberOfContours = 0;
  int iLevel;
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
  //xImageArray.resize(numberOfLevels, NULL);
  xImageArray.resize(numberOfLevels);
  //xImageCreated.resize(numberOfLevels, false);

  // use maxAllowableLevel because all imageSizes are the same
  // regardless of which level is showing
  // dont use imageBox.length() because of node centered boxes
  imageSizeH = pltAppPtr->CurrentScale() * dataSizeH[maxAllowableLevel];
  imageSizeV = pltAppPtr->CurrentScale() * dataSizeV[maxAllowableLevel];
  imageSize = imageSizeH * imageSizeV;

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
  maxsFound = false;
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
void AmrPicture::SetHVLine() {
  int first(0);
  for(int i = 0; i <= YZ; ++i) {
    if(i == myView) {
      // do nothing
    } else {
      if(first == 0) {
        hLine = imageSizeV-1 -
		((pltAppPtr->GetAmrPicturePtr(i)->GetSlice() -
		  pltAppPtr->GetAmrPicturePtr(YZ - i)->
		  GetSubDomain()[maxDrawnLevel].smallEnd(YZ - i)) *
		  pltAppPtr->CurrentScale());
        first = 1;
      } else {
        vLine = ( pltAppPtr->GetAmrPicturePtr(i)->GetSlice() -
		  pltAppPtr->GetAmrPicturePtr(YZ - i)->
		  GetSubDomain()[maxDrawnLevel].smallEnd(YZ - i)) *
		  pltAppPtr->CurrentScale();
      }
    }
  }
}


// ---------------------------------------------------------------------
AmrPicture::~AmrPicture() {
  if(framesMade) {
    for(int iSlice = 0;
        iSlice < subDomain[maxAllowableLevel].length(sliceDir); ++iSlice)
    {
      XDestroyImage(frameBuffer[iSlice]);
    }
  }
  for(int iLevel = minDrawnLevel; iLevel <= maxAllowableLevel; ++iLevel) {
    delete [] imageData[iLevel];
    free(scaledImageData[iLevel]);
    delete sliceFab[iLevel];
  }

  if(pixMapCreated) {
    XFreePixmap(GAptr->PDisplay(), pixMap);
  }
}


// ---------------------------------------------------------------------
void AmrPicture::SetSlice(int view, int here) {
  int lev;
  sliceDir = YZ - view;
  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  BL_ASSERT(amrData.RefRatio().length() > 0);

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

  for(lev = minDrawnLevel; lev < gpArray.length(); ++lev) {
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
    for(int iBox = 0; iBox < amrData.boxArray(lev).length(); ++iBox) {
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
		pltAppPtr->CurrentScale(), imageSizeH, imageSizeV,
		temp, sliceDataBox, sliceDir);
        ++gridNumber;
      }
    }
  }
}  // end SetSlice(...)


// ---------------------------------------------------------------------
void AmrPicture::ChangeContour(int array_val) {
  bool tmp_raster(raster);
  if(array_val == 0) {
    SetRasterOnly();
  } else if(array_val == 1) {
    SetRasterContour();
  } else if(array_val == 2) {
    SetColorContour();
  } else if(array_val == 3) {
    SetBWContour();
  } else if(array_val == 4) {
    SetVectorField();
  }
  // raster hasn't changed the image so all we
  // have to do is redraw the contours
  if(raster == tmp_raster) {
    Draw(minDrawnLevel, maxDrawnLevel);
  } else {
    ChangeRasterImage();  // instead we want to recreate the image
			  // -- though we shouldn't have to reread
			  // the data, as we do now
  }
}


// ---------------------------------------------------------------------
void AmrPicture::SetRasterOnly() {
  contours = false;
  raster = true;
  colContour = false;
  vectorField = false;
}

 
// ---------------------------------------------------------------------
void AmrPicture::SetRasterContour() {
  contours = true;
  raster = true;
  colContour = false;
  vectorField = false;
}


// ---------------------------------------------------------------------
void AmrPicture::SetColorContour() {
  contours = true;
  raster = false;
  colContour = true;
  vectorField = false;
}


// ---------------------------------------------------------------------
void AmrPicture::SetBWContour() {
  contours = true;
  raster = false;
  colContour = false;
  vectorField = false;
}


// ---------------------------------------------------------------------
void AmrPicture::SetVectorField() {
  contours = false;
  raster = true;
  colContour = false;
  vectorField = true;
}


// ---------------------------------------------------------------------
void AmrPicture::DrawBoxes(Array< Array<GridPicture> > &gp,
			   Drawable &drawable)
{
  short xbox, ybox;
  unsigned short wbox, hbox;
  bool bIsWindow(true);
  bool bIsPixmap(false);
  const AmrData &amrData = dataServicesPtr->AmrDataRef();

  if(showBoxes) {
    for(int level = minDrawnLevel; level <= maxDrawnLevel; ++level) {
      if(level == minDrawnLevel) {
        XSetForeground(GAptr->PDisplay(), GAptr->PGC(), palPtr->WhiteIndex());
      } else {
        XSetForeground(GAptr->PDisplay(), GAptr->PGC(),
		       MaxPaletteIndex()-80*(level-1));
      }
      if(amrData.Terrain()) {
	DrawTerrBoxes(level, bIsWindow, bIsPixmap);
      } else {
        for(int i = 0; i < gp[level].length(); ++i) {
	  xbox = gp[level][i].HPositionInPicture();
	  ybox = gp[level][i].VPositionInPicture();
	  wbox = gp[level][i].ImageSizeH(); 
	  hbox = gp[level][i].ImageSizeV(); 

          XDrawRectangle(GAptr->PDisplay(), drawable,  GAptr->PGC(),
			  xbox, ybox, wbox, hbox);
        }
      }
    }
  }
  // draw bounding box
  XSetForeground(GAptr->PDisplay(), GAptr->PGC(), palPtr->WhiteIndex());
  XDrawRectangle(GAptr->PDisplay(), drawable, GAptr->PGC(), 0, 0,
			imageSizeH-1, imageSizeV-1);
}


// ---------------------------------------------------------------------
void AmrPicture::DrawTerrBoxes(int level, bool bIsWindow, bool bIsPixmap) {
/*
  int i, lev, ix;
  short xbox, ybox;
  unsigned short wbox, hbox;

  BL_ASSERT((bIsWindow ^ bIsPixmap) == true);

  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  int expansion(pltAppPtr->GetExpansion());
  int hScale(pltAppPtr->CurrentScale() * expansion);

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

#if 0
  if(level > 0) {
    Real zhi(-1000000.0);
    for(i = 0; i < gpArray[0].length(); ++i) {
      GridPlot *gridptr_crse = gpArray[0][i].GetGridPtr();
      DataBoxPlot *meshdb_crse = gridptr_crse->Mesh();
      zhi = Max(zhi, meshdb_crse->max(0));
    }
    index_zhi = vScale * zhi / dx[0];
  }
#endif

      for(i = 0; i < gpArray[level].length(); ++i) {
        xbox = gpArray[level][i].HPositionInPicture();
        ybox = gpArray[level][i].VPositionInPicture();
        wbox = gpArray[level][i].ImageSizeH();
        hbox = gpArray[level][i].ImageSizeV();

        if(level == 0) {
          if(bIsWindow) {
            XDrawRectangle(GAptr->PDisplay(), pictureWindow,  GAptr->PGC(),
                           xbox, ybox, wbox, hbox);
          } else if(bIsPixmap) {
            XDrawRectangle(GAptr->PDisplay(), pixMap,  GAptr->PGC(),
                           xbox, ybox, wbox, hbox);
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
            XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
                      xlo, ylo, xhi, yhi);
          } else if(bIsPixmap) {
            XDrawLine(GAptr->PDisplay(), pixMap, GAptr->PGC(),
                      xlo, ylo, xhi, yhi);
          }

          // Draw right boundary
          ylo = index_zhi - vScale * meshdat[M_L(mhi[0], mlo[1])] / dx[0];
          yhi = index_zhi - vScale * meshdat[M_L(mhi[0], mhi[1])] / dx[0];
          xlo = hScale * mhi[0];
          xhi = hScale * mhi[0];
          if(bIsWindow) {
            XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
                      xlo, ylo, xhi, yhi);
          } else if(bIsPixmap) {
            XDrawLine(GAptr->PDisplay(), pixMap, GAptr->PGC(),
                      xlo, ylo, xhi, yhi);
          }

          // Draw bottom boundary
          for(ix = mlo[0]; ix < mhi[0]; ++ix) {
            ylo = index_zhi-vScale*meshdat[M_L(ix,mlo[1])] / dx[0];
            yhi = index_zhi-vScale*meshdat[M_L(ix+1,mlo[1])] / dx[0];
            xlo = hScale * ix;
            xhi = hScale * (ix + 1);
            if(bIsWindow) {
              XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
                        xlo, ylo, xhi, yhi);
            } else if(bIsPixmap) {
              XDrawLine(GAptr->PDisplay(), pixMap, GAptr->PGC(),
                        xlo, ylo, xhi, yhi);
            }
          }

          // Draw top boundary
          for(ix = mlo[0]; ix < mhi[0]; ++ix) {
            ylo = index_zhi - vScale * meshdat[M_L(ix, mhi[1])] / dx[0];
            yhi = index_zhi - vScale * meshdat[M_L(ix + 1, mhi[1])] / dx[0];
            xlo = hScale * ix;
            xhi = hScale * (ix + 1);
            if(bIsWindow) {
              XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
                        xlo, ylo, xhi, yhi);
            } else if(bIsPixmap) {
              XDrawLine(GAptr->PDisplay(), pixMap, GAptr->PGC(),
                        xlo, ylo, xhi, yhi);
            }
          }
#endif
        }
      }
*/
}


// ---------------------------------------------------------------------
void AmrPicture::Draw(int fromLevel, int toLevel) {
  if( ! pixMapCreated) {
    pixMap = XCreatePixmap(GAptr->PDisplay(), pictureWindow,
			   imageSizeH, imageSizeV, GAptr->PDepth());
    pixMapCreated = true;
  }  
 
  XPutImage(GAptr->PDisplay(), pixMap, GAptr->PGC(),
            xImageArray[toLevel], 0, 0, 0, 0, imageSizeH, imageSizeV);
           
  if(contours) {
    DrawContour(sliceFab, GAptr->PDisplay(), pixMap, GAptr->PGC());
  } else if(vectorField) {
    DrawVectorField(GAptr->PDisplay(), pixMap, GAptr->PGC());
  }

  if( ! pltAppPtr->Animating()) {
    DoExposePicture();
  }
}  // end AmrPicture::Draw.


// ---------------------------------------------------------------------
void AmrPicture::SyncPicture() {
  if(pendingTimeOut != 0) {
    SetSlice(myView, slice);
    ChangeDerived(currentDerived, palPtr);
  }
}


// ---------------------------------------------------------------------
void AmrPicture::ToggleBoxes() {
  showBoxes = (showBoxes ? false : true);
  DoExposePicture();
}


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
    XPutImage(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
      pltAppPtr->CurrentFrameXImage(), 0, 0, 0, 0, imageSizeH, imageSizeV);
  } else {
    if(pendingTimeOut == 0) {
      XCopyArea(GAptr->PDisplay(), pixMap, pictureWindow, 
 		GAptr->PGC(), 0, 0, imageSizeH, imageSizeV, 0, 0); 

      DrawBoxes(gpArray, pictureWindow);

      if( ! subCutShowing) {   // draw selected region
        XSetForeground(GAptr->PDisplay(), GAptr->PGC(), 60);
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  regionX+1, regionY+1, region2ndX+1, regionY+1); 
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  regionX+1, regionY+1, regionX+1, region2ndY+1); 
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  regionX+1, region2ndY+1, region2ndX+1, region2ndY+1); 
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  region2ndX+1, regionY+1, region2ndX+1, region2ndY+1);

        XSetForeground(GAptr->PDisplay(), GAptr->PGC(), 175);
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  regionX, regionY, region2ndX, regionY); 
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  regionX, regionY, regionX, region2ndY); 
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  regionX, region2ndY, region2ndX, region2ndY); 
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  region2ndX, regionY, region2ndX, region2ndY);
      }

#if (BL_SPACEDIM == 3)
      // draw plane "cutting" lines
      XSetForeground(GAptr->PDisplay(), GAptr->PGC(), hColor);
      XDrawLine(GAptr->PDisplay(), pictureWindow,
                GAptr->PGC(), 0, hLine, imageSizeH, hLine); 
      XSetForeground(GAptr->PDisplay(), GAptr->PGC(), vColor);
      XDrawLine(GAptr->PDisplay(), pictureWindow,
                GAptr->PGC(), vLine, 0, vLine, imageSizeV); 
      
      XSetForeground(GAptr->PDisplay(), GAptr->PGC(), hColor-30);
      XDrawLine(GAptr->PDisplay(), pictureWindow,
                GAptr->PGC(), 0, hLine+1, imageSizeH, hLine+1); 
      XSetForeground(GAptr->PDisplay(), GAptr->PGC(), vColor-30);
      XDrawLine(GAptr->PDisplay(), pictureWindow,
                GAptr->PGC(), vLine+1, 0, vLine+1, imageSizeV); 
      
      if(subCutShowing) {
        // draw subvolume cutting border 
        XSetForeground(GAptr->PDisplay(), GAptr->PGC(), 90);
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  subcutX+1, subcutY+1, subcut2ndX+1, subcutY+1); 
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  subcutX+1, subcutY+1, subcutX+1, subcut2ndY+1); 
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  subcutX+1, subcut2ndY+1, subcut2ndX+1, subcut2ndY+1); 
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  subcut2ndX+1, subcutY+1, subcut2ndX+1, subcut2ndY+1);
          
        XSetForeground(GAptr->PDisplay(), GAptr->PGC(), 155);
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  subcutX, subcutY, subcut2ndX, subcutY); 
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  subcutX, subcutY, subcutX, subcut2ndY); 
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  subcutX, subcut2ndY, subcut2ndX, subcut2ndY); 
        XDrawLine(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
		  subcut2ndX, subcutY, subcut2ndX, subcut2ndY);
      }
#endif
    }
  }
}  // end DoExposePicture


// ---------------------------------------------------------------------
void AmrPicture::ChangeSlice(int here) {
  if(pendingTimeOut != 0) {
    DoStop();
  }
  SetSlice(myView, here);
  ChangeDerived(currentDerived, palPtr);
}


// ---------------------------------------------------------------------
void AmrPicture::CreatePicture(Window drawPictureHere, Palette *palptr,
				const aString &derived)
{
  palPtr = palptr;
  currentDerived = derived;
  pictureWindow = drawPictureHere;
  ChangeDerived(currentDerived, palptr);
}


// ---------------------------------------------------------------------
void AmrPicture::ChangeDerived(aString derived, Palette *palptr) {
  int lev;
  Real dataMin, dataMax;
  int printDone = false;
  BL_ASSERT(palptr != NULL);
  palPtr = palptr;
  AmrData &amrData = dataServicesPtr->AmrDataRef();
  if(currentDerived != derived || whichRange == USEFILE) {
    maxsFound = false;
    pltAppPtr->PaletteDrawn(false);
  }

  if( ! maxsFound) {
    if(framesMade) {
      for(int i = 0; i < subDomain[maxAllowableLevel].length(sliceDir); ++i) {
        XDestroyImage(frameBuffer[i]);
      }
      framesMade = false;
    }
    if(pendingTimeOut != 0) {
      DoStop();
    }
    maxsFound = true;
    currentDerived =  derived;
    if(myView == XY) {
      aString outbuf = "Reading data for ";
      outbuf += pltAppPtr->GetFileName();
      outbuf += " ...";
      strcpy(buffer, outbuf.c_str());
      PrintMessage(buffer);
      if( ! pltAppPtr->IsAnim() || whichRange == USEFILE) {
        dataMinAllGrids =  AV_BIG_REAL;
        dataMaxAllGrids = -AV_BIG_REAL;
        dataMinRegion   =  AV_BIG_REAL;
        dataMaxRegion   = -AV_BIG_REAL;
        dataMinFile     =  AV_BIG_REAL;
        dataMaxFile     = -AV_BIG_REAL;

        // Call Derive for each Grid in AmrData. This finds the
        // global & sub domain min and max.
        for(lev = minDrawnLevel; lev <= maxDrawnLevel; lev++) {
	  if(Verbose()) {
            cout << "Deriving into Grids at level " << lev << endl;
	  }
	  bool minMaxValid(false);
          DataServices::Dispatch(DataServices::MinMaxRequest,
			         dataServicesPtr,
			         (void *) (&(amrData.ProbDomain()[lev])),
				 (void *) &currentDerived,
				 lev, &dataMin, &dataMax, &minMaxValid);
          dataMinAllGrids = Min(dataMinAllGrids, dataMin);
          dataMaxAllGrids = Max(dataMaxAllGrids, dataMax);
          dataMinFile = Min(dataMinFile, dataMin);
          dataMaxFile = Max(dataMaxFile, dataMax);
	  if(findSubRange && lev <= maxAllowableLevel && lev <= maxLevelWithGrids) {
            DataServices::Dispatch(DataServices::MinMaxRequest,
			           dataServicesPtr,
                                   (void *) (&(subDomain[lev])),
				   (void *) &currentDerived, lev,
	                           &dataMin, &dataMax, &minMaxValid);
	    if(minMaxValid) {
              dataMinRegion = Min(dataMinRegion, dataMin);
             dataMaxRegion = Max(dataMaxRegion, dataMax);
	    }
	  }
	}  // end for(lev...)
      } else {
        dataMinAllGrids = pltAppPtr->GlobalMin();
        dataMaxAllGrids = pltAppPtr->GlobalMax();
        dataMinRegion   = pltAppPtr->GlobalMin();
        dataMaxRegion   = pltAppPtr->GlobalMax();
      }
      sprintf(buffer, "...");
      printDone = true;
      PrintMessage(buffer);
      if( ! findSubRange) {
	dataMinRegion = dataMinAllGrids;
	dataMaxRegion = dataMaxAllGrids;
      }
    } else {
      dataMinAllGrids = pltAppPtr->GetAmrPicturePtr(XY)->GetMin();
      dataMaxAllGrids = pltAppPtr->GetAmrPicturePtr(XY)->GetMax();
      dataMinRegion   = pltAppPtr->GetAmrPicturePtr(XY)->GetRegionMin();
      dataMaxRegion   = pltAppPtr->GetAmrPicturePtr(XY)->GetRegionMax();
    }

  }  // end if( ! maxsFound)
  switch(whichRange) {
	case USEGLOBAL:
    		minUsing = dataMinAllGrids;
    		maxUsing = dataMaxAllGrids;
	break;
	case USELOCAL:
    		minUsing = dataMinRegion;
    		maxUsing = dataMaxRegion;
	break;
	case USESPECIFIED:
    		minUsing = dataMinSpecified;
    		maxUsing = dataMaxSpecified;
	break;
	case USEFILE:
    		minUsing = dataMinFile;
    		maxUsing = dataMaxFile;
	break;
	default:
    		minUsing = dataMinAllGrids;
    		maxUsing = dataMaxAllGrids;
	break;
  }
  VSHOWVAL(Verbose(), minUsing)
  VSHOWVAL(Verbose(), maxUsing)

  for(int iLevel = minDrawnLevel; iLevel <= maxAllowableLevel; ++iLevel) {
    DataServices::Dispatch(DataServices::FillVarOneFab, dataServicesPtr,
		           (void *) (sliceFab[iLevel]),
			   (void *) (&(sliceFab[iLevel]->box())),
			   iLevel,
			   (void *) &derived);
    CreateImage(*(sliceFab[iLevel]), imageData[iLevel],
 		dataSizeH[iLevel], dataSizeV[iLevel],
 	        minUsing, maxUsing, palPtr);
    CreateScaledImage(&(xImageArray[iLevel]), pltAppPtr->CurrentScale() *
                CRRBetweenLevels(iLevel, maxAllowableLevel, amrData.RefRatio()),
                imageData[iLevel], scaledImageData[iLevel],
                dataSizeH[iLevel], dataSizeV[iLevel],
                imageSizeH, imageSizeV);
  }
  if( ! pltAppPtr->PaletteDrawn()) {
    pltAppPtr->PaletteDrawn(true);
    palptr->Draw(minUsing, maxUsing, pltAppPtr->GetFormatString());
  }
  Draw(minDrawnLevel, maxDrawnLevel);
  if(printDone) {
    sprintf(buffer, "done.\n");
    PrintMessage(buffer);
  }

}  // end ChangeDerived


// -------------------------------------------------------------------
void AmrPicture::ChangeRasterImage() {
  AmrData &amrData = dataServicesPtr->AmrDataRef();
  for(int iLevel = minDrawnLevel; iLevel <= maxAllowableLevel; ++iLevel) {
    CreateImage(*(sliceFab[iLevel]), imageData[iLevel],
 		dataSizeH[iLevel], dataSizeV[iLevel],
 	        minUsing, maxUsing, palPtr);
    CreateScaledImage(&(xImageArray[iLevel]), pltAppPtr->CurrentScale() *
               CRRBetweenLevels(iLevel, maxAllowableLevel, amrData.RefRatio()),
               imageData[iLevel], scaledImageData[iLevel],
               dataSizeH[iLevel], dataSizeV[iLevel],
               imageSizeH, imageSizeV);
  }
  if( ! pltAppPtr->PaletteDrawn()) {
    pltAppPtr->PaletteDrawn(true);
    palPtr->Draw(minUsing, maxUsing, pltAppPtr->GetFormatString());
  }
  Draw(minDrawnLevel, maxDrawnLevel);
  
}  // end ChangeRasterImage


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
  int whiteIndex(palptr->WhiteIndex());
  int blackIndex(palptr->BlackIndex());
  int colorSlots(palptr->ColorSlots());
  int paletteStart(palptr->PaletteStart());
  int paletteEnd(palptr->PaletteEnd());
  int csm1(colorSlots - 1);
  
  Real dPoint;
  
  // flips the image in Vert dir: j => datasizev-j-1
  if(raster) {
    for(int j = 0; j < datasizev; ++j) {
      jdsh = j * datasizeh;
      jtmp1 = (datasizev-j-1) * datasizeh;
      for(int i = 0; i < datasizeh; ++i) {
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
      for(int i = 0; i < (datasizeh * datasizev); ++i) {
        imagedata[i] = blackIndex;
      }
    } else {
      for(int i = 0; i < (datasizeh * datasizev); ++i) {
        imagedata[i] = whiteIndex;
      }
    }
  }  // end if(raster)
}  // end CreateImage(...)


// ---------------------------------------------------------------------
void AmrPicture::CreateScaledImage(XImage **ximage, int scale,
				   unsigned char *imagedata,
				   unsigned char *scaledimagedata,
				   int datasizeh, int datasizev,
				   int imagesizeh, int imagesizev)
{ 
  int i, j, jish, jtmp;

  if( ! bCartGridSmoothing) {
    for(j = 0; j < imagesizev; ++j) {
      jish = j*imagesizeh;
      jtmp =  datasizeh * (j/scale);
      for(i = 0; i < imagesizeh; ++i) {
        scaledimagedata[i+jish] = imagedata [ (i/scale) + jtmp ];
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

    for(j = 0; j < dataSizeV; j++) {
      for(i = 0; i < dataSizeH; i++) {
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
            //minDAV = Min(diffAvgV[isum], diffAvgV[isum+1]);
            //maxDAV = Max(diffAvgV[isum], diffAvgV[isum+1]);
          //}
          //for(isum=nStartH; isum<=nEndH; isum++) {
            //minDAH = Min(diffAvgH[isum], diffAvgH[isum+1]);
            //maxDAH = Max(diffAvgH[isum], diffAvgH[isum+1]);
          //}
          normH =  ((diffAvgV[nEndV] - diffAvgV[nStartV]) * ((Real) nH));
                                                  // nH is correct here
          normV = -((diffAvgH[nEndH] - diffAvgH[nStartH]) * ((Real) nV));
                                                  // nV is correct here

          for(ii=0; ii<rrcs*rrcs; ii++) {
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
            iCurrent++;
            if(iCurrent > rrcs) {
              iCurrent = 1;
              jCurrent++;
            }
            if(jCurrent > rrcs) {
              break;
            }
            for(ii=0; ii<iCurrent; ii++) {
              yBody = (slope * ((iCurrent-ii)*cellDx)) + (jCurrent*cellDy);
              jBody = Min(rrcs-1, (int) (yBody/cellDy));
              for(jj=0; jj<=jBody; jj++) {
                isIndex = ii + ((rrcs-(jj+1))*rrcs);
                imageStencil[isIndex] = bodyCell;  // yflip
              }
            }

            // sum the body cells
            // do it this way to allow redundant setting of body cells
            nCalcBodyCells = 0;
            for(ii=0; ii<rrcs*rrcs; ii++) {
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
                jBody = Max(0, (int) (rrcs - (yBody / cellDy)));
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
              jBody = Min(rrcs-1, (int) (yBody/cellDy));
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
              jBody = Max(0, (int) (rrcs - (yBody / cellDy)));
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
              for(jj=0; jj<rrcs; jj++) {
                for(ii=0; ii<(nBodyCells/rrcs); ii++) {
                  isIndex = ii + ((rrcs-(jj+1))*rrcs);
                  imageStencil[isIndex] = bodyCell;
                }
              }
            } else {           // body is on the right edge of the cell
              for(jj=0; jj<rrcs; jj++) {
                for(ii=rrcs-(nBodyCells/rrcs); ii<rrcs; ii++) {
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

  *ximage = XCreateImage(GAptr->PDisplay(), GAptr->PVisual(),
		GAptr->PDepth(), ZPixmap, 0, (char *) scaledimagedata,
		imagesizeh, imagesizev,
		XBitmapPad(GAptr->PDisplay()), imagesizeh);

  (*ximage)->byte_order = MSBFirst;
  (*ximage)->bitmap_bit_order = MSBFirst;

}  // end CreateScaledImage()


// ---------------------------------------------------------------------
void AmrPicture::ChangeScale(int newScale) { 
  int iLevel;
  if(framesMade) {
    for(int i = 0; i < subDomain[maxAllowableLevel].length(sliceDir); ++i) {
      XDestroyImage(frameBuffer[i]);
    }
    framesMade = false;
  }
  if(pendingTimeOut != 0) {
    DoStop();
  }
  imageSizeH = newScale   * dataSizeH[maxAllowableLevel];
  imageSizeV = newScale   * dataSizeV[maxAllowableLevel];
  imageSize  = imageSizeH * imageSizeV;
  XClearWindow(GAptr->PDisplay(), pictureWindow);

  if(pixMapCreated) {
    XFreePixmap(GAptr->PDisplay(), pixMap);
  }  
  pixMap = XCreatePixmap(GAptr->PDisplay(), pictureWindow,
			   imageSizeH, imageSizeV, GAptr->PDepth());
  pixMapCreated = true;

  for(iLevel = minDrawnLevel; iLevel <= maxAllowableLevel; ++iLevel) {
   for(int grid = 0; grid < gpArray[iLevel].length(); ++grid) {
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

  hLine = ((hLine / (pltAppPtr->PreviousScale())) * newScale) + (newScale - 1);
  vLine = ((vLine / (pltAppPtr->PreviousScale())) * newScale) + (newScale - 1);
  Draw(minDrawnLevel, maxDrawnLevel);
}


// ---------------------------------------------------------------------
void AmrPicture::ChangeLevel(int lowLevel, int hiLevel) { 
  minDrawnLevel = lowLevel;
  maxDrawnLevel = hiLevel;
  if(framesMade) {
    for(int i = 0; i < subDomain[maxAllowableLevel].length(sliceDir); ++i) {
      XDestroyImage(frameBuffer[i]);
    }
    framesMade = false;
  }
  if(pendingTimeOut != 0) {
    DoStop();
  }
  XClearWindow(GAptr->PDisplay(), pictureWindow);

  Draw(minDrawnLevel, maxDrawnLevel);
}


// ---------------------------------------------------------------------
XImage *AmrPicture::GetPictureXImage() {
  int xbox, ybox, wbox, hbox;
  XImage *ximage;

  if(showBoxes) {
    for(int level = minDrawnLevel; level <= maxDrawnLevel; ++level) {
      if(level == minDrawnLevel) {
        XSetForeground(GAptr->PDisplay(), GAptr->PGC(), palPtr->WhiteIndex());
      } else {
        XSetForeground(GAptr->PDisplay(), GAptr->PGC(), MaxPaletteIndex()-80*level);
      }
      for(int i = 0; i < gpArray[level].length(); ++i) {
	xbox = gpArray[level][i].HPositionInPicture();
	ybox = gpArray[level][i].VPositionInPicture();
	wbox = gpArray[level][i].ImageSizeH(); 
	hbox = gpArray[level][i].ImageSizeV(); 

        XDrawRectangle(GAptr->PDisplay(), pixMap,  GAptr->PGC(),
			xbox, ybox, wbox, hbox);
      }
    }
  }
  XSetForeground(GAptr->PDisplay(), GAptr->PGC(), MaxPaletteIndex());
  XDrawRectangle(GAptr->PDisplay(), pixMap, GAptr->PGC(), 0, 0,
			imageSizeH-1, imageSizeV-1);

  ximage = XGetImage(GAptr->PDisplay(), pixMap, 0, 0,
		imageSizeH, imageSizeV, AllPlanes, ZPixmap);

  if(pixMapCreated) {
    XFreePixmap(GAptr->PDisplay(), pixMap);
  }  
  pixMap = XCreatePixmap(GAptr->PDisplay(), pictureWindow,
			   imageSizeH, imageSizeV, GAptr->PDepth());
  pixMapCreated = true;
  Draw(minDrawnLevel, maxDrawnLevel);
  return ximage;
}


// ---------------------------------------------------------------------
void AmrPicture::CoarsenSliceBox() {
  for(int i = maxAllowableLevel - 1; i >= minDrawnLevel; --i) {
    sliceBox[i] = sliceBox[maxAllowableLevel];
    sliceBox[i].coarsen(CRRBetweenLevels(i, maxAllowableLevel,
			dataServicesPtr->AmrDataRef().RefRatio()));
  }
}


// ---------------------------------------------------------------------
void AmrPicture::CreateFrames(AnimDirection direction) {
  int start, length, cancelled;
  int islice, i, j, lev, gridNumber;
  Array<int> intersectGrids;
  int maxLevelWithGridsHere;
  int posneg(1);
  if(direction == ANIMNEGDIR) {
    posneg = -1;
  }
  const AmrData &amrData = dataServicesPtr->AmrDataRef();

  sprintf(buffer, "Creating frames..."); 
  PrintMessage(buffer);
  cancelled = false;
  Array<Box> interBox(numberOfLevels);
  interBox[maxAllowableLevel] = subDomain[maxAllowableLevel];
  start = subDomain[maxAllowableLevel].smallEnd(sliceDir);
  length = subDomain[maxAllowableLevel].length(sliceDir);
  if(framesMade) {
    for(int ii = 0; ii < subDomain[maxAllowableLevel].length(sliceDir); ++ii) {
      XDestroyImage(frameBuffer[ii]);
    }
  }
  frameGrids.resize(length); 
  frameBuffer.resize(length);
  unsigned char *frameImageData = new unsigned char[dataSize[maxDrawnLevel]];
  int iEnd = 0;
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
      for(int iBox = 0; iBox < amrData.boxArray(lev).length(); ++iBox) {
	Box temp(amrData.boxArray(lev)[iBox]);
        if(interBox[lev].intersects(temp)) {
	  temp &= interBox[lev];
          Box sliceDataBox(temp);
	  temp.shift(XDIR, -subDomain[lev].smallEnd(XDIR));
	  temp.shift(YDIR, -subDomain[lev].smallEnd(YDIR));
          temp.shift(ZDIR, -subDomain[lev].smallEnd(ZDIR));
          frameGrids[islice][lev][gridNumber].GridPictureInit(lev,
                  CRRBetweenLevels(lev, maxAllowableLevel, amrData.RefRatio()),
                  pltAppPtr->CurrentScale(), imageSizeH, imageSizeV,
                  temp, sliceDataBox, sliceDir);
          ++gridNumber;
        }
      }
    }   // end for(lev...)

    // get the data for this slice
    FArrayBox imageFab(interBox[maxDrawnLevel], 1);
    DataServices::Dispatch(DataServices::FillVarOneFab, dataServicesPtr,
			   (void *) &imageFab,
			   (void *) (&(imageFab.box())),
			   maxDrawnLevel,
                           (void *) &currentDerived);

    CreateImage(imageFab, frameImageData,
		dataSizeH[maxDrawnLevel], dataSizeV[maxDrawnLevel],
                minUsing, maxUsing, palPtr);

    // this cannot be deleted because it belongs to the XImage
    unsigned char *frameScaledImageData;
    frameScaledImageData = (unsigned char *)malloc(imageSize);
    CreateScaledImage(&(frameBuffer[islice]), pltAppPtr->CurrentScale() *
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
    if(XCheckMaskEvent(GAptr->PDisplay(), ButtonPressMask, &event)) {
      if(event.xany.window == XtWindow(pltAppPtr->GetStopButtonWidget())) {
        XPutBackEvent(GAptr->PDisplay(), &event);
        cancelled = true;
        iEnd = i;
        break;
      }
    }
#endif
  }  // end for(i=0; ...)

  delete [] frameImageData;

  if(cancelled) {
    for(int i = 0; i <= iEnd; ++i) {
      int iDestroySlice((((slice - start + (posneg * i)) + length) % length));
      XDestroyImage(frameBuffer[iDestroySlice]);
    }
    framesMade = false;
    sprintf(buffer, "Cancelled.\n"); 
    PrintMessage(buffer);
    ChangeSlice(start+islice);
    AmrPicture *apXY = pltAppPtr->GetAmrPicturePtr(XY);
    AmrPicture *apXZ = pltAppPtr->GetAmrPicturePtr(XZ);
    AmrPicture *apYZ = pltAppPtr->GetAmrPicturePtr(YZ);
    if(sliceDir == XDIR) {
      apXY->SetVLine((slice - subDomain[maxAllowableLevel].smallEnd(XDIR)) *
                                 pltAppPtr->CurrentScale());
      apXY->DoExposePicture();
      apXZ->SetVLine((slice - subDomain[maxAllowableLevel].smallEnd(XDIR)) *
                                  pltAppPtr->CurrentScale());
      apXZ->DoExposePicture();
    }
    if(sliceDir == YDIR) {
      apXY->SetHLine(apXY->ImageSizeV()-1 - (slice -
				  subDomain[maxAllowableLevel].smallEnd(YDIR)) *
                       		  pltAppPtr->CurrentScale());
      apXY->DoExposePicture();
      apYZ->SetVLine((slice - subDomain[maxAllowableLevel].smallEnd(YDIR)) *
                                  pltAppPtr->CurrentScale());
      apYZ->DoExposePicture();
    }
    if(sliceDir == ZDIR) {
      apYZ->SetHLine(apYZ->ImageSizeV()-1 - (slice -
				  subDomain[maxAllowableLevel].smallEnd(ZDIR)) *
				  pltAppPtr->CurrentScale());
      apYZ->DoExposePicture();
      apXZ->SetHLine(apXZ->ImageSizeV()-1 - (slice -
				  subDomain[maxAllowableLevel].smallEnd(ZDIR)) *
                                  pltAppPtr->CurrentScale());
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
  int hpoint = hdspoint * pltAppPtr->CurrentScale();
  int vpoint = imageSizeV-1 - vdspoint * pltAppPtr->CurrentScale();
  int side = dsBoxSize * pltAppPtr->CurrentScale();
  XDrawRectangle(GAptr->PDisplay(), pictureWindow, GetPltAppPtr()->GetRbgc(),
            hpoint, vpoint, side, side);
}


// ---------------------------------------------------------------------
void AmrPicture::UnDrawDatasetPoint() {
  int hpoint = hdspoint * pltAppPtr->CurrentScale();
  int vpoint = imageSizeV-1 - vdspoint * pltAppPtr->CurrentScale();
  int side = dsBoxSize * pltAppPtr->CurrentScale();
  XDrawRectangle(GAptr->PDisplay(), pictureWindow, GetPltAppPtr()->GetRbgc(),
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
  BL_ASSERT( ! contours);
  int iRelSlice(slice - subDomain[maxAllowableLevel].smallEnd(sliceDir));
  ShowFrameImage(iRelSlice);
  XSync(GAptr->PDisplay(), false);
  pendingTimeOut = XtAppAddTimeOut(pltAppPtr->GetAppContext(),
                                   frameSpeed, &AmrPicture::CBFrameTimeOut,
                                   (XtPointer) this);
}  // end DoFrameUpdate()


// ---------------------------------------------------------------------
void AmrPicture::DoContourSweep() {
  if(sweepDirection == ANIMPOSDIR) {
    pltAppPtr->DoForwardStep(myView);
  } else {
    pltAppPtr->DoBackStep(myView);
  } 
  pendingTimeOut = XtAppAddTimeOut(pltAppPtr->GetAppContext(),
                                   frameSpeed, &AmrPicture::CBContourSweep,
                                   (XtPointer) this);
}


// ---------------------------------------------------------------------
void AmrPicture::DoStop() {
  if(pendingTimeOut != 0) {
    XtRemoveTimeOut(pendingTimeOut);
    pendingTimeOut = 0;
    ChangeSlice(slice);
  }
}


// ---------------------------------------------------------------------
void AmrPicture::Sweep(AnimDirection direction) {
  if(pendingTimeOut != 0) {
    DoStop();
  }
  if(contours) {
    pendingTimeOut = XtAppAddTimeOut(pltAppPtr->GetAppContext(),
                                     frameSpeed, &AmrPicture::CBContourSweep,
                                     (XtPointer) this);
    sweepDirection = direction;
    return; 
  }
  if( ! framesMade) {
    CreateFrames(direction);
  }
  if(framesMade) {
    pendingTimeOut = XtAppAddTimeOut(pltAppPtr->GetAppContext(),
  				frameSpeed, &AmrPicture::CBFrameTimeOut,
				(XtPointer) this);
    sweepDirection = direction;
  }
}


// ---------------------------------------------------------------------
void AmrPicture::DrawSlice(int iSlice) {
  XDrawLine(GAptr->PDisplay(), pictureWindow, GetPltAppPtr()->GetRbgc(),
            0, 30, imageSizeH, 30);
}


// ---------------------------------------------------------------------
void AmrPicture::ShowFrameImage(int iSlice) {
  AmrPicture *apXY = pltAppPtr->GetAmrPicturePtr(XY);
  AmrPicture *apXZ = pltAppPtr->GetAmrPicturePtr(XZ);
  AmrPicture *apYZ = pltAppPtr->GetAmrPicturePtr(YZ);
  int iRelSlice(iSlice);

  XPutImage(GAptr->PDisplay(), pictureWindow, GAptr->PGC(),
            frameBuffer[iRelSlice], 0, 0, 0, 0, imageSizeH, imageSizeV);

  DrawBoxes(frameGrids[iRelSlice], pictureWindow);

  if(contours) {
    DrawContour(sliceFab, GAptr->PDisplay(), pictureWindow, GAptr->PGC());
  }


  // draw plane "cutting" lines
  XSetForeground(GAptr->PDisplay(), GAptr->PGC(), hColor);
  XDrawLine(GAptr->PDisplay(), pictureWindow,
		GAptr->PGC(), 0, hLine, imageSizeH, hLine); 
  XSetForeground(GAptr->PDisplay(), GAptr->PGC(), vColor);
  XDrawLine(GAptr->PDisplay(), pictureWindow,
		GAptr->PGC(), vLine, 0, vLine, imageSizeV); 
  XSetForeground(GAptr->PDisplay(), GAptr->PGC(), hColor-30);
  XDrawLine(GAptr->PDisplay(), pictureWindow,
  	GAptr->PGC(), 0, hLine+1, imageSizeH, hLine+1); 
  XSetForeground(GAptr->PDisplay(), GAptr->PGC(), vColor-30);
  XDrawLine(GAptr->PDisplay(), pictureWindow,
  	GAptr->PGC(), vLine+1, 0, vLine+1, imageSizeV); 
  
  if(sliceDir == XDIR) {
    apXY->SetVLine(iRelSlice * pltAppPtr->CurrentScale());
    apXY->DoExposePicture();
    apXZ->SetVLine(iRelSlice * pltAppPtr->CurrentScale());
    apXZ->DoExposePicture();
  }
  if(sliceDir == YDIR) {
    apXY->SetHLine(apXY->ImageSizeV() - 1 - iRelSlice * pltAppPtr->CurrentScale());
    apXY->DoExposePicture();
    apYZ->SetVLine(iRelSlice * pltAppPtr->CurrentScale());
    apYZ->DoExposePicture();
  }
  if(sliceDir == ZDIR) {
    apYZ->SetHLine(apYZ->ImageSizeV() - 1 - iRelSlice * pltAppPtr->CurrentScale());
    apYZ->DoExposePicture();
    apXZ->SetHLine(apXZ->ImageSizeV() - 1 - iRelSlice * pltAppPtr->CurrentScale());
    apXZ->DoExposePicture();
  }

#if (BL_SPACEDIM == 3)
  if(framesMade) {
      for(int nP = 0; nP < 3; nP++)
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
Real AmrPicture::GetWhichMin() {
  if(whichRange == USEGLOBAL) {
    return dataMinAllGrids;
  } else if(whichRange == USELOCAL) {
    return dataMinRegion;
  } else if(whichRange == USEFILE) {
    return dataMinFile;
  } else {
    return dataMinSpecified;
  }
}


// ---------------------------------------------------------------------
Real AmrPicture::GetWhichMax() {
  if(whichRange == USEGLOBAL) {
    return dataMaxAllGrids;
  } else if(whichRange == USELOCAL) {
    return dataMaxRegion;
  } else if(whichRange == USEFILE) {
    return dataMaxFile;
  } else {
    return dataMaxSpecified;
  }
}


// ---------------------------------------------------------------------
void AmrPicture::SetWhichRange(Range newRange) {
  whichRange = newRange;
  if(framesMade) {
    for(int i = 0; i < subDomain[maxAllowableLevel].length(sliceDir); ++i) {
      XDestroyImage(frameBuffer[i]);
    }
    framesMade = false;
  }
  DoStop();
}


// ---------------------------------------------------------------------
void AmrPicture::SetCartGridSmoothing(bool tf) {
  bCartGridSmoothing = tf;
}


// ---------------------------------------------------------------------
void AmrPicture::SetContourNumber(int newContours) {
  numberOfContours = newContours;
}


// ---------------------------------------------------------------------
void AmrPicture::DrawContour(Array <FArrayBox *> passedSliceFab,
                             Display *passed_display, 
                             Drawable &passedPixMap, 
                             const GC &passedGC)
{
  Real v_min(GetWhichMin());
  Real v_max(GetWhichMax());
  Real v_off;
  if(numberOfContours != 0.0) {
    v_off = v_min + 0.5 * (v_max - v_min) / numberOfContours;
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
  amrData.LoNodeLoc(maxDrawnLevel, passedSliceFab[maxDrawnLevel]->smallEnd(), 
                    pos_low);
  amrData.HiNodeLoc(maxDrawnLevel, passedSliceFab[maxDrawnLevel]->bigEnd(), 
                    pos_high);
  
  Real xlft(pos_low[hDir]);
  Real ybot(pos_low[vDir]);
  Real xrgt(pos_high[hDir]);
  Real ytop(pos_high[vDir]);

  for(int lev = minDrawnLevel; lev <= maxDrawnLevel; ++lev) {
    const bool *mask_array(NULL);
    bool mask_bool(false);
    BaseFab<bool> mask(passedSliceFab[lev]->box());
    mask.setVal(false);
    
    if(lev != maxDrawnLevel) {
      int lratio(CRRBetweenLevels(lev, lev+1, amrData.RefRatio()));
      // construct mask array.  must be size FAB.
      const BoxArray &nextFinest = amrData.boxArray(lev+1);
      for(int j = 0; j < nextFinest.length(); ++j) {
        Box coarseBox(coarsen(nextFinest[j],lratio));
        if(coarseBox.intersects(passedSliceFab[lev]->box())) {
          coarseBox &= passedSliceFab[lev]->box();
          mask.setVal(true,coarseBox,0);
        }
      }
    }
    
    // get rid of the complement ofht e
    const BoxArray &levelBoxArray = amrData.boxArray(lev);
    BoxArray complement = complementIn(passedSliceFab[lev]->box(), 
                                       levelBoxArray);
    for(int i = 0; i < complement.length(); ++i) {
      mask.setVal(true, complement[i], 0);
    }
    
    mask_array = mask.dataPtr();
    mask_bool = true;
    
    int paletteEnd(palPtr->PaletteEnd());
    int paletteStart(palPtr->PaletteStart());
    int csm1(palPtr->ColorSlots() - 1);
    Real oneOverGDiff;
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
    for(int icont = 0; icont < (int) numberOfContours; ++icont) {
      Real frac((Real) icont / numberOfContours);
      Real value(v_off + frac*(v_max - v_min));
      if(colContour) {
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
  for(int j = 0; j < yLength - 1; ++j) {
    for(int i = 0; i < xLength - 1; ++i) {
      if(has_mask) {
        if(mask[(i) + (j) * xLength] ||
           mask[(i+1) + (j) * xLength] ||
           mask[(i+1) + (j+1) * xLength] ||
           mask[(i) + (j+1) * xLength])
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
      
      XSetForeground(GAptr->PDisplay(), GAptr->PGC(), FGColor);
      
      Real hReal2X = (Real) imageSizeH / (rightEdge - leftEdge);
      Real vReal2X = (Real) imageSizeV / (topEdge - bottomEdge);
      
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
VectorDerived AmrPicture::FindVectorDerived(Array<aString> &aVectorDeriveNames) {
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
  int DVFscale(pltAppPtr->CurrentScale());
  int DVFRatio(CRRBetweenLevels(maxDrawnLevel, 
                                maxAllowableLevel, amrData.RefRatio()));
  // get velocity field
  Box DVFSliceBox(sliceFab[maxDrawnLevel]->box());
  int maxLength(DVFSliceBox.longside());
  FArrayBox density(DVFSliceBox);
  FArrayBox hVelocity(DVFSliceBox);
  FArrayBox vVelocity(DVFSliceBox);
  VectorDerived whichVectorDerived;
  Array<aString> choice(BL_SPACEDIM);

  whichVectorDerived = FindVectorDerived(choice);

  if(whichVectorDerived == enNoneFound) {
    cerr << "Could not find suitable quantities to create velocity vectors."
	 << endl;
    return;
  }

  if(whichVectorDerived == enVelocity) {
    // fill the velocities
    aString hVel(choice[hDir]);
    aString vVel(choice[vDir]);
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
    aString sDensity("density");
    if( ! dataServicesPtr->CanDerive(sDensity)) {
      cerr << "Found momentums in the plot file but not density." << endl;
      return;
    }

    aString hMom(choice[hDir]);
    aString vMom(choice[vDir]);

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

  for(int k = 0; k < npts; ++k) {
    Real s = sqrt(hdat[k] * hdat[k] + vdat[k] * vdat[k]);
    smax = Max(smax,s);
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
                                   int dvfFactor){
  int maxpoints(numberOfContours);  // partition longest side into 20 parts
  BL_ASSERT(maxpoints > 0);
  Real sight_h(maxLength / maxpoints);
  int stride((int) sight_h);
  if(stride < 1) {
    stride = 1;
  }
  Real Amax(1.25 * sight_h);
  Real eps(1.0e-3);
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
  for(int jj = jlo; jj <= jhi; jj += stride) {
    Real y(ybot + (jj-jlo));
    for(int ii = ilo; ii <= ihi; ii += stride) {
      Real x(xlft + (ii-ilo));
      Real x1(hdat[ii-ilo + nx*(jj-jlo)]);
      Real y1(vdat[ii-ilo + nx*(jj-jlo)]);
      Real s(sqrt(x1*x1 + y1*y1));
      if(s < eps) {
        continue;
      }
      Real a(Amax * (x1 / velocityMax));
      Real b(Amax * (y1 / velocityMax));
      Real x2(x + a);
      Real y2(y + b); 
      XDrawLine(pDisplay, pDrawable, pGC, 
                (int) ((x - leftEdge) * dvfFactor),
                (int) (imageSizeV - ((y - bottomEdge) * dvfFactor)),
                (int) ((x2 - leftEdge) * dvfFactor),
                (int) (imageSizeV - ((y2 - bottomEdge) * dvfFactor)));
      Real p1(x2 - arrowLength * a);
      Real p2(y2 - arrowLength * b);
      p1 = p1 - (arrowLength / 2.0) * b;
      p2 = p2 + (arrowLength / 2.0) * a;
      XDrawLine(pDisplay, pDrawable, pGC, 
                (int) ((x2 - leftEdge) * dvfFactor),
                (int) (imageSizeV - ((y2 - bottomEdge) * dvfFactor)),
                (int) ((p1 - leftEdge) * dvfFactor),
                (int) (imageSizeV - ((p2 - bottomEdge) * dvfFactor)));
    }
  }
}
// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
