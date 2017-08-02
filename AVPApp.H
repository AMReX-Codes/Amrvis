// ---------------------------------------------------------------
// AVPApp.H
// ---------------------------------------------------------------
#ifndef _AVPAPP_H_
#define _AVPAPP_H_

#include<string>
#include <AMReX_DataServices.H>


class Palette;
class PltAppState;
class XYPlotParameters;
class XYPlotWin;

class AVPApp
{
  public:
    AVPApp() { }
    virtual ~AVPApp() { }
    virtual const std::string &GetFileName() { amrex::Abort("AVPApp not implemented."); return ""; };
    virtual PltAppState *GetPltAppState() { amrex::Abort("AVPApp not implemented."); return nullptr; }
    virtual amrex::DataServices *GetDataServicesPtr() { amrex::Abort("AVPApp not implemented."); return nullptr; }
    virtual XYPlotWin *GetXYPlotWin(int dir) const { return XYplotwin[dir]; }
    virtual void DetachXYPlotWin(int dir) { XYplotwin[dir] = NULL; }
    virtual amrex::XYPlotDataList *CreateLinePlot(int V, int sdir, int mal, int ixY,
                                     const std::string *derived)
				       { amrex::Abort("AVPApp not implemented."); return nullptr; }
    virtual Widget GetPalArea() { return wPalArea; }
    virtual bool PaletteCBQ() { return paletteCBQ; }
    virtual void SetPaletteCBQ(bool tf = true) { paletteCBQ = tf; }

    XYPlotParameters *GetXYPlotParameters() const { return XYplotparameters; }
    Palette *GetPalettePtr() { return pltPaletteptr; }
    Widget WId()                     { return wAmrVisTopLevel; }
    const std::string &GetPaletteName() const { return palFilename; }

  protected:
    Widget wAmrVisTopLevel, wPalArea;
    XYPlotParameters *XYplotparameters;
    Palette *pltPaletteptr;
    std::string palFilename;
    XYPlotWin *XYplotwin[BL_SPACEDIM];
    bool paletteCBQ;
};

#endif

