// -------------------------------------------------------------------
// DataServices.H
// -------------------------------------------------------------------
#ifndef _DATASERVICES_H_
#define _DATASERVICES_H_

#include "Array.H"
#include "aString.H"
#include "AmrData.H"

#ifdef BL_USE_NEW_HFILES
#include <cstdarg>
#else
#include <stdarg.h>
#endif



class DataServices {

  public:

    enum DSRequestType {
      InvalidRequestType = 0,
      ExitRequest,
      NewRequest,
      DeleteRequest,
      FillVarOneFab,
      FillVarArrayOfFabs,
      WriteFabOneVar,
      WriteFabAllVars,
      DumpSlicePlaneOneVar,
      DumpSlicePlaneAllVars,
      DumpSliceBoxOneVar,
      DumpSliceBoxAllVars,
      MinMaxRequest,
      PointValueRequest
    };

    DataServices(const aString &filename, const FileType &filetype);
    ~DataServices();

    static void SetBatchMode();
    static void Dispatch(DSRequestType requestType, DataServices *ds, ...);


  private:

    // Dispatchable functions

      // FillVarOneFab
      bool FillVar(FArrayBox *destFab, const Box &destBox,
                   int finestFillLevel, const aString &varname,
                   int procWithFab);

      // FillVarArrayOfFabs
      //FillVar(Array<FArrayBox *> &destFabs, const Array<Box> &destBoxes,
              //int finestFillLevel, const aString &varname,
              //int procWithFab);

      // WriteFabOneVar:  write a single variable to one fab
      bool WriteFab(const aString &fabfilename, const Box &region, int lev,
                    const aString &varname);

      // WriteFabAllVars:  write all vars in one fab
      bool WriteFab(const aString &fabfilename, const Box &region, int lev);

      // DumpSlicePlaneOneVar
      bool DumpSlice(int slicedir, int slicenum, const aString &varname);

      // DumpSlicePlaneAllVars
      bool DumpSlice(int slicedir, int slicenum);

      // DumpSliceBoxOneVar
      bool DumpSlice(const Box &slicebox, const aString &varname);

      // DumpSliceBoxAllVars
      bool DumpSlice(const Box &slicebox);

      // MinMaxRequest
      bool MinMax(const Box &onBox, const aString &derived, int level,
                  Real &dataMin, Real &dataMax, bool &minMaxValid);

      // PointValueRequest
      void PointValue(int pointBoxArraySize, Box *pointBoxArray,
		      const aString &currentDerived,
		      int coarsestLevelToSearch, int finestLevelToSearch,
		      int &intersectedLevel, Box &intersectedGrid,
		      Real &dataPointValue, bool &bPointIsValid);


  public:

    // information functions
    const aString &GetFileName() const { return fileName; }
    FileType GetFileType()       const { return fileType; }
    bool CanDerive(const aString &name) const;
    int  NumDeriveFunc() const;
    void ListDeriveFunc(ostream &os) const;
    const Array<aString> &PlotVarNames() const { return amrData.PlotVarNames(); }
    AmrData &AmrDataRef() { return amrData; }
    bool AmrDataOk() { return bAmrDataOk; }  

    // other functions
    int GetNumberOfUsers() const  { return numberOfUsers; }
    void IncrementNumberOfUsers() { ++numberOfUsers; }
    void DecrementNumberOfUsers() { --numberOfUsers; }


  private:
    const aString  fileName;
    const FileType fileType;
    AmrData amrData;
    bool bAmrDataOk;
    int dsArrayIndex;
    int numberOfUsers;

    static Array<DataServices *> dsArray;
    static int dsArrayIndexCounter;
    static bool dsBatchMode;
};

#endif
