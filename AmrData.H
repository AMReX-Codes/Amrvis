// -------------------------------------------------------------------
// AmrData.H
// -------------------------------------------------------------------
#ifndef _AMRDATA_H_
#define _AMRDATA_H_

#include "AmrvisConstants.H"
#include "List.H"
#include "Array.H"
#include "MultiFab.H"
#include "aString.H"
#include "Boolean.H"
#include "VisMF.H"


class AmrData {

  protected: 
	// List of grids at each level, level 0 being coarsest.
	Array<Array<MultiFab *> > dataGrids;    // [level][component]
	Array<Array<Array<bool> > > dataGridsDefined;  // [level][component][index]
	Array<VisMF *> visMF;    // [level]

	int finestLevel;   // most refined level available
	Array<int> refRatio; // refinement ratio between level L and L+1
	Array< Array<Real> > dxLevel;  // [level][dim];   // grid spacing
	Array<Box>  probDomain;
	Array<Box>  maxDomain;        // max problem domain size
	Real probSize[BL_SPACEDIM];
	Real probLo[BL_SPACEDIM];
	Real probHi[BL_SPACEDIM];
	Real time;
	Array<aString> plotVars;
	//int numberOfUsers;
	int nComp;
	int nRegions;         // number of boundary regions
	int boundaryWidth;    // number of zones in the boundary regions
	Array< Array< FArrayBox *> > regions; // [lev][nReg]  ptrs to bndry data
	
	FileType fileType;

  public:
	AmrData();
	~AmrData();

	bool ReadData(const aString &filename, FileType filetype);
	bool ReadNonPlotfileData(const aString &filename, FileType filetype);

	const Array<aString> &PlotVarNames() const { return plotVars; }

	//int GetNumberOfUsers() const ;
	//void IncrementNumberOfUsers();
	//void DecrementNumberOfUsers();

        int FinestLevel() const		{ return finestLevel; }
        const Array<int> &RefRatio() const      { return refRatio; }
        const BoxArray &boxArray(int level) const
				   { return visMF[level]->boxArray(); }

	// limits of interior region of computational domain at each level
        const Array<Box> &ProbDomain() const { return probDomain; }
	// physical size of computational domain
        Real ProbSize(int dir) const	{ return probSize[dir]; }
        Real ProbLo(int dir) const	{ return probLo[dir]; }
        Real ProbHi(int dir) const	{ return probHi[dir]; }

        // return physical location of cell at index ix, level lev
        // cellLoc   = location of cell center
	// loNodeLoc = location of node (vertex) on lower left hand corner
	// hiNodeLoc = location of node (vertex) on upper right hand corner
        void  CellLoc(int lev,   IntVect ix, Array<Real> &pos) const;
        void  LoNodeLoc(int lev, IntVect ix, Array<Real> &pos) const;
        void  HiNodeLoc(int lev, IntVect ix, Array<Real> &pos) const;

        int NComp() const		{ return nComp; }
        int BoundaryWidth() const	{ return boundaryWidth; }
	
        // fill a databox using conservative interpolation
	void FillPatch(FArrayBox &dest, int level, const Box &subbox);
	void FillVar(FArrayBox &dest, int level, const aString &varName);
	void FillVar(FArrayBox *destFab, const Box &destBox,
		     int finestFillLevel, const aString &varName, int procWithFabs);
	void FillVar(Array<FArrayBox *> &destFabs, const Array<Box> &destBoxes,
		     int finestFillLevel, const aString &varName, int procWithFabs);

	const aString &GetFileName() const { return fileName; }

	void SetFileType(FileType type);
	FileType GetFileType() const     { return fileType; }
	void SetBoundaryWidth(int width) { boundaryWidth = width; }

      bool CanDerive(const aString &name) const;
      int  NumDeriveFunc() const;
      void ListDeriveFunc(ostream &os) const;
      int  StateNumber(const aString &statename) const;

      // return the finest level <= startLevel that fully contains box b
      // b is defined on startLevel
      int FinestContainingLevel(const Box &b, int startLevel) const;

      // number of grids at level which intersect b
      int NIntersectingGrids(int level, const Box &b) const;
      MultiFab &GetGrids(int level, int componentIndex);
      MultiFab &GetGrids(int level, int componentIndex, const Box &onBox);

      // calculate the min and max values of derived on onBox at level
      // return false if onBox did not intersect any grids
      bool MinMax(const Box &onBox, const aString &derived, int level,
		  Real &dataMin, Real &dataMax);

  private:
	aString fileName;
	static FileType defaultFileType;

        // fill on interior by piecewise constant interpolation
        void FillInterior(FArrayBox &dest, int level, const Box &subbox);
	void Interp(FArrayBox &fine, FArrayBox &crse,
		    const Box &fine_box, int lrat);
        void PcInterp(FArrayBox &fine, const FArrayBox &crse,
		      const Box &subbox, int lrat);
	FArrayBox *ReadGrid(istream &is, int numVar);
        bool DefineFab(int level, int componentIndex, int fabIndex);
};

#endif
