/*
 * Output Device Information
 *
 * This file contains definitions for output device interfaces
 * to the graphing program xgraph.
 */
#ifndef _h_xgout
#define _h_xgout
/* Passed device option flags */
#define D_DOCU		0x01

/* Returned device capability flags */
#define D_COLOR		0x01

/* Text justifications */
#define T_CENTER	0
#define T_LEFT		1
#define T_UPPERLEFT	2
#define T_TOP		3
#define T_UPPERRIGHT	4
#define T_RIGHT		5
#define T_LOWERRIGHT	6
#define T_BOTTOM	7
#define T_LOWERLEFT	8

/* Text styles */
#define T_AXIS		0
#define T_TITLE		1

/* Line Styles */
#define L_AXIS		0
#define L_ZERO		1
#define L_VAR		2

/* Marker Styles */
#define P_PIXEL		0
#define P_DOT		1
#define P_MARK		2

/* Output device information returned by initialization routine */

typedef struct xg_out {
    int     areaW,
            areaH;		/* Width and height in pixels            */
    int     bdrPad;		/* Padding from border                   */
    int     axisPad;		/* Extra space around axis labels        */
    int     tickLen;		/* Length of tick mark on axis           */
    int     legendPad;		/* Top of legend text to legend line     */
    int     axisW;		/* Width of big character of axis font   */
    int     axisH;	        /* Height of big character of axis font  */
    int     titleW;	        /* Width of big character of title font  */
    int     titleH;	        /* Height of big character of title font */
    int     maxSegs;		/* Maximum number of segments in group   */

    char   *user_state;		/* User supplied data                    */
}       xgOut;

#define ERRBUFSIZE	2048
#endif /* _h_xgout */
