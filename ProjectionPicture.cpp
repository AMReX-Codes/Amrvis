
//
// $Id: ProjectionPicture.cpp,v 1.51 2003-02-28 02:01:38 vince Exp $
//

// ---------------------------------------------------------------
// ProjectionPicture.cpp
// ---------------------------------------------------------------
#include <X11/X.h>

#include "ProjectionPicture.H"
#include "PltApp.H"
#include "PltAppState.H"
#include "DataServices.H"
#include "Volume.H"

#include <cmath>
#include <ctime>
using std::min;
using std::max;
using std::cout;
using std::endl;

using std::cout;
using std::endl;
using std::min;
using std::max;

// -------------------------------------------------------------------
ProjectionPicture::ProjectionPicture(PltApp *pltappptr, ViewTransform *vtptr,
                                     Palette *PalettePtr,
                                     Widget da, int w, int h)
{
  int lev;
  pltAppPtr = pltappptr;
  amrPicturePtr = pltAppPtr->GetAmrPicturePtr(XY); 
  minDrawnLevel = pltAppPtr->GetPltAppState()->MinDrawnLevel();
  maxDrawnLevel = pltAppPtr->GetPltAppState()->MaxDrawnLevel();
  drawingArea = da; 
  viewTransformPtr = vtptr;
  daWidth = w;
  daHeight = h;
  palettePtr = PalettePtr;

  volumeBoxColor = (unsigned char) AVGlobals::GetBoxColor();
  volumeBoxColor = max((unsigned char) palettePtr->PaletteStart(),
                       min((unsigned char) AVGlobals::MaxPaletteIndex(),
		       volumeBoxColor));

  showSubCut = false;
  pixCreated = false;
  imageData = NULL;
  volpackImageData = 0;
  maxDataLevel = pltAppPtr->GetPltAppState()->MaxAllowableLevel();
  theDomain = amrPicturePtr->GetSubDomain();

#ifdef BL_VOLUMERENDER
  volRender = new VolRender(theDomain, minDrawnLevel, maxDrawnLevel, palettePtr,
			    pltAppPtr->GetLightingFileName());
#endif

  SetDrawingAreaDimensions(daWidth, daHeight);

  const AmrData &amrData = pltAppPtr->GetDataServicesPtr()->AmrDataRef();

  boxReal.resize(maxDataLevel + 1);
  boxTrans.resize(maxDataLevel + 1);
  for(lev = minDrawnLevel; lev <= maxDataLevel; ++lev) {
    int nBoxes(amrData.NIntersectingGrids(lev, theDomain[lev]));
    boxReal[lev].resize(nBoxes);
    boxTrans[lev].resize(nBoxes);
  }
  subCutColor = 160;
  sliceColor = 100;
  boxColors.resize(maxDataLevel + 1);
  for(lev = minDrawnLevel; lev <= maxDataLevel; ++lev) {
    int iBoxIndex(0);
    if(lev == minDrawnLevel) {
      boxColors[lev] = pltAppPtr->GetPalettePtr()->WhiteIndex();
    } else {
      boxColors[lev] = palettePtr->SafePaletteIndex(lev);
    }
    boxColors[lev] = max(0, min(AVGlobals::MaxPaletteIndex(), boxColors[lev]));
    for(int iBox(0); iBox < amrData.boxArray(lev).size(); ++iBox) {
      Box temp(amrData.boxArray(lev)[iBox]);
      if(temp.intersects(theDomain[lev])) {
        temp &= theDomain[lev];
        AddBox(temp.refine(AVGlobals::CRRBetweenLevels(lev, maxDataLevel,
		amrData.RefRatio())), iBoxIndex, lev);
	++iBoxIndex;
      }
    }
  }
  Box alignedBox(theDomain[minDrawnLevel]);
  alignedBox.refine(AVGlobals::CRRBetweenLevels(minDrawnLevel, maxDataLevel,
		     amrData.RefRatio()));
  realBoundingBox = RealBox(alignedBox);

  LoadSlices(alignedBox);

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
}  // end ProjectionPicture()


