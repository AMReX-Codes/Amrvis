
//
// $Id: GlobalUtilities.cpp,v 1.50 2002-12-10 20:12:23 vince Exp $
//

// ---------------------------------------------------------------
// GlobalUtilities.cpp
// ---------------------------------------------------------------
#include "GlobalUtilities.H"
#include "FArrayBox.H"
#include "FabConv.H"
#include <fstream>
#include <iostream>
using std::ifstream;
using std::ofstream;
using std::cout;
using std::cerr;
using std::endl;
using std::min;
using std::max;

#include "PltApp.H"
#include "ParallelDescriptor.H"

const int DEFAULTMAXPICTURESIZE = 600000;

int boundaryWidth;
int skipPltLines;
int boxColor;
int maxPictureSize;
FileType fileType;
bool animation;
Array<string> comlinefilename;
string initialDerived;
string initialFormat;
string initialPalette;
string initialLightingFile;
int fileCount;
int sleepTime;
bool givenBox;
bool givenBoxSlice;
bool makeSWFData;
bool makeSWFLight;
bool lowBlack;
bool dumpSlices;
bool sliceAllVars;
bool givenFilename;
Box comlinebox;
bool verbose;
Array< std::list<int> > dumpSliceList;
bool specifiedMinMax;
Real specifiedMin;
Real specifiedMax;
bool useMaxLevel;
int  maxLevel;
int  maxPaletteIndex;
bool SGIrgbfile(true);
int  fabIOSize;
bool bShowBody(true);

char *FileTypeString[] = {
  "invalidtype", "fab", "multifab", "newplt"
};

// -------------------------------------------------------------------
const string &AVGlobals::GetInitialDerived() {
  return initialDerived;
}


// -------------------------------------------------------------------
void AVGlobals::SetInitialDerived(const string &initialderived) {
  initialDerived = initialderived;
}


// -------------------------------------------------------------------
const string &AVGlobals::GetPaletteName() {
  return initialPalette;
}


// -------------------------------------------------------------------
const string &AVGlobals::GetLightingFileName() {
  return initialLightingFile;
}


// -------------------------------------------------------------------
void AddSlices(int dir, char *sliceset) {
  int rangeStart(-1), rangeEnd(-1), slice(-1);
  bool rangeSpecified(false);
  dumpSlices = true;

  for(int ipos(1); ipos <= strlen(sliceset); ++ipos) { // skip first char for neg
    if(sliceset[ipos] == '-') {  // we have a range of values specified
      rangeSpecified = true;
      rangeStart = atoi(sliceset);
      rangeEnd   = atoi(sliceset + ipos + 1);
      sliceset[ipos] = '\0';
      break;
    }
  }

  list<int>::iterator dsliter;
  if(rangeSpecified) {
    if(BL_SPACEDIM == 2 && dir == ZDIR) {
      slice = 0;
      dsliter =  find(dumpSliceList[dir].begin(), dumpSliceList[dir].end(), slice);
      if(dsliter == dumpSliceList[dir].end()) {
        dumpSliceList[dir].push_back(slice);
      }
    } else {
      for(slice = rangeStart; slice <= rangeEnd; ++slice) {
        dsliter =  find(dumpSliceList[dir].begin(),dumpSliceList[dir].end(),slice);
        if(dsliter == dumpSliceList[dir].end()) {
          dumpSliceList[dir].push_back(slice);
        }
      }
    }
  } else {
    slice = atoi(sliceset);
    if(BL_SPACEDIM == 2 && dir == ZDIR) {
      slice = 0;
    }
    dsliter =  find(dumpSliceList[dir].begin(),dumpSliceList[dir].end(),slice);
    if(dsliter == dumpSliceList[dir].end()) {
      dumpSliceList[dir].push_back(slice);
    }
  }
}


