// ---------------------------------------------------------------
// Output.cpp
// ---------------------------------------------------------------
#include <AMReX_ParallelDescriptor.H>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <Output.H>
#include <GraphicsAttributes.H>

#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

using std::hex;
using std::dec;
using std::ofstream;
using std::cout;
using std::cerr;
using std::endl;

using namespace amrex;

IMAGE *iopen(const char *file, unsigned int type, unsigned int dim,
             unsigned int xsize, unsigned int ysize, unsigned int zsize);
int putrow(IMAGE *image, unsigned short *buffer, unsigned int y, unsigned int z);
void cvtshorts( unsigned short buffer[], long n);
int img_write(IMAGE *image, const void *buffer, long count);
int img_seek(IMAGE *image, unsigned int y, unsigned int z);
int img_optseek(IMAGE *image, unsigned long offset);
int iclose(IMAGE *image);
void cvtlongs(long buffer[], long n);
void cvtimage(long *buffer);



// -------------------------------------------------------------------
void WriteNewPSFile(const char *filename, XImage *image,
		 int imagesizehoriz, int imagesizevert,
		 const Palette &palette, const AmrData &amrdata,
		 const int minlev, const int maxlev,
		 Vector< Vector<GridBoxes> > &gridBoxes)
{
  amrex::ignore_unused(amrdata);

  clock_t time0 = clock();
  unsigned char r, g, b;

  Vector<Real> rgb(256);
  for(int ii(0); ii < rgb.size(); ++ii) {
    rgb[ii] = (Real) ii / 255.0;
  }

  char newfilename[BUFSIZ];
  sprintf(newfilename, "%s.new.ps", filename);
  ofstream fout(newfilename);
  if( ! fout) {
    cerr << "*** Error:  cannot create file:  " << newfilename << endl;
    return;
  }
  fout.precision(3);
  fout << "%!PS-Adobe-2.0" << '\n';
  fout <<  "%%BoundingBox: 0 0 " << imagesizehoriz-1 << " "
       << imagesizevert-1 << '\n';
  fout << "gsave" << '\n';
  fout << "/d {moveto 1 0 rlineto 0 -1 rlineto -1 0 rlineto closepath fill} def\n";
  fout << "/s {setrgbcolor newpath} def\n";

  for(int ipal(palette.PaletteStart()); ipal <= palette.PaletteEnd(); ++ipal) {
    palette.unpixelate(ipal, r, g, b);
    fout << "/c" << ipal << " {"
	 << rgb[(int) r] << " " << rgb[(int) g] << " " << rgb[(int) b]
         << " s} def\n";
  }

  fout << "\n1 setlinewidth\n";
  fout << "\n% box color defs\n";
  for(int iblev(minlev); iblev <= maxlev; ++iblev) {
    if(iblev == minlev) {
      fout << "/boxcolor" << iblev << " {1.0 1.0 1.0} def  % white\n";
    } else {
      palette.unpixelate(palette.SafePaletteIndex(iblev), r, g, b);
      fout << "/boxcolor" << iblev << " {"
           << rgb[(int) r] << " " << rgb[(int) g] << " " << rgb[(int) b]
           << "} def\n";
    }
  }
  fout << "\n";

  // draw the pixels
  for(int j(0); j < imagesizevert; ++j) {
    int jflip(imagesizevert - (j + 1));
    for(int i(0); i < imagesizehoriz; ++i) {
      fout << "c" << XGetPixel(image, i, j) << " "
	   << i << " " << jflip << " d\n";
    }
  }

  // draw the boxes
  fout << "\n\n% draw the boxes\n";
  for(int ilev(minlev); ilev <= maxlev; ++ilev) {
    fout << "\n% boxes for level " << ilev << "\n";
    fout << "boxcolor" << ilev << " setrgbcolor\n\n";
    for(int i(0); i < gridBoxes[ilev].size(); ++i) {
      const GridBoxes gb = gridBoxes[ilev][i];
      int yboxinv(imagesizevert - (gb.ybox + 1));
      fout <<  gb.xbox << " " <<  yboxinv << " moveto\n";
      fout <<  gb.wbox << " " <<  0       << " rlineto\n";
      fout <<  0       << " " << -gb.hbox << " rlineto\n";
      fout << -gb.wbox << " " <<  0       << " rlineto\n";
      fout << "closepath stroke\n\n";
    }
  }


  fout << "grestore" << '\n';
  fout << "showpage" << '\n';
  fout.close();

  cout << ">>>>>>> WriteNewPSFileTime = " << ((clock()-time0)/1000000.0) << endl;
}


