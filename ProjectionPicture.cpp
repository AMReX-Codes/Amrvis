// -------------------------------------------------------------------
// ProjectionPicture.C
// -------------------------------------------------------------------
#include "ProjectionPicture.H"
#include "PltApp.H"
#include "DataServices.H"
#include "Volume.H"
#include "List.H"
#include <math.h>
#include <time.h>

extern Real RadToDeg(Real angle);
extern Real DegToRad(Real angle);
Real radToDeg = 180.0 / M_PI;

#define VOLUMEBOXES 0
//#define VOLUMEBOXES 1

// -------------------------------------------------------------------
ProjectionPicture::ProjectionPicture(PltApp *pltappptr, ViewTransform *vtptr,
				Widget da, int w, int h)
{
  int lev;
  pltAppPtr = pltappptr;
  amrPicturePtr = pltAppPtr->GetAmrPicturePtr(XY); 
  minDrawnLevel = pltAppPtr->MinDrawnLevel();
  maxDrawnLevel = pltAppPtr->MaxDrawnLevel();
  drawingArea = da; 
  viewTransformPtr = vtptr;
  daWidth = w;
  daHeight = h;

  volumeBoxColor = (unsigned char) GetBoxColor();
  volumeBoxColor = Max((unsigned char) 0, Min((unsigned char) 255, volumeBoxColor));

  showSubCut = false;
  pixCreated = false;
  imageData = NULL;
  maxDataLevel = amrPicturePtr->MaxAllowableLevel();
  theDomain = amrPicturePtr->GetSubDomain();

  //volRender = new VolRender(theDomain, minDrawnLevel, maxDataLevel);
  volRender = new VolRender(theDomain, minDrawnLevel, maxDrawnLevel);

  SetDrawingAreaDimensions(daWidth, daHeight);

  const AmrData &amrData = pltAppPtr->GetDataServicesPtr()->AmrDataRef();

  boxRealPoints.resize(maxDataLevel + 1);
  boxTransPoints.resize(maxDataLevel + 1);
  for(lev = minDrawnLevel; lev <= maxDataLevel; ++lev) {
    int nBoxes(amrData.NIntersectingGrids(lev, theDomain[lev]));
    if(lev == minDrawnLevel) {
      iSubCutBoxIndex = nBoxes;
      iBoundingBoxIndex = iSubCutBoxIndex + 1;
      nBoxes += 2;  // for the bounding box and subcut box
    }
    boxRealPoints[lev].resize(nBoxes);
    boxTransPoints[lev].resize(nBoxes);
    for(int iBox = 0; iBox < nBoxes; ++iBox) {
      boxRealPoints[lev][iBox].resize(NVERTICIES);
      boxTransPoints[lev][iBox].resize(NVERTICIES);
    }
  }
  subCutColor = 160;
  boxColors.resize(maxDataLevel + 1);
  for(lev = minDrawnLevel; lev <= maxDataLevel; ++lev) {
    int iBoxIndex(0);
    if(lev == minDrawnLevel) {
      boxColors[lev] = pltAppPtr->GetPalettePtr()->WhiteIndex();
    } else {
      boxColors[lev] = 255 - 80 * (lev - 1);
    }
    boxColors[lev] = Max(0, Min(255, boxColors[lev]));
    for(int iBox = 0; iBox < amrData.boxArray(lev).length(); ++iBox) {
      Box temp(amrData.boxArray(lev)[iBox]);
      if(temp.intersects(theDomain[lev])) {
        temp &= theDomain[lev];
        AddBox(temp.refine(CRRBetweenLevels(lev, maxDataLevel,
		amrData.RefRatio())), iBoxIndex, lev);
	++iBoxIndex;
      }
    }
  }
  Box alignedBox(theDomain[minDrawnLevel]);
  alignedBox.refine(CRRBetweenLevels(minDrawnLevel, maxDataLevel,
		     amrData.RefRatio()));
  AddBox(alignedBox, iBoundingBoxIndex, minDrawnLevel);  // the bounding box

  /*
  alignedBox.setSmall(XDIR, 0);
  alignedBox.setSmall(YDIR, 0);
  alignedBox.setSmall(ZDIR, 0);
  alignedBox.setBig(XDIR, 0);
  alignedBox.setBig(YDIR, 0);
  alignedBox.setBig(ZDIR, 0);
  */

  AddBox(alignedBox, iSubCutBoxIndex, minDrawnLevel);	// the sub cut box

  xcenter = (theDomain[maxDataLevel].bigEnd(XDIR) -
		theDomain[maxDataLevel].smallEnd(XDIR)) / 2 +
		theDomain[maxDataLevel].smallEnd(XDIR);
  ycenter = (theDomain[maxDataLevel].bigEnd(YDIR) -
		theDomain[maxDataLevel].smallEnd(YDIR)) / 2 +
		theDomain[maxDataLevel].smallEnd(YDIR);
  zcenter = (theDomain[maxDataLevel].bigEnd(ZDIR) -
		theDomain[maxDataLevel].smallEnd(ZDIR)) / 2 +
		theDomain[maxDataLevel].smallEnd(ZDIR);	// center of 3D object

  viewTransformPtr->SetObjCenter(xcenter, ycenter, zcenter);
  viewTransformPtr->SetScreenPosition(daWidth/2, daHeight/2);
  ReadTransferFile("vpramps.dat");
}  // end ProjectionPicture()


