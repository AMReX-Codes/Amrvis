//BL_COPYRIGHT_NOTICE

//
// $Id: DataServices.cpp,v 1.20 1999-05-10 17:18:42 car Exp $
//

// ---------------------------------------------------------------
// DataServices.cpp
// ---------------------------------------------------------------
#include "AmrvisConstants.H"
#include "DataServices.H"
#include "ParallelDescriptor.H"

#ifdef BL_USE_NEW_HFILES
#include <iostream>
#include <cstdio>
using std::ios;
#else
#include <iostream.h>
#include <stdio.h>
#endif

Array<DataServices *> DataServices::dsArray;
int DataServices::dsArrayIndexCounter = 0;
int DataServices::dsFabOutSize = 0;
bool DataServices::dsBatchMode = false;

// ---------------------------------------------------------------
DataServices::DataServices(const aString &filename, const FileType &filetype)
             : fileName(filename), fileType(filetype), bAmrDataOk(false)
{
  numberOfUsers = 0;  // the user must do all incrementing and decrementing

  bAmrDataOk = amrData.ReadData(fileName, fileType);

  if(bAmrDataOk) {
    dsArrayIndex = DataServices::dsArrayIndexCounter;
    ++DataServices::dsArrayIndexCounter;
    DataServices::dsArray.resize(DataServices::dsArrayIndexCounter);
    DataServices::dsArray[dsArrayIndex] = this;
  }
}


// ---------------------------------------------------------------
DataServices::~DataServices() {
  BLassert(numberOfUsers == 0);
  DataServices::dsArray[dsArrayIndex] = NULL;
}


// ---------------------------------------------------------------
void DataServices::SetBatchMode() {
  dsBatchMode = true;
}


// ---------------------------------------------------------------
void DataServices::SetFabOutSize(int iSize) {
  if(iSize == 8 || iSize == 32) {
    dsFabOutSize = iSize;
  } else {
    cerr << "Warning:  DataServices::SetFabOutSize:  size must be 8 or 32 only."
	 << "  Defaulting to native." << endl;
    dsFabOutSize = 0;
  }
}


