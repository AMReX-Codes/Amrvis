//BL_COPYRIGHT_NOTICE

//
// $Id: AmrData.cpp,v 1.34 1999-05-10 18:54:17 car Exp $
//

// ---------------------------------------------------------------
// AmrData.cpp
// ---------------------------------------------------------------
#include "AmrData.H"
#include "ArrayLim.H"
#include "BoxDomain.H"
#include "Misc.H"
#include "VisMF.H"

#ifdef BL_USE_NEW_HFILES
#include <iostream>
using std::ios;
#include <cstdio>
#else
#include <iostream.h>
#include <stdio.h>
#endif

#ifdef SHOWVAL
#undef SHOWVAL
#endif

#define SHOWVAL(val) { cout << #val << " = " << val << endl; }

#ifdef VSHOWVAL
#undef VSHOWVAL
#endif

#define VSHOWVAL(verbose, val) { if(verbose) { \
                 cout << #val << " = " << val << endl; } }


#if defined( BL_FORT_USE_UPPERCASE )
#  if (BL_SPACEDIM == 2)
#    define   FORT_CINTERP     CINTERP2D
#    define   FORT_PCINTERP    PCINTERP2D
#  elif (BL_SPACEDIM == 3)
#    define   FORT_CINTERP     CINTERP3D
#    define   FORT_PCINTERP    PCINTERP3D
#  endif
#elif defined( BL_FORT_USE_LOWERCASE )
#  if (BL_SPACEDIM == 2)
#    define   FORT_CINTERP     cinterp2d
#    define   FORT_PCINTERP    pcinterp2d
#  elif (BL_SPACEDIM == 3)
#    define   FORT_CINTERP     cinterp3d
#    define   FORT_PCINTERP    pcinterp3d
#  endif
#else
#  if (BL_SPACEDIM == 2)
#    define   FORT_CINTERP     cinterp2d_
#    define   FORT_PCINTERP    pcinterp2d_
#  elif (BL_SPACEDIM == 3)
#    define   FORT_CINTERP     cinterp3d_
#    define   FORT_PCINTERP    pcinterp3d_
#  endif
#endif


extern "C" {
  void FORT_CINTERP(Real *fine, ARLIM_P(flo), ARLIM_P(fhi),
                  const int *fblo, const int *fbhi,
                  const int &nvar, const int &lratio,
		  const Real *crse, const int &clo, const int &chi,
		  const int *cslo, const int *cshi,
		  const int *fslo, const int *fshi,
		  Real *cslope, const int &c_len,
		  Real *fslope, Real *fdat, const int &f_len,
		  Real *foff);

  void FORT_PCINTERP(Real *fine, ARLIM_P(flo), ARLIM_P(fhi),
                   const int *fblo, const int *fbhi,
		   const int &lrat, const int &nvar,
		   const Real *crse, ARLIM_P(clo), ARLIM_P(chi),
		   const int *cblo, const int *cbhi,
		   Real *temp, const int &tlo, const int &thi);
};


bool AmrData::verbose = false;
int  AmrData::skipPltLines  = 0;
int  AmrData::sBoundaryWidth = 0;

// ---------------------------------------------------------------
AmrData::AmrData() {
  probSize.resize(BL_SPACEDIM, -1.0);
  probLo.resize(BL_SPACEDIM,  0.0);
  probHi.resize(BL_SPACEDIM, -1.0);
  plotVars.clear();
  nRegions = 0;
  boundaryWidth = 0;
}