// -------------------------------------------------------------------
bool AVGlobals::ReadLightingFile(const string &lightdefaultsFile,
                      Real &ambientDef, Real &diffuseDef, Real &specularDef,
                      Real &shinyDef,
                      Real &minRayOpacityDef, Real &maxRayOpacityDef)
{
  char defaultString[LINELENGTH], cRealIn[LINELENGTH];
  // try to find the defaultsFile
  char buffer[BUFSIZ];

  ifstream defs(lightdefaultsFile.c_str());

  if(defs.fail()) {
    cout << "Cannot find lighting defaults file:  " << lightdefaultsFile << endl;
    return false;
  } else {
    cout << "Reading lighting defaults from:  " << lightdefaultsFile << endl;

    ws(defs);
    defs.getline(buffer, BUFSIZ, '\n');
  
    while( ! defs.eof()) {
      sscanf(buffer,"%s", defaultString);
    
      if(defaultString[0] != '#') {  // a comment starts with #
        if(strcmp(defaultString,"ambient") == 0) {
          sscanf(buffer, "%s%s", defaultString, cRealIn);
          ambientDef = atof(cRealIn);
        } else if(strcmp(defaultString, "diffuse") == 0) {
          sscanf(buffer, "%s%s", defaultString, cRealIn);
          diffuseDef = atof(cRealIn);
        } else if(strcmp(defaultString, "specular") == 0) {
          sscanf(buffer, "%s%s", defaultString, cRealIn);
          specularDef = atof(cRealIn);
        } else if(strcmp(defaultString, "shiny") == 0) {
          sscanf(buffer, "%s%s", defaultString, cRealIn);
          shinyDef = atof(cRealIn);
        } else if(strcmp(defaultString, "minRayOpacity") == 0) {
          sscanf(buffer, "%s%s", defaultString, cRealIn);
          minRayOpacityDef = atof(cRealIn);
        } else if(strcmp(defaultString, "maxRayOpacity") == 0) {
          sscanf(buffer, "%s%s", defaultString, cRealIn);
          maxRayOpacityDef = atof(cRealIn);
        }
      }
      defs.getline(buffer, BUFSIZ, '\n');
    }
  }
  return true;
}


// -------------------------------------------------------------------
bool AVGlobals::WriteLightingFile(const string &lightdefaultsFile,
                       const Real &ambientDef, const Real &diffuseDef,
		       const Real &specularDef, const Real &shinyDef,
                       const Real &minRayOpacityDef, const Real &maxRayOpacityDef)
{

  // the format of this file is:
  //ambient 0.42
  //diffuse 0.41
  //specular 0.40
  //shiny 12.0
  //minRayOpacity 0.04
  //maxRayOpacity 0.96

  if(ParallelDescriptor::IOProcessor()) {
    ofstream defs(lightdefaultsFile.c_str());
    if(defs.fail()) {
      cerr << "***** Error writing lighting file:  filename = "
	   << lightdefaultsFile << endl;
      return false;
    } else {
      defs << "ambient " << ambientDef << endl;
      defs << "diffuse " << diffuseDef << endl;
      defs << "specular " << specularDef << endl;
      defs << "shiny " << shinyDef << endl;
      defs << "minRayOpacity " << minRayOpacityDef << endl;
      defs << "maxRayOpacity " << maxRayOpacityDef << endl;
      defs.close();
    }
  }
  return true;
}


