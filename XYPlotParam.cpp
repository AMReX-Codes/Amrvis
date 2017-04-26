// -------------------------------------------------------------------
// XYPlotParam.cpp
// -------------------------------------------------------------------
#include <AMReX_ParallelDescriptor.H>

#include <X11/X.h>
#include <limits>
#undef index

#include <XYPlotParam.H>
#include <Palette.H>
#include <GraphicsAttributes.H>

#include <cstdlib>
#include <cstring>
#ifdef BL_IRIX64
#include <strings.h>  // for index
#endif

#define DEF(name, typ, def_name) \
 if((def_str = XGetDefault(gaPtr->PDisplay(), title, (name)))) \
   { Set_Parameter((name), (typ), def_str); } \
 else { Set_Parameter((name), (typ), (def_name)); }

#define STRDUP(xx) (strcpy(new char[strlen(xx)+1], (xx)))

static const char *defStyle[8] = {     // Default Line styles  
  "1", "10", "11110000", "010111", "1110",
  "1111111100000000", "11001111", "0011000111"
};

static const char *positive[] = {"on", "yes", "true", "1", "affirmative", NULL};
static const char *negative[] = {"off", "no", "false", "0", "negative", NULL};



// -------------------------------------------------------------------
XYPlotParameters::XYPlotParameters(Palette *palPtr,
                                   GraphicsAttributes *gaptr, char *name)
  : param_palette(palPtr), gaPtr(gaptr)
{
  title = new char[strlen(name) + 1];
  strcpy(title, name);

  // Initialize hash table.
  param_table.num_entries = 0;
  param_table.max_density = ST_DEFAULT_MAX_DENSITY;
  param_table.grow_factor = ST_DEFAULT_GROW_FACTOR;
  param_table.reorder_flag = ST_DEFAULT_REORDER_FLAG;
  param_table.num_bins = (ST_DEFAULT_INIT_TABLE_SIZE <= 0) ? 1 :
                                              ST_DEFAULT_INIT_TABLE_SIZE;
  param_table.bins = new st_table_entry *[param_table.num_bins];
  for(int idx(0); idx != param_table.num_bins; ++idx) {
    param_table.bins[idx] = NULL;
  }
  GetHardWiredDefaults();
  string fullDefaultsFile;
  fullDefaultsFile  = getenv("HOME");
  fullDefaultsFile += "/";
  fullDefaultsFile += ".XYPlot.Defaults";
  ReadFromFile(fullDefaultsFile.c_str()); // First read "global" defaults
  fullDefaultsFile = "./.XYPlot.Defaults";
  ReadFromFile(fullDefaultsFile.c_str());   // Then read "local" defaults
}


// -------------------------------------------------------------------
void XYPlotParameters::ResetPalette(Palette *newPalPtr) {
  if(param_palette != newPalPtr) {
    param_palette = newPalPtr;
    param_full *entry;
    if((entry = st_lookup(const_cast<char *>("GridColor"))) != NULL && entry->real_form) {
      free_resource(entry->real_form);
    }
    if((entry = st_lookup(const_cast<char *>("TextColor"))) != NULL && entry->real_form) {
      free_resource(entry->real_form);
    }
    unsigned int colorindex = param_palette->ColorSlots() / 8;
    char colorstr[10];
    char buf[20];
    int ici;
    for(unsigned int idx(0); idx < 8; ++idx) {
      sprintf(buf, "%d.Color", idx);
      ici = (idx * colorindex) + param_palette->PaletteStart();
      ici = std::min(ici, param_palette->PaletteEnd());
      ici = std::max(ici, param_palette->PaletteStart() + 1);  // for lowblack
      sprintf(colorstr, "%u", ici);
      Set_Parameter(buf, INT, colorstr);
    }
  }
}


// -------------------------------------------------------------------
XYPlotParameters::~XYPlotParameters() {
  st_table_entry *ptr, *next;

  // destroy table entries.
  for(int idx(0); idx < param_table.num_bins; ++idx) {
    ptr = param_table.bins[idx];
    while(ptr != NULL) {
      next = ptr->next;
      param_full *entry = ptr->record;
      if(entry->real_form) {
        free_resource(entry->real_form);
      }
      if(entry->text_form) {
        delete [] entry->text_form;
      }
      delete entry;
      delete ptr;
      ptr = next;
    }
  }
  
  // clear table bins and the table itself.
  delete param_table.bins;
  delete [] title;
}


