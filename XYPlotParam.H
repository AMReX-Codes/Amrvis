// -------------------------------------------------------------------
// XYPlotParam.H
// -------------------------------------------------------------------
#ifndef _XYPLOT_PARAM_H
#define _XYPLOT_PARAM_H

#include <X11/X.h>
#include <X11/Xlib.h>

#include <cctype>

#define ST_DEFAULT_MAX_DENSITY 5
#define ST_DEFAULT_INIT_TABLE_SIZE 11
#define ST_DEFAULT_GROW_FACTOR 2.0
#define ST_DEFAULT_REORDER_FLAG 0

#define ISO_FONT "*-*-%s-medium-r-normal-*-*-%d-*-*-*-*-iso8859-*"


typedef enum param_types_defn {
  INT, STR, PIXEL, FONT, STYLE, BOOL, DBL
} param_types;

typedef struct params_int_defn { // INT
  param_types type;
  int     value;
} param_int;

typedef struct params_str_defn { // STR
  param_types type;
  char   *value;
} param_str;

typedef struct params_pix_defn { // PIXEL
  param_types type;
  XColor  value;
  int iColorMapSlot;
} param_pix;

typedef struct params_font_defn { // FONT
  param_types type;
  XFontStruct *value;
} param_font;

typedef struct params_style_defn { // STYLE
  param_types type;
  int     len;
  char   *dash_list;
} param_style;

typedef struct params_bool_defn { // BOOL
  param_types type;
  int     value;
} param_bool;

typedef struct params_dbl_defn { // DOUBLE
  param_types type;
  double  value;
} param_dbl;

typedef union params_defn {
  param_types type;
  param_int intv;		// INT
  param_str strv;		// STR
  param_pix pixv;		// PIXEL
  param_font fontv;		// FONT
  param_style stylev;		// STYLE
  param_bool boolv;		// BOOL
  param_dbl dblv;		// DBL
} params;

typedef struct param_full_defn {
  param_types type;
  char   *text_form;
  params *real_form;
} param_full;

typedef struct st_table_entry_defn {
  char   *key;
  param_full   *record;
  struct st_table_entry_defn *next;
} st_table_entry;

typedef struct st_table_defn {
  int     num_bins;
  int     num_entries;
  int     max_density;
  int     reorder_flag;
  double  grow_factor;
  st_table_entry **bins;
} st_table;

#include <XYPlotDefaults.H>
#include <XYPlotWin.H>
class GraphicsAttributes;
class Palette;


class XYPlotParameters {
public:
  XYPlotParameters(Palette *palPtr, GraphicsAttributes *gaptr, char *name);
  ~XYPlotParameters();

  //
  // Sets the parameter with the given name to have the type
  // _type_ and the text value _value_.  This will be evaluated
  // to its full form the first time it is referenced using
  // Get_Parameter().  If it is already filled, the old value
  // will be reclaimed.
  //
  void Set_Parameter(const char *name, param_types type, const char *val);

  //
  // This routine sets the value of an existing parameter to a new
  // value.  The type of the parameter remains the same.  Changes
  // in type should be done by using param_set() directly.
  //
  void Reset_Parameter(char *name, char *val);

  //
  // Retrieves a value from the parameter table.  The value
  // is placed in val.  If successful, the routine will
  // return 1.  Otherwise, it will return 0.
  //
  int Get_Parameter(char *name, params *val);

  //
  // Writes the current settings out to a file.
  //
  void WriteToFile(char *filename);

  //
  // Reads in from a file.
  //
  void ReadFromFile(const char *filename);

  //
  // Resets the palette and its dependent parameters.
  //
  void ResetPalette(Palette *newPalPtr);

private:
  params param_temp;
  int param_ok;
  st_table param_table;
  Palette *param_palette;
  GraphicsAttributes *gaPtr;
  char *title;

  //
  // Reads in defaults defined by yours truly.
  //
  void GetHardWiredDefaults(void);


  //
  // Reclaims a resource based on its type
  //
  void free_resource(params *val);

  //
  // Allocates and returns an appropriate parameter structure
  // by translating form into its native type as given by type.
  // If it cant translate the given form, it will fall back onto
  // the default.
  //
  params *resolve_entry(char *name, param_types type, char *form);

  //
  // Translates name into a color and attempts to get the pixel
  // for the color using XAllocColor().
  //
  int do_color(const char *name, XColor *color, int &cmSlot);

  //
  // This routine translates a font name into a font structure.  The
  // font name can be in two forms.  The first form is <family>-<size>.
  // The family is a family name (like helvetica) and the size is
  // in points (like 12).  If the font is not in this form, it
  // is assumed to be a regular X font name specification and
  // is looked up using the standard means.
  //
  int do_font(const char *name, XFontStruct **font_info);

  //
  // Translates a string representation of a dash specification into
  // a form suitable for use in XSetDashes().  Assumes list
  // is a null terminated string of ones and zeros.
  //
  int do_style(const char *list, param_style *val);

  //
  // Translates a string representation into a suitable binary value.
  // Can parse all kinds of interesting boolean type words.
  //
  int do_bool(const char *name, int *val);

  //
  // Look up item in table -- return zero if not found
  //
  param_full * st_lookup(register char *key);

  //
  // Insert an item into the table - replacing if it already exists
  //
  int st_insert(char *key, param_full *value);

  //
  // Dumps a single parameter
  //
  void dump_it (char *key, char *value);

  //
  // Grows table
  //
  void rehash(void);

  int strihash(register char *xypstring) {
    register int val = 0;
    register int c;
    register int i = 0;
    
    while ((c = *xypstring++) != '\0') {
      if(isupper(c)) {
        c = tolower(c);
      }
      val = (val - ++i) + 997 * c;
    }
    
    return ((val < 0) ? -val : val) % param_table.num_bins;
  }
  
  int string_compare(const char *s1, const char *s2) {
    char c, d;
    
    while ((c = *s1) != '\0') {
      if(isupper(c)) {
        c = tolower(c);
      }
      d = *s2;
      if(isupper(d)) {
        d = tolower(d);
      }
      if(c != d) {
        return 1;
      }
      ++s1;
      ++s2;
    }
    if(*s2 != '\0') return 1;
    return 0;
  }
    
};

#endif
