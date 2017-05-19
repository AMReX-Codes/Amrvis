// ---------------------------------------------------------------
// RegionPicture.cpp
// ---------------------------------------------------------------
#include <RegionPicture.H>
#include <Palette.H>
#include <GraphicsAttributes.H>
#include <ProfDataServices.H>
#include <ProfApp.H>

using std::cout;
using std::cerr;
using std::endl;

using namespace amrex;

const int regionBaseHeight(16);
const int defaultDataSizeH(600);

#include <ctime>

// ---------------------------------------------------------------------
RegionPicture::RegionPicture(GraphicsAttributes *gaptr,
                             ProfDataServices *pdsp)
           : gaPtr(gaptr),
	     profDataServicesPtr(pdsp),
	     currentScale(1)
{
  BL_ASSERT(gaptr != nullptr);
  BL_ASSERT(pdsp  != nullptr);

  int nRegions(profDataServicesPtr->GetRegionsProfStats().GetMaxRNumber() + 1);
  ++nRegions;  // ---- for the active time intervals (ati)

  dataSizeH = defaultDataSizeH;
  dataSizeV = nRegions * regionBaseHeight;
  dataSize  = dataSizeH * dataSizeV;

  Box regionBox(IntVect(0, 0), IntVect(dataSizeH - 1, dataSizeV - 1));

  RegionPictureInit(regionBox);
}


// ---------------------------------------------------------------------
RegionPicture::RegionPicture(GraphicsAttributes *gaptr,
                             const amrex::Box &regionBox,
			     ProfApp *profAppPtr)
           : gaPtr(gaptr),
	     profDataServicesPtr(profAppPtr->GetProfDataServicesPtr()),
	     currentScale(profAppPtr->GetCurrentScale())
{
  BL_ASSERT(gaptr != nullptr);
  BL_ASSERT(profAppPtr != nullptr);

  int nRegions(profDataServicesPtr->GetRegionsProfStats().GetMaxRNumber() + 1);
  ++nRegions;  // ---- for the active time intervals (ati)

  dataSizeH = regionBox.length(Amrvis::XDIR);
  dataSizeV = nRegions * regionBaseHeight;
  dataSize  = dataSizeH * dataSizeV;

  RegionPictureInit(regionBox);
}


// ---------------------------------------------------------------------
void RegionPicture::RegionPictureInit(const amrex::Box &regionBox) {

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
  scaledImageData = new unsigned char[imageSize];
  scaledImageDataDim = new unsigned char[imageSize];


  atiDataSizeH = dataSizeH;
  atiDataSizeV = regionBaseHeight;
  atiDataSize  = atiDataSizeH * atiDataSizeV;

  atiImageSizeH = currentScale * atiDataSizeH;
  atiImageSizeV = currentScale * atiDataSizeV;
  int atiWidthpad = gaPtr->PBitmapPaddedWidth(atiImageSizeH);
  atiImageSize = atiImageSizeV * atiWidthpad * gaPtr->PBytesPerPixel();

  atiImageData = new unsigned char[atiDataSize];
  scaledATIImageData = new unsigned char[atiImageSize];
  scaledATIImageDataDim = new unsigned char[atiImageSize];

  subRegion = regionBox;
  cout << "subRegion = " << subRegion << endl;
  sliceFab = new FArrayBox(subRegion, 1);

  SetRegion(0, 0, 0, 0);

  pixMapCreated = false;
}


// ---------------------------------------------------------------------
RegionPicture::~RegionPicture() {
  delete [] imageData;
  delete [] scaledImageData;
  delete [] scaledImageDataDim;
  delete [] atiImageData;
  delete [] scaledATIImageData;
  delete [] scaledATIImageDataDim;
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
  XPutImage(display, pixMap, xgc, atiXImage, 0, 0, 0, imageSizeV-1-regionBaseHeight,
	    imageSizeH, imageSizeV);
  XPutImage(display, pixMap, xgc, xImageDim, 0, 0, 0, 0,
	    imageSizeH/2, imageSizeV);
  XPutImage(display, pixMap, xgc, atiXImageDim, 0, 0, 0, imageSizeV-1-regionBaseHeight,
	    imageSizeH/2, imageSizeV);
           
  DoExposePicture();
}