// ---------------------------------------------------------------
void DataServices::Dispatch(DSRequestType requestType, DataServices *ds, ...) {
  bool bContinueLooping(true);
  va_list ap;
  int whichDSIndex;
  int ioProcNumber(ParallelDescriptor::IOProcessorNumber());

 while(bContinueLooping) {
  if(ParallelDescriptor::IOProcessor() || dsBatchMode) {
    bContinueLooping = false;
  }

  ParallelDescriptor::Barrier();  // procs 1 - N wait here

  ParallelDescriptor::Broadcast(ioProcNumber, &requestType, &requestType,
				sizeof(DSRequestType));

  // handle new request
  if(requestType == NewRequest) {
    // broadcast the fileName and fileType to procs 1 - N
    char *fileNameCharPtr;
    int   fileNameLength, fileNameLengthPadded, checkArrayIndex;
    FileType newFileType;

    if(ParallelDescriptor::IOProcessor()) {
      fileNameLength = ds->fileName.length();
      newFileType = ds->fileType;
      checkArrayIndex = ds->dsArrayIndex;
    }

    ParallelDescriptor::Broadcast(ioProcNumber, &fileNameLength,  &fileNameLength,
				  sizeof(int));
    ParallelDescriptor::Broadcast(ioProcNumber, &newFileType,     &newFileType,
				  sizeof(FileType));
    ParallelDescriptor::Broadcast(ioProcNumber, &checkArrayIndex, &checkArrayIndex,
				  sizeof(int));

    fileNameLengthPadded = fileNameLength + 1;    // for the null
    fileNameLengthPadded += fileNameLengthPadded % 8;  // for alignment on the t3e
    fileNameCharPtr = new char[fileNameLengthPadded];
    if(ParallelDescriptor::IOProcessor()) {
      strcpy(fileNameCharPtr, ds->fileName.c_str());
    }

    ParallelDescriptor::Broadcast(ioProcNumber, fileNameCharPtr, fileNameCharPtr,
                                  fileNameLengthPadded);

    aString newFileName(fileNameCharPtr);
    delete [] fileNameCharPtr;

    // make a new DataServices for procs 1 - N
    if( ! ParallelDescriptor::IOProcessor()) {
      ds = new DataServices(newFileName, newFileType);
    }

    // verify dsArrayIndex is correct
    BLassert(ds->dsArrayIndex == checkArrayIndex);

    continue;  // go to the top of the while loop
  }  // end NewRequest


  // handle exit request
  if(requestType == ExitRequest) {                // cleanup memory
    for(int i = 0; i < dsArray.length(); ++i) {
      if(DataServices::dsArray[i] != NULL) {
	BLassert(DataServices::dsArray[i]->numberOfUsers == 0);
	delete DataServices::dsArray[i];
      }
    }
    ParallelDescriptor::EndParallel();
    exit(0);
  }  // end ExitRequest

  if(ParallelDescriptor::IOProcessor()) {
    va_start(ap, ds);
    whichDSIndex = ds->dsArrayIndex;
  }

  ParallelDescriptor::Broadcast(ioProcNumber, &whichDSIndex, &whichDSIndex,
				sizeof(int));
  if( ! ParallelDescriptor::IOProcessor()) {
    ds = DataServices::dsArray[whichDSIndex];
  }
  BLassert(ds != NULL);

  switch(requestType) {
    case DeleteRequest:
    {
      bool bDeleteDS(false);
      BLassert(DataServices::dsArray[whichDSIndex]->numberOfUsers >= 0);
      if(ParallelDescriptor::IOProcessor()) {
	bDeleteDS = (DataServices::dsArray[whichDSIndex]->numberOfUsers == 0);
      }
      ParallelDescriptor::Broadcast(ioProcNumber, &bDeleteDS, &bDeleteDS,
				    sizeof(bool));
      if(bDeleteDS) {
        delete DataServices::dsArray[whichDSIndex];
      }
    }
    break;

    case FillVarOneFab:
    {
      FArrayBox *destFab = NULL;
      Box destBox;
      int fineFillLevel;
      aString derivedTemp;
      char *derivedCharPtr;
      int derivedLength, derivedLengthPadded;

      if(ParallelDescriptor::IOProcessor()) {
	destFab = (FArrayBox *) va_arg(ap, void *);
        const Box *boxRef = (const Box *) va_arg(ap, void *);
	destBox = *boxRef;
        fineFillLevel = va_arg(ap, int);
        const aString *derivedRef = (const aString *) va_arg(ap, void *);
        derivedTemp = *derivedRef;
	derivedLength = derivedTemp.length();
      }

      ParallelDescriptor::Broadcast(ioProcNumber, &destBox, &destBox, sizeof(Box));
      ParallelDescriptor::Broadcast(ioProcNumber, &fineFillLevel, &fineFillLevel,
				    sizeof(int));
      ParallelDescriptor::Broadcast(ioProcNumber, &derivedLength, &derivedLength,
				    sizeof(int));

      derivedLengthPadded = derivedLength + 1;
      derivedLengthPadded += derivedLengthPadded % 8;
      derivedCharPtr = new char[derivedLengthPadded];
      if(ParallelDescriptor::IOProcessor()) {
        strcpy(derivedCharPtr, derivedTemp.c_str());
      }

      ParallelDescriptor::Broadcast(ioProcNumber, derivedCharPtr, derivedCharPtr,
				    derivedLengthPadded);

      aString derived(derivedCharPtr);
      delete [] derivedCharPtr;

      ds->FillVar(destFab, destBox, fineFillLevel, derived, ioProcNumber);

    }
    break;

    case FillVarArrayOfFabs:
    {
      BoxLib::Abort("FillVarArrayOfFabs not implemented yet.");
    }
    break;

    case FillVarMultiFab:
    {
      BoxLib::Abort("FillVarMultiFab not implemented yet.");
    }
    break;

    case WriteFabOneVar:
    {
      // interface: (requestType, dsPtr, fabFileName, box, maxLevel, derivedName)
      Box destBox;
      int fineFillLevel;
      aString fabFileName;
      aString derivedTemp;
      char *derivedCharPtr;
      int derivedLength, derivedLengthPadded;

      if(ParallelDescriptor::IOProcessor()) {
        const aString *fabFileNameRef = (const aString *) va_arg(ap, void *);
	fabFileName = *fabFileNameRef;
        const Box *boxRef = (const Box *) va_arg(ap, void *);
	destBox = *boxRef;
        fineFillLevel = va_arg(ap, int);
        const aString *derivedRef = (const aString *) va_arg(ap, void *);
        derivedTemp = *derivedRef;
	derivedLength = derivedTemp.length();
      }

      ParallelDescriptor::Broadcast(ioProcNumber, &destBox, &destBox, sizeof(Box));
      ParallelDescriptor::Broadcast(ioProcNumber, &fineFillLevel, &fineFillLevel,
				    sizeof(int));
      ParallelDescriptor::Broadcast(ioProcNumber, &derivedLength, &derivedLength,
				    sizeof(int));

      derivedLengthPadded = derivedLength + 1;
      derivedLengthPadded += derivedLengthPadded % 8;
      derivedCharPtr = new char[derivedLengthPadded];
      if(ParallelDescriptor::IOProcessor()) {
        strcpy(derivedCharPtr, derivedTemp.c_str());
      }

      ParallelDescriptor::Broadcast(ioProcNumber, derivedCharPtr, derivedCharPtr,
				    derivedLengthPadded);

      aString derived(derivedCharPtr);
      delete [] derivedCharPtr;

      ds->WriteFab(fabFileName, destBox, fineFillLevel, derived);

    }
    break;

    case WriteFabAllVars:
    {
      // interface: (requestType, dsPtr, fabFileName, box, maxLevel)
      Box destBox;
      int fineFillLevel;
      aString fabFileName;

      if(ParallelDescriptor::IOProcessor()) {
        const aString *fabFileNameRef = (const aString *) va_arg(ap, void *);
	fabFileName = *fabFileNameRef;
        const Box *boxRef = (const Box *) va_arg(ap, void *);
	destBox = *boxRef;
        fineFillLevel = va_arg(ap, int);
      }

      ParallelDescriptor::Broadcast(ioProcNumber, &destBox, &destBox, sizeof(Box));
      ParallelDescriptor::Broadcast(ioProcNumber, &fineFillLevel, &fineFillLevel,
				    sizeof(int));

      ds->WriteFab(fabFileName, destBox, fineFillLevel);
    }
    break;

    case DumpSlicePlaneOneVar:
    {
      int slicedir;
      int slicenum;
      aString derivedTemp;
      char *derivedCharPtr;
      int derivedLength, derivedLengthPadded;
      if(ParallelDescriptor::IOProcessor()) {
        slicedir = va_arg(ap, int);
        slicenum = va_arg(ap, int);
        const aString *derivedRef = (const aString *) va_arg(ap, void *);
        derivedTemp = *derivedRef;
	derivedLength = derivedTemp.length();
      }

      ParallelDescriptor::Broadcast(ioProcNumber, &slicedir, &slicedir,
				    sizeof(int));
      ParallelDescriptor::Broadcast(ioProcNumber, &slicenum, &slicenum,
				    sizeof(int));
      ParallelDescriptor::Broadcast(ioProcNumber, &derivedLength, &derivedLength,
				    sizeof(int));

      derivedLengthPadded = derivedLength + 1;
      derivedLengthPadded += derivedLengthPadded % 8;
      derivedCharPtr = new char[derivedLengthPadded];
      if(ParallelDescriptor::IOProcessor()) {
        strcpy(derivedCharPtr, derivedTemp.c_str());
      }
      ParallelDescriptor::Broadcast(ioProcNumber, derivedCharPtr, derivedCharPtr,
				    derivedLengthPadded);

      aString derived(derivedCharPtr);
      delete [] derivedCharPtr;

      ds->DumpSlice(slicedir, slicenum, derived);

    }
    break;

    case DumpSlicePlaneAllVars:
    {
      int slicedir;
      int slicenum;
      if(ParallelDescriptor::IOProcessor()) {
        slicedir = va_arg(ap, int);
        slicenum = va_arg(ap, int);
      }
      ParallelDescriptor::Broadcast(ioProcNumber, &slicedir, &slicedir,
				    sizeof(int));
      ParallelDescriptor::Broadcast(ioProcNumber, &slicenum, &slicenum,
				    sizeof(int));
      ds->DumpSlice(slicedir, slicenum);

    }
    break;

    case DumpSliceBoxOneVar:
    {
      Box box;
      aString derivedTemp;
      char *derivedCharPtr;
      int derivedLength, derivedLengthPadded;
      if(ParallelDescriptor::IOProcessor()) {
        const Box *boxRef = (const Box *) va_arg(ap, void *);
	box = *boxRef;
        const aString *derivedRef = (const aString *) va_arg(ap, void *);
	derivedTemp = *derivedRef;
	derivedLength = derivedTemp.length();
      }

      ParallelDescriptor::Broadcast(ioProcNumber, &box, &box, sizeof(Box));
      ParallelDescriptor::Broadcast(ioProcNumber, &derivedLength, &derivedLength,
				    sizeof(int));

      derivedLengthPadded = derivedLength + 1;
      derivedLengthPadded += derivedLengthPadded % 8;
      derivedCharPtr = new char[derivedLengthPadded];
      if(ParallelDescriptor::IOProcessor()) {
        strcpy(derivedCharPtr, derivedTemp.c_str());
      }
      ParallelDescriptor::Broadcast(ioProcNumber, derivedCharPtr, derivedCharPtr,
				    derivedLengthPadded);

      aString derived(derivedCharPtr);
      delete [] derivedCharPtr;

      ds->DumpSlice(box, derived);
    }
    break;

    case DumpSliceBoxAllVars:
    {
      Box box;
      if(ParallelDescriptor::IOProcessor()) {
        const Box *boxRef = (const Box *) va_arg(ap, void *);
	box = *boxRef;
      }

      ParallelDescriptor::Broadcast(ioProcNumber, &box, &box, sizeof(Box));

      ds->DumpSlice(box);

    }
    break;

    case MinMaxRequest:
    {
      Box box;
      aString derivedTemp;
      char *derivedCharPtr;
      int level;
      int derivedLength, derivedLengthPadded;
      Real dataMin, dataMax;
      bool minMaxValid;
      if(ParallelDescriptor::IOProcessor()) {
        const Box *boxRef = (const Box *) va_arg(ap, void *);
        const aString *derivedRef = (const aString *) va_arg(ap, void *);
        level = va_arg(ap, int);
	box = *boxRef;
	derivedTemp = *derivedRef;
	derivedLength = derivedTemp.length();
      }

      ParallelDescriptor::Broadcast(ioProcNumber, &box, &box, sizeof(Box));
      ParallelDescriptor::Broadcast(ioProcNumber, &derivedLength, &derivedLength,
				    sizeof(int));
      ParallelDescriptor::Broadcast(ioProcNumber, &level, &level, sizeof(int));

      derivedLengthPadded = derivedLength + 1;
      derivedLengthPadded += derivedLengthPadded % 8;
      derivedCharPtr = new char[derivedLengthPadded];
      if(ParallelDescriptor::IOProcessor()) {
        strcpy(derivedCharPtr, derivedTemp.c_str());
      }
      ParallelDescriptor::Broadcast(ioProcNumber, derivedCharPtr, derivedCharPtr,
				    derivedLengthPadded);

      aString derived(derivedCharPtr);
      delete [] derivedCharPtr;

      ds->MinMax(box, derived, level, dataMin, dataMax, minMaxValid);

      // set the return values
      if(ParallelDescriptor::IOProcessor()) {
        Real *dataMinRef = va_arg(ap, Real *);
        Real *dataMaxRef = va_arg(ap, Real *);
        bool *minMaxValidRef = va_arg(ap, bool *);
	*dataMinRef = dataMin;
	*dataMaxRef = dataMax;
	*minMaxValidRef = minMaxValid;
      }
    }
    break;

    case PointValueRequest:
    {
      // interface: (requestType, dsPtr,
      //             pointBoxArraySize, pointBoxArray *,
      //             derivedName,
      //             coarsestLevelToSearch, finestLevelToSearch,
      //             intersectedLevel,  /* return this value */
      //             intersectedBox,    /* return this value */
      //             dataPointValue,    /* return this value */
      //             bPointIsValid)     /* return this value */

      // need to broadcast pointBoxArraySize, pointBoxArray, derivedName,
      // coarsestLevelToSearch, and finestLevelToSearch

      int pointBoxArraySize;
      Box *pointBoxArrayPtr, *pointBoxArrayTempPtr;
      int coarsestLevelToSearch, finestLevelToSearch;

      aString derivedTemp;
      char *derivedCharPtr;
      int derivedLength, derivedLengthPadded;

      if(ParallelDescriptor::IOProcessor()) {
        pointBoxArraySize = va_arg(ap, int);
        pointBoxArrayTempPtr = (Box *) va_arg(ap, void *);
        const aString *derivedRef = (const aString *) va_arg(ap, void *);
        derivedTemp = *derivedRef;
	derivedLength = derivedTemp.length();
        coarsestLevelToSearch = va_arg(ap, int);
        finestLevelToSearch   = va_arg(ap, int);
      }

      ParallelDescriptor::Broadcast(ioProcNumber, &pointBoxArraySize,
						  &pointBoxArraySize,
						  sizeof(int));
      ParallelDescriptor::Broadcast(ioProcNumber, &derivedLength, &derivedLength,
                                    sizeof(int));
      ParallelDescriptor::Broadcast(ioProcNumber, &coarsestLevelToSearch,
						  &coarsestLevelToSearch,
						  sizeof(int));
      ParallelDescriptor::Broadcast(ioProcNumber, &finestLevelToSearch,
						  &finestLevelToSearch,
						  sizeof(int));
      pointBoxArrayPtr = new Box[pointBoxArraySize];

      derivedLengthPadded = derivedLength + 1;
      derivedLengthPadded += derivedLengthPadded % 8;
      derivedCharPtr = new char[derivedLengthPadded];
      if(ParallelDescriptor::IOProcessor()) {
        strcpy(derivedCharPtr, derivedTemp.c_str());
	for(int iBox = 0; iBox < pointBoxArraySize; ++iBox) {
	  pointBoxArrayPtr[iBox] = pointBoxArrayTempPtr[iBox];
	}
      }
      ParallelDescriptor::Broadcast(ioProcNumber, derivedCharPtr, derivedCharPtr,
				    derivedLengthPadded);
      ParallelDescriptor::Broadcast(ioProcNumber,
				    pointBoxArrayPtr, pointBoxArrayPtr,
				    pointBoxArraySize * sizeof(Box));

      aString derived(derivedCharPtr);
      delete [] derivedCharPtr;

      // return values
      int intersectedLevel;
      Box intersectedBox;
      Real dataPointValue;
      bool bPointIsValid;

      ds->PointValue(pointBoxArraySize, pointBoxArrayPtr,
		     derived,
		     coarsestLevelToSearch,
		     finestLevelToSearch,
		     intersectedLevel,
		     intersectedBox,
		     dataPointValue,
		     bPointIsValid);

      // set the return values
      if(ParallelDescriptor::IOProcessor()) {
        int *intersectedLevelRef = va_arg(ap, int *);
        Box *intersectedBoxRef   = (Box *) va_arg(ap, void *);
        Real *dataPointValueRef  = va_arg(ap, Real *);
        bool *bPointIsValidRef   = va_arg(ap, bool *);
	*intersectedLevelRef     = intersectedLevel;
	*intersectedBoxRef       = intersectedBox;
	*dataPointValueRef       = dataPointValue;
	*bPointIsValidRef        = bPointIsValid;
      }

      // dont need to broadcast the return values--only the IOProcessor uses them

      delete [] pointBoxArrayPtr;
    }
    break;

  }  // end switch

  if(ParallelDescriptor::IOProcessor()) {
    va_end(ap);
  }

 }  // end while(bContinueLooping)

  return;
}  // end Dispatch