// -------------------------------------------------------------------
void XYPlotParameters::GetHardWiredDefaults() {
  char *def_str;
  char buf[1024];

  DEF("Markers", BOOL, DEF_MARK_FLAG);
  DEF("LargeMarkers", BOOL, DEF_LARGEMARK_FLAG);
  DEF("StyleMarkers", BOOL, DEF_STYLEMARK_FLAG);
  DEF("Ticks", BOOL, DEF_TICK_FLAG);
  DEF("TickAxis", BOOL, DEF_TICKAXIS_FLAG);
  DEF("BoundBox", BOOL, DEF_BB_FLAG);
  DEF("PlotLines", BOOL, DEF_PLOTLINE_FLAG);
  DEF("DisplayHints", BOOL, DEF_DISPHINTS_FLAG);

  DEF("LineWidth", INT, DEF_LINE_WIDTH);
  DEF("GridWidth", INT, DEF_GRID_WIDTH);
  DEF("DotWidth", INT, DEF_DOT_WIDTH);

  DEF("XUnitTextX", STR, DEF_XUNIT_TEXT_X);
  DEF("XUnitTextY", STR, DEF_XUNIT_TEXT_Y);
  DEF("XUnitTextZ", STR, DEF_XUNIT_TEXT_Z);
  DEF("YUnitText", STR, DEF_YUNIT_TEXT);
  DEF("FormatX", STR, DEF_FMT_X);
  DEF("FormatY", STR, DEF_FMT_Y);

  DEF("GridColor", PIXEL, DEF_GRID_COLOR);
  DEF("TextColor", PIXEL, DEF_TEXT_COLOR);
  DEF("GridStyle", STYLE, DEF_GRID_STYLE);
  DEF("LabelFont", FONT, DEF_LABEL_FONT);
  DEF("TitleFont", FONT, DEF_TITLE_FONT);
  DEF("InitialWindowWidth", INT, DEF_INIT_WIN_WIDTH);
  DEF("InitialWindowHeight", INT, DEF_INIT_WIN_HEIGHT);
  DEF("InitialXWindowOffsetX", INT, DEF_INIT_WIN_OFFSET_X);
  DEF("InitialXWindowOffsetY", INT, DEF_INIT_WIN_OFFSET_Y);
  DEF("InitialYWindowOffsetX", INT, DEF_INIT_WIN_OFFSET_X);
  DEF("InitialYWindowOffsetY", INT, DEF_INIT_WIN_OFFSET_Y);
  DEF("InitialZWindowOffsetX", INT, DEF_INIT_WIN_OFFSET_X);
  DEF("InitialZWindowOffsetY", INT, DEF_INIT_WIN_OFFSET_Y);

  // Initalize attribute colors defaults
  unsigned int colorindex = param_palette->ColorSlots() / 8;
  char colorstr[10];
  int ici;
  for(unsigned int idx(0); idx < 8; ++idx) {
    sprintf(buf, "%d.Style", idx);
    Set_Parameter(buf, STYLE, defStyle[idx]);
    sprintf(buf, "%d.Color", idx);
    ici = (idx * colorindex) + param_palette->PaletteStart();
    ici = std::min(ici, param_palette->PaletteEnd());
    ici = std::max(ici, param_palette->PaletteStart() + 1);  // for lowblack
    sprintf(colorstr, "%u", ici);
    Set_Parameter(buf, INT, colorstr);
  }
}


// -------------------------------------------------------------------
void XYPlotParameters::Set_Parameter(const char *name,  param_types type,
				     const char *val)
{
  param_full *entry = st_lookup(const_cast<char *>(name));
  
  if(entry) {
    if(entry->real_form) {
      free_resource(entry->real_form);
    }
    if(entry->text_form) {
      delete [] entry->text_form;
    }
  } else {
    entry = new param_full;
    st_insert(STRDUP(name), entry);
  }
  entry->real_form = NULL;
  entry->type = type;
  entry->text_form = new char[strlen(val) + 1];
  strcpy(entry->text_form ,val);
}


