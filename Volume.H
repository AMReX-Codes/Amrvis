// -------------------------------------------------------------------
// Volume.H
// -------------------------------------------------------------------
#ifndef _VOLUME_H_
#define _VOLUME_H_

#include <stdio.h>

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


static RawVoxel *dummy_voxel;

#define BYTES_PER_VOXEL	sizeof(RawVoxel)	// voxel size in bytes

#define CheckVP(vpret, n)  \
	  if(vpret != VP_OK) { \
            cerr << "VolPack error " << n << ":  " \
		 << vpGetErrorString(vpGetError(vpc)) << endl; \
          exit(-4); \
	  }




// # define VOXEL_FIELDS	3	/* number of fields in voxel */

// # define NORMAL_FIELD	0
// # define NORMAL_OFFSET	vpFieldOffset(dummy_voxel, normal)
// # define NORMAL_SIZE	sizeof(short)
// # define NORMAL_MAX	VP_NORM_MAX

// # define DENSITY_FIELD	1
// # define DENSITY_OFFSET	vpFieldOffset(dummy_voxel, density)
// # define DENSITY_SIZE	sizeof(unsigned char)
// # define DENSITY_MAX	255

// # define GRADIENT_FIELD	2
// # define GRADIENT_OFFSET	vpFieldOffset(dummy_voxel, gradient)
// # define GRADIENT_SIZE	sizeof(unsigned char)
// //# define GRADIENT_MAX	VP_GRAD_MAX
// # define GRADIENT_MAX	255

// #else

//   typedef struct {		/* contents of a voxel */
//     short normal;			/*   encoded surface normal vector */
//     unsigned char density;	/*   original density */
//     unsigned char gradient;	/*   original gradient */
//   } RawVoxel;


// typedef struct {
//     unsigned char normal;
//     unsigned char density;
//   } RawVoxel;

// # define VOXEL_FIELDS	2

// # define NORMAL_FIELD	0
// # define NORMAL_OFFSET	vpFieldOffset(dummy_voxel, normal)
// # define NORMAL_SIZE	sizeof(unsigned char)
// # define NORMAL_MAX	255

// # define DENSITY_FIELD	1
// # define DENSITY_OFFSET	vpFieldOffset(dummy_voxel, density)
// # define DENSITY_SIZE	sizeof(unsigned char)
// # define DENSITY_MAX	255

// #endif

#endif
