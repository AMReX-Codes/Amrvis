// ---------------------------------------------------------------
// RegionPicture.cpp
// ---------------------------------------------------------------
#include <RegionPicture.H>
#include <Palette.H>
#include <GraphicsAttributes.H>
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
                             amrex::DataServices *pdsp)
           : gaPtr(gaptr),
	     dataServicesPtr(pdsp),
	     currentScale(1)
{
  BL_ASSERT(gaptr != nullptr);
  BL_ASSERT(pdsp  != nullptr);

  int nRegions(dataServicesPtr->GetRegionsProfStats().GetMaxRNumber() + 1);
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
	     dataServicesPtr(profAppPtr->GetDataServicesPtr()),
	     currentScale(profAppPtr->GetCurrentScale())
{
  BL_ASSERT(gaptr != nullptr);
  BL_ASSERT(profAppPtr != nullptr);

  int nRegions(dataServicesPtr->GetRegionsProfStats().GetMaxRNumber() + 1);
  ++nRegions;  // ---- for the active time intervals (ati)

  dataSizeH = regionBox.length(Amrvis::XDIR);
  dataSizeV = nRegions * regionBaseHeight;
  dataSize  = dataSizeH * dataSizeV;

  RegionPictureInit(regionBox);
}


// ---------------------------------------------------------------------
void RegionPicture::RegionPictureInit(const amrex::Box &regionBox)
{
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
  int widthpad(gaPtr->PBitmapPaddedWidth(imageSizeH));
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
void RegionPicture::APDraw(int /*fromLevel*/, int /*toLevel*/) {
  if( ! pixMapCreated) {
    pixMap = XCreatePixmap(display, pictureWindow,
			   imageSizeH, imageSizeV,
			   gaPtr->PDepth());
    pixMapCreated = true;
  }  
  int invert(imageSizeV - currentScale - (regionBaseHeight * currentScale));
 
  XPutImage(display, pixMap, xgc, xImage, 0, 0, 0, 0,
	    imageSizeH, imageSizeV);
  XPutImage(display, pixMap, xgc, atiXImage, 0, 0, 0, invert,
	    atiImageSizeH, atiImageSizeV);

  XImage *xi, *atixi;
  for(auto it = timeSpanOff.begin(); it != timeSpanOff.end(); ++it) {
      const Box &b = *it;
      cout << ":::: b subRegion = " << b << "  " << subRegion << endl;
      int bSX((b.smallEnd(Amrvis::XDIR) - subRegion.smallEnd(Amrvis::XDIR)) * currentScale);
      int bBY(b.bigEnd(Amrvis::YDIR) * currentScale);
      int bLX(b.length(Amrvis::XDIR) * currentScale);
      int bLY(b.length(Amrvis::YDIR) * currentScale);
      xi    = xImageDim;
      atixi = atiXImageDim;
      XPutImage(display, pixMap, xgc, xi,
                bSX, invert - bBY,
                bSX, invert - bBY,
                bLX, bLY);
      XPutImage(display, pixMap, xgc, atixi,
                bSX, 0,
                bSX, invert,
                bLX, atiImageSizeV);
  }

  DoExposePicture();
}


// ---------------------------------------------------------------------
void RegionPicture::DoExposePicture() {
  XCopyArea(display, pixMap, pictureWindow, xgc, 0, 0,
            imageSizeH, imageSizeV, 0, 0); 

  // ---- draw the selection box
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

  int nRegions(dataServicesPtr->GetRegionsProfStats().GetMaxRNumber() + 1);

  int allDataSizeH(defaultDataSizeH);
  int allDataSizeV((nRegions + 1) * regionBaseHeight);

  FArrayBox tempSliceFab;

  calcTimeRange = dataServicesPtr->GetRegionsProfStats().MakeRegionPlt(tempSliceFab, 0,
                                          allDataSizeH, allDataSizeV / (nRegions + 1),
					  regionBoxes);

  for(int i(0); i < regionBoxes.size(); ++i) {
    for(int j(0); j < regionBoxes[i].size(); ++j) {
      regionBoxes[i][j] &= subRegion;
    }
  }
  regionsOnOff.resize(regionBoxes.size());
  for(int i(0); i < regionBoxes.size(); ++i) {
    regionsOnOff[i].resize(regionBoxes[i].size());
    for(int j(0); j < regionsOnOff[i].size(); ++j) {
      regionsOnOff[i][j] = RP_ON;
    }
  }
  tempSliceFab.shift(Amrvis::YDIR, regionBaseHeight);  // ---- for ati
  sliceFab->setVal(tempSliceFab.min(0) - 1.0);
  sliceFab->copy(tempSliceFab);
  Real minUsing(tempSliceFab.min(0)-1), maxUsing(tempSliceFab.max(0));

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

  palptr->DrawPalette(minUsing, maxUsing, "%8.2f");

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
  Real gDiff(globalMax - globalMin);
  Real oneOverGDiff;
  if((globalMax - globalMin) < FLT_MIN) {
    oneOverGDiff = 0.0;
  } else {
    oneOverGDiff = 1.0 / gDiff;
  }
  const Real *dataPoint = fab.dataPtr();

  // flips the image in Vert dir: j => datasizev-j-1
  Real dPoint;
  int paletteStart(palptr->PaletteStart());
  int paletteEnd(palptr->PaletteEnd());
  int colorSlots(palptr->ColorSlots());
  for(int j(0); j < datasizev; ++j) {
    jdsh = j * datasizeh;
    jtmp1 = (datasizev-j-1) * datasizeh;
    for(int i(0); i < datasizeh; ++i) {
      dIndex = i + jtmp1;
      dPoint = dataPoint[dIndex];
      if(dPoint < 0.0) {
        dPoint = -2.0;  // ---- set both background and ati to -2 (black in palette)
      }
      iIndex = i + jdsh;
      if(dPoint > globalMax) {  // clip
        imagedata[iIndex] = paletteEnd;
      } else if(dPoint < globalMin) {  // clip
        imagedata[iIndex] = paletteStart;
      } else {
        imagedata[iIndex] = (unsigned char)
              ((((dPoint - globalMin) * oneOverGDiff) * colorSlots) );
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
				      int datasizeh, int /*datasizev*/,
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
void RegionPicture::APChangeScale(int newScale, int /*previousScale*/) { 
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
			 imageSizeH, imageSizeV,
			 gaPtr->PDepth());
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
			 imageSizeH, imageSizeV,
			 gaPtr->PDepth());
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
void RegionPicture::SetRegionOnOff(int regionIndex, int whichRegion,
                                   int onoff)
{
  if(regionIndex < 0 || regionIndex >= regionsOnOff.size()) {
    return;
  }
  if(whichRegion < 0 || whichRegion >= regionsOnOff[regionIndex].size()) {
    return;
  }
  Box regionSpanBox(regionBoxes[regionIndex][whichRegion]);
  regionSpanBox.setSmall(Amrvis::YDIR, subRegion.smallEnd(Amrvis::YDIR));
  regionSpanBox.setBig(Amrvis::YDIR, subRegion.bigEnd(Amrvis::YDIR) - regionBaseHeight);

  cout << "regionIndex whichRegion regionBox regionSpanBox subRegion = " << regionIndex
       << "  " << whichRegion << "  " << regionBoxes[regionIndex][whichRegion]
       << "  " << regionSpanBox << "  " << subRegion << endl;

  if(onoff == RP_ON) {
    BoxList iSect(timeSpanOff.boxList());
    iSect.intersect(regionSpanBox);
    for(auto it = iSect.begin(); it != iSect.end(); ++it) {
      Box onBox(*it);
      timeSpanOff.rmBox(onBox);
    }
  }
  if(onoff == RP_OFF) {
    timeSpanOff.add(regionSpanBox);
  }

  regionsOnOff[regionIndex][whichRegion] = onoff;
  APDraw(0, 0);
}


// ---------------------------------------------------------------------
void RegionPicture::SetAllOnOff(int onoff)
{
  if(onoff == RP_ON || onoff == RP_OFF) {
    for(int i(0); i < regionsOnOff.size(); ++i) {
      for(int j(0); j < regionsOnOff[i].size(); ++j) {
        regionsOnOff[i][j] = onoff;
      }
    }
    timeSpanOff.clear();
    if(onoff == RP_OFF) {
      Box regionSpanBox(subRegion);
      regionSpanBox.setBig(Amrvis::YDIR, subRegion.bigEnd(Amrvis::YDIR) - regionBaseHeight);
      //timeSpanOff.push_back(regionSpanBox);
      timeSpanOff.add(regionSpanBox);
    }
  } else {
    cerr << "**** Error in RegionPicture::SetAllOnOff:  bad value:  " << onoff << endl;
    return;
  }
  APDraw(0, 0);
}
// ---------------------------------------------------------------------
// ---------------------------------------------------------------------


