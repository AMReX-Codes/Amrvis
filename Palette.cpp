
//
// $Id: Palette.cpp,v 1.39 2001-10-05 23:01:32 vince Exp $
//

// ---------------------------------------------------------------
// Palette.cpp
// ---------------------------------------------------------------
#include "Palette.H"
#include "GlobalUtilities.H"
#include "GraphicsAttributes.H"

#include <fcntl.h>
#include <unistd.h>

#ifdef BL_USE_NEW_HFILES
#include <cassert>
#include <cstdio>
using std::cout;
using std::cerr;
using std::endl;
using std::min;
using std::max;
#else
#include <stdio.h>
#include <assert.h>
#endif

Colormap Palette::systemColmap;


// -------------------------------------------------------------------
Palette::Palette(Widget &w,  int datalistlength, int width,
		 int totalwidth, int totalheight, int reservesystemcolors)
{
  totalColorSlots = MaxPaletteIndex() + 1;
  sysccells.resize(totalColorSlots);
  transferArray.resize(totalColorSlots);
  ccells.resize(totalColorSlots);
  palPixmap = 0;
  pmin = 0.0;
  pmax = 1.0;
  defaultFormat = "%6.4f";

  GAptr = new GraphicsAttributes(w);

/*
  display = XtDisplay(w);
  root = RootWindow(display, DefaultScreen(display));
  screen = XtScreen(w);
  screenNumber = DefaultScreen(display);
  int status = XMatchVisualInfo(display, DefaultScreen(display),
                                8, PseudoColor, &visualInfo);
                               // fills visualInfo
  if( status != 0 ) {
      visual = visualInfo.visual;
      bits_per_rgb = visualInfo.bits_per_rgb;
      palDepth = 8;
  } else {
      int status = XMatchVisualInfo(display, DefaultScreen(display),
				    DefaultDepth(display, screenNumber),
                                    TrueColor, &visualInfo);
      if( status != 0 ) {
	  isTrueColor=true;
	  visual = visualInfo.visual;
	  bits_per_rgb = visualInfo.bits_per_rgb;
	  palDepth = DefaultDepth(display, screenNumber);
      } else {
	cerr << "Error: bad XMatchVisualInfo: no PseudoColor Visual"
             << endl;
	exit(1);
      }
    }
  //cout << endl;
  //cout << "_in Palette: screen DefScreen(display) visualInfo.screen = "
       //<< screen << "  " << DefaultScreen(display)
       //<< "  " << visualInfo.screen
       //<< endl;
  //cout << endl;
  gc = screen->default_gc;
*/

  totalPalWidth = totalwidth;
  palWidth  = width;
  totalPalHeight = totalheight;
  dataList.resize(datalistlength);
  if(GAptr->IsTrueColor()) {
    colmap = DefaultColormap(GAptr->PDisplay(), GAptr->PScreenNumber());
  } else {
    colmap = XCreateColormap(GAptr->PDisplay(), GAptr->PRoot(),
			     GAptr->PVisual(), AllocAll);
  }
  transSet = false;
  systemColmap = DefaultColormap(GAptr->PDisplay(), GAptr->PScreenNumber());
  for(int ii(0); ii < totalColorSlots; ++ii) {
    sysccells[ii].pixel = ii;
  }
  XQueryColors(GAptr->PDisplay(), systemColmap, sysccells.dataPtr(), totalColorSlots);
  if(GAptr->IsTrueColor()) {
    reserveSystemColors = 0;
    colorOffset = 0;
    colorSlots = 253;
    blackIndex = 1;
    whiteIndex = 0;
    paletteStart = 2;
  } else {
    reserveSystemColors = reservesystemcolors;
    colorOffset = reserveSystemColors;  // start our allocated palette here

    colorSlots   = totalColorSlots - reserveSystemColors - 2;
    blackIndex   = colorOffset + 1;
    whiteIndex   = colorOffset;
    paletteStart = colorOffset + 2;  // skip 2 for black and white
                                     // the data colors start here
  }
  
  remapTable = new unsigned char[totalColorSlots];  // this is faster than Array<uc>
  float sizeRatio(((float) colorSlots) / ((float) totalColorSlots));
  float mapLow(((float) paletteStart) + 0.5);
  for(int itab(0); itab < totalColorSlots; ++itab) {
    remapTable[itab] = (int) ((((float) itab) * sizeRatio) + mapLow);
  }
}  // end constructor


