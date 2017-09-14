// ---------------------------------------------------------------
// MessageArea.cpp
// ---------------------------------------------------------------
#include <MessageArea.H>
#include <iostream>


// -------------------------------------------------------------------
cMessageArea::cMessageArea() {
  currentTextPosition = 0;
}


// -------------------------------------------------------------------
cMessageArea::cMessageArea(Widget printTextHere) {
  wTextOut = printTextHere;
  currentTextPosition = 0;
}


// -------------------------------------------------------------------
void cMessageArea::Init(Widget printTextHere) {
  wTextOut = printTextHere;
  currentTextPosition = 0;
}


// -------------------------------------------------------------------
cMessageArea::~cMessageArea() {
}


// -------------------------------------------------------------------
void cMessageArea::PrintText(const char *buffer, bool scrollToTop,
                             bool clear)
{
  if(clear) {
    XmTextReplace(wTextOut, 0, XmTextGetLastPosition(wTextOut), const_cast<char *>(buffer));
    currentTextPosition = strlen(buffer);
  } else {
    XmTextInsert(wTextOut, currentTextPosition, const_cast<char *>(buffer));
    currentTextPosition += strlen(buffer);
  }
  XtVaSetValues(wTextOut, XmNcursorPosition, currentTextPosition, NULL);
  if(scrollToTop) {
    XmTextShowPosition(wTextOut, 0);
  } else {
    XmTextShowPosition(wTextOut, currentTextPosition);
  }
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