// ---------------------------------------------------------------
bool DataServices::DumpSlice(int slicedir, int slicenum, const aString &varname)
{
  if( ! bAmrDataOk) {
    return false;
  }
  aString sliceFile = fileName;
  sliceFile += ".";
  sliceFile += varname;
  sliceFile += ".";
  if(slicedir == XDIR) {
    sliceFile += "xslice";
  } else if(slicedir == YDIR) {
    sliceFile += "yslice";
  } else if(slicedir == ZDIR) {
    sliceFile += "zslice";
  } else {
    cerr << "bad slicedir = " << slicedir << endl;
    return false;
  }
  sliceFile += ".";
  char slicechar[16];
  sprintf(slicechar, "%d", slicenum);
  sliceFile += slicechar;
  sliceFile += ".fab";
  cout << "sliceFile = " << sliceFile << endl;

  Box sliceBox(amrData.ProbDomain()[amrData.FinestLevel()]);

  if(BL_SPACEDIM == 2 && slicedir == ZDIR) {
    // use probDomain for the sliceBox
  } else {
    // make the box one cell thick in the slice direction
    sliceBox.setSmall(slicedir, slicenum);
    sliceBox.setBig(slicedir, slicenum);
  }

  cout << "sliceBox  = " << sliceBox << endl;
  cout << endl;
  if( ! amrData.ProbDomain()[amrData.FinestLevel()].contains(sliceBox)) {
    cerr << "Error:  sliceBox = " << sliceBox << "  slicedir " << slicenum
	 << " not in probDomain: " << amrData.ProbDomain()[amrData.FinestLevel()]
	 << endl;
    return false;
  }
  WriteFab(sliceFile, sliceBox, amrData.FinestLevel(), varname);
  return true;
}