// -------------------------------------------------------------------
ProjectionPicture::~ProjectionPicture() {
  delete [] imageData;
  if(pixCreated) {
    XFreePixmap(XtDisplay(drawingArea), pixMap);
  }
  delete volRender;
}


// -------------------------------------------------------------------
void ProjectionPicture::AddBox(const Box &theBox, int index, int level) {
  boxRealPoints[level][index][0].x = theBox.smallEnd(XDIR);
  boxRealPoints[level][index][0].y = theBox.smallEnd(YDIR);
  boxRealPoints[level][index][0].z = theBox.smallEnd(ZDIR);
  boxRealPoints[level][index][0].lineto1 = 7;
  boxRealPoints[level][index][0].lineto2 = 1;

  boxRealPoints[level][index][1].x = theBox.bigEnd(XDIR)+1;
  boxRealPoints[level][index][1].y = theBox.smallEnd(YDIR);
  boxRealPoints[level][index][1].z = theBox.smallEnd(ZDIR);
  boxRealPoints[level][index][1].lineto1 = 6;
  boxRealPoints[level][index][1].lineto2 = 2;

  boxRealPoints[level][index][2].x = theBox.bigEnd(XDIR)+1;
  boxRealPoints[level][index][2].y = theBox.bigEnd(YDIR)+1;
  boxRealPoints[level][index][2].z = theBox.smallEnd(ZDIR);
  boxRealPoints[level][index][2].lineto1 = 5;
  boxRealPoints[level][index][2].lineto2 = 3;

  boxRealPoints[level][index][3].x = theBox.smallEnd(XDIR);
  boxRealPoints[level][index][3].y = theBox.bigEnd(YDIR)+1;
  boxRealPoints[level][index][3].z = theBox.smallEnd(ZDIR);
  boxRealPoints[level][index][3].lineto1 = 0;
  boxRealPoints[level][index][3].lineto2 = 4;

  boxRealPoints[level][index][4].x = theBox.smallEnd(XDIR);
  boxRealPoints[level][index][4].y = theBox.bigEnd(YDIR)+1;
  boxRealPoints[level][index][4].z = theBox.bigEnd(ZDIR)+1;
  boxRealPoints[level][index][4].lineto1 = 5;
  boxRealPoints[level][index][4].lineto2 = 3;

  boxRealPoints[level][index][5].x = theBox.bigEnd(XDIR)+1;
  boxRealPoints[level][index][5].y = theBox.bigEnd(YDIR)+1;
  boxRealPoints[level][index][5].z = theBox.bigEnd(ZDIR)+1;
  boxRealPoints[level][index][5].lineto1 = 6;
  boxRealPoints[level][index][5].lineto2 = 2;

  boxRealPoints[level][index][6].x = theBox.bigEnd(XDIR)+1;
  boxRealPoints[level][index][6].y = theBox.smallEnd(YDIR);
  boxRealPoints[level][index][6].z = theBox.bigEnd(ZDIR)+1;
  boxRealPoints[level][index][6].lineto1 = 7;
  boxRealPoints[level][index][6].lineto2 = 1;

  boxRealPoints[level][index][7].x = theBox.smallEnd(XDIR);
  boxRealPoints[level][index][7].y = theBox.smallEnd(YDIR);
  boxRealPoints[level][index][7].z = theBox.bigEnd(ZDIR)+1;
  boxRealPoints[level][index][7].lineto1 = 4;
  boxRealPoints[level][index][7].lineto2 = 0;
}


