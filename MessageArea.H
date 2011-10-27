// ---------------------------------------------------------------
// MessageArea.H
// ---------------------------------------------------------------
#ifndef _MESSAGEAREA_H
#define _MESSAGEAREA_H

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <AmrvisConstants.H>

class cMessageArea {
  private:
    Widget wTextOut;
    XmTextPosition currentTextPosition;
    char localBuffer[BUFSIZ];

  public:
    cMessageArea();
    cMessageArea(Widget printTextHere);
    ~cMessageArea();
    void Init(Widget printTextHere);
    void PrintText(char *buffer);
};

#endif
