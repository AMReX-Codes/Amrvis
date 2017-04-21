// -------------------------------------------------------------------
// XYPlotDataList.H
// -------------------------------------------------------------------
#ifndef XYPLOTDATALIST_H
#define XYPLOTDATALIST_H

#include <AMReX_Array.H>
#include <AMReX_BLassert.H>
#include <AMReX_REAL.H>
#include <AMReX_FArrayBox.H>

#include <list>
#include <string>
using std::string;
using std::list;


class OrderedBoxes {
  public:
    OrderedBoxes(const int level, int idir, const Box &databox, const Box &finebox)
      : iLevel(level), iDir(idir), dataBox(databox), finestBox(finebox)
    { }

    int ILevel() const { return iLevel; }
    const Box &DataBox() const { return dataBox; }
    const Box &FinestBox() const { return finestBox; }
    bool operator<(const OrderedBoxes &ob) const {
      return(finestBox.smallEnd()[iDir] < ob.finestBox.smallEnd()[iDir]);
    }

  protected:
    int iLevel, iDir;
    Box dataBox, finestBox;

};


class XYPlotDataListLink {
  public:
    XYPlotDataListLink(const Real *d, int sX, int len)
      : xypdllLength(len), startXi(sX)
    { 
      BL_ASSERT(len != 0);
      endXi = sX + len;
      xypdllData = new Real[len];
      for(int i(0); i < len; ++i) {
        xypdllData[i] = d[i];
      }
    }

    ~XYPlotDataListLink() {
      delete [] xypdllData;
    }

    Real *XYPDLLData() const { return xypdllData; }
    int XYDPLLength() const { return xypdllLength; }
    int StartXi() const { return startXi; }
    int EndXi() const { return endXi; }
    bool operator<(const XYPlotDataListLink &xyp) const {
      return(startXi < xyp.startXi);
    }

  protected:
    Real *xypdllData;
    int xypdllLength, startXi, endXi;
};


class XYPlotDataList {
  public:
    XYPlotDataList(const string &derived, int minlevel, int maxlevel,
		   int gridlinein,
		   const Array<int> &ratiolist,
		   const Array<Real> &dx,
		   const Array<char *> &intersectpoint,
		   Real startx = 0.0);
    XYPlotDataList(XYPlotDataList *src);
    ~XYPlotDataList();

    void AddFArrayBox(FArrayBox &fab, int direction, int level);

    void UpdateStats(void);
    void SetLevel(int new_level) {
      curLevel = (new_level < maxLevel) ? new_level : maxLevel;
    }
    const string &DerivedName() const { return xypdlDerived; }
    const XYPlotDataList *CopiedFrom() const { return copiedFrom; }
    void SetCopiedFrom(XYPlotDataList *xypdl) { copiedFrom = xypdl; }
    int MaxLevel()  const { return maxLevel; }
    int NumPoints() const { return numPoints[curLevel]; }
    int CurLevel()  const { return curLevel; }
    int Gridline()  const { return gridline; }
    double StartX() const { return startX; }
    double EndX()   const { return endX; }
    double XYPDLLoY(const int ilev) const { return xypdlLoY[ilev]; }
    double XYPDLHiY(const int ilev) const { return xypdlHiY[ilev]; }
    char *IntersectPoint(const int ilev) const { return intersectPoint[ilev]; }
    Array<double> &XVal(int level) { return xypdlXVal[level]; }
    Array<double> &YVal(int level) { return xypdlYVal[level]; }

  protected:
    Array<list<XYPlotDataListLink *> > dataSets; 
    Array<int> xypdlRatios;        // Used for insertion of data sets beginning
                                    // in the middle of a single cell [level]
    Array<Real> dX;                // dX in the proper direction [level]
    Array<char *> intersectPoint;  // Intersected value (in problem space)
                                    // for labelling plots [level]
    Array<double> xypdlLoY, xypdlHiY;  // [level]
    Array<Array<double> > xypdlXVal, xypdlYVal;  // [level][data]

    Array<int> numPoints;
    int  minLevel, maxLevel, curLevel, gridline, whichDir;
    bool updatedQ;
    double startX, endX, offsetX;
    string xypdlDerived;
    XYPlotDataList *copiedFrom;
    Array<BoxList> fabBoxLists, fillBoxLists;
};
#endif
// -------------------------------------------------------------------
// -------------------------------------------------------------------