// -------------------------------------------------------------------
void WritePSFile(const char *filename, XImage *image,
                 int imagesizehoriz, int imagesizevert,
                 const Palette &palette)
{
  clock_t time0 = clock();

  ofstream fout(filename);
  if( ! fout) {
    cerr << "*** Error:  cannot create file:  " << filename << endl;
    return;
  }
  fout << "%!PS-Adobe-2.0" << '\n';
  fout <<  "%%BoundingBox: 0 0 " << imagesizehoriz-1 << " "
       << imagesizevert-1 << '\n';
  fout << "gsave" << '\n';
  fout <<  "/picstr " << (imagesizehoriz * 3) << " string def" << '\n';
  fout << imagesizehoriz << " " << imagesizevert << " scale" << '\n';
  fout << imagesizehoriz << " " << imagesizevert << " 8" << '\n';
  fout << "[" << imagesizehoriz << " 0 0 -" << imagesizevert << " 0 "
       << imagesizevert << "]" << '\n';
  fout << "{ currentfile picstr readhexstring pop }" << '\n';
  fout << "false 3" << '\n';
  fout << "colorimage";   // no << '\n';

  fout << hex;
  char *buf = new char[8 * imagesizehoriz + 1];
  unsigned char r, g, b;
  int charindex;
  for(int j(0); j < imagesizevert; ++j) {
    charindex = 0;
    for(int i(0); i < imagesizehoriz; ++i) {
      //BL_ASSERT(charindex>8*imagesizehoriz+1);
      palette.unpixelate(XGetPixel(image, i, j), r, g, b);
      if(i % 10 == 0) {
        sprintf(buf+charindex, "\n");
        ++charindex;
      }
      //fout << setw(2) << setfill('0') << (color.red >> 8);
      //fout << setw(2) << setfill('0') << (color.green >> 8);
      //fout << setw(2) << setfill('0') << (color.blue >> 8) << ' ';
      sprintf(buf+charindex, "%02x%02x%02x ", r, g, b);
      charindex += 7;
    }
    fout << buf;
  }
  delete [] buf;
  fout << "grestore" << '\n';
  fout << "showpage" << '\n';
  fout.close();

  cout << "> > > > WritePSFileTime    = " << ((clock()-time0)/1000000.0) << endl;
}