// -------------------------------------------------------------------
void ProjectionPicture::TransformBoxPoints(int iLevel, int iBoxIndex) {
  Real px, py, pz;

  for(int i = 0; i < NVERTICIES; ++i) {
    viewTransformPtr->
	   TransformPoint(boxRealPoints[iLevel][iBoxIndex][i].x,
			  boxRealPoints[iLevel][iBoxIndex][i].y,
			  boxRealPoints[iLevel][iBoxIndex][i].z,
			  px, py, pz);
    boxTransPoints[iLevel][iBoxIndex][i].x = (int) (px+.5);
    boxTransPoints[iLevel][iBoxIndex][i].y = daHeight - (int)(py+.5);
    boxTransPoints[iLevel][iBoxIndex][i].lineto1 =
		          boxRealPoints[iLevel][iBoxIndex][i].lineto1;
    boxTransPoints[iLevel][iBoxIndex][i].lineto2 =
			  boxRealPoints[iLevel][iBoxIndex][i].lineto2;
  }
}




// -------------------------------------------------------------------
void ProjectionPicture::MakeSubCutBox() {
  Real px, py, pz;

  minDrawnLevel = pltAppPtr->MinDrawnLevel();
  maxDrawnLevel = pltAppPtr->MaxDrawnLevel();

  if(showSubCut) {
    for(int i = 0; i < NVERTICIES; ++i) {
      viewTransformPtr->
	   TransformPoint(boxRealPoints[minDrawnLevel][iSubCutBoxIndex][i].x,
			  boxRealPoints[minDrawnLevel][iSubCutBoxIndex][i].y,
			  boxRealPoints[minDrawnLevel][iSubCutBoxIndex][i].z,
			  px, py, pz);
      boxTransPoints[minDrawnLevel][iSubCutBoxIndex][i].x = (int) (px+.5);
      boxTransPoints[minDrawnLevel][iSubCutBoxIndex][i].y = daHeight - (int)(py+.5);
      boxTransPoints[minDrawnLevel][iSubCutBoxIndex][i].lineto1 =
		          boxRealPoints[minDrawnLevel][iSubCutBoxIndex][i].lineto1;
      boxTransPoints[minDrawnLevel][iSubCutBoxIndex][i].lineto2 =
			  boxRealPoints[minDrawnLevel][iSubCutBoxIndex][i].lineto2;
    }
  }
}


