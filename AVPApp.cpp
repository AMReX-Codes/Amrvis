// ---------------------------------------------------------------
// AVPApp.H
// ---------------------------------------------------------------
#include <Xm/Xm.h>
#include <AVPApp.H>

using namespace amrex;



// -------------------------------------------------------------------
void AVPApp::DrawTimeRange(Widget wCF, int sdLineXL, int sdLineXH,
                           int axisLengthX, int axisLengthY,
                           Real subTimeRangeStart, Real subTimeRangeStop,
			   const std::string &yAxisLabel)
{
  const std::string timeLabel(" time");
  const int xpos(10), ypos(15), positionPad(5);

  std::ostringstream timeRangeS;
  timeRangeS << '[' << subTimeRangeStart << ",  " << subTimeRangeStop << ']';

  XDrawLine(XtDisplay(wCF), XtWindow(wCF), xgc,
            xpos + positionPad, ypos + positionPad,
            xpos + positionPad, ypos + axisLengthY);
  XDrawLine(XtDisplay(wCF), XtWindow(wCF), xgc,
            xpos + positionPad, ypos + axisLengthY,
            xpos + positionPad + axisLengthX, ypos + axisLengthY);
  XDrawString(XtDisplay(wCF), XtWindow(wCF), xgc,
              xpos + positionPad + axisLengthX, ypos+positionPad + axisLengthY,
              timeLabel.c_str(), timeLabel.length());
  XDrawString(XtDisplay(wCF), XtWindow(wCF), xgc,
              xpos + positionPad, ypos,
              yAxisLabel.c_str(), yAxisLabel.length());

  // ---- lines indicating subregion
  XDrawLine(XtDisplay(wCF), XtWindow(wCF), xgc,
            xpos + positionPad + sdLineXL, ypos + axisLengthY,
            xpos + positionPad + sdLineXL, ypos + axisLengthY + 8);
  XDrawLine(XtDisplay(wCF), XtWindow(wCF), xgc,
            xpos + positionPad + sdLineXH, ypos + axisLengthY,
            xpos + positionPad + sdLineXH, ypos + axisLengthY + 8);
  XDrawString(XtDisplay(wCF), XtWindow(wCF), xgc,
              xpos + positionPad, ypos + axisLengthY + 24,
              timeRangeS.str().c_str(), timeRangeS.str().length());

}



// ---------------------------------------------------------------