// -------------------------------------------------------------------
Palette::Palette(int datalistlength, int width,
		 int totalwidth, int totalheight, int reservesystemcolors)
{
  GAptr = 0;
  //  bool visTrueColor = false;
  totalColorSlots = MaxPaletteIndex() + 1;
  sysccells.resize(totalColorSlots);
  transferArray.resize(totalColorSlots);
  ccells.resize(totalColorSlots);
  palPixmap = 0;
  pmin = 0.0;
  pmax = 1.0;
  defaultFormat = "%6.4f";

  totalPalWidth = totalwidth;
  palWidth  = width;
  totalPalHeight = totalheight;
  dataList.resize(datalistlength);

  transSet = false;

  for(int ii = 0; ii < totalColorSlots; ++ii) {
    sysccells[ii].pixel = ii;
  }
  reserveSystemColors = reservesystemcolors;
  colorOffset = reserveSystemColors;  // start our allocated palette here

  colorSlots   = totalColorSlots - reserveSystemColors - 2;
  blackIndex   = colorOffset + 1;
  whiteIndex   = colorOffset;
  paletteStart = colorOffset + 2;  // skip 2 for black and white
				   // the data colors start here

  remapTable = new unsigned char[totalColorSlots];  // this is faster than Array<uc>
  float sizeRatio(((float) colorSlots) / ((float) totalColorSlots));
  float mapLow(((float) paletteStart) + 0.5);
  for(int itab = 0; itab < totalColorSlots; ++itab) {
    remapTable[itab] = (int) ((((float) itab) * sizeRatio) + mapLow);
  }
}  // end constructor


// -------------------------------------------------------------------
Palette::~Palette() {
  delete [] remapTable;
  delete GAptr;
}


// -------------------------------------------------------------------
void Palette::ExposePalette() {
    XCopyArea(GAptr->PDisplay(), palPixmap, palWindow, GAptr->PGC(),
	    0, 0, totalPalWidth, totalPalHeight+50, 0, 0);
}


// -------------------------------------------------------------------
void Palette::SetFormat(const string &newFormat) {
  defaultFormat = newFormat;
}


// -------------------------------------------------------------------
void Palette::SetReserveSystemColors(int reservesystemcolors) {
  reserveSystemColors = reservesystemcolors;
  Draw(pmin, pmax, defaultFormat);  // use defaults
}


// -------------------------------------------------------------------
void Palette::Redraw() {
  Draw(pmin, pmax, defaultFormat);  // use defaults
}


