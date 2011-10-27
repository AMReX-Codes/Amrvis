// ---------------------------------------------------------------
// WriteRGB.cpp
// ---------------------------------------------------------------
#include <cstdio>

#define IMAGIC  0732

/* colormap of images */
#define CM_NORMAL               0       /* file contains rows of values which
                                         * are either RGB values (zsize == 3)
                                         * or greyramp values (zsize == 1) */
#define CM_DITHERED             1
#define CM_SCREEN               2       /* file contains data which is a screen
                                         * image; getrow returns buffer which
                                         * can be displayed directly with
                                         * writepixels */
#define CM_COLORMAP             3       /* a colormap file */

#define TYPEMASK                0xff00
#define BPPMASK                 0x00ff
#define ITYPE_VERBATIM          0x0000
#define ITYPE_RLE               0x0100
#define ISRLE(type)             (((type) & 0xff00) == ITYPE_RLE)
#define ISVERBATIM(type)        (((type) & 0xff00) == ITYPE_VERBATIM)
#define BPP(type)               ((type) & BPPMASK)
#define RLE(bpp)                (ITYPE_RLE | (bpp))
#define VERBATIM(bpp)           (ITYPE_VERBATIM | (bpp))
#define IBUFSIZE(pixels)        ((pixels+(pixels>>6))<<2)
#define RLE_NOP                 0x00

#define ierror(p)               (((p)->flags&_IOERR)!=0)
#define ifileno(p)              ((p)->file)
#define getpix(p)               (--(p)->cnt>=0 ? *(p)->ptr++ : ifilbuf(p))
#define putpix(p,x)             (--(p)->cnt>=0 \
                                    ? ((int)(*(p)->ptr++=(unsigned)(x))) \
                                    : iflsbuf(p,(unsigned)(x)))


typedef struct {
    unsigned short      imagic;         // stuff saved on disk
    unsigned short      type;
    unsigned short      dim;
    unsigned short      xsize;
    unsigned short      ysize;
    unsigned short      zsize;
    unsigned long       min;
    unsigned long       max;
    unsigned long       wastebytes;
    char                name[80];
    unsigned long       colormap;

    long                file;           // stuff used in core only
    unsigned short      flags;
    short               dorev;
    short               x;
    short               y;
    short               z;
    short               cnt;
    unsigned short      *ptr;
    unsigned short      *base;
    unsigned short      *tmpbuf;
    unsigned long       offset;
    unsigned long       rleend;         // for rle images
    unsigned long       *rowstart;      // for rle images
    long                *rowsize;       // for rle images
} IMAGE;


IMAGE *iopen();
unsigned short *ibufalloc();


// -------------------------------------------------------------
IMAGE *iopen(char *file, char *mode, unsigned int type, unsigned int dim,
             unsigned int xsize, unsigned int ysize, unsigned int zsize)
{
    return(imgopen(0, file, mode, type, dim, xsize, ysize, zsize));
}