// -------------------------------------------------------------------
ProjectionPicture::~ProjectionPicture() {
  delete [] imageData;
  delete [] volpackImageData;
  if(pixCreated) {
    XFreePixmap(XtDisplay(drawingArea), pixMap);
  }
#ifdef BL_VOLUMERENDER
  delete volRender;
#endif
}


// -------------------------------------------------------------------
void ProjectionPicture::AddBox(const Box &theBox, int index, int level) {
    boxReal[level][index] = RealBox(theBox);
}


// -------------------------------------------------------------------
void ProjectionPicture::TransformBoxPoints(int iLevel, int iBoxIndex) {
  Real px, py, pz;

  for(int i(0); i < NVERTICIES; ++i) {
    viewTransformPtr->
	   TransformPoint(boxReal[iLevel][iBoxIndex].vertices[i].component[0],
			  boxReal[iLevel][iBoxIndex].vertices[i].component[1],
			  boxReal[iLevel][iBoxIndex].vertices[i].component[2],
			  px, py, pz);
    boxTrans[iLevel][iBoxIndex].vertices[i] =
	     TransPoint((int)(px + 0.5), daHeight - (int)(py + 0.5));
  }
}


// -------------------------------------------------------------------
void ProjectionPicture::MakeBoxes() {
  minDrawnLevel = pltAppPtr->GetPltAppState()->MinDrawnLevel();
  maxDrawnLevel = pltAppPtr->GetPltAppState()->MaxDrawnLevel();

  if(pltAppPtr->GetPltAppState()->GetShowingBoxes()) {
    for(int iLevel(minDrawnLevel); iLevel <= maxDrawnLevel; ++iLevel) {
      int nBoxes(boxTrans[iLevel].size());
      for(int iBox(0); iBox < nBoxes; ++iBox) {
        TransformBoxPoints(iLevel, iBox);
      }
    }
  }

  MakeAuxiliaries(); // make subcut, boundingbox, slices
}


// -------------------------------------------------------------------
void ProjectionPicture::MakeAuxiliaries() {
  if(showSubCut) {
    MakeSubCutBox();
  }
  MakeBoundingBox();
  MakeSlices();
}

// -------------------------------------------------------------------
void ProjectionPicture::MakeBoundingBox() {
  Real px, py, pz;
  for(int i(0); i < NVERTICIES; ++i) {
    viewTransformPtr->
          TransformPoint(realBoundingBox.vertices[i].component[0],
                         realBoundingBox.vertices[i].component[1],
                         realBoundingBox.vertices[i].component[2],
                         px, py, pz);
    transBoundingBox.vertices[i] = TransPoint((int)(px + 0.5), 
                                              daHeight - (int)(py + 0.5));
  }
} 


// -------------------------------------------------------------------
void ProjectionPicture::MakeSubCutBox() {
  Real px, py, pz;
  minDrawnLevel = pltAppPtr->GetPltAppState()->MinDrawnLevel();
  maxDrawnLevel = pltAppPtr->GetPltAppState()->MaxDrawnLevel();

  for(int i(0); i < NVERTICIES; ++i) {
    viewTransformPtr->
          TransformPoint(realSubCutBox.vertices[i].component[0],
                         realSubCutBox.vertices[i].component[1],
                         realSubCutBox.vertices[i].component[2],
                         px, py, pz);
    transSubCutBox.vertices[i] = TransPoint((int) (px + 0.5), 
                                            daHeight - (int) (py + 0.5));
  }
}   


// -------------------------------------------------------------------
void ProjectionPicture::MakeSlices() {
  Real px, py, pz;
  for(int j(0); j < 3 ; ++j) {
    for(int i(0); i < 4; ++i) {
      viewTransformPtr->TransformPoint(realSlice[j].edges[i].component[0],
                                       realSlice[j].edges[i].component[1],
                                       realSlice[j].edges[i].component[2],
                                       px, py, pz);
      transSlice[j].edges[i] = TransPoint((int) (px + 0.5), 
                                          daHeight - (int) (py + 0.5));
    }
  }
}

