// ---------------------------------------------------------------
// PltAppState.cpp
// ---------------------------------------------------------------
#include "PltAppState.H"

/*
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
*/


// -------------------------------------------------------------------
PltAppState::PltAppState()
            : currentScale(-1),
	      currentContourType(INVALIDCONTOURTYPE)
{
}


// -------------------------------------------------------------------
PltAppState::~PltAppState() {
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