// -------------------------------------------------------------------
int XYPlotParameters::Get_Parameter(char *name, params *val) {
  param_full *entry = st_lookup(name);

  if( ! entry) {
    fprintf(stderr, "Couldn't find parameter %s for %s.\n", name, title);
    return 0;
  }
  
  if( ! entry->real_form) {
    entry->real_form = resolve_entry(name, entry->type, entry->text_form);
  }
  *val = *(entry->real_form);
  return 1;
}


// -------------------------------------------------------------------
void XYPlotParameters::free_resource(params *val) {
  switch (val->type) {
    case INT:
    case STR:
    case BOOL:
    case PIXEL:
    case DBL:
      // No reclaiming necessary
    break;
    case FONT:
      XFreeFont(gaPtr->PDisplay(), val->fontv.value);
    break;
    case STYLE:
      delete val->stylev.dash_list;
    break;
  }
  delete val;
}


// -------------------------------------------------------------------
params * XYPlotParameters::resolve_entry(char *name, param_types type,
				     char *form) {
  static char paramstr[] =
    "Parameter %s: can't translate `%s' into a %s (defaulting to `%s')\n";
  params *result = new params;
  
  result->type = type;
  switch (type) {
    case INT:
      if(sscanf(form, "%d", &result->intv.value) != 1) {
        fprintf(stderr, paramstr, name, form, "integer", DEF_INT);
        result->intv.value = atoi(DEF_INT);
      }
    break;

    case STR:
      result->strv.value = form;
    break;

    case PIXEL:
      if( ! do_color(form, &result->pixv.value, result->pixv.iColorMapSlot)) {
        fprintf(stderr, paramstr, name, form, "color", DEF_PIXEL);
        do_color(DEF_PIXEL, &result->pixv.value, result->pixv.iColorMapSlot);
      }
    break;

    case FONT:
      if( ! do_font(form, &result->fontv.value)) {
        fprintf(stderr, paramstr, name, form, "font", DEF_FONT);
        do_font(DEF_FONT, &result->fontv.value);
      }
    break;

    case STYLE:
      if( ! do_style(form, &result->stylev)) {
        fprintf(stderr, paramstr, name, form, "line style", DEF_STYLE);
        do_style(DEF_STYLE, &result->stylev);
      }
    break;

    case BOOL:
      if( ! do_bool(form, &result->boolv.value)) {
        fprintf(stderr, paramstr, name, form, "boolean flag", DEF_BOOL);
        do_bool(DEF_BOOL, &result->boolv.value);
      }
    break;

    case DBL:
      if(sscanf(form, "%lf", &result->dblv.value) != 1) {
        fprintf(stderr, paramstr, name, form, "double", DEF_DBL);
        result->dblv.value = atof(DEF_DBL);
      }
    break;
  }
  return result;
}


// -------------------------------------------------------------------
int XYPlotParameters::do_color(const char *name, XColor *color, int &cmSlot) {
  Colormap cmap = param_palette->GetColormap();
  if( ! XParseColor(gaPtr->PDisplay(),
		    DefaultColormap(gaPtr->PDisplay(), gaPtr->PScreenNumber()),
		    name, color))
  {
    return 0;
  }
  if(string_compare(name, "black") == 0) {
    color->pixel = BlackPixel(gaPtr->PDisplay(), gaPtr->PScreenNumber());
    cmSlot = param_palette->BlackIndex();;
    XQueryColor(gaPtr->PDisplay(), cmap, color);
    return 1;
  }
  if(string_compare(name, "white") == 0) {
    color->pixel = WhitePixel(gaPtr->PDisplay(), gaPtr->PScreenNumber());
    cmSlot = param_palette->WhiteIndex();;
    XQueryColor(gaPtr->PDisplay(), cmap, color);
    return 1;
  }

  // Choose the closest color in the palette, based on Euclidian distance
  // of RGB values (does this create pastel colors??)

  double red(color->red), green(color->green), blue(color->blue);
  double best(std::numeric_limits<Real>::max());
  unsigned long best_pix = WhitePixel(gaPtr->PDisplay(), gaPtr->PScreenNumber());
  int end(param_palette->PaletteEnd());
  int iBestPalEntry(param_palette->PaletteStart());
  for(int ii(param_palette->PaletteStart()); ii <= end; ++ii) {
    const XColor *newcolor = &param_palette->GetColorCells()[ii];
    double dred(newcolor->red - red);
    double dgreen(newcolor->green - green);
    double dblue(newcolor->blue - blue);
    double temp((dred * dred) + (dgreen * dgreen) + (dblue * dblue));
    if(temp < best) {
      best = temp;
      best_pix = newcolor->pixel;
      iBestPalEntry = ii;
    }
  }
  color->pixel = best_pix;
  cmSlot = iBestPalEntry;
  XQueryColor(gaPtr->PDisplay(), cmap, color);
  
  return 1;
}  


