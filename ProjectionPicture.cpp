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
  minDrawnLevel = pltAppPtr->MinAllowableLevel();
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

  volRender = new VolRender(theDomain, minDrawnLevel, maxDataLevel);

  SetDrawingAreaDimensions(daWidth, daHeight);

  nPoints = theDomain[maxDataLevel].numPts();

  // the following creates the BoxRealPoints.
  // bPoints = total box points = all grid boxes + bounding box + sub cut

  bPoints = 0;

  const AmrData &amrData = pltAppPtr->GetDataServicesPtr()->AmrDataRef();

  for(lev = maxDataLevel; lev >= minDrawnLevel; lev--) {
    for(int iBox = 0; iBox < amrData.boxArray(lev).length(); ++iBox) {
      if(amrData.boxArray(lev)[iBox].intersects(theDomain[lev])) {
        bPoints += 8;
      }
    }
  }
  boxrealpoints = new BoxRealPoint[bPoints+16]; 
  boxtranspoints = new BoxTransPoint[bPoints+16]; 

  bPoints = 0;

  for(lev = maxDataLevel; lev >= minDrawnLevel; lev--) {
    int boxColor = 255-35*lev;
    boxColor = Max(0, Min(255, boxColor));
    for(int iBox = 0; iBox < amrData.boxArray(lev).length(); ++iBox) {
      Box temp(amrData.boxArray(lev)[iBox]);
      if(temp.intersects(theDomain[lev])) {
        temp &= theDomain[lev];
        AddBox(temp.refine(CRRBetweenLevels(lev, maxDataLevel,
		amrData.RefRatio())), boxColor);
      }
    }
  }
  xHere = bPoints+1;		// _Here -- where to label axes with
  yHere = bPoints+3;		// X, Y, and Z
  zHere = bPoints+7;

  Box alignedBox(theDomain[minDrawnLevel]);
  alignedBox.refine(CRRBetweenLevels(minDrawnLevel, maxDataLevel,
		     amrData.RefRatio()));
  AddBox(alignedBox, 255);
  alignedBox.setSmall(XDIR, 0);
  alignedBox.setSmall(YDIR, 0);
  alignedBox.setSmall(ZDIR, 0);
  alignedBox.setBig(XDIR, 0);
  alignedBox.setBig(YDIR, 0);
  alignedBox.setBig(ZDIR, 0);
  AddBox(alignedBox, 0);	// reserved for sub cut box, changed by user

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
  delete [] boxrealpoints;
  delete [] boxtranspoints;
  delete [] imageData;
  if(pixCreated) {
    XFreePixmap(XtDisplay(drawingArea), pixMap);
  }
  delete volRender;
}


// -------------------------------------------------------------------
void ProjectionPicture::AddBox(Box theBox, int color) {
  boxrealpoints[bPoints].x = theBox.smallEnd(XDIR);
  boxrealpoints[bPoints].y = theBox.smallEnd(YDIR);
  boxrealpoints[bPoints].z = theBox.smallEnd(ZDIR);
  boxrealpoints[bPoints].lineto1 = bPoints+7;
  boxrealpoints[bPoints].lineto2 = bPoints+1;
  boxrealpoints[bPoints].color = color;

  boxrealpoints[bPoints+1].x = theBox.bigEnd(XDIR)+1;
  boxrealpoints[bPoints+1].y = theBox.smallEnd(YDIR);
  boxrealpoints[bPoints+1].z = theBox.smallEnd(ZDIR);
  boxrealpoints[bPoints+1].lineto1 = bPoints+6;
  boxrealpoints[bPoints+1].lineto2 = bPoints+2;
  boxrealpoints[bPoints+1].color = color;

  boxrealpoints[bPoints+2].x = theBox.bigEnd(XDIR)+1;
  boxrealpoints[bPoints+2].y = theBox.bigEnd(YDIR)+1;
  boxrealpoints[bPoints+2].z = theBox.smallEnd(ZDIR);
  boxrealpoints[bPoints+2].lineto1 = bPoints+5;
  boxrealpoints[bPoints+2].lineto2 = bPoints+3;
  boxrealpoints[bPoints+2].color = color;

  boxrealpoints[bPoints+3].x = theBox.smallEnd(XDIR);
  boxrealpoints[bPoints+3].y = theBox.bigEnd(YDIR)+1;
  boxrealpoints[bPoints+3].z = theBox.smallEnd(ZDIR);
  boxrealpoints[bPoints+3].lineto1 = bPoints;
  boxrealpoints[bPoints+3].lineto2 = bPoints+4;
  boxrealpoints[bPoints+3].color = color;

  boxrealpoints[bPoints+4].x = theBox.smallEnd(XDIR);
  boxrealpoints[bPoints+4].y = theBox.bigEnd(YDIR)+1;
  boxrealpoints[bPoints+4].z = theBox.bigEnd(ZDIR)+1;
  boxrealpoints[bPoints+4].lineto1 = bPoints+5;
  boxrealpoints[bPoints+4].lineto2 = bPoints+3;
  boxrealpoints[bPoints+4].color = color;

  boxrealpoints[bPoints+5].x = theBox.bigEnd(XDIR)+1;
  boxrealpoints[bPoints+5].y = theBox.bigEnd(YDIR)+1;
  boxrealpoints[bPoints+5].z = theBox.bigEnd(ZDIR)+1;
  boxrealpoints[bPoints+5].lineto1 = bPoints+6;
  boxrealpoints[bPoints+5].lineto2 = bPoints+2;
  boxrealpoints[bPoints+5].color = color;

  boxrealpoints[bPoints+6].x = theBox.bigEnd(XDIR)+1;
  boxrealpoints[bPoints+6].y = theBox.smallEnd(YDIR);
  boxrealpoints[bPoints+6].z = theBox.bigEnd(ZDIR)+1;
  boxrealpoints[bPoints+6].lineto1 = bPoints+7;
  boxrealpoints[bPoints+6].lineto2 = bPoints+1;
  boxrealpoints[bPoints+6].color = color;

  boxrealpoints[bPoints+7].x = theBox.smallEnd(XDIR);
  boxrealpoints[bPoints+7].y = theBox.smallEnd(YDIR);
  boxrealpoints[bPoints+7].z = theBox.bigEnd(ZDIR)+1;
  boxrealpoints[bPoints+7].lineto1 = bPoints+4;
  boxrealpoints[bPoints+7].lineto2 = bPoints;
  boxrealpoints[bPoints+7].color = color;
   
  bPoints += 8;
}


