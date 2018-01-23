// ---------------------------------------------------------------
// AVPApp.cpp
// ---------------------------------------------------------------
#include <algorithm>
#include <AVPApp.H>
#include <GlobalUtilities.H>

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


// -------------------------------------------------------------------
void AVPApp::ParseCallTraceFile(std::ifstream &ctFile)
{
  amrex::Vector<CallTraceLine> &cTLines = callTraceData.callTraceLines;
  std::map<int, std::string>  &fNNames = callTraceData.funcNumberNames;
  std::string line;

  cTLines.clear();
  fNNames.clear();

  do {
    std::getline(ctFile, line);
    const std::vector<std::string>& tokens = amrex::Tokenize(line, "\t");
    int offset;
    if(tokens.size() == 7) {
      offset = 0;
    } else if(tokens.size() == 8) {
      offset = 1;  // ---- skip the ---|---|---| entries
    } else {
      continue;
    }

    CallTraceLine  ctl;
    ctl.funcNumber     = atoi(tokens[offset + 0].c_str());
    fNNames[ctl.funcNumber] = tokens[offset + 1].c_str();
    ctl.inclTime       = atof(tokens[offset + 2].c_str());
    ctl.exclTime       = atof(tokens[offset + 3].c_str());
    ctl.nCalls         = atol(tokens[offset + 4].c_str());
    ctl.callStackDepth = atoi(tokens[offset + 5].c_str());
    ctl.callTime       = atof(tokens[offset + 6].c_str());
    cTLines.push_back(ctl);

  } while( ! ctFile.eof());

  amrex::Print() << "_in ParseCallTraceFile:  fNNames.size() = " << fNNames.size() << std::endl;
  amrex::Print() << "_in ParseCallTraceFile:  cTLines.size() = " << cTLines.size() << std::endl;
}


// -------------------------------------------------------------------
void AVPApp::DeriveCallStack(std::ostream &os, Real startTime, Real endTime)
{
  amrex::Vector<CallTraceLine> &cTLines = callTraceData.callTraceLines;
  std::map<int, std::string>  &fNNames = callTraceData.funcNumberNames;
  std::vector<CallTraceLine>::iterator lowB, highB;

  lowB = std::lower_bound(cTLines.begin(), cTLines.end(), startTime,
                          [] (const CallTraceLine &a, const Real &b)
			        { return a.callTime < b; });
  highB = std::upper_bound(cTLines.begin(), cTLines.end(), endTime,
                          [] (const Real &a, const CallTraceLine &b)
			        { return a < b.callTime; });
  long lowIndex(lowB - cTLines.begin());
  long highIndex(highB - cTLines.begin());
  lowIndex  = std::min(lowIndex,  cTLines.size() - 1);
  highIndex = std::min(highIndex, cTLines.size() - 1);
  amrex::Print() << "_in DeriveCallStack:  startTime lowIndex highIndex = "
                 << startTime << "  " << lowIndex << "  " << highIndex << std::endl;
  amrex::Print() << "_in DeriveCallStack:  lowLine = "
                 << fNNames[cTLines[lowIndex].funcNumber] << std::endl;

  amrex::Vector<int> callStack;
  for(int ci(highIndex); ci > lowIndex; --ci) {  // ---- push the range [high ,low)
    callStack.push_back(ci);
  }
  int currentCallDepth(cTLines[lowIndex].callStackDepth);
  for(int ci(lowIndex); ci >= 0; --ci) {
    CallTraceLine &ctl = cTLines[ci];
    if(ctl.callStackDepth == currentCallDepth) {
      callStack.push_back(ci);
      --currentCallDepth;
    }
  }

  for(int i(callStack.size() - 1); i >= 0; --i) {
    int index(callStack[i]);
    CallTraceLine &ctl = cTLines[index];
    if(index == lowIndex) {
      for(int d(0); d < ctl.callStackDepth; ++d) {
        os << "->->";
      }
    } else {
      for(int d(0); d < ctl.callStackDepth; ++d) {
        os << "---|";
      }
    }
    os << "  " << fNNames[ctl.funcNumber] << "  " << ctl.callTime << '\n';
  }
  os << std::endl;

}


// ---------------------------------------------------------------
// ---------------------------------------------------------------