// ---------------------------------------------------------------
AmrData::~AmrData() {
   for(int i = 0; i < nRegions; ++i) {
     for(int lev = 0; lev <= finestLevel; ++lev) {
       delete regions[lev][i];
     }
   }

   for(int lev = 0; lev <= finestLevel; ++lev) {
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
   bCartGrid = false;
   if(filetype == FAB || filetype == MULTIFAB) {
     return ReadNonPlotfileData(filename, filetype);
   }

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

   if(verbose) {
     if(ParallelDescriptor::IOProcessor()) {
       cout << "AmrData::opening file = " << filename << endl;
     }
   }

   is.open(File.c_str(), ios::in);
   if(is.fail()) {
     if(ParallelDescriptor::IOProcessor()) {
      cerr << "Unable to open file: " << filename << endl;
     }
      return false;
   }

   aString skipBuff(LINELENGTH);
   for(i = 0; i < skipPltLines; i++) {
     skipBuff.getline(is);
     if(ParallelDescriptor::IOProcessor()) {
       cout << "Skipped line in pltfile = " << skipBuff << endl;
     }
   }

     is >> plotFileVersion;
     if(strncmp(plotFileVersion.c_str(), "CartGrid", 8) == 0) {
       bCartGrid = true;
     }
     if(verbose) {
       if(ParallelDescriptor::IOProcessor()) {
         cout << "Plot file version:  " << plotFileVersion << endl;
	 if(bCartGrid) {
	   cout << ":::: Found a CartGrid file type." << endl;
	 }
       }
     }

     // read list of variables
     is >> nComp;
      if(nComp < 1 || nComp > 1024) {  // arbitrarily limit to 1024
        if(ParallelDescriptor::IOProcessor()) {
          cerr << "Error in AmrData:  bad nComp = " << nComp << endl;
        }
        return false;
      }
      if(ParallelDescriptor::IOProcessor()) {
        VSHOWVAL(verbose, nComp);
      }

      plotVars.resize(nComp);
      plotName.getline(is); // eat white space left by << operator
      for(i = 0; i < nComp; ++i) {
        plotName.getline(is);
        plotVars[i] = plotName;
        if(ParallelDescriptor::IOProcessor()) {
          VSHOWVAL(verbose, plotName);
	}
      }

      int spacedim;
      is >>  spacedim >> time >> finestLevel;
      if(ParallelDescriptor::IOProcessor()) {
        VSHOWVAL(verbose, spacedim);
        VSHOWVAL(verbose, time);
        VSHOWVAL(verbose, finestLevel);
      }
      if(spacedim != BL_SPACEDIM) {
        if(ParallelDescriptor::IOProcessor()) {
	  cerr << endl << " ~~~~ Error:  You are using " << BL_SPACEDIM
	       << "D amrvis "
	       << "to look at a " << spacedim << "D file." << endl << endl;
	}
	return false;
      }
      if(finestLevel < 0) {
        if(ParallelDescriptor::IOProcessor()) {
          cerr << "Error in AmrData:  bad finestLevel = " << finestLevel << endl;
	}
        return false;
      }
      for(i = 0; i < BL_SPACEDIM; i++) {
        is >> probLo[i];
        if(verbose) {
          if(ParallelDescriptor::IOProcessor()) {
	    cout << "probLo[" << i << "] = " << probLo[i] << endl;
	  }
	}
      }
      for(i = 0; i < BL_SPACEDIM; i++) {
        is >> probHi[i];
        if(verbose) {
          if(ParallelDescriptor::IOProcessor()) {
	    cout << "probHi[" << i << "] = " << probHi[i] << endl;
	  }
	}
      }
      if(verbose) {
        if(ParallelDescriptor::IOProcessor()) {
	  if(finestLevel > 0) {
	    cout << "Resizing refRatio to size = " << finestLevel << endl;
	  }
	}
      }
      if(finestLevel == 0) {
        refRatio.resize(1, 1);
      } else {
        refRatio.resize(finestLevel, -1);
      }
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
          if(verbose) {
            if(ParallelDescriptor::IOProcessor()) {
	      cout << "IntVect refRatio[" << i << "] = " << ivRefRatio << endl;
	    }
	  }
	  refRatio[i] = ivRefRatio[0];  // non-uniform ref ratios not supported
	} else {
          is >> refRatio[i];
	}
        if(verbose) {
          if(ParallelDescriptor::IOProcessor()) {
	    cout << "refRatio[" << i << "] = " << refRatio[i] << endl;
	  }
	}
      }
      for(i = 0; i < finestLevel; ++i ) {
        if(refRatio[i] < 2 || refRatio[i] > 32 ) {
          if(ParallelDescriptor::IOProcessor()) {
            cerr << "Error in AmrData:  bad refRatio at level " << i << " = "
	         << refRatio[i] << endl;
	  }
          return false;
        }
      }
      while(is.get() != '\n');
      probDomain.resize(finestLevel + 1);
      maxDomain.resize(finestLevel + 1);
      for(i = 0; i <= finestLevel; i++) {
        is >> probDomain[i];
	if(verbose) {
          if(ParallelDescriptor::IOProcessor()) {
	    cout << "probDomain[" << i << "] = " << probDomain[i] << endl;
	  }
	}
        if( ! probDomain[i].ok()) {
          if(ParallelDescriptor::IOProcessor()) {
            cerr << "Error in AmrData:  bad probDomain[" << i << "] = "
	         << probDomain[i] << endl;
	  }
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
      if(verbose) {
        if(ParallelDescriptor::IOProcessor()) {
	  cout << "Ignored levelSteps = " << lstepbuff << endl;
	}
      }
      
      dxLevel.resize(finestLevel + 1);
      for(i = 0; i <= finestLevel; i++) {
        dxLevel[i].resize(BL_SPACEDIM);
        for(k = 0; k < BL_SPACEDIM; k++) {
	  is >> dxLevel[i][k];
	  if(verbose) {
            if(ParallelDescriptor::IOProcessor()) {
	      cout << "dxLevel[" << i << "][" << k << "] = "
		   << dxLevel[i][k] << endl;
	    }
	  }
	}
      }

      for(i = 0; i < BL_SPACEDIM; i++) {
        probSize[i] = probHi[i] - probLo[i];
        if(probSize[i] <= 0.0 ) {
          if(ParallelDescriptor::IOProcessor()) {
            cerr << "Error in AmrData:  bad probSize[" << i << "] = "
	         << probSize[i] << endl;
	  }
          return false;
	}
      }

      is >> coordSys;
      if(ParallelDescriptor::IOProcessor()) {
        VSHOWVAL(verbose, coordSys);
      }
      while(is.get() != '\n') {
        ;  // do nothing
      }

      is >> width;   // width of bndry regions
      if(ParallelDescriptor::IOProcessor()) {
        VSHOWVAL(verbose, width);
      }
      while(is.get() != '\n') {
        ;  // do nothing
      }

   dataGrids.resize(finestLevel + 1);
   dataGridsDefined.resize(finestLevel + 1);

   int lev;
   boundaryWidth = Max(width, sBoundaryWidth);
   bool bRestrictDomain(maxDomain[0].ok());
   if(bRestrictDomain) {
      for(lev = 1; lev <= finestLevel; lev++) {
        maxDomain[lev] = refine(maxDomain[lev-1],refRatio[lev-1]);
      }
   }
   Array<Box> restrictDomain(finestLevel + 1);
   Array<Box> extendRestrictDomain(finestLevel + 1);
   regions.resize(finestLevel + 1);
   for(lev = 0; lev <= finestLevel; ++lev) {
      restrictDomain[lev] = probDomain[lev];
      if(bRestrictDomain) {
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
	if(verbose) {
          if(ParallelDescriptor::IOProcessor()) {
	    cout << "BNDRY REGION " << i << " : " << bli() << endl;
	    cout << "    numPts = " << bli().numPts() << endl;
	  }
	}
	++i;
	++bli;
      }
   }

   // if positive set up and read bndry databoxes
   if(width > 0) {
     if(ParallelDescriptor::IOProcessor()) {
        cerr << "Error in AmrData:  Boundary width > 0 not supported:  width = "
	     << width << endl;
     }
     return false;
   }  // end if(width...)

   // read all grids but only save those inside the restricted region

    visMF.resize(finestLevel + 1);
    compIndexToVisMFMap.resize(nComp);
    compIndexToVisMFComponentMap.resize(nComp);
    gridLocLo.resize(finestLevel + 1);
    gridLocHi.resize(finestLevel + 1);

    for(i = 0; i <= finestLevel; i++) {
      int nGrids;
      Real gTime;
      int iLevelSteps;
      is >> lev >> nGrids >> gTime >> iLevelSteps;
      if(ParallelDescriptor::IOProcessor()) {
        VSHOWVAL(verbose, lev);
        VSHOWVAL(verbose, nGrids);
        VSHOWVAL(verbose, gTime);
        VSHOWVAL(verbose, iLevelSteps);
      }
      if(i != lev) {
        if(ParallelDescriptor::IOProcessor()) {
	  cerr << "Level misrestart:mismatch on restart" << endl;
          cerr << "Error in AmrData:  Level mismatch:  read level " << lev
	       << " while expecting level " << i << endl;
	}
        return false;
      }
      if(nGrids < 1) {
        if(ParallelDescriptor::IOProcessor()) {
          cerr << "Error in AmrData:  bad nGrids = " << nGrids << endl;
	}
        return false;
      }

      gridLocLo[i].resize(nGrids);
      gridLocHi[i].resize(nGrids);
      for(int iloc = 0; iloc < nGrids; ++iloc) {
        gridLocLo[i][iloc].resize(BL_SPACEDIM);
        gridLocHi[i][iloc].resize(BL_SPACEDIM);
	for(int iDim = 0; iDim < BL_SPACEDIM; ++iDim) {
	  is >> gridLocLo[i][iloc][iDim] >>  gridLocHi[i][iloc][iDim];
          if(ParallelDescriptor::IOProcessor()) {
            VSHOWVAL(verbose, gridLocLo[i][iloc][iDim]);
            VSHOWVAL(verbose, gridLocHi[i][iloc][iDim]);
	  }
	}
      }

      // here we account for multiple multifabs in a plot file
      int currentIndexComp(0);
      int currentVisMF(0);
      dataGrids[i].resize(nComp);
      dataGridsDefined[i].resize(nComp);

      while(currentIndexComp < nComp) {

        aString mfNameRelative;
        is >> mfNameRelative;
        aString mfName(fileName);
#ifdef BL_PARALLEL_IO
        mfName += '/';
        mfName += mfNameRelative;
        VSHOWVAL(verbose, mfName);
        VSHOWVAL(verbose, mfNameRelative);
#endif /* BL_PARALLEL_IO */

        visMF[i].resize(currentVisMF + 1);  // this preserves previous ones
        visMF[i][currentVisMF] = new VisMF(mfName);
	int iComp(currentIndexComp);
        currentIndexComp += visMF[i][currentVisMF]->nComp();
	int currentVisMFComponent(0);
        for( ; iComp < currentIndexComp; ++iComp) {
          // make single component multifabs
          // defer reading the MultiFab data
          dataGrids[i][iComp] = new MultiFab(visMF[i][currentVisMF]->boxArray(), 1,
			                     visMF[i][currentVisMF]->nGrow(),
					     Fab_noallocate);
          dataGridsDefined[i][iComp].resize(visMF[i][currentVisMF]->length(),
					    false);
          compIndexToVisMFMap[iComp] = currentVisMF;
          compIndexToVisMFComponentMap[iComp] = currentVisMFComponent;
          ++currentVisMFComponent;
        }

        ++currentVisMF;
      }  // end while

    }  // end for(i...finestLevel)

   // fill a set of temporary bndry regions surrounding the
   // restricted domain by extension from interior data
   // only use this data in bndry regions that did not
   // get better data from interior or input bndry regions
   for(lev = 0; lev <= finestLevel; lev++) {
      Box inbox(restrictDomain[lev]);
      Box reg1(grow(restrictDomain[lev],boundaryWidth));
      Box reg2(grow(probDomain[lev],width));
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

   if(bRestrictDomain) {
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
  if(verbose) {
     cout << "AmrPlot::opening file = " << filename << endl;
  }

  fileName = filename;

#ifdef BL_USE_SETBUF
    VisMF::IO_Buffer io_buffer(VisMF::IO_Buffer_Size);
#endif

  time = 0;
  if(fileType == FAB) {
    finestLevel = 0;
  } else if(fileType == MULTIFAB) {
    finestLevel = 1;  // level zero is filler
  }
  probDomain.resize(finestLevel + 1);
  maxDomain.resize(finestLevel + 1);
  dxLevel.resize(finestLevel + 1);
  refRatio.resize(finestLevel + 1);
  if(fileType == FAB) {
    refRatio[0] = 1;
  } else if(fileType == MULTIFAB) {
    refRatio[0] = 2;
  }
  for(int iLevel(0); iLevel <= finestLevel; ++iLevel) {
    dxLevel[iLevel].resize(BL_SPACEDIM);
    for(i = 0; i < BL_SPACEDIM; i++) {
      probLo[i] = 0.0;
      probHi[i] = 1.0;  // arbitrarily
      probSize[i] = probHi[i] - probLo[i];
      dxLevel[iLevel][i] = 0.0;  // temporarily
    }
  }

  dataGrids.resize(finestLevel + 1);
  dataGridsDefined.resize(finestLevel + 1);

  if(fileType == FAB) {
    ifstream is;
    is.open(filename.c_str(), ios::in);
    if(is.fail()) {
       cerr << "Unable to open plotfile: " << filename << endl;
       return false;
    }

#ifdef BL_USE_SETBUF
    is.rdbuf()->setbuf(io_buffer.dataPtr(), io_buffer.length());
#endif

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
      newfab = new FArrayBox;
      is.seekg(0, ios::beg);
      newfab->readFrom(is, iComp);  // read the iComp component
      dataGrids[0][iComp]->setFab(0, newfab);
      dataGridsDefined[0][iComp].resize(1);
      dataGridsDefined[0][iComp][0] = true;
    }
    char fabname[64];  // arbitrarily
    plotVars.resize(nComp);
    for(i = 0; i < nComp; ++i) {
      sprintf(fabname, "%s%d", "Fab_", i);
      plotVars[i] = fabname;
    }
    probDomain[0] = newfab->box();
    for(i = 0; i < BL_SPACEDIM; ++i) {
      dxLevel[0][i] = 1.0 / probDomain[0].length(i);
    }
    is.close();

  } else if(fileType == MULTIFAB) {
    VisMF tempVisMF(filename);
    nComp = tempVisMF.nComp();
    probDomain[1] = tempVisMF.boxArray().minimalBox();
    probDomain[0] = probDomain[1];
    probDomain[0].coarsen(refRatio[0]);
    BoxArray mfBoxArray(tempVisMF.boxArray());
    BoxArray levelZeroBoxArray;
    levelZeroBoxArray.resize(1);
    levelZeroBoxArray.set(0, probDomain[0]);
    dataGrids[0].resize(nComp, NULL);
    dataGrids[1].resize(nComp, NULL);
    dataGridsDefined[0].resize(nComp);
    dataGridsDefined[1].resize(nComp);
    fabBoxArray.resize(1);
    fabBoxArray.set(0, probDomain[0]);

    int nGrow(0);
    char fabname[64];  // arbitrarily
    plotVars.resize(nComp);

    for(int iComp(0); iComp < nComp; ++iComp) {
      sprintf(fabname, "%s%d", "MultiFab_", iComp);
      plotVars[iComp] = fabname;

      for(int iDim(0); iDim < BL_SPACEDIM; ++iDim) {
        dxLevel[0][iDim] = 1.0 / probDomain[0].length(iDim);
        dxLevel[1][iDim] = 1.0 / probDomain[1].length(iDim);
      }

      // set the level zero multifab
      dataGridsDefined[0][iComp].resize(1, false);
      dataGrids[0][iComp] = new MultiFab;
      dataGrids[0][iComp]->define(levelZeroBoxArray, nGrow, Fab_noallocate);
      FArrayBox *newfab = new FArrayBox(probDomain[0], 1);
      Real levelZeroValue(tempVisMF.min(0, iComp) -
		  ((tempVisMF.max(0, iComp) - tempVisMF.min(0, iComp)) / 256.0));
      newfab->setVal(levelZeroValue);
      dataGrids[0][iComp]->setFab(0, newfab);
      dataGridsDefined[0][iComp][0] = true;

    }  // end for(iComp...)


      // set the level one multifab

      // here we account for multiple multifabs in a plot file
      int currentIndexComp(0);
      int currentVisMF(0);
      visMF.resize(finestLevel + 1);
      compIndexToVisMFMap.resize(nComp);
      compIndexToVisMFComponentMap.resize(nComp);

      while(currentIndexComp < nComp) {
        visMF[1].resize(currentVisMF + 1);  // this preserves previous ones
        visMF[1][currentVisMF] = new VisMF(filename);
        int iComp(currentIndexComp);
        currentIndexComp += visMF[1][currentVisMF]->nComp();
        for(int currentVisMFComponent(0); iComp < currentIndexComp; ++iComp) {
          // make single component multifabs for level one
          dataGrids[1][iComp] = new MultiFab(visMF[1][currentVisMF]->boxArray(), 1,
                                             visMF[1][currentVisMF]->nGrow(),
                                             Fab_noallocate);
          dataGridsDefined[1][iComp].resize(visMF[1][currentVisMF]->length(),
                                            false);
          compIndexToVisMFMap[iComp] = currentVisMF;
          compIndexToVisMFComponentMap[iComp] = currentVisMFComponent;
          ++currentVisMFComponent;
        }

        ++currentVisMF;
      }  // end while





  }  // end if(fileType...)

  return true;
}


// ---------------------------------------------------------------
void AmrData::CellLoc(int lev, IntVect ix, Array<Real> &pos) const {
   BL_ASSERT(pos.length() == dxLevel[lev].length());
   for(int i = 0; i < BL_SPACEDIM; i++) {
      pos[i] = probLo[i] + (dxLevel[lev][i])*(0.5 + Real(ix[i]));
   }
}


// ---------------------------------------------------------------
void AmrData::LoNodeLoc(int lev, IntVect ix, Array<Real> &pos) const {
   BL_ASSERT(pos.length() == dxLevel[lev].length());
   for(int i = 0; i < BL_SPACEDIM; i++) {
      pos[i] = probLo[i] + (dxLevel[lev][i])*Real(ix[i]);
   }
}


// ---------------------------------------------------------------
void AmrData::HiNodeLoc(int lev, IntVect ix, Array<Real> &pos) const {
   BL_ASSERT(pos.length() == dxLevel[lev].length());
   for(int i = 0; i < BL_SPACEDIM; i++) {
      pos[i] = probLo[i] + (dxLevel[lev][i])*Real(ix[i]+1);
   }
}


// ---------------------------------------------------------------
void AmrData::FillVar(FArrayBox *destFab, const Box &destBox,
		      int finestFillLevel, const aString &varname, int procWithFabs)
{
  Array<FArrayBox *> destFabs(1);
  Array<Box> destBoxes(1);
  destFabs[0] = destFab;
  destBoxes[0] = destBox;

  FillVar(destFabs, destBoxes, finestFillLevel, varname, procWithFabs);
}


// ---------------------------------------------------------------
void AmrData::FillVar(MultiFab &destMultiFab, int finestFillLevel,
		      const aString &varname, int destcomp)
{
  int numFillComps(1);
  Array<aString> varNames(numFillComps);
  Array<int> destComps(numFillComps);
  varNames[0]  = varname;
  destComps[0] = destcomp;
  FillVar(destMultiFab, finestFillLevel, varNames, destComps);
}


// ---------------------------------------------------------------
void AmrData::FillVar(MultiFab &destMultiFab, int finestFillLevel,
		      const Array<aString> &varNames,
		      const Array<int> &destFillComps)
{
// This function fills the destMultiFab which is defined on
// the finestFillLevel.

   BL_ASSERT(finestFillLevel >= 0 && finestFillLevel <= finestLevel);
   BoxArray destBoxes(destMultiFab.boxArray());
   for(int iIndex = 0; iIndex < destBoxes.length(); ++iIndex) {
     BL_ASSERT(probDomain[finestFillLevel].contains(destBoxes[iIndex]));
   }

    int myProc(ParallelDescriptor::MyProc());
    int srcComp(0);     // always 0 since AmrData uses single component MultiFabs
    int nFillComps(1);  // always
    int currentLevel;

    Array<int> cumulativeRefRatios(finestFillLevel + 1, -1);

    cumulativeRefRatios[finestFillLevel] = 1;
    for(currentLevel = finestFillLevel - 1; currentLevel >= 0; --currentLevel) {
      cumulativeRefRatios[currentLevel] = cumulativeRefRatios[currentLevel + 1] *
                                          refRatio[currentLevel];
    }

    BL_ASSERT(varNames.length() == destFillComps.length());
    int nFillVars(varNames.length());

  for(int currentFillIndex(0); currentFillIndex < nFillVars; ++currentFillIndex) {
    int destComp(destFillComps[currentFillIndex]);
    int stateIndex(StateNumber(varNames[currentFillIndex]));
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

    MultiFabCopyDescriptor multiFabCopyDesc;
    Array<MultiFabId> stateDataMFId(finestFillLevel + 1);
    for(currentLevel = 0; currentLevel <= finestFillLevel; ++currentLevel) {
      stateDataMFId[currentLevel] =
           multiFabCopyDesc.RegisterFabArray(dataGrids[currentLevel][stateIndex]);
    }

    BoxArray localMFBoxes(destBoxes.length());  // These are the ones
						  // we want to fillpatch.
    Array< Array< Array< Array<FillBoxId> > > > fillBoxId;
			          // [grid][level][fillablesubbox][oldnew]
			          // oldnew not used here
    Array< Array< Array<Box> > > savedFineBox;  // [grid][level][fillablesubbox]

    fillBoxId.resize(destBoxes.length());
    savedFineBox.resize(destBoxes.length());
    for(int iBox = 0; iBox < destBoxes.length(); ++iBox) {
      if(destMultiFab.DistributionMap()[iBox] == myProc) {
	localMFBoxes.set(iBox, destBoxes[iBox]);
        fillBoxId[iBox].resize(finestFillLevel + 1);
        savedFineBox[iBox].resize(finestFillLevel + 1);
      }
    }

    IndexType boxType(destBoxes[0].ixType());
    BoxList unfilledBoxesOnThisLevel(boxType);
    BoxList unfillableBoxesOnThisLevel(boxType);
    // Do this for all local fab boxes.
    for(int ibox = 0; ibox < localMFBoxes.length(); ++ibox) {
      if(destMultiFab.DistributionMap()[ibox] != myProc) {
	continue;
      }
        unfilledBoxesOnThisLevel.clear();
        BL_ASSERT(unfilledBoxesOnThisLevel.ixType() == boxType);
        BL_ASSERT(unfilledBoxesOnThisLevel.ixType() == localMFBoxes[ibox].ixType());
        unfilledBoxesOnThisLevel.add(localMFBoxes[ibox]);
        // Find the boxes that can be filled on each level--these are all
        // defined at their level of refinement.
        bool needsFilling = true;
        for(currentLevel = finestFillLevel; currentLevel >= 0 && needsFilling;
            --currentLevel)
        {
            unfillableBoxesOnThisLevel.clear();
            const Box &currentPDomain = probDomain[currentLevel];

	    int ufbLength = unfilledBoxesOnThisLevel.length();
            fillBoxId[ibox][currentLevel].resize(ufbLength);
            savedFineBox[ibox][currentLevel].resize(ufbLength);

            int currentBLI = 0;
            for(BoxListIterator bli(unfilledBoxesOnThisLevel); bli; ++bli) {
                BL_ASSERT(bli().ok());
                Box coarseDestBox(bli());
                Box fineTruncDestBox(coarseDestBox & currentPDomain);
                if(fineTruncDestBox.ok()) {
                  fineTruncDestBox.refine(cumulativeRefRatios[currentLevel]);
                  Box tempCoarseBox;
                  if(currentLevel == finestFillLevel) {
                    tempCoarseBox = fineTruncDestBox;
                  } else {
                    tempCoarseBox = fineTruncDestBox;
		    // check this vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
                    tempCoarseBox.coarsen(cumulativeRefRatios[currentLevel]);
                  }

                  savedFineBox[ibox][currentLevel][currentBLI] = fineTruncDestBox;
                  BL_ASSERT(localMFBoxes[ibox].intersects(fineTruncDestBox));

                  BoxList tempUnfillableBoxes(boxType);
                  fillBoxId[ibox][currentLevel][currentBLI].resize(1);
                  fillBoxId[ibox][currentLevel][currentBLI][0] = 
		      multiFabCopyDesc.AddBox(stateDataMFId[currentLevel],
					      tempCoarseBox, &tempUnfillableBoxes,
					      srcComp, 0, 1);

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
                  BL_ASSERT(unfilledInside.isEmpty());
                }
              }
            }
        }
    }

    multiFabCopyDesc.CollectData();


    for(int currentIndex = 0; currentIndex < destBoxes.length(); ++currentIndex) {
      if(destMultiFab.DistributionMap()[currentIndex] != myProc) {
	continue;
      }
      for(int currentLevel = 0; currentLevel <= finestFillLevel; ++currentLevel) {
        const Box &currentPDomain = probDomain[currentLevel];

        for(int currentBox = 0;
            currentBox < fillBoxId[currentIndex][currentLevel].length();
            ++currentBox)
        {
            Box tempCoarseBox(
		       fillBoxId[currentIndex][currentLevel][currentBox][0].box());
            FArrayBox tempCoarseDestFab(tempCoarseBox, 1);
            tempCoarseDestFab.setVal(1.e30);
            multiFabCopyDesc.FillFab(stateDataMFId[currentLevel],
			  fillBoxId[currentIndex][currentLevel][currentBox][0],
			  tempCoarseDestFab);

            Box intersectDestBox(savedFineBox[currentIndex][currentLevel][currentBox]);
            intersectDestBox &= destMultiFab[currentIndex].box();

            const BoxArray &filledBoxes =
                fillBoxId[currentIndex][currentLevel][currentBox][0].FilledBoxes();
            BoxArray fboxes(filledBoxes);
            FArrayBox *copyFromThisFab;
            const BoxArray *copyFromTheseBoxes;
            FArrayBox tempCurrentFillPatchedFab;

            if(intersectDestBox.ok()) {
              if(currentLevel != finestFillLevel) {
                    fboxes.refine(cumulativeRefRatios[currentLevel]);
                    // Interpolate up to fine patch.
                    tempCurrentFillPatchedFab.resize(intersectDestBox, nFillComps);
                    tempCurrentFillPatchedFab.setVal(1.e30);
		    BL_ASSERT(intersectDestBox.ok());
		    BL_ASSERT( tempCoarseDestFab.box().ok());
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
                    srcdestBox &= destMultiFab[currentIndex].box();
                    srcdestBox &= intersectDestBox;
                    if(srcdestBox.ok()) {
                      destMultiFab[currentIndex].copy(*copyFromThisFab,
                                                      srcdestBox, 0, srcdestBox,
                                                      destComp, nFillComps);
                    }
              }
            }
        }
      }  // end for(currentLevel...)
    }  // end for(currentIndex...)


  }  // end for(currentFillIndex...)


}


// ---------------------------------------------------------------
void AmrData::FillVar(Array<FArrayBox *> &destFabs, const Array<Box> &destBoxes,
		      int finestFillLevel, const aString &varname, int procWithFabs)
{

//
// This function fills dest only on procWithFabs.  All other dest
// pointers (on other processors) should be NULL.  destBox
// on all processors must be defined.
//

   BL_ASSERT(finestFillLevel >= 0 && finestFillLevel <= finestLevel);
   for(int iIndex = 0; iIndex < destBoxes.length(); ++iIndex) {
     BL_ASSERT(probDomain[finestFillLevel].contains(destBoxes[iIndex]));
   }

    int myproc(ParallelDescriptor::MyProc());
    int stateIndex(StateNumber(varname));
    int srcComp(0);
    int destComp(0);
    int numFillComps(1);

    int currentLevel;
    Array<int> cumulativeRefRatios(finestFillLevel + 1, -1);

    cumulativeRefRatios[finestFillLevel] = 1;
    for(currentLevel = finestFillLevel - 1; currentLevel >= 0; --currentLevel) {
      cumulativeRefRatios[currentLevel] = cumulativeRefRatios[currentLevel + 1] *
                                          refRatio[currentLevel];
    }

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

    MultiFabCopyDescriptor multiFabCopyDesc;
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
    // Do this for all local fab boxes.
    for(int ibox = 0; ibox < localMFBoxes.length(); ++ibox) {
        unfilledBoxesOnThisLevel.clear();
        BL_ASSERT(unfilledBoxesOnThisLevel.ixType() == boxType);
        BL_ASSERT(unfilledBoxesOnThisLevel.ixType() == localMFBoxes[ibox].ixType());
        unfilledBoxesOnThisLevel.add(localMFBoxes[ibox]);
        // Find the boxes that can be filled on each level--these are all
        // defined at their level of refinement.
        bool needsFilling = true;
        for(currentLevel = finestFillLevel; currentLevel >= 0 && needsFilling;
            --currentLevel)
        {
            unfillableBoxesOnThisLevel.clear();
            const Box &currentPDomain = probDomain[currentLevel];

	    int ufbLength = unfilledBoxesOnThisLevel.length();
            fillBoxId[ibox][currentLevel].resize(ufbLength);
            savedFineBox[ibox][currentLevel].resize(ufbLength);

            int currentBLI = 0;
            for(BoxListIterator bli(unfilledBoxesOnThisLevel); bli; ++bli) {
                BL_ASSERT(bli().ok());
                Box coarseDestBox(bli());
                Box fineTruncDestBox(coarseDestBox & currentPDomain);
                if(fineTruncDestBox.ok()) {
                  fineTruncDestBox.refine(cumulativeRefRatios[currentLevel]);
                  Box tempCoarseBox;
                  if(currentLevel == finestFillLevel) {
                    tempCoarseBox = fineTruncDestBox;
                  } else {
                    tempCoarseBox = fineTruncDestBox;
		    // check this vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
                    tempCoarseBox.coarsen(cumulativeRefRatios[currentLevel]);
                  }

                  savedFineBox[ibox][currentLevel][currentBLI] = fineTruncDestBox;
                  BL_ASSERT(localMFBoxes[ibox].intersects(fineTruncDestBox));

                  BoxList tempUnfillableBoxes(boxType);
                  fillBoxId[ibox][currentLevel][currentBLI].resize(1);
                  fillBoxId[ibox][currentLevel][currentBLI][0] = 
		      multiFabCopyDesc.AddBox(stateDataMFId[currentLevel],
					      tempCoarseBox, &tempUnfillableBoxes,
					      srcComp, destComp, numFillComps);

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
                  BL_ASSERT(unfilledInside.isEmpty());
                }
              }
            }
        }
    }

    multiFabCopyDesc.CollectData();


    for(int currentIndex = 0; currentIndex < destBoxes.length(); ++currentIndex) {
      for(int currentLevel = 0; currentLevel <= finestFillLevel; ++currentLevel) {
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
            FArrayBox tempCoarseDestFab(tempCoarseBox, numFillComps);
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
                    fboxes.refine(cumulativeRefRatios[currentLevel]);
                    // Interpolate up to fine patch.
                    tempCurrentFillPatchedFab.resize(intersectDestBox, numFillComps);
                    tempCurrentFillPatchedFab.setVal(1.e30);
		    BL_ASSERT(intersectDestBox.ok());
		    BL_ASSERT( tempCoarseDestFab.box().ok());
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
                                                   destComp, numFillComps);
                    }
              }
            }
        }
      }  // end for(currentLevel...)
    }  // end for(currentIndex...)
}    // end FillVar for a fab on a single processor


