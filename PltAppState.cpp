// ---------------------------------------------------------------
// PltAppState.cpp
// ---------------------------------------------------------------
#include "PltAppState.H"

#ifdef BL_USE_NEW_HFILES
#include <cctype>
#include <strstream>
#include <iostream>
using std::ostrstream;
using std::ends;
using std::cout;
using std::cerr;
using std::endl;
using std::min;
using std::max;
#else
#include <ctype.h>
#include <strstream.h>
#endif


// -------------------------------------------------------------------
CMinMax::CMinMax()
        : bMinMaxSet(false),
	  rMin(AV_BIG_REAL),
	  rMax(-AV_BIG_REAL)
{
}


// -------------------------------------------------------------------
CMinMax::~CMinMax() {
}


// -------------------------------------------------------------------
void CMinMax::SetMinMax(const Real rmin, const Real rmax) {
  BL_ASSERT(rmax >= rmin);
  rMin = rmin;
  rMax = rmax;
  bMinMaxSet = true;
}


// -------------------------------------------------------------------
void CMinMax::GetMinMax(Real &rmin, Real &rmax) {
  BL_ASSERT(bMinMaxSet);
  rmin = rMin;
  rmax = rMax;
}


// -------------------------------------------------------------------
// -------------------------------------------------------------------
PltAppState::PltAppState(int numFrames, int numDerived)
            : currentScale(NOTSETYET),
	      maxScale(NOTSETYET),
	      currentFrame(0),
	      currentDerivedNumber(NOTSETYET),
	      currentContourType(INVALIDCONTOURTYPE),
	      nContours(NOTSETYET),
	      currentMinMaxType(INVALIDMINMAX),
	      minDrawnLevel(NOTSETYET),
	      maxDrawnLevel(NOTSETYET),
	      minAllowableLevel(NOTSETYET),
	      maxAllowableLevel(NOTSETYET),
	      finestLevel(NOTSETYET)
{
  minMax.resize(numFrames);
  for(int nf(0); nf < numFrames; ++nf) {
    minMax[nf].resize(numDerived);
    for(int nd(0); nd < numDerived; ++nd) {
      minMax[nf][nd].resize(NUMBEROFMINMAX);
    }
  }
}


// -------------------------------------------------------------------
PltAppState::~PltAppState() {
}


// -------------------------------------------------------------------
PltAppState &PltAppState::operator=(const PltAppState &rhs) {
  if(this == &rhs) {
    return *this;
  }
  currentScale = rhs.currentScale;
  maxScale = rhs.maxScale;
  currentFrame = rhs.currentFrame;
  currentDerived = rhs.currentDerived;
  currentDerivedNumber = rhs.currentDerivedNumber;
  showBoxes = rhs.showBoxes;
  currentContourType = rhs.currentContourType;
  nContours = rhs.nContours;
  currentMinMaxType = rhs.currentMinMaxType;

  // mins and maxes
  //Array<Array<Array<CMinMax> > > minMax;   // minMax [frame] [derived] [RangeType]
  minMax = rhs.minMax;   // minMax [frame] [derived] [RangeType]

  //Array<Box> subDomains;
  subDomains = rhs.subDomains;

  minDrawnLevel = rhs.minDrawnLevel;
  maxDrawnLevel = rhs.maxDrawnLevel;
  minAllowableLevel = rhs.minAllowableLevel;
  maxAllowableLevel = rhs.maxAllowableLevel;
  finestLevel = rhs.finestLevel;
  contourNumString = rhs.contourNumString;
  formatString = rhs.formatString;
  fileName = rhs.fileName;
  palFilename = rhs.palFilename;

  return *this;
}


// -------------------------------------------------------------------
void PltAppState::SetMinMax(const MinMaxRangeType mmrangetype,
			    const int framenumber,
			    const int derivednumber,
		            const Real rmin, const Real rmax)
{
  minMax[framenumber][derivednumber][mmrangetype].SetMinMax(rmin, rmax);
}


// -------------------------------------------------------------------
void PltAppState::GetMinMax(const MinMaxRangeType mmrangetype,
			    const int framenumber,
			    const int derivednumber,
		            Real &rmin, Real &rmax)
{
  minMax[framenumber][derivednumber][mmrangetype].GetMinMax(rmin, rmax);
}


// -------------------------------------------------------------------
void PltAppState::GetMinMax(Real &rmin, Real &rmax) {
  minMax[currentFrame][currentDerivedNumber][currentMinMaxType].
						      GetMinMax(rmin, rmax);
}


// -------------------------------------------------------------------
bool PltAppState::IsSet(const MinMaxRangeType mmrangetype,
			const int framenumber,
	                const int derivednumber)
{
  return (minMax[framenumber][derivednumber][mmrangetype].IsSet());
}


// -------------------------------------------------------------------
void PltAppState::PrintSetMap() {
  cout << "PltAppState::PrintSetMap(): minMax[frame] [derived] [rangetype]" << endl;
  for(int iframe(0); iframe < minMax.size(); ++iframe) {
    for(int ider(0); ider < minMax[iframe].size(); ++ider) {
      for(int immrt(0); immrt < minMax[iframe][ider].size(); ++immrt) {
	cout << "minMax[" << iframe << "][" << ider << "][" << immrt << "] = ";
	if(minMax[iframe][ider][immrt].IsSet()) {
	  cout << " set      =  ";
	} else {
	  cout << " not set  =  ";
	}
	cout << minMax[iframe][ider][immrt].Min() << "   "
	     << minMax[iframe][ider][immrt].Max() << endl;
      }
      cout << endl;
    }
    cout << endl;
  }
}


// -------------------------------------------------------------------
void PltAppState::SetCurrentDerived(const string &newDerived, int cdnumber) {
  currentDerived = newDerived;
  currentDerivedNumber = cdnumber;
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