// -------------------------------------------------------------
IMAGE *imgopen(int f, char *file, char *mode, unsigned int type, unsigned int dim,
               unsigned int xsize, unsigned int ysize, unsigned int zsize)
{
        IMAGE  *image;
        int tablesize;
        int i, max;

        image = (IMAGE*)calloc(1,sizeof(IMAGE));
        if(*mode=='w') {
                if(file) {
                  f = creat(file, 0666);
                }
                if(f < 0) {
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
                isetname(image,"no name");
                image->wastebytes = 0;
                image->dorev = 0;
                if(write(f,image,sizeof(IMAGE)) != sizeof(IMAGE)) {
                  cerr << "iopen: error on write of image header" << endl;
                  return NULL;
                }
        } else {
        }
        image->flags = _IOREAD;
        if(ISRLE(image->type)) {
            tablesize = image->ysize*image->zsize*sizeof(long);
            image->rowstart = (unsigned long *)malloc(tablesize);
            image->rowsize = (long *)malloc(tablesize);
            if( image->rowstart == 0 || image->rowsize == 0 ) {
              cerr << "iopen: error on table alloc" << endl;
              return NULL;
            }
            image->rleend = 512L+2*tablesize;
            if(*mode=='w') {
              max = image->ysize*image->zsize;
              for(i = 0; i < max; ++i) {
                image->rowstart[i] = 0;
                image->rowsize[i] = -1;
              }
            }
        }
        image->cnt = 0;
        image->ptr = 0;
        image->base = 0;
        if( (image->tmpbuf = ibufalloc(image)) == 0 ) {
          cerr << "iopen: error on tmpbuf alloc " << image->xsize << endl;
          return NULL;
        }
        image->x = image->y = image->z = 0;
        image->file = f;
        image->offset = 512L;          // set up for img_optseek
        lseek(image->file, 512L, 0);
        return(image);
}


//----------------------------------------------------------------
unsigned short *ibufalloc(IMAGE *image) {
  return (unsigned short *) malloc(IBUFSIZE(image->xsize));
}


//----------------------------------------------------------------
cvtshorts( unsigned short buffer[], long n) {
    short i;
    long nshorts = n>>1;
    unsigned short swrd;

    for(i = 0; i < nshorts; ++i) {
        swrd = *buffer;
        *buffer++ = (swrd>>8) | (swrd<<8);
    }
}


//----------------------------------------------------------------
int putrow(IMAGE *image, unsigned short *buffer, unsigned int y, unsigned int z) {
    unsigned short     *sptr;
    unsigned char      *cptr;
    unsigned int x;
    unsigned long min, max;
    long cnt;

    if( ! (image->flags & (_IORW|_IOWRT)) ) {
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
                for(x=image->xsize; --x;) {
                    *cptr = *sptr++;
                    if(*cptr > max) { max = *cptr; }
                    if(*cptr < min) { min = *cptr; }
                    ++cptr;
                }
                image->min = min;
                image->max = max;
                img_seek(image,y,z);
                cnt = image->xsize;
                if(img_write(image,image->tmpbuf,cnt) != cnt) {
                  return -1;
		} else {
                  return cnt;
		}
	    break;
            case 2:
                min = image->min;
                max = image->max;
                sptr = buffer;
                for(x=image->xsize; --x;) {
                  if(*sptr > max) { max = *sptr; }
                  if(*sptr < min) { min = *sptr; }
                  ++sptr;
                }
                image->min = min;
                image->max = max;
                img_seek(image,y,z);
                cnt = image->xsize<<1;
                if(image->dorev) {
                  cvtshorts(buffer,cnt);
		}
                if(img_write(image,buffer,cnt) != cnt) {
                  if(image->dorev) {
                    cvtshorts(buffer,cnt);
		  }
                  return -1;
                } else {
                  if(image->dorev) {
                    cvtshorts(buffer,cnt);
		  }
                  return image->xsize;
                }
	    break;
            default:
                cerr << "putrow: weird bpp" << endl;
        }
    } else if(ISRLE(image->type)) {
        switch(BPP(image->type)) {
            case 1:
                min = image->min;
                max = image->max;
                sptr = buffer;
                for(x = image->xsize; --x;) {
                  if(*sptr > max) { max = *sptr; }
                  if(*sptr < min) { min = *sptr; }
                  ++sptr;
                }
                image->min = min;
                image->max = max;
                cnt = img_rle_compact(buffer,2,image->tmpbuf,1,image->xsize);
                img_setrowsize(image,cnt,y,z);
                img_seek(image,y,z);
                if(img_write(image,image->tmpbuf,cnt) != cnt) {
                  return -1;
		} else {
                  return image->xsize;
		}
	    break;
            case 2:
                min = image->min;
                max = image->max;
                sptr = buffer;
                for(x=image->xsize; --x;) {
                  if(*sptr > max) { max = *sptr; }
                  if(*sptr < min) { min = *sptr; }
                  ++sptr;
                }
                image->min = min;
                image->max = max;
                cnt = img_rle_compact(buffer,2,image->tmpbuf,2,image->xsize);
                cnt <<= 1;
                img_setrowsize(image,cnt,y,z);
                img_seek(image,y,z);
                if(image->dorev) {
                    cvtshorts(image->tmpbuf,cnt);
		}
                if(img_write(image,image->tmpbuf,cnt) != cnt) {
                    if(image->dorev)
                        cvtshorts(image->tmpbuf,cnt);
                    return -1;
                } else {
                    if(image->dorev)
                        cvtshorts(image->tmpbuf,cnt);
                    return image->xsize;
                }
	    break;
            default:
                cerr << "putrow: weird bpp\n" << endl;
        }
    } else {
        cerr << "putrow: bad image type" << endl;
    }
}
// -------------------------------------------------------------
// -------------------------------------------------------------
