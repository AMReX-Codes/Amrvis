// -------------------------------------------------------------------
// VolRender.cpp
// -------------------------------------------------------------------
#include "VolRender.H"
#include "DataServices.H"
#include "GlobalUtilities.H"
#include "ParallelDescriptor.H"

#include <time.h>
#include <unistd.h>
#include <fcntl.h>

extern Real RadToDeg(Real angle);
extern Real DegToRad(Real angle);

#define VOLUMEBOXES 0
//#define VOLUMEBOXES 1

// -------------------------------------------------------------------
VolRender::VolRender()
          : bVolRenderDefined(false)
{
}


// -------------------------------------------------------------------
VolRender::VolRender(const Array<Box> &drawdomain, int mindrawnlevel,
		     int maxdrawnlevel, Palette *PalettePtr)
{
  minDrawnLevel = mindrawnlevel;
  maxDataLevel = maxdrawnlevel;
  drawnDomain = drawdomain;
  vpDataValid     = false;
  swfDataValid    = false;
  swfDataAllocated = false;
  palettePtr = PalettePtr;

  volData = NULL;

  voxelFields = 3;

  //voxel field variables:
  normalField = 0;
  normalOffset = vpFieldOffset(dummy_voxel, normal);
  normalSize = sizeof(short);
  normalMax = VP_NORM_MAX;

  densityField = 1;
  densityOffset = vpFieldOffset(dummy_voxel, density);
  densitySize = sizeof(unsigned char);
  densityMax = MaxPaletteIndex();


  gradientField = 2;
  gradientOffset = vpFieldOffset(dummy_voxel, gradient);
  gradientSize = sizeof(unsigned char);
  gradientMax = MaxPaletteIndex();

  lightingModel = true;
  paletteSize = 256;

  if(ParallelDescriptor::IOProcessor()) {
    // must initialize vpc before calling ReadTransferFile
    vpc = vpCreateContext();

    maxDenRampPts   = densityMax  + 1;
    maxShadeRampPts = normalMax   + 1;
    density_ramp    = new float[maxDenRampPts];
    shade_table     = new float[maxShadeRampPts];
    value_shade_table = new float[paletteSize];
    
    maxGradRampPts = gradientMax + 1;
    gradient_ramp  = new float[maxGradRampPts];
    gradientRampX  = NULL;
    gradientRampY  = NULL;

    densityRampX  = NULL;
    densityRampY  = NULL;

    aString rampFileName("vpramps.dat");
    ReadTransferFile(rampFileName);

    rows   = drawnDomain[maxDataLevel].length(XDIR);
    cols   = drawnDomain[maxDataLevel].length(YDIR);
    planes = drawnDomain[maxDataLevel].length(ZDIR);

    // --- describe the layout of the volume
    vpResult vpret = vpSetVolumeSize(vpc, rows, cols, planes);
    CheckVP(vpret, 1);

  }
  bVolRenderDefined = true;
}  // end VolRender()


// -------------------------------------------------------------------
VolRender::~VolRender() {
  assert(bVolRenderDefined);
  if(ParallelDescriptor::IOProcessor()) {
    vpDestroyContext(vpc);
    //cout << endl << "~~~~~~~~~~ volData = " << volData << endl << endl;
    //delete [] volData;
    if(swfDataAllocated) {
      delete [] swfData;
    }
    delete [] density_ramp;
    delete [] gradient_ramp;
    delete [] shade_table;
    delete [] value_shade_table;
  }
}


// -------------------------------------------------------------------
bool VolRender::AllocateSWFData() {
  assert(bVolRenderDefined);

  // --- create the big array
  swfDataSize = drawnDomain[maxDataLevel].numPts();
  cout << "swfData box size = "
       << drawnDomain[maxDataLevel] << "  " << swfDataSize << endl;

  swfData = new unsigned char[swfDataSize];
  if(swfData == NULL) {
    cerr << "Error in AmrPicture::ChangeDerived:  could not allocate "
         << swfDataSize << " bytes for swfData." << endl;
    swfDataAllocated = false;
  } else {
    swfDataAllocated = true;
  }
  return swfDataAllocated;
}


