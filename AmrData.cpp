// ---------------------------------------------------------------
// AmrData.cpp
// ---------------------------------------------------------------
#include "AmrData.H"
#include "GlobalUtilities.H"
#include "ArrayLim.H"
#include "BoxDomain.H"
#include "Boolean.H"
#include "VisMF.H"

#include "kutils.H"

#ifdef CRAY
#     if (BL_SPACEDIM == 2)
#	   define   FORT_CNTRP     CNTRP2D
#	   define   FORT_PCNTRP    PCNTRP2D

#     elif (BL_SPACEDIM == 3)
#	   define   FORT_CNTRP     CNTRP3D
#	   define   FORT_PCNTRP    PCNTRP3D
#     endif
#else
#     if (BL_SPACEDIM == 2)
#	   define   FORT_CNTRP     cntrp2d_
#	   define   FORT_PCNTRP    pcntrp2d_

#     elif (BL_SPACEDIM == 3)
#	   define   FORT_CNTRP     cntrp3d_
#	   define   FORT_PCNTRP    pcntrp3d_
#     endif
#endif


extern "C" {
  void FORT_CNTRP(Real *fine, ARLIM_P(flo), ARLIM_P(fhi),
                  const int *fblo, const int *fbhi,
                  const int &nvar, const int &lratio,
		  const Real *crse, const int &clo, const int &chi,
		  const int *cslo, const int *cshi,
		  const int *fslo, const int *fshi,
		  Real *cslope, const int &c_len,
		  Real *fslope, Real *fdat, const int &f_len,
		  Real *foff);

  void FORT_PCNTRP(Real *fine, ARLIM_P(flo), ARLIM_P(fhi),
                   const int *fblo, const int *fbhi,
		   const int &lrat, const int &nvar,
		   const Real *crse, ARLIM_P(clo), ARLIM_P(chi),
		   const int *cblo, const int *cbhi,
		   Real *temp, const int &tlo, const int &thi);
};


// ---------------------------------------------------------------
AmrData::AmrData() {
  for(int i = 0; i < BL_SPACEDIM; i++ ) {
    probLo[i]   =  0.0;
    probHi[i]   = -1.0;
    probSize[i] = -1.0;
  }
  plotVars.clear();
  nRegions = 0;
  boundaryWidth = 0;
}


// ---------------------------------------------------------------
AmrData::~AmrData(){

   for(int i = 0; i < nRegions; i++) {
     for(int lev = 0; lev <= finestLevel; lev++) {
       delete regions[lev][i];
     }
   }
   for(int lev = 0; lev <= finestLevel; lev++) {
     for(int iComp = 0; iComp < nComp; ++iComp) {
       for(MultiFabIterator mfi(*dataGrids[lev][iComp]); mfi.isValid(); ++mfi) {
         if(dataGridsDefined[lev][iComp][mfi.index()]) {
           delete dataGrids[lev][iComp]->remove(mfi.index());
         }
       }
       delete dataGrids[lev][iComp];
     }
   }
}