// -------------------------------------------------------------------
void Palette::Draw(Real palMin, Real palMax, const string &numberFormat) {
  int i, cy;
  XWindowAttributes winAttribs;

  pmin = palMin;
  pmax = palMax;
  defaultFormat = numberFormat;
  XClearWindow(GAptr->PDisplay(), palWindow);

  if(palPixmap == 0) {
    palPixmap = XCreatePixmap(GAptr->PDisplay(), palWindow, totalPalWidth,
			      totalPalHeight + 50, GAptr->PDepth());
  }
  XGetWindowAttributes(GAptr->PDisplay(), palWindow, &winAttribs);
  XSetForeground(GAptr->PDisplay(), GAptr->PGC(), BlackIndex());
// ERROR here for 24 bit color pc
  XFillRectangle(GAptr->PDisplay(), palPixmap, GAptr->PGC(), 0, 0, totalPalWidth, totalPalHeight + 50);

  if(transSet) {    // show transfers in palette
    int transpnt, zerolinex = palWidth - 5;
    for(i = paletteStart; i < totalColorSlots; ++i) {
      cy = ((totalColorSlots - 1) - i) + 14;
      // draw transparency as black
      // FIXME:
      XSetForeground(GAptr->PDisplay(), GAptr->PGC(), ccells[blackIndex].pixel);
      transpnt = (int) (zerolinex*(1.0-transferArray[i]));
      XDrawLine(GAptr->PDisplay(), palPixmap, GAptr->PGC(), 0, cy, transpnt, cy);

      // draw color part of line
      // FIXME:
      XSetForeground(GAptr->PDisplay(), GAptr->PGC(), ccells[i].pixel);
      XDrawLine(GAptr->PDisplay(), palPixmap, GAptr->PGC(), transpnt, cy, palWidth, cy);
    }
    
    // draw black line represening zero opacity
      // FIXME:
    XSetForeground(GAptr->PDisplay(), GAptr->PGC(), ccells[blackIndex].pixel);
    XDrawLine(GAptr->PDisplay(), palPixmap, GAptr->PGC(), zerolinex, 14, zerolinex, colorSlots + 14);

  } else {
    for(i = paletteStart; i < totalColorSlots; ++i) {
      XSetForeground(GAptr->PDisplay(), GAptr->PGC(), ccells[i].pixel);
      cy = ((totalColorSlots - 1) - i) + 14;
      XDrawLine(GAptr->PDisplay(), palPixmap, GAptr->PGC(), 0, cy, palWidth, cy);
    }
  }

  char palString[64];
  for(i = 0; i < dataList.size(); ++i) {
    XSetForeground(GAptr->PDisplay(), GAptr->PGC(), WhiteIndex());
    dataList[i] = palMin + (dataList.size()-1-i) *
			   (palMax - palMin)/(dataList.size() - 1);
    if(i == 0) {
      dataList[i] = palMax;  // to avoid roundoff
    }
    sprintf(palString, numberFormat.c_str(), dataList[i]);
    XDrawString(GAptr->PDisplay(), palPixmap, GAptr->PGC(), palWidth + 4,
		(i * colorSlots / (dataList.size() - 1)) + 20,
		palString, strlen(palString));
  }
  ExposePalette();
}  // end Palette::Draw.


// -------------------------------------------------------------------
void Palette::SetWindow(Window drawPaletteHere) {
  palWindow = drawPaletteHere;
}


// -------------------------------------------------------------------
void Palette::SetWindowPalette(const string &palName, Window newPalWindow,
			       bool bRedraw)
{
  ReadPalette(palName, bRedraw);
  XSetWindowColormap(GAptr->PDisplay(), newPalWindow, colmap);
}


// -------------------------------------------------------------------
void Palette::ChangeWindowPalette(const string &palName, Window newPalWindow) {
  ReadPalette(palName);
}


// -------------------------------------------------------------------
void Palette::ReadPalette(const string &palName, bool bRedraw) {
  BL_ASSERT(GAptr != 0);
  ReadSeqPalette(palName, bRedraw);
  if(GAptr->IsTrueColor()) {
    return;
  }
  XStoreColors(GAptr->PDisplay(), colmap, ccells.dataPtr(), totalColorSlots);
  XStoreColors(GAptr->PDisplay(), colmap, sysccells.dataPtr(), reserveSystemColors);
}