// -------------------------------------------------------------------
void VolRender::MakeSWFData(DataServices *dataServicesPtr,
			    Real rDataMin, Real rDataMax,
			    const aString &derivedName,
			    int iPaletteStart, int iPaletteEnd,
			    int iBlackIndex, int iWhiteIndex,
			    int iColorSlots)
{
  if(ParallelDescriptor::NProcs() == 1) {
    MakeSWFDataNProcs(dataServicesPtr, rDataMin, rDataMax,
	              derivedName, iPaletteStart, iPaletteEnd,
	              iBlackIndex, iWhiteIndex, iColorSlots);
  } else {
    MakeSWFDataNProcs(dataServicesPtr, rDataMin, rDataMax,
	              derivedName, iPaletteStart, iPaletteEnd,
	              iBlackIndex, iWhiteIndex, iColorSlots);
  }
}


// -------------------------------------------------------------------
void VolRender::MakeSWFDataNProcs(DataServices *dataServicesPtr,
			    Real rDataMin, Real rDataMax,
			    const aString &derivedName,
			    int iPaletteStart, int iPaletteEnd,
			    int iBlackIndex, int iWhiteIndex,
			    int iColorSlots)
{
  assert(bVolRenderDefined);
#if (VOLUMEBOXES)
  ParallelDescriptor::Abort("VOLUMEBOXES not implemented in parallel.");
#endif
  
  if(swfDataValid) {
    return;
  }
  
  if( ! swfDataAllocated) {
    if(ParallelDescriptor::IOProcessor()) {
      swfDataAllocated = AllocateSWFData();
    } else {
      swfDataAllocated = true;
    }
  }
  
  swfDataValid = true;
  clock_t time0 = clock();
  
  int maxDrawnLevel(maxDataLevel);
  Box gbox, grefbox;
  Box swfDataBox(drawnDomain[maxDrawnLevel]);
  FArrayBox swfFabData;
  if(ParallelDescriptor::IOProcessor()) {
    swfFabData.resize(swfDataBox, 1);
  }
  
  DataServices::Dispatch(DataServices::FillVarOneFab, dataServicesPtr,
                         &swfFabData, swfDataBox, maxDrawnLevel, derivedName);
  
  if(ParallelDescriptor::IOProcessor()) {
    Real gmin(rDataMin);
    Real gmax(rDataMax);
    Real globalDiff = gmax - gmin;
    Real oneOverGDiff;
    if(globalDiff < FLT_MIN) {
      oneOverGDiff = 0.0;  // so we dont divide by zero
    } else {
      oneOverGDiff = 1.0 / globalDiff;
    }
    int cSlotsAvail = iColorSlots - 1;
    
    cout << "Filling swfFabData..." << endl;
    
    // copy data into swfData and change to chars
    Real dat;
    char chardat;
    Real *dataPoint = swfFabData.dataPtr();
    
    int sindexbase;
    int srows   = swfDataBox.length(XDIR);
    int scols   = swfDataBox.length(YDIR);
    //int splanes = swfDataBox.length(ZDIR);
    int scolssrowstmp = scols*srows;
    int sstartr = swfDataBox.smallEnd(XDIR);
    int sstartc = swfDataBox.smallEnd(YDIR);
    int sstartp = swfDataBox.smallEnd(ZDIR);
    int sendr   = swfDataBox.bigEnd(XDIR);
    int sendc   = swfDataBox.bigEnd(YDIR);
    int sendp   = swfDataBox.bigEnd(ZDIR);
    
    Box gbox(swfDataBox);
    Box goverlap = gbox & drawnDomain[maxDrawnLevel];
    //grefbox = goverlap;
    //grefbox.refine(crr);
    
    int gstartr = gbox.smallEnd(XDIR);
    int gstartc = gbox.smallEnd(YDIR);
    int gstartp = gbox.smallEnd(ZDIR);
    
    int gostartr = goverlap.smallEnd(XDIR) - gstartr;
    int gostartc = goverlap.smallEnd(YDIR) - gstartc;
    int gostartp = goverlap.smallEnd(ZDIR) - gstartp;
    int goendr   = goverlap.bigEnd(XDIR)   - gstartr;
    int goendc   = goverlap.bigEnd(YDIR)   - gstartc;
    int goendp   = goverlap.bigEnd(ZDIR)   - gstartp;
    
    int grows   = gbox.length(XDIR);
    int gcols   = gbox.length(YDIR);
    //int gplanes = gbox.length(ZDIR);
    
    int gcolsgrowstmp = gcols*grows;
    int gpgcgrtmp, gcgrowstmp;
    //    unsigned int outputvalue = 0;
    for(int gp=gostartp; gp <= goendp; gp++) {
      gpgcgrtmp = gp*gcolsgrowstmp;
      for(int gc=gostartc; gc <= goendc; gc++) {
        gcgrowstmp = gpgcgrtmp + gc*grows;
        for(int gr=gostartr; gr <= goendr; gr++) {
          //dat = dataPoint[(gp*gcols*grows)+(gc*grows)+gr];  // works
          dat = dataPoint[gcgrowstmp + gr];
          dat = Max(dat,gmin); // clip data if out of range
          dat = Min(dat,gmax);
          chardat = (char)(((dat-gmin)*oneOverGDiff)*cSlotsAvail);
          chardat += (char)iPaletteStart;
          sindexbase =
            (((gp+gstartp)-sstartp) * scolssrowstmp) +
            ((sendc-((gc+gstartc))) * srows) +  // check this
            ((gr+gstartr)-sstartr);
          
          swfData[sindexbase] = chardat;
        }
      }
    }  // end for(gp...)
    cout<<endl;
    cout << "--------------- make swfData time = "
         << ((clock()-time0)/1000000.0) << endl;
  }
  
}  // end MakeSWFDataNProcs(...)