// -------------------------------------------------------------------
void AVGlobals::GetDefaults(const string &defaultsFile) {
  char buffer[BUFSIZ];
  char defaultString[LINELENGTH];
  char tempString[LINELENGTH];
  int  tempInt;

  // standard defaults
  PltApp::SetDefaultPalette("Palette");
  PltApp::SetDefaultLightingFile("amrvis.lighting");
  initialPalette = "Palette";
  PltApp::SetInitialDerived("density");
  PltApp::SetInitialScale(1);
  PltApp::SetInitialFormatString("%7.5f");
  PltApp::SetDefaultShowBoxes(true);
  PltApp::SetInitialWindowHeight(500);
  PltApp::SetInitialWindowWidth(850);
  PltApp::SetReserveSystemColors(24);
  maxPictureSize = DEFAULTMAXPICTURESIZE;
  boundaryWidth = 0;
  skipPltLines = 0;
  maxPaletteIndex = 255;  // dont clip the top palette index (default)
  boxColor = -1;  // invalid
  fileType = NEWPLT;  // default
  fabIOSize = 0;
  lowBlack = false;
  bShowBody = true;

  // try to find the defaultsFile
  string fullDefaultsFile;

  fullDefaultsFile = "./" + defaultsFile;     // try dot first
  ifstream defs;
  defs.open(fullDefaultsFile.c_str());

  if(defs.fail()) {  // try ~ (tilde)
    cout << "Cannot find amrvis defaults file:  " << fullDefaultsFile << endl;
    fullDefaultsFile  = getenv("HOME");
    fullDefaultsFile += "/";
    fullDefaultsFile += defaultsFile;
    defs.clear();  // must do this to clear the fail bit
    defs.open(fullDefaultsFile.c_str());
    if(defs.fail()) {  // try ~/.  (hidden file)
      cout << "Cannot find amrvis defaults file:  " << fullDefaultsFile << endl;
      fullDefaultsFile  = getenv("HOME");
      fullDefaultsFile += "/.";
      fullDefaultsFile += defaultsFile;
      defs.clear();  // must do this to clear the fail bit
      defs.open(fullDefaultsFile.c_str());
      if(defs.fail()) {  // punt
        cout << "Cannot find amrvis defaults file:  " << fullDefaultsFile << endl;
        cout << "Using standard defaults." << endl;
        return;
      }
    }
  }
  if(ParallelDescriptor::IOProcessor()) {
    cout << "Reading defaults from:  " << fullDefaultsFile << endl;
  }

  ws(defs);
  defs.getline(buffer, BUFSIZ, '\n');
  
  while( ! defs.eof()) {
    sscanf(buffer,"%s", defaultString);
    
    if(defaultString[0] != '#') {  // a comment starts with #
      if(strcmp(defaultString,"palette") == 0) {
        sscanf(buffer, "%s%s",defaultString, tempString);
        PltApp::SetDefaultPalette(tempString);
        initialPalette = tempString;
      }
      else if(strcmp(defaultString,"lightingfile") == 0) {
        sscanf(buffer, "%s%s",defaultString, tempString);
        PltApp::SetDefaultLightingFile(tempString);
        initialLightingFile = tempString;
      }
      else if(strcmp(defaultString, "initialderived") == 0) {
        sscanf(buffer, "%s%s", defaultString, tempString);
        PltApp::SetInitialDerived(tempString);
	initialDerived = tempString;
      }
      else if(strcmp(defaultString, "initialscale") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        PltApp::SetInitialScale(tempInt);
      }
      else if(strcmp(defaultString, "numberformat") == 0) {
        sscanf(buffer, "%s%s", defaultString, tempString);
        PltApp::SetInitialFormatString(tempString);
      }
      else if(strcmp(defaultString, "windowheight") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        PltApp::SetInitialWindowHeight(tempInt);
      }
      else if(strcmp(defaultString, "windowwidth") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        PltApp::SetInitialWindowWidth(tempInt);
      }
      else if(strcmp(defaultString, "maxpixmapsize") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        maxPictureSize = tempInt;
      }
      else if(strcmp(defaultString, "reservesystemcolors") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        PltApp::SetReserveSystemColors(tempInt);
      }
      else if(strcmp(defaultString, "showboxes") == 0) {
        sscanf(buffer, "%s%s", defaultString, tempString);
        if(*tempString == 't' || *tempString == 'T') {
          PltApp::SetDefaultShowBoxes(true);
        } else {
          PltApp::SetDefaultShowBoxes(false);
        }
      }
      else if(strcmp(defaultString, "showbody") == 0) {
        sscanf(buffer, "%s%s", defaultString, tempString);
        if(*tempString == 'f' || *tempString == 'F') {
          bShowBody = false;
        }
      }
      else if(strcmp(defaultString, "ppm") == 0) {
        sscanf(buffer, "%s%s", defaultString, tempString);
        if(*tempString == 't' || *tempString == 'T') {
          AVGlobals::ClearSGIrgbFile();
        }
      }
      else if(strcmp(defaultString, "boundarywidth") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        boundaryWidth = tempInt;
      }
      else if(strcmp(defaultString, "maxlev") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        maxLevel = tempInt;
        useMaxLevel = true;
        if(maxLevel < 0) {
          cerr << "Error in defaults file:  invalid parameter for maxLevel:  "
               << maxLevel << endl;
          maxLevel = -1;
          useMaxLevel = false;
        }
      }
      else if(strcmp(defaultString, "sleep") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        sleepTime = tempInt;
      }
      else if(strcmp(defaultString, "skippltlines") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        skipPltLines = tempInt;
      }
      else if(strcmp(defaultString, "boxcolor") == 0) {
        sscanf(buffer, "%s%d", defaultString, &tempInt);
        boxColor = tempInt;
      }
      else if(strcmp(defaultString, "filetype") == 0) {
        sscanf(buffer, "%s%s", defaultString, tempString);
	if(strcmp(tempString, "fab") == 0) {
          fileType = FAB;
	} else if(strcmp(tempString, "multifab") == 0) {
          fileType = MULTIFAB;
	} else if(strcmp(tempString, "newplt") == 0) {
          fileType = NEWPLT;
	} else {  // error
	  cerr << "Error in defaults file:  invalid parameter for filetype:  "
	       << tempString << endl;
	}
      }
      else if(strcmp(defaultString, "cliptoppalette") == 0) {
        maxPaletteIndex = 254;  // clip the top palette index
      }
      else if(strcmp(defaultString, "fixdenormals") == 0) {
        RealDescriptor::SetFixDenormals();
      }
      else if(strcmp(defaultString, "lowblack") == 0) {
        lowBlack = true;
      }
      else {
        cout << "bad default argument:  " << defaultString << endl;
      }
    }

    ws(defs);
    defs.getline(buffer, BUFSIZ, '\n');
  }
  defs.close();

}  // end GetDefaults