// ---------------------------------------------------------------
bool AmrData::ReadData(const aString &filename, FileType filetype) {

   fileType = filetype;
   if(filetype == FAB || filetype == MULTIFAB) {
     return ReadNonPlotfileData(filename, filetype);
   }

   // NEWPLT follows

   int i, j, k, width;
   aString plotName;
   fileName = filename;

    aString File = filename;

#ifdef BL_PARALLEL_IO
    File += '/';
    File += "Header";
#endif /*BL_PARALLEL_IO*/

#ifdef BL_USE_SETBUF
    VisMF::IO_Buffer io_buffer(VisMF::IO_Buffer_Size);
#endif

    ifstream is;

#ifdef BL_USE_SETBUF
    is.rdbuf()->setbuf(io_buffer.dataPtr(), io_buffer.length());
#endif

   if(Verbose()) {
      cout << "AmrData::opening file = " << File << endl;
   }

   is.open(File.c_str(), ios::in);
   if(is.fail()) {
      cerr << "Unable to open plotfile: " << File << endl;
      return false;
   }

   aString skipBuff(LINELENGTH);
   for(i = 0; i < GetSkipPltLines(); i++) {
     skipBuff.getline(is);
     cout << "Skipped line in pltfile = " << skipBuff << endl;
   }

     aString plotFileVersion;
     is >> plotFileVersion;
     if(Verbose()) {
       cout << "Plot file version:  " << plotFileVersion << endl;
     }

     // read list of variables written
     is >> nComp;
      if(nComp < 1 || nComp > 1024) {  // arbitrarily limit to 1024
        cerr << "Error in AmrData:  bad nComp = " << nComp << endl;
        return false;
      }
      VSHOWVAL(Verbose(), nComp);

      plotVars.resize(nComp);
      eatws(is); // eat white space left by << operator
      for(i = 0; i < nComp; ++i) {
          plotName.getline(is);
          plotVars[i] = plotName;
          VSHOWVAL(Verbose(), plotName);
      }

      int spacedim;
      is >>  spacedim >> time >> finestLevel;
      VSHOWVAL(Verbose(), spacedim);
      VSHOWVAL(Verbose(), time);
      VSHOWVAL(Verbose(), finestLevel);
      if(spacedim != BL_SPACEDIM) {
	cerr << endl << " ~~~~ Error:  You are using " << BL_SPACEDIM << "D amrvis "
	     << "to look at a " << spacedim << "D file." << endl << endl;
	return false;
      }
      if(finestLevel < 0) {
        cerr << "Error in AmrData:  bad finestLevel = " << finestLevel << endl;
        return false;
      }
      for(i = 0; i < BL_SPACEDIM; i++) {
        is >> probLo[i];
        if(Verbose()) {
	  cout << "probLo[" << i << "] = " << probLo[i] << endl;
	}
      }
      for(i = 0; i < BL_SPACEDIM; i++) {
        is >> probHi[i];
        if(Verbose()) {
	  cout << "probHi[" << i << "] = " << probHi[i] << endl;
	}
      }
      if(Verbose()) {
	cout << "Resizing refRatio to size = " << finestLevel << endl;
      }
      refRatio.resize(finestLevel);
      while(is.get() != '\n');
      bool bIVRefRatio(false);
      if(is.peek() == '(') {  // it is an IntVect
        bIVRefRatio = true;
      }
      for(i = 0; i < finestLevel; i++) {
	// try to guess if refRatio is an IntVect
	if(bIVRefRatio) {  // it is an IntVect
	  IntVect ivRefRatio;
	  is >> ivRefRatio;
          if(Verbose()) {
	    cout << "IntVect refRatio[" << i << "] = " << ivRefRatio << endl;
	  }
	  refRatio[i] = ivRefRatio[0];  // non-uniform ref ratios not supported
	} else {
          is >> refRatio[i];
	}
        if(Verbose()) {
	  cout << "refRatio[" << i << "] = " << refRatio[i] << endl;
	}
      }
      for(i = 0; i < finestLevel; ++i ) {
        if(refRatio[i] < 2 || refRatio[i] > 32 ) {
          cerr << "Error in AmrData:  bad refRatio at level " << i << " = "
	       << refRatio[i] << endl;
          return false;
        }
      }
      while(is.get() != '\n');
      probDomain.resize(finestLevel + 1);
      maxDomain.resize(finestLevel + 1);
      for(i = 0; i <= finestLevel; i++) {
        is >> probDomain[i];
	if(Verbose()) {
	  cout << "probDomain[" << i << "] = " << probDomain[i] << endl;
	}
        if( ! probDomain[i].ok()) {
          cerr << "Error in AmrData:  bad probDomain[" << i << "] = "
	       << probDomain[i] << endl;
          return false;
        }
      }

      char lstepbuff[128];
      while(is.get() != '\n') {
        ;  // do nothing
      }
      is.getline(lstepbuff, 128);  // ignore levelsteps--some files have
				   // finestlevel of these, others have
				   // finestlevel + 1
      if(Verbose()) {
	cout << "Ignored levelSteps = " << lstepbuff << endl;
      }
      
      dxLevel.resize(finestLevel + 1);
      for(i = 0; i <= finestLevel; i++) {
        dxLevel[i].resize(BL_SPACEDIM);
        for(k = 0; k < BL_SPACEDIM; k++) {
	  is >> dxLevel[i][k];
	  if(Verbose()) {
	    cout << "dxLevel[" << i << "][" << k << "] = " << dxLevel[i][k] << endl;
	  }
	}
      }

      for(i = 0; i < BL_SPACEDIM; i++) {
        probSize[i] = probHi[i] - probLo[i];
        if(probSize[i] <= 0.0 ) {
          cerr << "Error in AmrData:  bad probSize[" << i << "] = "
	       << probSize[i] << endl;
          return false;
	}
      }

      int cSys;
      is >> cSys;
      VSHOWVAL(Verbose(), cSys);
      while(is.get() != '\n') {
        ;  // do nothing
      }

      is >> width;   // width of bndry regions
      VSHOWVAL(Verbose(), width);
      while(is.get() != '\n') {
        ;  // do nothing
      }

   dataGrids.resize(finestLevel + 1);
   dataGridsDefined.resize(finestLevel + 1);

   int lev;
   boundaryWidth = Max(width, GetBoundaryWidth());
   int Restrict = maxDomain[0].ok();  // cap R: restrict is a cray keyword
   if(Restrict) {
      for(lev = 1; lev <= finestLevel; lev++) {
        maxDomain[lev] = refine(maxDomain[lev-1],refRatio[lev-1]);
      }
   }
   Array<Box> restrictDomain(finestLevel + 1);
   Array<Box> extendRestrictDomain(finestLevel + 1);
   regions.resize(finestLevel + 1);
   for(lev = 0; lev <= finestLevel; ++lev) {
      restrictDomain[lev] = probDomain[lev];
      if(Restrict) {
        restrictDomain[lev] = maxDomain[lev];
      }
      extendRestrictDomain[lev] = grow(restrictDomain[lev],boundaryWidth);
      BoxList bndry_boxes = boxDiff(extendRestrictDomain[lev], restrictDomain[lev]);
      nRegions = bndry_boxes.length();

      BoxListIterator bli(bndry_boxes);
      regions[lev].resize(nRegions);
      i = 0;
      while(bli) {
	regions[lev][i] = new FArrayBox(bli(),nComp);
	if(Verbose()) {
	  cout << "BNDRY REGION " << i << " : " << bli() << endl;
	  cout << "    numPts = " << bli().numPts() << endl;
	}
	++i;
	++bli;
      }
   }

   // if positive set up and read bndry databoxes
   if(width > 0) {
        cerr << "Error in AmrData:  Boundary width > 0 not supported:  width = "
	     << width << endl;
        return false;
   }  // end if(width...)

   // read all grids but only save those inside the restricted region

    visMF.resize(finestLevel + 1);

    for(i = 0; i <= finestLevel; i++) {
      int nGrids;
      Real gTime;
      int iLevelSteps;
      is >> lev >> nGrids >> gTime >> iLevelSteps;
      VSHOWVAL(Verbose(), lev);
      VSHOWVAL(Verbose(), nGrids);
      VSHOWVAL(Verbose(), gTime);
      VSHOWVAL(Verbose(), iLevelSteps);
      if(i != lev) {
	cerr << "Level misrestart:mismatch on restart" << endl;
        cerr << "Error in AmrData:  Level mismatch:  read level " << lev
	     << " while expecting level " << i << endl;
        return false;
      }
      if(nGrids < 1) {
        cerr << "Error in AmrData:  bad nGrids = " << nGrids << endl;
        return false;
      }

      Real gridLocLo, gridLocHi;  // unused here
      for(int iloc = 0; iloc < nGrids; ++iloc) {
	for(int iDim = 0; iDim < BL_SPACEDIM; ++iDim) {
	  is >> gridLocLo >>  gridLocHi;
          VSHOWVAL(Verbose(), gridLocLo);
          VSHOWVAL(Verbose(), gridLocHi);
	}
      }

      aString mfNameRelative;
      is >> mfNameRelative;
      aString mfName(fileName);
#ifdef BL_PARALLEL_IO
      mfName += '/';
      mfName += mfNameRelative;
      VSHOWVAL(Verbose(), mfName);
      VSHOWVAL(Verbose(), mfNameRelative);
#endif /* BL_PARALLEL_IO */

      visMF[i] = new VisMF(mfName);
      dataGrids[i].resize(nComp);
      dataGridsDefined[i].resize(nComp);
      for(int iComp = 0; iComp < nComp; ++iComp) {
        // make single component multifabs
        // defer reading the MultiFab data
        dataGrids[i][iComp] = new MultiFab(visMF[i]->boxArray(), 1,
			                   visMF[i]->nGrow(), Fab_noallocate);
        dataGridsDefined[i][iComp].resize(visMF[i]->length());
        for(int iIdx = 0; iIdx < dataGridsDefined[i][iComp].length(); ++iIdx) {
          dataGridsDefined[i][iComp][iIdx] = false;
        }
      }

    }  // end for(i...finestLevel)

   // fill a set of temporary bndry regions surrounding the
   // restricted domain by extension from interior data
   // only use this data in bndry regions that did not
   // get better data from interior or input bndry regions
   for(lev = 0; lev <= finestLevel; lev++) {
      Box inbox(restrictDomain[lev]);
      Box reg1 = grow(restrictDomain[lev],boundaryWidth);
      Box reg2 = grow(probDomain[lev],width);
      BoxList outside = boxDiff(reg1,reg2);
      if(outside.length() > 0) {
         // parts of the bndry have not been filled from the input
	 // data, must extending from interior regions

         for(int idir = 0; idir < BL_SPACEDIM; idir++) {
            Box bx(adjCellLo(inbox,idir,boundaryWidth));
	    Box br(bx);
	    for(k = 0; k < BL_SPACEDIM; k++) {
	      if(k != idir) {
	        br.grow(k,boundaryWidth);
	      }
	    }

	    br.shift(idir,1);
	    FArrayBox tmpreg(br,nComp);
	    Box reg_bx = tmpreg.box();
	    reg_bx &= inbox;
	    FillInterior(tmpreg,lev,reg_bx);
	    br.shift(idir,-1);
	    FArrayBox tmpreg_lo(br,nComp);
	    tmpreg_lo.copy(tmpreg,tmpreg.box(),0,tmpreg_lo.box(),0,nComp);

            // now fill out tmp region along idir direction
	    Box b_lo(adjCellLo(inbox,idir,1));
	    for(k = 1; k < boundaryWidth; k++) {
	       Box btmp(b_lo);
	       btmp.shift(idir,-k);
	       tmpreg_lo.copy(tmpreg_lo,b_lo,0,btmp,0,nComp);
	    }

	    // now fill out temp bndry region
	    Box b_src, b_dest;
	    int n;
	    for(k = 1; k < BL_SPACEDIM; k++) {
	       int kdir = (idir + k) % BL_SPACEDIM;
 	       b_dest = adjCellLo(bx, kdir, 1);
	       b_src  = b_dest;
	       b_src  = b_src.shift(kdir, 1);
               for(n = 1; n <= boundaryWidth; n++) {
	          tmpreg_lo.copy(tmpreg_lo, b_src, 0, b_dest, 0, nComp);
	          b_dest.shift(kdir, -1);
	       }

	       b_dest = adjCellHi(bx,kdir,1);
	       b_src = b_dest;
	       b_src.shift(kdir,-1);
               for(n = 1; n <= boundaryWidth; n++) {
	          tmpreg_lo.copy(tmpreg_lo,b_src,0,b_dest,0,nComp);
	          b_dest.shift(kdir,1);
	       }
	       bx.grow(kdir,boundaryWidth);
	    }

	    // now copy into real bndry regions
	    for(j = 0; j < nRegions; j++) {
	       FArrayBox *p = regions[lev][j];
	       Box p_box = p->box();
	       BoxListIterator bli(outside);
	       while(bli) {
                 Box ovlp(p_box);
		 ovlp &= bli();
		 ovlp &= br;
		 if(ovlp.ok()) {
  		   p->copy(tmpreg_lo,ovlp);
		 }
		 bli++;
               }
	    }  // end for j

            // now work on the high side of the bndry region
            bx = adjCellHi(inbox,idir,boundaryWidth);
	    br = bx;
	    for(k = 0; k < BL_SPACEDIM; k++) {
	      if(k != idir) br.grow(k, boundaryWidth);
	    }

	    br.shift(idir,-1);
	    FArrayBox tmpreg2(br,nComp);
	    reg_bx = tmpreg2.box();
	    reg_bx &= inbox;
	    FillInterior(tmpreg2,lev,reg_bx);
	    br.shift(idir,1);
	    FArrayBox tmpreg_hi(br,nComp);
	    tmpreg_hi.copy(tmpreg2,tmpreg2.box(),0,tmpreg_hi.box(),0,nComp);

            // now fill out tmp region along idir direction
	    Box b_hi(adjCellHi(inbox,idir,1));
	    for(k = 1; k < boundaryWidth; k++) {
	       Box btmp(b_hi);
	       btmp.shift(idir,k);
	       tmpreg_hi.copy(tmpreg_hi,b_hi,0,btmp,0,nComp);
	    }

	    // now fill out temp bndry region
	    for(k = 1; k < BL_SPACEDIM; k++) {
	       int kdir = (idir + k) % BL_SPACEDIM;
	       b_dest = adjCellLo(bx, kdir, 1);
	       b_src  = b_dest;
	       b_src.shift(kdir, 1);
               for(n = 1; n <= boundaryWidth; n++) {
	          tmpreg_hi.copy(tmpreg_hi, b_src, 0, b_dest, 0, nComp);
	          b_dest.shift(kdir,-1);
	       }

	       b_dest = adjCellHi(bx, kdir, 1);
	       b_src  = b_dest;
	       b_src.shift(kdir, -1);
               for(n = 1; n <= boundaryWidth; n++) {
	          tmpreg_hi.copy(tmpreg_hi, b_src, 0, b_dest, 0, nComp);
	          b_dest.shift(kdir, 1);
	       }
	       bx.grow(kdir, boundaryWidth);
	    }

	    // now copy into real bndry regions
	    for(j = 0; j < nRegions; j++) {
	       FArrayBox *p = regions[lev][j];
	       Box p_box = p->box();
	       BoxListIterator bli(outside);
	       while(bli) {
                 Box ovlp(p_box);
		 ovlp &= bli();
		 ovlp &= br;
		 if(ovlp.ok()) {
  		   p->copy(tmpreg_hi,ovlp);
		 }
		 bli++;
               }
	    }  // end for j

         }  // end for(idir...)
      }  // end if(outside.length())...

      outside.clear();

   }  // end for(lev...)

   if(Restrict) {
      Array<Real> p_lo(BL_SPACEDIM), p_hi(BL_SPACEDIM);
      LoNodeLoc(0,maxDomain[0].smallEnd(),p_lo);
      HiNodeLoc(0,maxDomain[0].bigEnd(),p_hi);
      for(i = 0; i < BL_SPACEDIM; i++) {
         probLo[i] = p_lo[i];
	 probHi[i] = p_hi[i];
	 probSize[i] = p_hi[i] - p_lo[i];
      }
      for(lev = 0; lev <= finestLevel; lev++) {
         probDomain[lev] = maxDomain[lev];
      }
   }

   return true;

}  // end ReadData