// -------------------------------------------------------------------
void VolRender::MakeSWFDataOneProc(DataServices *dataServicesPtr,
			    Real rDataMin, Real rDataMax,
			    const aString &derivedName,
			    int iPaletteStart, int iPaletteEnd,
			    int iBlackIndex, int iWhiteIndex,
			    int iColorSlots)
{
  assert(bVolRenderDefined);

  if(swfDataValid) {
    return;
  }

  if( ! swfDataAllocated) {
    AllocateSWFData();
  }

      AmrData &amrData = dataServicesPtr->AmrDataRef();
      swfDataValid = true;
      clock_t time0 = clock();

      int maxDrawnLevel = maxDataLevel;
      Box gbox, grefbox;
      Box swfDataBox = drawnDomain[maxDrawnLevel];

      Real *dataPoint, dat;
      int lev, crr;
      int grows, gcols, gr, gc, gp;
      int srows, scols, sr, sc, sp;
      //int gplanes splanes;
      int sindex, sindexbase;
      char chardat;
      Real gmin = rDataMin;
      Real gmax = rDataMax;
      Real globalDiff = gmax - gmin;
      Real oneOverGDiff;
      if(globalDiff < FLT_MIN) {
        oneOverGDiff = 0.0;  // so we dont divide by zero
      } else {
        oneOverGDiff = 1.0 / globalDiff;
      }

      srows   = swfDataBox.length(XDIR);
      scols   = swfDataBox.length(YDIR);
      //splanes = swfDataBox.length(ZDIR);
      int scolssrowstmp = scols*srows;
      int sstartr = swfDataBox.smallEnd(XDIR);
      int sstartc = swfDataBox.smallEnd(YDIR);
      int sstartp = swfDataBox.smallEnd(ZDIR);
      int sendr   = swfDataBox.bigEnd(XDIR);
      int sendc   = swfDataBox.bigEnd(YDIR);
      int sendp   = swfDataBox.bigEnd(ZDIR);

      //int blackIndex = iBlackIndex;
      int colorSlots = iColorSlots;
      int paletteStart = iPaletteStart;
      //paletteEnd   = iPaletteEnd;
      //bodyColor = blackIndex;
      int cSlotsAvail = colorSlots - 1;

      cout << "Filling swfFabData..." << endl;

#if ( ! VOLUMEBOXES)
      for(lev = minDrawnLevel; lev <= maxDrawnLevel; lev++) {
        crr = CRRBetweenLevels(lev, maxDrawnLevel, amrData.RefRatio());

        for(MultiFabIterator gpli((amrData.GetGrids(lev,
					amrData.StateNumber(derivedName),
					drawnDomain[lev])));
	    gpli.isValid(); ++gpli)
	{
          gbox = gpli.validbox();
          Box goverlap = gbox & drawnDomain[lev];
          grefbox = goverlap;
          grefbox.refine(crr);

          int gstartr = gbox.smallEnd(XDIR);
          int gstartc = gbox.smallEnd(YDIR);
          int gstartp = gbox.smallEnd(ZDIR);

          int gostartr = goverlap.smallEnd(XDIR) - gstartr;
          int gostartc = goverlap.smallEnd(YDIR) - gstartc;
          int gostartp = goverlap.smallEnd(ZDIR) - gstartp;
          int goendr   = goverlap.bigEnd(XDIR)   - gstartr;
          int goendc   = goverlap.bigEnd(YDIR)   - gstartc;
          int goendp   = goverlap.bigEnd(ZDIR)   - gstartp;

          FArrayBox *swfFabData = new FArrayBox(gbox, 1);
	  swfFabData->copy(gpli(), amrData.StateNumber(derivedName), 0, 1);
          dataPoint = swfFabData->dataPtr();

          grows   = gbox.length(XDIR);
          gcols   = gbox.length(YDIR);
          //gplanes = gbox.length(ZDIR);

          // expand data into swfData and change to chars

        if(crr != 1) {
          int gcolsgrowstmp = gcols*grows;
          int gpgcgrtmp, gcgrowstmp;
          for(gp=gostartp; gp <= goendp; gp++) {
            gpgcgrtmp = gp*gcolsgrowstmp;
            for(gc=gostartc; gc <= goendc; gc++) {
              gcgrowstmp = gpgcgrtmp + gc*grows;
              for(gr=gostartr; gr <= goendr; gr++) {
                //dat = dataPoint[(gp*gcols*grows)+(gc*grows)+gr];  // works
                dat = dataPoint[gcgrowstmp + gr];
                dat = Max(dat,gmin); // clip data if out of range
                dat = Min(dat,gmax);
                chardat = (char) (((dat-gmin)*oneOverGDiff) * cSlotsAvail);
                chardat += paletteStart;

                //   works
                //sindexbase =
                //      (((gp+gstartp)*crr-sstartp) * scols*srows) +
                //  //((sendc-sstartc-((gc+gstartc)*crr-sstartc)) * srows)+
                //      ((sendc-((gc+gstartc)*crr)) * srows)+  // check this
                //      ((gr+gstartr)*crr-sstartr);
                //
                sindexbase =
                      (((gp+gstartp)*crr-sstartp) * scolssrowstmp) +
                      ((sendc-((gc+gstartc)*crr)) * srows)+  // check this
                      ((gr+gstartr)*crr-sstartr);

                for(sp=0; sp < crr; sp++) {
                  for(sc=0; sc < crr; sc++) {
                    for(sr=0; sr < crr; sr++) {
                      sindex = sindexbase +
                           //((sp*scols*srows) - (sc*srows) + sr);  // works
                           ((sp*scolssrowstmp) - (sc*srows) + sr);
                      //check out of range
                      //if(sindex < 0 || sindex > swfDataBox.numPts()) {
                        //cerr << "bad sindex = " << sindex << endl;
                        //cerr << "    npts = " << swfDataBox.numPts() << endl;
                        //exit(-4);
                      //}
                      swfData[sindex] = chardat;

                    }
                  }
                }  // end for(sp...)
              }  // end for(gr...)
            }  // end for(gc...)
          }  // end for(gp...)

        } else {  // crr == 1
          int gcolsgrowstmp = gcols*grows;
          int gpgcgrtmp, gcgrowstmp;
          for(gp=gostartp; gp <= goendp; gp++) {
            gpgcgrtmp = gp*gcolsgrowstmp;
            for(gc=gostartc; gc <= goendc; gc++) {
              gcgrowstmp = gpgcgrtmp + gc*grows;
              for(gr=gostartr; gr <= goendr; gr++) {
                //dat = dataPoint[(gp*gcols*grows)+(gc*grows)+gr];  // works
                dat = dataPoint[gcgrowstmp + gr];
                dat = Max(dat,gmin); // clip data if out of range
                dat = Min(dat,gmax);
                chardat = (char) (((dat-gmin)*oneOverGDiff) * cSlotsAvail);
                chardat += paletteStart;

                sindexbase =
                      (((gp+gstartp)-sstartp) * scolssrowstmp) +
                      ((sendc-((gc+gstartc))) * srows) +  // check this
                      ((gr+gstartr)-sstartr);

                swfData[sindexbase] = chardat;
              }
            }
          }  // end for(gp...)

        }  // end if(crr...)

	delete swfFabData;

        }  // end while(gpli)
      }  // end for(lev...)
#endif


      cout << "--------------- make swfData time = "
           << ((clock()-time0)/1000000.0) << endl;

}  // end MakeSWFData


