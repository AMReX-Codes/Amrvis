// -------------------------------------------------------------------
// GraphicsAttributes.H
// -------------------------------------------------------------------
#ifndef _GRAPHICSATTRIBUTES_H
#define _GRAPHICSATTRIBUTES_H

#include <fcntl.h>
#include <iostream.h>
#include <fstream.h>
#include <stdio.h>

#include "AmrvisConstants.H"

#include <X.h>
#include <Xlib.h>
#include <Intrinsic.h>
#include <StringDefs.h>

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
};

#endif
// -------------------------------------------------------------------
// -------------------------------------------------------------------