// -------------------------------------------------------------------
void ProjectionPicture::MakeBoxes() {
  Real px, py, pz;

  minDrawnLevel = pltAppPtr->MinDrawnLevel();
  maxDrawnLevel = pltAppPtr->MaxDrawnLevel();

  if(amrPicturePtr->ShowingBoxes()) {
    for(int iLevel = minDrawnLevel; iLevel <= maxDrawnLevel; ++iLevel) {
      int nBoxes(boxTransPoints[iLevel].length());
      if(iLevel == minDrawnLevel) {
        nBoxes -= 2;  // for sub cut and bounding box
      }
      for(int iBox = 0; iBox < nBoxes; ++iBox) {
	/*
        for(int i = 0; i < NVERTICIES; ++i) {
          viewTransformPtr->TransformPoint(boxRealPoints[iLevel][iBox][i].x,
			                   boxRealPoints[iLevel][iBox][i].y,
			                   boxRealPoints[iLevel][iBox][i].z,
			                   px, py, pz);
          boxTransPoints[iLevel][iBox][i].x = (int) (px+.5);
          boxTransPoints[iLevel][iBox][i].y = daHeight - (int) (py+.5);
          boxTransPoints[iLevel][iBox][i].lineto1 =
				       boxRealPoints[iLevel][iBox][i].lineto1;
          boxTransPoints[iLevel][iBox][i].lineto2 =
				       boxRealPoints[iLevel][iBox][i].lineto2;
        }
	*/
        TransformBoxPoints(iLevel, iBox);
      }
    }
  }

  MakeSubCutBox();

  // bounding box
  for(int i = 0; i < NVERTICIES; ++i) {
    viewTransformPtr->
	 TransformPoint(boxRealPoints[minDrawnLevel][iBoundingBoxIndex][i].x,
			boxRealPoints[minDrawnLevel][iBoundingBoxIndex][i].y,
			boxRealPoints[minDrawnLevel][iBoundingBoxIndex][i].z,
			px, py, pz);
    boxTransPoints[minDrawnLevel][iBoundingBoxIndex][i].x = (int) (px+.5);
    boxTransPoints[minDrawnLevel][iBoundingBoxIndex][i].y = daHeight - (int)(py+.5);
    boxTransPoints[minDrawnLevel][iBoundingBoxIndex][i].lineto1 =
		        boxRealPoints[minDrawnLevel][iBoundingBoxIndex][i].lineto1;
    boxTransPoints[minDrawnLevel][iBoundingBoxIndex][i].lineto2 =
			boxRealPoints[minDrawnLevel][iBoundingBoxIndex][i].lineto2;
  }
}


// -------------------------------------------------------------------
void ProjectionPicture::MakePicture() {
  vpContext *vpc = volRender->GetVPContext();
  vpResult        vpret;
  clock_t time0 = clock();

  viewTransformPtr->GetScale(scale[XDIR], scale[YDIR], scale[ZDIR]);

  vpCurrentMatrix(vpc, VP_MODEL);
  vpIdentityMatrix(vpc);
  vpRotate(vpc, VP_X_AXIS, radToDeg * (viewTransformPtr->GetRho()));
  vpRotate(vpc, VP_Y_AXIS, radToDeg * (viewTransformPtr->GetTheta()));
  vpRotate(vpc, VP_Z_AXIS, radToDeg * (viewTransformPtr->GetPhi()));

  vpCurrentMatrix(vpc, VP_PROJECT);
  vpIdentityMatrix(vpc);

  lenRatio = longestWindowLength/(scale[longestBoxSideDir]*longestBoxSide);
  vpLen = 0.5*lenRatio;
  if(daWidth < daHeight) {    // undoes volpacks aspect ratio scaling
    vpWindow(vpc, VP_PARALLEL, -vpLen*aspect, vpLen*aspect,
			       -vpLen, vpLen,
			       -vpLen, vpLen);
  } else {
    vpWindow(vpc, VP_PARALLEL, -vpLen, vpLen,
			       -vpLen*aspect, vpLen*aspect,
			       -vpLen, vpLen);
  }

# if LIGHTING
  vpret = vpShadeTable(vpc);
  CheckVP(vpret, 12);
  vpret = vpRenderClassifiedVolume(vpc);   // --- render
  CheckVP(vpret, 13);
#else
  vpret = vpRenderRawVolume(vpc);    // --- render
  CheckVP(vpret, 11);
#endif

  // map imageData colors to colormap range
  Palette *palPtr = pltAppPtr->GetPalettePtr();
  if(palPtr->ColorSlots() != palPtr->PaletteSize()) {
    const unsigned char *remapTable = palPtr->RemapTable();
    int idat;
    for(idat=0; idat<daWidth*daHeight; idat++) {
      imageData[idat] = remapTable[(unsigned char)imageData[idat]];
    }
  }

  XPutImage(XtDisplay(drawingArea), pixMap, XtScreen(drawingArea)->
        default_gc, image, 0, 0, 0, 0, daWidth, daHeight);

  cout << "----- make picture time = " << ((clock()-time0)/1000000.0) << endl;


/*
// write out the volumetric image as a 2d boxchar file
ofstream imageout("imageData.boxchar");
// fake a 2d Box
imageout << "((0,0)(" << daWidth-1 << "," << daHeight-1 << ") (0,0)) 1" << endl;
imageout.write((unsigned char *)imageData, daWidth*daHeight*sizeof(char));
imageout.close();
*/


}  // end MakePicture()


