// -------------------------------------------------------------------
// GraphicsAttributes.C
// -------------------------------------------------------------------
#include "GraphicsAttributes.H"
#include <Xlibint.h>

// -------------------------------------------------------------------
GraphicsAttributes::GraphicsAttributes(Widget topLevel) {
  appTopLevel = topLevel;
  display = XtDisplay(topLevel);
  screen = XtScreen(topLevel);
  screennumber = DefaultScreen(display);
  int status;
  status = XMatchVisualInfo(display, DefaultScreen(display),
			    8, PseudoColor, &visual_info);
  if( ! status) {
    cerr << "Error: bad XMatchVisualInfo" << endl;
  }
  visual = visual_info.visual;
  gc = screen->default_gc;
  root = RootWindow(display, DefaultScreen(display));
  //depth = DefaultDepthOfScreen(screen);
  depth = 8;
  bytesPerPixel = CalculateNBP();
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