// ---------------------------------------------------------------
bool AmrData::ReadNonPlotfileData(const aString &filename, FileType filetype) {
  int i;
  if(Verbose()) {
     cout << "AmrPlot::opening file = " << filename << endl;
  }

  fileName = filename;
  ifstream is;
  is.open(filename.c_str(), ios::in);
  if(is.fail()) {
     cerr << "Unable to open plotfile: " << filename << endl;
     return false;
  }

#ifdef BL_USE_SETBUF
  is.rdbuf()->setbuf(io_buffer.dataPtr(), io_buffer.length());
#endif

   aString skipBuff(LINELENGTH);
   for(i = 0; i < GetSkipPltLines(); i++) {
     skipBuff.getline(is);
     cout << "Skipped line in file = " << skipBuff << endl;
   }

  time = 0;
  finestLevel = 0;
  probDomain.resize(finestLevel + 1);
  maxDomain.resize(finestLevel + 1);
  dxLevel.resize(finestLevel + 1);
  dxLevel[0].resize(BL_SPACEDIM);
  refRatio.resize(1);
  refRatio[0] = 1;
  for(i = 0; i < BL_SPACEDIM; i++) {
    probLo[i] = 0.0;
    probHi[i] = 1.0;  // arbitrarily
    probSize[i] = probHi[i] - probLo[i];
    dxLevel[0][i] = 0.0;
  }

  dataGrids.resize(finestLevel + 1);
  dataGridsDefined.resize(finestLevel + 1);

  if(fileType == FAB) {
    FArrayBox *newfab = new FArrayBox;
    nComp = newfab->readFrom(is, 0);  // read the first component
    Box fabbox(newfab->box());
    fabBoxArray.resize(1);
    fabBoxArray.set(0, fabbox);
    dataGrids[0].resize(nComp);
    dataGridsDefined[0].resize(nComp);
    dataGridsDefined[0][0].resize(1);
    dataGrids[0][0] = new MultiFab;
    int nGrow(0);
    dataGrids[0][0]->define(fabBoxArray, nGrow, Fab_noallocate);
    dataGrids[0][0]->setFab(0, newfab);
    dataGridsDefined[0][0][0] = true;
    // read subsequent components
    // need to optimize this for lazy i/o
    for(int iComp = 1; iComp < nComp; ++iComp) {
      dataGrids[0][iComp] = new MultiFab;
      dataGrids[0][iComp]->define(fabBoxArray, nGrow, Fab_noallocate);
      dataGrids[0][iComp]->setFab(0, newfab);
      dataGridsDefined[0][iComp].resize(1);
      dataGridsDefined[0][iComp][0] = true;
    }
    char fabname[32];  // arbitrarily
    plotVars.resize(nComp);
    for(i = 0; i < nComp; i++) {
      sprintf(fabname, "%s%d", "Fab_", i);
      plotVars[i] = fabname;
    }
    probDomain[0] = newfab->box();
  } else if(fileType == MULTIFAB) {
    cerr << "MULTIFAB file type not yet supported." << endl;
    return false;
  }

  is.close();

  return true;
}