// ---------------------------------------------------------------
bool DataServices::DumpSlice(int slicedir, int slicenum) {  // dump all vars
  if( ! bAmrDataOk) {
    return false;
  }
  aString sliceFile = fileName;
  sliceFile += ".";
  if(slicedir == XDIR) {
    sliceFile += "xslice";
  } else if(slicedir == YDIR) {
    sliceFile += "yslice";
  } else if(slicedir == ZDIR) {
    sliceFile += "zslice";
  } else {
    cerr << "bad slicedir = " << slicedir << endl;
    return false;
  }
  sliceFile += ".";
  char slicechar[16];
  sprintf(slicechar, "%d", slicenum);
  sliceFile += slicechar;
  sliceFile += ".fab";
  cout << "sliceFile = " << sliceFile << endl;

  Box sliceBox(amrData.ProbDomain()[amrData.FinestLevel()]);

  if(BL_SPACEDIM == 2 && slicedir == ZDIR) {
    // use probDomain for the sliceBox
  } else {
    // make the box one cell thick in the slice direction
    sliceBox.setSmall(slicedir, slicenum);
    sliceBox.setBig(slicedir, slicenum);
  }

  cout << "sliceBox  = " << sliceBox << endl;
  cout << endl;
  if( ! amrData.ProbDomain()[amrData.FinestLevel()].contains(sliceBox)) {
    cerr << "Error:  sliceBox = " << sliceBox << "  slicedir " << slicenum
	 << " not in probDomain: " << amrData.ProbDomain()[amrData.FinestLevel()]
	 << endl;
    return false;
  }
  WriteFab(sliceFile, sliceBox, amrData.FinestLevel());
  return true;
}