// -------------------------------------------------------------------
void VolRender::WriteSWFData(const aString &filenamebase, bool SWFLight) {
    cout<<"VolRender::WriteSWFData"<<endl;
    assert(bVolRenderDefined);
    if(ParallelDescriptor::IOProcessor()) {
        cout << "vpClassify Scalars..." << endl;           // --- classify
        clock_t time0 = clock();
        vpResult vpret;
        bool PCtemp = preClassify;
        preClassify = true;
        //here set lighting or value model
        bool LMtemp = lightingModel;
        lightingModel = SWFLight;
 
        MakeVPData();
        
        preClassify = PCtemp;
        lightingModel = LMtemp;
   

  cout << "----- make vp data time = " << ((clock()-time0)/1000000.0) << endl;
  aString filename = "swf.";
  filename += filenamebase;
  filename += (SWFLight ? ".lt" : ".val" );
  filename += ".vpdat";
  cout << "----- storing classified volume into file:  " << filename << endl;
#ifndef S_IRUSR  /* the T3E does not define this */
#define S_IRUSR 0000400
#endif
#ifndef S_IWUSR  /* the T3E does not define this */
#define S_IWUSR 0000200
#endif
#ifndef S_IRGRP  /* the T3E does not define this */
#define S_IRGRP 0000040
#endif
    int fp = open(filename.c_str(), O_CREAT | O_WRONLY,
		  S_IRUSR | S_IWUSR | S_IRGRP);
    vpret = vpStoreClassifiedVolume(vpc, fp);
    CheckVP(vpret, 4);
    close(fp);
  }
}


