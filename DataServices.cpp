// ---------------------------------------------------------------
// DataServices.cpp
// ---------------------------------------------------------------
#include "AmrvisConstants.H"
#include "DataServices.H"
#include "GlobalUtilities.H"
#include "ParallelDescriptor.H"

Array<DataServices *> DataServices::dsArray;
int DataServices::dsArrayIndexCounter = 0;

// ---------------------------------------------------------------
DataServices::DataServices(const aString &filename, const FileType &filetype)
             : fileName(filename), fileType(filetype), bAmrDataOk(false)
{
  bAmrDataOk = amrData.ReadData(fileName, fileType);

  dsArrayIndex = DataServices::dsArrayIndexCounter;
  ++DataServices::dsArrayIndexCounter;
  DataServices::dsArray.resize(DataServices::dsArrayIndexCounter);
  DataServices::dsArray[dsArrayIndex] = this;
  numberOfUsers = 0;  // the user must do all incrementing and decrementing
}


// ---------------------------------------------------------------
DataServices::~DataServices() {
  assert(numberOfUsers == 0);
  DataServices::dsArray[dsArrayIndex] = NULL;
}


// ---------------------------------------------------------------
void DataServices::Dispatch(DSRequestType requestType, DataServices *ds, ...) {
  bool bContinueLooping(true);
  va_list ap;
  int whichDSIndex;
  int ioProcNumber(ParallelDescriptor::IOProcessorNumber());

 while(bContinueLooping) {
  if(ParallelDescriptor::IOProcessor()) {
    bContinueLooping = false;
  }

  ParallelDescriptor::Synchronize();  // procs 1 - N wait here

  ParallelDescriptor::ShareVar(&requestType, sizeof(DSRequestType));
  ParallelDescriptor::Synchronize();  // for ShareVar
  ParallelDescriptor::Broadcast(ioProcNumber, &requestType, &requestType);
  ParallelDescriptor::UnshareVar(&requestType);


  // handle new request
  if(requestType == NewRequest) {
    // broadcast the fileName and fileType to procs 1 - N
    char *fileNameCharPtr;
    int   fileNameLength, checkArrayIndex;
    FileType newFileType;

    if(ParallelDescriptor::IOProcessor()) {
      fileNameLength = ds->fileName.length();
      newFileType = ds->fileType;
      checkArrayIndex = ds->dsArrayIndex;
    }
    ParallelDescriptor::ShareVar(&fileNameLength, sizeof(int));
    ParallelDescriptor::ShareVar(&newFileType, sizeof(FileType));
    ParallelDescriptor::ShareVar(&checkArrayIndex, sizeof(int));
    ParallelDescriptor::Synchronize();  // for ShareVar
    ParallelDescriptor::Broadcast(ioProcNumber, &fileNameLength,  &fileNameLength);
    ParallelDescriptor::Broadcast(ioProcNumber, &newFileType,     &newFileType);
    ParallelDescriptor::Broadcast(ioProcNumber, &checkArrayIndex, &checkArrayIndex);
    ParallelDescriptor::UnshareVar(&checkArrayIndex);
    ParallelDescriptor::UnshareVar(&newFileType);
    ParallelDescriptor::UnshareVar(&fileNameLength);

    fileNameCharPtr = new char[fileNameLength + 1];  // for the null
    if(ParallelDescriptor::IOProcessor()) {
      strcpy(fileNameCharPtr, ds->fileName.c_str());
    }
    ParallelDescriptor::ShareVar(fileNameCharPtr, fileNameLength + 1);
    ParallelDescriptor::Synchronize();  // for ShareVar
    ParallelDescriptor::Broadcast(ioProcNumber, fileNameCharPtr, fileNameCharPtr,
                                  fileNameLength + 1);
    ParallelDescriptor::UnshareVar(fileNameCharPtr);

    aString newFileName(fileNameCharPtr);
    delete [] fileNameCharPtr;

    // make a new DataServices for procs 1 - N
    if( ! ParallelDescriptor::IOProcessor()) {
      ds = new DataServices(newFileName, newFileType);
    }

    // verify dsArrayIndex is correct
    assert(ds->dsArrayIndex == checkArrayIndex);

    continue;
  }


  // handle exit request
  if(requestType == ExitRequest) {
    // cleanup memory
    for(int i = 0; i < dsArray.length(); ++i) {
      if(DataServices::dsArray[i] != NULL) {
	DataServices::dsArray[i]->numberOfUsers = 0;
	delete DataServices::dsArray[i];
      }
    }
    EndParallel();
    exit(0);
  }

  if(ParallelDescriptor::IOProcessor()) {
    va_start(ap, ds);
    whichDSIndex = ds->dsArrayIndex;
  }

  ParallelDescriptor::ShareVar(&whichDSIndex, sizeof(int));
  ParallelDescriptor::Synchronize();  // for ShareVar
  ParallelDescriptor::Broadcast(ioProcNumber, &whichDSIndex, &whichDSIndex);
  if( ! ParallelDescriptor::IOProcessor()) {
    ds = DataServices::dsArray[whichDSIndex];
  }
  assert(ds != NULL);
  ParallelDescriptor::UnshareVar(&whichDSIndex);


  switch(requestType) {
    case FillVarOneFab:
    {
      FArrayBox *destFab = NULL;
      Box destBox;
      int fineFillLevel;
      aString derivedTemp;
      char *derivedCharPtr;
      int derivedLength;

      if(ParallelDescriptor::IOProcessor()) {
	destFab = va_arg(ap, FArrayBox *);
        const Box &boxRef = va_arg(ap, const Box);
	destBox = boxRef;
        fineFillLevel = va_arg(ap, int);
        const aString &derivedRef = va_arg(ap, const aString);
        derivedTemp = derivedRef;
	derivedLength = derivedTemp.length();
      }
      ParallelDescriptor::ShareVar(&destBox, sizeof(Box));
      ParallelDescriptor::ShareVar(&fineFillLevel, sizeof(int));
      ParallelDescriptor::ShareVar(&derivedLength, sizeof(int));
      ParallelDescriptor::Synchronize();  // for ShareVar
      ParallelDescriptor::Broadcast(ioProcNumber, &destBox, &destBox);
      ParallelDescriptor::Broadcast(ioProcNumber, &fineFillLevel, &fineFillLevel);
      ParallelDescriptor::Broadcast(ioProcNumber, &derivedLength, &derivedLength);
      ParallelDescriptor::UnshareVar(&derivedLength);
      ParallelDescriptor::UnshareVar(&fineFillLevel);
      ParallelDescriptor::UnshareVar(&destBox);

      derivedCharPtr = new char[derivedLength + 1];  // for the null
      if(ParallelDescriptor::IOProcessor()) {
        strcpy(derivedCharPtr, derivedTemp.c_str());
      }
      ParallelDescriptor::ShareVar(derivedCharPtr, derivedLength + 1);
      ParallelDescriptor::Synchronize();  // for ShareVar
      ParallelDescriptor::Broadcast(ioProcNumber, derivedCharPtr, derivedCharPtr,
				    derivedLength + 1);
      ParallelDescriptor::UnshareVar(derivedCharPtr);

      aString derived(derivedCharPtr);
      delete [] derivedCharPtr;

      ds->FillVar(destFab, destBox, fineFillLevel, derived, ioProcNumber);

    }
    break;

    case FillVarArrayOfFabs:
    {
      ParallelDescriptor::Abort("FillVarArrayOfFabs not implemented yet.");
    }
    break;

    case WriteFabOneVar:
    {
      ParallelDescriptor::Abort("WriteFabOneVar not implemented yet.");
    }
    break;

    case WriteFabAllVars:
    {
      ParallelDescriptor::Abort("WriteFabAllVars not implemented yet.");
    }
    break;

    case DumpSlicePlaneOneVar:
    {
      int slicedir;
      int slicenum;
      aString derivedTemp;
      char *derivedCharPtr;
      int derivedLength;
      if(ParallelDescriptor::IOProcessor()) {
        slicedir = va_arg(ap, int);
        slicenum = va_arg(ap, int);
        const aString &derivedRef = va_arg(ap, const aString);
        derivedTemp = derivedRef;
	derivedLength = derivedTemp.length();
      }
      ParallelDescriptor::ShareVar(&slicedir, sizeof(int));
      ParallelDescriptor::ShareVar(&slicenum, sizeof(int));
      ParallelDescriptor::ShareVar(&derivedLength, sizeof(int));
      ParallelDescriptor::Synchronize();  // for ShareVar
      ParallelDescriptor::Broadcast(ioProcNumber, &slicedir, &slicedir);
      ParallelDescriptor::Broadcast(ioProcNumber, &slicenum, &slicenum);
      ParallelDescriptor::Broadcast(ioProcNumber, &derivedLength, &derivedLength);
      ParallelDescriptor::UnshareVar(&derivedLength);
      ParallelDescriptor::UnshareVar(&slicenum);
      ParallelDescriptor::UnshareVar(&slicedir);

      derivedCharPtr = new char[derivedLength + 1];  // for the null
      if(ParallelDescriptor::IOProcessor()) {
        strcpy(derivedCharPtr, derivedTemp.c_str());
      }
      ParallelDescriptor::ShareVar(derivedCharPtr, derivedLength + 1);
      ParallelDescriptor::Synchronize();  // for ShareVar
      ParallelDescriptor::Broadcast(ioProcNumber, derivedCharPtr, derivedCharPtr,
				    derivedLength + 1);
      ParallelDescriptor::UnshareVar(derivedCharPtr);

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
      ParallelDescriptor::ShareVar(&slicedir, sizeof(int));
      ParallelDescriptor::ShareVar(&slicenum, sizeof(int));
      ParallelDescriptor::Synchronize();  // for ShareVar
      ParallelDescriptor::Broadcast(ioProcNumber, &slicedir, &slicedir);
      ParallelDescriptor::Broadcast(ioProcNumber, &slicenum, &slicenum);
      ParallelDescriptor::UnshareVar(&slicenum);
      ParallelDescriptor::UnshareVar(&slicedir);

      ds->DumpSlice(slicedir, slicenum);

    }
    break;

    case DumpSliceBoxOneVar:
    {
      Box box;
      aString derivedTemp;
      char *derivedCharPtr;
      int derivedLength;
      if(ParallelDescriptor::IOProcessor()) {
        const Box &boxRef = va_arg(ap, const Box);
        const aString &derivedRef = va_arg(ap, const aString);
	box = boxRef;
	derivedTemp = derivedRef;
	derivedLength = derivedTemp.length();
      }
      ParallelDescriptor::ShareVar(&box, sizeof(Box));
      ParallelDescriptor::ShareVar(&derivedLength, sizeof(int));
      ParallelDescriptor::Synchronize();  // for ShareVar
      ParallelDescriptor::Broadcast(ioProcNumber, &box, &box);
      ParallelDescriptor::Broadcast(ioProcNumber, &derivedLength, &derivedLength);
      ParallelDescriptor::UnshareVar(&derivedLength);
      ParallelDescriptor::UnshareVar(&box);

      derivedCharPtr = new char[derivedLength + 1];  // for the null
      if(ParallelDescriptor::IOProcessor()) {
        strcpy(derivedCharPtr, derivedTemp.c_str());
      }
      ParallelDescriptor::ShareVar(derivedCharPtr, derivedLength + 1);
      ParallelDescriptor::Synchronize();  // for ShareVar
      ParallelDescriptor::Broadcast(ioProcNumber, derivedCharPtr, derivedCharPtr,
				    derivedLength + 1);
      ParallelDescriptor::UnshareVar(derivedCharPtr);

      aString derived(derivedCharPtr);
      delete [] derivedCharPtr;

      ds->DumpSlice(box, derived);
    }
    break;

    case DumpSliceBoxAllVars:
    {
      Box box;
      if(ParallelDescriptor::IOProcessor()) {
        const Box &boxRef = va_arg(ap, const Box);
	box = boxRef;
      }
      ParallelDescriptor::ShareVar(&box, sizeof(Box));
      ParallelDescriptor::Synchronize();  // for ShareVar
      ParallelDescriptor::Broadcast(ioProcNumber, &box, &box);
      ParallelDescriptor::UnshareVar(&box);

      ds->DumpSlice(box);

    }
    break;

    case MinMaxRequest:
    {

      Box box;
      aString derivedTemp;
      char *derivedCharPtr;
      int derivedLength, level;
      Real dataMin, dataMax;
      bool minMaxValid;
      if(ParallelDescriptor::IOProcessor()) {
        const Box &boxRef = va_arg(ap, const Box);
        const aString &derivedRef = va_arg(ap, const aString);
        level = va_arg(ap, int);
	box = boxRef;
	derivedTemp = derivedRef;
	derivedLength = derivedTemp.length();
      }
      ParallelDescriptor::ShareVar(&box, sizeof(Box));
      ParallelDescriptor::ShareVar(&derivedLength, sizeof(int));
      ParallelDescriptor::ShareVar(&level, sizeof(int));
      ParallelDescriptor::Synchronize();  // for ShareVar
      ParallelDescriptor::Broadcast(ioProcNumber, &box, &box);
      ParallelDescriptor::Broadcast(ioProcNumber, &derivedLength, &derivedLength);
      ParallelDescriptor::Broadcast(ioProcNumber, &level, &level);
      ParallelDescriptor::UnshareVar(&level);
      ParallelDescriptor::UnshareVar(&derivedLength);
      ParallelDescriptor::UnshareVar(&box);

      derivedCharPtr = new char[derivedLength + 1];  // for the null
      if(ParallelDescriptor::IOProcessor()) {
        strcpy(derivedCharPtr, derivedTemp.c_str());
      }
      ParallelDescriptor::ShareVar(derivedCharPtr, derivedLength + 1);
      ParallelDescriptor::Synchronize();  // for ShareVar
      ParallelDescriptor::Broadcast(ioProcNumber, derivedCharPtr, derivedCharPtr,
				    derivedLength + 1);
      ParallelDescriptor::UnshareVar(derivedCharPtr);

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
  SHOWVAL(sliceFile);

  Box sliceBox(amrData.ProbDomain()[amrData.FinestLevel()]);

  if(BL_SPACEDIM == 2 && slicedir == ZDIR) {
    // use probDomain for the sliceBox
  } else {
    // make the box one cell thick in the slice direction
    sliceBox.setSmall(slicedir, slicenum);
    sliceBox.setBig(slicedir, slicenum);
  }

  SHOWVAL(sliceBox);
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
  SHOWVAL(sliceFile);

  Box sliceBox(amrData.ProbDomain()[amrData.FinestLevel()]);

  if(BL_SPACEDIM == 2 && slicedir == ZDIR) {
    // use probDomain for the sliceBox
  } else {
    // make the box one cell thick in the slice direction
    sliceBox.setSmall(slicedir, slicenum);
    sliceBox.setBig(slicedir, slicenum);
  }

  SHOWVAL(sliceBox);
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
  SHOWVAL(sliceFile);

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
  SHOWVAL(sliceFile);

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
    //amrData.FillVar(data, lev, varname);
    amrData.FillVar(destFabs, destBoxes, lev, varname,
		    ParallelDescriptor::IOProcessorNumber());

    if(ParallelDescriptor::IOProcessor()) {
      ofstream os;
      os.open(fname.c_str(), ios::out);
      data.writeOn(os);
      os.close();
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
      int srccomp = 0;
      int destcomp = ivar;
      int ncomp = 1;
      if(ParallelDescriptor::IOProcessor()) {
        data.copy(tempdata, srccomp, destcomp, ncomp);
      }
    }

    if(ParallelDescriptor::IOProcessor()) {
      ofstream os;
      os.open(fname.c_str(), ios::out);
      data.writeOn(os);
      os.close();
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
bool DataServices::PointValue(Array<Box> &pointBox, int &intersectLevel,
		 Box &intersectGrid, aString &dataValue,
		 int coarseLevel, int fineLevel,
		 const aString &currentDerived, aString formatString)
{
  if( ! bAmrDataOk) {
    return false;
  }
/*
  Box gridBox;
  Real dataPointValue;
  int intersectBoxIndex;
  assert(coarseLevel >= 0 && coarseLevel <= amrData.FinestLevel());
  assert(fineLevel >= 0 && fineLevel <= amrData.FinestLevel());
  bool bReturnValue(false);
  bool bBodyValue(false);
  bool bValueFoundLocally(false);
  bool bValueFoundAnywhere(false);
  ParallelDescriptor::ShareVar(&dataPointValue, sizeof(Real));
  ParallelDescriptor::ShareVar(&intersectBoxIndex, sizeof(int));
  ParallelDescriptor::ShareVar(&intersectLevel, sizeof(int));
  for(int lev = fineLevel; lev >= coarseLevel; lev--) {
    for(MultiFabIterator gpli(*grids[lev]); gpli.isValid(); ++gpli) {
      gridBox = gpli.validbox();
      if(gridBox.intersects(pointBox[lev])) {
        Box dataBox(gridBox);
        dataBox &= pointBox[lev];
        FArrayBox *dataFab = new FArrayBox(dataBox, 1);
	dataFab->copy(gpli(), StateNumber(currentDerived), 0, 1);
	dataPointValue = (dataFab->dataPtr())[0];
        delete dataFab;
	bValueFoundLocally  = true;
	bValueFoundAnywhere = true;
	intersectBoxIndex = gpli.index();
	intersectLevel = lev;
        bReturnValue = true;
      }
    }  // end for(gpli...)
    // the following is so we stop when the value is found on any processor
    // (the for loop goes from the finest level to the coarsest)
    ReduceBoolOr(bValueFoundAnywhere);
    if(bValueFoundAnywhere) {
      break;  // dont look at coarser levels
    }
  }  // end for(lev...)

  if(bValueFoundLocally) {
    int myproc = ParallelDescriptor::MyProc();
    ParallelDescriptor::Broadcast(myproc, &dataPointValue, &dataPointValue);
    ParallelDescriptor::Broadcast(myproc, &intersectBoxIndex, &intersectBoxIndex);
    ParallelDescriptor::Broadcast(myproc, &intersectLevel, &intersectLevel);
  }
  ReduceBoolOr(bBodyValue);
  ReduceBoolOr(bReturnValue);

  intersectGrid = *grids[intersectLevel].box(intersectBoxIndex);
  char tdv[LINELENGTH];
  if(bBodyValue) {
    dataValue = "body";
  } else {
    sprintf(tdv, formatString.c_str(), dataPointValue);
    dataValue = tdv;
  }

  ParallelDescriptor::UnshareVar(&intersectLevel);
  ParallelDescriptor::UnshareVar(&intersectBoxIndex);
  ParallelDescriptor::UnshareVar(&dataPointValue);

  return bReturnValue;
*/
  return false;
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