// ---------------------------------------------------------------
bool DataServices::DumpSlice(const Box &b, const aString &varname) {
  if( ! bAmrDataOk) {
    return false;
  }
  aString sliceFile = fileName;
  sliceFile += ".";
  sliceFile += varname;
  sliceFile += ".";
  char slicechar[128];
# if (BL_SPACEDIM == 2)
    sprintf(slicechar, "%d.%d.%d.%d",
            b.smallEnd(XDIR), b.smallEnd(YDIR),
            b.bigEnd(XDIR),   b.bigEnd(YDIR));
# else
    sprintf(slicechar, "%d.%d.%d.%d.%d.%d",
            b.smallEnd(XDIR), b.smallEnd(YDIR), b.smallEnd(ZDIR),
            b.bigEnd(XDIR),   b.bigEnd(YDIR),   b.bigEnd(ZDIR));
#endif
  sliceFile += slicechar;
  sliceFile += ".fab";
  cout << "sliceFile = " << sliceFile << endl;

  cout << "sliceBox = " << b << endl;
  cout << endl;
  if( ! amrData.ProbDomain()[amrData.FinestLevel()].contains(b)) {
    cerr << "Slice box not in probDomain: "
	 << amrData.ProbDomain()[amrData.FinestLevel()] << endl;
    return false;
  }
  WriteFab(sliceFile, b, amrData.FinestLevel(), varname);
  return true;
}