// -------------------------------------------------------------------
void WritePSPaletteFile(const char *filename, XImage *image,
                        int imagesizehoriz, int imagesizevert,
                        const Vector<Real> &palValueList,
			const string &palNumFormat, const Palette &palette)
{
    ofstream fout(filename);
    if( ! fout) {
      cerr << "*** Error:  cannot create file:  " << filename << endl;
      return;
    }
    fout << "%!PS-Adobe-2.0" << '\n';
    fout <<  "%%BoundingBox: 0 0 " << imagesizehoriz-1 << " "
         << imagesizevert-1 << '\n';
    fout << "gsave" << '\n';
    fout <<  "/picstr " << (imagesizehoriz * 3) << " string def" << '\n';
    fout << imagesizehoriz << " " << imagesizevert << " scale" << '\n';
    fout << imagesizehoriz << " " << imagesizevert << " 8" << '\n';
    fout << "[" << imagesizehoriz << " 0 0 -" << imagesizevert << " 0 "
         << imagesizevert << "]" << '\n';
    fout << "{ currentfile picstr readhexstring pop }" << '\n';
    fout << "false 3" << '\n';
    fout << "colorimage";   // no << '\n';
    
    fout << hex;
    char *buf = new char[8*imagesizehoriz+1];
    for(int j(0); j < imagesizevert; ++j) {
      int charindex(0);
        for(int i(0); i < imagesizehoriz; ++i) {
	  //FIXME
	    unsigned long index = (unsigned long) XGetPixel(image, i, j);
	    unsigned char r, g, b;
	    palette.unpixelate(index, r, g, b);
            if(i % 10 == 0) {
                sprintf(buf+charindex, "\n");
                ++charindex;
            }
            sprintf(buf+charindex, "%02x%02x%02x ", r, g, b);
            charindex += 7;
        }
        fout << buf;
    }
    fout << dec;
    fout << "grestore"  << '\n';
    fout << "0 setgray" << '\n';
    fout << "24 0"      << '\n';
    fout << "120 " << imagesizevert-1 << '\n';
    fout << "rectfill"  << '\n';
    int paletteHeight(216);
    int topOfPalette(256);
    double pSpacing((double) paletteHeight / (double) (palValueList.size() - 1));
    int palSpacing = int(ceil(pSpacing)) + 1;
    fout << "/Palatino-Roman findfont" << '\n' << "20 scalefont"
        << '\n' << "setfont\n1 setgray" << '\n';
    for(int j(0); j < palValueList.size(); ++j) {
        fout << "40 " << topOfPalette - ( j * palSpacing) << " moveto" << '\n';
        fout << "(";
        char dummyString[50];//should be big enough
        sprintf(dummyString, palNumFormat.c_str(), palValueList[j]);
        fout << dummyString << ") show" << '\n';
    }

    fout << '\n' << "showpage" << '\n';
    fout.close();
}


// -------------------------------------------------------------------
void WriteRGBFile(const char *filename, XImage *ximage,
		  int imagesizehoriz, int imagesizevert,
		  const Palette& palette)
{
  unsigned short rbuf[8192];
  unsigned short gbuf[8192];
  unsigned short bbuf[8192];
  int xsize, ysize;
  IMAGE *image;

  xsize = imagesizehoriz;
  ysize = imagesizevert;

  //image = iopen(filename, RLE(1), 3, xsize, ysize, 3);
  // no support for RLE.
  image = iopen(filename, VERBATIM(1), 3, xsize, ysize, 3);
  if(image == NULL) {
    //cerr << "*** Error:  cannot create file:  " << filename << endl;
    return;
  }

  Pixel index;
  unsigned char r, g, b;
  for(int y(0); y < ysize; ++y) {
    /* fill rbuf, gbuf, and bbuf with pixel values */
    for(int x(0); x < xsize; ++x) {
      index = XGetPixel(ximage,x,y);
      palette.unpixelate(index, r, g, b);
      rbuf[x] = r;
      gbuf[x] = g;
      bbuf[x] = b;
    }
    putrow(image,rbuf,ysize-1-y,0);         /* red row */
    putrow(image,gbuf,ysize-1-y,1);         /* green row */
    putrow(image,bbuf,ysize-1-y,2);         /* blue row */
  }
  iclose(image);
}


