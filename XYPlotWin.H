// ---------------------------------------------------------------
// XYPlotWin.H
// ---------------------------------------------------------------
#ifndef _XYPLOTWIN_H_
#define _XYPLOTWIN_H_

#include <Xm/Xm.h>
#undef index

#include <List.H>

#include "AmrvisConstants.H"
#include "XYPlotDataList.H"
#include "XYPlotParam.H"
//#include "GraphicsAttributes.H"
#include "xgout.H"
//#include "PltApp.H"
class PltApp;
class GraphicsAttributes;
class XYPlotParameters;

#ifndef BUFSIZE
#define BUFSIZE 1024
#endif

#define LEFT_CODE	        0x01
#define RIGHT_CODE	        0x02
#define BOTTOM_CODE	        0x04
#define TOP_CODE	        0x08

#define BORDER_PADDING 	        5
#define AXIS_PADDING            5

#ifndef DBL_MAX
#define DBL_MAX		        1e300
#endif

#define NUM_INIT_XSEGS          1000
#define PADDING 	        2
#define SPACE 		        10
#define TICKLENGTH	        5
#define MAXLS           50


typedef struct attr_typ {
  char    lineStyle[MAXLS];
  int     lineStyleLen;
  Pixel   pixelValue;
  Pixmap  markStyle;
} AttrSet;

class XYPlotWin;

typedef void (XYPlotWin::*memberXYCB)(Widget, XtPointer, XtPointer);

typedef struct legend_item_typ {
  XYPlotDataList *list;
  legend_item_typ *prev, *next;
  Widget wid,                     // For legend item drawingarea
   frame,                         // For drawingarea frame
   menu;                          // Popup menu for this item
  char drawQ;
  int style, color;
  Pixel pixel;
#if (BL_SPACEDIM == 2)
  Array<XYPlotDataList *> *anim_lists;
  Array<char> *ready_list;
#endif
} XYPlotLegendItem;


class XYPlotWin {
  
public:

  XYPlotWin(char *title, XtAppContext app, Widget w,
	    PltApp *parent, int type, int curr_frame = 0);
  
  ~XYPlotWin();
  
  void AddDataList(XYPlotDataList *new_list,
		   XYPlotLegendItem *insert_after = NULL);


#if (BL_SPACEDIM == 2)
  void InitializeAnimation(int curr_frame, int num_frames);
  void UpdateFrame(int frame);
  void StopAnimation(void);
#endif

  void PopUp(void) { XtPopup(wXYPlotTopLevel, XtGrabNone); }

  void SetPalette(void);

private:
  
  XtAppContext            appContext;
  Widget                  wTopLevel;
  PltApp                 *pltParent;
  char                   *pltTitle;
  XYPlotParameters       *parameters;
  int                     whichType;

  Widget                  wXYPlotTopLevel;

  GraphicsAttributes	 *GAptr;
  Display                *disp;
  Visual                 *vis;
  Cursor	          cursor,
                          zoomCursor;
  XFontStruct            *labeltextFont, *titletextFont;
  GC		          titletextGC, labeltextGC, rbGC, segGC, dotGC;

  // Other important numbers.
  unsigned char           zoomedInQ;
  XYPlotLegendItem       *legendHead, *legendTail, *colorChangeItem;
  unsigned int            numDrawnItems, numItems;
  unsigned char           lineFormats[8];
  unsigned int            numXsegs;
  XSegment               *Xsegs[2];

  // parameters.
  unsigned char           markQ, tickQ, axisQ, boundBoxQ,
                          plotLinesQ, saveDefaultQ, dispHintsQ,
                          animatingQ, tbool[7];
  int                     lineW, gridW, dotW;
  char                   *XUnitText, *YUnitText, *formatX, *formatY;
  Pixel                   foregroundPix, backgroundPix, gridPix, textPix;
  param_style             gridStyle;
  AttrSet                 AllAttrs[8];
  xgOut                   devInfo;

  Widget                  wLegendMenu, wLegendButtons, wPlotWin,
                          wOptionsDialog, wExportFileDialog, wOptionsWidgets[13];
  Window                  pWindow;
  Atom                    WM_DELETE_WINDOW;

