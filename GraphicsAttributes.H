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

    Widget   PTopLevel()      { return appTopLevel; }
    Display *PDisplay()       { return display; }
    Screen  *PScreen()        { return screen;  }
    Visual  *PVisual()        { return visual;  }
    GC       PGC()            { return gc;    }
    Window   PRoot()          { return root;  }
    int      PDepth()         { return depth; }
    int      PScreenNumber()  { return screennumber;  }
    int      PBytesPerPixel() { return bytesPerPixel; }
};

#endif
// -------------------------------------------------------------------
// -------------------------------------------------------------------
