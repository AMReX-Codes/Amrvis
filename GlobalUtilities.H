//BL_COPYRIGHT_NOTICE

//
// $Id: GlobalUtilities.H,v 1.10 1998-10-29 23:56:08 vince Exp $
//

// ---------------------------------------------------------------
// GlobalUtilities.H
// ---------------------------------------------------------------
#ifndef _GLOBALUTILITIES_H_
#define _GLOBALUTILITIES_H_

#include "AmrvisConstants.H"
#include "Box.H"
#include "Array.H"
#include "aString.H"
#include "List.H"

Box GetBoxFromCommandLine();
void SetMaxPictureSize(int size);
int  MaxPictureSize();
int  MaxPaletteIndex();
void SetVerbose();
bool Verbose();
void ParseCommandLine(int, char **);
void GetDefaults(const aString &defaultsfilename);
void SetAnimation();
bool IsAnimation();
int GetFileCount();
int SleepTime();
const aString &GetComlineFilename(int index);
FileType GetDefaultFileType();
bool GivenBox();
bool GivenBoxSlice();
bool GivenFilename();
int GetFileCount();
bool MakeSWFData();
bool MakeSWFLight();
bool LowBlack();
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
const aString &GetPaletteName();

bool DumpSlices();
bool SliceAllVars();
Array< List<int> > &GetDumpSlices();
int  GetFabOutFormat();

#define SHOWVAL(val) { cout << #val << " = " << val << endl; }  // Jensens device
#define VSHOWVAL(verbose, val) { if(verbose) { \
		 cout << #val << " = " << val << endl; } }

#endif
