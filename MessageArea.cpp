//BL_COPYRIGHT_NOTICE

//
// $Id: MessageArea.cpp,v 1.3 1998-10-27 18:16:36 lijewski Exp $
//

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
