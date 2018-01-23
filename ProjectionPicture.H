// ---------------------------------------------------------------
// ProjectionPicture.H
// ---------------------------------------------------------------
#ifndef _PROJECTIONPICTURE_H_
#define _PROJECTIONPICTURE_H_

#include <Xm/Xm.h>
#undef index

#include <AmrPicture.H>
#include <ViewTransform.H>
#include <Palette.H>

#ifdef BL_VOLUMERENDER
#include <VolRender.H>
#endif

using amrex::Real;

class PltApp;

// represents data points in physical space that connect lines to other points
class RealPoint {
  public:
    RealPoint() { component[0] = component[1] = component[2] = 0.0; }
    Real component[3];
    RealPoint(Real r1, Real r2, Real r3) {
      component[0] = r1; 
      component[1] = r2;
      component[2] = r3;
    }
};

// represents box points in screen space with color values
class TransPoint {
  public:
    TransPoint() { x = y = 0; }
    int x, y;
    TransPoint( int n1, int n2) { x = n1; y = n2; }
};


class AVRealBox {
  public:
    AVRealBox();
    ~AVRealBox() { }
    AVRealBox(const amrex::Box &theBox);
    RealPoint vertices[8];
};

class TransBox {
  public:
    TransBox();
    ~TransBox() { }
    void Draw(Display *display, Window window, GC gc);
    TransPoint vertices[8];
};

class RealSlice {
  public:
    RealSlice();
    ~RealSlice() {}
    RealSlice(int count, int slice, const amrex::Box &worldBox);
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
  ProjectionPicture(PltApp *, ViewTransform *, Palette *, Widget, int, int);
  
  ~ProjectionPicture();
  
#ifdef BL_VOLUMERENDER
  VolRender *GetVolRenderPtr() const { return volRender; }
#endif
  void	MakeBoxes();
  void MakeSlices();
  void	MakePicture();
  void	DrawBoxesIntoDrawable(const Drawable &drawable,
                              int iFromLevel, int iToLevel);
  void	DrawBoxes(int iFromLevel, int iToLevel);
  XImage *DrawBoxesIntoPixmap(int iFromLevel, int iToLevel);
  void	DrawPicture();
  void	LabelAxes();
  void	ToggleShowSubCut();
  void	SetSubCut(const amrex::Box &newbox);
  void	SetDrawingAreaDimensions(int w, int h); // called when resized 
  void SetDrawingArea(Widget drawhere) { drawingArea = drawhere; }
  XImage *GetPictureXImage() const { return PPXImage; }
  int ImageSizeH() const { return daWidth; }
  int ImageSizeV() const { return daHeight; }
  void ChangeSlice(int Dir, int newSlice);
  Real LongestBoxSide() const { return longestBoxSide; }
  void ResetPalette(Palette *newpally);
  
  
private:
  enum { NVERTICIES = 8 };
  
  Widget	  drawingArea;
  AmrPicture     *amrPicturePtr;
  ViewTransform  *viewTransformPtr;
  amrex::Vector<amrex::Vector<AVRealBox> > boxReal;
  amrex::Vector<amrex::Vector<TransBox> > boxTrans;
  Real longestBoxSide;
  
  AVRealBox realBoundingBox;
  TransBox transBoundingBox;
  
  AVRealBox realSubCutBox;
  TransBox transSubCutBox;
  
  RealSlice realSlice[3];
  TransSlice transSlice[3];
  
  amrex::Vector<int> boxPoints;
  amrex::Vector<Pixel> boxColors;
  int daWidth, daHeight;
  Pixel subCutColor, sliceColor;
  bool showSubCut, pixCreated;
  int  minDrawnLevel, maxDrawnLevel, maxDataLevel;
  Real xcenter, ycenter, zcenter;
  Pixmap pixMap;
  Palette *palettePtr;
  unsigned char *imageData, *volpackImageData;
  XImage *PPXImage;
  amrex::Vector<amrex::Box> theDomain;
  char buffer[BUFSIZ];
  PltApp *pltAppPtr;
  Real scale[3];
  Real longestWindowLength, shortestWindowLength;
  int  longestBoxSideDir;
  int  nDenRampPts, nGradRampPts, nShadeRampPts;
  int  maxDenRampPts, maxGradRampPts, maxShadeRampPts;
  unsigned char	volumeBoxColor;
#ifdef BL_VOLUMERENDER
  VolRender *volRender;
#endif
  void MakeAuxiliaries();
  void MakeBoundingBox();
  void MakeSubCutBox();
  void DrawAuxiliaries(const Drawable &drawable);
  void DrawSlices(const Drawable &drawable);
  void LoadSlices(const amrex::Box &surroundingBox);
  // converts a Box to eight BoxRealPoints at the specified level
  void AddBox(const amrex::Box &theBox, int index, int level);
  void TransformBoxPoints(int iLevel, int iBoxIndex);

};

//--------------------------------------------------------------------

#endif