// -------------------------------------------------------------------
void ProjectionPicture::MakePicture() {
#ifdef BL_VOLUMERENDER
  clock_t time0 = clock();

  scale[XDIR] = viewTransformPtr->GetScale();
  scale[YDIR] = viewTransformPtr->GetScale();
  scale[ZDIR] = viewTransformPtr->GetScale();

  Real mvmat[4][4];

  Real tempLenRatio(longestWindowLength/(scale[longestBoxSideDir]*longestBoxSide));
  Real tempLen(0.5 * tempLenRatio);
  viewTransformPtr->SetAdjustments(tempLen, daWidth, daHeight);
  viewTransformPtr->MakeTransform();

  viewTransformPtr->GetRenderRotationMat(mvmat);
  volRender->MakePicture(mvmat, tempLen, daWidth, daHeight);

  // map imageData colors to colormap range
  Palette *palPtr = pltAppPtr->GetPalettePtr();
cout << "&&&&&&&&&&&&&&& colorslots palsize = " << palPtr->ColorSlots() << "  " << palPtr->PaletteSize() << endl;
  if(palPtr->ColorSlots() != palPtr->PaletteSize()) {
    const unsigned char *remapTable = palPtr->RemapTable();
    for(int idat(0); idat < daWidth * daHeight; ++idat) {
      volpackImageData[idat] = remapTable[(unsigned char) volpackImageData[idat]];
    }
  }

  for(int j(0); j < daHeight; ++j ) {
    for(int i(0); i < daWidth; ++i ) {
      unsigned char imm1 = volpackImageData[i+j*daWidth];
      XPutPixel(PPXImage, i, j, palPtr->makePixel(imm1));
      // imageData[i+j*daWidth] = imm1;
    }
  }

  XPutImage(XtDisplay(drawingArea), pixMap, XtScreen(drawingArea)->
            default_gc, PPXImage, 0, 0, 0, 0, daWidth, daHeight);

  cout << "----- make picture time = " << ((clock()-time0)/1000000.0) << endl;

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
  minDrawnLevel = pltAppPtr->GetPltAppState()->MinDrawnLevel();
  maxDrawnLevel = pltAppPtr->GetPltAppState()->MaxDrawnLevel();
  
  if(pltAppPtr->GetPltAppState()->GetShowingBoxes()) {
    for(int iLevel(iFromLevel); iLevel <= iToLevel; ++iLevel) {
      // FIXME:
      XSetForeground(XtDisplay(drawingArea), XtScreen(drawingArea)->default_gc,
                     boxColors[iLevel]);
      int nBoxes(boxTrans[iLevel].size());
      for(int iBox(0); iBox < nBoxes; ++iBox) {
        boxTrans[iLevel][iBox].Draw(XtDisplay(drawingArea), drawable,
                                    XtScreen(drawingArea)->default_gc);
        
      }
    }
  }
  DrawAuxiliaries(drawable);
  // FIXME:
  XSetForeground(XtDisplay(drawingArea), XtScreen(drawingArea)->default_gc,
                 boxColors[minDrawnLevel]);

//viewTransformPtr->ViewRotationMat();

}


// -------------------------------------------------------------------
void ProjectionPicture::DrawAuxiliaries(const Drawable &drawable) {
  if(showSubCut) {
    // FIXME:
    XSetForeground(XtDisplay(drawingArea), XtScreen(drawingArea)->default_gc,
                   subCutColor);
    transSubCutBox.Draw(XtDisplay(drawingArea),drawable,
                        XtScreen(drawingArea)->default_gc);
  }    
  //bounding box
  // FIXME:
  XSetForeground(XtDisplay(drawingArea), XtScreen(drawingArea)->default_gc,
                 boxColors[minDrawnLevel]);
  transBoundingBox.Draw(XtDisplay(drawingArea), drawable,
                        XtScreen(drawingArea)->default_gc);
  DrawSlices(drawable);
}


