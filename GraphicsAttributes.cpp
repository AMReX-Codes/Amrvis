//BL_COPYRIGHT_NOTICE

//
// $Id: GraphicsAttributes.cpp,v 1.9 2000-06-13 23:18:20 car Exp $
//

// ---------------------------------------------------------------
// GraphicsAttributes.cpp
// ---------------------------------------------------------------
#ifdef BL_USE_NEW_HFILES
#include <iostream>
#include <cassert>
using std::cerr;
using std::endl;
#else
#include <iostream.h>
#endif

#include "GraphicsAttributes.H"
#include <Xlibint.h>


// -------------------------------------------------------------------
GraphicsAttributes::GraphicsAttributes(Widget topLevel) {
  appTopLevel = topLevel;
  display = XtDisplay(topLevel);
  screen = XtScreen(topLevel);
  screennumber = DefaultScreen(display);
  int status = XMatchVisualInfo(display, DefaultScreen(display),
				8, PseudoColor, &visual_info);
  if ( status != 0 )
    {
      visual = visual_info.visual;
      depth = 8;
    }
  else
    {
      int status = XMatchVisualInfo(display, DefaultScreen(display),
				    DefaultDepth(display, screennumber), TrueColor, &visual_info);
      if ( status != 0 )
	{
	  visual = visual_info.visual;
	  depth = DefaultDepth(display, screennumber);
	}
      else if( status == 0 ) {
	cerr << "Error: bad XMatchVisualInfo: no PseudoColor Visual" << endl;
	exit(1);
      }
    }
  gc = screen->default_gc;
  root = RootWindow(display, DefaultScreen(display));
  bytesPerPixel = CalculateNBP();
}

int
GraphicsAttributes::PBitmapPaddedWidth(int width) const
{
  return (1+(width-1)/(BitmapPad(display)/8))*BitmapPad(display)/8;
}

// -------------------------------------------------------------------
GraphicsAttributes::~GraphicsAttributes() {
}


// -------------------------------------------------------------------
// return number of bytes per pixel this display uses
int GraphicsAttributes::CalculateNBP() {
  int planes = DisplayPlanes(display, DefaultScreen(display));

  if(planes <= 8) {
    return(1);
  }
  if(planes <= 16) {
    return(2);
  }
  if(planes <= 32) {
    return(4);
  }
  return(1);

} /* CalculateNBP() */
// -------------------------------------------------------------------
// -------------------------------------------------------------------