// -------------------------------------------------------------------
int Palette::ReadSeqPalette(const string &fileName, bool bRedraw) {
  int iSeqPalSize(256);  // this must be 256 (size of sequential palettes).
  rbuff.resize(iSeqPalSize);
  gbuff.resize(iSeqPalSize);
  bbuff.resize(iSeqPalSize);
  abuff.resize(iSeqPalSize);
  Array<int> indexArray(iSeqPalSize);
  int i, fd;

  //BL_ASSERT(GAptr != 0);

  bool bTrueColor;
  unsigned long bprgb;
  if(GAptr == 0) {
    bTrueColor = false;
    bprgb = 8;
  } else {
    bTrueColor = GAptr->IsTrueColor();
    bprgb = GAptr->PBitsPerRGB();
  } 
 

  if((fd = open(fileName.c_str(), O_RDONLY, NULL)) < 0) {
    cout << "Can't open colormap file:  " << fileName << endl;
    for(i = 0; i < totalColorSlots; ++i) {    // make a default grayscale colormap.
      if(bTrueColor) {
	// FIXME: not 24 bit!
	ccells[i].pixel = (((rbuff[i] >> (8 - bprgb)) << 2 * bprgb)
			 | ((gbuff[i] >> (8 - bprgb)) << bprgb)
			 | ((bbuff[i] >> (8 - bprgb)) << 0) );
      } else {
	ccells[i].pixel = i;
      }
      mcells[ccells[i].pixel] = ccells[i];
      ccells[i].red   = (unsigned short) i * 256;
      ccells[i].green = (unsigned short) i * 256;
      ccells[i].blue  = (unsigned short) i * 256;
      ccells[i].flags = DoRed|DoGreen|DoBlue;
    }
    // set low value to black
    ccells[paletteStart].red   = 0;
    ccells[paletteStart].green = 0;
    ccells[paletteStart].blue  = 0;

    ccells[blackIndex].red   = (unsigned short) 0;
    ccells[blackIndex].green = (unsigned short) 0;
    ccells[blackIndex].blue  = (unsigned short) 0;
    ccells[whiteIndex].red   = (unsigned short) 65535;
    ccells[whiteIndex].green = (unsigned short) 65535;
    ccells[whiteIndex].blue  = (unsigned short) 65535;

    paletteType = NON_ALPHA;
    transferArray.resize(iSeqPalSize);
    for(int j(0); j < iSeqPalSize; ++j) {
      indexArray[j] = j; 
      transferArray[j] = (float) j / (float)(iSeqPalSize-1);
      rbuff[j] = j;
      gbuff[j] = j;
      bbuff[j] = j;
      abuff[j] = 128;
    }
    rbuff[blackIndex] = 0;
    gbuff[blackIndex] = 0;
    bbuff[blackIndex] = 0;
    rbuff[whiteIndex] = 255;
    gbuff[whiteIndex] = 255;
    bbuff[whiteIndex] = 255;
    return(1);
  }

  if(read(fd, rbuff.dataPtr(), iSeqPalSize) != iSeqPalSize) {
    cout << "palette is not a seq colormap." << endl;
    return(0);
  }
  if(read(fd, gbuff.dataPtr(), iSeqPalSize) != iSeqPalSize) {
    cout << "file is not a seq colormap." << endl;
    return(0);
  }
  if(read(fd, bbuff.dataPtr(), iSeqPalSize) != iSeqPalSize) {
    cout << "palette is not a seq colormap." << endl;
    return(0);
  }
  if(read(fd, abuff.dataPtr(), iSeqPalSize) != iSeqPalSize) {
    if(BL_SPACEDIM == 3) {
      cout << "Palette does not have an alpha component:  using the default." 
           << endl;
    }
    paletteType = NON_ALPHA;
  } else {
    paletteType = ALPHA;
  }

  (void) close(fd);

  rbuff[blackIndex] = 0;
  gbuff[blackIndex] = 0;
  bbuff[blackIndex] = 0;
  rbuff[whiteIndex] = 255;
  gbuff[whiteIndex] = 255;
  bbuff[whiteIndex] = 255;

  if(LowBlack()) {   // set low value to black
    rbuff[paletteStart] = 0;
    gbuff[paletteStart] = 0;
    bbuff[paletteStart] = 0;
    abuff[paletteStart] = 0;
  }

  for(i = 0; i < totalColorSlots; ++i) {
    if(bTrueColor) {
      // FIXME: not 24 bit!
      ccells[i].pixel = (((rbuff[i] >> (8 - bprgb)) << 2 * bprgb)
		       | ((gbuff[i] >> (8 - bprgb)) << bprgb)
		       | ((bbuff[i] >> (8 - bprgb)) << 0));
    } else {
      ccells[i].pixel = i;
    }
    mcells[ccells[i].pixel] = ccells[i];
    ccells[i].red   = (unsigned short) rbuff[i] * 256;
    ccells[i].green = (unsigned short) gbuff[i] * 256;
    ccells[i].blue  = (unsigned short) bbuff[i] * 256;
    ccells[i].flags = DoRed | DoGreen | DoBlue;
  }

  // set Transfer function here  NOTE:  doesn't call
  transferArray.resize(iSeqPalSize);
  if(paletteType == NON_ALPHA) {
    for(int j(0); j < iSeqPalSize; ++j) {
      indexArray[j] = j; 
      transferArray[j] = (float) j / (float) (iSeqPalSize-1);
    }
  } else if(paletteType == ALPHA) {
    for(int j(0); j < iSeqPalSize; ++j) {
      indexArray[j] = j; 
      int tmp = (unsigned short) abuff[j];
      transferArray[j] = (float) tmp / 100.0;
    }
  }
  if(BL_SPACEDIM == 3) {
    transSet = true;
  }

  if(bRedraw) {
    Redraw();
  }

  return(1);

}  // end ReadSeqPalette()


