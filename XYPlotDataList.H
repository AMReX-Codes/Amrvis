#ifndef XYPLOTDATALIST_H
#define XYPLOTDATALIST_H

#include <Xm/Xm.h>
#undef index
#include <stdlib.h>
#include <stdio.h>
#include <float.h>

#include <List.H>
#include <Array.H>
#include <BLassert.H>
#include <REAL.H>
#include <IntVect.H>
#include <FArrayBox.H>
#include <aString.H>

class XYPlotDataListLink {
  friend class XYPlotDataListIterator;
  friend class XYPlotDataList;
public:

  XYPlotDataListLink(Real *d, int sX, int l)
    : data(d), length(l), startXi(sX)
  { 
    BL_ASSERT(l != 0);
  };

protected:
  Real *data;
  int length, startXi, endXi;
  XYPlotDataListLink *down;
  int Ndown;
};


class XYPlotDataList
{
  friend class XYPlotWin;
  friend class XYPlotDataListLink;
  friend class XYPlotDataListIterator;
public:
  XYPlotDataList(const aString &_derived, int max_level,
		 int _gridline,
		 const Array<int> &ratio_list,
		 const Array<Real> &d_x,
		 const Array<char *> &intersect_point,
		 Real start_x = 0.0);
  XYPlotDataList(XYPlotDataList *src);

  ~XYPlotDataList();

  char IsEmpty(void) { return emptyQ; }
  void AddFArrayBox(FArrayBox &fab, int direction, int level);

  void UpdateStats(void);
  void SetLevel(int new_level) {
    curLevel = (new_level < maxLevel) ? new_level : maxLevel;
  }

protected:
  Array <List <XYPlotDataListLink *> *> dataSets; 
  Array <List <int> *> upXi;      // Used for data set iteration [level]
  Array <int> ratios;             // Used for insertion of data sets beginning
                                  // in the middle of a single cell [level]
  Array <Real> dX;                // dX in the proper direction [level]
  Array <char *> intersectPoint;  // Intersected value (in problem space)
                                  // for labelling plots [level]
  Array <double> lloY;            // min Y [level]
  Array <double> hhiY;            // max Y [level]

  int numPoints, maxLevel, curLevel, gridline;
  char updatedQ, emptyQ;
  double startX, endX, offsetX;
  aString derived;
  XYPlotDataList *copied_from;

private:
  void addLink(XYPlotDataListLink *l, int level);

};


class XYPlotDataListIterator
{
public:

  explicit XYPlotDataListIterator (XYPlotDataList *aList);
  ~XYPlotDataListIterator();
  XYPlotDataListIterator& operator++ ();
  bool operator! () const { return (curLink == NULL) ? true : false; }
  operator void* () { return (curLink != NULL) ? this : NULL; }

  double xval, yval;
  
private:

  XYPlotDataList *list;
  Array <ListIterator <int> *> XiLI;
  Array <ListIterator <XYPlotDataListLink *> *> linkLI;
  XYPlotDataListLink *curLink, *nextLink;
  int curXi, nextXi, curLevel, maxLevel;
  Real *data;
  
};
 
#endif /*BL_LIST_H*/
