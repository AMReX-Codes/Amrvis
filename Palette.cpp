// -------------------------------------------------------------------
// Palette.cpp
// -------------------------------------------------------------------
#include "Palette.H"
#include "GlobalUtilities.H"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

Colormap Palette::systemColmap;


// -------------------------------------------------------------------
Palette::Palette(Widget &w,  int datalistlength, int width,
		 int totalwidth, int totalheight, int reservesystemcolors)
{
  totalColorSlots = MaxPaletteIndex() + 1;
  sysccells.resize(totalColorSlots);
  transY.resize(totalColorSlots);
  ccells.resize(totalColorSlots);
  palPixmap = NULL;
  pmin = 0.0;
  pmax = 1.0;
  defaultFormat = "%6.4f";

  display = XtDisplay(w);
  root = RootWindow(display, DefaultScreen(display));
  screen = XtScreen(w);
  screenNumber = DefaultScreen(display);
  int status = XMatchVisualInfo(display, DefaultScreen(display),
                                8, PseudoColor, &visualInfo);  // fills visualInfo

  visual = visualInfo.visual;
  gc = screen->default_gc;

  totalPalWidth = totalwidth;
  palWidth  = width;
  totalPalHeight = totalheight;
  dataList.resize(datalistlength);
  colmap = XCreateColormap(display, root, visual, AllocAll);

  transSet = false;

  systemColmap = DefaultColormap(display, screenNumber);
  for(int ii = 0; ii < totalColorSlots; ++ii) {
    sysccells[ii].pixel = ii;
  }
  XQueryColors(display, systemColmap, sysccells.dataPtr(), totalColorSlots);
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
}


// -------------------------------------------------------------------
void Palette::ExposePalette() {
    XCopyArea(display, palPixmap, palWindow, gc,
	    0, 0, totalPalWidth, totalPalHeight+50, 0, 0);
}


