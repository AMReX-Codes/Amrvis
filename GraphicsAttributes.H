//BL_COPYRIGHT_NOTICE

//
// $Id: GraphicsAttributes.H,v 1.11 2000-06-14 20:08:26 car Exp $
//

// ---------------------------------------------------------------
// GraphicsAttributes.H
// ---------------------------------------------------------------
#ifndef _GRAPHICSATTRIBUTES_H
#define _GRAPHICSATTRIBUTES_H

#include "AmrvisConstants.H"

#include <X11/X.h>
#include <X11/Intrinsic.h>
#undef index

extern "C" {
  int close(int);
}

class GraphicsAttributes {
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
  public:

    GraphicsAttributes(Widget);
    ~GraphicsAttributes();

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
    bool     isTrueColor()    const { return visual_info.c_class == TrueColor; }
    int      PBitmapPaddedWidth(int width) const;
};

#endif
// -------------------------------------------------------------------
// -------------------------------------------------------------------
