// ---------------------------------------------------------------
// Palette.H
// ---------------------------------------------------------------
#ifndef _PALETTE_H
#define _PALETTE_H

#include <map>
#include <string>
using std::map;
using std::string;

#include <REAL.H>
#include <Array.H>
#include <Array.H>
#include <ParallelDescriptor.H>

const int PALLISTLENGTH   = 8;
const int PALWIDTH        = 24;
const int TEXTCOLOR       = 224;
const int TOTALPALHEIGHT  = 280;
const int TOTALPALWIDTH   = 150;

#include <X11/Intrinsic.h>

class GraphicsAttributes;


enum PaletteType { ALPHA, NON_ALPHA };

class Palette {

  protected:
    Array<XColor> ccells;
    map<Pixel, XColor> mcells;
    Array<Pixel> pixelCache;
    Pixmap 	palPixmap;
    int	  	totalPalWidth, palWidth, totalPalHeight;
    Array<Real> dataList;
    Window 	palWindow;
    Colormap    colmap;
    int         totalColorSlots, colorSlots;
    int         reserveSystemColors, colorOffset, paletteStart;
    int 	blackIndex, whiteIndex, bodyIndex;
    Real	pmin, pmax;
    string	defaultFormat;
    Array<float>  transferArray;
    unsigned char *remapTable; // for remapping to a smaller color index set
    GraphicsAttributes *gaPtr;
    Array<XColor> sysccells;
    bool bReadPalette;
  
    static Colormap systemColmap;
    PaletteType paletteType;
    Array<unsigned char> abuff;
    Array<unsigned char> rbuff;
    Array<unsigned char> gbuff;
    Array<unsigned char> bbuff;

  public:
    Palette(Widget &w, int datalistlength, int width,
            int totalwidth, int totalheight, int reservesystemcolors);
    Palette(int datalistlength, int width,
            int totalwidth, int totalheight, int reservesystemcolors);
    ~Palette();
    PaletteType GetPaletteType() { return paletteType; }
    Array<float> GetTransferArray() { return transferArray; }
    int   ReadSeqPalette(const string &fileName, bool bRedraw = true);
    void  Draw(Real palMin, Real palMax, const string &numberFormat);
    void  SetWindow(Window drawPaletteHere);
    void  SetWindowPalette(const string &palName, Window newPalWindow,
			   bool bRedraw = true);
    void  ChangeWindowPalette(const string &palName, Window newPalWindow);
    void  ExposePalette();
    void  Redraw();
    void  SetReserveSystemColors(int reservesystemcolors);
    void  SetFormat(const string &newFormat);
    Colormap GetColormap() const  { return colmap; }
    const Array<XColor> &GetColorCells() const { return ccells; }
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
    const Array<Real> &PaletteDataList() const { return dataList; }
    string PaletteNumberFormat() { return defaultFormat; }
    XImage *GetPictureXImage();
    unsigned char *RemapTable() const  { return remapTable;     }

    const Array<unsigned char> &theABuff() const { return abuff; }
    const Array<unsigned char> &theRBuff() const { return rbuff; }
    const Array<unsigned char> &theGBuff() const { return gbuff; }
    const Array<unsigned char> &theBBuff() const { return bbuff; }

    unsigned char theABuff(int i) const { return abuff[i]; }
    unsigned char theRBuff(int i) const { return rbuff[i]; }
    unsigned char theGBuff(int i) const { return gbuff[i]; }
    unsigned char theBBuff(int i) const { return bbuff[i]; }
    Pixel pixelate(int i) const;
    void unpixelate(Pixel p, unsigned char &r, unsigned char &g,
		    unsigned char& b) const;
    //Pixel makePixel(unsigned char index) const;
    Pixel makePixel(unsigned char index) const { return pixelCache[index]; }

  private:
    void ReadPalette(const string &palNam, bool bRedraw = true);
    Palette(Widget &w);
};

#endif
// ---------------------------------------------------------------
// ---------------------------------------------------------------