// ---------------------------------------------------------------
bool DataServices::DumpSlice(const Box &b) {  // dump all vars
  if( ! bAmrDataOk) {
    return false;
  }
  aString sliceFile = fileName;
  sliceFile += ".";
  char slicechar[128];
# if (BL_SPACEDIM == 2)
    sprintf(slicechar, "%d.%d.%d.%d",
            b.smallEnd(XDIR), b.smallEnd(YDIR),
            b.bigEnd(XDIR),   b.bigEnd(YDIR));
# else
    sprintf(slicechar, "%d.%d.%d.%d.%d.%d",
            b.smallEnd(XDIR), b.smallEnd(YDIR), b.smallEnd(ZDIR),
            b.bigEnd(XDIR),   b.bigEnd(YDIR),   b.bigEnd(ZDIR));
#endif
  sliceFile += slicechar;
  sliceFile += ".fab";
  cout << "sliceFile = " << sliceFile << endl;

  cout << "sliceBox = " << b << endl;
  cout << endl;
  if( ! amrData.ProbDomain()[amrData.FinestLevel()].contains(b)) {
    cerr << "Slice box not in probDomain: "
	 << amrData.ProbDomain()[amrData.FinestLevel()] << endl;
    return false;
  }
  WriteFab(sliceFile, b, amrData.FinestLevel());
  return true;
}



