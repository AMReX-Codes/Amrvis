//BL_COPYRIGHT_NOTICE

//
// $Id: AmrPicture.cpp,v 1.31 1998-10-27 18:16:34 lijewski Exp $
//

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
		       PltApp *pltappptr, DataServices *dataservicesptr)
{
  int i, ilev;
  contours = false;
  raster = true;
  colContour = false;
  vectorField = false;
  minDrawnLevel = mindrawnlevel;
  GAptr = gaptr;
  pltAppPtr = pltappptr;
  myView = XY;
  isSubDomain = false;
  findSubRange = false;

  dataServicesPtr = dataservicesptr;
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
                       PltApp *parentPltAppPtr, PltApp *pltappptr)
{
  assert(parentPicturePtr != NULL);
  assert(pltappptr != NULL);
  int ilev;

  contours = parentPicturePtr->Contours();
  raster = parentPicturePtr->Raster();
  colContour = parentPicturePtr->ColorContour();
  vectorField = parentPicturePtr->VectorField();

  myView = view;
  minDrawnLevel = mindrawnlevel;
  GAptr = gaptr;
  pltAppPtr = pltappptr;
  isSubDomain = true;
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

  numberOfLevels = maxAllowableLevel + 1;
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
  assert(amrData.RefRatio().length() > 0);

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

  if(showBoxes) {
    for(int level = minDrawnLevel; level <= maxDrawnLevel; ++level) {
      if(level == minDrawnLevel) {
        XSetForeground(GAptr->PDisplay(), GAptr->PGC(), palPtr->WhiteIndex());
      } else {
        XSetForeground(GAptr->PDisplay(), GAptr->PGC(),
		       MaxPaletteIndex()-80*(level-1));
      }
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
  // draw bounding box
  XSetForeground(GAptr->PDisplay(), GAptr->PGC(), palPtr->WhiteIndex());
  XDrawRectangle(GAptr->PDisplay(), drawable, GAptr->PGC(), 0, 0,
			imageSizeH-1, imageSizeV-1);
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
  assert(palptr != NULL);
  palPtr = palptr;
  AmrData &amrData = dataServicesPtr->AmrDataRef();
  if(currentDerived != derived || whichRange == USEFILE) {
    maxsFound = false;
    pltAppPtr->PaletteDrawn(false);
  }

  if( ! maxsFound) {
    if(framesMade) {
      for(int i=0;i<subDomain[maxAllowableLevel].length(sliceDir);++i) {
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
			         amrData.ProbDomain()[lev], currentDerived, lev,
			         &dataMin, &dataMax, &minMaxValid);
          dataMinAllGrids = Min(dataMinAllGrids, dataMin);
          dataMaxAllGrids = Max(dataMaxAllGrids, dataMax);
          dataMinFile = Min(dataMinFile, dataMin);
          dataMaxFile = Max(dataMaxFile, dataMax);
	  if(findSubRange && lev <= maxAllowableLevel && lev <= maxLevelWithGrids) {
            DataServices::Dispatch(DataServices::MinMaxRequest,
			           dataServicesPtr,
                                   subDomain[lev], currentDerived, lev,
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
		           sliceFab[iLevel], sliceFab[iLevel]->box(),
			   iLevel, derived);
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
  int whiteIndex = palptr->WhiteIndex();
  int blackIndex = palptr->BlackIndex();
  int colorSlots = palptr->ColorSlots();
  int paletteStart = palptr->PaletteStart();
  int paletteEnd = palptr->PaletteEnd();
  int csm1 = colorSlots - 1;
  
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

  for(j = 0; j < imagesizev; ++j) {
    jish = j*imagesizeh;
    jtmp =  datasizeh * (j/scale);
    for(i = 0; i < imagesizeh; ++i) {
      scaledimagedata[i+jish] = imagedata [ (i/scale) + jtmp ];
    }
  }

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
			   &imageFab, imageFab.box(), maxDrawnLevel,
                           currentDerived);

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
  assert( ! contours);
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
void AmrPicture::SetContourNumber(int newContours) {
  numberOfContours = newContours;
}


// ---------------------------------------------------------------------
void AmrPicture::DrawContour(Array <FArrayBox *> passedSliceFab,
                             Display *passed_display, 
                             Drawable &passed_PixMap, 
                             GC passed_gc)
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
      contour(*(passedSliceFab[lev]), value, mask_bool, mask_array,
              passed_display, passed_PixMap, passed_gc, drawColor,
              passedSliceFab[lev]->box().length(hDir), 
              passedSliceFab[lev]->box().length(vDir),
              xlft, ybot, xrgt, ytop);
    }
  } // loop over levels
}


// contour plotting
// ---------------------------------------------------------------------
int AmrPicture::contour(const FArrayBox &fab, Real value,
                        bool has_mask, const bool *mask,
                        Display *display, Drawable &dPixMap, GC gc, int FGColor,
                        int xLength, int yLength,
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
  bool failure_status = false;
  for(int j = 0; j < yLength - 1; ++j) {
    for(int i = 0; i < xLength - 1; ++i) {
      if(has_mask) {
        if(mask[(i)+(j)*xLength] ||
           mask[(i+1)+(j)*xLength] ||
           mask[(i+1)+(j+1)*xLength] ||
           mask[(i)+(j+1)*xLength])
	{
          continue;
        }
      }
      Real left_bottom = data[(i)+(j)*xLength];         // left bottom value
      Real left_top = data[(i)+(j+1)*xLength];          // left top value
      Real right_bottom = data[(i+1)+(j)*xLength];      // right bottom value
      Real right_top = data[(i+1)+(j+1)*xLength];       // right top value
      xLeft = leftEdge + xDiff*(i);
      xRight = xLeft + xDiff;
      yBottom = bottomEdge + yDiff*(j);
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
          failure_status = true;
        }
      }
      if(right) {
        if(right_bottom != right_top) {
          yRight = yBottom + yDiff*(value-right_bottom)/(right_top-right_bottom);
        } else {
          yRight = yBottom;
          failure_status = true;
        }
      }
      if(bottom) {
        if(left_bottom != right_bottom) {
          xBottom = xLeft + xDiff*(value-left_bottom)/(right_bottom-left_bottom);
        } else {
          xBottom = xRight;
          failure_status = true;
        }
      }
      if(top) {
        if(left_top != right_top) {
          xTop = xLeft + xDiff*(value-left_top)/(right_top-left_top);
        } else {
          xTop = xRight;
          failure_status = true;
        }
      }
      
      XSetForeground(GAptr->PDisplay(), GAptr->PGC(), FGColor);
      
      Real vReal2X, hReal2X;
      hReal2X = (Real)imageSizeH /(rightEdge - leftEdge);
      vReal2X = (Real)imageSizeV /(topEdge - bottomEdge);
      
      if(left) { 
        xLeft -= leftEdge; yLeft -= bottomEdge;
        xLeft *= hReal2X; yLeft *= vReal2X;
      }
      if(right) {
        xRight -= leftEdge; yRight -= bottomEdge;
        xRight *= hReal2X; yRight *= vReal2X;
      }
      if(top) {
        xTop -= leftEdge; yTop -= bottomEdge;
        xTop *= hReal2X; yTop *= vReal2X; 
      }
      if(bottom) {
        xBottom -= leftEdge; yBottom -= bottomEdge;
        xBottom *= hReal2X; yBottom *= vReal2X; 
      }
      
      // finally, draw contour line
      if(left && right && bottom && top) {
        // intersects all sides, generate saddle point
        XDrawLine(display, dPixMap, gc, 
                  xLeft, imageSizeV-yLeft,xRight, imageSizeV-yRight);
        XDrawLine(display, dPixMap, gc,
                  xTop, imageSizeV-yTop,xBottom,imageSizeV-yBottom);
      } else if(top && bottom) {
        // only intersects top and bottom sides
        XDrawLine(display, dPixMap, gc,
                  xTop,imageSizeV-yTop,xBottom,imageSizeV-yBottom);
      } else if(left) {
        if(right) {
          XDrawLine(display, dPixMap, gc,
                    xLeft,imageSizeV-yLeft,xRight,imageSizeV-yRight);
        } else if(top) {
          XDrawLine(display, dPixMap, gc,
                    xLeft,imageSizeV-yLeft,xTop,imageSizeV-yTop);
        } else {
          XDrawLine(display, dPixMap, gc,
                    xLeft,imageSizeV-yLeft,xBottom,imageSizeV-yBottom);
        }
      } else if(right) {
        if(top) {
          XDrawLine(display, dPixMap, gc,
                    xRight,imageSizeV-yRight,xTop,imageSizeV-yTop);
        } else {
          XDrawLine(display, dPixMap, gc,
                    xRight,imageSizeV-yRight,xBottom,imageSizeV-yBottom);
        }
      }
      
    }  // end for(i...)
  }  // end for(j...)
  return failure_status;
}


