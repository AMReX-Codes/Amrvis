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

#ifdef BL_VOLUMERENDER
#include "VolRender.H" 
#endif

#ifdef BL_PARALLELVOLUMERENDER
//#include "PVolRender.H" 
class PVolRender;
#endif

class PltApp;

// represents data points in physical space that connect lines to other points
class RealPoint {
public:
    RealPoint() { component[0] = component[1] = component[2] = 0.; }
    Real component[3];
    RealPoint(Real r1, Real r2, Real r3) { component[0] = r1; 
    component[1] = r2; component[2] = r3; }
};

// represents box points in screen space with color values
class TransPoint {
public:
    TransPoint() { x = y = 0; }
    int x, y;
    TransPoint( int n1, int n2) { x = n1; y = n2; }
};


class RealBox {
public:
    RealBox();
    ~RealBox() {}
    RealBox(const Box& theBox);
    RealBox(RealPoint p1, RealPoint p2, RealPoint p3, RealPoint p4, 
            RealPoint p5, RealPoint p6, RealPoint p7, RealPoint p8);
    RealPoint vertices[8];
};

class TransBox {
public:
    TransBox();
    ~TransBox() {}
    TransBox(TransPoint p1, TransPoint p2, TransPoint p3, TransPoint p4, 
             TransPoint p5, TransPoint p6, TransPoint p7, TransPoint p8);
    void Draw(Display *display, Window window, GC gc);
    TransPoint vertices[8];
};

class RealSlice {
public:
    RealSlice();
    ~RealSlice() {}
    RealSlice(int count, int slice, const Box &worldBox);
    RealSlice(RealPoint p1, RealPoint p2, RealPoint p3, RealPoint p4);
    void ChangeSlice(int NewSlice);
    RealPoint edges[4];
    int dirOfFreedom; // [0, 2]
};

class TransSlice {
public:
    TransSlice();
    ~TransSlice() {}
    TransSlice(TransPoint p1, TransPoint p2, TransPoint p3, TransPoint p4);
    void Draw(Display *display, Window window, GC gc);
    TransPoint edges[4];
};

//-----------------------------------------------------------------

class ProjectionPicture {
public:
    ProjectionPicture(PltApp *, ViewTransform *, Widget, int, int);
    
    ~ProjectionPicture();
    
#ifdef BL_VOLUMERENDER
	VolRender *GetVolRenderPtr() const { return volRender; }
#endif
#ifdef BL_PARALLELVOLUMERENDER
	PVolRender *GetVolRenderPtr() const { return volRender; }
#endif
    void    ReadTransferFile(const aString &rampFileName);
    void	MakeBoxes();
    void MakeSlices();
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
    void ChangeSlice(int Dir, int newSlice);

    Real longestBoxSide;

private:
    enum { NVERTICIES = 8 };
    
    Widget		drawingArea;
    AmrPicture     *amrPicturePtr;
    ViewTransform  *viewTransformPtr;
    Array<Array<RealBox> > boxReal;
    Array<Array<TransBox> > boxTrans;

    RealBox realBoundingBox;
    TransBox transBoundingBox;

    RealBox realSubCutBox;
    TransBox transSubCutBox;

    RealSlice realSlice[3];
    TransSlice transSlice[3];

    Array<int>	boxPoints;
    Array<int>	boxColors;
    int subCutColor, sliceColor, daWidth, daHeight;
    bool showSubCut, pixCreated;
    int	minDrawnLevel, maxDrawnLevel, maxDataLevel;
    Real xcenter, ycenter, zcenter;
    Pixmap pixMap;
    unsigned char  *imageData;
    XImage  *image;
    Array<Box> theDomain;
    char buffer[BUFSIZ];
    PltApp *pltAppPtr;
    Real scale[3], aspect, lenRatio, vpLen;
    Real longestWindowLength, shortestWindowLength;
    int		longestBoxSideDir;
    int		nDenRampPts, nGradRampPts, nShadeRampPts;
    int		maxDenRampPts, maxGradRampPts, maxShadeRampPts;
    char		rampfilename[LINELENGTH];
    unsigned char	volumeBoxColor;
#ifdef BL_VOLUMERENDER
	VolRender       *volRender;
#endif
#ifdef BL_PARALLELVOLUMERENDER
	PVolRender      *volRender;
#endif
    void MakeAuxiliaries();
    void MakeBoundingBox();
    void MakeSubCutBox();
    void DrawAuxiliaries(const Drawable &drawable);
    void DrawSlices(const Drawable &drawable);
    void LoadSlices(const Box &surroundingBox);
    // converts a Box to eight BoxRealPoints at the specified level
    void		AddBox(const Box &theBox, int index, int level);
    void            TransformBoxPoints(int iLevel, int iBoxIndex);

};

#endif
