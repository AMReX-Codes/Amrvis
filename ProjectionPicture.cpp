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

#ifdef BL_VOLUMERENDER
  volRender = new VolRender(theDomain, minDrawnLevel, maxDrawnLevel);
#endif

  SetDrawingAreaDimensions(daWidth, daHeight);

  const AmrData &amrData = pltAppPtr->GetDataServicesPtr()->AmrDataRef();

  boxReal.resize(maxDataLevel + 1);
  boxTrans.resize(maxDataLevel + 1);
  for(lev = minDrawnLevel; lev <= maxDataLevel; ++lev) {
    int nBoxes(amrData.NIntersectingGrids(lev, theDomain[lev]));
    if(lev == minDrawnLevel) {
      iSubCutBoxIndex = nBoxes;
      iBoundingBoxIndex = iSubCutBoxIndex + 1;
      nBoxes += 2;  // for the bounding box and subcut box
    }
    boxReal[lev].resize(nBoxes);
    boxTrans[lev].resize(nBoxes);
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
#ifdef BL_VOLUMERENDER
  delete volRender;
#endif
}


// -------------------------------------------------------------------
void ProjectionPicture::AddBox(const Box &theBox, int index, int level) {
    RealBox newBox;
    Real dimensions[6] = { theBox.smallEnd(XDIR), theBox.smallEnd(YDIR),
                           theBox.smallEnd(ZDIR), theBox.bigEnd(XDIR)+1,
                           theBox.bigEnd(YDIR)+1, theBox.bigEnd(ZDIR)+1 };
    newBox.vertices[0] = RealPoint(dimensions[0], dimensions[1], dimensions[2]);
    newBox.vertices[1] = RealPoint(dimensions[3], dimensions[1], dimensions[2]);
    newBox.vertices[2] = RealPoint(dimensions[3], dimensions[4], dimensions[2]);
    newBox.vertices[3] = RealPoint(dimensions[0], dimensions[4], dimensions[2]);
    newBox.vertices[4] = RealPoint(dimensions[0], dimensions[1], dimensions[5]);
    newBox.vertices[5] = RealPoint(dimensions[3], dimensions[1], dimensions[5]);
    newBox.vertices[6] = RealPoint(dimensions[3], dimensions[4], dimensions[5]);
    newBox.vertices[7] = RealPoint(dimensions[0], dimensions[4], dimensions[5]);

    boxReal[level][index] = newBox;
}


// -------------------------------------------------------------------
void ProjectionPicture::TransformBoxPoints(int iLevel, int iBoxIndex) {
  Real px, py, pz;

  for(int i = 0; i < NVERTICIES; ++i) {
    viewTransformPtr->
	   TransformPoint(boxReal[iLevel][iBoxIndex].vertices[i].x,
			  boxReal[iLevel][iBoxIndex].vertices[i].y,
			  boxReal[iLevel][iBoxIndex].vertices[i].z,
			  px, py, pz);
    boxTrans[iLevel][iBoxIndex].vertices[i]=TransPoint((int)(px+.5), daHeight-(int)(py+.5));
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
	   TransformPoint(boxReal[minDrawnLevel][iSubCutBoxIndex].vertices[i].x,
			  boxReal[minDrawnLevel][iSubCutBoxIndex].vertices[i].y,
			  boxReal[minDrawnLevel][iSubCutBoxIndex].vertices[i].z,
			  px, py, pz);
      boxTrans[minDrawnLevel][iSubCutBoxIndex].vertices[i]=TransPoint((int)(px+.5), daHeight-(int)(py+.5));
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
      int nBoxes(boxTrans[iLevel].length());
      if(iLevel == minDrawnLevel) {
        nBoxes -= 2;  // for sub cut and bounding box
      }
      for(int iBox = 0; iBox < nBoxes; ++iBox) {
        TransformBoxPoints(iLevel, iBox);
      }
    }
  }

  MakeSubCutBox();

  // bounding box
  for(int i = 0; i < NVERTICIES; ++i) {
//      cout<<"int "<<i<<"\ttransformpoints:"<<endl;
      viewTransformPtr->
	 TransformPoint(boxReal[minDrawnLevel][iBoundingBoxIndex].vertices[i].x,
			boxReal[minDrawnLevel][iBoundingBoxIndex].vertices[i].y,
			boxReal[minDrawnLevel][iBoundingBoxIndex].vertices[i].z,
			px, py, pz);
    boxTrans[minDrawnLevel][iBoundingBoxIndex].vertices[i]=TransPoint((int)(px+.5), daHeight-(int)(py+.5));
  }
}


