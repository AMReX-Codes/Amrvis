// ---------------------------------------------------------------
// GlobalUtilities.H
// ---------------------------------------------------------------
#ifndef _GLOBALUTILITIES_H_
#define _GLOBALUTILITIES_H_

#include <AMReX_AmrvisConstants.H>
#include <AMReX_Box.H>
#include <AMReX_Vector.H>

#include <list>
#include <string>
using std::list;
using std::string;

using amrex::Real;


namespace AVGlobals {

  enum ENUserVectorNames { enUserVelocities, enUserMomentums, enUserNone };

  const ENUserVectorNames &GivenUserVectorNames();
  const amrex::Vector<string> &UserVectorNames();

  amrex::Box GetBoxFromCommandLine();
  void SetMaxPictureSize(int size);
  int  MaxPictureSize();
  int  MaxPaletteIndex();
  void SetVerbose();
  bool Verbose();
  void ParseCommandLine(int, char **);
  void GetDefaults(const string &defaultsfilename);
  bool ReadLightingFile(const string &lightdefaultsfilename,
		        Real &ambientDef, Real &diffuseDef, Real &specularDef,
		        Real &shinyDef,
		        Real &minRayOpacityDef, Real &maxRayOpacityDef);
  bool WriteLightingFile(const string &lightdefaultsfilename,
		         const Real &ambientDef, const Real &diffuseDef,
		         const Real &specularDef, const Real &shinyDef,
		         const Real &minRayOpacityDef,
			 const Real &maxRayOpacityDef);
  void SetAnimation();
  bool IsAnimation();
  void SetAnnotated();
  bool IsAnnotated();
  bool CacheAnimFrames();
  void SetSGIrgbFile();
  void ClearSGIrgbFile();
  bool IsSGIrgbFile();
  int GetFileCount();
  int SleepTime();
  const string &GetComlineFilename(int index);
  string StripSlashes(const string &inString);
  amrex::Amrvis::FileType GetDefaultFileType();
  bool GivenBox();
  bool GivenBoxSlice();
  bool GivenFilename();
  int GetFileCount();
  bool CreateSWFData();
  bool MakeSWFLight();
  bool StartWithValueModel();
  bool LowBlack();
  bool UseSpecifiedMinMax();
  void SetSpecifiedMinMax(Real  specifiedmin, Real  specifiedmax);
  void GetSpecifiedMinMax(Real &specifiedmin, Real &specifiedmax);
  int DetermineMaxAllowableLevel(const amrex::Box &finebox, int finestlevel,
			         int maxpoints, const amrex::Vector<int> &refratios);

  void SetBoundaryWidth(int width);
  int  GetBoundaryWidth();
  void SetSkipPltLines(int nlines);
  int  GetSkipPltLines();
  bool UseMaxLevel();
  int  GetMaxLevel();
  void SetBoxColor(int boxcolor);
  int  GetBoxColor();

  void SetInitialDerived(const string &formatString);
  const string &GetInitialDerived();
  const string &GetPaletteName();
  const string &GetLightingFileName();

  bool DumpSlices();
  bool SliceAllVars();
  amrex::Vector< list<int> > &GetDumpSlices();
  int  GetFabOutFormat();
  bool GivenInitialPlanes();
  amrex::IntVect GetInitialPlanes();
  bool GivenInitialPlanesReal();
  amrex::Vector< Real > GetInitialPlanesReal();
  bool IsProfDirName(const std::string &pdname);

  // -------------------- cartGrid functions
  void SetShowBody(const bool btf);
  bool GetShowBody();
  Real GetBodyOpacity();

  #define SHOWVAL(val) { cout << #val << " = " << val << endl; }  // Jensens device
  #define VSHOWVAL(verbose, val) { if(verbose) { \
		   cout << #val << " = " << val << endl; } }

}  // end namespace AVGlobals
#endif