// ---------------------------------------------------------------------
void AmrPicture::DrawVectorField(Display *pDisplay, 
                                 Drawable &pDrawable, GC pGC)
{
  int hDir, vDir;
  if(myView == XY) {
    hDir = 0;
    vDir = 1;
  } else if(myView == XZ) {
    hDir = 0;
    vDir = 2;
  } else if(myView == YZ) {
    hDir = 1;
    vDir = 2;
  } else {
    assert(0);
  }
  const AmrData &amrData = dataServicesPtr->AmrDataRef();
  int DVFscale(pltAppPtr->CurrentScale());
  int DVFRatio(CRRBetweenLevels(maxDrawnLevel, 
                                  maxAllowableLevel, 
                                  amrData.RefRatio()));
  // get velocity field
  Box DVFSliceBox(sliceFab[maxDrawnLevel]->box());
  int maxLength(DVFSliceBox.longside());
  FArrayBox density(DVFSliceBox);
  FArrayBox hVelocity(DVFSliceBox);
  FArrayBox vVelocity(DVFSliceBox);

  // fill the density and momentum:
  aString Density("density");
  aString choice[3];
  choice[0] = "xmom";
  choice[1] = "ymom";
  choice[2] = "zmom";
  aString hMom = choice[hDir];
  aString vMom = choice[vDir];

  DataServices::Dispatch(DataServices::FillVarOneFab, dataServicesPtr, 
                         &density, density.box(),
                         maxDrawnLevel, Density);
  DataServices::Dispatch(DataServices::FillVarOneFab, dataServicesPtr, 
                         &hVelocity, hVelocity.box(),
                         maxDrawnLevel, hMom);
  DataServices::Dispatch(DataServices::FillVarOneFab, dataServicesPtr, 
                         &vVelocity, vVelocity.box(),
                         maxDrawnLevel, vMom);

  // then divide to get velocity
  hVelocity /= density;
  vVelocity /= density;
  
  // compute maximum speed
  Real smax(0.0);
  int npts(hVelocity.box().numPts());
  const Real *hdat = hVelocity.dataPtr();
  const Real *vdat = vVelocity.dataPtr();

  for(int k = 0; k < npts; ++k) {
    Real s = sqrt(hdat[k] * hdat[k] + vdat[k] * hdat[k]);
    smax = Max(smax,s);
  }
  
  
  if(smax < 1.0e-8) {
    cout << "CONTOUR: zero velocity field" << endl;
  } else {
    draw_vector_field(pDisplay, pDrawable, pGC, hDir, vDir, maxLength,
                      hdat, vdat, smax, DVFSliceBox, DVFscale*DVFRatio);
  }
}


