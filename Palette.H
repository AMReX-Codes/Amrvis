// -------------------------------------------------------------------
// Palette.H
// -------------------------------------------------------------------
#ifndef _PALETTE_H
#define _PALETTE_H

#include "REAL.H"
#include "aString.H"
#include "Array.H"

const int PALLISTLENGTH   = 8;
const int PALWIDTH        = 24;
const int TEXTCOLOR       = 224;
const int TOTALPALHEIGHT  = 280;
const int TOTALPALWIDTH   = 150;

#include <Intrinsic.h>
#include <StringDefs.h>

extern "C" {
  int close(int);
}

enum PaletteType { ALPHA, NON_ALPHA };

class Palette {

protected:
  Array<XColor> ccells;
  Pixmap 	palPixmap;
  int	  	totalPalWidth, palWidth, totalPalHeight;
  Array<Real> dataList;
  Window 	palWindow;
  Colormap    colmap;
  int         totalColorSlots, colorSlots;
  int         reserveSystemColors, colorOffset, paletteStart;
  int		blackIndex, whiteIndex;
  Real	pmin, pmax;
  aString	defaultFormat;
  int		transSet;
  Array<float> transferArray;
  unsigned char *remapTable; // for remapping to a smaller color index set
  Display    *display;
  Visual     *visual;
  Screen     *screen;
  XVisualInfo visualInfo;
  GC          gc;
  Window      root;
  int         screenNumber;
  Array<XColor> sysccells;
  
  static Colormap systemColmap;
  PaletteType paletteType;
  
public:
  Palette(Widget &w, int datalistlength, int width,
          int totalwidth, int totalheight, int reservesystemcolors);
  Palette(int datalistlength, int width,
          int totalwidth, int totalheight, int reservesystemcolors);
  ~Palette();
  PaletteType GetPaletteType() { return paletteType; }
  Array<float> GetTransferArray() { return transferArray; }
  int   ReadSeqPalette(const aString &fileName, bool bRedraw = true);
  void  Draw(Real palMin, Real palMax, const aString &numberFormat);
  void  SetWindow(Window drawPaletteHere);
  void  SetWindowPalette(const aString &palName, Window newPalWindow);
  void  ChangeWindowPalette(const aString &palName, Window newPalWindow);
  void  ExposePalette();
  void  Redraw();
  void  SetReserveSystemColors(int reservesystemcolors);
  void  SetFormat(const aString &newFormat);
  void  SetTransfers(const Array<int> &transx, const Array<float> &transy);
  Colormap GetColormap() const  { return colmap;         }
  const Array<XColor> &GetColorCells() const { return ccells; }
  int  BlackIndex()    const    { return blackIndex;     }
  int  WhiteIndex()    const    { return whiteIndex;     }
  int  ColorSlots()    const    { return colorSlots;     }
  int  PaletteStart()  const    { return paletteStart;   }
  int  PaletteEnd()    const    { return totalColorSlots - 1; }
  int  PaletteSize()   const    { return totalColorSlots;     }
  int  PaletteWidth()  const    { return totalPalWidth;  }
  int  PaletteHeight() const    { return totalPalHeight; }
  Real PaletteMin()    const    { return pmin; }
  Real PaletteMax()    const    { return pmax; }
  const Array<Real> &PaletteDataList() const { return dataList; }
  aString PaletteNumberFormat() { return defaultFormat; }
  XImage *GetPictureXImage();
  unsigned char *RemapTable() const  { return remapTable;     }
  
private:
  void ReadPalette(const aString &palNam);
  Palette(Widget &w);
};

#endif