// -------------------------------------------------------------------
void ProjectionPicture::DrawSlices(const Drawable &drawable) {
  Palette* palPtr = pltAppPtr->GetPalettePtr();
  XSetForeground(XtDisplay(drawingArea), XtScreen(drawingArea)->default_gc,
                 palPtr->pixelate(sliceColor));
  for(int k = 0; k < 3; ++k) {
    transSlice[k].Draw(XtDisplay(drawingArea), drawable,
                       XtScreen(drawingArea)->default_gc);
  }
}    


// -------------------------------------------------------------------
void ProjectionPicture::LoadSlices(const Box &surroundingBox) {
  for(int j = 0; j < 3 ; ++j) {
    int slice(pltAppPtr->GetAmrPicturePtr(j)->GetSlice());
    realSlice[j] = RealSlice(j,slice,surroundingBox);
  }
}


// -------------------------------------------------------------------
void ProjectionPicture::ChangeSlice(int Dir, int newSlice) {
  for(int j(0); j < 3; ++j) {
    if(Dir == j) {
      realSlice[j].ChangeSlice(newSlice);
    }
  }
}


// -------------------------------------------------------------------
XImage *ProjectionPicture::DrawBoxesIntoPixmap(int iFromLevel, int iToLevel) {
  XSetForeground(XtDisplay(drawingArea), XtScreen(drawingArea)->default_gc,
                 palettePtr->BlackIndex());

  XFillRectangle(XtDisplay(drawingArea),  pixMap,
		 XtScreen(drawingArea)->default_gc, 0, 0, daWidth, daHeight);
  DrawBoxesIntoDrawable(pixMap, iFromLevel, iToLevel);
  
  return XGetImage(XtDisplay(drawingArea), pixMap, 0, 0,
                   daWidth, daHeight, AllPlanes, ZPixmap);
}


// -------------------------------------------------------------------
void ProjectionPicture::DrawPicture() {
  XCopyArea(XtDisplay(drawingArea), pixMap, XtWindow(drawingArea),
            XtScreen(drawingArea)->default_gc, 0, 0, daWidth, daHeight, 0, 0);
}


// -------------------------------------------------------------------
void ProjectionPicture::LabelAxes() {
  const int xHere(1);  // Where to label axes with X, Y, and Z
  const int yHere(3);
  const int zHere(4);
  minDrawnLevel = pltAppPtr->GetPltAppState()->MinDrawnLevel();
  maxDrawnLevel = pltAppPtr->GetPltAppState()->MaxDrawnLevel();
  XDrawString(XtDisplay(drawingArea), XtWindow(drawingArea),
              XtScreen(drawingArea)->default_gc,
              transBoundingBox.vertices[xHere].x,
              transBoundingBox.vertices[xHere].y,
              "X", 1);
  XDrawString(XtDisplay(drawingArea), XtWindow(drawingArea),
              XtScreen(drawingArea)->default_gc,
              transBoundingBox.vertices[yHere].x,
              transBoundingBox.vertices[yHere].y,
              "Y", 1);
  XDrawString(XtDisplay(drawingArea), XtWindow(drawingArea),
              XtScreen(drawingArea)->default_gc,
              transBoundingBox.vertices[zHere].x,
              transBoundingBox.vertices[zHere].y,
              "Z", 1);
  
}


// -------------------------------------------------------------------
void ProjectionPicture::ToggleShowSubCut() {
  showSubCut = (showSubCut ? false : true);
}


// -------------------------------------------------------------------
void ProjectionPicture::SetSubCut(const Box &newbox) {
  realSubCutBox = RealBox(newbox);
  MakeSubCutBox();
}