// -------------------------------------------------------------------
void ProjectionPicture::MakePicture() {
#ifdef BL_VOLUMERENDER
  vpContext *vpc = volRender->GetVPContext();
  vpResult        vpret;
  clock_t time0 = clock();

  viewTransformPtr->GetScale(scale[XDIR], scale[YDIR], scale[ZDIR]);


  Real mvmat[4][4];
  viewTransformPtr->GetRenderRotationMat(mvmat);
  vpCurrentMatrix(vpc, VP_MODEL);
  vpIdentityMatrix(vpc);
  //viewTransformPtr->ViewRotationMat();
  vpSetMatrix(vpc, mvmat);



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

#endif

}  // end MakePicture()


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
        int nBoxes(boxTrans[iLevel].length());
        if(iLevel == minDrawnLevel) {
	  nBoxes -= 2;  // for subcut and bounding box
        }
	for(int iBox = 0; iBox < nBoxes; ++iBox) {
            boxTrans[iLevel][iBox].Draw(XtDisplay(drawingArea), drawable,
                                        XtScreen(drawingArea)->default_gc);
          
	}
      }
    }

    if(showSubCut) {
      XSetForeground(XtDisplay(drawingArea), XtScreen(drawingArea)->default_gc,
                     subCutColor);
      //DrawBox(boxTransPoints[minDrawnLevel][iSubCutBoxIndex]);
      //
      boxTrans[minDrawnLevel][iSubCutBoxIndex].Draw(XtDisplay(drawingArea), drawable,
                                                    XtScreen(drawingArea)->default_gc);

    }
    // draw bounding box
    XSetForeground(XtDisplay(drawingArea), XtScreen(drawingArea)->default_gc,
                   boxColors[minDrawnLevel]);
    
    boxTrans[minDrawnLevel][iBoundingBoxIndex].Draw(XtDisplay(drawingArea), drawable,
                                                    XtScreen(drawingArea)->default_gc);
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
  int zHere(4);
  minDrawnLevel = pltAppPtr->MinDrawnLevel();
  maxDrawnLevel = pltAppPtr->MaxDrawnLevel();
  XDrawString(XtDisplay(drawingArea), XtWindow(drawingArea),
              XtScreen(drawingArea)->default_gc,
              boxTrans[minDrawnLevel][iBoundingBoxIndex].vertices[xHere].x,
              boxTrans[minDrawnLevel][iBoundingBoxIndex].vertices[xHere].y,
              "X", 1);
  XDrawString(XtDisplay(drawingArea), XtWindow(drawingArea),
              XtScreen(drawingArea)->default_gc,
              boxTrans[minDrawnLevel][iBoundingBoxIndex].vertices[yHere].x,
              boxTrans[minDrawnLevel][iBoundingBoxIndex].vertices[yHere].y,
              "Y", 1);
  XDrawString(XtDisplay(drawingArea), XtWindow(drawingArea),
              XtScreen(drawingArea)->default_gc,
              boxTrans[minDrawnLevel][iBoundingBoxIndex].vertices[zHere].x,
              boxTrans[minDrawnLevel][iBoundingBoxIndex].vertices[zHere].y,
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

#ifdef BL_VOLUMERENDER
  // --- set the image buffer
  vpContext *vpc = volRender->GetVPContext();
  vpSetImage(vpc, (unsigned char *)imageData, daWidth, daHeight,
             daWidth, VP_LUMINANCE);
#endif

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
#ifdef BL_VOLUMERENDER
  volRender->ReadTransferFile(rampFileName);
  pltAppPtr->GetPalettePtr()->SetTransfers(volRender->NDenRampPts(),
					   volRender->DensityRampX(),
					   volRender->DensityRampY());
#endif
}  // end ReadTransferFile
// -------------------------------------------------------------------
// -------------------------------------------------------------------


//--------------------------------------------------------------------
//----- BOX AND POINT STUFF-------------------------------------------
//--------------------------------------------------------------------

RealBox::RealBox() {
    RealPoint zero(0., 0., 0.);
    for (int i = 0; i < 8 ; i++)
        vertices[i] = zero;
}

RealBox::RealBox(RealPoint p1, RealPoint p2, RealPoint p3, RealPoint p4, 
                 RealPoint p5, RealPoint p6, RealPoint p7, RealPoint p8)
{
    vertices[0] = p1; vertices[1] = p2; vertices[2] = p3; vertices[3] = p4;
    vertices[4] = p5; vertices[5] = p6; vertices[6] = p7; vertices[7] = p8;
}

TransBox::TransBox() {
    TransPoint zero(0,0);
    for (int i = 0; i < 8 ; i++)
        vertices[i] = zero;
}

TransBox::TransBox(TransPoint p1, TransPoint p2, TransPoint p3, TransPoint p4, 
                   TransPoint p5, TransPoint p6, TransPoint p7, TransPoint p8)
{
    vertices[0] = p1; vertices[1] = p2; vertices[2] = p3; vertices[3] = p4;
    vertices[4] = p5; vertices[5] = p6; vertices[6] = p7; vertices[7] = p8;
}

void TransBox::Draw(Display *display, Window window, GC gc)
{
    for(int j = 0; j<2;j++) {
        for(int i = 0; i<3;i++){
            XDrawLine(display, window, gc,
                      vertices[j*4+i].x, vertices[j*4+i].y,
                      vertices[j*4+i+1].x,vertices[j*4+i+1].y);
        }
        XDrawLine(display, window, gc, vertices[j*4].x, vertices[j*4].y, 
                  vertices[j*4+3].x, vertices[j*4+3].y);
    }
    for(int k = 0;k<4;k++) {
        XDrawLine(display, window, gc,
                  vertices[k].x, vertices[k].y,
                  vertices[k+4].x, vertices[k+4].y);
    }
            
}

//--------------------------------------------------------------------
