// ---------------------------------------------------------------
// Palette.cpp
// ---------------------------------------------------------------
#include <Palette.H>
#include <GlobalUtilities.H>
#include <GraphicsAttributes.H>

#include <fcntl.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
using std::cout;
using std::cerr;
using std::endl;
using std::min;
using std::max;

Colormap Palette::systemColmap;


// -------------------------------------------------------------------
Palette::Palette(Widget &w,  int datalistlength, int width,
		 int totalwidth, int totalheight, int reservesystemcolors)
{
  bReadPalette = true;
  bTimeline = false;
  bRegions = false;
  totalColorSlots = AVGlobals::MaxPaletteIndex() + 1;
  sysccells.resize(totalColorSlots);
  transferArray.resize(totalColorSlots);
  ccells.resize(totalColorSlots);
  palPixmap = 0;
  pmin = 0.0;
  pmax = 1.0;
  defaultFormat = "%6.4f";

  gaPtr = new GraphicsAttributes(w);

  totalPalWidth = totalwidth;
  palWidth  = width;
  totalPalHeight = totalheight;
  dataList.resize(datalistlength);
  if(gaPtr->IsTrueColor()) {
    colmap = DefaultColormap(gaPtr->PDisplay(), gaPtr->PScreenNumber());
  } else {
    colmap = XCreateColormap(gaPtr->PDisplay(), gaPtr->PRoot(),
			     gaPtr->PVisual(), AllocAll);
  }
  systemColmap = DefaultColormap(gaPtr->PDisplay(), gaPtr->PScreenNumber());
  for(int ii(0); ii < totalColorSlots; ++ii) {
    sysccells[ii].pixel = ii;
  }
  XQueryColors(gaPtr->PDisplay(), systemColmap, sysccells.dataPtr(),
               totalColorSlots);
  if(gaPtr->IsTrueColor()) {
    reserveSystemColors = 0;
    colorOffset = 0;
    colorSlots   = totalColorSlots - reserveSystemColors - 3;
    bodyIndex  = 2;
    blackIndex = 1;
    whiteIndex = 0;
    paletteStart = 3;
  } else {
    reserveSystemColors = reservesystemcolors;
    colorOffset = reserveSystemColors;  // start our allocated palette here

    colorSlots   = totalColorSlots - reserveSystemColors - 3;
    bodyIndex    = colorOffset + 2;
    blackIndex   = colorOffset + 1;
    whiteIndex   = colorOffset;
    paletteStart = colorOffset + 3;  // skip 3 for black, white, and body
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
Palette::Palette(int datalistlength, int width, int totalwidth,
                 int totalheight, int reservesystemcolors)
{
  gaPtr = 0;
  bReadPalette = true;
  bTimeline = false;
  bRegions = false;
  //  bool visTrueColor = false;
  totalColorSlots = AVGlobals::MaxPaletteIndex() + 1;
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

  for(int ii(0); ii < totalColorSlots; ++ii) {
    sysccells[ii].pixel = ii;
  }
  reserveSystemColors = reservesystemcolors;
  colorOffset = reserveSystemColors;  // start our allocated palette here

  colorSlots   = totalColorSlots - reserveSystemColors - 3;
  bodyIndex    = colorOffset + 2;
  blackIndex   = colorOffset + 1;
  whiteIndex   = colorOffset;
  paletteStart = colorOffset + 3;  // skip 3 for black, white, and body
				   // the data colors start here

  remapTable = new unsigned char[totalColorSlots];  // this is faster than Array<uc>
  float sizeRatio(((float) colorSlots) / ((float) totalColorSlots));
  float mapLow(((float) paletteStart) + 0.5);
  for(int itab(0); itab < totalColorSlots; ++itab) {
    remapTable[itab] = (int) ((((float) itab) * sizeRatio) + mapLow);
  }
}  // end constructor


// -------------------------------------------------------------------
Palette::~Palette() {
  delete [] remapTable;
  delete gaPtr;
}


// -------------------------------------------------------------------
void Palette::ExposePalette() {
  XCopyArea(gaPtr->PDisplay(), palPixmap, palWindow, gaPtr->PGC(),
	    0, 0, totalPalWidth, totalPalHeight + 50, 0, 0);
}


// -------------------------------------------------------------------
void Palette::SetFormat(const string &newFormat) {
  defaultFormat = newFormat;
}


// -------------------------------------------------------------------
void Palette::SetReserveSystemColors(int reservesystemcolors) {
  reserveSystemColors = reservesystemcolors;
  DrawPalette(pmin, pmax, defaultFormat);  // use defaults
}


// -------------------------------------------------------------------
void Palette::RedrawPalette() {
  DrawPalette(pmin, pmax, defaultFormat);  // use defaults
}


// -------------------------------------------------------------------
void Palette::DrawPalette(Real palMin, Real palMax, const string &numberFormat) {
  int i, cy, palOffsetY(14);
  XWindowAttributes winAttribs;
  Display *display(gaPtr->PDisplay());
  GC gc(gaPtr->PGC());

  pmin = palMin;
  pmax = palMax;
  defaultFormat = numberFormat;
  XClearWindow(display, palWindow);

  if(palPixmap == 0) {
    palPixmap = XCreatePixmap(display, palWindow, totalPalWidth,
			      totalPalHeight + 50, gaPtr->PDepth());
  }
  XGetWindowAttributes(display, palWindow, &winAttribs);
  XSetForeground(display, gc, AVBlackPixel());
  XFillRectangle(display, palPixmap, gc, 0, 0,
                 totalPalWidth, totalPalHeight + 50);

# if (BL_SPACEDIM == 3)
  // show transfers in palette
    int transpnt, zerolinex(palWidth - 5);
    for(i = paletteStart; i < totalColorSlots; ++i) {
      cy = ((totalColorSlots - 1) - i) + palOffsetY;
      // draw transparency as black
      XSetForeground(display, gc, AVBlackPixel());
      transpnt = (int) (zerolinex * (1.0 - transferArray[i]));
      XDrawLine(display, palPixmap, gc, 0, cy, transpnt, cy);

      // draw color part of line
      XSetForeground(display, gc, makePixel(i));
      XDrawLine(display, palPixmap, gc, transpnt, cy, palWidth, cy);
    }
    
    // draw black line represening zero opacity
    XSetForeground(display, gc, AVBlackPixel());
    XDrawLine(display, palPixmap, gc, zerolinex, palOffsetY,
              zerolinex, colorSlots + palOffsetY);

#else
    for(i = paletteStart; i < totalColorSlots; ++i) {
      XSetForeground(display, gc, makePixel(i));
      cy = ((totalColorSlots - 1) - i) + palOffsetY;
      XDrawLine(display, palPixmap, gc, 0, cy, palWidth, cy);
    }
#endif

  if(bTimeline) {
    int nPalVals(mpiFuncNames.size()), count(0), cftRange(palMax - palMin);
    int nameLocation, palLocation, cftIndex, noffX(18), noffY(20);
    Array<int> palIndex(mpiFuncNames.size(), 0);
    XSetForeground(display, gc, AVWhitePixel());

    for(std::map<int, string>::const_iterator it = mpiFuncNames.begin();
        it != mpiFuncNames.end(); ++it)
    {
      cftIndex = it->first;
      string fname(it->second);
      nameLocation = (totalColorSlots - 1) -
                     (count * totalColorSlots / (nPalVals - 1)) + palOffsetY;
      palLocation  = (totalColorSlots - 1) -
                     (totalColorSlots * (cftIndex - palMin) / cftRange) + palOffsetY;
      XDrawString(display, palPixmap, gc, palWidth + noffX,
		  nameLocation, fname.c_str(), strlen(fname.c_str()));
      XDrawLine(display, palPixmap, gc,
                palWidth + 2, palLocation, palWidth + noffX - 4, nameLocation - 4);
      palIndex[count] = paletteStart + (((cftIndex - palMin) / cftRange) * colorSlots);
      ++count;
    }

    int iplo, iphi;
    for(int ip(0); ip < palIndex.size(); ++ip) {
      if(ip == 0) {
        iplo = paletteStart;
      } else {
        iplo = (palIndex[ip] + palIndex[ip - 1]) / 2;
      }
      if(ip == palIndex.size() - 1) {
        iphi = palIndex[ip];
      } else {
        iphi = (palIndex[ip] + palIndex[ip + 1]) / 2;
      }
      iphi = totalColorSlots - 1;
      for(int i(iplo); i < iphi; ++i) {
        XSetForeground(display, gc, makePixel(palIndex[ip]));
        cy = ((totalColorSlots - 1) - i) + palOffsetY;
        XDrawLine(display, palPixmap, gc, 0, cy, palWidth, cy);
      }
    }

  } else if(bRegions) {
    int nPalVals(regionNames.size()), count(0), cftRange(palMax - palMin);
    int nameLocation, palLocation, cftIndex, noffX(18), noffY(20);
    Array<int> palIndex(regionNames.size(), 0);
    XSetForeground(display, gc, AVWhitePixel());

    for(std::map<int, string>::const_iterator it = regionNames.begin();
        it != regionNames.end(); ++it)
    {
      cftIndex = it->first;
      string fname(it->second);
      nameLocation = (totalColorSlots - 1) -
                     (count * totalColorSlots / (nPalVals - 1)) + palOffsetY;
      palLocation  = (totalColorSlots - 1) -
                     (totalColorSlots * (cftIndex - palMin) / cftRange) + palOffsetY;
      XDrawString(display, palPixmap, gc, palWidth + noffX,
		  nameLocation, fname.c_str(), strlen(fname.c_str()));
      XDrawLine(display, palPixmap, gc,
                palWidth + 2, palLocation, palWidth + noffX - 4, nameLocation - 4);
      palIndex[count] = paletteStart + (((cftIndex - palMin) / cftRange) * colorSlots);
      ++count;
    }

    int iplo, iphi;
    for(int ip(0); ip < palIndex.size(); ++ip) {
      if(ip == 0) {
        iplo = paletteStart;
      } else {
        iplo = (palIndex[ip] + palIndex[ip - 1]) / 2;
      }
      if(ip == palIndex.size() - 1) {
        iphi = palIndex[ip];
      } else {
        iphi = (palIndex[ip] + palIndex[ip + 1]) / 2;
      }
      iphi = totalColorSlots - 1;
      for(int i(iplo); i < iphi; ++i) {
        XSetForeground(display, gc, makePixel(palIndex[ip]));
        cy = ((totalColorSlots - 1) - i) + palOffsetY;
        XDrawLine(display, palPixmap, gc, 0, cy, palWidth, cy);
      }
    }

  } else {
    char palString[128];
    for(i = 0; i < dataList.size(); ++i) {
      XSetForeground(display, gc, AVWhitePixel());
      dataList[i] = palMin + (dataList.size() - 1 - i) *
			     (palMax - palMin) / (dataList.size() - 1);
      if(i == 0) {
        dataList[i] = palMax;  // to avoid roundoff
      }
      sprintf(palString, numberFormat.c_str(), dataList[i]);
      XDrawString(display, palPixmap, gc, palWidth + 4,
		  (i * colorSlots / (dataList.size() - 1)) + 20,
		  palString, strlen(palString));
    }
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
  XSetWindowColormap(gaPtr->PDisplay(), newPalWindow, colmap);
}


// -------------------------------------------------------------------
void Palette::ChangeWindowPalette(const string &palName, Window newPalWindow)
{
  bReadPalette = true;
  ReadPalette(palName);
}


// -------------------------------------------------------------------
void Palette::ReadPalette(const string &palName, bool bRedraw) {
  BL_ASSERT(gaPtr != 0);
  ReadSeqPalette(palName, bRedraw);
  if(gaPtr->IsTrueColor()) {
    return;
  }
  XStoreColors(gaPtr->PDisplay(), colmap, ccells.dataPtr(), totalColorSlots);
  XStoreColors(gaPtr->PDisplay(), colmap, sysccells.dataPtr(),
               reserveSystemColors);
}


// -------------------------------------------------------------------
int Palette::ReadSeqPalette(const string &fileName, bool bRedraw) {
  const int iSeqPalSize(256);  // this must be 256 (size of sequential palettes).
  Array<int> indexArray(iSeqPalSize);
  int i, fd;

  bool bTrueColor;
  unsigned long bprgb;
  if(gaPtr == 0) {
    bTrueColor = false;
    bprgb = 8;
  } else {
    bTrueColor = gaPtr->IsTrueColor();
    bprgb = gaPtr->PBitsPerRGB();
  } 
 
  if(bReadPalette) {
    bReadPalette = false;
    rbuff.resize(iSeqPalSize);
    gbuff.resize(iSeqPalSize);
    bbuff.resize(iSeqPalSize);
    abuff.resize(iSeqPalSize);

    if((fd = open(fileName.c_str(), O_RDONLY, NULL)) < 0) {
      cout << "Can't open colormap file:  " << fileName << endl;
      for(i = 0; i < totalColorSlots; ++i) {  // make a default grayscale colormap.
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
        ccells[i].flags = DoRed | DoGreen | DoBlue;
      }
      // set low value to black
      ccells[paletteStart].red   = 0;
      ccells[paletteStart].green = 0;
      ccells[paletteStart].blue  = 0;

      //ccells[bodyIndex].red   = (unsigned short) 32000;
      ccells[bodyIndex].red   = (unsigned short) 0;
      //ccells[bodyIndex].green = (unsigned short) 32000;
      ccells[bodyIndex].green = (unsigned short) 0;
      //ccells[bodyIndex].blue  = (unsigned short) 32000;
      ccells[bodyIndex].blue  = (unsigned short) 0;
      ccells[blackIndex].red   = (unsigned short) 0;
      ccells[blackIndex].green = (unsigned short) 0;
      ccells[blackIndex].blue  = (unsigned short) 0;
      ccells[whiteIndex].red   = (unsigned short) 65535;
      ccells[whiteIndex].green = (unsigned short) 65535;
      ccells[whiteIndex].blue  = (unsigned short) 65535;

      paletteType = ALPHA;

      transferArray.resize(iSeqPalSize);
      for(int j(0); j < iSeqPalSize; ++j) {
        indexArray[j] = j; 
        //transferArray[j] = (float) j / (float) (iSeqPalSize - 1);
        rbuff[j] = j;
        gbuff[j] = j;
        bbuff[j] = j;
        abuff[j] = static_cast<unsigned char> (100.0 *
	                           ((float) j / (float) (iSeqPalSize - 1)));
      }
      //rbuff[bodyIndex] = 127;
      //gbuff[bodyIndex] = 127;
      //bbuff[bodyIndex] = 127;
      rbuff[bodyIndex] = 0;
      gbuff[bodyIndex] = 0;
      bbuff[bodyIndex] = 0;
      rbuff[blackIndex] = 0;
      gbuff[blackIndex] = 0;
      bbuff[blackIndex] = 0;
      rbuff[whiteIndex] = 255;
      gbuff[whiteIndex] = 255;
      bbuff[whiteIndex] = 255;

    } else {

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
    }

  }  // end if(bReadPalette)

  // =====================================
    bool bCCPal(false);
    if(bCCPal) {
      cout << "making a CCPal colormap" << endl;
      for(i = 0; i < totalColorSlots; ++i) {
        if(bTrueColor) {
	  ccells[i].pixel = (((rbuff[i] >> (8 - bprgb)) << 2 * bprgb)
			   | ((gbuff[i] >> (8 - bprgb)) << bprgb)
			   | ((bbuff[i] >> (8 - bprgb)) << 0) );
        } else {
	  ccells[i].pixel = i;
        }
        mcells[ccells[i].pixel] = ccells[i];
        ccells[i].red   = (unsigned short) 0 * 256;
        ccells[i].green = (unsigned short) 0 * 256;
        ccells[i].blue  = (unsigned short) 0 * 256;
        ccells[i].flags = DoRed | DoGreen | DoBlue;
      }

      int nold(0), nnew(0), npts(0);
      Real delr, delg, delb;
      Real rtmp, gtmp, btmp;
      const Real rZero(0.0), rOne(1.0);

      //int ncolor(3);
      //int nptsc[] = { 125, 125 };
      //Real ccRed[]   = { 0.2, 1.0, 1.0 };
      //Real ccGreen[] = { 0.5, 0.0, 1.0 };
      //Real ccBlue[]  = { 1.0, 0.0, 0.0 };

      //int ncolor(3);
      //int nptsc[] = { 125, 125 };
      //Real ccRed[]   = { 1.0, 1.0, 0.2 };
      //Real ccGreen[] = { 1.0, 0.0, 0.5 };
      //Real ccBlue[]  = { 0.0, 0.0, 1.0 };

      int ncolor(3);
      int nptsc[] = { 125, 130 };
      Real ccRed[]   = { 1.0, 0.8, 0.0 };
      Real ccGreen[] = { 1.0, 0.0, 0.0 };
      Real ccBlue[]  = { 0.0, 0.0, 1.0 };

      //int ncolor(13);
      //int nptsc[] = { 31,13,22,22,22,13,13,22,22,22,0,48};
      //Real ccRed[]   = { 1.,1.,1.,1.,1.,.6,0.,0.,0.,0.,0.,.8,.4};
      //Real ccGreen[] = { 1.,.6,0.,0.,0.,0.,0.,.6,1.,1.,1.,.8,.4};
      //Real ccBlue[]  = { 0.,0.,0.,.6,1.,1.,1.,1.,1.,.6,0.,.8,.4};

      nnew = 0;
      for(int nc(1); nc < ncolor; ++nc) {
        npts = nptsc[nc-1];
	nold = nnew;
	nnew = nold + npts;
        delr = ccRed[nc]   - ccRed[nc-1];
        delg = ccGreen[nc] - ccGreen[nc-1];
        delb = ccBlue[nc]  - ccBlue[nc-1];
         if(npts > 0) {
            for(int n(nold); n < nnew; ++n) {
               rtmp = ccRed[nc-1]   + delr * (n-nold+1) / (nnew-nold+1);
               gtmp = ccGreen[nc-1] + delg * (n-nold+1) / (nnew-nold+1);
               btmp = ccBlue[nc-1]  + delb * (n-nold+1) / (nnew-nold+1);

               rtmp = min(max(rtmp,rZero),rOne);
               gtmp = min(max(gtmp,rZero),rOne);
               btmp = min(max(btmp,rZero),rOne);

               ccells[n].red   = (unsigned short) (rtmp * 256);
               ccells[n].green = (unsigned short) (gtmp * 256);
               ccells[n].blue  = (unsigned short) (btmp * 256);

	    }  // 200
	 }
      }  // 300
      cout << "******* nnew = " << nnew << endl;

      // set low value to black
      //ccells[paletteStart].red   = 0;
      //ccells[paletteStart].green = 0;
      //ccells[paletteStart].blue  = 0;

      ccells[bodyIndex].red    = (unsigned short) 0;
      ccells[bodyIndex].green  = (unsigned short) 0;
      ccells[bodyIndex].blue   = (unsigned short) 0;
      ccells[blackIndex].red   = (unsigned short) 0;
      ccells[blackIndex].green = (unsigned short) 0;
      ccells[blackIndex].blue  = (unsigned short) 0;
      ccells[whiteIndex].red   = (unsigned short) 65535;
      ccells[whiteIndex].green = (unsigned short) 65535;
      ccells[whiteIndex].blue  = (unsigned short) 65535;

      paletteType = ALPHA;

      transferArray.resize(iSeqPalSize);
      for(int j(0); j < iSeqPalSize; ++j) {
        int ij(min(j+paletteStart,ccells.size()-1));
        indexArray[j] = j; 
        rbuff[j] = (char) ccells[ij].red;
        gbuff[j] = (char) ccells[ij].green;
        bbuff[j] = (char) ccells[ij].blue;
        abuff[j] = static_cast<unsigned char> (100.0 *
	                           ((float) j / (float) (iSeqPalSize - 1)));
      }
      rbuff[bodyIndex] = 0;
      gbuff[bodyIndex] = 0;
      bbuff[bodyIndex] = 0;
      rbuff[blackIndex] = 0;
      gbuff[blackIndex] = 0;
      bbuff[blackIndex] = 0;
      rbuff[whiteIndex] = 255;
      gbuff[whiteIndex] = 255;
      bbuff[whiteIndex] = 255;
    }
  // =====================================


  //rbuff[bodyIndex] = 127;
  //gbuff[bodyIndex] = 127;
  //bbuff[bodyIndex] = 127;
  rbuff[bodyIndex] = 0;
  gbuff[bodyIndex] = 0;
  bbuff[bodyIndex] = 0;
  rbuff[blackIndex] = 0;
  gbuff[blackIndex] = 0;
  bbuff[blackIndex] = 0;
  rbuff[whiteIndex] = 255;
  gbuff[whiteIndex] = 255;
  bbuff[whiteIndex] = 255;

  if(AVGlobals::LowBlack()) {   // set low value to black
    rbuff[paletteStart] = 0;
    gbuff[paletteStart] = 0;
    bbuff[paletteStart] = 0;
  }



  pixelCache.resize(iSeqPalSize);
  assert( gaPtr->PBitsPerRGB() <= 8 );
  if(bTrueColor) {
    Pixel r, g, b;
    unsigned long rs(gaPtr->PRedShift());
    unsigned long gs(gaPtr->PGreenShift());
    unsigned long bs(gaPtr->PBlueShift());
    for(i = 0; i < iSeqPalSize; ++i) {
      r = rbuff[i] >> (8 - bprgb);
      g = gbuff[i] >> (8 - bprgb);
      b = bbuff[i] >> (8 - bprgb);
      pixelCache[i] = ( (r << rs) | (g << gs) | (b << bs) );
    }
  } else {
    for(i = 0; i < iSeqPalSize; ++i) {
      pixelCache[i] =  Pixel(i);
    }
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

  // set Transfer function here
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

  if(bRedraw) {
    RedrawPalette();
  }

  return(1);

}  // end ReadSeqPalette()


// -------------------------------------------------------------------
XImage *Palette::GetPictureXImage() {
  return (XGetImage(gaPtr->PDisplay(), palPixmap, 0, 0, totalPalWidth,
                    totalPalHeight, AllPlanes, ZPixmap));
}


// -------------------------------------------------------------------
int Palette::BodyIndex() const {
  return bodyIndex;
}


// -------------------------------------------------------------------
Pixel Palette::AVBlackPixel() const {
  if(gaPtr->IsTrueColor()) {
    return BlackPixel(gaPtr->PDisplay(), gaPtr->PScreenNumber());
  } else {
    return blackIndex;
  }
}


// -------------------------------------------------------------------
Pixel Palette::AVWhitePixel() const {
  if(gaPtr->IsTrueColor()) {
    return WhitePixel(gaPtr->PDisplay(), gaPtr->PScreenNumber());
  } else {
    return whiteIndex;
  }
}


// -------------------------------------------------------------------
unsigned int Palette::SafePaletteIndex(unsigned int atlevel, int mlev) const
{
  // return a number in [PaletteStart(), PaletteEnd()]
  float flev(atlevel), fmlev(mlev), fcs(colorSlots - 10);
  fcs = fcs * ((fmlev - flev) / fmlev);

  unsigned int cslots((int)(fcs));
  cslots = colorSlots - cslots - 1;

  unsigned int indexRet(paletteStart + cslots);
  unsigned int pSt(PaletteStart()), pEnd(PaletteEnd());
  indexRet = std::max(pSt, std::min(indexRet, pEnd));
  return indexRet;
}


// -------------------------------------------------------------------
Pixel Palette::pixelate(int i) const {
  if(i < 0) {
    return AVBlackPixel();
  } else if( i > (totalColorSlots - 1) ) {
    return AVWhitePixel();
  } else {
    return ccells[i].pixel;
  }
}


// -------------------------------------------------------------------
/*
Pixel Palette::makePixel(unsigned char ind) const {
  if(gaPtr->IsTrueColor()) {
    assert( gaPtr->PBitsPerRGB() <= 8 );
    Pixel r = rbuff[ind] >> (8 - gaPtr->PBitsPerRGB());
    Pixel g = gbuff[ind] >> (8 - gaPtr->PBitsPerRGB());
    Pixel b = bbuff[ind] >> (8 - gaPtr->PBitsPerRGB());
    return(( r << gaPtr->PRedShift())
	     | (g << gaPtr->PGreenShift())
	     | (b << gaPtr->PBlueShift()));
  } else {
    return Pixel(ind);
  }
}
*/


// -------------------------------------------------------------------
void Palette::unpixelate(Pixel index, unsigned char &r,
			 unsigned char &g, unsigned char &b) const
{
  if(gaPtr->IsTrueColor()) {
//#define AV_UNPIXV1
#ifdef AV_UNPIXV1
    map<Pixel, XColor>::const_iterator mi = mcells.find(index);
    if(mi != mcells.end()) {
      r = mi->second.red   >> 8;
      g = mi->second.green >> 8;
      b = mi->second.blue  >> 8;
      return;
    }
    r = (index&gaPtr->PRedMask()) >> gaPtr->PRedShift();
    g = (index&gaPtr->PGreenMask()) >> gaPtr->PGreenShift();
    b = (index&gaPtr->PBlueMask()) >> gaPtr->PBlueShift();
#else
    //cout << "bad index = " << index << endl;
    r = (index&gaPtr->PRedMask()) >> gaPtr->PRedShift();
    g = (index&gaPtr->PGreenMask()) >> gaPtr->PGreenShift();
    b = (index&gaPtr->PBlueMask()) >> gaPtr->PBlueShift();
#endif
  } else {
    int vIndex = std::max(0, std::min(static_cast<int> (index),
                                      (totalColorSlots - 1)));
    r = ccells[vIndex].red   >> 8;
    g = ccells[vIndex].green >> 8;
    b = ccells[vIndex].blue  >> 8;
  }
}


// -------------------------------------------------------------------
void Palette::SetMPIFuncNames(const map<int, string> &mpifnames) {
  for(std::map<int, string>::const_iterator it = mpifnames.begin();
      it != mpifnames.end(); ++it)
  {
    mpiFuncNames.insert(*it);
  }
}


// -------------------------------------------------------------------
void Palette::SetRegionNames(const map<int, string> &regnames) {
  for(std::map<int, string>::const_iterator it = regnames.begin();
      it != regnames.end(); ++it)
  {
    regionNames.insert(*it);
  }
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------