// ---------------------------------------------------------------------
void RegionPicture::DoExposePicture() {
  XCopyArea(display, pixMap, pictureWindow, xgc, 0, 0,
            imageSizeH, imageSizeV, 0, 0); 

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

  int nRegions(profDataServicesPtr->GetRegionsProfStats().GetMaxRNumber() + 1);
  cout << "nRegions = " << nRegions << endl;

  int allDataSizeH(defaultDataSizeH);
  int allDataSizeV((nRegions + 1) * regionBaseHeight);

  FArrayBox tempSliceFab;

  calcTimeRange = profDataServicesPtr->GetRegionsProfStats().MakeRegionPlt(tempSliceFab, 0,
                                          allDataSizeH, allDataSizeV / (nRegions + 1),
					  regionBoxes);

  cout << "btbtbtbt:  tempSliceFab.box() = " << tempSliceFab.box() << endl;

  tempSliceFab.shift(Amrvis::YDIR, regionBaseHeight);  // ---- for ati
  sliceFab->setVal(tempSliceFab.min(0) - 1.0);
  sliceFab->copy(tempSliceFab);
  Real minUsing(tempSliceFab.min(0)), maxUsing(tempSliceFab.max(0));

  Box fullDomainBox(tempSliceFab.box());
  Real subPercentLow(static_cast<Real>(subRegion.smallEnd(Amrvis::XDIR) -
                                       fullDomainBox.smallEnd(Amrvis::XDIR)) /
                     static_cast<Real>(fullDomainBox.length(Amrvis::XDIR) - 1));
  Real subPercentHigh(static_cast<Real>(subRegion.bigEnd(Amrvis::XDIR) -
                                        fullDomainBox.smallEnd(Amrvis::XDIR)) /
                      static_cast<Real>(fullDomainBox.length(Amrvis::XDIR) - 1));

  Real calcTime(calcTimeRange.stopTime - calcTimeRange.startTime);
  subTimeRange.startTime = calcTimeRange.startTime + (subPercentLow  * calcTime);
  subTimeRange.stopTime  = calcTimeRange.startTime + (subPercentHigh * calcTime);
  cout << "calcTimeRange = " << calcTimeRange << endl;
  cout << "subTimeRange  = " << subTimeRange << endl;

//std::ofstream tfout("sliceFab.fab");
//sliceFab->writeOn(tfout);
//tfout.close();

  cout << "tttttttt:  tempSliceFab.box() = " << tempSliceFab.box() << endl;
  cout << "ssssssss:  sliceFab->box() = " << sliceFab->box() << endl;
  cout << "ssssssss:  sliceFab->minmax() = " << sliceFab->min(0) << "  " << sliceFab->max(0) << endl;
  CreateImage(*(sliceFab), imageData, dataSizeH, dataSizeV, minUsing, maxUsing, palPtr);
  CreateScaledImage(&(xImage), currentScale,
                imageData, scaledImageData, dataSizeH, dataSizeV,
                imageSizeH, imageSizeV);
  CreateScaledImage(&(xImageDim), currentScale,
                imageData, scaledImageDataDim, dataSizeH, dataSizeV,
                imageSizeH, imageSizeV, true);  // ---- make dim
  for(int j(0); j < atiDataSizeV; ++j) {
    for(int i(0); i < atiDataSizeH; ++i) {
      atiImageData[j * atiDataSizeH + i] = 0;
    }
  }
  CreateScaledImage(&(atiXImage), currentScale,
                atiImageData, scaledATIImageData, atiDataSizeH, atiDataSizeV,
                atiImageSizeH, atiImageSizeV);
  CreateScaledImage(&(atiXImageDim), currentScale,
                atiImageData, scaledATIImageDataDim, atiDataSizeH, atiDataSizeV,
                atiImageSizeH, atiImageSizeV, true);  // ---- make dim

  //if( ! pltAppPtr->PaletteDrawn()) {
    //pltAppPtr->PaletteDrawn(true);
    palptr->DrawPalette(minUsing, maxUsing, "%8.2f");
  //}

  APDraw(0, 0);
}