// -------------------------------------------------------------------
int XYPlotParameters::do_font(const char *name, XFontStruct **font_info) {
  char name_copy[DEF_MAX_FONT], query_spec[DEF_MAX_FONT];
  char *font_family, *font_size, **font_list;
  int font_size_value, font_count, i;
  
  // First attempt to interpret as font family/size
  strcpy(name_copy, name);
  if((font_size = strchr(name_copy, '-'))) {
    *font_size = '\0';
    font_family = name_copy;
    ++font_size;
    font_size_value = atoi(font_size);
    if(font_size_value > 0) {
      // Still a little iffy -- what about weight and roman vs. other
      sprintf(query_spec, ISO_FONT, font_family, font_size_value * 10);
      font_list = XListFonts(gaPtr->PDisplay(), query_spec,
			     DEF_MAX_NAMES, &font_count);
      
      // Load first one that you can
      for(i = 0; i < font_count; ++i) {
	if((*font_info = XLoadQueryFont(gaPtr->PDisplay(), font_list[i]))) {
	  break;
	}
      }
      if(*font_info) {
        return 1;
      }
    }
  }
  // Assume normal font name
  *font_info = XLoadQueryFont(gaPtr->PDisplay(), name);
  if((*font_info = XLoadQueryFont(gaPtr->PDisplay(), name)) != NULL) {
    return 1;
  }
  return 0;
}


// -------------------------------------------------------------------
int XYPlotParameters::do_style(const char *list, param_style *val) {
  const char *i;
  char *spot;
  char last_char;
  int count;

  for(i = list; *i; ++i) {
    if((*i != '0') && (*i != '1')) {
      break;
    }
  }
  
  if( ! *i) {
    val->len = 0;
    last_char = '\0';
    for(i = list; *i; ++i) {
      if(*i != last_char) {
	val->len += 1;
	last_char = *i;
      }
    }
    val->dash_list = new char[val->len+1];
    last_char = *list;
    spot = val->dash_list;
    count = 0;
    for(i = list; *i; ++i) {
      if(*i != last_char) {
	*spot++ = (char) count;
	last_char = *i;
	count = 1;
      } else {
        ++count;
      }
    }
    *spot = (char) count;
    return 1;
  } else {
    return 0;
  }
}


// -------------------------------------------------------------------
int XYPlotParameters::do_bool(const char *name, int *val) {
  const char **term;
  
  for(term = positive; *term; ++term) {
    if(string_compare(name, *term) == 0) {
      *val = 1;
      return 1;
    }
  }
  for(term = negative; *term; ++term) {
    if(string_compare(name, *term) == 0) {
      *val = 0;
      return 1;
    }
  }
  return 0;
}


// -------------------------------------------------------------------
void XYPlotParameters::ReadFromFile(const char *filename) {
  FILE *fs;
  if((fs = fopen(filename, "r")) != NULL) {
    char linebuffer[100], keybuffer[50], typebuffer[50], valbuffer[50];
    param_types type;
    while(fgets(linebuffer, 100, fs)) {
      if(sscanf(linebuffer, "%s %s %s", keybuffer, typebuffer, valbuffer) != 3) {
        break;
      }
      if( ! strcmp(typebuffer, "BOOL"))       type = BOOL;
      else if( ! strcmp(typebuffer, "STYLE")) type = STYLE;
      else if( ! strcmp(typebuffer, "INT"))   type = INT;
      else if( ! strcmp(typebuffer, "STR"))   type = STR;
      else if( ! strcmp(typebuffer, "PIXEL")) type = PIXEL;
      else if( ! strcmp(typebuffer, "FONT"))  type = FONT;
      else if( ! strcmp(typebuffer, "DBL"))   type = DBL;
      else break;
      Set_Parameter(keybuffer, type, valbuffer);
    }
    fclose(fs);
  }
}


