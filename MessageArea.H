// ---------------------------------------------------------------
// MessageArea.H
// ---------------------------------------------------------------
#ifndef _MESSAGEAREA_H
#define _MESSAGEAREA_H

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <AMReX_AmrvisConstants.H>

class cMessageArea {
  private:
    Widget wTextOut;
    XmTextPosition currentTextPosition;

  public:
    cMessageArea();
    cMessageArea(Widget printTextHere);
    ~cMessageArea();
    void Init(Widget printTextHere);
    void PrintText(const char *buffer, bool scrollToTop = false,
                   bool clear = false);
};

#endif
