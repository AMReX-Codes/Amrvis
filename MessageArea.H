
//
// $Id: MessageArea.H,v 1.7 2001-02-20 21:48:37 vince Exp $
//

// ---------------------------------------------------------------
// MessageArea.H
// ---------------------------------------------------------------
#ifndef _MESSAGEAREA_H
#define _MESSAGEAREA_H

#include <stdio.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>

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
