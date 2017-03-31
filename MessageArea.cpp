// ---------------------------------------------------------------
// MessageArea.cpp
// ---------------------------------------------------------------
#include <MessageArea.H>
#include <iostream>


// -------------------------------------------------------------------
cMessageArea::cMessageArea() {
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
void cMessageArea::PrintText(const char *buffer) {
  XmTextInsert(wTextOut, currentTextPosition, const_cast<char *>(buffer));
  currentTextPosition += strlen(buffer);
  XtVaSetValues(wTextOut, XmNcursorPosition, currentTextPosition, NULL);
  XmTextShowPosition(wTextOut, currentTextPosition);
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
