// -------------------------------------------------------------------
// Output.H
// -------------------------------------------------------------------
#ifndef _OUTPUT_H_
#define _OUTPUT_H_

#include <stdio.h>
#include <iostream.h>
#include <Intrinsic.h>

#include "REAL.H"
#include "aString.H"

// the following is support for writing rgb files.
#define IMAGIC  0732

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


typedef struct {
    unsigned short      imagic;         /* stuff saved on disk . . */
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

    long                file;           /* stuff used in core only */
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
    unsigned long       rleend;         /* for rle images */
    unsigned long       *rowstart;      /* for rle images */
    long                *rowsize;       /* for rle images */
} IMAGE;


void WritePSFile(char *filename, XImage *image,
                 int imagesizehoriz, int imagesizevert,
                 XColor *colorcells, int colorcellsize);

void WritePSPaletteFile(char *filename, XImage *image,
                        int imagesizehoriz, int imagesizevert,
                        XColor *colorcells, int colorcellsize,
                        Real *palValuesList, const aString &palNumFormat,
                        int palDataLength);

void WriteRGBFile(char *filename, XImage *image,
		   int imagesizehoriz, int imagesizevert,
                  XColor *colorcells, int colorcellsize);

#endif