// -------------------------------------------------------------------
void PrintUsage(char *exname) {
  cout << endl;
  cout << exname << " [-help]" << endl;
  cout << "       [<file type flag>] [-v]" << endl;
  //cout << "       [-bw n] " << endl;
  cout << "       [-maxpixmapsize <max picture size in # of pixels>]" << endl;
  cout << "       [-xslice n] [-yslice n] [-zslice n] [-sliceallvars]" << endl;
# if (BL_SPACEDIM == 2)
  cout << "       [-boxslice xlo ylo xhi yhi]" << endl;
  cout << "       [-a]" << endl;
# else
  cout << "       [-boxslice xlo ylo zlo xhi yhi zhi]" << endl;
#endif
  cout << "       [-fabiosize nbits]" << endl;
  cout << "       [-maxlev n]" << endl;
  cout << "       [-palette palname] [-initialderived dername]" << endl;
  cout << "       [-lightingfile name]" << endl;
  cout << "       [-initialscale n] [-showboxes tf] [-numberformat fmt]" << endl;
  cout << "       [-lowblack] [-showbody tf]"<< endl;
  cout << "       [-cliptoppalette]"<< endl;
  cout << "       [-fixdenormals]"<< endl;
  cout << "       [-ppm]" << endl;
#if (BL_SPACEDIM == 3)
#ifdef BL_VOLUMERENDER
    cout << "       [-boxcolor n]" << endl;
    cout << "       [-makeswf_light]" << endl;
    cout << "       [-makeswf_value]" << endl;
#endif
    cout << "       [-useminmax min max]" << endl;
#endif
  cout << "       [<filename(s)>]" << endl;
  cout << endl;



  cout << "  file type flags:   -fab, -multifab, -newplt (-newplt is the default)" << endl;
  cout << "  -v                 verbose." << endl; 
  //cout << "  -bw n              specify maximum boundary width." << endl; 
  cout << "  -maxpixmapsize n   specify maximum allowed picture size in pixels."
       << endl;
  //cout << "  -b                 specify subdomain box (on finest level)." << endl;
  cout << "  -skippltlines n    skip n lines at head of the plt file." << endl; 
  cout << "  -boxcolor n        set volumetric box color value [0,255]." << endl; 
  cout << "  -xslice n          write a fab slice at x = n (n at the finest level)."
       << endl; 
  cout << "  -yslice n          write a fab slice at y = n (n at the finest level)."
       << endl; 
  cout << "  -zslice n          write a fab slice at z = n (n at the finest level)."
       << endl; 
  cout << "  -sliceallvars      write all fab variables instead of just initialderived."
       << endl; 
  cout << "  -boxslice _box_    write a fab on the box (box at the finest level)."
       << endl; 
  cout << "                     box format:  lox loy loz hix hiy hiz." << endl;
  cout << "                     example:  -boxslice 0 0 0 120 42 200." << endl;
  cout << "                     Note:  slices are written in batch mode." << endl;
#if(BL_SPACEDIM == 2)
  cout << "  -a                 load files as an animation." << endl; 
#endif
  //cout << "  -sleep  n          specify sleep time (for attaching parallel debuggers)." << endl;
  cout << "  -fabiosize nbits   write fabs with nbits (valid values are 8 and 32." << endl;
  cout << "                     the default is native (usually 64)." << endl;
  cout << "  -maxlev n          specify the maximum drawn level." << endl;
  cout << "  -palette palname   set the initial palette." << endl; 
  cout << "  -lightingfile name set the initial lighting parameter file." << endl; 
  cout << "  -initialderived dername   set the initial derived to dername." << endl; 
  cout << "  -initialscale n    set the initial scale to n." << endl; 
  cout << "  -showboxes tf      show boxes (the value of tf is true or false)." << endl; 
  cout << "  -showbody tf       show cartGrid body as body cells (def is true)." << endl; 
  cout << "  -numberformat fmt  set the initial format to fmt (ex:  %4.2f)." << endl; 
  cout << "  -lowblack          sets the lowest color in the palette to black."<<endl;
  cout << "  -cliptoppalette    do not use the top palette index (for exceed)."<<endl;
  cout << "  -fixdenormals      always fix denormals when reading fabs."<<endl;
  cout << "  -ppm               output rasters using PPM file format."<<endl;
#if (BL_SPACEDIM == 3)
#ifdef BL_VOLUMERENDER
  cout << "  -makeswf_light     make volume rendering data using the" << endl;
  cout << "                     current transfer function and write data" << endl;
  cout << "                     to a file, using the lighting model."<<endl
       << "                     note:  works in batch mode." << endl;
  cout << "  -makeswf_value     same as above, with value model rendering."<<endl;
#endif
  cout << "  -useminmax min max use min and max as the global min max values" << endl;
#endif
  cout << "  <filename(s)>      must be included if box is specified." << endl;
  cout << endl;

  exit(0);
}