// ---------------------------------------------------------------------
void AmrPicture::draw_vector_field(Display *pDisplay, Drawable &pDrawable, 
                                   GC pGC, int hDir, int vDir, int maxLength,
                                   const Real *hdat, const Real *vdat, 
                                   Real velocityMax, Box dvfSliceBox,
                                   int dvfFactor){
  int maxpoints(numberOfContours);  // partition longest side into 20 parts
  assert(maxpoints > 0);
  Real sight_h(maxLength / maxpoints);
  int stride(sight_h);
  if(stride < 1) {
    stride = 1;
  }
  Real Amax(1.25 * sight_h);
  Real eps(1.0e-3);
  Real arrowLength(0.25);
  XSetForeground(pDisplay, pGC, palPtr->WhiteIndex());
  Box theBox(dvfSliceBox);
  int ilo(theBox.smallEnd(hDir));
  int leftEdge(ilo);
  int ihi(theBox.bigEnd(hDir));
  int nx(ihi - ilo + 1);
  int jlo(theBox.smallEnd(vDir));
  int bottomEdge(jlo);
  int jhi(theBox.bigEnd(vDir));
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
      int x2(x + a);
      int y2(y + b); 
      XDrawLine(pDisplay, pDrawable, pGC, 
                (x-leftEdge)*dvfFactor, 
                imageSizeV-((y-bottomEdge)*dvfFactor), 
                (x2-leftEdge)*dvfFactor, 
                imageSizeV-((y2-bottomEdge)*dvfFactor));
      Real p1 = x2 - arrowLength * a;
      Real p2 = y2 - arrowLength * b;
      p1 = p1 - (arrowLength / 2.0) * b;
      p2 = p2 + (arrowLength / 2.0) * a;
      XDrawLine(pDisplay, pDrawable, pGC, 
                (x2-leftEdge)*dvfFactor, 
                imageSizeV-((y2-bottomEdge)*dvfFactor), 
                (p1-leftEdge)*dvfFactor, 
                imageSizeV-((p2-bottomEdge)*dvfFactor));
    }
  }
}
// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
