// ---------------------------------------------------------------
// RegionPicture.cpp
// ---------------------------------------------------------------
#include <RegionPicture.H>
#include <Palette.H>

using std::cout;
using std::cerr;
using std::endl;

using namespace amrex;

#include <ctime>

// ---------------------------------------------------------------------
RegionPicture::RegionPicture(GraphicsAttributes *gaptr)
           : gaPtr(gaptr),
             myView(Amrvis::XY),
	     currentScale(1)
{
  int i, ilev;

  dataSizeH = 600;
  dataSizeV = 100;
  dataSize  = dataSizeH * dataSizeV;  // for a picture (slice).

  RegionPictureInit();
}


// ---------------------------------------------------------------------
void RegionPicture::RegionPictureInit() {

    Box sliceBox(IntVect(0, dataSizeH - 1), IntVect(0, dataSizeV - 1));
    sliceFab = new FArrayBox(sliceBox, 1);

  display = gaPtr->PDisplay();
  xgc = gaPtr->PGC();

  imageSizeH = currentScale * dataSizeH;
  imageSizeV = currentScale * dataSizeV;
  if(imageSizeH > 65528 || imageSizeV > 65528) {  // not quite sizeof(short)
    // XImages cannot be larger than this
    cerr << "*** imageSizeH = " << imageSizeH << endl;
    cerr << "*** imageSizeV = " << imageSizeV << endl;
    amrex::Abort("Error in RegionPicture:  Image size too large.  Exiting.");
  }
  int widthpad = gaPtr->PBitmapPaddedWidth(imageSizeH);
  imageSize = imageSizeV * widthpad * gaPtr->PBytesPerPixel();

  imageData = new unsigned char[dataSize];
  scaledImageData = (unsigned char *) malloc(imageSize);

    hColor = 220;
    vColor = 65;
  pixMapCreated = false;
}


// ---------------------------------------------------------------------
RegionPicture::~RegionPicture() {
  delete [] imageData;
  free(scaledImageData);
  delete sliceFab;

  if(pixMapCreated) {
    XFreePixmap(display, pixMap);
  }
}


// ---------------------------------------------------------------------
void RegionPicture::APDraw(int fromLevel, int toLevel) {
  if( ! pixMapCreated) {
    pixMap = XCreatePixmap(display, pictureWindow,
			   imageSizeH, imageSizeV, gaPtr->PDepth());
    pixMapCreated = true;
  }  
 
  XPutImage(display, pixMap, xgc, xImage, 0, 0, 0, 0,
	    imageSizeH, imageSizeV);
           
    DoExposePicture();
}


// ---------------------------------------------------------------------
void RegionPicture::DoExposePicture() {
  if(pltAppPtr->Animating()) {
  } else {
    if(pendingTimeOut == 0) {
      XCopyArea(display, pixMap, pictureWindow, 
 		xgc, 0, 0, imageSizeH, imageSizeV, 0, 0); 

/*
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
*/
    }
  }
}


// ---------------------------------------------------------------------
void RegionPicture::CreatePicture(Window drawPictureHere, Palette *palptr) {
  palPtr = palptr;
  pictureWindow = drawPictureHere;
  APMakeImages(palptr);
}


// ---------------------------------------------------------------------
void RegionPicture::APMakeImages(Palette *palptr) {
  BL_ASSERT(palptr != NULL);
  palPtr = palptr;

  Real minUsing, maxUsing;
  pltAppStatePtr->GetMinMax(minUsing, maxUsing);

    amrex::DataServices::Dispatch(amrex::DataServices::FillVarOneFab, dataServicesPtr,
		           (void *) (sliceFab),
			   (void *) (&(sliceFab->box())),
			   iLevel,
			   (void *) &currentDerived);
    CreateImage(*(sliceFab), imageData, dataSizeH, dataSizeV, minUsing, maxUsing, palPtr);
    CreateScaledImage(&(xImage), currentScale, 1,
                imageData, scaledImageData, dataSizeH, dataSizeV,
                imageSizeH, imageSizeV);

  if( ! pltAppPtr->PaletteDrawn()) {
    pltAppPtr->PaletteDrawn(true);
    palptr->DrawPalette(minUsing, maxUsing, "%8.2f");
  }
  APDraw(0, 0);

}


// -------------------------------------------------------------------
// convert Real to char in imagedata from fab
void RegionPicture::CreateImage(const FArrayBox &fab, unsigned char *imagedata,
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
  const Real *vfDataPoint = 0;
  if(bCartGrid) {
    BL_ASSERT(vfracFab != NULL);
    vfDataPoint = vfracFab->dataPtr();
  }
      
  // flips the image in Vert dir: j => datasizev-j-1
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
}


// ---------------------------------------------------------------------
void RegionPicture::CreateScaledImage(XImage **ximage, int scale,
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

	for(int j(0); j < imagesizev; ++j) {
	    int jtmp(datasizeh * (j/scale));
	    for(int i(0); i < widthpad; ++i) {
		int itmp(i / scale);
		unsigned char imm1(imagedata[ itmp + jtmp ]);
		XPutPixel(*ximage, i, j, palPtr->makePixel(imm1));
	      }
	  }

}


// ---------------------------------------------------------------------
void RegionPicture::APChangeScale(int newScale, int previousScale) { 
  imageSizeH = newScale * dataSizeH;
  imageSizeV = newScale * dataSizeV;
  int widthpad = gaPtr->PBitmapPaddedWidth(imageSizeH);
  imageSize  = widthpad * imageSizeV * gaPtr->PBytesPerPixel();
  XClearWindow(display, pictureWindow);

  if(pixMapCreated) {
    XFreePixmap(display, pixMap);
  }  
  pixMap = XCreatePixmap(display, pictureWindow,
			   imageSizeH, imageSizeV, gaPtr->PDepth());
  pixMapCreated = true;

    free(scaledImageData);
    scaledImageData = (unsigned char *) malloc(imageSize);
    CreateScaledImage(&xImage, newScale, 1,
                imageData, scaledImageData, dataSizeH, dataSizeV,
                imageSizeH, imageSizeV);

  hLine = ((hLine / previousScale) * newScale) + (newScale - 1);
  vLine = ((vLine / previousScale) * newScale) + (newScale - 1);
  APDraw(0, 0);
}


// ---------------------------------------------------------------------
XImage *RegionPicture::GetPictureXImage(const bool bdrawboxesintoimage) {
  int xbox, ybox, wbox, hbox;
  XImage *ximage;

  ximage = XGetImage(display, pixMap, 0, 0,
		imageSizeH, imageSizeV, AllPlanes, ZPixmap);

  if(pixMapCreated) {
    XFreePixmap(display, pixMap);
  }  
  pixMap = XCreatePixmap(display, pictureWindow,
			   imageSizeH, imageSizeV, gaPtr->PDepth());
  pixMapCreated = true;
  APDraw(0, 0);
  return ximage;
}


// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