// ---------------------------------------------------------------
void AmrData::FillInterior(FArrayBox &dest, int level, const Box &subbox) {
   BoxLib::Abort("Error:  should not be in AmrData::FillInterior");
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


// ---------------------------------------------------------------
int AmrData::NIntersectingGrids(int level, const Box &b) const {
  BL_ASSERT(level >=0 && level <= finestLevel);
  BL_ASSERT(b.ok());

  int nGrids(0);
  if(fileType == FAB || (fileType == MULTIFAB && level == 0)) {
    nGrids = 1;
  } else {
    const BoxArray &visMFBA = visMF[level][0]->boxArray();
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
  BL_ASSERT(startLevel >= 0 && startLevel <= finestLevel);
  BL_ASSERT(b.ok());

  if(fileType == FAB) {
    return 0;
  } else {
    Box levelBox(b);
    for(int level = startLevel; level > 0; --level) {
      const BoxArray &visMFBA = visMF[level][0]->boxArray();
      if(visMFBA.contains(levelBox)) {
        return level;
      }
      levelBox.coarsen(refRatio[level - 1]);
    }
  }
  return 0;
}


// ---------------------------------------------------------------
int AmrData::FinestIntersectingLevel(const Box &b, int startLevel) const {
  BL_ASSERT(startLevel >= 0 && startLevel <= finestLevel);
  BL_ASSERT(b.ok());

  if(fileType == FAB) {
    return 0;
  } else {
    Box levelBox(b);
    for(int level = startLevel; level > 0; --level) {
      const BoxArray &visMFBA = visMF[level][0]->boxArray();

      for (int box = 0; box < visMFBA.length(); box++)
        if(visMFBA[box].intersects(levelBox)) {
          return level;}

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
  if(fileType == FAB || (fileType == MULTIFAB && level == 0)) {
    // do nothing
  } else {
    int whichVisMF(compIndexToVisMFMap[componentIndex]);
    for(MultiFabIterator mfi(*dataGrids[level][componentIndex]);
        mfi.isValid(); ++mfi)
    {
      if(onBox.intersects(visMF[level][whichVisMF]->boxArray()[mfi.index()])) {
        DefineFab(level, componentIndex, mfi.index());
      }
    }
  }
  return *dataGrids[level][componentIndex];
}


// ---------------------------------------------------------------
bool AmrData::DefineFab(int level, int componentIndex, int fabIndex) {

  if( ! dataGridsDefined[level][componentIndex][fabIndex]) {
    int whichVisMF(compIndexToVisMFMap[componentIndex]);
    int whichVisMFComponent(compIndexToVisMFComponentMap[componentIndex]);
    dataGrids[level][componentIndex]->setFab(fabIndex,
                visMF[level][whichVisMF]->readFAB(fabIndex, whichVisMFComponent));
    dataGridsDefined[level][componentIndex][fabIndex] = true;
  }
  return true;
}


// ---------------------------------------------------------------
bool AmrData::MinMax(const Box &onBox, const aString &derived, int level,
		     Real &dataMin, Real &dataMax)
{
  BL_ASSERT(level >= 0 && level <= finestLevel);
  BL_ASSERT(onBox.ok());

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

  if(fileType == FAB || (fileType == MULTIFAB && level == 0)) {
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
      int whichVisMF(compIndexToVisMFMap[compIndex]);
      int whichVisMFComponent(compIndexToVisMFComponentMap[compIndex]);
      Real visMFMin(visMF[level][whichVisMF]->min(gpli.index(),
		    whichVisMFComponent));
      Real visMFMax(visMF[level][whichVisMF]->max(gpli.index(),
		    whichVisMFComponent));
      if(onBox.contains(gpli.validbox())) {
        dataMin = Min(dataMin, visMFMin);
        dataMax = Max(dataMax, visMFMax);
        valid = true;
      } else if(onBox.intersects(visMF[level][whichVisMF]->
				 boxArray()[gpli.index()]))
      {
        if(visMFMin < dataMin || visMFMax > dataMax) {  // do it the hard way
	  DefineFab(level, compIndex, gpli.index());
          valid = true;
          overlap = onBox;
          overlap &= gpli.validbox();
          min = gpli().min(overlap, 0);
          max = gpli().max(overlap, 0);

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
   BL_ASSERT(fine.box().contains(fine_box));
   Box crse_bx(coarsen(fine_box,lrat));
   Box fslope_bx(refine(crse_bx,lrat));
   Box cslope_bx(crse_bx);
   cslope_bx.grow(1);
   BL_ASSERT(crse.box() == cslope_bx);

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

   FORT_CINTERP(fine.dataPtr(0),ARLIM(fine.loVect()),ARLIM(fine.hiVect()),
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
   BL_ASSERT(fine.box().contains(subbox));
   BL_ASSERT(fine.nComp() == crse.nComp());
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
      FORT_PCINTERP(fine.dataPtr(0),ARLIM(fine.loVect()),ARLIM(fine.hiVect()),
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
   VSHOWVAL(verbose, gbox)
   VSHOWVAL(verbose, glev)

   is >> gstep >> time;
   VSHOWVAL(verbose, gstep)
   VSHOWVAL(verbose, time)

   for(i = 0; i < BL_SPACEDIM; i++) {
     Real xlo, xhi;
     is >> xlo >> xhi;  // unused
     if(verbose) {
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

   if(verbose) {
     cout << "Constructing Grid, lev = " << glev << "  id = " << gid;
     cout << " box = " << gbox << endl;
   }
  return fabPtr;
}
// ---------------------------------------------------------------
// ---------------------------------------------------------------
