// -------------------------------------------------------------------
// Output.C
// -------------------------------------------------------------------
#include "Output.H"
#include <fstream.h>
#include <iomanip.h>
#include <fcntl.h>
#include <unistd.h>

IMAGE *iopen(char *file, unsigned int type, unsigned int dim,
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
void WritePSFile(char *filename, XImage *image,
		 int imagesizehoriz, int imagesizevert,
		 XColor *colorcells, int colorcellsize)
{
  int i, j, index;
  XColor color;

  ofstream fout(filename);
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
  char buf[256];
  for(j = 0; j < imagesizevert; j++) {
    for(i = 0; i < imagesizehoriz; i++) {
      index = (int) XGetPixel(image, i, j);
      color = colorcells[index];
      if(i % 10 == 0) {
        fout << '\n';
      }
      //fout << setw(2) << setfill('0') << (color.red >> 8);
      //fout << setw(2) << setfill('0') << (color.green >> 8);
      //fout << setw(2) << setfill('0') << (color.blue >> 8) << ' ';
      sprintf(buf, "%02x%02x%02x ",
	      (color.red   >> 8),
	      (color.green >> 8),
	      (color.blue  >> 8));
      fout << buf;
    }
  }
  fout << "grestore" << '\n';
  fout << "showpage" << '\n';
  fout.close();
}


// -------------------------------------------------------------------
void WriteRGBFile(char *filename, XImage *ximage,
		   int imagesizehoriz, int imagesizevert,
		   XColor *colorcells, int colorcellsize)
{
  unsigned short rbuf[8192];
  unsigned short gbuf[8192];
  unsigned short bbuf[8192];
  XColor color;
  int x, y, xsize, ysize, index;
  IMAGE *image;

  xsize = imagesizehoriz;
  ysize = imagesizevert;

  //image = iopen(filename, RLE(1), 3, xsize, ysize, 3);
  // no support for RLE.
  image = iopen(filename, VERBATIM(1), 3, xsize, ysize, 3);

  for(y=0; y<ysize; y++) {
    /* fill rbuf, gbuf, and bbuf with pixel values */
    for(x=0; x<xsize; x++) {
      index = (int) XGetPixel(ximage,x,y);
      color = colorcells[index];
      rbuf[x] = color.red   >> 8;
      gbuf[x] = color.green >> 8;
      bbuf[x] = color.blue  >> 8;
    }
    putrow(image,rbuf,ysize-1-y,0);         /* red row */
    putrow(image,gbuf,ysize-1-y,1);         /* green row */
    putrow(image,bbuf,ysize-1-y,2);         /* blue row */
  }
  iclose(image);
}

// -------------------------------------------------------------
IMAGE *iopen(char *file, unsigned int type, unsigned int dim,
             unsigned int xsize, unsigned int ysize, unsigned int zsize)
{
  IMAGE  *image;
  int fdesc = 0;

  image = new IMAGE;
  fdesc = creat(file, 0666);
  if(fdesc < 0) {
    cerr << "iopen: can't open output file " << file << endl;
    return NULL;
  }
  image->imagic = IMAGIC;
  image->type = type;
  image->xsize = xsize;
  image->ysize = 1;
  image->zsize = 1;
  if(dim>1) {
    image->ysize = ysize;
  }
  if(dim>2) {
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
  image->offset = 512L;                   /* set up for img_optseek */
  lseek(image->file, 512L, 0);
  return(image);
}  // end iopen


//----------------------------------------------------------------
int putrow(IMAGE *image, unsigned short *buffer, unsigned int y, unsigned int z) {
    unsigned short     *sptr;
    unsigned char      *cptr;
    unsigned int x;
    unsigned long min, max;
    long cnt;

    if( !(image->flags & (_IORW|_IOWRT)) ) {
      cerr << "Error 1 in putrow." << endl;
      return -1;
    }
    if(image->dim<3) {
        z = 0;
    }
    if(image->dim<2) {
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
                    cptr++;
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
                /* NOTREACHED */

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
    if(y>=image->ysize || z>=image->zsize) {
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

    for(i=0; i<nlongs; i++) {
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

    for(i=0; i<nshorts; i++) {
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