// -------------------------------------------------------------------
void AVGlobals::ParseCommandLine(int argc, char *argv[]) {
  char clsx[10];
  char clsy[10];
  char clbx[10];
  char clby[10];

#if (BL_SPACEDIM == 3)
  char clsz[10];
  char clbz[10];
#endif

  givenFilename = false;
  givenBox = false;
  givenBoxSlice = false;
  makeSWFData = false;
  makeSWFLight = false;
  dumpSlices = false;
  sliceAllVars = false;
  verbose = false;
  fileCount = 0;
  maxLevel = -1;
  useMaxLevel = false;
  sleepTime = 0;
  animation = false;
  specifiedMinMax = false;
  specifiedMin = 0.0;
  specifiedMax = 1.0;
  comlinefilename.resize(argc);  // slightly larger than eventual fileCount
  dumpSliceList.resize(3);  // always use 3 (zslice in 2d is image plane)

  int i;
  for(i = 1; i <= argc - 1; ++i) {
    if(strcmp(argv[i], "-maxpixmapsize") == 0) {
      if(argc-1<i+1 || atoi(argv[i+1]) == 0) {
        PrintUsage(argv[0]);
      } else {
	maxPictureSize = atoi(argv[i+1]);
      }
      ++i;
    } else if(strcmp(argv[i], "-bw") == 0) {
      if(argc-1<i+1 || atoi(argv[i+1]) < 0) {
        PrintUsage(argv[0]);
      } else {
	boundaryWidth = atoi(argv[i+1]);
      }
      ++i;
    } else if(strcmp(argv[i], "-maxlev") == 0) {
      if(argc-1<i+1 || atoi(argv[i+1]) < 0) {
        PrintUsage(argv[0]);
      } else {
        maxLevel = atoi(argv[i+1]);
        useMaxLevel = true;
      }
      ++i;
    } else if(strcmp(argv[i], "-sleep") == 0) {
      sleepTime = atoi(argv[i+1]);
      ++i;
    } else if(strcmp(argv[i], "-fabiosize") == 0) {
      int iSize = atoi(argv[i+1]);
      if(iSize == 8 || iSize == 32) {
        fabIOSize = iSize;
      } else {
	cerr << "Warning:  -fabiosize must be 8 or 32.  Defaulting to native."
	     << endl;
        fabIOSize = 0;
      }
      ++i;
    } else if(strcmp(argv[i], "-skippltlines") == 0) {
      if(argc-1<i+1 || atoi(argv[i+1]) < 0) {
        PrintUsage(argv[0]);
      } else {
	skipPltLines = atoi(argv[i+1]);
      }
      ++i;
    } else if(strcmp(argv[i], "-boxcolor") == 0) {
      if(argc-1<i+1 || atoi(argv[i+1]) < 0) {
        PrintUsage(argv[0]);
      } else {
	boxColor = atoi(argv[i+1]);
      }
      ++i;
#    if (BL_SPACEDIM == 2)
    } else if(strcmp(argv[i],"-a") == 0) {
      animation = true; 
#   endif
    } else if(strcmp(argv[i],"-fab") == 0) {
      fileType = FAB;
    } else if(strcmp(argv[i],"-multifab") == 0) {
      fileType = MULTIFAB;
    } else if(strcmp(argv[i],"-newplt") == 0) {
      fileType = NEWPLT;
    } else if(strcmp(argv[i],"-v") == 0) {
      verbose = true;
    } else if(strcmp(argv[i], "-b") == 0) {
#    if (BL_SPACEDIM == 2)
      if(argc-1<i+1 || ! strcpy(clsx, argv[i+1])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+2 || ! strcpy(clsy, argv[i+2])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+3 || ! strcpy(clbx, argv[i+3])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+4 || ! strcpy(clby, argv[i+4])) {
        PrintUsage(argv[0]);
      }
      i += 4;
      givenBox = true;
#    else
      if(argc-1<i+1 || ! strcpy(clsx, argv[i+1])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+2 || ! strcpy(clsy, argv[i+2])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+3 || ! strcpy(clsz, argv[i+3])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+4 || ! strcpy(clbx, argv[i+4])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+5 || ! strcpy(clby, argv[i+5])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+6 || ! strcpy(clbz, argv[i+6])) {
        PrintUsage(argv[0]);
      }
      i += 6;
      givenBox = true;
#   endif

    } else if(strcmp(argv[i], "-makeswf_light") == 0) {
        makeSWFData = true;
	makeSWFLight = true;
    } else if(strcmp(argv[i], "-makeswf_value") == 0) {
        makeSWFData = true;
	makeSWFLight = false;
    } else if(strcmp(argv[i], "-lowblack") == 0) {
        lowBlack = true;
    } else if(strcmp(argv[i], "-useminmax") == 0) {
      specifiedMinMax = true;
      if(argc-2<i+2) {
        PrintUsage(argv[0]);
      } else {
        specifiedMin = atof(argv[i+1]);
        specifiedMax = atof(argv[i+2]);
	cout << "******* using min max = " << specifiedMin
	     << "  " << specifiedMax << endl;
      }
      i += 2;
    } else if(strcmp(argv[i],"-help") == 0) {
      PrintUsage(argv[0]);
    } else if(strcmp(argv[i], "-xslice") == 0) {
      if(argc-1<i+1) {
        PrintUsage(argv[0]);
      } else {
	AddSlices(XDIR, argv[i+1]);
      }
      ++i;
    } else if(strcmp(argv[i], "-yslice") == 0) {
      if(argc-1<i+1) {
        PrintUsage(argv[0]);
      } else {
	AddSlices(YDIR, argv[i+1]);
      }
      ++i;
    } else if(strcmp(argv[i], "-zslice") == 0) {
      if(argc-1<i+1) {
        PrintUsage(argv[0]);
      } else {
	AddSlices(ZDIR, argv[i+1]);
      }
      ++i;
    } else if(strcmp(argv[i], "-boxslice") == 0) {
#    if (BL_SPACEDIM == 2)
      if(argc-1<i+1 || ! strcpy(clsx, argv[i+1])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+2 || ! strcpy(clsy, argv[i+2])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+3 || ! strcpy(clbx, argv[i+3])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+4 || ! strcpy(clby, argv[i+4])) {
        PrintUsage(argv[0]);
      }
      i += 4;
      givenBoxSlice = true;
#    else
      if(argc-1<i+1 || ! strcpy(clsx, argv[i+1])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+2 || ! strcpy(clsy, argv[i+2])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+3 || ! strcpy(clsz, argv[i+3])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+4 || ! strcpy(clbx, argv[i+4])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+5 || ! strcpy(clby, argv[i+5])) {
        PrintUsage(argv[0]);
      }
      if(argc-1<i+6 || ! strcpy(clbz, argv[i+6])) {
        PrintUsage(argv[0]);
      }
      i += 6;
      givenBoxSlice = true;
#   endif
    } else if(strcmp(argv[i],"-palette") == 0) {
      PltApp::SetDefaultPalette(argv[i+1]);
      initialPalette = argv[i+1];
      ++i;
    } else if(strcmp(argv[i],"-lightingfile") == 0) {
      PltApp::SetDefaultLightingFile(argv[i+1]);
      initialLightingFile = argv[i+1];
      ++i;
    } else if(strcmp(argv[i], "-initialderived") == 0) {
      PltApp::SetInitialDerived(argv[i+1]);
      initialDerived = argv[i+1];
      ++i;
    } else if(strcmp(argv[i], "-initialscale") == 0) {
      int tempiscale = atoi(argv[i+1]);
      if(argc-1 < i+1 || tempiscale < 1) {
        PrintUsage(argv[0]);
      }
      PltApp::SetInitialScale(tempiscale);
      ++i;
    } else if(strcmp(argv[i], "-numberformat") == 0) {
      PltApp::SetInitialFormatString(argv[i+1]);
      ++i;
    } else if(strcmp(argv[i], "-showboxes") == 0) {
      if(*argv[i+1] == 't' || *argv[i+1] == 'T') {
        PltApp::SetDefaultShowBoxes(true);
      } else if(*argv[i+1] == 'f' || *argv[i+1] == 'F') {
        PltApp::SetDefaultShowBoxes(false);
      } else {
        PrintUsage(argv[0]);
      }
      ++i;
    } else if(strcmp(argv[i], "-showbody") == 0) {
      if(*argv[i+1] == 't' || *argv[i+1] == 'T') {
        bShowBody = true;
      } else if(*argv[i+1] == 'f' || *argv[i+1] == 'F') {
        bShowBody = false;
      } else {
        PrintUsage(argv[0]);
      }
      ++i;
    } else if(strcmp(argv[i], "-cliptoppalette") == 0) {
      maxPaletteIndex = 254;  // clip the top palette index
    } else if(strcmp(argv[i], "-fixdenormals") == 0) {
      RealDescriptor::SetFixDenormals();
    } else if(strcmp(argv[i], "-ppm") == 0) {
	SGIrgbfile = false;
    } else if(strcmp(argv[i], "-sliceallvars") == 0) {
      sliceAllVars = true;
    } else if(i < argc) {
      if(fileType == MULTIFAB) {
        // delete the _H from the filename if it is there
        char *tempfilename = new char[strlen(argv[i]) + 1];
        strcpy(tempfilename, argv[i]);
        const char *uH = "_H";
        char *fm2 = tempfilename + (strlen(tempfilename) - 2);
        if(strcmp(uH, fm2) == 0) {
          tempfilename[strlen(tempfilename) - 2] = '\0';
        }
        comlinefilename[fileCount] = tempfilename;
	delete tempfilename;
      } else {
        comlinefilename[fileCount] = argv[i];
      }
      ++fileCount;
      givenFilename = true;
    } else {
      PrintUsage(argv[0]);
    }
  }  // end for(i...)

  if(givenBox && ! givenFilename) {
    PrintUsage(argv[0]);
  }

  if(givenBox || givenBoxSlice) {
#   if (BL_SPACEDIM == 2)
      if(atoi(clsx) > atoi(clbx) || atoi(clsy) > atoi(clby)) {
        cout << "A sub-region box must be specified as:\n\t <small x> <small y> "
	     << "<big x> <big y>\n" << endl;
        exit(0);
      }
      comlinebox.setSmall(XDIR, atoi(clsx));
      comlinebox.setSmall(YDIR, atoi(clsy));
      comlinebox.setBig(XDIR, atoi(clbx));
      comlinebox.setBig(YDIR, atoi(clby));
#   else
      if(atoi(clsx) > atoi(clbx) || atoi(clsy) > atoi(clby) || 
	 atoi(clsz) > atoi(clbz))
      {
        cout << "A sub-region box must be specified as:\n\t<small x> <small y>"
	     << " <small z> <big x> <big y> <big z>" << endl;
        exit(0);
      }
      comlinebox.setSmall(XDIR, atoi(clsx));
      comlinebox.setSmall(YDIR, atoi(clsy));
      comlinebox.setSmall(ZDIR, atoi(clsz));
      comlinebox.setBig(XDIR,   atoi(clbx));
      comlinebox.setBig(YDIR,   atoi(clby));
      comlinebox.setBig(ZDIR,   atoi(clbz));
#   endif
  }


  if(fileType == INVALIDTYPE) {
    BoxLib::Abort("Error:  invalid file type.  Exiting.");
  } else {
    if(ParallelDescriptor::IOProcessor()) {
      if(verbose) {
        cout << ">>>>>>> Setting file type to "
             << FileTypeString[fileType] << "." << endl << endl;
      }
    }
  }

}  // end ParseCommandLine


