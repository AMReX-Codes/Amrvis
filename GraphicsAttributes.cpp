// ---------------------------------------------------------------
// GraphicsAttributes.cpp
// ---------------------------------------------------------------
#include <AMReX.H>

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <GraphicsAttributes.H>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#undef index


// -------------------------------------------------------------------
unsigned long buildShift(unsigned long mask) {
  unsigned long shift(0);
  while( ! (mask & 1)) {
    mask >>= 1;
    ++shift;
  }
  return shift;
}


// -------------------------------------------------------------------
GraphicsAttributes::GraphicsAttributes(Widget topLevel)
  : appTopLevel(topLevel)
{
  display = XtDisplay(topLevel);
  screen = XtScreen(topLevel);
  screennumber = DefaultScreen(display);
/*
  int status = XMatchVisualInfo(display, DefaultScreen(display),
				8, PseudoColor, &visual_info);
  cout << "----------- status = " << status << endl;
  //status = 0;
  if(status != 0) {
      cout << "***************** using PseudoColor visual" << endl;
    visual = visual_info.visual;
    depth = 8;
    red_shift = blue_shift = green_shift = 0;
  } else {
    int status = XMatchVisualInfo(display, DefaultScreen(display),
                                  DefaultDepth(display, screennumber),
                                  TrueColor, &visual_info);
    if(status != 0) {
    cout << "***************** using TrueColor visual" << endl;
      visual = visual_info.visual;
      depth = DefaultDepth(display, screennumber);
      red_shift = buildShift(visual_info.red_mask);
      green_shift = buildShift(visual_info.green_mask);
      blue_shift = buildShift(visual_info.blue_mask);
    } else if( status == 0 ) {
      amrex::Abort("Error: bad XMatchVisualInfo: no PseudoColor Visual.");
    }
  }
*/

/*
*/
  int status(0);
  status = XMatchVisualInfo(display, DefaultScreen(display),
			    DefaultDepth(display, screennumber),
			    TrueColor, &visual_info);
  if(status != 0) {
    //cout << "***************** using TrueColor visual" << endl;
    visual = visual_info.visual;
#ifdef AV_NX_FIX
    int nItems(-1);
    Visual *defaultV(XDefaultVisual(display, screennumber));
    XVisualInfo *defaultXVI, defaultXVITemplate;
    defaultXVITemplate.visualid = XVisualIDFromVisual(defaultV);
    defaultXVI = XGetVisualInfo(display, VisualIDMask, &defaultXVITemplate, &nItems);
    if(visual_info.red_mask   == defaultXVI->red_mask &&
       visual_info.green_mask == defaultXVI->green_mask &&
       visual_info.blue_mask  == defaultXVI->blue_mask)
    {
      visual = defaultV;
    }
    XFree(defaultXVI);
#endif
    depth = DefaultDepth(display, screennumber);
    red_shift = buildShift(visual_info.red_mask);
    green_shift = buildShift(visual_info.green_mask);
    blue_shift = buildShift(visual_info.blue_mask);
    //cout << "***************** depth = " << depth << endl;
  } else {
    status = XMatchVisualInfo(display, DefaultScreen(display),
			      8, PseudoColor, &visual_info);
    if(status != 0) {
      cout << "***************** using PseudoColor visual" << endl;
      visual = visual_info.visual;
      depth = 8;
      red_shift   = 0;
      blue_shift  = 0;
      green_shift = 0;
    } else if( status == 0 ) {
      amrex::Abort("Error: bad XMatchVisualInfo: no PseudoColor Visual.");
    }
  }

  gc = screen->default_gc;
  root = RootWindow(display, DefaultScreen(display));
  bytesPerPixel = CalculateNBP();
}


// -------------------------------------------------------------------
int GraphicsAttributes::PBitmapPaddedWidth(int width) const {
  return (1+(width-1)/(BitmapPad(display)/8))*BitmapPad(display)/8;
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
ostream& operator<<(ostream &os, const GraphicsAttributes &ga) {
  return os << "("
	    << "red_shift = " << ga.PRedShift() << ", "
	    << "green_shift = " << ga.PGreenShift() << ", "
	    << "blue_shift = " << ga.PBlueShift() << ", "
	    << "red_mask = " << ga.PRedMask() << ", "
	    << "green_mask = " << ga.PGreenMask() << ", "
	    << "blue_mask = " << ga.PBlueMask() << ", "
	    << "bits_per_rgb = " << ga.PBitsPerRGB()
	    << ")";
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