// ---------------------------------------------------------------
void AmrData::CellLoc(int lev, IntVect ix, Array<Real> &pos) const {
   assert(pos.length() == dxLevel[lev].length());
   for(int i = 0; i < BL_SPACEDIM; i++) {
      pos[i] = probLo[i] + (dxLevel[lev][i])*(0.5 + Real(ix[i]));
   }
}


// ---------------------------------------------------------------
void AmrData::LoNodeLoc(int lev, IntVect ix, Array<Real> &pos) const {
   assert(pos.length() == dxLevel[lev].length());
   for(int i = 0; i < BL_SPACEDIM; i++) {
      pos[i] = probLo[i] + (dxLevel[lev][i])*Real(ix[i]);
   }
}


// ---------------------------------------------------------------
void AmrData::HiNodeLoc(int lev, IntVect ix, Array<Real> &pos) const {
   assert(pos.length() == dxLevel[lev].length());
   for(int i = 0; i < BL_SPACEDIM; i++) {
      pos[i] = probLo[i] + (dxLevel[lev][i])*Real(ix[i]+1);
   }
}


// ---------------------------------------------------------------
void AmrData::FillVar(FArrayBox *destFab, const Box &destBox,
		      int finestFillLevel, const aString &nm, int procWithFabs)
{
  Array<FArrayBox *> destFabs(1);
  Array<Box> destBoxes(1);
  destFabs[0] = destFab;
  destBoxes[0] = destBox;

  FillVar(destFabs, destBoxes, finestFillLevel, nm, procWithFabs);
}