// -------------------------------------------------------------------
void AVGlobals::SetSGIrgbFile() { SGIrgbfile = true; }
void AVGlobals::ClearSGIrgbFile() { SGIrgbfile = false; }
bool AVGlobals::IsSGIrgbFile() { return SGIrgbfile; }

void AVGlobals::SetAnimation() { animation = true; }
bool AVGlobals::IsAnimation()  { return animation; }

Box AVGlobals::GetBoxFromCommandLine() { return comlinebox; }

void AVGlobals::SetMaxPictureSize(int maxpicsize) { maxPictureSize = maxpicsize; }
int AVGlobals::MaxPictureSize() { return maxPictureSize; }

int AVGlobals::MaxPaletteIndex() { return maxPaletteIndex; }

void AVGlobals::SetVerbose() { verbose = true; }
bool AVGlobals::Verbose()    { return verbose; }

int AVGlobals::GetFileCount() { return fileCount; }
int AVGlobals::SleepTime() { return sleepTime; }

int  AVGlobals::GetMaxLevel() { return maxLevel; }
bool AVGlobals::UseMaxLevel() { return useMaxLevel; }

const string &AVGlobals::GetComlineFilename(int i) { return comlinefilename[i]; }
FileType AVGlobals::GetDefaultFileType()   { return fileType;    }

