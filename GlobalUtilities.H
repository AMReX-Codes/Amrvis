
//
// $Id: GlobalUtilities.H,v 1.13 2001-05-10 23:38:00 vince Exp $
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
bool ReadLightingFile(const aString &lightdefaultsfilename,
		      Real &ambientDef, Real &diffuseDef, Real &specularDef,
		      Real &shinyDef,
		      Real &minRayOpacityDef, Real &maxRayOpacityDef);
bool WriteLightingFile(const aString &lightdefaultsfilename,
		       const Real &ambientDef, const Real &diffuseDef,
		       const Real &specularDef, const Real &shinyDef,
		       const Real &minRayOpacityDef, const Real &maxRayOpacityDef);
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
const aString &GetLightingFileName();

bool DumpSlices();
bool SliceAllVars();
Array< List<int> > &GetDumpSlices();
int  GetFabOutFormat();

#define SHOWVAL(val) { cout << #val << " = " << val << endl; }  // Jensens device
#define VSHOWVAL(verbose, val) { if(verbose) { \
		 cout << #val << " = " << val << endl; } }

#endif
