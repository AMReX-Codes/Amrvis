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
typedef struct {
  Real x, y, z;
  int lineto1, lineto2, color;
} BoxRealPoint;

// represents box points in screen space with color values
struct BoxTransPoint {
  int x, y, lineto1, lineto2, color;
};


class ProjectionPicture {
public:
	ProjectionPicture(PltApp *, ViewTransform *, Widget, int, int);

	~ProjectionPicture();

	VolRender *GetVolRenderPtr() { return volRender; }
	void    ReadTransferFile(const aString &rampFileName);
	void	MakeBoxes();
	void	MakePicture();
	void	DrawBoxes();
	void	DrawBoxesIntoPixmap();
	void	DrawPicture();
	void	LabelAxes();
	void	ToggleShowSubCut();
	void	SetSubCut(Box newbox);
	void	SetDrawingAreaDimensions(int w, int h); // called when resized 
	void SetDrawingArea(Widget drawhere) { drawingArea = drawhere; }
	XImage *GetPictureXImage() { return image; }
	int ImageSizeH() { return daWidth; }
	int ImageSizeV() { return daHeight; }

private:
	Widget		drawingArea;
	AmrPicture     *amrPicturePtr;
	ViewTransform  *viewTransformPtr;
	BoxRealPoint   *boxrealpoints;
	BoxTransPoint  *boxtranspoints;
	unsigned int	nPoints, bPoints;
	int		daWidth, daHeight, pixCreated;
	int		xHere, yHere, zHere, showSubCut;
	int		minDrawnLevel, maxDataLevel;
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

	void		AddBox(Box, int);  // converts a Box to eight BoxRealPoints 

};

#endif
