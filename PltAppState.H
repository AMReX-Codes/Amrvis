// ---------------------------------------------------------------
// PltAppState.H
// ---------------------------------------------------------------
//  state observers:
//    pltapp
//    amrpicture
//    projectionpicture
//    dataset
//    palette
//    lightingwindow
//    xyplots
// ---------------------------------------------------------------
#ifndef _PLTAPPSTATE_H_
#define _PLTAPPSTATE_H_

#include "AmrvisConstants.H"
/*
#include "AmrPicture.H"
#include "Box.H"
#include "Dataset.H"
#include "GraphicsAttributes.H"
#include "GlobalUtilities.H"
#include "DataServices.H"
#include "Palette.H"
#include "List.H"
#if (BL_SPACEDIM==3)
# include "ViewTransform.H"
# include "ProjectionPicture.H"
#endif

*/

#include "aString.H"

class AmrPicture;


class PltAppState {
    
private:
  //PltAppState();  // not defined
  
public:
  // created when opening a new file
  PltAppState();
  
  // for subregion
  /*
  PltAppState(XtAppContext app, Widget w, const Box &region,
         const IntVect &offset,
         AmrPicture *parentPtr, PltApp *pltparent,
         const aString &palfile, bool isAnim,
         const aString &newderived, const aString &file);
  */
  
  ~PltAppState();
  
  int  CurrentScale()  const          { return currentScale; }
  void SetCurrentScale(int newScale)  { currentScale = newScale; }

  const aString &CurrentDerived() const { return currentDerived; }
  void SetCurrentDerived(const aString &newDerived) { currentDerived = newDerived; }

  //bool GetShowingBoxes() const        { return showBoxes; }
  //void SetShowingBoxes(bool showing)  { showBoxes = showing; }

/*
  int MinAllowableLevel() const    { return minAllowableLevel; }
  int MinDrawnLevel() const        { return minDrawnLevel; }
  int MaxDrawnLevel() const        { return maxDrawnLevel; }
  Real CurrentMin()    const        { return currentMin; }
  Real CurrentMax()    const        { return currentMax; }
  GraphicsAttributes *GetGAptr() const  { return GAptr; }
  void SetGlobalMinMax(Real specifiedMin, Real specifiedMax);
*/

private:

  // state variables
  int currentScale;
  aString currentDerived;
  //int maxAllowableScale;
  //bool showBoxes;

  //RangeType rangeType;
  //bool datasetShowing, paletteDrawn, writingRGB, interfaceReady;
  //int minDrawnLevel, maxDrawnLevel, minDataLevel, maxDataLevel;
  //int activeView;
  //aString contourNumString, formatString, fileName, palFilename;
  //Real		globalMin, globalMax;
  //Array<aString> fileNames, derivedStrings;
  //GraphicsAttributes	*GAptr;



  // observers
  AmrPicture *amrPicturePtrArray[NPLANES];

  //Palette *pltPaletteptr;
  //Dataset *datasetPtr;
  //Array<DataServices *> dataServicesPtr;

};
#endif
