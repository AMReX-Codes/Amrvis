// -------------------------------------------------------------------
// GlobalUtilites.H
// -------------------------------------------------------------------
#ifndef _GLOBALUTILITIES_H_
#define _GLOBALUTILITIES_H_

#include <fcntl.h>
#include <iostream.h>
#include <strstream.h>
#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "AmrvisConstants.H"
#include "Box.H"
#include "Array.H"
#include "aString.H"
#include "List.H"

Box GetBoxFromCommandLine();
void SetMaxPictureSize(int size);
int  MaxPictureSize();
void SetVerbose();
bool Verbose();
void ParseCommandLine(int, char **);
void GetDefaults(const aString &defaultsfilename);
void SetAnimation();
bool IsAnimation();
int GetFileCount();
int NProcs();
int SleepTime();
const aString &GetComlineFilename(int index);
FileType GetDefaultFileType();
bool GivenBox();
bool GivenBoxSlice();
bool GivenFilename();
int GetFileCount();
bool MakeSWFData();
bool UseSpecifiedMinMax();
void SetSpecifiedMinMax(Real  specifiedmin, Real  specifiedmax);
void GetSpecifiedMinMax(Real &specifiedmin, Real &specifiedmax);
int CRRBetweenLevels(int fromlevel, int tolevel, const Array<int> &refratios);
int DetermineMaxAllowableLevel(const Box &finebox, int finestlevel,
			       int maxpoints, const Array<int> &refratios);

void SetBoundaryWidth(int width);
int  GetBoundaryWidth();
void SetSkipPltLines(int nlines);
int  GetSkipPltLines();
bool UseMaxLevel();
int  GetMaxLevel();
void SetBoxColor(int boxcolor);
int  GetBoxColor();

void SetInitialDerived(const aString &formatString);
const aString &GetInitialDerived();
void SetFormatString(const aString &formatString);
const aString &GetFormatString();

bool DumpSlices();
bool SliceAllVars();
Array< List<int> > &GetDumpSlices();

#define SHOWVAL(val) { cout << #val << " = " << val << endl; }  // Jensens device
#define VSHOWVAL(verbose, val) { if(verbose) { \
		 cout << #val << " = " << val << endl; } }

#endif