// ---------------------------------------------------------------
void AmrData::FillVar(Array<FArrayBox *> &destFabs, const Array<Box> &destBoxes,
		      int finestFillLevel, const aString &nm, int procWithFabs)
{

//
// This function fills dest only on procWithFabs.  All other dest
// pointers (on other processors) should be NULL.  destBox
// on all processors must be defined.
//

   assert(finestFillLevel >= 0 && finestFillLevel <= finestLevel);
   for(int iIndex = 0; iIndex < destBoxes.length(); ++iIndex) {
     assert(probDomain[finestFillLevel].contains(destBoxes[iIndex]));
   }

/*  original code vvvvvvvvvvvvvv
   Box dbox(dest.box());
   Box ovlp(dbox);
   ovlp &= probDomain[finestFillLevel];
   if(ovlp.ok()) {   // fill on the interior
      if(finestFillLevel > 0) {
         BoxDomain fd;
         fd.add(ovlp);
	 for(MultiFabIterator gpli(*dataGrids[finestFillLevel]);
	     gpli.isValid(); ++gpli)
	 {
           if(ovlp.intersects(gpli.validbox())) {
   	     fd.rmBox(gpli.validbox());
           }
         }
         Box fine_unfilled = fd.minimalBox();
      
         if(fine_unfilled.ok()) {
            int lrat = refRatio[finestFillLevel-1];
            // not all filled, must interpolate from a coarser level.
            fd.simplify();
	    BoxDomainIterator bli(fd);
            while(bli) {
               Box fine_box(bli());
               Box crse_box = coarsen(fine_box,lrat);
               FArrayBox crse_patch(crse_box,1);
	       FillVar(crse_patch,finestFillLevel-1,nm);
               PcInterp(dest, crse_patch,fine_box,lrat);

	      bli++;
            }
         }
         fd.clear();
      }  // finestFillLevel > 0
   }     // ovlp ok

   // now, overwrite with good data from this level
   dataGrids[finestFillLevel]->copy(dest, StateNumber(nm), 0, 1);
*/


    int myproc = ParallelDescriptor::MyProc();

    //cout << endl;
    //cout << myproc << ":  " << "_in AmrData::FillVar(...)" << endl;

    int stateIndex = StateNumber(nm);
    int srcComp    = 0;
    int destComp   = 0;
    int numComps   = 1;

    int currentLevel;
    Array<int> cumulativeRefRatios(finestFillLevel + 1);

    cumulativeRefRatios[finestFillLevel] = 1;
    for(currentLevel = finestFillLevel - 1; currentLevel >= 0; --currentLevel) {
      cumulativeRefRatios[currentLevel] = cumulativeRefRatios[currentLevel + 1] *
                                          refRatio[currentLevel];
    }
    //for(currentLevel = 0; currentLevel <= finestFillLevel; ++currentLevel) {
      //cout << "cumulativeRefRatios[" << currentLevel << "] = "
	   //<< cumulativeRefRatios[currentLevel] << endl;
    //}


    // ensure the required grids are in memory
    for(currentLevel = 0; currentLevel <= finestFillLevel; ++currentLevel) {
      for(int iBox = 0; iBox < destBoxes.length(); ++iBox) {
	Box tempCoarseBox(destBoxes[iBox]);
        if(currentLevel != finestFillLevel) {
          tempCoarseBox.coarsen(cumulativeRefRatios[currentLevel]);
        }
        GetGrids(currentLevel, stateIndex, tempCoarseBox);
      }
    }


    MultiFabCopyDescriptor multiFabCopyDesc(true);
    Array<MultiFabId> stateDataMFId(finestFillLevel + 1);
    for(currentLevel = 0; currentLevel <= finestFillLevel; ++currentLevel) {
      stateDataMFId[currentLevel] =
           multiFabCopyDesc.RegisterFabArray(dataGrids[currentLevel][stateIndex]);
    }

    Array<Box> localMFBoxes;      // These are the ones we want to fillpatch.
    Array< Array< Array< Array<FillBoxId> > > > fillBoxId;
			   // [grid][level][fillablesubbox][oldnew]
			   // oldnew not used here
    Array< Array< Array<Box> > > savedFineBox;  // [grid][level][fillablesubbox]
    if(myproc == procWithFabs) {
      localMFBoxes = destBoxes;
      fillBoxId.resize(destBoxes.length());
      savedFineBox.resize(destBoxes.length());
      for(int iLocal = 0; iLocal < localMFBoxes.length(); ++iLocal) {
        fillBoxId[iLocal].resize(finestFillLevel + 1);
        savedFineBox[iLocal].resize(finestFillLevel + 1);
      }
    }

    IndexType boxType(destBoxes[0].ixType());
    BoxList unfilledBoxesOnThisLevel(boxType);
    BoxList unfillableBoxesOnThisLevel(boxType);
    //
    // Do this for all local fab boxes.
    //
    for(int ibox = 0; ibox < localMFBoxes.length(); ++ibox) {
        //if(levelData.DistributionMap().ProcessorMap()[ibox] != myproc)
            //continue;  // Not local.

        unfilledBoxesOnThisLevel.clear();
        assert(unfilledBoxesOnThisLevel.ixType() == boxType);
        assert(unfilledBoxesOnThisLevel.ixType() == localMFBoxes[ibox].ixType());
        unfilledBoxesOnThisLevel.add(localMFBoxes[ibox]);
        //
        // Find the boxes that can be filled on each level--these are all
        // defined at their level of refinement.
        //
        bool needsFilling = true;
        for(currentLevel = finestFillLevel; currentLevel >= 0 && needsFilling;
            --currentLevel)
        {
            unfillableBoxesOnThisLevel.clear();

            //StateData& currentState   = amrLevels[currentLevel].state[stateIndex];
            const Box &currentPDomain = probDomain[currentLevel];

	    int ufbLength = unfilledBoxesOnThisLevel.length();
            fillBoxId[ibox][currentLevel].resize(ufbLength);
            savedFineBox[ibox][currentLevel].resize(ufbLength);

            int currentBLI = 0;
            for(BoxListIterator bli(unfilledBoxesOnThisLevel); bli; ++bli) {
                assert(bli().ok());
                Box coarseDestBox(bli());
                Box fineTruncDestBox(coarseDestBox & currentPDomain);
                if(fineTruncDestBox.ok()) {
                  fineTruncDestBox.refine(cumulativeRefRatios[currentLevel]);
                  Box tempCoarseBox;
                  if(currentLevel == finestFillLevel) {
                    tempCoarseBox = fineTruncDestBox;
                  } else {
                    //tempCoarseBox = map[currentLevel]->CoarseBox(fineTruncDestBox,
                                               //cumulativeRefRatios[currentLevel]);
                    tempCoarseBox = fineTruncDestBox;
		    // check this vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
                    tempCoarseBox.coarsen(cumulativeRefRatios[currentLevel]);
                  }

                  savedFineBox[ibox][currentLevel][currentBLI] = fineTruncDestBox;
                  assert(localMFBoxes[ibox].intersects(fineTruncDestBox));

                  BoxList tempUnfillableBoxes(boxType);
                  fillBoxId[ibox][currentLevel][currentBLI].resize(1);
                  fillBoxId[ibox][currentLevel][currentBLI][0] = 
		      multiFabCopyDesc.AddBox(stateDataMFId[currentLevel],
					      tempCoarseBox, tempUnfillableBoxes,
					      srcComp, destComp, numComps);

                  unfillableBoxesOnThisLevel.join(tempUnfillableBoxes);
                  ++currentBLI;
                }
            }

            unfilledBoxesOnThisLevel.clear();
            unfilledBoxesOnThisLevel =
                unfillableBoxesOnThisLevel.intersect(currentPDomain);

            if(unfilledBoxesOnThisLevel.isEmpty()) {
              needsFilling = false;
            } else {
              Box coarseLocalMFBox(localMFBoxes[ibox]);
              coarseLocalMFBox.coarsen(cumulativeRefRatios[currentLevel]);
              unfilledBoxesOnThisLevel.intersect(coarseLocalMFBox);
	      // check this vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	      if(currentLevel != 0) {
                unfilledBoxesOnThisLevel.coarsen(refRatio[currentLevel - 1]);
	      }

              if(currentLevel == 0) {
                BoxList unfilledInside =
                        unfilledBoxesOnThisLevel.intersect(currentPDomain);
                if( ! unfilledInside.isEmpty()) {
                  unfilledInside.intersect(coarseLocalMFBox);
                  assert(unfilledInside.isEmpty());
                }
              }
            }
        }
    }

    //cout << myproc << ":  " << "About to CollectData" << endl;

    multiFabCopyDesc.CollectData();

    //cout << myproc << ":  " << "After CollectData" << endl;
