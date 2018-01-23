// ---------------------------------------------------------------
// VolRender.H
// ---------------------------------------------------------------
#ifndef _VOLRENDER_H_
#define _VOLRENDER_H_

#include <cstdio>
#include <string>
using std::string;

#include <AMReX_Vector.H>
#include <AMReX_REAL.H>
#include <AMReX_Box.H>
#include <AMReX_AmrvisConstants.H>
#include <AMReX_DataServices.H>
#include <Volume.H>
#include <Palette.H>

#ifdef BL_VOLUMERENDER
#include <volpack.h>
#endif

using amrex::Real;
using amrex::DataServices;


class VolRender {
  private:
    VolRender();
  public:
    VolRender(const amrex::Vector<amrex::Box> &drawdomain, int mindrawnlevel,
	      int maxdrawnlevel, Palette *pptr, const string &asLightFileName);
  
    ~VolRender();
  
  private:
    unsigned int nPoints, bPoints;
    int		 minDrawnLevel, maxDataLevel;
    amrex::Vector<amrex::Box>   drawnDomain;
    char	 buffer[BUFSIZ];
    int		 nDenRampPts, nGradRampPts, nShadeRampPts;
    int		 maxDenRampPts, maxGradRampPts, maxShadeRampPts;
    bool	 bVolRenderDefined;
    Palette     *palettePtr;
    int          paletteSize;
  
  private:
    vpContext    *vpc;
    amrex::Vector<float> density_ramp;       // opacity as a function of density
    amrex::Vector<float> gradient_ramp;      // opacity as a function of grad magnitude
    amrex::Vector<float> shade_table;        // shading lookup table
    amrex::Vector<float> value_shade_table;  //for the value model
  
    int  rows, cols, planes;
    unsigned char  *swfData;
    int  swfDataSize;
    bool swfDataAllocated, vpDataValid, swfDataValid, vpCreated;
    int	classifyFields, shadeFields;
    amrex::Vector<int>	densityRampX, gradientRampX;
    amrex::Vector<float>  densityRampY, gradientRampY;
    float minRayOpacity, maxRayOpacity;
    RawVoxel *volData;
  
  
  public:
    vpContext *GetVPContext() { return vpc; }
    void MakeVPData();
    void MakeSWFData(DataServices *dataServicesPtr, Real rDataMin, Real rDataMax,
                     const string &derivedName,
                     int iPaletteStart, int iPaletteEnd,
                     int iBlackIndex, int iWhiteIndex, int iColorSlots,
		     const bool bdrawboxes);
  
    void MakePicture(Real mvmat[4][4], Real Length, int width, int height);

    void SetImage(unsigned char *image_data, int width, int height, int pixel_type);
    void WriteSWFData(const string &filenamebase, bool SWFLight);
    unsigned char *GetSWFData()    { return swfData; }
    int  GetSWFDataSize()   const  { return swfDataSize; }
    bool VPDataValid()      const  { return vpDataValid; }
    void InvalidateVPData();
    bool SWFDataValid()     const  { return swfDataValid; }
    void InvalidateSWFData();
    bool SWFDataAllocated() const  { return swfDataAllocated; }
    const amrex::Vector<int>   &DensityRampX() const  { return densityRampX; }
    const amrex::Vector<float> &DensityRampY() const  { return densityRampY; }
    void SetLightingModel(bool);
    bool GetLightingModel() const  { return lightingModel; }
    void SetPreClassifyAlgorithm(bool);
    bool GetPreClassifyAlgorithm() const { return preClassify; }
    void SetAspect( Real newAspect) { vpAspect = newAspect; }
    void SetTransferProperties();
    void SetLighting(Real ambient, Real diffuse, Real specular, Real shiny,
		     Real minRay, Real maxRay);
    Real GetDiffuse() { return diffuseMat; }
    Real GetAmbient() { return ambientMat; }
    Real GetSpecular() { return specularMat; }
    Real GetShiny() { return shinyMat; }
    Real GetMinRayOpacity() { return minRayOpacity; }
    Real GetMaxRayOpacity() { return maxRayOpacity; }

  private:
    Real diffuseMat, shinyMat, specularMat, ambientMat;
    Real vpLen, vpAspect;
    bool lightingModel, preClassify;
    bool bDrawAllBoxes;
    int  voxelFields;
    int  normalField, normalOffset, normalSize, normalMax;
    int  densityField, densityOffset, densitySize, densityMax;
    int  gradientField, gradientOffset, gradientSize, gradientMax;

    bool AllocateSWFData();
    void MakeDefaultTransProperties();
    void SetProperties();
};

#endif
