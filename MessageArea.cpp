
//
// $Id: MessageArea.cpp,v 1.5 2000-10-02 20:53:08 lijewski Exp $
//

// ---------------------------------------------------------------
// MessageArea.cpp
// ---------------------------------------------------------------
#include "MessageArea.H"


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
void cMessageArea::PrintText(char *buffer) {
  strcpy(localBuffer, buffer);
  XmTextInsert(wTextOut, currentTextPosition, localBuffer);
  currentTextPosition += strlen(localBuffer);
  XtVaSetValues(wTextOut, XmNcursorPosition, currentTextPosition, NULL);
  XmTextShowPosition(wTextOut, currentTextPosition);
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