// -----------

    // AmrLevel::isValid(bool bDoSync)

    //int myproc = ParallelDescriptor::MyProc();
    //PArray<AmrLevel> &amrLevels = amrLevel.parent->getAmrLevels();

    for(int currentIndex = 0; currentIndex < destBoxes.length(); ++currentIndex) {

      //Box destBox(destBoxes[currentIndex]);
      //currentFillPatchedFab.resize(destBox, numComps);
      //currentFillPatchedFab.setVal(1.e30);

      for(int currentLevel = 0; currentLevel <= finestFillLevel; ++currentLevel) {
        //StateData& currentState = amrLevels[currentLevel].state[stateIndex];
        const Box &currentPDomain = probDomain[currentLevel];

	if(myproc != procWithFabs) {
	  break;
	}
        for(int currentBox = 0;
            currentBox < fillBoxId[currentIndex][currentLevel].length();
            ++currentBox)
        {
            Box tempCoarseBox(
		       fillBoxId[currentIndex][currentLevel][currentBox][0].box());
            FArrayBox tempCoarseDestFab(tempCoarseBox, numComps);
            tempCoarseDestFab.setVal(1.e30);
            multiFabCopyDesc.FillFab(stateDataMFId[currentLevel],
			  fillBoxId[currentIndex][currentLevel][currentBox][0],
			  tempCoarseDestFab);

            Box intersectDestBox(savedFineBox[currentIndex][currentLevel][currentBox]);
            intersectDestBox &= destFabs[currentIndex]->box();

            const BoxArray &filledBoxes =
                fillBoxId[currentIndex][currentLevel][currentBox][0].FilledBoxes();
            BoxArray fboxes(filledBoxes);
            FArrayBox *copyFromThisFab;
            const BoxArray *copyFromTheseBoxes;
            FArrayBox tempCurrentFillPatchedFab;

            if(intersectDestBox.ok()) {
              if(currentLevel != finestFillLevel) {
                    //// Get boundary conditions for this patch.
                    //Array<BCRec> bcCoarse(numComps);
                    //const StateDescriptor &desc =
                         //amrLevels[currentLevel].desc_lst[stateIndex];
                    //setBC(intersectDestBox, currentPDomain, srcComp, 0, numComps,
                          //desc.getBCs(), bcCoarse);
                    fboxes.refine(cumulativeRefRatios[currentLevel]);
                    // Interpolate up to fine patch.
                    tempCurrentFillPatchedFab.resize(intersectDestBox, numComps);
		    PcInterp(tempCurrentFillPatchedFab,
			     tempCoarseDestFab,
			     intersectDestBox,
			     cumulativeRefRatios[currentLevel]);
                    copyFromThisFab = &tempCurrentFillPatchedFab;
                    copyFromTheseBoxes = &fboxes;
              } else {
                    copyFromThisFab = &tempCoarseDestFab;
                    copyFromTheseBoxes = &filledBoxes;
              }
              for(int iFillBox = 0; iFillBox < copyFromTheseBoxes->length();
                  ++iFillBox)
              {
                    Box srcdestBox((*copyFromTheseBoxes)[iFillBox]);
                    srcdestBox &= destFabs[currentIndex]->box();
                    srcdestBox &= intersectDestBox;
                    if(srcdestBox.ok()) {
                        destFabs[currentIndex]->copy(*copyFromThisFab,
                                                   srcdestBox, 0, srcdestBox,
                                                   destComp, numComps);
                    }
              }
            }
        }
      }  // end for(currentLevel...)
    }  // end for(currentIndex...)


    //cout << myproc << ":  " << "_out AmrData::FillVar(...)" << endl;

}    // end FillVar for a fab on a single processor


// ---------------------------------------------------------------
void AmrData::FillVar(FArrayBox &dest, int level, const aString &nm) {
   ParallelDescriptor::Abort("Error:  should not be in AmrData::FillVar(Fab, int aString");
/*
   assert(level >= 0 && level <= finestLevel);

   int i;
   Box dbox(dest.box());
   Box sbox(dbox);
   //sbox.grow(s_wid);

   // can we fill box entirely from within system interior?
   Box sys(probDomain[level]);
   int inside_domain = sys.contains(sbox);
      
   // if not, can it be filled using existing bndry regions?
   sys.grow(boundaryWidth);
   assert(sys.contains(sbox));

   Box ovlp(dbox);
   ovlp &= probDomain[level];
   if(ovlp.ok()) {   // fill on the interior
      if(level > 0) {
         BoxDomain fd;
         fd.add(ovlp);
	 for(MultiFabIterator gpli(GetGrids(level, StateNumber(nm)));
	     gpli.isValid(); ++gpli)
	 {
           if(ovlp.intersects(gpli.validbox())) {
   	     fd.rmBox(gpli.validbox());
           }
         }
         Box fine_unfilled = fd.minimalBox();
      
         if(fine_unfilled.ok()) {
            int lrat = refRatio[level-1];
            // not all filled, must interpolate from a coarser level.
            fd.simplify();
	    BoxDomainIterator bli(fd);
            while(bli) {
               Box fine_box(bli());
               Box crse_box = coarsen(fine_box,lrat);

               FArrayBox crse_patch(crse_box,1);
	       FillVar(crse_patch,level-1,nm);
               PcInterp(dest, crse_patch,fine_box,lrat);

	      bli++;
            }
         }
         fd.clear();
      }  // level > 0
   }     // sbox ok


   // now, overwrite with good data from this level
   //dataGrids[level]->Derive(dest, level, nm.c_str(), StateNumber(nm), true, 0);
   GetGrids(level, StateNumber(nm)).copy(dest, 0, 0, 1);

   // now fill with bndry regions
   if( ! inside_domain) {
      for(i = 0; i < nRegions; i++) {
         FArrayBox *reg = regions[level][i];
	 Box regbox(reg->box());
	 Box ovlp(regbox);
	 ovlp &= dbox;
	 if(ovlp.ok()) {
	    Box ovlp_domain(grow(ovlp,0));
	    if(regbox.contains(ovlp_domain)) {
	       dest.copy(*reg, ovlp, StateNumber(nm), ovlp, 0, 1);
	    } else {
	       FArrayBox tmpdat(ovlp_domain,nComp);
	       FillPatch(tmpdat,level,ovlp_domain);
	       dest.copy(tmpdat, ovlp, StateNumber(nm), ovlp, 0, 1);
	    }
	 }
      }
   }
*/
}    // end fillVar


// ---------------------------------------------------------------
void AmrData::FillPatch(FArrayBox &dest, int level, const Box &subbox) {
   ParallelDescriptor::Abort("Error:  should not be in AmrData::FillPatch");
/*
   assert(level >= 0 && level <= finestLevel);

   Box dbox(dest.box());
   assert(dbox.contains(subbox));

   Box sys(probDomain[level]);     // is box contained in system interior?
   int inside_domain = sys.contains(subbox);
   
   sys.grow(boundaryWidth);        // is box too large for system?
   assert(sys.contains(subbox));

   Box sbox(subbox);
   sbox &= probDomain[level];
   if(sbox.ok()) {                 // fill on the interior
      if(level > 0) {              // can we fill entire patch at this level?
         BoxDomain fd;
         fd.add(sbox);
         for(MultiFabIterator gpli(GetGrids(level, iComp)); gpli.isValid(); ++gpli) {
            if(sbox.intersects(gpli.validbox())) {
   	       fd.rmBox(gpli.validbox());
           }
         }
         Box fine_unfilled = fd.minimalBox();
      
         if(fine_unfilled.ok()) {
            int lrat = refRatio[level-1];
            // not all filled, must interpolate from a coarser level.
            fd.simplify();
	    BoxDomainIterator bli(fd);
	    while(bli) {
               Box fine_box(bli());

	    // widen coarse box and fill it
            Box crse_box = coarsen(fine_box,lrat);
	    crse_box.grow(1);
            FArrayBox crse_patch(crse_box,nComp);
	    FillPatch(crse_patch,level-1,crse_box);
            Interp(dest, crse_patch,fine_box,lrat);

	      bli++;
            }
         }
         fd.clear();
      }  // end if(level > 0)
   }   // end if(sbox...)


   // now, overwrite with good data from this level
   for(MultiFabIterator gpli(GetGrids(level, iComp)); gpli.isValid(); ++gpli) {
     if(sbox.intersects(gpli.validbox())) {
       dest.copy(gpli(), sbox);
     }
   }

   // now fill with bndry regions
   if( ! inside_domain) {
      for(int i = 0; i < nRegions; i++) {
         FArrayBox *reg = regions[level][i];
         dest.copy(*reg,subbox);
      }
   }
*/
}  // end FillPatch