/*
// -------------------------------------------------------------------
void ProjectionPicture::DrawBox(const BoxTransPoint &btp[NVERTICIES]) {
  for(i = 0; i < NVERTICIES; ++i) {
     XDrawLine(XtDisplay(drawingArea), XtWindow(drawingArea),
                  XtScreen(drawingArea)->default_gc,
		  btp[minDrawnLevel][iSubCutBoxIndex][i].x,
		  btp[minDrawnLevel][iSubCutBoxIndex][i].y,
                  btp[btp[i].lineto1].x,
                  btp[btp[i].lineto1].y);

     XDrawLine(XtDisplay(drawingArea), XtWindow(drawingArea),
                  XtScreen(drawingArea)->default_gc, boxTransPoints[i].x,
                  btp[i].y,
                  btp[btp[i].lineto2].x,
                  btp[btp[i].lineto2].y);
  }
}
*/

 
// -------------------------------------------------------------------
void ProjectionPicture::DrawBoxes(int iFromLevel, int iToLevel) {
  DrawBoxesIntoDrawable(XtWindow(drawingArea), iFromLevel, iToLevel);
}

 
// -------------------------------------------------------------------
void ProjectionPicture::DrawBoxesIntoDrawable(const Drawable &drawable,
					      int iFromLevel, int iToLevel)
{
  minDrawnLevel = pltAppPtr->MinDrawnLevel();
  maxDrawnLevel = pltAppPtr->MaxDrawnLevel();

    if(amrPicturePtr->ShowingBoxes()) {
      for(int iLevel = iFromLevel; iLevel <= iToLevel; ++iLevel) {
        XSetForeground(XtDisplay(drawingArea), XtScreen(drawingArea)->default_gc,
                       boxColors[iLevel]);
        int nBoxes(boxTransPoints[iLevel].length());
        if(iLevel == minDrawnLevel) {
	  nBoxes -= 2;  // for subcut and bounding box
        }
	for(int iBox = 0; iBox < nBoxes; ++iBox) {
          for(int i = 0; i < NVERTICIES; ++i) {
            XDrawLine(XtDisplay(drawingArea), drawable,
                      XtScreen(drawingArea)->default_gc,
		      boxTransPoints[iLevel][iBox][i].x,
		      boxTransPoints[iLevel][iBox][i].y,
                      boxTransPoints[iLevel][iBox]
		         [boxTransPoints[iLevel][iBox][i].lineto1].x,
                      boxTransPoints[iLevel][iBox]
		         [boxTransPoints[iLevel][iBox][i].lineto1].y);

            XDrawLine(XtDisplay(drawingArea), drawable,
                      XtScreen(drawingArea)->default_gc,
		      boxTransPoints[iLevel][iBox][i].x,
		      boxTransPoints[iLevel][iBox][i].y,
                      boxTransPoints[iLevel][iBox]
		         [boxTransPoints[iLevel][iBox][i].lineto2].x,
                      boxTransPoints[iLevel][iBox]
		         [boxTransPoints[iLevel][iBox][i].lineto2].y);
          }
	}
      }
    }

    if(showSubCut) {
      XSetForeground(XtDisplay(drawingArea), XtScreen(drawingArea)->default_gc,
                     subCutColor);
      //DrawBox(boxTransPoints[minDrawnLevel][iSubCutBoxIndex]);
      //
      for(int i = 0; i < NVERTICIES; ++i) {
        XDrawLine(XtDisplay(drawingArea), drawable,
                  XtScreen(drawingArea)->default_gc,
		  boxTransPoints[minDrawnLevel][iSubCutBoxIndex][i].x,
		  boxTransPoints[minDrawnLevel][iSubCutBoxIndex][i].y,
                  boxTransPoints[minDrawnLevel][iSubCutBoxIndex]
		     [boxTransPoints[minDrawnLevel][iSubCutBoxIndex][i].lineto1].x,
                  boxTransPoints[minDrawnLevel][iSubCutBoxIndex]
		     [boxTransPoints[minDrawnLevel][iSubCutBoxIndex][i].lineto1].y);

        XDrawLine(XtDisplay(drawingArea), drawable,
                  XtScreen(drawingArea)->default_gc,
		  boxTransPoints[minDrawnLevel][iSubCutBoxIndex][i].x,
		  boxTransPoints[minDrawnLevel][iSubCutBoxIndex][i].y,
                  boxTransPoints[minDrawnLevel][iSubCutBoxIndex]
		     [boxTransPoints[minDrawnLevel][iSubCutBoxIndex][i].lineto2].x,
                  boxTransPoints[minDrawnLevel][iSubCutBoxIndex]
		     [boxTransPoints[minDrawnLevel][iSubCutBoxIndex][i].lineto2].y);
      }
      //
    }

    // draw bounding box
    XSetForeground(XtDisplay(drawingArea), XtScreen(drawingArea)->default_gc,
                   boxColors[minDrawnLevel]);
    for(int i = 0; i < NVERTICIES; ++i) {
      XDrawLine(XtDisplay(drawingArea), drawable,
                XtScreen(drawingArea)->default_gc,
		boxTransPoints[minDrawnLevel][iBoundingBoxIndex][i].x,
		boxTransPoints[minDrawnLevel][iBoundingBoxIndex][i].y,
                boxTransPoints[minDrawnLevel][iBoundingBoxIndex]
		  [boxTransPoints[minDrawnLevel][iBoundingBoxIndex][i].lineto1].x,
                boxTransPoints[minDrawnLevel][iBoundingBoxIndex]
		  [boxTransPoints[minDrawnLevel][iBoundingBoxIndex][i].lineto1].y);

      XDrawLine(XtDisplay(drawingArea), drawable,
                XtScreen(drawingArea)->default_gc,
		boxTransPoints[minDrawnLevel][iBoundingBoxIndex][i].x,
		boxTransPoints[minDrawnLevel][iBoundingBoxIndex][i].y,
                boxTransPoints[minDrawnLevel][iBoundingBoxIndex]
		  [boxTransPoints[minDrawnLevel][iBoundingBoxIndex][i].lineto2].x,
                boxTransPoints[minDrawnLevel][iBoundingBoxIndex]
		  [boxTransPoints[minDrawnLevel][iBoundingBoxIndex][i].lineto2].y);
    }
}