// -------------------------------------------------------------------
void ProjectionPicture::MakeBoxes() {
  Real px, py, pz;
  int i;
  int showBoxesFix = 1 - amrPicturePtr->ShowingBoxes();
  for(i = showBoxesFix*(bPoints-16); i < bPoints-(1-showSubCut)*8; i++) {
    viewTransformPtr->TransformPoint(boxrealpoints[i].x,
			boxrealpoints[i].y, boxrealpoints[i].z, px, py, pz);
    boxtranspoints[i].x = (int) (px+.5);
    boxtranspoints[i].y = daHeight - (int) (py+.5);
    boxtranspoints[i].color = boxrealpoints[i].color;
    boxtranspoints[i].lineto1 = boxrealpoints[i].lineto1;
    boxtranspoints[i].lineto2 = boxrealpoints[i].lineto2;
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

 
// -------------------------------------------------------------------
void ProjectionPicture::DrawBoxes() {
    int showBoxesFix(1 - amrPicturePtr->ShowingBoxes());
    for(int i = showBoxesFix*(bPoints-16); i < bPoints-(1-showSubCut)*8; ++i) {
      XSetForeground(XtDisplay(drawingArea), 
                     XtScreen(drawingArea)->default_gc,
                     boxtranspoints[i].color);
      XDrawLine(XtDisplay(drawingArea), XtWindow(drawingArea),
                XtScreen(drawingArea)->default_gc, boxtranspoints[i].x,
                boxtranspoints[i].y,
                boxtranspoints[boxtranspoints[i].lineto1].x,
                boxtranspoints[boxtranspoints[i].lineto1].y);
      XDrawLine(XtDisplay(drawingArea), XtWindow(drawingArea),
                XtScreen(drawingArea)->default_gc, boxtranspoints[i].x,
                boxtranspoints[i].y,
                boxtranspoints[boxtranspoints[i].lineto2].x,
                boxtranspoints[boxtranspoints[i].lineto2].y);
    }
}


// -------------------------------------------------------------------
void ProjectionPicture::DrawBoxesIntoPixmap() {
  XSetForeground(XtDisplay(drawingArea), XtScreen(drawingArea)->default_gc, 0);
  XFillRectangle(XtDisplay(drawingArea),  pixMap,
		 XtScreen(drawingArea)->default_gc, 0, 0, daWidth, daHeight);
  int showBoxesFix(1 - amrPicturePtr->ShowingBoxes());
  for(int i = showBoxesFix*(bPoints-16); i < bPoints-(1-showSubCut)*8; i++) {
    XSetForeground(XtDisplay(drawingArea), XtScreen(drawingArea)->default_gc,
		boxtranspoints[i].color);
    XDrawLine(XtDisplay(drawingArea), pixMap,
    		XtScreen(drawingArea)->default_gc, boxtranspoints[i].x,
		boxtranspoints[i].y,
		boxtranspoints[boxtranspoints[i].lineto1].x,
                boxtranspoints[boxtranspoints[i].lineto1].y);
    XDrawLine(XtDisplay(drawingArea), pixMap,
		XtScreen(drawingArea)->default_gc, boxtranspoints[i].x,
		boxtranspoints[i].y,
                boxtranspoints[boxtranspoints[i].lineto2].x,
                boxtranspoints[boxtranspoints[i].lineto2].y);
  }
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
    XDrawString(XtDisplay(drawingArea), XtWindow(drawingArea),
		XtScreen(drawingArea)->default_gc, boxtranspoints[xHere].x,
                boxtranspoints[xHere].y,
                "X", 1);
    XDrawString(XtDisplay(drawingArea), XtWindow(drawingArea),
		XtScreen(drawingArea)->default_gc, boxtranspoints[yHere].x,
                boxtranspoints[yHere].y,
                "Y", 1);
    XDrawString(XtDisplay(drawingArea), XtWindow(drawingArea),
		XtScreen(drawingArea)->default_gc, boxtranspoints[zHere].x,
                  boxtranspoints[zHere].y,
                "Z", 1);
}


// -------------------------------------------------------------------
void ProjectionPicture::ToggleShowSubCut() {
  showSubCut = (showSubCut ? false : true);
}


// -------------------------------------------------------------------
void ProjectionPicture::SetSubCut(Box newbox) {
  bPoints -= 8;
  AddBox(newbox, 160);
}


// -------------------------------------------------------------------
void ProjectionPicture::SetDrawingAreaDimensions(int w, int h) {

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