  double                  gridBase, gridStep, gridJuke[101];
  int                     gridNJuke, gridCurJuke;
  double                  lloX, lloY,
                          hhiX, hhiY,     // Global bounding box of window
                          loX, loY,
                          hiX, hiY,       // Local bounding box of window
                          UsrOrgX,
                          UsrOrgY,  // Origin of bounding box in user space
                          UsrOppX,
                          UsrOppY,        // Other point of bounding box
                          XUnitsPerPixel, // X Axis scale factor
                          YUnitsPerPixel; // Y Axis scale factor
  int                     XOrgX, XOrgY,   // Origin of bounding box on screen
                          XOppX, XOppY,   // Other point defining bounding box
                          XLocWinX, XLocWinY, currHint;
#if (BL_SPACEDIM == 2)
  int                     numFrames;
#endif
  int                     currFrame;
  
  void DoError(char *error_str) { fprintf(stderr, "%s\n", error_str); }
  void AddStaticCallback(Widget w, String cbtype,
			 memberXYCB cbf, void *data = NULL);
  void AddStaticWMCallback(Widget w, Atom cbtype,
			   memberXYCB cbf, void *data = NULL);
  void AddStaticEventHandler(Widget w, EventMask mask,
			     memberXYCB cbf, void *data = NULL);
  static void StaticCallback(Widget, XtPointer, XtPointer);
  static void StaticEvent(Widget w, XtPointer client_data,
			  XEvent *event, char*);

  void setBoundingBox (double lowX = 1.0, double lowY = 1.0,
		       double highX = 0.0, double highY = 0.0);
  void updateBoundingBox(XYPlotDataList *list);
  void textX (Widget win, int x, int y, char *text, int just, int style);
  void segX(Widget win, int ns, XSegment *segs, int width,
	    int style, int lappr, Pixel color);
  void dotX(Widget win, int x, int y, int style, int color);
  double initGrid(double low, double high, double step);
  double roundUp(double val);
  void writeValue(char *str, char *fmt, double val, int expv);
  void CalculateBox(void);
  void drawGridAndAxis(void);
  void clearData(void);
  void drawData(void);
  void drawHint(void);

  void CBdoInitializeListColorChange(Widget, XtPointer, XtPointer);
  void CBdoSetListColor(Widget, XtPointer, XtPointer);
  void CBdoExportFileDialog(Widget, XtPointer, XtPointer);
  void CBdoExportFile(Widget, XtPointer, XtPointer data);
  void CBdoASCIIDump(Widget, XtPointer, XtPointer data);
  void CBdoDrawPlot(Widget, XtPointer, XtPointer);
  void CBdoRedrawPlot(Widget, XtPointer, XtPointer);
  void CBdoDrawLegendItem(Widget, XtPointer data, XtPointer);
  void CBdoOptions(Widget, XtPointer, XtPointer);
  void CBdoOptionsToggleButton(Widget, XtPointer data, XtPointer);
  void CBdoOptionsOKButton(Widget, XtPointer data, XtPointer);
  void CBdoSelectDataList(Widget, XtPointer data, XtPointer);
  void CBdoClearData(Widget, XtPointer, XtPointer);
  void CBdoSelectAllData(Widget, XtPointer, XtPointer);
  void CBdoDeselectAllData(Widget, XtPointer, XtPointer);
  void CBdoRubberBanding(Widget, XtPointer, XtPointer);
  void CBdoSetListLevel(Widget w, XtPointer data, XtPointer);
  void CBdoCopyDataList(Widget, XtPointer data, XtPointer);
  void CBdoRemoveDataList(Widget, XtPointer data, XtPointer);
  void CBdoDrawLocation(Widget, XtPointer, XtPointer);

  class XYCBData {  // callback data
    public:
      XYCBData(XYPlotWin *p, void *d, memberXYCB mcb)
        : instance(p), data(d), cbFunc(mcb) { }
      ~XYCBData() { }
      XYPlotWin *instance;
      void *data;
      memberXYCB cbFunc;
    };
  Array<XYCBData *> xycbdPtrs;

  class XYMenuCBData {
    public:
      XYMenuCBData(XYPlotLegendItem *_item, int _which)
        : item(_item), which(_which) { }
      ~XYMenuCBData() { }
      XYPlotLegendItem *item;
      int which;
  };
  Array<XYMenuCBData *> xymenucbdPtrs;
};

void	CBcloseXYPlotWin(Widget, XtPointer, XtPointer);

#endif

