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

#include <AMReX_AmrvisConstants.H>
#include <AMReX_Vector.H>
#include <AMReX_Box.H>

#include <string>
using std::string;

using amrex::Real;

const int NOTSETYET(-42);
static void Cnsy(int val) { BL_ASSERT(val != NOTSETYET); }

class CMinMax {
  public:
    CMinMax();
    ~CMinMax();
    void SetMinMax(const Real rmin, const Real rmax);
    void GetMinMax(Real &rmin, Real &rmax);
    bool IsSet() const { return bMinMaxSet; }

    // no is_set checking on these
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
  ~PltAppState();

  PltAppState &operator=(const PltAppState &rhs);
  
  // ------------------------ scale
  int  CurrentScale()  const          { Cnsy(currentScale);  return currentScale; }
  void SetCurrentScale(int newScale)  { currentScale = newScale; }
  int  MaxScale()  const          { Cnsy(maxScale);  return maxScale; }
  void SetMaxScale(int newMaxScale)  { maxScale = newMaxScale; }

  // ------------------------ frame
  void SetCurrentFrame(int newFrame)  { currentFrame = newFrame; }

  // ------------------------ derived
  void SetCurrentDerived(const string &newDerived, int cdnumber);
  const string &CurrentDerived() const { return currentDerived; }
  int CurrentDerivedNumber() const { Cnsy(currentDerivedNumber);
				     return currentDerivedNumber; }

  // ------------------------ boxes
  bool GetShowingBoxes() const        { return showBoxes; }
  void SetShowingBoxes(bool showing)  { showBoxes = showing; }

  // ------------------------ cg smoothing
  bool GetCGSmoothing() const        { return cgSmoothing; }
  void SetCGSmoothing(bool smoothing)  { cgSmoothing = smoothing; }

  // ------------------------ number format
  string GetFormatString() const                 { return formatString;      }
  void SetFormatString(const string &newformat)  { formatString = newformat; }

  // ------------------------ contours
  void SetContourType(amrex::Amrvis::ContourType newContourType)
			     { currentContourType = newContourType; }
  amrex::Amrvis::ContourType GetContourType() const { return currentContourType; }
  void SetNumContours(int ncontours) { nContours = ncontours; }
  int  GetNumContours() const        { Cnsy(nContours);  return nContours; }

  // ------------------------ mins and maxes
  amrex::Amrvis::MinMaxRangeType GetMinMaxRangeType() const { return currentMinMaxType; }
  void SetMinMaxRangeType(amrex::Amrvis::MinMaxRangeType newmmrtype)
				       { currentMinMaxType = newmmrtype; }

  void SetMinMax(const amrex::Amrvis::MinMaxRangeType mmrangetype,
		 const int framenumber,
		 const int derivednumber,
		 const Real rmin, const Real rmax);
  void GetMinMax(const amrex::Amrvis::MinMaxRangeType mmrangetype,
		 const int framenumber,
		 const int derivednumber,
		 Real &rmin, Real &rmax);
  // this one uses the current frame, derived, and range type
  void GetMinMax(Real &rmin, Real &rmax);
  bool IsSet(const amrex::Amrvis::MinMaxRangeType mmrangetype, const int framenumber,
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


  //GraphicsAttributes *GetGAptr() const  { return gaPtr; }

private:

  // state variables
  int currentScale;
  int maxScale;
  int currentFrame;
  string currentDerived;
  int currentDerivedNumber;
  bool showBoxes;
  bool cgSmoothing;
  amrex::Amrvis::ContourType currentContourType;
  int nContours;
  amrex::Amrvis::MinMaxRangeType currentMinMaxType;
  string formatString;

  // mins and maxes
  amrex::Vector<amrex::Vector<amrex::Vector<CMinMax> > > minMax;   // minMax [frame] [derived] [RangeType]

  amrex::Vector<amrex::Box> subDomains;
  int minDrawnLevel, maxDrawnLevel, minAllowableLevel, maxAllowableLevel;
  int finestLevel;


  //bool datasetShowing, paletteDrawn, writingRGB, interfaceReady;
  //int minDrawnLevel, maxDrawnLevel, minDataLevel, maxDataLevel;
  //int activeView;
  string contourNumString, fileName, palFilename;
  //Array<string> fileNames, derivedStrings;
  //GraphicsAttributes	*gaPtr;



  // observers
  //AmrPicture *amrPicturePtrArray[NPLANES];

  //Palette *pltPaletteptr;
  //Dataset *datasetPtr;
  //Array<DataServices *> dataServicesPtr;

};
#endif