// -------------------------------------------------------------------
void VolRender::InvalidateSWFData() {
  swfDataValid = false;
}


// -------------------------------------------------------------------
void VolRender::InvalidateVPData() {
  vpDataValid = false;
}

void VolRender::SetLightingModel(bool lightOn) {
  if (lightingModel == lightOn) return;
  lightingModel = lightOn;
  if (lightingModel == true) {
    vpSetVoxelField(vpc,  normalField, normalSize, normalOffset,
                    maxShadeRampPts-1);
  } else {
    vpSetVoxelField(vpc,  normalField, normalSize, normalOffset,
                    paletteSize-1);
  }
}

void VolRender::SetPreClassifyAlgorithm(bool pC)
{
  preClassify = pC;
}

void VolRender::SetImage(unsigned char *image_data, int width, int height,
                         int pixel_type)
{
    vpSetImage(vpc, image_data, width, height, width, pixel_type);
}

void VolRender::MakePicture(Real mvmat[4][4], Real Length,
                            int width, int height)
{
    vpCurrentMatrix(vpc, VP_MODEL);
    vpIdentityMatrix(vpc);
    vpSetMatrix(vpc, mvmat);
    vpCurrentMatrix(vpc, VP_PROJECT);
    vpIdentityMatrix(vpc);
    vpLen = Length;
    if(width < height) {    // undoes volpacks aspect ratio scaling
        vpWindow(vpc, VP_PARALLEL, 
                 -Length*vpAspect, Length*vpAspect,
                 -Length, Length,
                 -Length, Length);
    } else {
        vpWindow(vpc, VP_PARALLEL, 
                 -Length, Length,
                 -Length*vpAspect, Length*vpAspect,
                 -Length, Length);
    }
    vpResult vpret;
    
    if(lightingModel) {
        vpret = vpShadeTable(vpc);
        CheckVP(vpret, 12);
    }
    
    if (preClassify) {
        vpret = vpRenderClassifiedVolume(vpc);   // --- render
        CheckVP(vpret, 11);
    } else {
        vpret = vpClassifyVolume(vpc);  // - classify and then render
        CheckVP(vpret, 11.1);
        vpret = vpRenderRawVolume(vpc);
        CheckVP(vpret, 11.2);
    }
}

