// -------------------------------------------------------------------
// Palette.H
// -------------------------------------------------------------------
#ifndef _PALETTE_H
#define _PALETTE_H

#include "REAL.H"
#include "aString.H"

const int COLORSLOTS      = 256;
const int PALHEIGHT       = 254;
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


class Palette {

  protected:
    XColor 	ccells[COLORSLOTS];
    Pixmap 	palPixmap;
    int	  	totalPalWidth, palWidth, totalPalHeight, palHeight;
    Real	*dataList;
    int   	dataListLength;
    Window 	palWindow;
    Colormap    colmap;
    int         colorSlots, reserveSystemColors, colorOffset, paletteStart;
    int		blackIndex, whiteIndex;
    Real	pmin, pmax;
    aString	defaultFormat;
    int		transSet;
    float	transY[COLORSLOTS];  // float for compaitbility with volpack
    unsigned char *remapTable; // for remapping to a smaller color
				     // index set
    Display    *display;
    Visual     *visual;
    Screen     *screen;
    XVisualInfo visualInfo;
    GC          gc;
    Window      root;
    int         screenNumber;

    static XColor   sysccells[COLORSLOTS];
    static Colormap systemColmap;

  public:
    Palette(Widget &w, int datalistlength, int width, int height,
            int totalwidth, int totalheight, int reservesystemcolors);
    ~Palette();
    int   ReadSeqPalette(const aString &fileName);
    void  Draw(Real palMin, Real palMax, const aString &numberFormat);
    void  SetWindow(Window drawPaletteHere);
    void  SetWindowPalette(const aString &palName, Window newPalWindow);
    void  ChangeWindowPalette(const aString &palName, Window newPalWindow);
    void  ExposePalette();
    void  Redraw();
    void  SetReserveSystemColors(int reservesystemcolors);
    void  SetFormat(const aString &newFormat);
    void  SetTransfers(int ntranspts, int *transx, float *transy);
    Colormap GetColormap()        { return colmap;         }
    XColor  GetColorCell(int i)   { return ccells[i];      }
    XColor *GetColorCells()       { return ccells;         }
    int  BlackIndex()             { return blackIndex;     }
    int  WhiteIndex()             { return whiteIndex;     }
    int  ColorSlots()             { return colorSlots;     }
    int  PaletteStart()           { return paletteStart;   }
    int  PaletteEnd()             { return COLORSLOTS - 1; }
    int  PaletteSize()            { return COLORSLOTS;     }
    int  PaletteWidth()           { return totalPalWidth;  }
    int  PaletteHeight()          { return totalPalHeight; }
    XImage *GetPictureXImage();
    unsigned char *RemapTable()   { return remapTable;     }

  private:
    Palette(Widget &w);
};

#endif
