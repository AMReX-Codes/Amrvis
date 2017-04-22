// ---------------------------------------------------------------
// Palette.H
// ---------------------------------------------------------------
#ifndef _PALETTE_H
#define _PALETTE_H

#include <map>
#include <string>
using std::map;
using std::string;

#include <AMReX_REAL.H>
#include <AMReX_Array.H>
#include <AMReX_Array.H>
#include <AMReX_ParallelDescriptor.H>

using amrex::Real;

namespace AVPalette {
const int PALLISTLENGTH   = 8;
const int PALWIDTH        = 24;
const int TEXTCOLOR       = 224;
const int TOTALPALHEIGHT  = 280;
const int TOTALPALWIDTH   = 150;
}

#include <X11/Intrinsic.h>

class GraphicsAttributes;


enum PaletteType { AV_PAL_ALPHA, AV_PAL_NON_ALPHA };

class Palette {

  protected:
    amrex::Array<XColor> ccells;
    map<Pixel, XColor> mcells;
    amrex::Array<Pixel> pixelCache;
    Pixmap 	palPixmap;
    int	  	totalPalWidth, palWidth, totalPalHeight;
    amrex::Array<Real> dataList;
    Window 	palWindow;
    Colormap    colmap;
    int         totalColorSlots, colorSlots;
    int         reserveSystemColors, colorOffset, paletteStart;
    int 	blackIndex, whiteIndex, bodyIndex;
    Real	pmin, pmax;
    string	defaultFormat;
    amrex::Array<float>  transferArray;
    unsigned char *remapTable; // for remapping to a smaller color index set
    GraphicsAttributes *gaPtr;
    amrex::Array<XColor> sysccells;
    map<int, string> mpiFuncNames;
    map<int, string> regionNames;
    bool bReadPalette, bTimeline, bRegions;
  
    static Colormap systemColmap;
    PaletteType paletteType;
    amrex::Array<unsigned char> abuff;
    amrex::Array<unsigned char> rbuff;
    amrex::Array<unsigned char> gbuff;
    amrex::Array<unsigned char> bbuff;

  public:
    Palette(Widget &w, int datalistlength, int width,
            int totalwidth, int totalheight, int reservesystemcolors);
    Palette(int datalistlength, int width,
            int totalwidth, int totalheight, int reservesystemcolors);
    ~Palette();
    PaletteType GetPaletteType() { return paletteType; }
    amrex::Array<float> GetTransferArray() { return transferArray; }
    int   ReadSeqPalette(const string &fileName, bool bRedraw = true);
    void  DrawPalette(Real palMin, Real palMax, const string &numberFormat);
    void  SetWindow(Window drawPaletteHere);
    void  SetWindowPalette(const string &palName, Window newPalWindow,
			   bool bRedraw = true);
    void  ChangeWindowPalette(const string &palName, Window newPalWindow);
    void  ExposePalette();
    void  RedrawPalette();
    void  SetReserveSystemColors(int reservesystemcolors);
    void  SetFormat(const string &newFormat);
    Colormap GetColormap() const  { return colmap; }
    const amrex::Array<XColor> &GetColorCells() const { return ccells; }
    const map<Pixel, XColor> &GetColorCellsMap() const { return mcells; }
    int   BlackIndex()   const    { return blackIndex; }
    int   WhiteIndex()   const    { return whiteIndex; }
    int   BodyIndex()   const;
    Pixel AVBlackPixel()   const;
    Pixel AVWhitePixel()   const;
    //Pixel AVBodyPixel()   const;
    int  ColorSlots()    const    { return colorSlots;     }
    int  PaletteStart()  const    { return paletteStart;   }
    int  PaletteEnd()    const    { return totalColorSlots - 1; }
    int  PaletteSize()   const    { return totalColorSlots;     }
    int  PaletteWidth()  const    { return totalPalWidth;  }
    int  PaletteHeight() const    { return totalPalHeight; }
    unsigned int SafePaletteIndex(unsigned int atlevel = 0, int mlev = 8) const;
    Real PaletteMin()    const    { return pmin; }
    Real PaletteMax()    const    { return pmax; }
    const amrex::Array<Real> &PaletteDataList() const { return dataList; }
    string PaletteNumberFormat()  { return defaultFormat; }
    XImage *GetPictureXImage();
    Pixmap GetPixmap()   const   { return palPixmap; }
    unsigned char *RemapTable() const  { return remapTable;     }

    const amrex::Array<unsigned char> &theABuff() const { return abuff; }
    const amrex::Array<unsigned char> &theRBuff() const { return rbuff; }
    const amrex::Array<unsigned char> &theGBuff() const { return gbuff; }
    const amrex::Array<unsigned char> &theBBuff() const { return bbuff; }

    unsigned char theABuff(int i) const { return abuff[i]; }
    unsigned char theRBuff(int i) const { return rbuff[i]; }
    unsigned char theGBuff(int i) const { return gbuff[i]; }
    unsigned char theBBuff(int i) const { return bbuff[i]; }
    Pixel pixelate(int i) const;
    void unpixelate(Pixel p, unsigned char &r, unsigned char &g,
		    unsigned char& b) const;
    //Pixel makePixel(unsigned char index) const;
    Pixel makePixel(unsigned char index) const { return pixelCache[index]; }

    bool IsTimeline() const  { return bTimeline; }
    void SetTimeline(bool b) { bTimeline = b;    }
    bool IsRegions() const   { return bRegions; }
    void SetRegions(bool b)  { bRegions = b;    }
    void SetMPIFuncNames(const map<int, string> &mpifnames);
    void SetRegionNames(const map<int, string> &mpifnames);

  private:
    void ReadPalette(const string &palNam, bool bRedraw = true);
    Palette(Widget &w);
};

#endif
// ---------------------------------------------------------------
// ---------------------------------------------------------------
