// -------------------------------------------------------------------
// ProjectionPicture.H
// -------------------------------------------------------------------
#ifndef _PROJECTIONPICTURE_H_
#define _PROJECTIONPICTURE_H_

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iostream.h>
#include <Xm.h>
#include <DrawingA.h>

#include "AmrPicture.H" 
#include "ViewTransform.H" 
#include "VolRender.H" 

class PltApp;

// represents data points in physical space that connect lines to other points
struct BoxRealPoint {
  Real x, y, z;
  int lineto1, lineto2;
};

// represents box points in screen space with color values
struct BoxTransPoint {
  int x, y, lineto1, lineto2;
};


class ProjectionPicture {
public:
	ProjectionPicture(PltApp *, ViewTransform *, Widget, int, int);

	~ProjectionPicture();

	VolRender *GetVolRenderPtr() const { return volRender; }
	void    ReadTransferFile(const aString &rampFileName);
	void	MakeBoxes();
	void	MakePicture();
	void	DrawBoxesIntoDrawable(const Drawable &drawable,
				      int iFromLevel, int iToLevel);
	void	DrawBoxes(int iFromLevel, int iToLevel);
	void	DrawBoxesIntoPixmap(int iFromLevel, int iToLevel);
	void	DrawPicture();
	void	LabelAxes();
	void	ToggleShowSubCut();
	void	SetSubCut(const Box &newbox);
	void	SetDrawingAreaDimensions(int w, int h); // called when resized 
	void SetDrawingArea(Widget drawhere) { drawingArea = drawhere; }
	XImage *GetPictureXImage() const { return image; }
	int ImageSizeH() const { return daWidth; }
	int ImageSizeV() const { return daHeight; }

private:
        enum { NVERTICIES = 8 };

	Widget		drawingArea;
	AmrPicture     *amrPicturePtr;
	ViewTransform  *viewTransformPtr;
	Array<Array<Array<BoxRealPoint> > >  boxRealPoints;
	Array<Array<Array<BoxTransPoint> > > boxTransPoints;
	Array<int>	boxPoints;
	Array<int>	boxColors;
	int             subCutColor;
	int		daWidth, daHeight;
	bool		showSubCut, pixCreated;
	int             iSubCutBoxIndex, iBoundingBoxIndex;
	int		minDrawnLevel, maxDrawnLevel, maxDataLevel;
	Real		xcenter, ycenter, zcenter;
	Pixmap		pixMap;
	unsigned char  *imageData;
	XImage         *image;
	Array<Box>      theDomain;
	char		buffer[BUFSIZ];
	PltApp         *pltAppPtr;
	Real 		scale[3], aspect, lenRatio, vpLen;
	Real		longestBoxSide;
	Real		longestWindowLength, shortestWindowLength;
	int		longestBoxSideDir;
	int		nDenRampPts, nGradRampPts, nShadeRampPts;
	int		maxDenRampPts, maxGradRampPts, maxShadeRampPts;
	char		rampfilename[LINELENGTH];
	unsigned char	volumeBoxColor;
	VolRender       *volRender;

	// converts a Box to eight BoxRealPoints at the specified level
	void		AddBox(const Box &theBox, int index, int level);
	void            MakeSubCutBox();
	void            TransformBoxPoints(int iLevel, int iBoxIndex);

};

#endif