// -------------------------------------------------------------------
void XYPlotParameters::WriteToFile(char *filename) {
  st_table_entry *ptr;
  FILE *fs;

  if((fs = fopen(filename, "w")) == NULL) {
    return;
  }
  for(int i(0); i < param_table.num_bins; ++i) {
    for(ptr = param_table.bins[i]; ptr; ptr = ptr->next) {
      param_full *val = ptr->record;
      fprintf(fs, "%s\t", ptr->key);
      if(strlen(ptr->key) < 10) {
        fprintf(fs, "\t");
      }
      switch (val->type) {
        case INT:   fprintf(fs, "INT");   break;
        case STR:   fprintf(fs, "STR");   break;
        case PIXEL: fprintf(fs, "PIXEL"); break;
        case FONT:  fprintf(fs, "FONT");  break;
        case STYLE: fprintf(fs, "STYLE"); break;
        case BOOL:  fprintf(fs, "BOOL");  break;
        case DBL:   fprintf(fs, "DBL");   break;
      }
      fprintf(fs, "\t%s\n", val->text_form);
    }
  }
  fclose(fs);
}


//
//
// replace this with a map  vvvvvvvvvvvvvvvv
//
//
// -------------------------------------------------------------------
param_full *XYPlotParameters::st_lookup(register char *key) {
  register st_table_entry *ptr;
  
  // find entry.
  ptr = param_table.bins[strihash(key)];
  while(true) {
    if(ptr == NULL) {
      break;
    }
    if( ! string_compare(key, ptr->key)) {
      return ptr->record;
    }
    ptr = ptr->next;
  }
  return NULL;
}


// -------------------------------------------------------------------
int XYPlotParameters::st_insert(char *key, param_full *value) {
  int hash_val;
  st_table_entry *newentry;
  register st_table_entry *ptr, **last;
  
  hash_val = strihash(key);

  // find entry.
  last = &param_table.bins[hash_val];
  ptr = *last;
  while(ptr && string_compare(key, ptr->key)) {
    last = &ptr->next;
    ptr = *last;
  }
  if(ptr && param_table.reorder_flag) {
    *last = ptr->next;
    ptr->next = param_table.bins[hash_val];
    param_table.bins[hash_val] = ptr;
  }
  
  if(ptr == NULL) {
    // add directly.
    if(param_table.num_entries/param_table.num_bins >= param_table.max_density) {
	rehash();
	hash_val = strihash(key);
    }
    newentry = new st_table_entry;
    newentry->key = key;
    newentry->record = value;
    newentry->next = param_table.bins[hash_val];
    param_table.bins[hash_val] = newentry;
    param_table.num_entries++;
    return 0;
  } else {
    ptr->record = value;
    return 1;
  }
}


// -------------------------------------------------------------------
void XYPlotParameters::rehash(void) {
  st_table_entry *ptr, *next, **old_bins = param_table.bins;
  int i, hash_val, old_num_bins = param_table.num_bins;
  Real new_num_bins(param_table.grow_factor * old_num_bins);
  
  param_table.num_bins = (int) new_num_bins;
  
  if(param_table.num_bins % 2 == 0) {
    param_table.num_bins++;
  }
  
  param_table.bins = new st_table_entry *[param_table.num_bins];
  for(i = 0; i != new_num_bins; ++i) {
    param_table.bins[i] = NULL;
  }

  for(i = 0; i < old_num_bins; ++i) {
    ptr = old_bins[i];
    while(ptr) {
      next = ptr->next;
      hash_val = strihash(ptr->key);
      ptr->next = param_table.bins[hash_val];
      param_table.bins[hash_val] = ptr;
      ptr = next;
    }
  }
  delete old_bins;
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