// -------------------------------------------------------------------
void WritePPMFile(const char *filename, XImage *ximage,
	          int imagesizehoriz, int imagesizevert,
	          const Palette &palette)
{
    std::ofstream img(filename, std::ios::binary);
    if( ! img) {
      cerr << "*** Error:  cannot create file:  " << filename << endl;
      return;
    }
    int xsize(imagesizehoriz);
    int ysize(imagesizevert);
    unsigned char *ivdata = new unsigned char[3 * xsize * ysize];
    Pixel index;
    unsigned char r,g,b;
    int cnt(0);
    for(int y(0); y < ysize; ++y) {
      for(int x(0); x < xsize; ++x) {
        index = XGetPixel(ximage, x, y);
        palette.unpixelate(index, r, g, b);
        ivdata[cnt++] = r;
        ivdata[cnt++] = g;
        ivdata[cnt++] = b;
      }
    }
    BL_ASSERT(cnt == 3*xsize*ysize);
    img << "P6" << endl << xsize << " " << ysize << endl << 255 << endl;
    img.write(reinterpret_cast<const char*>(ivdata), xsize*ysize*3);
    if( ! img) {
      amrex::Error("WritePPMFile:  failed to write image file.");
    }
    delete [] ivdata;
}


// -------------------------------------------------------------------
void WritePPMFileAnnotated(const char *filename, XImage *ximage,
	                   int imagesizehoriz, int imagesizevert,
	                   const Palette &palette, int frame,
			   Real time, GraphicsAttributes *gaPtr,
			   const string &pltFilename, const string &derived)
{
    std::ofstream img(filename, std::ios::binary);
    if( ! img) {
      cerr << "*** Error:  cannot create file:  " << filename << endl;
      return;
    }

    Display *display(gaPtr->PDisplay());
    GC gc(gaPtr->PGC());

    int palWidth(palette.PaletteWidth()), palHeight(palette.PaletteHeight());
    int borderL(40);
    int borderR(palWidth + 48);
    int borderT(40);
    int borderB(40);
    int xsize(imagesizehoriz),  ysize(imagesizevert);
    int xsizeB(xsize + borderL + borderR), ysizeB(ysize + borderT + borderB);

    Pixmap pMap(XCreatePixmap(display, XtWindow(gaPtr->PTopLevel()),
                              xsize, ysize, gaPtr->PDepth()));
    XPutImage(display, pMap, gc, ximage, 0, 0, 0, 0, xsize, ysize);

    Pixmap pMapB(XCreatePixmap(display, XtWindow(gaPtr->PTopLevel()),
                               xsizeB, ysizeB, gaPtr->PDepth()));

    XSetForeground(display, gc, palette.AVBlackPixel());
    XFillRectangle(display, pMapB, gc,  0, 0, xsizeB, ysizeB);

    XSetForeground(display, gc, palette.AVWhitePixel());

    std::ostringstream dataString;
    dataString << std::setprecision(8) << pltFilename
               << "    Frame = " << std::setw(3) << frame
               << "    Time = " << time << " s";
    XDrawString(display, pMapB, gc, borderL, 24, dataString.str().c_str(),
                dataString.str().length());

    int lineX(2 * borderL + xsize);
    dataString.str(std::string());
    dataString << derived;
    XDrawString(display, pMapB, gc, lineX + 32, 24, dataString.str().c_str(),
                dataString.str().length());
    XDrawLine(display, pMapB, gc, lineX, 0, lineX, ysizeB);

    XCopyArea(display, pMap, pMapB, gc, 0, 0, xsize, ysize, borderL, borderT);

    Pixmap pMapPal(palette.GetPixmap());
    XCopyArea(display, pMapPal, pMapB, gc, 0, 0, palWidth, palHeight,
              lineX + 24, borderT - 12);

    XImage *ximageB = XGetImage(display, pMapB, 0, 0, xsizeB, ysizeB, AllPlanes, ZPixmap);

    Pixel index;
    unsigned char r, g, b;
    unsigned char *ivdataB = new unsigned char[3 * xsizeB * ysizeB];
    int cnt(0);
    for(int y(0); y < ysizeB; ++y) {
      for(int x(0); x < xsizeB; ++x) {
        index = XGetPixel(ximageB, x, y);
        palette.unpixelate(index, r, g, b);
        ivdataB[cnt++] = r;
        ivdataB[cnt++] = g;
        ivdataB[cnt++] = b;
      }
    }
    BL_ASSERT(cnt == 3 * xsizeB * ysizeB);

    img << "P6" << '\n' << xsizeB << ' ' << ysizeB << '\n' << 255 << '\n';
    img.write(reinterpret_cast<const char*>(ivdataB), 3 * xsizeB * ysizeB);
    if( ! img) {
      amrex::Error("WritePPMFileAnnotated:  failed to write image file.");
    }

    delete [] ivdataB;
    XFreePixmap(display, pMap);
    XFreePixmap(display, pMapB);
}