// -------------------------------------------------------------------
void ProjectionPicture::SetDrawingAreaDimensions(int w, int h) {

  minDrawnLevel = pltAppPtr->GetPltAppState()->MinDrawnLevel();
  maxDrawnLevel = pltAppPtr->GetPltAppState()->MaxDrawnLevel();
  
  daWidth = w;
  daHeight = h;
  int widthpad = (1+(w-1)/(XBitmapPad(XtDisplay(drawingArea))/8))*XBitmapPad(XtDisplay(drawingArea))/8;

  if(imageData != NULL) {
    delete [] imageData;
    delete [] volpackImageData;
  }
  volpackImageData = new unsigned char[daWidth*daHeight];
  imageData = static_cast<unsigned char *> (malloc(widthpad * daHeight *
                                        XBitmapPad(XtDisplay(drawingArea)) / 8));
  if(imageData == NULL || volpackImageData == NULL ) {
    cout << " SetDrawingAreaDimensions::imageData : new failed" << endl;
    BoxLib::Abort("Exiting.");
  }
  
  viewTransformPtr->SetScreenPosition(daWidth / 2, daHeight / 2);
  
  PPXImage = XCreateImage(XtDisplay(drawingArea),
			  XDefaultVisual(XtDisplay(drawingArea),
					 DefaultScreen(XtDisplay(drawingArea))),
			  DefaultDepthOfScreen(XtScreen(drawingArea)), ZPixmap, 0,
			  (char *) imageData, widthpad, daHeight,
			  XBitmapPad(XtDisplay(drawingArea)),
			  widthpad * XBitmapPad(XtDisplay(drawingArea)) / 8);
  
  if(pixCreated) {
    XFreePixmap(XtDisplay(drawingArea), pixMap);
  }
  pixMap = XCreatePixmap(XtDisplay(drawingArea), XtWindow(drawingArea),
			 daWidth, daHeight,
			 DefaultDepthOfScreen(XtScreen(drawingArea)));
  pixCreated = true;

#ifdef BL_VOLUMERENDER
  // --- set the image buffer
  volRender->SetImage( (unsigned char *) volpackImageData, daWidth, daHeight,
                       VP_LUMINANCE);
#endif
  longestWindowLength  = (Real) max(daWidth, daHeight);
  shortestWindowLength = (Real) min(daWidth, daHeight);
  if(longestWindowLength == 0) {  // this happens when x deletes this window
#ifdef BL_VOLUMERENDER
    volRender->SetAspect(1.0);
#endif
    viewTransformPtr->SetAspect(1.0);
  } else {
#ifdef BL_VOLUMERENDER
    volRender->SetAspect(shortestWindowLength/longestWindowLength);
#endif
    viewTransformPtr->SetAspect(shortestWindowLength/longestWindowLength);
  }

  Box alignedBox(theDomain[minDrawnLevel]);
  const AmrData &amrData = pltAppPtr->GetDataServicesPtr()->AmrDataRef();
  alignedBox.refine(AVGlobals::CRRBetweenLevels(minDrawnLevel, maxDataLevel,
		     amrData.RefRatio()));
  longestBoxSide = (Real) alignedBox.longside(longestBoxSideDir);
}


//--------------------------------------------------------------------
//----- BOX AND POINT STUFF-------------------------------------------
//--------------------------------------------------------------------

//--------------------------------------------------------------------
RealBox::RealBox() {
}


//--------------------------------------------------------------------
RealBox::RealBox(const Box &theBox) {
    Real dimensions[6] = { theBox.smallEnd(XDIR), theBox.smallEnd(YDIR),
                           theBox.smallEnd(ZDIR), theBox.bigEnd(XDIR)+1,
                           theBox.bigEnd(YDIR)+1, theBox.bigEnd(ZDIR)+1 };

    vertices[0] = RealPoint(dimensions[0], dimensions[1], dimensions[2]);
    vertices[1] = RealPoint(dimensions[3], dimensions[1], dimensions[2]);
    vertices[2] = RealPoint(dimensions[3], dimensions[4], dimensions[2]);
    vertices[3] = RealPoint(dimensions[0], dimensions[4], dimensions[2]);
    vertices[4] = RealPoint(dimensions[0], dimensions[1], dimensions[5]);
    vertices[5] = RealPoint(dimensions[3], dimensions[1], dimensions[5]);
    vertices[6] = RealPoint(dimensions[3], dimensions[4], dimensions[5]);
    vertices[7] = RealPoint(dimensions[0], dimensions[4], dimensions[5]);
}


//--------------------------------------------------------------------
TransBox::TransBox() {
  TransPoint zero(0,0);
  for(int i(0); i < 8 ; ++i) {
    vertices[i] = zero;
  }
}


