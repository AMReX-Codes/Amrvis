// ---------------------------------------------------------------
// PltAppState.H
// ---------------------------------------------------------------
//  state observers:
//    pltapp
//    amrpicture
//    projectionpicture
//    dataset
//    palette
//    lightingwindow
//    xyplots
// ---------------------------------------------------------------
#ifndef _PLTAPPSTATE_H_
#define _PLTAPPSTATE_H_

#include "AmrvisConstants.H"
#include "aString.H"
#include "Array.H"
#include "Box.H"

const int NOTSETYET(-42);
static void Cnsy(int val) { BL_ASSERT(val != NOTSETYET); }

class CMinMax {
  public:
    CMinMax();
    ~CMinMax();
    void SetMinMax(const Real rmin, const Real rmax);
    void GetMinMax(Real &rmin, Real &rmax);
    bool IsSet() const { return bMinMaxSet; }

    // no is set checking on these
    Real Min() const { return rMin; }
    Real Max() const { return rMax; }

  private:
    Real rMin, rMax;
    bool bMinMaxSet;
};


class AmrPicture;


class PltAppState {
    
private:
  PltAppState();  // disallow

public:
  // created when opening a new file
  PltAppState(int numFrames, int numDerived);
  
  // for subregion
  /*
  PltAppState(XtAppContext app, Widget w, const Box &region,
         const IntVect &offset,
         AmrPicture *parentPtr, PltApp *pltparent,
         const aString &palfile, bool isAnim,
         const aString &newderived, const aString &file);
  */
  
  ~PltAppState();
  
  // ------------------------ scale
  int  CurrentScale()  const          { Cnsy(currentScale);  return currentScale; }
  void SetCurrentScale(int newScale)  { currentScale = newScale; }
  int  MaxScale()  const          { Cnsy(maxScale);  return maxScale; }
  void SetMaxScale(int newMaxScale)  { maxScale = newMaxScale; }

  // ------------------------ frame
  void SetCurrentFrame(int newFrame)  { currentFrame = newFrame; }

  // ------------------------ derived
  void SetCurrentDerived(const aString &newDerived, int cdnumber);
  const aString &CurrentDerived() const { return currentDerived; }
  int CurrentDerivedNumber() const { Cnsy(currentDerivedNumber);
				     return currentDerivedNumber; }

  // ------------------------ boxes
  bool GetShowingBoxes() const        { return showBoxes; }
  void SetShowingBoxes(bool showing)  { showBoxes = showing; }

  // ------------------------ contours
  void SetContourType(ContourType newContourType)
			     { currentContourType = newContourType; }
  ContourType GetContourType() const { return currentContourType; }
  void SetNumContours(int ncontours) { nContours = ncontours; }
  int  GetNumContours() const        { Cnsy(nContours);  return nContours; }

  // ------------------------ mins and maxes
  MinMaxRangeType GetMinMaxRangeType() const { return currentMinMaxType; }
  void SetMinMaxRangeType(MinMaxRangeType newmmrtype)
				       { currentMinMaxType = newmmrtype; }

  void SetMinMax(const MinMaxRangeType mmrangetype,
		 const int framenumber,
		 const int derivednumber,
		 const Real rmin, const Real rmax);
  void GetMinMax(const MinMaxRangeType mmrangetype,
		 const int framenumber,
		 const int derivednumber,
		 Real &rmin, Real &rmax);
  // this one uses the current frame, derived, and range type
  void GetMinMax(Real &rmin, Real &rmax);
  bool IsSet(const MinMaxRangeType mmrangetype, const int framenumber,
	     const int derivednumber);
  void PrintSetMap();

  // ------------------------ levels
  void SetMinAllowableLevel(int newval) { minAllowableLevel = newval; }
  void SetMaxAllowableLevel(int newval) { maxAllowableLevel = newval; }
  void SetMinDrawnLevel(int newval)     { minDrawnLevel = newval; }
  void SetMaxDrawnLevel(int newval)     { maxDrawnLevel = newval; }
  void SetFinestLevel(int newval)       { finestLevel = newval; }

  int MinAllowableLevel() const { Cnsy(minAllowableLevel);
				  return minAllowableLevel; }
  int MaxAllowableLevel() const { Cnsy(maxAllowableLevel);
				  return maxAllowableLevel; }
  int MinDrawnLevel() const { Cnsy(minDrawnLevel);  return minDrawnLevel; }
  int MaxDrawnLevel() const { Cnsy(maxDrawnLevel);  return maxDrawnLevel; }
  int FinestLevel()   const { Cnsy(finestLevel);    return finestLevel; }


  //GraphicsAttributes *GetGAptr() const  { return GAptr; }

private:

  // state variables
  int currentScale;
  int maxScale;
  int currentFrame;
  aString currentDerived;
  int currentDerivedNumber;
  bool showBoxes;
  ContourType currentContourType;
  int nContours;
  MinMaxRangeType currentMinMaxType;

  // mins and maxes
  Array<Array<Array<CMinMax> > > minMax;   // minMax [frame] [derived] [RangeType]

  Array<Box> subDomains;
  int minDrawnLevel, maxDrawnLevel, minAllowableLevel, maxAllowableLevel;
  int finestLevel;


  //bool datasetShowing, paletteDrawn, writingRGB, interfaceReady;
  //int minDrawnLevel, maxDrawnLevel, minDataLevel, maxDataLevel;
  //int activeView;
  aString contourNumString, formatString, fileName, palFilename;
  //Array<aString> fileNames, derivedStrings;
  //GraphicsAttributes	*GAptr;



  // observers
  //AmrPicture *amrPicturePtrArray[NPLANES];

  //Palette *pltPaletteptr;
  //Dataset *datasetPtr;
  //Array<DataServices *> dataServicesPtr;

};
#endif
