// ---------------------------------------------------------------
// GraphicsAttributes.H
// ---------------------------------------------------------------
#ifndef _GRAPHICSATTRIBUTES_H
#define _GRAPHICSATTRIBUTES_H

#include <AMReX_AmrvisConstants.H>

#include <iostream>
using std::ostream;

#include <X11/X.h>
#include <X11/Intrinsic.h>
#undef index

class GraphicsAttributes {
    friend ostream& operator<<(ostream &os, const GraphicsAttributes &ga);
  protected:

    Widget	appTopLevel;
    Display 	*display;
    Screen  	*screen;
    Visual	*visual;
    XVisualInfo visual_info;
    GC   	gc;
    Window 	root;
    int		depth, screennumber, bytesPerPixel;

    int		CalculateNBP();
    unsigned long red_shift;
    unsigned long green_shift;
    unsigned long blue_shift;

  public:
    explicit GraphicsAttributes(Widget);

    Widget   PTopLevel()      const { return appTopLevel; }
    Display *PDisplay()       const { return display; }
    Screen  *PScreen()        const { return screen;  }
    Visual  *PVisual()        const { return visual;  }
    GC       PGC()            const { return gc;    }
    Window   PRoot()          const { return root;  }
    int      PDepth()         const { return depth; }
    int      PScreenNumber()  const { return screennumber;  }
    int      PBytesPerPixel() const { return bytesPerPixel; }
    int      PBitsPerRGB()    const { return visual_info.bits_per_rgb; }
    bool     IsTrueColor()    const { return visual_info.c_class == TrueColor; }
    unsigned long PRedMask()  const { return visual_info.red_mask; }
    unsigned long PGreenMask() const { return visual_info.green_mask; }
    unsigned long PBlueMask()  const { return visual_info.blue_mask; }
    unsigned long PRedShift()  const { return red_shift; }
    unsigned long PGreenShift() const { return green_shift; }
    unsigned long PBlueShift() const { return blue_shift; }
    int      PBitmapPaddedWidth(int width) const;
};

#endif
// -------------------------------------------------------------------
// -------------------------------------------------------------------