//--------------------------------------------------------------------
void TransBox::Draw(Display *display, Window window, GC gc) {
  for(int j(0); j < 2; ++j) {
    for(int i(0); i < 3; ++i) {
      XDrawLine(display, window, gc,
                vertices[j*4+i].x, vertices[j*4+i].y,
                vertices[j*4+i+1].x, vertices[j*4+i+1].y);
    }
    XDrawLine(display, window, gc, vertices[j*4].x, vertices[j*4].y, 
              vertices[j*4+3].x, vertices[j*4+3].y);
  }
  for(int k(0); k < 4; ++k) {
    XDrawLine(display, window, gc, vertices[k].x, vertices[k].y,
              vertices[k+4].x, vertices[k+4].y);
  }
            
}


//--------------------------------------------------------------------
RealSlice::RealSlice() {
  RealPoint zero(0.0, 0.0, 0.0);
  for(int i = 0; i < 4; ++i) {
    edges[i] = zero;
  }
}


//--------------------------------------------------------------------
RealSlice::RealSlice(RealPoint p1, RealPoint p2, RealPoint p3, RealPoint p4) {
  edges[0] = p1;
  edges[1] = p2;
  edges[2] = p3;
  edges[3] = p4;
}


//--------------------------------------------------------------------
RealSlice::RealSlice(int count, int slice, const Box &worldBox) {
  Real dimensions[6] = { worldBox.smallEnd(XDIR), worldBox.smallEnd(YDIR),
                         worldBox.smallEnd(ZDIR), worldBox.bigEnd(XDIR)+1,
                         worldBox.bigEnd(YDIR)+1, worldBox.bigEnd(ZDIR)+1 };
  dirOfFreedom = count;
  if(dirOfFreedom == 0) {
    edges[0] = RealPoint(dimensions[0], dimensions[1], slice);
    edges[1] = RealPoint(dimensions[3], dimensions[1], slice);
    edges[2] = RealPoint(dimensions[3], dimensions[4], slice);
    edges[3] = RealPoint(dimensions[0], dimensions[4], slice);
  }
  if(dirOfFreedom == 1) {
    edges[0] = RealPoint(dimensions[0], slice, dimensions[2]);
    edges[1] = RealPoint(dimensions[3], slice, dimensions[2]);
    edges[2] = RealPoint(dimensions[3], slice, dimensions[5]);
    edges[3] = RealPoint(dimensions[0], slice, dimensions[5]);
  }
  if(dirOfFreedom == 2) {
    edges[0] = RealPoint(slice, dimensions[1], dimensions[2]);
    edges[1] = RealPoint(slice, dimensions[4], dimensions[2]);
    edges[2] = RealPoint(slice, dimensions[4], dimensions[5]);
    edges[3] = RealPoint(slice, dimensions[1], dimensions[5]);
  }
}


//--------------------------------------------------------------------
void RealSlice::ChangeSlice(int NewSlice) {
  for(int k(0); k < 4; ++k) {
    edges[k].component[2-dirOfFreedom] = NewSlice;
  }
}


//--------------------------------------------------------------------
TransSlice::TransSlice() {
  TransPoint zero(0,0);
  for(int i = 0; i < 4; ++i) {
    edges[i] = zero;
  }
}


//--------------------------------------------------------------------
TransSlice::TransSlice(TransPoint p1, TransPoint p2,
                       TransPoint p3, TransPoint p4)
{
  edges[0] = p1;
  edges[1] = p2;
  edges[2] = p3;
  edges[3] = p4;
}


//--------------------------------------------------------------------
void TransSlice::Draw(Display *display, Window window, GC gc) {
  for(int j = 0; j < 3; ++j) {
    XDrawLine(display, window, gc,
              edges[j].x, edges[j].y,
              edges[j+1].x,edges[j+1].y);
  }
  XDrawLine(display, window, gc, edges[0].x, edges[0].y, edges[3].x, edges[3].y);
}
//--------------------------------------------------------------------
//--------------------------------------------------------------------