// -------------------------------------------------------------------
void ProjectionPicture::DrawBoxesIntoPixmap(int iFromLevel, int iToLevel) {
  XSetForeground(XtDisplay(drawingArea), XtScreen(drawingArea)->default_gc, 0);
  XFillRectangle(XtDisplay(drawingArea),  pixMap,
		 XtScreen(drawingArea)->default_gc, 0, 0, daWidth, daHeight);
  DrawBoxesIntoDrawable(pixMap, iFromLevel, iToLevel);
 
  image = XGetImage(XtDisplay(drawingArea), pixMap, 0, 0,
		    daWidth, daHeight, AllPlanes, ZPixmap);
}


// -------------------------------------------------------------------
void ProjectionPicture::DrawPicture() {
  XCopyArea(XtDisplay(drawingArea), pixMap, XtWindow(drawingArea),
     XtScreen(drawingArea)->default_gc, 0, 0, daWidth, daHeight, 0, 0);
}


// -------------------------------------------------------------------
void ProjectionPicture::LabelAxes() {
  int xHere(1);	// Where to label axes with X, Y, and Z
  int yHere(3);
  int zHere(7);
  minDrawnLevel = pltAppPtr->MinDrawnLevel();
  maxDrawnLevel = pltAppPtr->MaxDrawnLevel();
  XDrawString(XtDisplay(drawingArea), XtWindow(drawingArea),
              XtScreen(drawingArea)->default_gc,
              boxTransPoints[minDrawnLevel][iBoundingBoxIndex][xHere].x,
              boxTransPoints[minDrawnLevel][iBoundingBoxIndex][xHere].y,
              "X", 1);
    XDrawString(XtDisplay(drawingArea), XtWindow(drawingArea),
              XtScreen(drawingArea)->default_gc,
              boxTransPoints[minDrawnLevel][iBoundingBoxIndex][yHere].x,
              boxTransPoints[minDrawnLevel][iBoundingBoxIndex][yHere].y,
              "Y", 1);
    XDrawString(XtDisplay(drawingArea), XtWindow(drawingArea),
              XtScreen(drawingArea)->default_gc,
              boxTransPoints[minDrawnLevel][iBoundingBoxIndex][zHere].x,
              boxTransPoints[minDrawnLevel][iBoundingBoxIndex][zHere].y,
              "Z", 1);
}


