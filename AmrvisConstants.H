
//
// $Id: AmrvisConstants.H,v 1.18 2001-11-16 01:48:08 vince Exp $
//

// ---------------------------------------------------------------
// AmrvisConstants.H
// ---------------------------------------------------------------
#ifndef _AMRVISCONSTANTS_H
#define _AMRVISCONSTANTS_H

#include <REAL.H>
#include <cfloat>

#include <stdio.h>

#ifndef BUFSIZ
# define BUFSIZ 8192
#endif

const int LINELENGTH = 160;
const int MAXARGS = 50;

#if (BL_SPACEDIM == 2)
const int NPLANES = 1;
#endif
#if (BL_SPACEDIM == 3)
const int NPLANES = 3;
#endif

const int XDIR = 0;
const int YDIR = 1;
const int ZDIR = 2;

const int XY = 0;
const int XZ = 1;
const int YZ = 2;

const int XYPLANE = 0;
const int XZPLANE = 1;
const int YZPLANE = 2;

const int ZPLANE = 0;
const int YPLANE = 1;
const int XPLANE = 2;
const int ISOVIEW = 3;
const int PALETTEVIEW = 4;


const int WOFFSET = 4;

enum AnimDirection { ANIMPOSDIR = 0, ANIMNEGDIR };

// these are for PltApps wRangeRadioButton
enum MinMaxRangeTypeForButtons {
    BGLOBALMINMAX        = 0,
    BSUBREGIONMINMAX     = 1,
    BUSERMINMAX          = 2,
    BNUMBEROFMINMAX      = 3,
    BINVALIDMINMAX
};

enum MinMaxRangeType {
    GLOBALMINMAX        = 0,
    SUBREGIONMINMAX     = 1,
    USERMINMAX          = 2,
    FILEGLOBALMINMAX    = 3,
    FILESUBREGIONMINMAX = 4,
    FILEUSERMINMAX      = 5,
    NUMBEROFMINMAX      = 6,
    INVALIDMINMAX
};

static bool UsingFileRange(const MinMaxRangeType rt) {
  return(rt == FILEGLOBALMINMAX    ||
         rt == FILESUBREGIONMINMAX ||
	 rt == FILEUSERMINMAX);
}

enum ContourType {
  RASTERONLY     = 0,
  RASTERCONTOURS = 1,
  COLORCONTOURS  = 2,
  BWCONTOURS     = 3,
  VECTORS        = 4,
  NCONTOPTIONS   = 5,
  INVALIDCONTOURTYPE
};

enum FileType { INVALIDTYPE = 0, FAB, MULTIFAB, NEWPLT, NUMDBPTYPES };

static char *FileTypeString[] = {
  "invalidtype", "fab", "multifab", "newplt"
};


#ifdef BL_USE_FLOAT
#define AV_BIG_REAL FLT_MAX
#else
# ifdef BL_USE_DOUBLE
#define AV_BIG_REAL DBL_MAX
# else
#   error Must define BL_USE_FLOAT or BL_USE_DOUBLE
# endif
#endif

#endif
// -------------------------------------------------------------------
// -------------------------------------------------------------------
