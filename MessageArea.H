//BL_COPYRIGHT_NOTICE

//
// $Id: MessageArea.H,v 1.3 1998-10-27 18:16:36 lijewski Exp $
//

#ifndef _MESSAGEAREA_H
#define _MESSAGEAREA_H

#include <Text.h>

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