// -------------------------------------------------------------------
void Palette::SetFormat(const aString &newFormat) {
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
void Palette::Draw(Real palMin, Real palMax, const aString &numberFormat) {
  int i, cy;
  XWindowAttributes winAttribs;

  pmin = palMin;
  pmax = palMax;
  defaultFormat = numberFormat;
  XClearWindow(display, palWindow);

  if(palPixmap == 0) {
    palPixmap = XCreatePixmap(display, palWindow, totalPalWidth,
			      totalPalHeight + 50, 8);
  }
  XGetWindowAttributes(display, palWindow, &winAttribs);

  XSetForeground(display, gc, blackIndex);
  XFillRectangle(display, palPixmap, gc, 0, 0, totalPalWidth, totalPalHeight + 50);

  if(transSet) {    // show transfers in palette
    int transpnt, zerolinex = palWidth - 5;
    for(i = paletteStart; i < totalColorSlots; i++) {
      cy = ((totalColorSlots - 1) - i) + 14;
      // draw transparency as black
      XSetForeground(display, gc, ccells[blackIndex].pixel);
      transpnt = (int) (zerolinex*(1.0-transY[i]));
      XDrawLine(display, palPixmap, gc, 0, cy, transpnt, cy);

      // draw color part of line
      XSetForeground(display, gc, ccells[i].pixel);
      XDrawLine(display, palPixmap, gc, transpnt, cy, palWidth, cy);
    }
    
    // draw black line represening zero opacity
    XSetForeground(display, gc, ccells[blackIndex].pixel);
    XDrawLine(display, palPixmap, gc, zerolinex, 14, zerolinex, colorSlots + 14);

  } else {
    for(i = paletteStart; i < totalColorSlots; i++) {
      XSetForeground(display, gc, ccells[i].pixel);
      cy = ((totalColorSlots - 1) - i) + 14;
      XDrawLine(display, palPixmap, gc, 0, cy, palWidth, cy);
    }
  }

  char palString[64];
  for(i = 0; i < dataList.length(); ++i) {
    XSetForeground(display, gc, whiteIndex);
    dataList[i] = palMin + (dataList.length()-1-i) *
			   (palMax - palMin)/(dataList.length() - 1);
    if(i == 0) {
      dataList[i] = palMax;  // to avoid roundoff
    }
    sprintf(palString, numberFormat.c_str(), dataList[i]);
    XDrawString(display, palPixmap, gc, palWidth + 4,
		(i * colorSlots / (dataList.length() - 1)) + 20,
		palString, strlen(palString));
  }
  ExposePalette();
}  // end Palette::Draw.


// -------------------------------------------------------------------
void Palette::SetTransfers(const Array<int> &transx, const Array<float> &transy) {
  // interpolate between points
  assert(transx.length() == transy.length());
  int ntranspts(transx.length());
  int n, leftx, rightx;
  float lefty, righty, m, x;
  transSet = true;
  int ntrans(0);  // where we are in transx

  assert(transx[0] == 0);

  for(n=0; n < totalColorSlots; ++n) {

    if((ntrans+1) > ntranspts) {
	cerr << (ntrans+1) << " > " << ntranspts << endl;
    }

    if(transx[ntrans] == n) {
      if(n != (totalColorSlots - 1)) {
        leftx = transx[ntrans];
        lefty = transy[ntrans];
        rightx = transx[ntrans+1];
        righty = transy[ntrans+1];
      }
      transY[n] = transy[ntrans];
      ++ntrans;
    } else {
      m = (righty - lefty) / (rightx - leftx);
      x = (float) (n - leftx);
      transY[n] = m*x + lefty;
    }
  }
  Redraw();  
}


// -------------------------------------------------------------------
void Palette::SetWindow(Window drawPaletteHere) {
  palWindow = drawPaletteHere;
}


// -------------------------------------------------------------------
void Palette::SetWindowPalette(const aString &palName, Window newPalWindow) {
  ReadSeqPalette(palName);
  XStoreColors(display, colmap, ccells.dataPtr(), totalColorSlots);
  XStoreColors(display, colmap, sysccells.dataPtr(), reserveSystemColors);
  XSetWindowColormap(display, newPalWindow, colmap);
}


// -------------------------------------------------------------------
void Palette::ChangeWindowPalette(const aString &palName, Window newPalWindow) {
  ReadSeqPalette(palName);
  XStoreColors(display, colmap, ccells.dataPtr(), totalColorSlots);
  XStoreColors(display, colmap, sysccells.dataPtr(), reserveSystemColors);
}


// -------------------------------------------------------------------
int Palette::ReadSeqPalette(const aString &fileName) {
  int iSeqPalSize(256);  // this must be 256 (size of sequential palettes).
  Array<unsigned char> rbuff(iSeqPalSize);
  Array<unsigned char> gbuff(iSeqPalSize);
  Array<unsigned char> bbuff(iSeqPalSize);
  int	i, fd;		/* file descriptor */

  if((fd = open(fileName.c_str(), O_RDONLY, NULL)) < 0) {
    cout << "Can't open colormap file:  " << fileName << endl;
    for(i = 0; i < totalColorSlots; i++) {    // make a default grayscale colormap.
      ccells[i].pixel = i;
      ccells[i].red   = (unsigned short) i*256;
      ccells[i].green = (unsigned short) i*256;
      ccells[i].blue  = (unsigned short) i*256;
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

    return(1);
  }

  if(read(fd, rbuff.dataPtr(), iSeqPalSize) != iSeqPalSize) {
    cout << "palette is not a seq colormap." << endl;
    return(NULL);
  }
  if(read(fd, gbuff.dataPtr(), iSeqPalSize) != iSeqPalSize) {
    cout << "file is not a seq colormap." << endl;
    return(NULL);
  }
  if(read(fd, bbuff.dataPtr(), iSeqPalSize) != iSeqPalSize) {
    cout << "palette is not a seq colormap." << endl;
    return(NULL);
  }
  (void) close(fd);

  rbuff[blackIndex] = 0;   gbuff[blackIndex] = 0;   bbuff[blackIndex] = 0;
  rbuff[whiteIndex] = 255; gbuff[whiteIndex] = 255; bbuff[whiteIndex] = 255;

  if(LowBlack()) {   // set low value to black
    rbuff[paletteStart] = 0;
    gbuff[paletteStart] = 0;
    bbuff[paletteStart] = 0;
  }

  for(i = 0; i < totalColorSlots; ++i) {
    ccells[i].pixel = i;
    ccells[i].red   = (unsigned short) rbuff[i] * 256;
    ccells[i].green = (unsigned short) gbuff[i] * 256;
    ccells[i].blue  = (unsigned short) bbuff[i] * 256;
    ccells[i].flags = DoRed|DoGreen|DoBlue;
  }
  return(1);

}  // end ReadSeqPalette()


// -------------------------------------------------------------------
XImage *Palette::GetPictureXImage() {
  return (XGetImage(display, palPixmap, 0, 0,
                totalPalWidth, totalPalHeight, AllPlanes, ZPixmap));
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