// ---------------------------------------------------------------
void AmrData::FillInterior(FArrayBox &dest, int level, const Box &subbox) {
   ParallelDescriptor::Abort("Error:  should not be in AmrData::FillInterior");
/*
   assert(level >= 0 && level <= finestLevel);

   Box dbox(dest.box());
   assert(dbox.contains(subbox));

   // now fill from interior region by using piecewise constant interpolation
   for(int lev = 0; lev < level; lev++) {
      // compute refinement ratio
      int lrat = 1;
      for(int i = lev; i < level; i++) {
        lrat *= refRatio[i];
      }
      // intersect with grids at this level
      for(MultiFabIterator gpli(GetGrids(lev, iComp)); gpli.isValid(); ++gpli) {
        Box gbox(gpli.validbox());
	Box ovlp(refine(gbox,lrat));
	ovlp &= subbox;
	if(ovlp.ok()) {
	  Box crse_box(coarsen(ovlp,lrat));
	  FArrayBox crse(crse_box,nComp);
	  crse.copy(gpli(), crse_box);
	  PcInterp(dest, crse,ovlp,lrat);
	}
      }
   }

   // fill by copy at given level
   for(MultiFabIterator gpli(GetGrids(level, iComp)); gpli.isValid(); ++gpli) {
     Box ovlp(gpli.validbox());
     ovlp &= subbox;
     if(ovlp.ok()) {
       dest.copy(gpli(), ovlp);
     }
   }
*/
}


// ---------------------------------------------------------------
int AmrData::NumDeriveFunc() const {
  return (plotVars.length());
}


// ---------------------------------------------------------------
// return true if the given name is the name of a plot variable
// that can be derived from what is known.
bool AmrData::CanDerive(const aString &name) const {
   for(int i = 0; i < plotVars.length(); i++) {
     if(plotVars[i] == name) {
       return true;
     }
   }
   return false;
}


// ---------------------------------------------------------------
// output the list of variables that can be derived
void AmrData::ListDeriveFunc(ostream &os) const {
   for(int i = 0; i < plotVars.length(); i++) {
     os << plotVars[i] << endl;
   }
}


/*
// ---------------------------------------------------------------
bool AmrData::PointValue(Array<Box> &pointBox, int &intersectLevel,
		 Box &intersectGrid, aString &dataValue,
		 int coarseLevel, int fineLevel,
		 const aString &currentDerived, aString formatString)
{
  int lev;
  Box gridBox;
  assert(coarseLevel >= 0 && coarseLevel <= finestLevel);
  assert(fineLevel >= 0 && fineLevel <= finestLevel);
  for(lev = fineLevel; lev >= coarseLevel; lev--) {
    for(MultiFabIterator gpli(GetGrids(lev, StateNumber(currentDerived),
			      pointBox[lev]));
	gpli.isValid(); ++gpli)
    {
      gridBox = gpli.validbox();
      if(gridBox.intersects(pointBox[lev])) {
        Box dataBox(gridBox);
        dataBox &= pointBox[lev];
        FArrayBox *dataFab = new FArrayBox(dataBox, 1);
	dataFab->copy(gpli(), StateNumber(currentDerived), 0, 1);

	char tdv[LINELENGTH];
        sprintf(tdv, formatString.c_str(), (dataFab->dataPtr())[0]);
	dataValue = tdv;
	intersectLevel = lev;
        intersectGrid = gridBox;
        delete dataFab;
        return true;
      }
    }
  }
  return false;
}  // end PointValue
*/


// ---------------------------------------------------------------
int AmrData::NIntersectingGrids(int level, const Box &b) const {
  assert(level >=0 && level <= finestLevel);
  assert(b.ok());

  int nGrids(0);
  if(fileType == FAB) {
    nGrids = 1;
  } else {
    const BoxArray &visMFBA = visMF[level]->boxArray();
    for(int boxIndex = 0; boxIndex < visMFBA.length(); ++boxIndex) {
      if(b.intersects(visMFBA[boxIndex])) {
        ++nGrids;
      }
    }
  }
  return nGrids;
}

// ---------------------------------------------------------------
int AmrData::FinestContainingLevel(const Box &b, int startLevel) const {
  assert(startLevel >= 0 && startLevel <= finestLevel);
  assert(b.ok());

  if(fileType == FAB) {
    return 0;
  } else {
    Box levelBox(b);
    for(int level = startLevel; level > 0; --level) {
      const BoxArray &visMFBA = visMF[level]->boxArray();
      if(visMFBA.contains(levelBox)) {
        return level;
      }
      levelBox.coarsen(refRatio[level - 1]);
    }
  }
  return 0;
}


// ---------------------------------------------------------------
MultiFab &AmrData::GetGrids(int level, int componentIndex) {

  for(MultiFabIterator mfi(*dataGrids[level][componentIndex]);
      mfi.isValid(); ++mfi)
  {
    DefineFab(level, componentIndex, mfi.index());
  }

  return *dataGrids[level][componentIndex];
}


// ---------------------------------------------------------------
MultiFab &AmrData::GetGrids(int level, int componentIndex, const Box &onBox) {

  if(fileType == FAB) {
  } else {
    for(MultiFabIterator mfi(*dataGrids[level][componentIndex]);
        mfi.isValid(); ++mfi)
    {
      if(onBox.intersects(visMF[level]->boxArray()[mfi.index()])) {
        DefineFab(level, componentIndex, mfi.index());
      }
    }
  }

  return *dataGrids[level][componentIndex];
}


// ---------------------------------------------------------------
bool AmrData::DefineFab(int level, int componentIndex, int fabIndex) {

  if( ! dataGridsDefined[level][componentIndex][fabIndex]) {
    dataGrids[level][componentIndex]->setFab(fabIndex,
			  visMF[level]->readFAB(fabIndex, componentIndex));
    dataGridsDefined[level][componentIndex][fabIndex] = true;
  }
  return true;
}


