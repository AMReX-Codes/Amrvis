//BL_COPYRIGHT_NOTICE

//
// $Id: AmrvisConstants.H,v 1.4 1998-10-29 23:56:06 vince Exp $
//

// ---------------------------------------------------------------
// AmrvisConstants.H
// ---------------------------------------------------------------
#ifndef _AMRVISCONSTANTS_H
#define _AMRVISCONSTANTS_H

#include "REAL.H"
#ifdef BL_USE_NEW_HFILES
#include <cfloat>
#else
#include <float.h>
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

enum AnimDirection { ANIMPOSDIR = 0, ANIMNEGDIR };

enum Range { USEGLOBAL = 0, USELOCAL, USESPECIFIED, USEFILE, NRANGES, WOFFSET };

enum FileType {
  INVALIDTYPE = 0, FAB, MULTIFAB, NEWPLT, NUMDBPTYPES
};

static char *FileTypeString[] = {
  "invalidtype", "fab", "multifab", "newplt"
};

// number of regions in computational domain
#if(BL_SPACEDIM == 1)
#error 1D not supported by amrvis
const int NUM_REGIONS = 3;
#endif
#if(BL_SPACEDIM == 2)
const int NUM_REGIONS = 9;
#endif
#if(BL_SPACEDIM == 3)
const int NUM_REGIONS = 27;
#endif

#ifdef BL_USE_FLOAT
  const Real AV_BIG_REAL = FLT_MAX;
#else
# ifdef BL_USE_DOUBLE
    const Real AV_BIG_REAL = DBL_MAX;
# else
#   error Must define BL_USE_FLOAT or BL_USE_DOUBLE
# endif
#endif

#endif
// -------------------------------------------------------------------
// -------------------------------------------------------------------