// -------------------------------------------------------------------
// ---- convert Real to char in imagedata from fab
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
      if(dIndex >= fab.nPts()) {
	cout << "**** dIndex fab.nPts() = " << dIndex << "  " << fab.nPts() << endl;
      }
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
				      int imagesizeh, int imagesizev,
				      bool dim)
{ 
  int widthpad(gaPtr->PBitmapPaddedWidth(imagesizeh));
  *ximage = XCreateImage(display, gaPtr->PVisual(), gaPtr->PDepth(),
                         ZPixmap, 0, (char *) scaledimagedata,
		         widthpad, imagesizev, XBitmapPad(display),
			 widthpad * gaPtr->PBytesPerPixel());

  unsigned char imm1;
  if(dim) {
    for(int j(0); j < imagesizev; ++j) {
      int jtmp(datasizeh * (j / scale));
      for(int i(0); i < widthpad; ++i) {
        int itmp(i / scale);
        imm1 = imagedata[ itmp + jtmp ];
        XPutPixel(*ximage, i, j, palPtr->makePixelDim(imm1));
      }
    }
  } else {
    for(int j(0); j < imagesizev; ++j) {
      int jtmp(datasizeh * (j / scale));
      for(int i(0); i < widthpad; ++i) {
        int itmp(i / scale);
        imm1 = imagedata[ itmp + jtmp ];
        XPutPixel(*ximage, i, j, palPtr->makePixel(imm1));
      }
    }
  }
}


// ---------------------------------------------------------------------
void RegionPicture::APChangeScale(int newScale, int previousScale) { 
  currentScale = newScale;
  imageSizeH = currentScale * dataSizeH;
  imageSizeV = currentScale * dataSizeV;
  int widthpad = gaPtr->PBitmapPaddedWidth(imageSizeH);
  imageSize  = widthpad * imageSizeV * gaPtr->PBytesPerPixel();
  XClearWindow(display, pictureWindow);

  if(pixMapCreated) {
    XFreePixmap(display, pixMap);
  }  
  pixMap = XCreatePixmap(display, pictureWindow,
			 imageSizeH, imageSizeV, gaPtr->PDepth());
  pixMapCreated = true;

  delete [] scaledImageData;
  delete [] scaledImageDataDim;
  scaledImageData = new unsigned char[imageSize];
  scaledImageDataDim = new unsigned char[imageSize];
  CreateScaledImage(&xImage, currentScale,
                    imageData, scaledImageData, dataSizeH, dataSizeV,
                    imageSizeH, imageSizeV);
  CreateScaledImage(&xImageDim, currentScale,
                    imageData, scaledImageDataDim, dataSizeH, dataSizeV,
                    imageSizeH, imageSizeV, true);  // ---- make dim

  atiImageSizeH = currentScale * atiDataSizeH;
  atiImageSizeV = currentScale * atiDataSizeV;
  int atiWidthpad = gaPtr->PBitmapPaddedWidth(atiImageSizeH);
  atiImageSize  = atiWidthpad * atiImageSizeV * gaPtr->PBytesPerPixel();
  delete [] scaledATIImageData;
  delete [] scaledATIImageDataDim;
  scaledATIImageData = new unsigned char[atiImageSize];
  scaledATIImageDataDim = new unsigned char[atiImageSize];
  CreateScaledImage(&(atiXImage), currentScale,
                atiImageData, scaledATIImageData, atiDataSizeH, atiDataSizeV,
                atiImageSizeH, atiImageSizeV);
  CreateScaledImage(&(atiXImageDim), currentScale,
                atiImageData, scaledATIImageDataDim, atiDataSizeH, atiDataSizeV,
                atiImageSizeH, atiImageSizeV, true);  // ---- make dim

  APDraw(0, 0);
}


// ---------------------------------------------------------------------
XImage *RegionPicture::GetPictureXImage() {
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
void RegionPicture::SetRegion(int startX, int startY, int endX, int endY) {
  regionX = startX;
  regionY = startY;
  region2ndX = endX;
  region2ndY = endY;
}


// ---------------------------------------------------------------------
Real RegionPicture::DataValue(int i, int j, bool &outOfRange) {
  IntVect iv;
  iv[Amrvis::XDIR] = i;
  iv[Amrvis::YDIR] = j;
  if(sliceFab->box().contains(iv)) {
    outOfRange = false;
    return (*sliceFab)(iv);
  } else {
    outOfRange = true;
    return -42.0;
  }
}
// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
