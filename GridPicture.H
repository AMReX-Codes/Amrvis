//BL_COPYRIGHT_NOTICE

//
// $Id: GridPicture.H,v 1.7 1998-10-29 23:56:08 vince Exp $
//

// ---------------------------------------------------------------
// GridPicture.H
// ---------------------------------------------------------------
#ifndef _GRIDPICTURE_H
#define _GRIDPICTURE_H

#include "AmrvisConstants.H"
#include "Box.H"


class GridPicture {

 public:

  GridPicture();
  ~GridPicture();
  void GridPictureInit(int level, int rratio, int scale,
    			int picSizeH, int picSizeV,
			const Box &overlapbox, const Box &boxWithData,
			int slicedir);
  void ChangeScale(int newScale, int picSizeH, int picSizeV);
  int HPositionInPicture();
  int VPositionInPicture();

  int ImageSizeH() const { return imageSizeH;  }
  int ImageSizeV() const { return imageSizeV;  }


 private:

  Box		overlapBox;
  unsigned int 	dataSizeH, dataSizeV;
  unsigned int  imageSizeH, imageSizeV;
  unsigned int	pictureSizeH, pictureSizeV;
  int		currentScale;
  int		refRatio, sliceDir;
  Box		imageBox;
};

#endif