// ---------------------------------------------------------------
bool DataServices::FillVar(FArrayBox *destFab, const Box &destBox,
			   int finestFillLevel, const aString &varname,
			   int procWithFab)
{
  if( ! bAmrDataOk) {
    return false;
  }

  amrData.FillVar(destFab, destBox, finestFillLevel, varname, procWithFab);

  return true;
}  // end FillVar


// ---------------------------------------------------------------
bool DataServices::FillVar(MultiFab &destMultiFab, int finestFillLevel,
			   const aString &varname)
{
  if( ! bAmrDataOk) {
    return false;
  }

  amrData.FillVar(destMultiFab, finestFillLevel, varname);

  return true;
}  // end FillVar


// ---------------------------------------------------------------
//
//
// Change this to take an Array<Box> (or BoxArray?) and create
// a MultiFab and pass the proc number for the multifabs disributionMapping
// to create the FillVared fabs on separate processors
//
//
bool DataServices::WriteFab(const aString &fname, const Box &region, int lev,
		            const aString &varname)
{
  if( ! bAmrDataOk) {
    return false;
  }
  FArrayBox data;
  if(ParallelDescriptor::IOProcessor()) {
    data.resize(region, 1);
  }

  Array<FArrayBox *> destFabs(1);
  Array<Box> destBoxes(1);
  destFabs[0]  = &data;
  destBoxes[0] = region;
  amrData.FillVar(destFabs, destBoxes, lev, varname,
		  ParallelDescriptor::IOProcessorNumber());

  if(ParallelDescriptor::IOProcessor()) {
    FABio::Format oldFabFormat = FArrayBox::getFormat();
    if(dsFabOutSize == 8) {
      FArrayBox::setFormat(FABio::FAB_8BIT);
    }
    if(dsFabOutSize == 32) {
      FArrayBox::setFormat(FABio::FAB_IEEE_32);
    }

    ofstream os;
    os.open(fname.c_str(), ios::out);
    data.writeOn(os);
    os.close();

    FArrayBox::setFormat(oldFabFormat);
  }

  return true;
}  // end WriteFab