// -------------------------------------------------------------------
void VolRender::MakeVPData() {
  assert(bVolRenderDefined);
  if(ParallelDescriptor::IOProcessor()) {
    //cout << "_in MakeVPData()" << endl;
    clock_t time0 = clock();
    
    vpDataValid = true;
    
    cout << "vpClassifyScalars..." << endl;           // --- classify
    
    vpResult vpret;
    if (preClassify) {
      if (lightingModel) {
        vpret = vpClassifyScalars(vpc, swfData, swfDataSize,
                                  densityField, gradientField, normalField);
        CheckVP(vpret, 6);
      } else {
        // the classification and loading of the value model
        delete [] volData;
        volData = new RawVoxel[swfDataSize]; // volpack will delete this
        // there is a memory leak with volData if vpDestroyContext is not called
        //hmmm -- I have deleted volData, so this comment is old.

        //cout << endl << "********** volData = " << volData << endl << endl;
        int xStride, yStride, zStride;
        xStride = sizeof(RawVoxel);
        yStride = drawnDomain[maxDataLevel].length(XDIR) * sizeof(RawVoxel);
        zStride = drawnDomain[maxDataLevel].length(XDIR) *
          drawnDomain[maxDataLevel].length(YDIR) * sizeof(RawVoxel);
        vpret = vpSetRawVoxels(vpc, volData, swfDataSize * sizeof(RawVoxel),
                               xStride, yStride, zStride);
        CheckVP(vpret, 9.4);
        for(int vindex = 0; vindex<swfDataSize; vindex++) {
          volData[vindex].normal  = swfData[vindex];
          volData[vindex].density = swfData[vindex];
        }
        
        vpret = vpClassifyVolume(vpc);
        CheckVP(vpret, 9.5);
        
      }
    } else { //value rendering --
      //load the volume data and precompute the minmax octree
      if (lightingModel) {
        delete [] volData;
        volData = new RawVoxel[swfDataSize]; 
        int xStride, yStride, zStride;
        xStride = sizeof(RawVoxel);
        yStride = drawnDomain[maxDataLevel].length(XDIR) * sizeof(RawVoxel);
        zStride = drawnDomain[maxDataLevel].length(XDIR) *
          drawnDomain[maxDataLevel].length(YDIR) * sizeof(RawVoxel);
        vpret = vpSetRawVoxels(vpc, volData, swfDataSize * sizeof(RawVoxel),
                               xStride, yStride, zStride);
        CheckVP(vpret, 9.45);
        vpret = vpVolumeNormals(vpc, swfData, swfDataSize,
                                densityField, gradientField, normalField);
        CheckVP(vpret, 6.1);
        
      } else {
        delete [] volData;
        volData = new RawVoxel[swfDataSize]; 
        int xStride, yStride, zStride;
        xStride = sizeof(RawVoxel);
        yStride = drawnDomain[maxDataLevel].length(XDIR) * sizeof(RawVoxel);
        zStride = drawnDomain[maxDataLevel].length(XDIR) *
          drawnDomain[maxDataLevel].length(YDIR) * sizeof(RawVoxel);
        vpret = vpSetRawVoxels(vpc, volData, swfDataSize * sizeof(RawVoxel),
                               xStride, yStride, zStride);
        CheckVP(vpret, 9.4);
        for(int vindex = 0; vindex<swfDataSize; vindex++) {
          volData[vindex].normal  = swfData[vindex];
          volData[vindex].density = swfData[vindex];
        }
      }     
      vpret = vpMinMaxOctreeThreshold(vpc, DENSITY_PARAM, 
                                      OCTREE_DENSITY_THRESH);
      CheckVP(vpret, 9.41);
      
      if (classifyFields == 2) {
        vpret = vpMinMaxOctreeThreshold(vpc, GRADIENT_PARAM, 
                                        OCTREE_GRADIENT_THRESH);
        CheckVP(vpret, 9.42);
      }
      
      vpret = vpCreateMinMaxOctree(vpc, 1, OCTREE_BASE_NODE_SIZE);
      CheckVP(vpret, 9.43);
    }
    
    // --- set the shading parameters
    
    if (lightingModel) {
      vpret = vpSetLookupShader(vpc, 1, 1, normalField, shade_table,
                                maxShadeRampPts * sizeof(float), 0, NULL, 0);
      CheckVP(vpret, 7);
      vpSetMaterial(vpc, VP_MATERIAL0, VP_AMBIENT, VP_BOTH_SIDES, 0.28, 0.28, 0.28);
      vpSetMaterial(vpc, VP_MATERIAL0, VP_DIFFUSE, VP_BOTH_SIDES, 0.35, 0.35, 0.35);
      vpSetMaterial(vpc, VP_MATERIAL0, VP_SPECULAR, VP_BOTH_SIDES, 0.39, 0.39, 0.39);
      vpSetMaterial(vpc, VP_MATERIAL0, VP_SHINYNESS, VP_BOTH_SIDES, 10.0,  0.0, 0.0);
      
      vpSetLight(vpc, VP_LIGHT0, VP_DIRECTION, 0.3, 0.3, 1.0);
      vpSetLight(vpc, VP_LIGHT0, VP_COLOR, 1.0, 1.0, 1.0);
      vpEnable(vpc, VP_LIGHT0, 1);
      
      vpSeti(vpc, VP_CONCAT_MODE, VP_CONCAT_LEFT);
      
      // --- compute shading lookup table
      vpret = vpShadeTable(vpc);
      CheckVP(vpret, 8);
      
    } else {
      assert(palettePtr != NULL);
      for(int sn=0; sn<paletteSize/*maxShadeRampPts*/; sn++) {
        value_shade_table[sn] = (float)sn; //the correct old version
      }
      value_shade_table[0] = (Real) MaxPaletteIndex();
     
      float maxf = 0.0;
      float minf = 1000000.0;
      for(int ijk = 0; ijk < paletteSize/*maxShadeRampPts*/; ijk++) {
        maxf = Max(maxf, value_shade_table[ijk]);
        minf = Min(minf, value_shade_table[ijk]);
      }
      SHOWVAL(minf);
      SHOWVAL(maxf);
      
      vpret = vpSetLookupShader(vpc, 1, 1, normalField, value_shade_table,
                                paletteSize * sizeof(float), 0, NULL, 0);
      CheckVP(vpret, 9);
      
      vpSetMaterial(vpc, VP_MATERIAL0, VP_AMBIENT, VP_BOTH_SIDES, 1.00, 1.00, 1.00);
      vpSetMaterial(vpc, VP_MATERIAL0, VP_AMBIENT, VP_BOTH_SIDES, 0.28, 0.28, 0.28);
      vpSetMaterial(vpc, VP_MATERIAL0, VP_DIFFUSE, VP_BOTH_SIDES, 0.35, 0.35, 0.35);
      
      vpSetLight(vpc, VP_LIGHT0, VP_DIRECTION, 0.3, 0.3, 1.0);
      vpSetLight(vpc, VP_LIGHT0, VP_COLOR, 1.0, 1.0, 1.0);
      vpEnable(vpc, VP_LIGHT0, 1);
      
      vpSeti(vpc, VP_CONCAT_MODE, VP_CONCAT_LEFT);
    }
    cout << "----- make vp data time = " << ((clock()-time0)/1000000.0) << endl;
  }
  
}  // end MakeVPData()


