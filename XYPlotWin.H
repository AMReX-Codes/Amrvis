// ---------------------------------------------------------------
// XYPlotWin.H
// ---------------------------------------------------------------
#ifndef _XYPLOTWIN_H_
#define _XYPLOTWIN_H_

#include <Xm/Xm.h>
#undef index

#include <list>
using std::list;

#include <AMReX_AmrvisConstants.H>
#include <AMReX_XYPlotDataList.H>
#include <XYPlotParam.H>
#include <xgout.H>

class AVPApp;
class GraphicsAttributes;
class XYPlotParameters;

#define LEFT_CODE	        0x01
#define RIGHT_CODE	        0x02
#define BOTTOM_CODE	        0x04
#define TOP_CODE	        0x08

#define BORDER_PADDING 	        5
#define AXIS_PADDING            5

#define NUM_INIT_XSEGS          1000
#define PADDING 	        2
#define SPACE 		        10
#define TICKLENGTH	        5
#define MAXLS                   50


typedef struct attr_typ {
  char    lineStyle[MAXLS];
  int     lineStyleLen;
  Pixel   pixelValue;
  Pixmap  markStyle;
} AttrSet;

class XYPlotWin;

typedef void (XYPlotWin::*memberXYCB)(Widget, XtPointer, XtPointer);

typedef struct {
  amrex::XYPlotDataList *XYPLIlist;
  Widget wid;                     // For legend item drawingarea
  Widget frame;                   // For drawingarea frame
  Widget menu;                    // Popup menu for this item
  bool drawQ;
  int style, color;
  Pixel pixel;
#if (BL_SPACEDIM != 3)
  amrex::Vector<amrex::XYPlotDataList *> *anim_lists;
  amrex::Vector<char> *ready_list;
#endif
} XYPlotLegendItem;


class XYPlotWin {
  public:
    XYPlotWin(char *title, XtAppContext app, Widget w,
	      //PltApp *parent, int type, int curr_frame = 0);
	      AVPApp *parent, int type, int curr_frame = 0);
    ~XYPlotWin();
  
    void AddDataList(amrex::XYPlotDataList *new_list,
		     XYPlotLegendItem *insert_after = NULL);

#if (BL_SPACEDIM != 3)
    void InitializeAnimation(int curr_frame, int num_frames);
    void UpdateFrame(int frame);
    void StopAnimation();
#endif

    void PopUp() { XtPopup(wXYPlotTopLevel, XtGrabNone); }
    void SetPalette();
    void DoASCIIDump(FILE *fs, const char *plotname);
    int  NumItems() { return numItems; }

  private:
    XtAppContext            appContext;
    Widget                  wTopLevel;
    AVPApp                 *pltParent;
    char                   *pltTitle;
    XYPlotParameters       *parameters;
    int                     whichType;

    Widget                  wXYPlotTopLevel;

    GraphicsAttributes	 *gaPtr;
    Display              *disp;
    Visual               *vis;
    Cursor	          cursor, zoomCursor;
    XFontStruct          *labeltextFont, *titletextFont;
    GC		          titletextGC, labeltextGC, rbGC, segGC, dotGC;

    // Other important numbers.
    bool                    zoomedInQ;
    XYPlotLegendItem       *colorChangeItem;
    list<XYPlotLegendItem *> legendList;
    unsigned int            numDrawnItems, numItems;
    unsigned char           lineFormats[8];
    unsigned int            numXsegs;
    XSegment               *Xsegs[2];

    // parameters.
    unsigned char           markQ, tickQ, axisQ, boundBoxQ;
    unsigned char           plotLinesQ, saveDefaultQ, dispHintsQ;
    bool                    animatingQ;
    unsigned char           tbool[7];
    int                     lineW, gridW, dotW;
    char                   *XUnitText, *YUnitText, *formatX, *formatY;
    Pixel                   foregroundPix, backgroundPix, gridPix, textPix;
    param_style             gridStyle;
    AttrSet                 AllAttrs[8];
    xgOut                   devInfo;

    Widget wLegendMenu, wLegendButtons, wPlotWin, wScrollArea;
    Widget wOptionsDialog, wExportFileDialog, wOptionsWidgets[13];
    Widget wChooseColor;
    Window pWindow;
    Atom   WM_DELETE_WINDOW;

    double  dGridBase, dGridStep;
    double  lloX, lloY, hhiX, hhiY;  // Global bounding box of window
    double  loX, loY, hiX, hiY;      // Local bounding box of window
    double  dUsrOrgX, dUsrOrgY;      // Origin of bounding box in user space
    double  dUsrOppX, dUsrOppY;      // Other point of bounding box
    double  dXUnitsPerPixel;         // X Axis scale factor
    double  dYUnitsPerPixel;         // Y Axis scale factor
    int     iXOrgX, iXOrgY;          // Origin of bounding box on screen
    int     iXOppX, iXOppY;          // Other point defining bounding box
    int     iXLocWinX, iXLocWinY, iCurrHint;
#if (BL_SPACEDIM != 3)
    int     numFrames;
#endif
    int     currFrame;
  
    void ReattachLegendFrames();
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

    void SetBoundingBox(double lowX  = 1.0, double lowY  = 1.0,
		        double highX = 0.0, double highY = 0.0);
    void UpdateBoundingBox(amrex::XYPlotDataList *list);
    void textX (Widget win, int x, int y, char *text, int just, int style);
    void segX(Widget win, int ns, XSegment *segs, int width,
	      int style, int lappr, Pixel color);
    void dotX(Widget win, int x, int y, int style, int color);
    double InitGrid(double low, double high, double step);
    double roundUp(double val);
    void writeValue(char *str, char *fmt, double val, int expv);
    void CalculateBox();
    void drawGridAndAxis();
    void clearData();
    void drawData();
    void drawHint();

    void CBdoInitializeListColorChange(Widget, XtPointer, XtPointer);
    void CBdoSetListColor(Widget, XtPointer, XtPointer);
    void CBdoExportFileDialog(Widget, XtPointer, XtPointer);
    void CBdoExportFile(Widget, XtPointer, XtPointer data);
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
    void CBdoASCIIDump(Widget, XtPointer, XtPointer data);

    class XYCBData {  // callback data
      public:
        XYCBData(XYPlotWin *p, void *d, memberXYCB mcb)
          : instance(p), data(d), cbFunc(mcb) { }
        ~XYCBData() { }
        XYPlotWin *instance;
        void *data;
        memberXYCB cbFunc;
      };
    amrex::Vector<XYCBData *> xycbdPtrs;

    class XYMenuCBData {
      public:
        XYMenuCBData(XYPlotLegendItem *_item, int _which)
          : item(_item), which(_which) { }
        ~XYMenuCBData() { }
        XYPlotLegendItem *item;
        int which;
    };
    amrex::Vector<XYMenuCBData *> xymenucbdPtrs;
};

void CBcloseXYPlotWin(Widget, XtPointer, XtPointer);

#endif