#ifdef BL_Darwin
#define _IOWRT 0002
#define _IOERR 0040
#define _IORW  0400
#endif

#ifdef BL_Linux
#define _IOWRT 0002
#define _IOERR 0040
#define _IORW  0400
#endif

#ifdef BL_CYGWIN_NT
#define _IOWRT 0002
#define _IOERR 0040
#define _IORW  0400
#endif


// -------------------------------------------------------------
IMAGE *iopen(const char *file, unsigned int type, unsigned int dim,
             unsigned int xsize, unsigned int ysize, unsigned int zsize)
{
  IMAGE  *image;
  int fdesc(0);

  image = new IMAGE;
  fdesc = creat(file, 0666);
  if(fdesc < 0) {
    cerr << "*** Error:  cannot create file:  " << file << endl;
    return NULL;
  }
  image->imagic = IMAGIC;
  image->type = type;
  image->xsize = xsize;
  image->ysize = 1;
  image->zsize = 1;
  if(dim > 1) {
    image->ysize = ysize;
  }
  if(dim > 2) {
    image->zsize = zsize;
  }
  if(image->zsize == 1) {
    image->dim = 2;
    if(image->ysize == 1) {
      image->dim = 1;
    }
  } else {
    image->dim = 3;
  }
  image->min = 10000000;
  image->max = 0;
  strncpy(image->name,"no name",80);
  image->wastebytes = 0;
  image->dorev = 0;
  if(write(fdesc,image,sizeof(IMAGE)) != sizeof(IMAGE)) {
    cerr << "iopen: error on write of image header" << endl;
    return NULL;
  }
  image->flags = _IOWRT;
  image->cnt = 0;
  image->ptr = 0;
  image->base = 0;
  if( (image->tmpbuf = new unsigned short[IBUFSIZE(image->xsize)]) == 0 ) {
    cerr << "iopen: error on tmpbuf alloc " << image->xsize << endl;
    return NULL;
  }
  image->x = image->y = image->z = 0;
  image->file = fdesc;
  image->offset = 512L;                   // set up for img_optseek
  lseek(image->file, 512L, 0);
  return(image);
}  // end iopen


//----------------------------------------------------------------
int putrow(IMAGE *image, unsigned short *buffer, unsigned int y, unsigned int z)
{
    unsigned short *sptr;
    unsigned char  *cptr;
    unsigned int x;
    unsigned long min, max;
    long cnt;

    if( ! (image->flags & (_IORW|_IOWRT)) ) {
      cerr << "Error 1 in putrow." << endl;
      return -1;
    }
    if(image->dim < 3) {
        z = 0;
    }
    if(image->dim < 2) {
        y = 0;
    }
    if(ISVERBATIM(image->type)) {
        switch(BPP(image->type)) {
            case 1:
                min = image->min;
                max = image->max;
                cptr = (unsigned char *)image->tmpbuf;
                sptr = buffer;
                for(x=image->xsize; x--;) {
                    *cptr = *sptr++;
                    if(*cptr > max) max = *cptr;
                    if(*cptr < min) min = *cptr;
                    ++cptr;
                }
                image->min = min;
                image->max = max;
                img_seek(image,y,z);
                cnt = image->xsize;
                if(img_write(image,(const void *)image->tmpbuf,cnt) != cnt) {
                    cerr << "Error 2 in putrow." << endl;
                    return -1;
		} else {
                    return cnt;
		}
                // NOTREACHED

            case 2:
		cerr << "2 bytes per pixel not supported" << endl;
                return -1;
            default:
                cerr << "putrow: weird bpp" << endl;
        }
    } else if(ISRLE(image->type)) {
      cerr << "RLE not supported" << endl;
    } else {
      cerr << "putrow: weird image type" << endl;
    }
  return 0;
}