// -------------------------------------------------------------------
XImage *Palette::GetPictureXImage() {
  return (XGetImage(GAptr->PDisplay(), palPixmap, 0, 0,
                totalPalWidth, totalPalHeight, AllPlanes, ZPixmap));
}


// -------------------------------------------------------------------
Pixel Palette::BlackIndex() const {
  if(GAptr->IsTrueColor()) {
    return BlackPixel(GAptr->PDisplay(), GAptr->PScreenNumber());
  } else {
    return blackIndex;
  }
}


// -------------------------------------------------------------------
Pixel Palette::WhiteIndex() const {
  if(GAptr->IsTrueColor()) {
    return WhitePixel(GAptr->PDisplay(), GAptr->PScreenNumber());
  } else {
    return whiteIndex;
  }
}


// -------------------------------------------------------------------
Pixel Palette::pixelate(int i) const {
  if(i < 0) {
    return BlackIndex();
  } else if ( i > 255 ) {
    return WhiteIndex();
  } else {
    return ccells[i].pixel;
  }
}


// -------------------------------------------------------------------
Pixel Palette::makePixel(unsigned char ind) const {
  if(GAptr->IsTrueColor()) {
    assert( GAptr->PBitsPerRGB() <= 8 );
    Pixel r = rbuff[ind] >> (8-GAptr->PBitsPerRGB());
    Pixel g = gbuff[ind] >> (8-GAptr->PBitsPerRGB());
    Pixel b = bbuff[ind] >> (8-GAptr->PBitsPerRGB());
    return((  r << GAptr->PRedShift()   )
	      | (g << GAptr->PGreenShift() )
	      | (b << GAptr->PBlueShift()  ));
  } else {
    return Pixel(ind);
  }
}


// -------------------------------------------------------------------
void Palette::unpixelate(Pixel index, unsigned char& r,
			 unsigned char& g, unsigned char& b) const
{
  if(GAptr->IsTrueColor()) {
    map<Pixel, XColor>::const_iterator mi = mcells.find(index);
    if(mi != mcells.end()) {
      r = mi->second.red   >> 8;
      g = mi->second.green >> 8;
      b = mi->second.blue  >> 8;
      return;
    }
    cout << "Hmm, not found index = " << index << endl;
    r = (index&GAptr->PRedMask()) >> GAptr->PRedShift();
    g = (index&GAptr->PGreenMask()) >> GAptr->PGreenShift();
    b = (index&GAptr->PBlueMask()) >> GAptr->PBlueShift();
  } else {
    r = ccells[index].red   >> 8;
    g = ccells[index].green >> 8;
    b = ccells[index].blue  >> 8;
  }
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