// ---------------------------------------------------------------
bool AmrData::MinMax(const Box &onBox, const aString &derived, int level,
		     Real &dataMin, Real &dataMax)
{
  assert(level >= 0 && level <= finestLevel);
  assert(onBox.ok());

  bool valid(false);  // does onBox intersect any grids (are minmax valid)
  //FArrayBox *fabPtr;
  Real min, max;
  dataMin =  AV_BIG_REAL;
  dataMax = -AV_BIG_REAL;
  Box overlap;

  //
  //  our strategy here is to use the VisMF min and maxes if possible
  //  first, test if onBox completely contains each multifab box
  //  if so, use VisMF min and max
  //  if not, test if VisMF min and max are within dataMin and dataMax
  //  if so, use VisMF min and max
  //

  int compIndex(StateNumber(derived));

  if(fileType == FAB) {
    for(MultiFabIterator gpli(*dataGrids[level][compIndex]);
	gpli.isValid(); ++gpli)
    {
      if(onBox.intersects(dataGrids[level][compIndex]->boxArray()[gpli.index()])) {
          valid = true;
          overlap = onBox;
          overlap &= gpli.validbox();
          min = gpli().min(overlap, 0);
          max = gpli().max(overlap, 0);

          dataMin = Min(dataMin, min);
          dataMax = Max(dataMax, max);
      }
    }
  } else {
    for(MultiFabIterator gpli(*dataGrids[level][compIndex]);
	gpli.isValid(); ++gpli)
    {
      Real visMFMin(visMF[level]->min(gpli.index(), compIndex));
      Real visMFMax(visMF[level]->max(gpli.index(), compIndex));
      if(onBox.contains(gpli.validbox())) {
        dataMin = Min(dataMin, visMFMin);
        dataMax = Max(dataMax, visMFMax);
        valid = true;
      } else if(onBox.intersects(visMF[level]->boxArray()[gpli.index()])) {
        if(visMFMin < dataMin || visMFMax > dataMax) {  // do it the hard way
	  DefineFab(level, compIndex, gpli.index());
          valid = true;
          overlap = onBox;
          overlap &= gpli.validbox();
          //fabPtr = new FArrayBox(overlap, 1);
          //fabPtr->copy(gpli(), 0, 0, 1);
          //min = fabPtr->min(0);
          //max = fabPtr->max(0);
          min = gpli().min(overlap, 0);
          max = gpli().max(overlap, 0);

          //delete fabPtr;
          dataMin = Min(dataMin, min);
          dataMax = Max(dataMax, max);
        }  // end if(visMFMin...)
      }
    }
  }

  ParallelDescriptor::ReduceRealMin(dataMin);
  ParallelDescriptor::ReduceRealMax(dataMax);

  return valid;
}  // end MinMax


// ---------------------------------------------------------------
int AmrData::StateNumber(const aString &statename) const {
  for(int ivar = 0; ivar < plotVars.length(); ivar++) {
    if(statename == plotVars[ivar]) {
      return ivar;
    }
  }
  return(-1);
}


// ---------------------------------------------------------------
void AmrData::Interp(FArrayBox &fine, FArrayBox &crse,
                     const Box &fine_box, int lrat)
{
   assert(fine.box().contains(fine_box));
   Box crse_bx(coarsen(fine_box,lrat));
   Box fslope_bx(refine(crse_bx,lrat));
   Box cslope_bx(crse_bx);
   cslope_bx.grow(1);
   assert(crse.box() == cslope_bx);

   // alloc temp space for coarse grid slopes
   long cLen = cslope_bx.numPts();
   Real *cslope = new Real[BL_SPACEDIM*cLen];
   long loslp    = cslope_bx.index(crse_bx.smallEnd());
   long hislp    = cslope_bx.index(crse_bx.bigEnd());
   long cslope_vol = cslope_bx.numPts();
   long clo = 1 - loslp;
   long chi = clo + cslope_vol - 1;
   cLen = hislp - loslp + 1;

   // alloc temp space for one strip of fine grid slopes
   int dir;
   int fLen = fslope_bx.longside(dir);
   Real *fdat   = new Real[(BL_SPACEDIM+2)*fLen];
   Real *foff   = fdat + fLen;
   Real *fslope = foff + fLen;


   // alloc tmp space for slope calc and to allow for vectorization
   const int *fblo = fine_box.loVect();
   const int *fbhi = fine_box.hiVect();
   const int *cblo = crse_bx.loVect();
   const int *cbhi = crse_bx.hiVect();
   const int *fslo = fslope_bx.loVect();
   const int *fshi = fslope_bx.hiVect();

   FORT_CNTRP(fine.dataPtr(0),ARLIM(fine.loVect()),ARLIM(fine.hiVect()),
              fblo,fbhi,fine.nComp(),lrat,
              crse.dataPtr(0),clo,chi,cblo,cbhi,fslo,fshi,
              cslope,cLen,fslope,fdat,fLen,foff);

   delete [] fdat;
   delete [] cslope;
}


// ---------------------------------------------------------------
void AmrData::PcInterp(FArrayBox &fine, const FArrayBox &crse,
                       const Box &subbox, int lrat)
{
   assert(fine.box().contains(subbox));
   assert(fine.nComp() == crse.nComp());
   Box cfine(crse.box());
   cfine.refine(lrat);
   Box fine_ovlp(subbox);
   fine_ovlp &= cfine;
   if(fine_ovlp.ok()) {
      const int *fblo = fine_ovlp.smallEnd().getVect();
      const int *fbhi = fine_ovlp.bigEnd().getVect();
      Box crse_ovlp(fine_ovlp);
      crse_ovlp.coarsen(lrat);
      const int *cblo = crse_ovlp.smallEnd().getVect();
      const int *cbhi = crse_ovlp.bigEnd().getVect();
      Box fine_temp(crse_ovlp);
      fine_temp.refine(lrat);
      int tlo = fine_temp.smallEnd()[0];
      int thi = fine_temp.bigEnd()[0];
      Real *tempSpace = new Real[thi-tlo+1];
      FORT_PCNTRP(fine.dataPtr(0),ARLIM(fine.loVect()),ARLIM(fine.hiVect()),
                  fblo,fbhi, lrat,fine.nComp(),
                  crse.dataPtr(),ARLIM(crse.loVect()),ARLIM(crse.hiVect()),
                  cblo,cbhi, tempSpace,tlo,thi);

      delete [] tempSpace;
   }
}


// ---------------------------------------------------------------
FArrayBox *AmrData::ReadGrid(istream &is, int numVar) {
   long i, gstep;
   Real time;
   static int grid_count = 0;
   Box gbox;
   int glev;

   int gid  = grid_count;
   grid_count++;

   is >> gbox >> glev;
   VSHOWVAL(Verbose(), gbox)
   VSHOWVAL(Verbose(), glev)

   is >> gstep >> time;
   VSHOWVAL(Verbose(), gstep)
   VSHOWVAL(Verbose(), time)

   for(i = 0; i < BL_SPACEDIM; i++) {
     Real xlo, xhi;
     is >> xlo >> xhi;  // unused
     if(Verbose()) {
       cout << "xlo xhi [" << i << "] = " << xlo << "  " << xhi << endl;
     }
   }
   while (is.get() != '\n') {
     ;
   }

   FArrayBox *fabPtr = new FArrayBox(gbox, numVar);
   int whileTrap = 0;
   int ivar = 0;
   //  optimize this for numVar == newdat.nComp()
   while(ivar < numVar) {
     FArrayBox tempfab(is);
     fabPtr->copy(tempfab, 0, ivar, tempfab.nComp());
     ivar += tempfab.nComp();
     if(++whileTrap > 256) {   // an arbitrarily large number
       cerr << "Error in GridPlot:  whileTrap caught loop." << endl;
       exit(-4);
     }
   }

   if(Verbose()) {
     cout << "Constructing Grid, lev = " << glev << "  id = " << gid;
     cout << " box = " << gbox << endl;
   }
  return fabPtr;
}
// ---------------------------------------------------------------
// ---------------------------------------------------------------