bool AVGlobals::GivenBox()    { return givenBox;     }
bool AVGlobals::GivenBoxSlice() { return givenBoxSlice;     }
bool AVGlobals::MakeSWFData() { return makeSWFData;  }
bool AVGlobals::MakeSWFLight() { return makeSWFLight; }
bool AVGlobals::LowBlack() { return lowBlack; }
bool AVGlobals::DumpSlices() { return dumpSlices;   }
bool AVGlobals::SliceAllVars() { return sliceAllVars; }

bool AVGlobals::GivenFilename() { return givenFilename; }

void AVGlobals::SetBoundaryWidth(int width) { boundaryWidth = width; }
int AVGlobals::GetBoundaryWidth() { return boundaryWidth;  }

void AVGlobals::SetSkipPltLines(int nlines) { skipPltLines = nlines; }
int AVGlobals::GetSkipPltLines() { return skipPltLines; }

void AVGlobals::SetBoxColor(int boxcolor) { boxColor = boxcolor; }
int AVGlobals::GetBoxColor() { return boxColor; }

Array< list<int> > &AVGlobals::GetDumpSlices() { return dumpSliceList; }

int  AVGlobals::GetFabOutFormat() { return fabIOSize;  }

bool AVGlobals::UseSpecifiedMinMax() { return specifiedMinMax; }
void AVGlobals::SetSpecifiedMinMax(Real  specifiedmin, Real  specifiedmax) {
  specifiedMin = specifiedmin;
  specifiedMax = specifiedmax;
}