// ---------------------------------------------------------------
//
//
// Change this to take an Array<Box> (or BoxArray?) and create
// a MultiFab and pass the proc number for the multifabs disributionMapping
// to create the FillVared fabs on separate processors
//
//
bool DataServices::WriteFab(const aString &fname, const Box &region, int lev) {
  if( ! bAmrDataOk) {
    return false;
  }

  // write all fab vars
  FArrayBox tempdata;
  FArrayBox data;
  if(ParallelDescriptor::IOProcessor()) {
    tempdata.resize(region, 1);
    data.resize(region, amrData.NComp());
  }
  for(int ivar = 0; ivar < amrData.NComp(); ivar++) {
    //amrData.FillVar(tempdata, lev, amrData.PlotVarNames()[ivar]);
    Array<FArrayBox *> destFabs(1);
    Array<Box> destBoxes(1);
    destFabs[0]  = &tempdata;
    destBoxes[0] = region;
    amrData.FillVar(destFabs, destBoxes, lev, amrData.PlotVarNames()[ivar],
		    ParallelDescriptor::IOProcessorNumber());
    int srccomp(0);
    int destcomp(ivar);
    int ncomp(1);
    if(ParallelDescriptor::IOProcessor()) {
      data.copy(tempdata, srccomp, destcomp, ncomp);
    }
  }

  if(ParallelDescriptor::IOProcessor()) {
    FABio::Format oldFabFormat = FArrayBox::getFormat();
    if(dsFabOutSize == 8) {
      FArrayBox::setFormat(FABio::FAB_8BIT);
    }
    if(dsFabOutSize == 32) {
      FArrayBox::setFormat(FABio::FAB_IEEE_32);
    }

    ofstream os;
    os.open(fname.c_str(), ios::out);
    data.writeOn(os);
    os.close();

    FArrayBox::setFormat(oldFabFormat);
  }

  return true;
}  // end WriteFab


// ---------------------------------------------------------------
bool DataServices::CanDerive(const aString &name) const {
  if( ! bAmrDataOk) {
    return false;
  }
  return amrData.CanDerive(name);
}


// ---------------------------------------------------------------
// output the list of variables that can be derived
void DataServices::ListDeriveFunc(ostream &os) const {
  if( ! bAmrDataOk) {
    return;
  }
  amrData.ListDeriveFunc(os);
}


// ---------------------------------------------------------------
int DataServices::NumDeriveFunc() const {
  return amrData.NumDeriveFunc();
}


// ---------------------------------------------------------------
void DataServices::PointValue(int pointBoxArraySize, Box *pointBoxArray,
		              const aString &currentDerived,
		              int coarsestLevelToSearch,
			      int finestLevelToSearch,
		              int &intersectedLevel,
		              Box &intersectedGrid,
			      Real &dataPointValue,
		              bool &bPointIsValid)
{
  bPointIsValid = false;
  if( ! bAmrDataOk) {
    return;
  }

  intersectedLevel =
	  amrData.FinestContainingLevel(pointBoxArray[finestLevelToSearch],
					finestLevelToSearch);

  if(intersectedLevel < coarsestLevelToSearch) {
    return;
  }

  Box destBox(pointBoxArray[intersectedLevel]);
  BLassert(destBox.volume() == 1);

  const BoxArray &intersectedBA = amrData.boxArray(intersectedLevel);
  for(int iGrid = 0; iGrid < intersectedBA.length(); ++iGrid) {
    if(destBox.intersects(intersectedBA[iGrid])) {
      intersectedGrid = intersectedBA[iGrid];
      break;
    }
  }

  FArrayBox *destFab(NULL);
  if(ParallelDescriptor::IOProcessor()) {
    destFab = new FArrayBox(destBox, 1);
  }
  amrData.FillVar(destFab, destBox, intersectedLevel, currentDerived,
		  ParallelDescriptor::IOProcessorNumber());

  if(ParallelDescriptor::IOProcessor()) {
    dataPointValue = (destFab->dataPtr())[0];
    delete destFab;
  }
  bPointIsValid = true;

}  // end PointValue


// ---------------------------------------------------------------
bool DataServices::MinMax(const Box &onBox, const aString &derived, int level,
		          Real &dataMin, Real &dataMax, bool &minMaxValid)
{
  minMaxValid =  amrData.MinMax(onBox, derived, level, dataMin, dataMax);
  return minMaxValid;
}  // end MinMax

// ---------------------------------------------------------------
// ---------------------------------------------------------------