// -------------------------------------------------------------
int img_optseek(IMAGE *image, unsigned long offset) {
   if(image->offset != offset) {
     image->offset = offset;
     return lseek(image->file,offset,0);
   }
   return offset;
}


// -------------------------------------------------------------
int img_seek(IMAGE *image, unsigned int y, unsigned int z) {
    if(y >= image->ysize || z >= image->zsize) {
      cerr << "img_seek: row number out of range" << endl;
      return EOF;
    }
    image->x = 0;
    image->y = y;
    image->z = z;
    if(ISVERBATIM(image->type)) {
        switch(image->dim) {
            case 1:
                return img_optseek(image, 512L);
            case 2:
                return img_optseek(image,512L+(y*image->xsize)*BPP(image->type));
            case 3:
                return img_optseek(image,
                    512L+(y*image->xsize+z*image->xsize*image->ysize)*
                                                        BPP(image->type));
            default:
                cerr << "img_seek: weird dim" << endl;
                break;
        }
    } else if(ISRLE(image->type)) {
      cerr << "RLE not supported" << endl;
    } else {
      cerr << "img_seek: weird image type" << endl;
    }
    return 0;
}


// -------------------------------------------------------------
int img_write(IMAGE *image, const void *buffer, long count) {
    long retval;

    retval =  write(image->file,buffer,count);
    if(retval == count) {
        image->offset += count;
    } else {
        image->offset = 0;
    }
    return retval;
}



// -------------------------------------------------------------------
int iclose(IMAGE *image) {
    int ret;
    unsigned short *base;

    if((image->flags&_IOWRT) && (base=image->base)!=NULL && (image->ptr-base)>0) {
      if(putrow(image, base, image->y,image->z)!=image->xsize) {
        image->flags |= _IOERR;
        return(EOF);
      }
    }
    img_optseek(image, 0);
    if(image->flags & _IOWRT) {
      if(image->dorev) {
        cvtimage((long *) image);
      }
      if(img_write(image,(const void *)image,sizeof(IMAGE)) != sizeof(IMAGE)) {
        cerr << "iclose: error on write of image header" << endl;
        return EOF;
      }
      if(image->dorev) {
        cvtimage((long *) image);
      }
    }
    if(image->base) {
      delete image->base;
      image->base = 0;
    }
    if(image->tmpbuf) {
      delete [] image->tmpbuf;
      image->tmpbuf = 0;
    }
    ret = close(image->file);
    delete image;
    return ret;
}


// -------------------------------------------------------------------
void cvtlongs(long buffer[], long n) {
    short i;
    long nlongs = n>>2;
    unsigned long lwrd;

    for(i = 0; i < nlongs; ++i) {
        lwrd = buffer[i];
        buffer[i] =     ((lwrd>>24)             |
                        (lwrd>>8 & 0xff00)      |
                        (lwrd<<8 & 0xff0000)    |
                        (lwrd<<24)              );
    }
}


//----------------------------------------------------------------
void cvtshorts(unsigned short buffer[], long n) {
    short i;
    long nshorts = n>>1;
    unsigned short swrd;

    for(i = 0; i < nshorts; ++i) {
        swrd = *buffer;
        *buffer++ = (swrd>>8) | (swrd<<8);
    }
}


// -------------------------------------------------------------------
void cvtimage(long *buffer) {
  cvtshorts((unsigned short *) buffer, 12);
  cvtlongs(buffer+3, 12);
  cvtlongs(buffer+26, 4);
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
