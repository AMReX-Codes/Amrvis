// ---------------------------------------------------------------
// PltAppState.cpp
// ---------------------------------------------------------------
#include "PltAppState.H"

#ifdef BL_USE_NEW_HFILES
#include <cctype>
#include <strstream>
using std::ostrstream;
using std::ends;
using std::endl;
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
  for(int iframe(0); iframe < minMax.length(); ++iframe) {
    for(int ider(0); ider < minMax[iframe].length(); ++ider) {
      for(int immrt(0); immrt < minMax[iframe][ider].length(); ++immrt) {
	cout << "minMax[" << iframe << "][" << ider << "][" << immrt << "] = ";
	if(minMax[iframe][ider][immrt].IsSet()) {
	  cout << " set      =  ";
	} else {
	  cout << " not set  =  ";
	}
	cout << minMax[iframe][ider][immrt].Min() << "   "
	     << minMax[iframe][ider][immrt].Max() << endl;
      }
    }
  }
}


// -------------------------------------------------------------------
void PltAppState::SetCurrentDerived(const aString &newDerived, int cdnumber) {
  currentDerived = newDerived;
  currentDerivedNumber = cdnumber;
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