// -------------------------------------------------------------------
void ProjectionPicture::ToggleShowSubCut() {
  showSubCut = (showSubCut ? false : true);
}


// -------------------------------------------------------------------
void ProjectionPicture::SetSubCut(const Box &newbox) {
  AddBox(newbox, iSubCutBoxIndex, minDrawnLevel);
  MakeSubCutBox();
}


// -------------------------------------------------------------------
void ProjectionPicture::SetDrawingAreaDimensions(int w, int h) {

  minDrawnLevel = pltAppPtr->MinDrawnLevel();
  maxDrawnLevel = pltAppPtr->MaxDrawnLevel();

  daWidth = w;
  daHeight = h;
  if(imageData != NULL) {
    delete [] imageData;
  }
  imageData = new unsigned char[daWidth*daHeight];
  if(imageData==NULL) {
    cout << " SetDrawingAreaDimensions::imageData : new failed" << endl;
    exit(1);
  }

  viewTransformPtr->SetScreenPosition(daWidth/2, daHeight/2);

  image = XCreateImage(XtDisplay(drawingArea),
                XDefaultVisual(XtDisplay(drawingArea),
                DefaultScreen(XtDisplay(drawingArea))),
                DefaultDepthOfScreen(XtScreen(drawingArea)), ZPixmap, 0,
                (char *) imageData, daWidth, daHeight,
		XBitmapPad(XtDisplay(drawingArea)), daWidth);

  if(pixCreated) {
    XFreePixmap(XtDisplay(drawingArea), pixMap);
  }
  pixMap = XCreatePixmap(XtDisplay(drawingArea), XtWindow(drawingArea),
			daWidth, daHeight,
			DefaultDepthOfScreen(XtScreen(drawingArea)));
  pixCreated = true;

  // --- set the image buffer
  vpContext *vpc = volRender->GetVPContext();
  vpSetImage(vpc, (unsigned char *)imageData, daWidth, daHeight,
             daWidth, VP_LUMINANCE);

  longestWindowLength  = (Real) Max(daWidth, daHeight);
  shortestWindowLength = (Real) Min(daWidth, daHeight);
  aspect = shortestWindowLength/longestWindowLength;

  Box alignedBox(theDomain[minDrawnLevel]);
  const AmrData &amrData = pltAppPtr->GetDataServicesPtr()->AmrDataRef();
  alignedBox.refine(CRRBetweenLevels(minDrawnLevel, maxDataLevel,
		     amrData.RefRatio()));
  longestBoxSide = (Real) alignedBox.longside(longestBoxSideDir);
}


// -------------------------------------------------------------------
void ProjectionPicture::ReadTransferFile(const aString &rampFileName) {
  volRender->ReadTransferFile(rampFileName);
  pltAppPtr->GetPalettePtr()->SetTransfers(volRender->NDenRampPts(),
					   volRender->DensityRampX(),
					   volRender->DensityRampY());
}  // end ReadTransferFile
// -------------------------------------------------------------------
// -------------------------------------------------------------------
