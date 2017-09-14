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
    virtual const std::string &GetFileName() const { return fileNames[currentFrame]; }
    PltAppState *GetPltAppState() const { return pltAppState; }
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
    int CurrentFrame()  const        { return currentFrame; }
    amrex::DataServices *GetDataServicesPtr() { return dataServicesPtr[currentFrame]; }
    amrex::Array<amrex::DataServices *> &GetDataServicesPtrArray() { return dataServicesPtr; }

    void DrawTimeRange(Widget wCF, int sdLineXL, int sdLineXH,
                       int axisLengthX, int axisLengthY,
		       amrex::Real subTimeRangeStart, amrex::Real subTimeRangeStop,
		       const std::string &yAxisLabel);
    void ParseCallTraceFile(std::ifstream &ctFile);
    void DeriveCallStack(std::ostream &os, Real startTime, Real endTime);

    struct CallTraceLine {
      int funcNumber, callStackDepth;
      long nCalls;
      amrex::Real inclTime, exclTime, callTime;
    };

    struct CallTraceData {
      amrex::Array<CallTraceLine> callTraceLines;
      std::map<int, std::string> funcNumberNames;  // ---- <fnumber, fname>
    };



  protected:
    Widget wAmrVisTopLevel, wPalArea;
    GC xgc;
    XYPlotParameters *XYplotparameters;
    Palette *pltPaletteptr;
    std::string palFilename;
    XYPlotWin *XYplotwin[BL_SPACEDIM];
    bool paletteCBQ;
    int currentFrame;
    amrex::Array<std::string> fileNames;
    PltAppState *pltAppState;
    amrex::Array<amrex::DataServices *> dataServicesPtr;
    CallTraceData callTraceData;
};

#endif

