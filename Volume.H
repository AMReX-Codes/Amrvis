//BL_COPYRIGHT_NOTICE

//
// $Id: Volume.H,v 1.15 2000-06-14 21:43:44 car Exp $
//

// ---------------------------------------------------------------
// Volume.H
// ---------------------------------------------------------------
#ifndef _VOLUME_H_
#define _VOLUME_H_

#ifdef BL_VOLUMERENDER
#include <volpack.h>
#else
#define VP_NORM_MAX             7923    /* maximum value of a 13 bit normal */
#endif

#define SWF_HEADER	62			/* bytes of header in file */

  typedef struct {		/* contents of a voxel */
    short normal;			/*   encoded surface normal vector */
      //the short is unused in the non-lighting model
    unsigned char density;	/*   original density */
    unsigned char gradient;	/*   original gradient */
      //gradient becomes the normal in the non-lighting model
  } RawVoxel;


#define DENSITY_PARAM		0		/* classification parameters */
#define OCTREE_DENSITY_THRESH	4
#define GRADIENT_PARAM		1
#define OCTREE_GRADIENT_THRESH	4
#define OCTREE_BASE_NODE_SIZE	4


#define BYTES_PER_VOXEL	sizeof(RawVoxel)	// voxel size in bytes

#endif
