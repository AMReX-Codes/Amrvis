// -------------------------------------------------------------------
// VolRender.H
// -------------------------------------------------------------------
#ifndef _VOLRENDER_H_
#define _VOLRENDER_H_

#include "aString.H" 
#include "Array.H" 
#include "REAL.H" 
#include "Box.H" 
#include "AmrvisConstants.H" 
#include "Volume.H" 
#include "Palette.H"

#ifdef BL_VOLUMERENDER
#include <volpack.h>
#endif

class DataServices;

class VolRender {
public:
  VolRender();
  VolRender(const Array<Box> &drawdomain,
            int mindrawnlevel, int maxdrawnlevel, Palette *);
  
  ~VolRender();
  
private:
  unsigned int nPoints, bPoints;
  int		 minDrawnLevel, maxDataLevel;
  Array<Box>   drawnDomain;
  char	 buffer[BUFSIZ];
  int		 nDenRampPts, nGradRampPts, nShadeRampPts;
  int		 maxDenRampPts, maxGradRampPts, maxShadeRampPts;
  bool	 bVolRenderDefined;
  Palette     *palettePtr;
  int          paletteSize;
  
private:
  vpContext      *vpc;
  Array<float>   density_ramp;       // opacity as a function of density
  Array<float>   gradient_ramp;      // opacity as a function of grad magnitude
  Array<float>   shade_table;        // shading lookup table
  Array<float>   value_shade_table;  //for the value model
  
  int             rows, cols, planes;
  unsigned char  *swfData;
  int             swfDataSize;
  bool		swfDataAllocated, vpDataValid, swfDataValid, vpCreated;
  int		classifyFields, shadeFields;
  Array<int>	densityRampX, gradientRampX;
  Array<float>  densityRampY, gradientRampY;
  float		minRayOpacity, maxRayOpacity;
  RawVoxel       *volData;
  
  
public:
  vpContext      *GetVPContext()             { return vpc; }
  void	    MakeVPData();
  void            MakeSWFData(DataServices *dataServicesPtr,
                              Real rDataMin, Real rDataMax,
                              const aString &derivedName,
                              int iPaletteStart, int iPaletteEnd,
                              int iBlackIndex, int iWhiteIndex,
                              int iColorSlots);
  
  void MakePicture(Real mvmat[4][4], Real Length, int width, int height);
  
  void SetImage(unsigned char *image_data, int width, int height, int pixel_type);
  void           WriteSWFData(const aString &filenamebase, bool SWFLight);
  unsigned char *GetSWFData()               { return swfData; }
  int             GetSWFDataSize()   const  { return swfDataSize; }
  bool      	    VPDataValid()      const  { return vpDataValid; }
  void      	    InvalidateVPData();
  bool      	    SWFDataValid()     const  { return swfDataValid; }
  void      	    InvalidateSWFData();
  bool      	    SWFDataAllocated() const  { return swfDataAllocated; }
  const Array<int>   &DensityRampX() const  { return densityRampX; }
  const Array<float> &DensityRampY() const  { return densityRampY; }
  void            SetLightingModel(bool);
  bool            GetLightingModel() const  { return lightingModel; }
  void            SetPreClassifyAlgorithm(bool);
  bool            GetPreClassifyAlgorithm() const { return preClassify; }
  void SetAspect( Real newAspect) { vpAspect = newAspect; }
  void SetTransferProperties();
  
private:
  Real diffuseMat, shinyMat, specularMat, ambientMat;
  Real vpLen, vpAspect;
  bool            lightingModel, preClassify;
  int voxelFields;
  int normalField, normalOffset, normalSize, normalMax;
  int densityField, densityOffset, densitySize, densityMax;
  int gradientField, gradientOffset, gradientSize, gradientMax;
  
  bool     	    AllocateSWFData();
  void            MakeSWFDataOneProc(DataServices *dataServicesPtr,
                                     Real rDataMin, Real rDataMax,
                                     const aString &derivedName,
                                     int iPaletteStart, int iPaletteEnd,
                                     int iBlackIndex, int iWhiteIndex,
                                     int iColorSlots);
  void            MakeSWFDataNProcs(DataServices *dataServicesPtr,
                                    Real rDataMin, Real rDataMax,
                                    const aString &derivedName,
                                    int iPaletteStart, int iPaletteEnd,
                                    int iBlackIndex, int iWhiteIndex,
                                    int iColorSlots);
  void MakeDefaultTransProperties();
  void SetProperties();
};

#endif