// -------------------------------------------------------------------
void VolRender::ReadTransferFile(const aString &rampFileName) {
  int n;
  if(densityRampX  != NULL) { delete [] densityRampX;  }
  if(densityRampY  != NULL) { delete [] densityRampY;  }
  if(gradientRampX != NULL) { delete [] gradientRampX; }
  if(gradientRampY != NULL) { delete [] gradientRampY; }

  ifstream istrans;
  istrans.open(rampFileName.c_str());
  if(istrans.fail()) {
    cerr << "Error in ReadTransferFile:  cannot open file = "
	 << rampFileName << endl;

    // make default transfer functions
    classifyFields = 2;
    shadeFields = 2;
    nDenRampPts = 2;
    densityRampX  = new int[nDenRampPts];
    densityRampY  = new float[nDenRampPts];
    nGradRampPts = 2;
    gradientRampX = new int[nDenRampPts];
    gradientRampY = new float[nDenRampPts];
    densityRampX[0] = 0;    densityRampX[1] = MaxPaletteIndex();;
    densityRampY[0] = 0.0;  densityRampY[1] = 1.0;

    gradientRampX[0] = 0;    gradientRampX[1] = MaxPaletteIndex();;
    gradientRampY[0] = 0.0;  gradientRampY[1] = 1.0;

    minRayOpacity = 0.05;
    maxRayOpacity = 0.95;

  } else {

    // read transfer parameters from the file

    istrans >> classifyFields;
    istrans >> shadeFields;

    istrans >> nDenRampPts;
    densityRampX = new int[nDenRampPts];
    densityRampY = new float[nDenRampPts];

    for(n=0; n<nDenRampPts; n++) {
      istrans >> densityRampX[n];
    }

    for(n=0; n<nDenRampPts; n++) {
      istrans >> densityRampY[n];
    }

    istrans >> nGradRampPts;

    gradientRampX = new int[nGradRampPts];
    gradientRampY = new float[nGradRampPts];

    for(n=0; n<nGradRampPts; n++) {
      istrans >> gradientRampX[n];
    }
    for(n=0; n<nGradRampPts; n++) {
      istrans >> gradientRampY[n];
    }
    istrans >> minRayOpacity;
    istrans >> maxRayOpacity;
    }


  vpResult vpret = vpSetVoxelSize(vpc, BYTES_PER_VOXEL, voxelFields,
                        shadeFields, classifyFields);
  CheckVP(vpret, 14);
  if (lightingModel) {
    vpSetVoxelField(vpc,  normalField, normalSize, normalOffset,
                  maxShadeRampPts-1);
  } else {
    vpSetVoxelField(vpc,  normalField, normalSize, normalOffset,
                    paletteSize-1);
  }
  vpSetVoxelField(vpc, densityField, densitySize, densityOffset,
                  densityMax);

  vpSetVoxelField(vpc, gradientField, gradientSize, gradientOffset,
                  gradientMax);

  vpRamp(density_ramp, sizeof(float), nDenRampPts, densityRampX, densityRampY);
  vpSetClassifierTable(vpc, DENSITY_PARAM, densityField,
                         density_ramp, maxDenRampPts * sizeof(float));

  if(classifyFields == 2) {
    vpRamp(gradient_ramp, sizeof(float), nGradRampPts,
           gradientRampX, gradientRampY);
    vpSetClassifierTable(vpc, GRADIENT_PARAM, gradientField,
                         gradient_ramp, maxGradRampPts * sizeof(float));
  }

  vpSetd(vpc, VP_MIN_VOXEL_OPACITY, minRayOpacity);
  vpSetd(vpc, VP_MAX_RAY_OPACITY,   maxRayOpacity);

}  // end ReadTransferFile

// -------------------------------------------------------------------
// -------------------------------------------------------------------