void AVGlobals::GetSpecifiedMinMax(Real &specifiedmin, Real &specifiedmax) {
  specifiedmin = specifiedMin;
  specifiedmax = specifiedMax;
}


// -------------------------------------------------------------------
int AVGlobals::CRRBetweenLevels(int fromlevel, int tolevel,
                                const Array<int> &refratios)
{
  BL_ASSERT(fromlevel >= 0);
  BL_ASSERT(tolevel >= fromlevel);
  BL_ASSERT(tolevel <= refratios.size());
  int level, rr = 1;
  for(level = fromlevel; level < tolevel; ++level) {
    rr *= refratios[level];
  }
  return rr;
}


// -------------------------------------------------------------------
int AVGlobals::DetermineMaxAllowableLevel(const Box &finestbox,
                                          int finestlevel,
                                          int maxpoints,
					  const Array<int> &refratios)
{
  BL_ASSERT(finestlevel >= 0);
  BL_ASSERT(maxpoints >= 0);
  BL_ASSERT(finestbox.ok());

  Box levelDomain(finestbox);
  int maxallowablelevel(finestlevel);
  unsigned long boxpoints;
  while(maxallowablelevel > 0) {
#   if (BL_SPACEDIM == 2)
      boxpoints = levelDomain.length(XDIR) * levelDomain.length(YDIR);
#   else
      unsigned long tempLength;
      boxpoints = (unsigned long) levelDomain.length(XDIR) *
                  (unsigned long) levelDomain.length(YDIR);
      tempLength = (unsigned long) levelDomain.length(YDIR) *
                   (unsigned long) levelDomain.length(ZDIR);
      boxpoints = max(boxpoints, tempLength);
      tempLength = levelDomain.length(XDIR) * levelDomain.length(ZDIR);
      boxpoints = max(boxpoints, tempLength);
#   endif

    if(boxpoints > maxpoints) {  // try next coarser level
      --maxallowablelevel;
      levelDomain = finestbox;
      levelDomain.coarsen
              (CRRBetweenLevels(maxallowablelevel, finestlevel, refratios));
    } else {
      break;
    }
  }
  return(maxallowablelevel);
}


// -------------------------------------------------------------------
// this function strips slashes to try to find the true file name
//   so:  /d/e/f/pltname/  would return pltname
// -------------------------------------------------------------------
string AVGlobals::StripSlashes(const string &inString) {
  string sTemp;
  string::size_type startString, endString;
  endString = inString.find_last_not_of('/');
  startString = inString.find_last_of('/', endString);
  if(startString == string::npos) {  // no slashes found
    startString = 0;
  } else {                           // skip over the last one found
    ++startString;
  }
  sTemp = inString.substr(startString, (endString - startString + 1));

  return sTemp;
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------




